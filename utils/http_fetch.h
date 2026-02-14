#include <cstdint>
#include <string>
#include <cstring>
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

uint64_t http_loader(const char* source_url, void** result,  const std::string& user_agent="N0CALL-clock-Agent/1.0");