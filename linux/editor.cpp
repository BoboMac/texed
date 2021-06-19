#include "editor.h"

int is_separator(int c) {
    return c == '\0' || isspace(c) || strchr(",.()+-/*=~%[];",c) != NULL;
}

/* =========================== Syntax highlights DB =========================
 *
 * In order to add a new syntax, define two arrays with a list of file name
 * matches and keywords. The file name matches are used in order to match
 * a given syntax with a given file name: if a match pattern starts with a
 * dot, it is matched as the last past of the filename, for example ".c".
 * Otherwise the pattern is just searched inside the filenme, like "Makefile").
 *
 * The list of keywords to highlight is just a list of words, however if they
 * a trailing '|' character is added at the end, they are highlighted in
 * a different color, so that you can have two different sets of keywords.
 *
 * Finally add a stanza in the HLDB global variable with two two arrays
 * of strings, and a set of flags in order to enable highlighting of
 * comments and numbers.
 *
 * The characters for single and multi line comments must be exactly two
 * and must be provided as well (see the C language example).
 *
 * There is no support to highlight patterns currently. */
// C/C++
char *C_HL_extensions[] = {".c",".h",".cpp",".hpp",".cc",NULL};
char *C_HL_keywords[] = {
	/* C Keywords */
	"auto","break","case","continue","default","do","else","enum",
	"extern","for","goto","if","register","return","sizeof","static",
	"struct","switch","typedef","union","volatile","while","NULL",

	/* C++ Keywords */
	"alignas","alignof","and","and_eq","asm","bitand","bitor","class",
	"compl","constexpr","const_cast","deltype","delete","dynamic_cast",
	"explicit","export","false","friend","inline","mutable","namespace",
	"new","noexcept","not","not_eq","nullptr","operator","or","or_eq",
	"private","protected","public","reinterpret_cast","static_assert",
	"static_cast","template","this","thread_local","throw","true","try",
	"typeid","typename","virtual","xor","xor_eq",

	/* C types */
    "int|","long|","double|","float|","char|","unsigned|","signed|",
    "void|","short|","auto|","const|","bool|",NULL
};

/* Here we define an array of syntax highlights by extensions, keywords,
 * comments delimiters and flags. */
editorSyntax HLDB[] = {
    {
        /* C / C++ */
        C_HL_extensions,
        C_HL_keywords,
        {'/', '/'}, {'/', '*'}, {'*', '/'}, // C++ porting be like
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS
    }
};


void disableRawMode(editorConfig *E, int fd) {
    /* Don't even check the return value as it's too late. */
    if (E->rawmode) {
        tcsetattr(fd,TCSAFLUSH,&orig_termios);
        E->rawmode = 0;
    }
}

/* Called at exit to avoid remaining in raw mode. */
void editorAtExit(editorConfig *E) {
    disableRawMode(E, STDIN_FILENO);
}

/* Raw mode: 1960 magic shit. */
int enableRawMode(editorConfig *E, int fd) {
    struct termios raw;

    //if (E->rawmode) return 0; /* Already enabled. */
    if (!isatty(STDIN_FILENO)) goto fatal;
    if (tcgetattr(fd,&orig_termios) == -1) goto fatal;

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer. */
    raw.c_cc[VMIN] = 0; /* Return each byte, or zero for timeout. */
    raw.c_cc[VTIME] = 1; /* 100 ms timeout (unit is tens of second). */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;
    E->rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

/* Read a key from the terminal put in raw mode, trying to handle
 * escape sequences. */
/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor is stored at *rows and *cols and 0 is returned. */
int getCursorPosition(int ifd, int ofd, int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf)-1) {
        if (read(ifd,buf+i,1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf+2,"%d;%d",rows,cols) != 2) return -1;
    return 0;
}

/* Try to get the number of columns in the current terminal. If the ioctl()
 * call fails the function will try to query the terminal itself.
 * Returns 0 on success, -1 on error. */
