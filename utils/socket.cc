#include "socket.h"
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
