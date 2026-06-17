#include "socket.h"
#include <fcntl.h>

dx_socket_t init_fd(const struct plugin_server_info dx_server, aaediclock_host_api* host_api) {
    dx_socket_t dxsocket;
//    struct plugin_server_info dx_server = host_api->AaediHAM_ConfigGetDXServer();
    std::string serverip=dx_server.name;
    std::string serverport=std::to_string(dx_server.port);
    struct addrinfo* serveraddr = nullptr;
    struct addrinfo hints;
    dxsocket = 0;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // or AF_UNSPEC to allow IPv4/IPv6
    hints.ai_socktype = SOCK_STREAM;
#ifdef _WIN32
    WSADATA wsaData;
    int res = 0;
    *(host_api->AaediHAM_LogDebug) << "DXSPOTS: WSAStartup ... ";
    res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        *(host_api->AaediHAM_LogDebug) << "DXSPOTS: failed: " << res << "\n";
        return dxsocket;
    } else {
        *(host_api->AaediHAM_LogDebug) << "DXSPOTS: Success" << "\n";
    }

    *(host_api->AaediHAM_LogDebug) << "DXSPOTS: GetAddrInfo ... ";
    res = getaddrinfo(serverip.c_str(), serverport.c_str(), &hints, &serveraddr);
    if (res == 0) {
        *(host_api->AaediHAM_LogDebug) << " Success" << "\n";
    }
    else {
        *(host_api->AaediHAM_LogDebug) << " Failed " << WSAGetLastError() << "\n";
        WSACleanup();
        if (serveraddr) {
            freeaddrinfo(serveraddr);
        }
        return dxsocket;
    }

    *(host_api->AaediHAM_LogDebug) << "DXSPOTS: Getting DX Socket ... ";
    dxsocket = socket(serveraddr->ai_family, serveraddr->ai_socktype, serveraddr->ai_protocol);
    if (dxsocket == INVALID_SOCKET) {
        *(host_api->AaediHAM_LogDebug) << "Bad DX socket " << WSAGetLastError() << "\n";
        WSACleanup();
        if (serveraddr) {
            freeaddrinfo(serveraddr);
        }
        return dxsocket;
    } else {
        *(host_api->AaediHAM_LogDebug) << "Got DX socket" << "\n";
    }
    *(host_api->AaediHAM_LogDebug) << "DXSPOTS: Connecting to " << serverip << " " << serverport << "\n";
    if (connect(dxsocket, serveraddr->ai_addr, static_cast<int>(serveraddr->ai_addrlen)) == SOCKET_ERROR) {
        *(host_api->AaediHAM_LogDebug) << "DXSPOTS: server connect error on client: " << WSAGetLastError() << "\n";
        shutdown(dxsocket, SHUT_RDWR);
        WSACleanup();
        dxsocket = 0;
    } else {
        *(host_api->AaediHAM_LogDebug) << "DXSPOTS: client reporting connected to server on fd " << dxsocket << "with errno: " << errno << "\n";
    }
#else
    int addrerr =getaddrinfo(serverip.c_str(), serverport.c_str(), &hints, &serveraddr);
    if (addrerr !=0) {
        *(host_api->AaediHAM_LogDebug) << "DXSPOTS: DX Spots connection error: " << gai_strerror(addrerr) << "\n";
        return 0;
    }
    dxsocket = socket(serveraddr->ai_family, serveraddr->ai_socktype, serveraddr->ai_protocol);
    if (dxsocket < 0) {
        *(host_api->AaediHAM_LogDebug) << "DXSPOTS: Bad DX socket" << errno << "\n";
        freeaddrinfo(serveraddr);
        return 0;
    }
/*
    if (connect(dxsocket, serveraddr->ai_addr, serveraddr->ai_addrlen) == -1) {
        *(host_api->AaediHAM_LogDebug) << "DXSPOTS: server connect error on client: " << errno << "\n";
        *(host_api->AaediHAM_LogDebug) << "DXSPOTS: Connecting to " << serverip << " " << serverport << "\n";
        shutdown(dxsocket, SHUT_RDWR);
        dxsocket = 0;
    } else {
        *(host_api->AaediHAM_LogDebug) << "DXSPOTS: client reporting connected to server on fd " << dxsocket << "with errno: " << errno << "\n";
    }
*/
// test code
    // Set non-blocking
    int flags = fcntl(dxsocket, F_GETFL, 0);
    fcntl(dxsocket, F_SETFL, flags | O_NONBLOCK);

    if (connect(dxsocket, serveraddr->ai_addr, serveraddr->ai_addrlen) == -1) {
        if (errno == EINPROGRESS) {
            pollfd pfd;
            pfd.fd = dxsocket;
            pfd.events = POLLOUT;
            int res = poll(&pfd, 1, 5000);
            std::cout << "RES: " << res << "\tREVENTS: " << pfd.revents << "\n";
            if (res <= 0 || !(pfd.revents & POLLOUT)) {
                *(host_api->AaediHAM_LogDebug) << "DXSPOTS: connect timeout\n";
                shutdown(dxsocket, SHUT_RDWR);
                freeaddrinfo(serveraddr);
                return 0;
            }
        } else {
            shutdown(dxsocket, SHUT_RDWR);
            freeaddrinfo(serveraddr);
            return 0;
        }
    }
    // Restore blocking for reads
flags = fcntl(dxsocket, F_GETFL, 0);
fcntl(dxsocket, F_SETFL, flags & ~O_NONBLOCK);
// end test coed
#endif
    freeaddrinfo(serveraddr);
    return dxsocket;
}

int read_socket(dx_socket_t fd, std::string &result) {

    int bytesin = 7;
    char temp[10];
    temp[0]=0;
    int total = 0;
    pollfd poll_list;
    poll_list.fd=fd;
    poll_list.events = POLLIN;
    result.clear();
    result.reserve(256);
//    int max_count = 0;
    while (bytesin >0 && temp[0] !=10) {
        errno = 0;
        int poll_res = poll(&poll_list, 1, 100);
//        max_count++;
        bytesin=0;
        if (poll_res > 0) {
            if (poll_list.revents & POLLIN) {

#ifdef _WIN32
                bytesin = recv(fd, temp, 1, 0);
#else
                bytesin = recv(fd, (void*)temp, 1, 0);
#endif
                if (bytesin) {
                    total += bytesin;
                    result += temp[0];
                } else {
//                    SDL_Log ("Poll says there is something here, but got nothing");
                }
                if (bytesin < 0) {
                    SDL_Log ("Bad Read, requesting reset");
                    return -1;
                }
#ifdef _WIN32
                if (bytesin == SOCKET_ERROR) {
                    SDL_Log ("Socket Error, requesting reset");
                    return -1;
                }
#endif
            } else if (poll_list.revents & POLLHUP) {
                return -1;

            } else if (poll_list.revents & POLLERR) {
                return -1;

            } else {
//                SDL_Log ("Nothing to read");
                bytesin=-1;
            }
        } else if (poll_res < 0) {
            // POLL error condition
            SDL_Log("Read Poll Error: %s", strerror(errno));
            return poll_res;
        } else {
//            SDL_Log("Read Poll Timeout");
//            return 0;
        }
    }
//    SDL_Log ("Returning %s", result.c_str());
    return total;
}
