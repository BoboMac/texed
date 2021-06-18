/*
Track list:
- Wasn't born in Cicago - Gary Moore

TODO:
- put every function in the now-struct-then-class editorConfig
- split everything in .h and .cpp files (editor.h, editor_output.cpp, editor_backend.cpp)
- get rid of all the kilo occurences and use texed instead
- make sure u changed it enought to not use a license
- ???
- profit
- special binds for normal mode:
  - qq for exitting
  - f for finding
  - u for undo, r for redo or smth
  - c for copy, p for paste
  - : for entering a command
  - x to delete character (?)
  - dd to delete line (? press x to doubt)

- undo system (maybe use the GUI code?)
  - line 1325

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
        fprintf(stderr,"Usage: kilo <filename>\n");
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