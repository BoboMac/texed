# TODO:
# - add all the infrastructure needed
#   - read as many github editor projects as possible and design *everything*
# - copy *all* the implementation

import curses
import signal
import sys

# also does some other stuff for the terminal
def init_curses():
    # disabling CTRL-Z, CTRL-C
    def signal_handler(sig, frame):
        pass
    signal.signal(signal.SIGINT,  signal_handler)
    signal.signal(signal.SIGTSTP, signal_handler)

    global stdscr
    stdscr = curses.initscr()
    curses.noecho()
    curses.cbreak()
    stdscr.keypad(True)
    pass

# also does some other stuff for the terminal
def end_curses():
    curses.nocbreak()
    stdscr.keypad(False)
    curses.echo()
    curses.endwin()

class Buffer:
    scroll_x = 0
    scroll_y = 0
    cursor_x = 0
    cursor_y = 0
    def load_file(self, filename):
        pass
    def render_line(self, line, line_number):
        pass
    def render(self):
        pass
    def render_cursor(self):
        pass
    def cursor_move_left(self):
        pass
    def cursor_move_right(self):
        pass
    def cursor_move_up(self):
        pass
    def cursor_move_down(self):
        pass
    def insert_char(self, c):
        pass
    def insert_newline(self):
        pass
    def delete_char(self):
        pass
    def undo(self):
        pass
    def redo(self):
        pass
    def start_selection(self):
        pass
    def selection_exists(self):
        pass
    def end_selection(self):
        pass
    def copy(self):
        pass
    def paste(self):
        paste

buffers = []
buffer_index = -1

# responsible for switching buffers and editing in one of them
def process_keypress():
    pass

init_curses()
while True:
    character = stdscr.getch()
    if character == ord('q'):
        break
end_curses()
