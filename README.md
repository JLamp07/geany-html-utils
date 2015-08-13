# geany-html-utils
Auto close and indent multiple HTML tags

Compiler command:
<code>gcc -c GeanyHtmlUtils.c -fPIC \`pkg-config --cflags geany\` && gcc GeanyHtmlUtils.o -o GeanyHtmlUtils.so -shared \`pkg-config --libs geany\` && sudo cp GeanyHtmlUtils.so /usr/lib/x86_64-linux-gnu/geany</code>
