#pragma once

#define KILO_VERSION "0.0.1"

#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#endif

// C
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>

// C++
#include <vector>

/* Syntax highlight types */
#define HL_NORMAL 0
#define HL_NONPRINT 1
#define HL_COMMENT 2   /* Single line comment. */
#define HL_MLCOMMENT 3 /* Multi-line comment. */
#define HL_KEYWORD1 4
#define HL_KEYWORD2 5
#define HL_STRING 6
#define HL_NUMBER 7
#define HL_MATCH 8      /* Search match. */

#define HL_HIGHLIGHT_STRINGS (1<<0)
#define HL_HIGHLIGHT_NUMBERS (1<<1)

// @TODO: rename to Int2 goddamnit
struct Uint2 {
	int x = 0, y = 0;
};

// Undo system
enum {
	UNDO_CMD_INSERT,
	UNDO_CMD_DELETE_LEFT_CHAR,
	UNDO_CMD_DELETE_RIGHT_CHAR,
};

struct UndoCommand {
	char ID = 0;
	Uint2 pos;
	char c;
};
typedef std::vector<UndoCommand> UndoCommandBus;

struct editorSyntax {
    char **filematch;
    char **keywords;
    char singleline_comment_start[2];
    char multiline_comment_start[3];
    char multiline_comment_end[3];
    int flags;
};

/* This structure represents a single line of the file we are editing. */
struct erow {
    int idx;            /* Row index in the file, zero-based. */
    int size;           /* Size of the row, excluding the null term. */
    int rsize;          /* Size of the rendered row. */
    char *chars;        /* Row content. */
    char *render;       /* Row content "rendered" for screen (for TABs). */
    unsigned char *hl;  /* Syntax highlight type for each character in render.*/
    int hl_oc;          /* Row had open comment at end in last syntax highlight check. */
};

struct hlcolor {
    int r,g,b;
};

enum {
	EDITOR_MODE_NORMAL,
	EDITOR_MODE_INSERT
};

struct editorConfig {
    int cx,cy;      /* Cursor x and y position in characters */
    int rowoff;     /* Offset of row displayed. */
    int coloff;     /* Offset of column displayed. */
    int screenrows; /* Number of rows that we can show */
    int screencols; /* Number of cols that we can show */
    int numrows;    /* Number of rows */
    int rawmode;    /* Is terminal raw mode enabled? */
    erow *row;      /* Rows */
    int dirty;      /* File modified but not saved. */
    char *filename; /* Currently open filename */
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax;    /* Current syntax highlight, or NULL. */

	// Undo system
	std::vector<UndoCommandBus> m_command_queue;
	unsigned int m_command_index = 0;

	int mode;
};


enum KEY_ACTION {
        KEY_NULL = 0,       /* NULL */
        CTRL_C = 3,         /* Ctrl-c */
        CTRL_D = 4,         /* Ctrl-d */
        CTRL_F = 6,         /* Ctrl-f */
        CTRL_H = 8,         /* Ctrl-h */
        TAB = 9,            /* Tab */
        CTRL_L = 12,        /* Ctrl+l */
        ENTER = 13,         /* Enter */
        CTRL_Q = 17,        /* Ctrl-q */
        CTRL_S = 19,        /* Ctrl-s */
        CTRL_U = 21,        /* Ctrl-u */
        ESC = 27,           /* Escape */
        BACKSPACE =  127,   /* Backspace */
        /* The following are just soft codes, not really reported by the
         * terminal directly. */
        ARROW_LEFT = 1000,
        ARROW_RIGHT,
        ARROW_UP,
        ARROW_DOWN,
		ARROW_CTRL_LEFT,
		ARROW_CTRL_RIGHT,
		ARROW_CTRL_UP,
		ARROW_CTRL_DOWN,
        DEL_KEY,
        HOME_KEY,
        END_KEY,
        PAGE_UP,
        PAGE_DOWN
};

/* C / C++ */
extern char *C_HL_extensions[];
extern char *C_HL_keywords[];
extern editorSyntax HLDB[1];

#define HLDB_ENTRIES (sizeof(HLDB)/sizeof(HLDB[0]))

/* ======================= Low level terminal handling ====================== */

static struct termios orig_termios; /* In order to restore at exit.*/

/* We define a very simple "append buffer" structure, that is an heap
 * allocated string where we can append to. This is useful in order to
 * write all the escape sequences in a buffer and flush them to the standard
 * output in a single call, to avoid flickering effects. */
struct abuf {
    char *b;
    int len;
};
#define ABUF_INIT {NULL,0}
void abAppend(struct abuf *ab, const char *s, int len);
void abFree(struct abuf *ab);

// Find mode
#define KILO_QUERY_LEN 256
#define FIND_RESTORE_HL do { \
    if (saved_hl) { \
        memcpy(E->row[saved_hl_line].hl, saved_hl, E->row[saved_hl_line].rsize); \
        free(saved_hl); \
        saved_hl = NULL; \
    } \
} while (0)
#define KILO_QUIT_TIMES 3

int is_separator(int c);
void disableRawMode(editorConfig *E, int fd);
void editorAtExit(editorConfig *E);
int enableRawMode(editorConfig *E, int fd);
int editorReadKey(int fd);
int getCursorPosition(int ifd, int ofd, int *rows, int *cols);
int getWindowSize(int ifd, int ofd, int *rows, int *cols);
int editorRowHasOpenComment(erow *row);
void editorUpdateSyntax(editorConfig *E, erow *row);
int editorSyntaxToColor(int hl);
void editorSelectSyntaxHighlight(editorConfig *E, char *filename);
void editorUpdateRow(editorConfig *E, erow *row);
void editorInsertRow(editorConfig *E, int at, char *s, size_t len);
void editorFreeRow(erow *row);
void editorDelRow(editorConfig *E, int at);
char *editorRowsToString(editorConfig *E, int *buflen);
void editorRowInsertChar(editorConfig *E, erow *row, int at, int c);
void editorRowAppendString(editorConfig *E, erow *row, char *s, size_t len);
void editorRowDelChar(editorConfig *E, erow *row, int at);
void editorInsertChar(editorConfig *E, int c);
void editorInsertNewline(editorConfig *E);
char editorDelChar(editorConfig *E);
int editorOpen(editorConfig *E, char *filename);
int editorSave(editorConfig *E);
void editorRefreshScreen(editorConfig *E);
void editorSetStatusMessage(editorConfig *E, const char *fmt, ...);
void editorFind(editorConfig *E, int fd);
void editorMoveCursor(editorConfig *E, int key);
void editorProcessKeypress(editorConfig *E, int fd);
int editorFileWasModified(editorConfig *E);
void updateWindowSize(editorConfig *E);
void editorHandleResize(editorConfig *E);
void initEditor(editorConfig *E);
void editorUndo(editorConfig *E);
void editorRedo(editorConfig *E);
void editorCopy();
void editorPaste();
void DeleteRedoQueue(editorConfig *E);
void PushCommand(editorConfig *E, UndoCommandBus* bus, char ID, char c);
void PushCommandBus(editorConfig *E, UndoCommandBus bus);