int getWindowSize(int ifd, int ofd, int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int orig_row, orig_col, retval;

        /* Get the initial position so we can restore it later. */
        retval = getCursorPosition(ifd,ofd,&orig_row,&orig_col);
        if (retval == -1) goto failed;

        /* Go to right/bottom margin and get position. */
        if (write(ofd,"\x1b[999C\x1b[999B",12) != 12) goto failed;
        retval = getCursorPosition(ifd,ofd,rows,cols);
        if (retval == -1) goto failed;

        /* Restore position. */
        char seq[32];
        snprintf(seq,32,"\x1b[%d;%dH",orig_row,orig_col);
        if (write(ofd,seq,strlen(seq)) == -1) {
            /* Can't recover... */
        }
        return 0;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }

failed:
    return -1;
}

/* Update the rendered version and the syntax highlight of a row. */
void editorUpdateRow(editorConfig *E, erow *row) {
    unsigned int tabs = 0, nonprint = 0;
    int j, idx;

   /* Create a version of the row we can directly print on the screen,
     * respecting tabs, substituting non printable characters with '?'. */
    free(row->render);
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == TAB) tabs++;

    unsigned long long allocsize =
        (unsigned long long) row->size + tabs*8 + nonprint*9 + 1;
    if (allocsize > UINT32_MAX) {
        printf("Some line of the edited file is too long for kilo\n");
        exit(1);
    }

    row->render = (char*)malloc(row->size + tabs*8 + nonprint*9 + 1);
    idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == TAB) {
            row->render[idx++] = ' ';
            while((idx+1) % 8 != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->rsize = idx;
    row->render[idx] = '\0';

    /* Update the syntax highlighting attributes of the row. */
    editorUpdateSyntax(E, row);
}

/* Insert a row at the specified position, shifting the other rows on the bottom
 * if required. */
void editorInsertRow(editorConfig *E, int at, char *s, size_t len) {
    if (at > E->numrows) return;
    E->row = (erow*)realloc(E->row, sizeof(erow)*(E->numrows+1));
    if (at != E->numrows) {
        memmove(E->row+at+1, E->row+at,sizeof(E->row[0])*(E->numrows-at));
        for (int j = at+1; j <= E->numrows; j++) E->row[j].idx++;
    }
    E->row[at].size = len;
    E->row[at].chars = (char*)malloc(len+1);
    memcpy(E->row[at].chars,s,len+1);
    E->row[at].hl = NULL;
    E->row[at].hl_oc = 0;
    E->row[at].render = NULL;
    E->row[at].rsize = 0;
    E->row[at].idx = at;
    editorUpdateRow(E, E->row+at);
    E->numrows++;
    E->dirty++;
}

/* Free row's heap allocated stuff. */
void editorFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}







/* Remove the row at the specified position, shifting the remainign on the
 * top. */
void editorDelRow(editorConfig *E, int at) {
    erow *row;

    if (at >= E->numrows) return;
    row = E->row+at;
    editorFreeRow(row);
    memmove(E->row+at,E->row+at+1,sizeof(E->row[0])*(E->numrows-at-1));
    for (int j = at; j < E->numrows-1; j++) E->row[j].idx++;
    E->numrows--;
    E->dirty++;
}

/* Turn the editor rows into a single heap-allocated string.
 * Returns the pointer to the heap-allocated string and populate the
 * integer pointed by 'buflen' with the size of the string, escluding
 * the final nulterm. */
char *editorRowsToString(editorConfig *E, int *buflen) {
    char *buf = NULL, *p;
    int totlen = 0;
    int j;

    /* Compute count of bytes */
    for (j = 0; j < E->numrows; j++)
        totlen += E->row[j].size+1; /* +1 is for "\n" at end of every row */
    *buflen = totlen;
    totlen++; /* Also make space for nulterm */

    p = buf = (char*)malloc(totlen);
    for (j = 0; j < E->numrows; j++) {
        memcpy(p,E->row[j].chars, E->row[j].size);
        p += E->row[j].size;
        *p = '\n';
        p++;
    }
    *p = '\0';
    return buf;
}

/* Insert a character at the specified position in a row, moving the remaining
 * chars on the right if needed. */
