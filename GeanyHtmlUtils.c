#include <geanyplugin.h>

GeanyPlugin *geany_plugin;
GeanyData *geany_data;

PLUGIN_VERSION_CHECK(211)

PLUGIN_SET_INFO("Geany HTML Utils", 
	"Auto close and indent multiple HTML tags", 
	"1.0", "Nho Huynh <nhohb07@gmail.com>");
	
typedef struct{
	GeanyDocument *doc;
	ScintillaObject *sci;
}PluginData;

typedef struct{
	gint start;
	gint end;
	gchar *tag_name;
}Tag;

static PluginData *plugin_data = NULL;


gchar *remove_char(gchar *tag, char remove){
	char *pr = tag, *pw = tag;
	while (*pr) {
		*pw = *pr++;
		pw += (*pw != remove);
	}
	*pw = '\0';
	
	return tag;
}
gchar *get_tag_name(gchar *tag){
	int i = 0;
	int tag_len = (gint)g_strlcpy(tag, tag, 0);
	int substr_to = tag_len - 1;
	
	while(i < tag_len){
		i++;
		if(tag[i] == ' '){
			substr_to = i;
			break;
		}
	}
	
	gchar *tag_name = g_strndup(tag, substr_to);
	
	tag_name = remove_char(tag_name, '<');
	tag_name = remove_char(tag_name, '/');
	
	return tag_name;
}

gboolean is_open_tag(gchar *tag){
	return g_regex_match(g_regex_new("< *([^/][^ ]*).*?>", 0, 0, NULL), tag, 0, NULL);
}

gboolean is_close_tag(gchar *tag){
	return g_regex_match(g_regex_new("</ *([^ ]*).*?>", 0, 0, NULL), tag, 0, NULL);
}

gboolean is_comment_tag(gchar *tag){
	/*tags like <!-- --> and <!doctype ...>*/
	return g_regex_match(g_regex_new("<!.*?>", 0, 0, NULL), tag, 0, NULL);
}

gboolean is_special_tag(gchar *tag){
	/*tags like <?, <?=, <?php*/
	return g_regex_match(g_regex_new("<\\?.*?>", 0, 0, NULL), tag, 0, NULL);
}

gboolean is_neutral_tag(gchar *tag){
	//return g_regex_match(g_regex_new("<(meta|link|br|input|hr|base|img|embed|param|area|col)?.*>", 0, 0, NULL), tag, 0, NULL);
	return g_regex_match(g_regex_new("<(meta|link|br|input|hr|base|img|embed|param|area|col).*?>", 0, 0, NULL), tag, 0, NULL);
}

