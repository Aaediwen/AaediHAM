#include <string>
#include <iostream>
#include <mutex>
#include <vector>
class sysuserbuf : public std::streambuf {
	public:
		std::string plugin_name;
		const std::string readline();
		uint16_t size();
		const std::string read_index(uint16_t index);
	protected:
		int overflow(int c) override;
		std::streamsize xsputn (const char* s, std::streamsize n);
		int sync() override;
	private:
		struct log_entry {
			std::string log_entry;
			bool fetched;
		};
		void buffer_stuff();
		std::string strbuf;
		std::vector<struct log_entry> log_buffer;
		std::recursive_mutex user_lock;
};