void editorRowInsertChar(editorConfig *E, erow *row, int at, int c) {
    if (at > row->size) {
        /* Pad the string with spaces if the insert location is outside the
         * current length by more than a single character. */
        int padlen = at-row->size;
        /* In the next line +2 means: new char and null term. */
        row->chars = (char*)realloc(row->chars,row->size+padlen+2);
        memset(row->chars+row->size, ' ', padlen);
        row->chars[row->size+padlen + 1] = '\0';
        row->size += padlen + 1;
    } else {
        /* If we are in the middle of the string just make space for 1 new
         * char plus the (already existing) null term. */
        row->chars = (char*)realloc(row->chars,row->size+2);
        memmove(row->chars+at+1,row->chars+at,row->size-at+1);
        row->size++;
    }
    row->chars[at] = c;
    editorUpdateRow(E, row);
    E->dirty++;
}

/* Append the string 's' at the end of a row */
void editorRowAppendString(editorConfig *E, erow *row, char *s, size_t len) {
    row->chars = (char*)realloc(row->chars,row->size+len+1);
    memcpy(row->chars+row->size,s,len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(E, row);
    E->dirty++;
}

/* Delete the character at offset 'at' from the specified row. */
void editorRowDelChar(editorConfig *E, erow *row, int at) {
    if (row->size <= at) return;
    memmove(row->chars+at,row->chars+at+1,row->size-at);
    editorUpdateRow(E, row);
    row->size--;
    E->dirty++;
}

/* Insert the specified char at the current prompt position. */
void editorInsertChar(editorConfig *E, int c) {
    int filerow = E->rowoff + E->cy;
    int filecol = E->coloff + E->cx;
    erow *row = (filerow >= E->numrows) ? NULL : &E->row[filerow];

    /* If the row where the cursor is currently located does not exist in our
     * logical representaion of the file, add enough empty rows as needed. */
    if (!row) {
        while(E->numrows <= filerow)
            editorInsertRow(E, E->numrows, "", 0);
    }
    row = &E->row[filerow];
    editorRowInsertChar(E, row, filecol, c);
    if (E->cx == E->screencols-1)
        E->coloff++;
    else
        E->cx++;
    E->dirty++;
}

/* Inserting a newline is slightly complex as we have to handle inserting a
 * newline in the middle of a line, splitting the line as needed. */
void editorInsertNewline(editorConfig *E) {
    int filerow = E->rowoff + E->cy;
    int filecol = E->coloff + E->cx;
    erow *row = (filerow >= E->numrows) ? NULL : &E->row[filerow];

    if (!row) {
        if (filerow == E->numrows) {
            editorInsertRow(E, filerow,"",0);
            goto fixcursor;
        }
        return;
    }
    /* If the cursor is over the current line size, we want to conceptually
     * think it's just over the last character. */
    if (filecol >= row->size) filecol = row->size;
    if (filecol == 0) {
        editorInsertRow(E, filerow, "", 0);
    } else {
        /* We are in the middle of a line. Split it between two rows. */
        editorInsertRow(E, filerow+1,row->chars+filecol,row->size-filecol);
        row = &E->row[filerow];
        row->chars[filecol] = '\0';
        row->size = filecol;
        editorUpdateRow(E, row);
    }
fixcursor:
    if (E->cy == E->screenrows-1) {
        E->rowoff++;
    } else {
        E->cy++;
    }
    E->cx = 0;
    E->coloff = 0;
}

/* Delete the char at the current prompt position.
 * Returns the deleted char.*/
char editorDelChar(editorConfig *E) {
    int filerow = E->rowoff+E->cy;
    int filecol = E->coloff+E->cx;
    erow *row = (filerow >= E->numrows) ? NULL : &E->row[filerow];

    if (!row || (filecol == 0 && filerow == 0)) return 0;
    if (filecol == 0) {
        /* Handle the case of column 0, we need to move the current line
         * on the right of the previous one. */
        filecol = E->row[filerow-1].size;
        editorRowAppendString(E, &E->row[filerow-1],row->chars,row->size);
        editorDelRow(E, filerow);
        row = NULL;
        if (E->cy == 0)
            E->rowoff--;
        else
            E->cy--;
        E->cx = filecol;
        if (E->cx >= E->screencols) {
            int shift = (E->screencols-E->cx)+1;
            E->cx -= shift;
            E->coloff += shift;
		}
		return '\n';
    }
	else {
		char deleted_char = row->chars[filecol-1];
        editorRowDelChar(E, row,filecol-1);
        if (E->cx == 0 && E->coloff)
            E->coloff--;
        else
            E->cx--;
		return deleted_char;
    }
    if (row) editorUpdateRow(E, row);
    E->dirty++;
}

/* Load the specified program in the editor memory and returns 0 on success
 * or 1 on error. */
int editorOpen(editorConfig *E, char *filename) {
    FILE *fp;

    E->dirty = 0;
    free(E->filename);
    size_t fnlen = strlen(filename)+1;
    E->filename = (char*)malloc(fnlen);
    memcpy(E->filename,filename,fnlen);

    fp = fopen(filename,"r");
    if (!fp) {
        if (errno != ENOENT) {
            perror("Opening file");
            exit(1);
        }
        return 1;
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line,&linecap,fp)) != -1) {
        if (linelen && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
            line[--linelen] = '\0';
        editorInsertRow(E, E->numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E->dirty = 0;
    return 0;
}

/* Save the current file on disk. Return 0 on success, 1 on error. */
int editorSave(editorConfig *E) {
    int len;
    char *buf = editorRowsToString(E, &len);
    int fd = open(E->filename,O_RDWR|O_CREAT,0644);
    if (fd == -1) goto writeerr;

    /* Use truncate + a single write(2) call in order to make saving
     * a bit safer, under the limits of what we can do in a small editor. */
    if (ftruncate(fd,len) == -1) goto writeerr;
    if (write(fd,buf,len) != len) goto writeerr;

    close(fd);
    free(buf);
    E->dirty = 0;
    editorSetStatusMessage(E, "%d bytes written on disk", len);
    return 0;

writeerr:
    free(buf);
    if (fd != -1) close(fd);
    editorSetStatusMessage(E, "Can't save! I/O error: %s",strerror(errno));
    return 1;
}

/* This function writes the whole screen using VT100 escape characters
 * starting from the logical state of the editor in the global state 'E'. */
void editorRefreshScreen(editorConfig *E) {
	// Telling the user the editor mode
	const char* mode_status;
	if (E->mode == EDITOR_MODE_NORMAL)
		mode_status = "--NORMAL--";
	else if (E->mode = EDITOR_MODE_INSERT)
		mode_status = "--INSERT--";	
	//editorSetStatusMessage("%s", status_message);

    int y;
    erow *r;
    char buf[32];
    struct abuf ab = ABUF_INIT;

    abAppend(&ab,"\x1b[?25l",6); /* Hide cursor. */
    abAppend(&ab,"\x1b[H",3); /* Go home. */
    for (y = 0; y < E->screenrows; y++) {
        int filerow = E->rowoff+y;

        if (filerow >= E->numrows) {
            if (E->numrows == 0 && y == E->screenrows/3) {
                char welcome[80];
                int welcomelen = snprintf(welcome,sizeof(welcome),
                    "Kilo editor -- verison %s\x1b[0K\r\n", KILO_VERSION);
                int padding = (E->screencols-welcomelen)/2;
                if (padding) {
                    abAppend(&ab,"~",1);
                    padding--;
                }
                while(padding--) abAppend(&ab," ",1);
                abAppend(&ab,welcome,welcomelen);
            } else {
                abAppend(&ab,"~\x1b[0K\r\n",7);
            }
            continue;
        }

        r = &E->row[filerow];

        int len = r->rsize - E->coloff;
        int current_color = -1;
        if (len > 0) {
            if (len > E->screencols) len = E->screencols;
            char *c = r->render + E->coloff;
            unsigned char *hl = r->hl+E->coloff;
            int j;
            for (j = 0; j < len; j++) {
                if (hl[j] == HL_NONPRINT) {
                    char sym;
                    abAppend(&ab,"\x1b[7m",4);
                    if (c[j] <= 26)
                        sym = '@'+c[j];
                    else
                        sym = '?';
                    abAppend(&ab,&sym,1);
                    abAppend(&ab,"\x1b[0m",4);
                } else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        abAppend(&ab,"\x1b[39m",5);
                        current_color = -1;
                    }
                    abAppend(&ab,c+j,1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        char buf[16];
                        int clen = snprintf(buf,sizeof(buf),"\x1b[%dm",color);
                        current_color = color;
                        abAppend(&ab,buf,clen);
                    }
                    abAppend(&ab,c+j,1);
                }
            }
        }
        abAppend(&ab,"\x1b[39m",5);
        abAppend(&ab,"\x1b[0K",4);
        abAppend(&ab,"\r\n",2);
    }

    /* Create a two rows status. First row: */
    abAppend(&ab,"\x1b[0K",4);
    abAppend(&ab,"\x1b[7m",4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), " %s %.20s - %d lines %s",
        mode_status, E->filename, E->numrows, E->dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus),
        "%d/%d", E->rowoff + E->cy+1, E->numrows);
    if (len > E->screencols) len = E->screencols;
    abAppend(&ab,status,len);
    while(len < E->screencols) {
        if (E->screencols - len == rlen) {
            abAppend(&ab,rstatus,rlen);
            break;
        } else {
            abAppend(&ab," ",1);
            len++;
        }
    }
    abAppend(&ab,"\x1b[0m\r\n",6);

    /* Second row depends on E.statusmsg and the status message update time. */
    abAppend(&ab,"\x1b[0K",4);
    int msglen = strlen(E->statusmsg);
    if (msglen && time(NULL) - E->statusmsg_time < 5)
        abAppend(&ab, E->statusmsg,msglen <= E->screencols ? msglen : E->screencols);

    /* Put cursor at its current position. Note that the horizontal position
     * at which the cursor is displayed may be different compared to 'E.cx'
     * because of TABs. */
    int j;
    int cx = 1;
    int filerow = E->rowoff + E->cy;
    erow *row = (filerow >= E->numrows) ? NULL : &E->row[filerow];
    if (row) {
        for (j = E->coloff; j < E->cx + E->coloff; j++) {
            if (j < row->size && row->chars[j] == TAB) cx += 7-((cx)%8);
            cx++;
        }
    }
    snprintf(buf, sizeof(buf),"\x1b[%d;%dH", E->cy + 1, cx);
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab,"\x1b[?25h",6); /* Show cursor. */
    write(STDOUT_FILENO,ab.b,ab.len);
    abFree(&ab);
}

