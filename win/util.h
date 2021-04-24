#pragma once

#include "include/curses.h"
#include <Windows.h>
#include <ctype.h>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <regex>
#include <iostream>
#pragma comment(lib, "pdcurses")

#ifndef CTRL
#define CTRL(c) ((c) & 037)
#endif

UINT __abs(int x) {
	if (x < 0) return -x;
	else return x;
}
#define ABS(x) __abs(x)

#define NC_COLOR_COMP(x) (x / 256.0 * 1000)

enum {
	TAB_SIZE = 4,

	NC_KEY_ESC = 27,
	NC_KEY_BACKSPACE = 8,
	NC_KEY_DELETE = 330,
	NC_KEY_ENTER = '\n',
	NC_KEY_TAB = '\t',
	NC_KEY_UP = KEY_UP,
	NC_KEY_DOWN = KEY_DOWN,
	NC_KEY_LEFT = KEY_LEFT,
	NC_KEY_RIGHT = KEY_RIGHT,
	NC_KEY_SHIFT_RIGHT = 400,
	NC_KEY_SHIFT_LEFT = 391,
	NC_KEY_SHIFT_UP = 547,
	NC_KEY_SHIFT_DOWN = 548,
	NC_KEY_CTRL_UP = 480,
	NC_KEY_CTRL_DOWN = 481,
	NC_KEY_CTRL_LEFT = 443,
	NC_KEY_CTRL_RIGHT = 444
};

struct Uint2 {
	UINT x = 0, y = 0;
	Uint2() {}
	Uint2(UINT _x, UINT _y) { x = _x; y = _y; }
};
struct Int2 {
	int x = 0, y = 0;
	Int2() {}
	Int2(int _x, int _y) { x = _x; y = _y; }
};

char GetDigitNum(UINT num) {
	if (!num) return 1;
	UINT result = 0;
	while (num) {
		result++;
		num /= 10;
	}
	return result;
}

std::string ReplaceTabs(std::string str) {
	std::string result = str;
	for (int i = 0; i < result.size(); i++) {
		if (result[i] == '\t') {
			result.erase(result.begin() + i);
			for (int j = 0; j < TAB_SIZE; j++) {
				result.insert(result.begin() + i, ' ');
			}
		}
	}
	return result;
}

std::string ReplaceSpaces(std::string str) {
	std::string result = str;
	UINT spaces_count = 0;
	for (int i = 0; i < result.size(); i++) {
		if (spaces_count == 4) {
			result.erase(i - 4, 4);
			i -= 4;
			spaces_count = 0;
			result.insert(result.begin() + i, '\t');
			i++;
		}
		spaces_count++;

		if (result[i] != ' ') {
			break;
		}
	}
	return result;
}

bool CharIsPrintable(char c) {
	return
		isalpha(c) || isdigit(c) ||
		c == ' ' || c == '_' ||
		c == '+' || c == '-' ||
		c == '*' || c == '/' ||
		c == '\"' || c == '\\' ||
		c == '\'' || c == '=' ||
		c == '!' || c == '@' ||
		c == '#' || c == '$' ||
		c == '(' || c == ')' ||
		c == '&' || c == '^' ||
		c == '%' || c == ';' ||
		c == '<' || c == '/' ||
		c == '>' || c == ',' ||
		c == '`' || c == '.' ||
		c == '~' || c == '}' ||
		c == '[' || c == '{' ||
		c == ']' || c == ':';
}

/*
bool CheckString(std::string str) {
	for (int i = 0; i < str.size(); i++) {
		if (!(CharIsPrintable(str[i]) || str[i] == '\t' || str[i] == '\n'))
			return 0;
	}
	return 1;
}
*/
// @TODO: Not working
bool CheckString(std::string str) {
	for (int i = 0; i < str.size(); i++) {
		if (!(isgraph(str[i]) || str[i] == '\t' || str[i] == '\n'))
			return 0;
	}
	return 1;
}


void ClipboardSet(HWND hwnd, const std::string& s) {
	OpenClipboard(hwnd);
	EmptyClipboard();
	HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
	if (!hg) {
		CloseClipboard();
		return;
	}
	memcpy(GlobalLock(hg), s.c_str(), s.size() + 1);
	GlobalUnlock(hg);
	SetClipboardData(CF_TEXT, hg);
	CloseClipboard();
	GlobalFree(hg);
}
std::string ClipboardGet(HWND hwnd) {
	if (OpenClipboard(hwnd)) {
		// Get handle of clipboard object for ANSI text
		HANDLE hData = GetClipboardData(CF_TEXT);
		if (hData == nullptr) return "";
		// Lock the handle to get the actual text pointer
		char* pszText = static_cast<char*>(GlobalLock(hData));
		if (pszText == nullptr) return "";
		// Save text in a string class instance
		std::string text(pszText);
		// Release the lock
		GlobalUnlock(hData);
		// Release the clipboard
		CloseClipboard();

		return text;
	}
	return "";
}

