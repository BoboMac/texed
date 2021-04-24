#pragma once

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

struct WordPos {
	UINT begin = 0, end = 0;
	char hl_type;
};
bool CharInWord(unsigned index, WordPos word_pos) {
	return (word_pos.begin <= index && index <= word_pos.end);
}
std::vector<std::string> c_keywords = {
	"return", "if", "else", "while", "for",
	"break", "continue", "class", "struct",
	"private", "public", "protected", "overwrite",
	"delete", "virtual", "new", "void", "char",
	"int", "unsigned", "short", "long"
};
void GetKeywordPositions(std::string str, std::vector<WordPos>* keyword_pos) {
	UINT begin = 0;
	// Push all the keywords
	for (int i = 0; i < c_keywords.size(); i++) {
		begin = str.find(c_keywords[i]);
		if (begin != std::string::npos) {
			WordPos wp;
			wp.begin = begin;
			wp.end = begin + c_keywords[i].size();
			wp.hl_type = NC_PAIR_KEYWORDS;
			keyword_pos->push_back(wp);
		}
	}
	// Push the digits
	{
		std::smatch match;
		std::regex r(" [0-9]{1,10} ");
		while (std::regex_search(str, match, r)) {
			WordPos wp;
			wp.begin = match.position();
			wp.end = wp.begin + match.size();
			wp.hl_type = NC_PAIR_DIGITS;
			keyword_pos->push_back(wp);
		}
	}
}

