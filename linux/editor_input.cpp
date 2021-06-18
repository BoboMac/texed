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

