/*
Track list:
- Wasn't born in Cicago - Gary Moore

TODO:
- special binds for normal mode:
  - c for copy, p for paste
  - : for entering a command

- get rid of all the kilo occurences and use texed instead

- refactor line 1212 (in a galaxy far far away)

Note:
- deleting (DEL key) a '\n' doesn't work (is it that bad tho?)
*/

#include "editor.h"

void editorUndo(editorConfig *E);
void editorRedo(editorConfig *E);
void DeleteRedoQueue(editorConfig *E);
void PushCommand(editorConfig *E, UndoCommandBus* bus, char ID, char c);
void PushCommandBus(editorConfig *E, UndoCommandBus bus);
void editorSetStatusMessage(editorConfig *E, const char *fmt, ...);

void editorCopy() {

}

void editorPaste() {

}

int main(int argc, char **argv) {
	/*
	char c;
	while(c = getchar()) {
		if (c == 'q') break;
		printf("%d\n", c);
	}
	*/

	editorConfig E;

    if (argc != 2) {
        fprintf(stderr,"Usage: texed <filename>\n");
        exit(1);
    }

    initEditor(&E);
    editorSelectSyntaxHighlight(&E, argv[1]);
    editorOpen(&E, argv[1]);

    if (enableRawMode(&E, STDIN_FILENO) == -1) exit(0);
    editorSetStatusMessage(&E, "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");
    while(1) {
        editorRefreshScreen(&E);
        editorProcessKeypress(&E, STDIN_FILENO);
    	editorHandleResize(&E);
	}
	
	return 0;
}