/* Set an editor status message for the second line of the status, at the
 * end of the screen. */
void editorSetStatusMessage(editorConfig *E, const char *fmt, ...) {
    va_list ap;
    va_start(ap,fmt);
    vsnprintf(E->statusmsg,sizeof(E->statusmsg),fmt,ap);
    va_end(ap);
    E->statusmsg_time = time(NULL);
}

void editorFind(editorConfig *E, int fd) {
    char query[KILO_QUERY_LEN+1] = {0};
    int qlen = 0;
    int last_match = -1; /* Last line where a match was found. -1 for none. */
    int find_next = 0; /* if 1 search next, if -1 search prev. */
    int saved_hl_line = -1;  /* No saved HL */
    char *saved_hl = NULL;

    /* Save the cursor position in order to restore it later. */
    int saved_cx = E->cx, saved_cy = E->cy;
    int saved_coloff = E->coloff, saved_rowoff = E->rowoff;

    while(1) {
        editorSetStatusMessage(E,
            "Search: %s (Use ESC/Arrows/Enter)", query);
        editorRefreshScreen(E);

        int c = editorReadKey(fd);
        if (c == DEL_KEY || c == CTRL_H || c == BACKSPACE) {
            if (qlen != 0) query[--qlen] = '\0';
            last_match = -1;
        } else if (c == ESC || c == ENTER) {
            if (c == ESC) {
                E->cx = saved_cx; E->cy = saved_cy;
                E->coloff = saved_coloff; E->rowoff = saved_rowoff;
            }
            FIND_RESTORE_HL;
            editorSetStatusMessage(E, "");
            return;
        } else if (c == ARROW_RIGHT || c == ARROW_DOWN) {
            find_next = 1;
        } else if (c == ARROW_LEFT || c == ARROW_UP) {
            find_next = -1;
        } else if (isprint(c)) {
            if (qlen < KILO_QUERY_LEN) {
                query[qlen++] = c;
                query[qlen] = '\0';
                last_match = -1;
            }
        }

        /* Search occurrence. */
        if (last_match == -1) find_next = 1;
        if (find_next) {
            char *match = NULL;
            int match_offset = 0;
            int i, current = last_match;

            for (i = 0; i < E->numrows; i++) {
                current += find_next;
                if (current == -1) current = E->numrows-1;
                else if (current == E->numrows) current = 0;
                match = strstr(E->row[current].render,query);
                if (match) {
                    match_offset = match - E->row[current].render;
                    break;
                }
            }
            find_next = 0;

            /* Highlight */
            FIND_RESTORE_HL;

            if (match) {
                erow *row = &E->row[current];
                last_match = current;
                if (row->hl) {
                    saved_hl_line = current;
                    saved_hl = (char*)malloc(row->rsize);
                    memcpy(saved_hl,row->hl,row->rsize);
                    memset(row->hl+match_offset,HL_MATCH,qlen);
                }
                E->cy = 0;
                E->cx = match_offset;
                E->rowoff = current;
                E->coloff = 0;
                /* Scroll horizontally as needed. */
                if (E->cx > E->screencols) {
                    int diff = E->cx - E->screencols;
                    E->cx -= diff;
                    E->coloff += diff;
                }
            }
        }
    }
}

