#include <cstdint>
#include <string>
#include <cstring>
#include <SDL3/SDL_stdinc.h>
#ifdef _WIN32
#define poll WSAPoll
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#else
#include <poll.h>
#include <error.h>
#include <curl/curl.h>
#endif

struct http_payload {
	std::string source_url;
	void** result = nullptr;
	uint8_t timeout_s = 15;
	std::string user_agent = "N0CALL-clock-Agent/1.0";
	int http_code = 0;
	uint64_t result_size = 0;
};

std::string url_encode(const std::string& input);
uint64_t disk_cache_read (const std::string full_cache_path, void** result, const SDL_Time max_age, std::string& error_string);
uint64_t http_loader(const char* source_url, void** result, uint8_t timeout_s = 15,  const std::string& user_agent="N0CALL-clock-Agent/1.0");
uint64_t http_loader(struct http_payload& input);