Uint2 GetConsoleSize() {
	CONSOLE_SCREEN_BUFFER_INFO info;
	int ret;
	ret = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
	return { (UINT)info.dwSize.X, (UINT)info.dwSize.Y };
}

struct Char3 {
	char x, y, z;
};
typedef Char3 Color;

Color HexToRGB(std::string color) {
	if (color[0] == '#') {
		color.erase(0, 1);
	}
	char r_str[] = { color[0], color[1] };
	char r = strtoul(r_str, 0, 0);
	char g_str[] = { color[2], color[3] };
	char g = strtoul(g_str, 0, 0);
	char b_str[] = { color[4], color[5] };
	char b = strtoul(b_str, 0, 0);
	//__debugbreak();
	return { r, g, b };
}

struct EditorConfig {
	// Global labels
	Color color_background;
	Color color_text;
	Color color_selection_text;
	Color color_selection_bkg;
	Color color_gui_text;
	Color color_gui_bkg;
	char tab_size;

	// C specific labels
	bool c_tabbed_switch;
	Color c_color_text; // Deprecated (?)
	Color c_color_comment;
	Color c_color_digit;
	Color c_color_keyword;
};

bool LoadEditorConfig(std::string filename, EditorConfig* config) {
	std::ifstream file;
	file.open(filename);
	if (file.is_open()) {
		std::string str;
		while (file) {
			file >> str;
			if (str == "color-background") {
				std::string color;
				file >> color;
				config->color_background = HexToRGB(color);
			}
			else if (str == "color-text") {
				std::string color;
				file >> color;
				config->color_text = HexToRGB(color);
			}
			else if (str == "color-selection-text") {
				std::string color;
				file >> color;
				config->color_selection_text = HexToRGB(color);
			}
			else if (str == "color-selection-bkg") {
				std::string color;
				file >> color;
				config->color_selection_bkg = HexToRGB(color);
			}
			else if (str == "tab-size") {
				std::string tab_size_str;
				file >> tab_size_str;
				config->tab_size = std::stoi(tab_size_str);
			}
			else if (str == "c-color-digit") {
				std::string color;
				file >> color;
				config->c_color_digit = HexToRGB(color);
			}
			else if (str == "c-color-keyword") {
				std::string color;
				file >> color;
				config->c_color_keyword = HexToRGB(color);
			}
			else if (str == "c-tabbed-switch") {
				std::string value;
				file >> value;
				if (value == "true") {
					config->c_tabbed_switch = 1;
				}
				else if (value == "false") {
					config->c_tabbed_switch = 0;
				}
			}
		}
		file.close();
		//__debugbreak();
		return 1;
	}
	return 0;
}


enum {
	NC_COLOR_GUI_TEXT = 16,
	NC_COLOR_GUI_BKG,
	NC_COLOR_SELECTION_TEXT,
	NC_COLOR_SELECTION_BKG,
	NC_COLOR_TEXT,
	NC_COLOR_BACKGROUND,
	NC_COLOR_DIGITS,
	NC_COLOR_KEYWORDS
};

enum {
	NC_PAIR_GUI = 1,
	NC_PAIR_SELECTION,
	NC_PAIR_TEXT,
	NC_PAIR_DIGITS,
	NC_PAIR_KEYWORDS
};

void ncurses_create_color(char index, Color color) {
	int temp = init_color(index, NC_COLOR_COMP(color.x), NC_COLOR_COMP(color.y), NC_COLOR_COMP(color.z));
}
void ncurses_create_pair(char index, char foreground, char background) {
	init_pair(index, foreground, background);
}

void ncurses_init(const EditorConfig &config) {
	initscr();
	noecho();
	cbreak();
	keypad(stdscr, true);

	start_color();
	ncurses_create_color(NC_COLOR_GUI_TEXT, config.color_gui_text);
	ncurses_create_color(NC_COLOR_GUI_BKG,  config.color_gui_bkg);
	ncurses_create_color(NC_COLOR_SELECTION_TEXT, config.color_selection_text);
	ncurses_create_color(NC_COLOR_SELECTION_BKG, config.color_selection_bkg);
	ncurses_create_color(NC_COLOR_TEXT, config.color_text);
	ncurses_create_color(NC_COLOR_BACKGROUND, config.color_background);
	ncurses_create_color(NC_COLOR_DIGITS, config.c_color_digit);
	ncurses_create_color(NC_COLOR_KEYWORDS, config.c_color_keyword);

	ncurses_create_pair(NC_PAIR_GUI, NC_COLOR_GUI_TEXT, NC_COLOR_GUI_BKG);
	ncurses_create_pair(NC_PAIR_SELECTION, NC_COLOR_SELECTION_TEXT, NC_COLOR_SELECTION_BKG);
	ncurses_create_pair(NC_PAIR_TEXT, NC_COLOR_TEXT, NC_COLOR_BACKGROUND);
	ncurses_create_pair(NC_PAIR_DIGITS, NC_COLOR_DIGITS, NC_COLOR_BACKGROUND);
	ncurses_create_pair(NC_PAIR_KEYWORDS, NC_COLOR_KEYWORDS, NC_COLOR_BACKGROUND);
}