/* Handle cursor position change because arrow keys were pressed. */
void editorMoveCursor(editorConfig *E, int key) {
    int filerow = E->rowoff+E->cy;
    int filecol = E->coloff+E->cx;
    int rowlen;
    erow *row = (filerow >= E->numrows) ? NULL : &E->row[filerow];

    switch(key) {
    case ARROW_LEFT:
        if (E->cx == 0) {
            if (E->coloff) {
                E->coloff--;
            } else {
                if (filerow > 0) {
                    E->cy--;
                    E->cx = E->row[filerow-1].size;
                    if (E->cx > E->screencols-1) {
                        E->coloff = E->cx-E->screencols+1;
                        E->cx = E->screencols-1;
                    }
                }
            }
        } else {
            E->cx -= 1;
        }
        break;
    case ARROW_RIGHT:
        if (row && filecol < row->size) {
            if (E->cx == E->screencols-1) {
                E->coloff++;
            } else {
                E->cx += 1;
            }
        } else if (row && filecol == row->size) {
            E->cx = 0;
            E->coloff = 0;
            if (E->cy == E->screenrows-1) {
                E->rowoff++;
            } else {
                E->cy += 1;
            }
        }
        break;
    case ARROW_UP:
        if (E->cy == 0) {
            if (E->rowoff) E->rowoff--;
        } else {
            E->cy -= 1;
        }
        break;
    case ARROW_DOWN:
        if (filerow < E->numrows) {
            if (E->cy == E->screenrows-1) {
                E->rowoff++;
            } else {
                E->cy += 1;
            }
        }
        break;
    }
    /* Fix cx if the current line has not enough chars. */
    filerow = E->rowoff+E->cy;
    filecol = E->coloff+E->cx;
    row = (filerow >= E->numrows) ? NULL : &E->row[filerow];
    rowlen = row ? row->size : 0;
    if (filecol > rowlen) {
        E->cx -= filecol-rowlen;
        if (E->cx < 0) {
            E->coloff += E->cx;
            E->cx = 0;
        }
    }
}