Tag find_matching_tag(ScintillaObject *sci, gint cur_pos){
	Tag tag_data;
	tag_data.start = -1;
	tag_data.end = -1;
	tag_data.tag_name = NULL;
	
	GRegex *regex;
	GMatchInfo *match_info;
	gchar *tag;
	gchar *string;
	GArray *open_tags = g_array_new(FALSE, FALSE, sizeof(Tag));
	gint opend_tag_size = 0;

	string = sci_get_contents_range(sci, 0, cur_pos);

	regex = g_regex_new ("<.*?>+", 0, 0, NULL);
	g_regex_match (regex, string, 0, &match_info);

	while(g_match_info_matches(match_info)){
		gint start = -1, end = -1;

		tag = g_match_info_fetch (match_info, 0);

		g_match_info_fetch_pos(match_info, 0, &start, &end);

		if(is_special_tag(tag) && sci_get_char_at(sci, start + 1) != '?'){
			tag = get_tag_name(tag);
			tag = g_strconcat("<", tag, ">", NULL);
			end = sci_find_matching_brace(sci, start) + 1;
		}

		if(!is_comment_tag(tag) && !is_neutral_tag(tag) && !is_special_tag(tag) && start != -1 && end != -1){
			if(is_open_tag(tag)){
				Tag t;
				t.start = start;
				t.end = end;
				t.tag_name = get_tag_name(tag);
				g_array_append_val(open_tags, t);
				opend_tag_size++;
			}
			if(is_close_tag(tag)){
				while(TRUE){
					if(opend_tag_size <= 0){
						tag_data.start = start;
						tag_data.end = end;
						tag_data.tag_name = get_tag_name(tag);
						return tag_data;
					}
					opend_tag_size--;
					Tag *t = &g_array_index(open_tags, Tag, opend_tag_size);
					g_array_remove_index(open_tags, opend_tag_size);

					gchar *closed_tag = get_tag_name(tag);

					if(g_strcmp0(t->tag_name, closed_tag) == 0){
						g_free(closed_tag);
						break;
					}
				}

			}
		}

		g_match_info_next (match_info, NULL);
	}

	if(opend_tag_size > 0){
		tag_data = g_array_index(open_tags, Tag, opend_tag_size - 1);
		g_array_free(open_tags, TRUE);
	}

	g_match_info_free (match_info);
	g_regex_unref (regex);
	g_free(tag);
	g_free(string);
	
	return tag_data;
}

	
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data){
	PluginData *data = user_data;
	g_return_if_fail(data);
	
	gint left_ch, current_ch, cur_pos;
	
	if(data->doc->file_type->id == GEANY_FILETYPES_PHP || data->doc->file_type->id == GEANY_FILETYPES_HTML){
		cur_pos = sci_get_current_position(data->sci);
		current_ch = event->keyval;
		left_ch = sci_get_char_at(data->sci, cur_pos - 1);
		
		if(current_ch == '/' && left_ch == '<'){
			Tag tag = find_matching_tag(data->sci, cur_pos);
			msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s", tag.tag_name);
			
			if(tag.tag_name != NULL){
				gchar *close_tag = tag.tag_name;
				close_tag = g_strconcat("/", close_tag, NULL);
				close_tag = g_strconcat(close_tag, ">", NULL);
				scintilla_send_message(data->sci, SCI_INSERTTEXT, cur_pos, (sptr_t)close_tag);
				sci_set_current_position(data->sci, cur_pos + (int)g_strlcpy(close_tag, close_tag, 0), FALSE);
				
				gint current_line = sci_get_current_line(data->sci);
				gint open_tag_line = sci_get_line_from_position(data->sci, tag.start);
				
				if(current_line != open_tag_line){
					gint tag_width = sci_get_tab_width(data->sci);
					gint current_tag_indent = sci_get_line_indentation(data->sci, current_line);

					gchar *text_before_tag = sci_get_contents_range(data->sci, sci_get_position_from_line(data->sci, current_line), cur_pos - 1);
					gchar *text_indent = "";

					guint i;
					foreach_range(i, current_tag_indent / tag_width){
						text_indent = g_strconcat("\t", text_indent, NULL);
					}

					if(g_strcmp0(text_before_tag, text_indent) == 0){
						sci_set_line_indentation(data->sci, current_line, sci_get_line_indentation(data->sci, open_tag_line));
					}
					
					g_free(text_before_tag);
					g_free(text_indent);
				}
					
				return TRUE;
			}
	
		}
		
	}

	return FALSE;
}
	
static void on_document_open(GObject *obj, GeanyDocument *doc, gpointer user_data){
	g_return_if_fail(DOC_VALID(doc));
	
	PluginData *data;
	data = g_new0(PluginData, 1);
	data->doc = doc;
	data->sci = doc->editor->sci;
	plugin_signal_connect(geany_plugin, G_OBJECT(data->sci), "key-press-event", FALSE, G_CALLBACK(on_key_press), data);
	//plugin_signal_connect(geany_plugin, G_OBJECT(data->sci), "sci-notify", FALSE, G_CALLBACK(on_sci_notify), data);
}
	
void plugin_init(GeanyData *data){
	int i;
	foreach_document(i){
		on_document_open(NULL, documents[i], NULL);
	}
	
	plugin_data = g_new0(PluginData, 1);
}

void plugin_cleanup(void){
	//g_free(smarty_data->config_file);
	g_free(plugin_data);
}

void plugin_help(void){
	utils_open_browser("https://github.com/JLamp07/geany-smarty");
}