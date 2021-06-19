#include "editor.h"

int editorReadKey(int fd) {
    int nread;
    char c, seq[5];
    while ((nread = read(fd,&c,1)) != 1);
    if (nread == -1) exit(1);

    while(1) {
        switch(c) {
        case ESC:    /* escape sequence */
            /* If this is just an ESC, we'll timeout here. */
            if (read(fd,seq,1) == 0) return ESC;
            if (read(fd,seq+1,1) == 0) return ESC;

            /* ESC [ sequences. */
            if (seq[0] == '[') {
				if (seq[1] == 49) {
					if (
						read(fd, seq + 2, 1) && seq[2] == 59 &&
						read(fd, seq + 3, 1) && seq[3] == 53 &&
						read(fd, seq + 4, 1)
					) {
						switch(seq[4]) {
							case 'A': return ARROW_CTRL_UP;
							case 'B': return ARROW_CTRL_DOWN;
							case 'C': return ARROW_CTRL_RIGHT;
							case 'D': return ARROW_CTRL_LEFT;
							default:
								break;
						}
					}
				}
                else if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    if (read(fd,seq+2,1) == 0) return ESC;
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '3': return DEL_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }
            break;
        default:
            return c;
        }
    }
}

/* Process events arriving from the standard input, which is, the user
 * is typing stuff on the terminal. */
void editorProcessKeypress(editorConfig *E, int fd) {
    /* When the file is modified, requires Ctrl-q to be pressed N times
     * before actually quitting. */
    static int quit_times = EDITOR_QUIT_TIMES;
	static int delete_line_key_pressed_times = 1;

    int c = editorReadKey(fd);
	if (E->mode == EDITOR_MODE_NORMAL) {
		switch(c) {
		case CTRL_S:
			editorSave(E);
			break;
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(E, c);
			break;
		case ARROW_CTRL_UP:
			// Maybe move to previous paragraph?
			break;
		case ARROW_CTRL_DOWN:
			// Maybe move to next paragraph?
			break;
		case ARROW_CTRL_LEFT:
			if (is_separator(E->row[E->cy].chars[E->cx - 1]))
				while(is_separator(E->cx && E->row[E->cy].chars[E->cx - 1]))
					editorMoveCursor(E, ARROW_LEFT);
			else
				while(!is_separator(E->cx && E->row[E->cy].chars[E->cx - 1]))
					editorMoveCursor(E, ARROW_LEFT);
			break;
		case ARROW_CTRL_RIGHT:
			if (is_separator(E->row[E->cy].chars[E->cx]))
				while(E->cx != E->row[E->cy].size && is_separator(E->row[E->cy].chars[E->cx]))
					editorMoveCursor(E, ARROW_RIGHT);
			else
				while(E->cx != E->row[E->cy].size && !is_separator(E->row[E->cy].chars[E->cx]))
					editorMoveCursor(E, ARROW_RIGHT);
			break;
		case 'k':
			if (E->dirty) {
				editorSetStatusMessage(E, "Unsaved changes in the current file. Save before exiting.");
				break;
			}
			if (quit_times) {
				quit_times--;
				return;
			}
			editorAtExit(E);
			system("clear");
			exit(0);
			break;
		case 'x':
			editorMoveCursor(E, ARROW_RIGHT);
			editorDelChar(E);
			break;
		case 'd':
			if (!E->numrows) break;
			E->dirty = 1;
			if (delete_line_key_pressed_times) {
				delete_line_key_pressed_times--;
				return;
			}
			for (int i = E->cy; i < E->numrows - 1; i++)
				E->row[i] = E->row[i + 1];
			E->numrows--;
			break;
		case 'f':
			editorFind(E, fd);
			break;
		case 'i':
			E->mode = EDITOR_MODE_INSERT;
			break;
		case ':':
			// Let the user insert a command (?)
			break;
		case 'u':
			editorUndo(E);
			break;
		case 'r':
			editorRedo(E);
			break;
		case 'c':
			editorCopy();
			break;
		case 'p':
			editorPaste();
			break;
		default:
			break;
		}
	}
	else if (E->mode == EDITOR_MODE_INSERT) {
		switch(c) {
		case ESC:
			E->mode = EDITOR_MODE_NORMAL;
			break;
		case ENTER: {         /* Enter */
			editorInsertNewline(E);
			// Undo system
		} break;
		case CTRL_S:        /* Ctrl-s */
			editorSave(E);
			break;
		case CTRL_F:
			editorFind(E, fd);
			break;
		case BACKSPACE: {     /* Backspace */
			char deleted_char = editorDelChar(E);
			UndoCommandBus bus;
			PushCommand(E, &bus, UNDO_CMD_DELETE_LEFT_CHAR, deleted_char);
			PushCommandBus(E, bus);
		} break;
		case DEL_KEY:
			if (E->cy != E->numrows && E->cx != E->row[E->cy].size) {
				editorMoveCursor(E, ARROW_RIGHT);
				char deleted_char = editorDelChar(E);
				UndoCommandBus bus;
				PushCommand(E, &bus, UNDO_CMD_DELETE_LEFT_CHAR, deleted_char);
				PushCommandBus(E, bus);
			}
			break;
		case PAGE_UP:
		case PAGE_DOWN: {
			if (c == PAGE_UP && E->cy != 0)
				E->cy = 0;
			else if (c == PAGE_DOWN && E->cy != E->screenrows-1)
				E->cy = E->screenrows-1;
			int times = E->screenrows;
			while(times--)
				editorMoveCursor(E, c == PAGE_UP ? ARROW_UP:
												ARROW_DOWN);
		} break;
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(E, c);
			break;
		case ARROW_CTRL_UP:
			// Maybe move to previous paragraph?
			break;
		case ARROW_CTRL_DOWN:
			// Maybe move to next paragraph?
			break;
		case ARROW_CTRL_LEFT:
			if (E->row[E->cy].chars[E->cx - 1] == ' ')
				while(E->cx && E->row[E->cy].chars[E->cx - 1] == ' ')
					editorMoveCursor(E, ARROW_LEFT);
			else
				while(E->cx && E->row[E->cy].chars[E->cx - 1] != ' ')
					editorMoveCursor(E, ARROW_LEFT);
			break;
		case ARROW_CTRL_RIGHT:
			if (E->row[E->cy].chars[E->cx] == ' ')
				while(E->cx != E->row[E->cy].size && E->row[E->cy].chars[E->cx] == ' ')
					editorMoveCursor(E, ARROW_RIGHT);
			else
				while(E->cx != E->row[E->cy].size && E->row[E->cy].chars[E->cx] != ' ')
					editorMoveCursor(E, ARROW_RIGHT);
			break;
		case CTRL_L: /* ctrl+l, clear screen */
			/* Just refresh the line as side effect. */
			break;
		default: {
			editorInsertChar(E, c);
			// Undo system
			UndoCommandBus bus;
			PushCommand(E, &bus, UNDO_CMD_INSERT, c);
			PushCommandBus(E, bus);
		} break;
    	}
	}

    quit_times = EDITOR_QUIT_TIMES; /* Reset it to the original value. */
	delete_line_key_pressed_times = 1;
}
