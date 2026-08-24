
#ifdef _WIN32
#define poll WSAPoll
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <poll.h>
#ifndef __APPLE__
#include <error.h>
#endif
#endif
#include "aaediclock.h"
#ifdef _WIN32
#include <time.h>
#define timegm _mkgmtime
#define SHUT_RDWR SD_BOTH
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif


#ifdef _WIN32
using dx_socket_t = uintptr_t;
#else
using dx_socket_t = int;
#endif

dx_socket_t init_fd(const struct plugin_server_info dx_server, aaediclock_host_api* host_api);

int read_socket(dx_socket_t fd, std::string &result);
