#pragma once

class Pane {
private:
	Uint2 m_scr_size;
	Uint2 m_last_scr_size;
	std::vector<Buffer> m_buffers;
	char m_active_buffer = 0;
	HWND m_console_window;
	
public:
	bool Init(std::string filename) {
		m_console_window = GetConsoleWindow();
		Buffer buf;
		bool rval = buf.LoadFile(filename);
		m_buffers.push_back(buf);
		getmaxyx(stdscr, m_scr_size.y, m_scr_size.x);
		return rval;
	}
	void Update(const EditorConfig& config) {
		while (1) {
			getmaxyx(stdscr, m_scr_size.y, m_scr_size.x);
			if (m_buffers.size()) {
				if (!m_buffers[m_active_buffer].Update(m_scr_size, m_console_window, config)) {
					m_buffers.erase(m_buffers.begin() + m_active_buffer);
				}
			}
			else {
				exit(0);
			}
		}
	}
};