int editorFileWasModified(editorConfig *E) {
    return E->dirty;
}

void updateWindowSize(editorConfig *E) {
    if (getWindowSize(STDIN_FILENO,STDOUT_FILENO,
                      &E->screenrows, &E->screencols) == -1) {
        perror("Unable to query the screen for size (columns / rows)");
        exit(1);
    }
    E->screenrows -= 2; /* Get room for status bar. */
}

void editorHandleResize(editorConfig *E) {
    updateWindowSize(E);
    if (E->cy > E->screenrows) E->cy = E->screenrows - 1;
    if (E->cx > E->screencols) E->cx = E->screencols - 1;
    editorRefreshScreen(E);
}

void initEditor(editorConfig *E) {
    E->cx = 0;
    E->cy = 0;
    E->rowoff = 0;
    E->coloff = 0;
    E->numrows = 0;
    E->row = NULL;
    E->dirty = 0;
    E->filename = NULL;
    E->syntax = NULL;
    updateWindowSize(E);
	editorRefreshScreen(E);
}

void editorUndo(editorConfig *E) {
	if (E->m_command_index) {
		UndoCommandBus bus = E->m_command_queue[E->m_command_index - 1];
		E->m_command_index--;
		for (int i = bus.size() - 1; i >= 0; i--) {
			switch (bus[i].ID) {
			case UNDO_CMD_INSERT:
				E->cx = bus[i].pos.x;
				E->cy = bus[i].pos.y;
				editorDelChar(E);
				break;
			case UNDO_CMD_DELETE_LEFT_CHAR:
				E->cx = bus[i].pos.x;
				E->cy = bus[i].pos.y;
				if (bus[i].c == '\n')
					editorInsertNewline(E);
				else
					editorInsertChar(E, bus[i].c);
				break;
			case UNDO_CMD_DELETE_RIGHT_CHAR:
				E->cx = bus[i].pos.x;
				E->cy = bus[i].pos.y;
				if (bus[i].c == '\n')
					editorInsertNewline(E);
				else
					editorInsertChar(E, bus[i].c);
				break;
			default:
				break;
			}
		}
	}
}

