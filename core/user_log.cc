#include "core/user_log.h"
#include <iostream>
//********************************************************************************
// System User Log
//*******************************************************************************i*
const std::string sysuserbuf::readline() {
	if (!log_buffer.empty() && !(log_buffer.back().fetched)) {
		log_buffer.back().fetched = true;
		return (log_buffer.back().log_entry);
	} else {
		return "";
	}
}

uint16_t sysuserbuf::size() {
	if (!log_buffer.empty()) {
		return (0+static_cast<uint16_t>(log_buffer.size()));
	} else {
		return 0;
	}
}

const std::string sysuserbuf::read_index(uint16_t index) {
	if (index < log_buffer.size()) {
		return log_buffer[index].log_entry;
	} else {
		return "";
	}
}

void sysuserbuf::buffer_stuff() {
	struct log_entry new_log;
	new_log.fetched = false;
	new_log.log_entry = strbuf;
	std::cout << "USER LOG: "<< new_log.log_entry;
	log_buffer.push_back(new_log);

	strbuf.clear();
	while (log_buffer.size() > 2048) {
		log_buffer.erase(log_buffer.begin());
	}
	return;
}

int sysuserbuf::overflow(int c) {
	const std::lock_guard<std::recursive_mutex>char_lock(user_lock);
	if (c != EOF) {
		strbuf.push_back(static_cast<char>(c));
	}
	if (c == '\n') {
//		std::cout << plugin_name << ": " << strbuf;
//		log_buffer.push_back(strbuf)
//		strbuf.clear();
		buffer_stuff();
	}
	if (strbuf.size() > 1024) {
//		std::cout << plugin_name << ": " << strbuf << "\n";
//		log_buffer.push_back(strbuf);
//		strbuf.clear();
		buffer_stuff();
	}
	if (c == EOF) {
//		std::cout << plugin_name << ": " << strbuf << "\n";
//		log_buffer.push_back(strbuf);
//		strbuf.clear();
		buffer_stuff();
	}
	return c;
}

std::streamsize sysuserbuf::xsputn (const char* s, std::streamsize n) {
	std::streamsize count = 0;
	const std::lock_guard<std::recursive_mutex>str_lock(user_lock);
	for (std::streamsize i = 0; i < n; i++) {
		int result = overflow(static_cast<unsigned char>(s[i]));
		if (result != EOF) {
			count++;
		}
	}
	return count;
}

int sysuserbuf::sync() {
	if (!strbuf.empty()) {
//		std::cout << plugin_name << ": " << strbuf << "\n";
//		log_buffer.push_back(strbuf);
//		strbuf.clear();
		buffer_stuff();
	}
	std::cout.flush();
	return 0;
}