class Buffer {
private:
	Uint2 m_cursor;
	Uint2 m_scroll;
	Int2 m_selection_start = Int2(-1, -1);
	std::vector<std::string> m_text;
	std::string m_filename;
	std::vector<UndoCommandBus> m_command_queue;
	UINT m_command_index = 0;
	char m_tab_num = 0;
public:
	bool LoadFile(std::string filename) {
		m_filename = filename;
		std::ifstream file;
		file.open(filename);
		if (!file.is_open()) return 0;
		std::string ln;
		while (!file.eof()) {
			std::getline(file, ln);
			ln = ReplaceTabs(ln);
			m_text.push_back(ln + '\n');
		}

		// Crash proof
		if (m_text.size() < 2) {
			InsertNewLine();
		}
		return 1;
	}
private:
	void Save() {
		// Replace every (TAB_SIZE) spaces at the begining of the line with tabs
		std::ofstream file;
		file.open(m_filename, std::ofstream::trunc);
		for (int i = 0; i < m_text.size(); i++) {
			file << ReplaceSpaces(m_text[i]);
		}
		file.close();
	}
	char GetTabNum() {
		char result = 0;
		for (int i = 0; i <= m_cursor.y; i++) {
			for (int j = 0; j < m_text[i].size(); j++) {
				if (m_text[i][j] == '{' || m_text[i][j] == '[' || m_text[i][j] == '(')
					result++;
				else if (result && m_text[i][j] == '}' || m_text[i][j] == ']' || m_text[i][j] == ')')
					result--;
			}
		}
		return result;
	}
	void PushCommand(UndoCommandBus* bus, char ID, char c) {
		UndoCommand cmd;
		cmd.c = c;
		cmd.ID = ID;
		cmd.pos = m_cursor;
		bus->push_back(cmd);
	}
	void PushCommandBus(UndoCommandBus bus) {
		m_command_queue.insert(m_command_queue.begin() + m_command_index, bus);
		m_command_index++;
		DeleteRedoQueue();
	}
	void DeleteRedoQueue() {
		if ((m_command_queue.size() - m_command_index)) {
			m_command_queue.erase(m_command_queue.begin() + m_command_index, m_command_queue.end());
		}
	}
	void ScrollUp() {
		if (m_scroll.y > 0) m_scroll.y--;
	}
	void ScrollDown() {
		m_scroll.y++;
	}
	void ScrollLeft() {
		if (m_scroll.x > 0) m_scroll.x--;
	}
	void ScrollRight() {
		m_scroll.x++;
	}
	UINT GetAbsPos(Int2 point) {
		UINT result = 0;
		for (int i = 0; i < point.y; i++) {
			result += m_text[i].size();
		}
		result += point.x;
		return result;
	}
	UINT GetAbsPos(Uint2 _point) {
		UINT result = 0;
		for (int i = 0; i < (int)_point.y; i++) {
			result += m_text[i].size();
		}
		result += _point.x;
		return result;
	}
	void StartSelection() {
		m_selection_start = { (int)m_cursor.x, (int)m_cursor.y };
	}
	void EndSelection() {
		m_selection_start = { -1, -1 };
	}
	bool SelectionExists() {
		if (m_selection_start.x != -1 && m_selection_start.y != -1) return 1;
		return 0;
	}
	void MoveCursorLeft() {
		if (m_cursor.x > 0) {
			m_cursor.x--;
		}
		else if (m_cursor.y > 0) {
			m_cursor.y--;
			m_cursor.x = m_text[m_cursor.y].size() - 1;
		}
	}
	void MoveCursorRight() {
		if (m_cursor.x < (int)m_text[m_cursor.y].size() - 1) {
			m_cursor.x++;
		}
		else if (m_cursor.y < m_text.size() - 2) {
			m_cursor.x = 0;
			m_cursor.y++;
		}
	}
	void ResetCursor() {
		MoveCursorRight();
		MoveCursorLeft();
	}
	void MoveCursorUp() {
		if (m_cursor.y > 0) {
			m_cursor.y--;
			if (m_text[m_cursor.y].size() < m_cursor.x) {
				m_cursor.x = m_text[m_cursor.y].size() - 1;
			}
		}
		ResetCursor();
	}
	void MoveCursorDown() {
		if (m_cursor.y < m_text.size() - 1) {
			m_cursor.y++;
			if (m_text[m_cursor.y].size() < m_cursor.x) {
				m_cursor.x = m_text[m_cursor.y].size() - 1;
			}
		}
		ResetCursor();
	}
	void MoveCursorToPrevWord() {
		if (m_cursor.x)
			MoveCursorLeft();
		if (isalpha(m_text[m_cursor.y][m_cursor.x]) && m_cursor.x) {
			MoveCursorToPrevWord();
		}
	}
	void MoveCursorToPrevSpace() {
		if (m_cursor.x)
			MoveCursorLeft();
		if (m_text[m_cursor.y][m_cursor.x] == ' ' && m_cursor.x) {
			MoveCursorToPrevSpace();
		}
	}
	void MoveCursorToNextWord() {
		MoveCursorRight();
		if (m_text[m_cursor.y][m_cursor.x] == ' ') {
			while (m_cursor.x < m_text[m_cursor.y].size() && m_text[m_cursor.y][m_cursor.x] == ' ') {
				MoveCursorRight();
			}
		}
		else if (isalpha(m_text[m_cursor.y][m_cursor.x])) {
			while (m_cursor.x < m_text[m_cursor.y].size() && (isalpha(m_text[m_cursor.y][m_cursor.x]) || isdigit(m_text[m_cursor.y][m_cursor.x]))) {
				MoveCursorRight();
			}
		}
		else {
			while (m_cursor.x < m_text[m_cursor.y].size() && !(isalpha(m_text[m_cursor.y][m_cursor.x]) || isdigit(m_text[m_cursor.y][m_cursor.x]))) {
				MoveCursorRight();
			}
		}
	}
	void AutoIndent(char c) {
		switch (c) {
		case '{':
			InsertCharacter('}');
			MoveCursorLeft();
			break;
		case '[':
			InsertCharacter(']');
			MoveCursorLeft();
			break;
		case '(':
			InsertCharacter(')');
			MoveCursorLeft();
			break;
		case '\"':
		case '\'':
			// Cannot use InserCharacter() bcuz it's recursive
			m_text[m_cursor.y].insert(m_text[m_cursor.y].begin() + m_cursor.x, c);
			break;
		default:
			break;
		}
	}
	void InsertCharacter(char c, bool indent = 1) {
		if (indent && m_text[m_cursor.y][m_cursor.x] == ')' && c == ')') {
			MoveCursorRight();
		}
		else if (indent && m_text[m_cursor.y][m_cursor.x] == ']' && c == ']') {
			MoveCursorRight();
		}
		else if (indent && m_text[m_cursor.y][m_cursor.x] == '}' && c == '}') {
			MoveCursorRight();
		}
		else if (indent && m_text[m_cursor.y][m_cursor.x] == '\'' && c == '\'') {
			MoveCursorRight();
		}
		else if (indent && m_text[m_cursor.y][m_cursor.x] == '\"' && c == '\"') {
			MoveCursorRight();
		}
		else {
			m_text[m_cursor.y].insert(m_text[m_cursor.y].begin() + m_cursor.x, c);
			MoveCursorRight();
		}
		if (indent) AutoIndent(c);
	}
	void InsertNewLine(bool indent = 1) {
		// Copy the left part of the line
		std::string str;
		for (int i = m_cursor.x; i < m_text[m_cursor.y].size(); i++) {
			str += m_text[m_cursor.y][i];
		}
		// Delete the left part of the line
		m_text[m_cursor.y].erase(m_text[m_cursor.y].begin() + m_cursor.x, m_text[m_cursor.y].end() - 1);
		// Insert new line with the copied string
		m_text.insert(m_text.begin() + m_cursor.y + 1, str);
		// Move cursor to the begining of next line
		MoveCursorRight();

		// Indentation
		if (indent) {
			for (int i = 0; i < GetTabNum(); i++) {
				InsertTab();
			}
		}
	}
	void InsertTab() {
		for (int i = 0; i < TAB_SIZE; i++) {
			InsertCharacter(' ');
		}
	}
	void DeleteRightCharacter(bool push_to_cmd_queue = 1) {
		char deleted_char = 0;
		if (m_cursor.x == m_text[m_cursor.y].size() - 1 && m_cursor.y < m_text.size() - 1) {
			deleted_char = '\n';
			// Append the next line to this one
			m_text[m_cursor.y].erase(m_text[m_cursor.y].size() - 1, 1);
			m_text[m_cursor.y].append(m_text[m_cursor.y + 1]);
			// Delete the next line
			m_text.erase(m_text.begin() + m_cursor.y + 1, m_text.begin() + m_cursor.y + 2);
		}
		else if (m_cursor.x < m_text[m_cursor.y].size() - 1) {
			deleted_char = m_text[m_cursor.y][m_cursor.x];
			m_text[m_cursor.y].erase(m_cursor.x, 1);
		}
	}
	void DeleteLeftCharacter(bool push_to_cmd_queue = 1, bool indent = 1) {
		// Deleting a new line
		if (m_cursor.x == 0 && m_cursor.y) {
			// Move cursor at the end of the previous line
			MoveCursorLeft();
			// Append the next line to this one
			m_text[m_cursor.y].erase(m_text[m_cursor.y].size() - 1, 1);
			m_text[m_cursor.y].append(m_text[m_cursor.y + 1]);
			// Delete the next line
			m_text.erase(m_text.begin() + m_cursor.y + 1, m_text.begin() + m_cursor.y + 2);
		}
		// Deleting a character
		else if (m_cursor.x) {
			char deleted_char = m_text[m_cursor.y][m_cursor.x - 1];
			if (indent && deleted_char == '(' && m_text[m_cursor.y][m_cursor.x] == ')') {
				MoveCursorLeft();
				DeleteRightCharacter(push_to_cmd_queue);
				MoveCursorLeft();
				DeleteRightCharacter(push_to_cmd_queue);
			}
			else if (indent && deleted_char == '[' && m_text[m_cursor.y][m_cursor.x] == ']') {
				MoveCursorLeft();
				DeleteRightCharacter(push_to_cmd_queue);
				MoveCursorLeft();
				DeleteRightCharacter(push_to_cmd_queue);
			}
			else if (indent && deleted_char == '{' && m_text[m_cursor.y][m_cursor.x] == '}') {
				MoveCursorLeft();
				DeleteRightCharacter(push_to_cmd_queue);
				MoveCursorLeft();
				DeleteRightCharacter(push_to_cmd_queue);
			}
			else if (indent && deleted_char == '\'' && m_text[m_cursor.y][m_cursor.x] == '\'') {
				MoveCursorLeft();
				DeleteRightCharacter(push_to_cmd_queue);
				MoveCursorLeft();
				DeleteRightCharacter(push_to_cmd_queue);
			}
			else if (indent && deleted_char == '\"' && m_text[m_cursor.y][m_cursor.x] == '\"') {
				MoveCursorLeft();
				DeleteRightCharacter(push_to_cmd_queue);
				MoveCursorLeft();
				DeleteRightCharacter(push_to_cmd_queue);
			}
			else {
				MoveCursorLeft();
				DeleteRightCharacter(push_to_cmd_queue);
			}
		}
	}
	// *commands cannot be 0 when push_cmd_bus is 0
	void DeleteSelection(bool push_cmd_bus = 1, UndoCommandBus *commands = 0) {
		UndoCommandBus bus;

		// Get number of characters that have to be deleted and whether the cursor is beyond or after selection
		UINT abs_selection_start = GetAbsPos(m_selection_start);
		UINT abs_cursor = GetAbsPos(m_cursor);
		if (abs_cursor > abs_selection_start) {
			for (int i = 0; i < abs_cursor - abs_selection_start + m_cursor.y - m_selection_start.y; i++) {
				char deleted_char;
				if (m_cursor.x) {
					deleted_char = m_text[m_cursor.y][m_cursor.x - 1];
				}
				else {
					deleted_char = '\n';
				}
				DeleteLeftCharacter();
				if (push_cmd_bus)
					PushCommand(&bus, UNDO_CMD_DELETE_LEFT_CHAR, deleted_char);
				else
					PushCommand(commands, UNDO_CMD_DELETE_LEFT_CHAR, deleted_char);
			}
		}
		else {
			for (int i = 0; i < abs_selection_start - abs_cursor; i++) {
				char deleted_char = m_text[m_cursor.y][m_cursor.x];
				DeleteRightCharacter();
				if (push_cmd_bus)
					PushCommand(&bus, UNDO_CMD_DELETE_RIGHT_CHAR, deleted_char);
				else
					PushCommand(commands, UNDO_CMD_DELETE_LEFT_CHAR, deleted_char);
			}
		}
		EndSelection();
		if (push_cmd_bus)
			PushCommandBus(bus);
	}
	std::string GetSelection() {
		std::string str;
		int selection_length = GetAbsPos(m_selection_start) - GetAbsPos(m_cursor);
		Uint2 init_cursor_pos = m_cursor;
		if (selection_length < 0) {
			for (int i = 0; i < -selection_length; i++) {
				MoveCursorLeft();
				str += m_text[m_cursor.y][m_cursor.x];
			}
			std::reverse(str.begin(), str.end());
		}
		else {
			for (int i = 0; i < selection_length; i++) {
				str += m_text[m_cursor.y][m_cursor.x];
				MoveCursorRight();
			}
		}
		m_cursor = { (UINT)m_selection_start.x, (UINT)m_selection_start.y };
		return str;
	}
	void CopySelection(HWND console_window) {
		ClipboardSet(console_window, GetSelection());
	}
	void CutSelection(HWND console_window) {
		CopySelection(console_window);
		DeleteSelection();
	}
	void SelectAllText() {
		m_selection_start = { 0, 0 };
		m_cursor = { (UINT)m_text[m_text.size() - 1].size() - 1, (UINT)m_text.size() - 1 };
	}
	void Paste(HWND console_window) {
		UndoCommandBus bus;
		if (SelectionExists())
			DeleteSelection(0, &bus); // Pushes the deleting commands directly to the paste's command bus
		EndSelection();
		std::string str = ClipboardGet(console_window);
		str = ReplaceTabs(str);
		if (1) {
			for (int i = 0; i < str.size(); i++) {
				if (str[i] == '\n') {
					InsertNewLine(0);
					PushCommand(&bus, UNDO_CMD_INSERT, '\n');
				}
				else {
					InsertCharacter(str[i], 0);
					PushCommand(&bus, UNDO_CMD_INSERT, str[i]);
				}
			}
		}
		PushCommandBus(bus);
	}
	void CutSelection() {
		if (SelectionExists()) {
			//DeleteSelection(0);
			//Paste(console_window);
		}
	}
	bool PointInSelection(Uint2 point) {
		return SelectionExists()
			&& (GetAbsPos(Uint2(point.x, point.y)) >= GetAbsPos(m_selection_start) && GetAbsPos(Uint2(point.x, point.y)) < GetAbsPos(m_cursor)
				|| GetAbsPos(Uint2(point.x, point.y)) < GetAbsPos(m_selection_start) && GetAbsPos(Uint2(point.x, point.y)) >= GetAbsPos(m_cursor));
	}
	void PrintLine(UINT x, UINT y, std::string line, UINT linenum, UINT linenum_max_len) {
		// Get vector of positions of keywords
		std::vector<WordPos> keyword_pos;
		GetKeywordPositions(line, &keyword_pos);
		
		char space_num = linenum_max_len - GetDigitNum(linenum);
		mvaddstr(y - m_scroll.y, space_num, std::to_string(linenum).c_str());
		std::string ln = line;
		if (ln.size()) ln[ln.size() - 1] = ' '; // Fill the '\n' slot with a space so we can draw it when selected
		for (int i = 0; i < ln.size(); i++) {
			if (PointInSelection(Uint2(x + i + m_scroll.x, y))) {
				attron(COLOR_PAIR(NC_PAIR_SELECTION));
				mvaddch(y - m_scroll.y, x + i + linenum_max_len + 1, ln[i]);
				attroff(COLOR_PAIR(NC_PAIR_SELECTION));
			}
			else {
				char hl = 0;
				// Checking if character is part of a highlighted word
				for (int j = 0; j < keyword_pos.size(); j++) {
					hl = CharInWord(x + i + m_scroll.x, keyword_pos[j]);
					if (hl) {
						attron(COLOR_PAIR(hl));
						mvaddch(y - m_scroll.y, x + i + linenum_max_len + 1, ln[i]);
						attroff(COLOR_PAIR(hl));
					}
				}
				if (!hl) {
					attron(COLOR_PAIR(NC_PAIR_TEXT));
					mvaddch(y - m_scroll.y, x + i + linenum_max_len + 1, ln[i]);
					attroff(COLOR_PAIR(NC_PAIR_TEXT));
				}
			}
		}
	}
	std::string GenerateFileInfo(UINT width) {
		std::string result = "   " + m_filename + "  " + std::to_string(m_cursor.x) + "," + std::to_string(m_cursor.y);
		for (int i = result.size(); i < width; i++) {
			result += ' ';
		}
		return result;
	}
	void Render(Uint2 scr_size, const EditorConfig &config) {
		// Line numbers
		char linenum_max_len = 1;
		if (m_text.size() > 10000)
			linenum_max_len = 5;
		else if (m_text.size() > 1000)
			linenum_max_len = 4;
		else if (m_text.size() > 100)
			linenum_max_len = 3;
		else if (m_text.size() > 10)
			linenum_max_len = 2;
		else if (m_text.size() > 0)
			linenum_max_len = 1;

		// Drawing each line individually
		// For HL, parse each line of the screen text to find the position of keywords etc.
		clear();
		for (int i = m_scroll.y; i - m_scroll.y < scr_size.y && i < m_text.size(); i++) {
			std::string ln = m_text[i];
			// Apply horizontal scroll
			if (ln.size() > m_scroll.x) ln.erase(0, m_scroll.x); else ln.erase(0);
			// Output the line
			PrintLine(0, i, ln, i, linenum_max_len);
		}
		// Render file info
		attron(COLOR_PAIR(NC_PAIR_GUI));
		mvprintw(scr_size.y - 1, 0, GenerateFileInfo(scr_size.x).c_str());
		attroff(COLOR_PAIR(NC_PAIR_GUI));
		// Set the cursor position
		move(m_cursor.y - m_scroll.y, m_cursor.x - m_scroll.x + linenum_max_len + 1);
	}
	void Undo() {
		if (m_command_index) {
			UndoCommandBus bus = m_command_queue[m_command_index - 1];
			m_command_index--;
			for (int i = bus.size() - 1; i >= 0; i--) {
				switch (bus[i].ID)
				{
				case UNDO_CMD_INSERT:
					//m_cursor = { bus[i].pos.x - 1, bus[i].pos.y };
					m_cursor = bus[i].pos;
					DeleteLeftCharacter(0);
					break;
				case UNDO_CMD_DELETE_LEFT_CHAR:
					m_cursor = bus[i].pos;
					if (bus[i].c == '\n')
						InsertNewLine(0);
					else
						InsertCharacter(bus[i].c, 0);
					break;
				case UNDO_CMD_DELETE_RIGHT_CHAR:
					m_cursor = bus[i].pos;
					if (bus[i].c == '\n')
						InsertNewLine(0);
					else
						InsertCharacter(bus[i].c, 0);
					break;
				default:
					break;
				}
			}
		}
	}
	void Redo() {
		if (m_command_index < m_command_queue.size()) {
			UndoCommandBus bus = m_command_queue[m_command_index++];
			for(int i = 0; i < bus.size(); i++) {
				switch (bus[i].ID)
				{
				case UNDO_CMD_INSERT:
					m_cursor = bus[i].pos;
					if (bus[i].c == '\n')
						InsertNewLine(0);
					else {
						MoveCursorLeft();
						InsertCharacter(bus[i].c, 0);
					}
					break;
				case UNDO_CMD_DELETE_LEFT_CHAR:
					m_cursor = bus[i].pos;
					DeleteRightCharacter(0);
					break;
				case UNDO_CMD_DELETE_RIGHT_CHAR:
					m_cursor = bus[i].pos;
					//DeleteRightCharacter();
					DeleteRightCharacter(0);
					break;
				default:
					break;
				}
			}
		}
	}
	bool UpdateInput(Uint2 scr_size, HWND console_window) {
		UINT ch = getch();
		bool shift = GetAsyncKeyState(VK_SHIFT);
		bool ctrl = GetAsyncKeyState(VK_CONTROL);

		if (m_cursor.y) {
			bool only_spaces = 1;
			UINT i = m_cursor.y - 1;
			for (int j = 0; j < m_text[i].size() - 1; j++) {
				if (m_text[i][j] != ' ') {
					only_spaces = 0;
				}
			}
			if (only_spaces) {
				m_text[i].clear();
				m_text[i] += ('\n');
			}
		}
		if (m_cursor.y < m_text.size() - 1) {
			bool only_spaces = 1;
			UINT i = m_cursor.y + 1;
			for (int j = 0; j < m_text[i].size() - 1; j++) {
				if (m_text[i][j] != ' ') {
					only_spaces = 0;
				}
			}
			if (only_spaces) {
				m_text[i].clear();
				m_text[i] += ('\n');
			}
		}
	
		switch (ch) {
		case CTRL('K'):
			if (!shift)
				Save();
			clear();
			endwin();
			return 0;
			break;
		case NC_KEY_UP:
			MoveCursorUp();
			EndSelection();
			break;
		case NC_KEY_DOWN:
			MoveCursorDown();
			EndSelection();
			break;
		case NC_KEY_LEFT:
			MoveCursorLeft();
			EndSelection();
			break;
		case NC_KEY_RIGHT:
			MoveCursorRight();
			EndSelection();
			break;
		case NC_KEY_SHIFT_UP:
			if (!SelectionExists()) StartSelection();
			MoveCursorUp();
			break;
		case NC_KEY_SHIFT_DOWN:
			if (!SelectionExists()) StartSelection();
			MoveCursorDown();
			break;
		case NC_KEY_SHIFT_LEFT:
			if (!SelectionExists()) StartSelection();
			if (ctrl) {
				if (m_cursor.x && isalpha(m_text[m_cursor.y][m_cursor.x - 1]))
					MoveCursorToPrevWord();
				else
					MoveCursorToPrevSpace();
			}
			else
				MoveCursorLeft();
			break;
		case NC_KEY_SHIFT_RIGHT:
			if (!SelectionExists()) StartSelection();
			if (ctrl)
				MoveCursorToNextWord();
			else
				MoveCursorRight();
			break;
		case NC_KEY_CTRL_LEFT:
			if (m_cursor.x && isalpha(m_text[m_cursor.y][m_cursor.x - 1]))
				MoveCursorToPrevWord();
			else
				MoveCursorToPrevSpace();
			EndSelection();
			break;
		case NC_KEY_CTRL_RIGHT:
			MoveCursorToNextWord();
			EndSelection();
			break;
		case NC_KEY_CTRL_UP:
			break;
		case NC_KEY_CTRL_DOWN:
			break;
		case NC_KEY_BACKSPACE:
			if (SelectionExists())
				DeleteSelection();
			else {
				char deleted_char = 0;
				if (m_cursor.x > 0)
					deleted_char = m_text[m_cursor.y][m_cursor.x - 1];
				else
					deleted_char = '\n';
				DeleteLeftCharacter();
				// Undo
				UndoCommandBus bus;
				PushCommand(&bus, UNDO_CMD_DELETE_LEFT_CHAR, deleted_char);
				PushCommandBus(bus);
			}
			break;
		case NC_KEY_DELETE:
			if (SelectionExists())
				DeleteSelection();
			else {
				char deleted_char = m_text[m_cursor.y][m_cursor.x];
				DeleteRightCharacter();
				// Undo
				UndoCommandBus bus;
				PushCommand(&bus, UNDO_CMD_DELETE_RIGHT_CHAR, deleted_char);
				PushCommandBus(bus);
			}
			break;
		case CTRL('S'):
			Save();
			break;
		case CTRL('A'):
			SelectAllText();
			break;
		case CTRL('C'):
			if (SelectionExists())
				CopySelection(console_window);
			EndSelection();
			break;
		case CTRL('X'):
			if (SelectionExists())
				CutSelection(console_window);
			EndSelection();
			break;
		case CTRL('V'):
			Paste(console_window);
			break;
		case CTRL('Z'):
			Undo();
			EndSelection();
			break;
		case CTRL('Y'):
			Redo();
			EndSelection();
			break;
		case NC_KEY_ENTER: {
			InsertNewLine();

			// Undo
			UndoCommandBus bus;
			PushCommand(&bus, UNDO_CMD_INSERT, '\n');
			PushCommandBus(bus);
		} break;
		case NC_KEY_TAB: {
			InsertTab();

			// Undo
			UndoCommandBus bus;
			for (int i = 0; i < TAB_SIZE; i++) {
				PushCommand(&bus, UNDO_CMD_DELETE_RIGHT_CHAR, ' ');
			}
			PushCommandBus(bus);
			DeleteRedoQueue();
		} break;
		default:
			EndSelection();
			if (CharIsPrintable(ch)) {
				InsertCharacter(ch);
				
				// Undo
				UndoCommandBus bus;
				PushCommand(&bus, UNDO_CMD_INSERT, ch);
				PushCommandBus(bus);
			}
			break;
		}

		// Automatic scroll
		while (m_cursor.x > m_scroll.x + scr_size.x * 0.75f) {
			ScrollRight();
		}
		while ((int)m_cursor.x - (int)m_scroll.x <= scr_size.x * 0.25f && m_scroll.x) {
			ScrollLeft();
		}
		while (m_cursor.y > m_scroll.y + scr_size.y * 0.75f) {
			ScrollDown();
		}
		if (m_cursor.y < (int)m_scroll.y - (int)scr_size.y) {
			while (m_cursor.y < m_scroll.y) {
				ScrollUp();
			}
		}

		// Not let the user crash the program bcuz bad design
		if (m_text.size() < 2) {
			InsertNewLine();
		}
		return 1;
	}
public:
	bool Update(Uint2 scr_size, HWND console_window, const EditorConfig &config) {
		if (m_cursor.x > m_text[m_cursor.y].size() - 1) {
			m_cursor.x = m_text[m_cursor.y].size();
		}
		Render(scr_size, config);
		return UpdateInput(scr_size, console_window);
	}
};