void editorRedo(editorConfig *E) {
	if (E->m_command_index < E->m_command_queue.size()) {
		UndoCommandBus bus = E->m_command_queue[E->m_command_index++];
		for(int i = 0; i < bus.size(); i++) {
			switch (bus[i].ID)
			{
			case UNDO_CMD_INSERT:
				E->cx = bus[i].pos.x;
				E->cy = bus[i].pos.y;
				if (bus[i].c == '\n')
					editorInsertNewline(E);
				else {
					editorMoveCursor(E, ARROW_LEFT);
					editorInsertChar(E, bus[i].c);
				}
				break;
			case UNDO_CMD_DELETE_LEFT_CHAR:
				E->cx = bus[i].pos.x;
				E->cy = bus[i].pos.y;
				editorDelChar(E);
				break;
			case UNDO_CMD_DELETE_RIGHT_CHAR:
				E->cx = bus[i].pos.x;
				E->cy = bus[i].pos.y;
				editorDelChar(E);
				break;
			default:
				break;
			}
		}
	}
}

void DeleteRedoQueue(editorConfig *E) {
	if ((E->m_command_queue.size() - E->m_command_index)) {
		E->m_command_queue.erase(E->m_command_queue.begin() + E->m_command_index, E->m_command_queue.end());
	}
}
void PushCommand(editorConfig *E, UndoCommandBus* bus, char ID, char c) {
	UndoCommand cmd;
	cmd.c = c;
	cmd.ID = ID;
	cmd.pos = {E->cx, E->cy};
	bus->push_back(cmd);
}
void PushCommandBus(editorConfig *E, UndoCommandBus bus) {
	E->m_command_queue.insert(E->m_command_queue.begin() + E->m_command_index, bus);
	E->m_command_index++;
	DeleteRedoQueue(E);
}




// =======================================
void abAppend(struct abuf *ab, const char *s, int len) {
    char *new_ = (char*)realloc(ab->b,ab->len+len);
    if (new_ == NULL) return;
    memcpy(new_ + ab->len,s,len);
    ab->b = new_;
    ab->len += len;
}
void abFree(struct abuf *ab) {
    free(ab->b);
}
