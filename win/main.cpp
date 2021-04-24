/*
FEATURES:
- CLI args (opening a file)            [DONE]
- multiple buffers, one pane           [kinda]
- simple GUI                           [DONE]
- good navigation                      [DONE]
- intuitive scrolling                  [DONE]
- highlighting (based on parsing)      
- fast find
- intuitive selection                  [DONE]
- character insertion and deletion     [DONE]
- copy                                 [DONE]
- paste                                [DONE]
- undo/redo                            [DONE]
- indentation                          [kinda]
- code formating                       
- command terminal
- basic commands (?)
- terminal emulator
*/

/*
TODO:
- 450 make for
- initialize ncurses colors
- config file
  - tab num
  - color digit and keyword
  - more options on formatting (?)
- high lighting
- paran new line indent

- paste clipboard check bug
- resizing bug
- cleanup
*/

#include "util.h"
#include "buffer.h"
#include "pane.h"

int main(int argc, char **argv) {
	
	EditorConfig config;
	LoadEditorConfig("config.ed", &config);
	ncurses_init(config);
	//int c = COLOR_PAIRS - 1;
	//__debugbreak();
	Pane pane;
	if (argc > 1) {
		if (pane.Init(argv[1])) {
			while (1) {
				pane.Update(config);
			}
		}
		else {
			std::cout << "Error: No such file found. Exitting.\n";
		}
	}
	else {
		std::cout << "Error: No file specified. Exitting.\n";
	}
	return 0;
}
