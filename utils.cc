#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include "aaediclock.h"
#include "utils.h"
#include <map>
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
#include <string>



int read_socket(int fd, std::string &result) {

    int bytesin = 7;
    char temp[10];
    temp[0]=0;
    int total = 0;
    pollfd poll_list;
    poll_list.fd=fd;
    poll_list.events = POLLIN;
    result.clear();
    while (bytesin >0 && temp[0] !=10) {
        errno = 0;
        if (poll(&poll_list, 1, 100) >0) {
            if (poll_list.revents & POLLIN) {
                    bytesin=0;
#ifdef _WIN32
                    bytesin = recv(fd, temp, 1, 0);
#else
                    bytesin = recv(fd, (void*)temp, 1, 0);
#endif
                    if (bytesin) {
                        total += bytesin;
                        result += temp[0];
                    }
            } else {
//                SDL_Log ("Nothing to read");
                bytesin=-1;
            }
        } else {
//            SDL_Log ("Other POLL error: %s", strerror(errno));
            bytesin=-1;
            temp[0]=0;
//            SDL_Log ("returning empty");
        }
    }
//    SDL_Log ("Returning %s", result.c_str());
   return total;
}


int read_socket(int fd, char** result) {
    int bytesin = 7;
    char temp[10];
    temp[0]=0;
    char* str_ptr;
    int total = 0;
    pollfd poll_list;
    poll_list.fd=fd;
    poll_list.events = POLLIN;
    *result=(char*)malloc(1);
    while (bytesin >0 && temp[0] !=10) {
        if (poll(&poll_list, 1, 100) >0) {
            if (poll_list.revents & POLLIN) {
                *result=(char*)realloc(*result,total+2);
                SDL_Log("MALLOCED %i", total+2);
                if (*result) {
                    bytesin=0;
                    str_ptr = (*result)+total;
#ifdef _WIN32
                    bytesin = recv(fd, temp, 1, 0);
#else
                    bytesin = recv(fd, (void*)temp, 1, 0);
#endif

                    if (bytesin) {
                        temp[1]=0;
                        str_ptr[0]=temp[0];
                        str_ptr[1]=0;
                        total += bytesin;
                        (*result)[total]=0;
                    }
//                    SDL_Log("read: %s %i", *result, bytesin);
                } else {
                    SDL_Log ("Socket read MALLOC error");
                    bytesin=-1;
                }
            } else {
                SDL_Log ("Nothing to read");
                bytesin=-1;
            }
        } else {
            SDL_Log ("Other POLL error");
            bytesin=-1;
            temp[0]=0;
            SDL_Log ("returning empty");
        }
    }
    (*result)[total]=0;
    SDL_Log ("Returning %s", *result);
   return total;

}

double solar_altitude(double lat_deg, double lon_deg, struct tm *utc, double decl_deg) {
    //Converts latitude and solar declination from degrees to radians
    double lat = lat_deg * M_PI / 180.0;
    double decl = decl_deg * M_PI / 180.0;

    double utc_hours = utc->tm_hour + utc->tm_min / 60.0 + utc->tm_sec / 3600.0;
    double solar_time = utc_hours + (lon_deg / 15.0);  // Local solar time for pixel
    double hour_angle = (15.0 * (solar_time - 12.0)) * M_PI / 180.0;
    double sin_alt = sin(lat) * sin(decl) + cos(lat) * cos(decl) * cos(hour_angle);
    return asin(sin_alt) * 180.0 / M_PI;
}

void maidenhead(double lat, double lon, char* maiden) {

    // generate maidenhead grid square
    // result should be at least 7 bytes long

    double madlon, madlat;
    madlon = lon + 180;
    madlat = lat + 90;
    maiden[6]=0;
    maiden[0]= static_cast<char>((int)(madlon/20))+65;	// Offset from 'A'
    maiden[1]= static_cast<char>((int)(madlat/10))+65;
    maiden[2]= static_cast<char>((int)(((int)madlon % 20)/2)+48);	// offset from '0'
    maiden[3]= static_cast<char>((int)(((int)madlat + 90) % 10)+48);
    maiden[4] = static_cast<char>((int)(((fmod(madlon,2.0))/2.0)*24)+97);	// offset from 'a'
    maiden[5] = static_cast<char>((int)((fmod(madlat,1.0))*24)+97);
    return;
}

void cords_to_px(double lat, double lon, int w, int h, SDL_FPoint* result) {
    result->x=static_cast<float>((lon/180.0f)*(w/2.0f)+(w/2.0f));
    result->y= static_cast<float>(((-1*lat)/90.0f)*(h/2.0f)+(h/2.0f));
    return ;
}

int month_to_int(const std::string& month) {

     static const std::map<std::string, int> months = {
                                                         {"Jan", 0}, {"Feb", 1}, {"Mar", 2}, {"Apr", 3},
                                                         {"May", 4}, {"Jun", 5}, {"Jul", 6}, {"Aug", 7},
                                                         {"Sep", 8}, {"Oct", 9}, {"Nov", 10}, {"Dec", 11}
                                                    };
    auto temp = months.find(month);
    if (temp != months.end()) {
        return temp->second;
    } else {
        return -1;
    }
}


void sun_times(double lat, double lon, time_t* sunrise, time_t* sunset, double *solar_alt, time_t now) {
    // fet sunrise and sunset times
    tm* utc = gmtime(&now);
    double solar_decl = 23.45 * (sin( (2 * M_PI/365) * (284+(utc->tm_yday+1)) ));
    *solar_alt = solar_altitude(lat, lon, utc, solar_decl);
    // find the next zero crossing for sunrise if current alt <0

    double test_alt;
    test_alt = *solar_alt;
    tm* test_time;
    *sunrise = now;
    *sunset = now;
    if (test_alt <0) {
        while (test_alt <0) {
            *sunrise +=5;
            *sunset += 5;
            test_time = gmtime(sunrise);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
        while (test_alt >0) {
            *sunset += 5;
            test_time = gmtime(sunset);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
    } else {
       while (test_alt >0) {
            *sunrise +=5;
            *sunset += 5;
            test_time = gmtime(sunset);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
        while (test_alt <0) {
            *sunrise += 5;
            test_time = gmtime(sunrise);
            test_alt = solar_altitude(lat, lon, test_time, solar_decl);
        }
    }

}

int add_pin(struct map_pin* new_pin) {
    // add a new map pin for a module
//    SDL_Log("Adding pin %s", new_pin->label);
    struct map_pin* empty_pin;
    struct map_pin* current_pin;
    if (map_pins) {
        current_pin = map_pins;
        while (current_pin->next) {
            current_pin = current_pin->next;
        }
        current_pin->next = (struct map_pin*)malloc(sizeof(struct map_pin));
        empty_pin = current_pin->next;
    } else {
        map_pins = (struct map_pin*)malloc(sizeof(struct map_pin));
        empty_pin=map_pins;
    }
    empty_pin->next=0;
    empty_pin->owner = new_pin->owner;
    empty_pin->lat = new_pin->lat;
    empty_pin->lon = new_pin->lon;
    empty_pin->icon = new_pin->icon;
    empty_pin->color = new_pin->color;
    empty_pin->label[0]=0;
    if (new_pin->label[0]) {
        memcpy(empty_pin->label, new_pin->label, 16);
        empty_pin->label[15]=0;
    }
    empty_pin->tooltip[0]=0;
    if (new_pin->tooltip[0]) {
        memcpy(empty_pin->tooltip, new_pin->tooltip, 512);
        empty_pin->tooltip[511]=0;
    }
    return (0);
}

int delete_owner_pins(enum mod_name owner) {
    // delete all map pins owned by a module
//    SDL_Log("killing pins %i", owner);
    struct map_pin* current_pin;
    struct map_pin* next_pin;
    struct map_pin* last_pin;
    struct map_pin* old_pin;
    if (map_pins) {
        current_pin=map_pins;
//        SDL_Log("Initial map_pins: %p", map_pins);
        last_pin=0;
        while (current_pin) {
//            SDL_Log("Current pin: %p, owner: %d", current_pin, current_pin->owner);
            if (!current_pin) {
                SDL_Log("Null current_pin!");
                break;
            }

            next_pin = current_pin->next;
//            SDL_Log("Next pin: %p", next_pin);
            if (current_pin->owner == owner) {
//                SDL_Log("Removing pin");
                // delete current pin
                old_pin = current_pin;
                if (current_pin == map_pins) {
                    map_pins = next_pin;
                }
                if (last_pin) {
                    last_pin->next = next_pin;
                }
                current_pin = next_pin;
                free (old_pin);
            } else {
//                SDL_Log("NOT Removing pin");
                last_pin = current_pin;
                current_pin=next_pin;
            }
        }
    }
    return(0);
}


int delete_mod_cache(enum mod_name owner) {
//    SDL_Log("killing pins %i", owner);
    struct data_blob* current_chunk;
    struct data_blob* next_chunk;
    struct data_blob* last_chunk;
    struct data_blob* old_chunk;
    if (data_cache) {
        current_chunk=data_cache;
        last_chunk=0;
        while (current_chunk) {
            if (!current_chunk) {
                SDL_Log("Null current chunk!");
                break;
            }

            next_chunk = current_chunk->next;
            if (current_chunk->owner == owner) {
                // delete current pin
                old_chunk = current_chunk;
                if (current_chunk == data_cache) {
                    data_cache = next_chunk;
                }
                if (last_chunk) {
                    last_chunk->next = next_chunk;
                }
                current_chunk = next_chunk;
                free (old_chunk->data);
                free (old_chunk);
            } else {
                last_chunk = current_chunk;
                current_chunk=next_chunk;
            }
        }
    }
    return(0);

}

int add_data_cache(enum mod_name owner, const Uint32 size, const void* data) {
    delete_mod_cache(owner);

    // add a new data_cache for a module
    struct data_blob* empty_locker;
    struct data_blob* current_chunk;
    if (data_cache) {
        current_chunk = data_cache;
        while (current_chunk->next) {
            current_chunk = current_chunk->next;
        }
        current_chunk->next = (struct data_blob*)malloc(sizeof(struct data_blob));
        empty_locker = current_chunk->next;
    } else {
        data_cache = (struct data_blob*)malloc(sizeof(struct data_blob));
        empty_locker=data_cache;
    }
    if (!empty_locker) {
        SDL_Log("Cache Allocation Error!");
        return (0);
    }
    empty_locker->next=0;
    empty_locker->owner = owner;
    empty_locker->fetch_time=time(NULL);
    empty_locker->size = size;
    empty_locker->data = malloc(size+1);
    if (empty_locker->data) {
        memset(empty_locker->data, 0, size + 1);
        memcpy(empty_locker->data, data, size);
        ((char*)empty_locker->data)[size] = '\0';
    } else {
        return (0);
    }

//    printf ("Test stored data\n %s \n -----------\n",(char*)empty_locker->data);
    return (1);

}

int fetch_data_cache(enum mod_name owner, time_t *age, Uint32 *size, void* data) {
    // function to check for and return locally cached web data
    if (!age || !size) {
        SDL_Log("VERY BAD Data Cache call! No return values!");
        return 0;
    }
    if (data_cache) {
        struct data_blob* current = data_cache;
        while (current) {
            if (current->owner == owner) {
//                printf ("Test fetched data\n %s \n -----------\n",(char*)current->data);
                memcpy(age, &(current->fetch_time), sizeof(time_t));
                memcpy(size, &(current->size), sizeof(Uint32));
                if (data != NULL) {
                    memcpy(data, current->data, current->size);
//                    SDL_Log("Cache returning %i bytes", current->size);
                }
                return (1);
            }	// found a cache hit
            current = current->next;
        }	// itterate through the current cache
    } // do we have anything at all cached yet?
//    SDL_Log("Cache miss");
    return (0);
}


size_t cache_http_callback( char* in, size_t size, size_t nmemb, void* out) {
    std::string* buffer = static_cast<std::string*>(out);
    buffer->append(in, (size*nmemb));
    return (size*nmemb);
}

int http_loader(const char* source_url, void** result) {
#ifndef _WIN32          // *NIX version starts here
    CURLcode curlres;
    std::string httpbuffer;
//    SDL_Log("Fetching data from %s", source_url);
    CURL *curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, source_url);
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION,
                        (long)CURL_HTTP_VERSION_3);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15);
        std::string user_agent = clockconfig.CallSign()+"-clock-Agent/1.0";
        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cache_http_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&httpbuffer);
        curlres = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (!curlres) {
//            SDL_Log("Fetched %lu Bytes", httpbuffer.size());
            *result = realloc(*result, httpbuffer.size()+1);

            if (*result) {
                // return our result text in *result
                memset(*result, 0, httpbuffer.size() + 1);
                memcpy(*result, httpbuffer.c_str(), httpbuffer.size());
                return(httpbuffer.size());
            } else {
                SDL_Log ("Curl result MALLOC error");
                return 0;
            }
        } else {
            SDL_Log ("Curl Fetch Error: %s", curl_easy_strerror(curlres));
            return 0;
        }
    } else {
        SDL_Log("Failed to init Curl!");
        return 0;
    }
#else               // WIN32 version starts here
    URL_COMPONENTS exploded_url{};
    ZeroMemory(&exploded_url, sizeof(exploded_url));
    exploded_url.dwStructSize = sizeof(exploded_url);
    // Set required component lengths to non-zero
    // so that they are cracked.
    exploded_url.dwSchemeLength = (DWORD)-1;
    exploded_url.dwHostNameLength = (DWORD)-1;
    exploded_url.dwUrlPathLength = (DWORD)-1;
    exploded_url.dwExtraInfoLength = (DWORD)-1;
    bool read_result;
    if (source_url == 0) {
        return 0;
    }
    if (source_url[0] == 0) {
        return 0;
    }
    // call once to get the result size
    int len = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, source_url, -1, NULL, 0);
    if (len == 0) return 0;

    // actually convert to UTF8
    LPWSTR utf8_url = new wchar_t[len];
    if (MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, source_url, -1, utf8_url, len) == 0) {
        delete[] utf8_url;
        return 0;
    }
    std::string narrow = clockconfig.CallSign() + "-clock-Agent/1.0";
    len = MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), -1, nullptr, 0);
    std::wstring user_agent(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), -1, &user_agent[0], len);
    HINTERNET http, http_connection, http_request;
    // open an http session
    http = WinHttpOpen(user_agent.c_str(), WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, NULL);
//    SDL_Log("Wide version URL (len=%zu): %ls", wcslen(utf8_url), utf8_url);
    if (!WinHttpCrackUrl(
        utf8_url,
        0,
        0,
        &exploded_url )) {
        SDL_Log("Error %u trying to Split URL", GetLastError());
        return 0;

    }
    std::wstring host(exploded_url.lpszHostName, exploded_url.dwHostNameLength);
    std::wstring path(exploded_url.lpszUrlPath, exploded_url.dwUrlPathLength);
    std::wstring extra(exploded_url.lpszExtraInfo, exploded_url.dwExtraInfoLength);

//    SDL_Log("Cracked URL: host(%zu)=%.*ls\n port=%u\n path(%zu)=%.*ls%.*ls (%zu)",
//        exploded_url.dwHostNameLength, exploded_url.dwHostNameLength, exploded_url.lpszHostName,
//        exploded_url.nPort,
//        exploded_url.dwUrlPathLength, exploded_url.dwUrlPathLength, exploded_url.lpszUrlPath,
//        exploded_url.dwExtraInfoLength, exploded_url.lpszExtraInfo, exploded_url.dwExtraInfoLength);
    if (!http) {
        SDL_Log("Unable to Init HTTP");
        return 0;
    }
    else {
//        SDL_Log("Initialized HTTP correctly");
    }

    http_connection = WinHttpConnect(http, host.c_str(),
        exploded_url.nPort, 0);
    SDL_Log("Attemped to connect to server. (Error %u)", GetLastError());
    if (!http_connection) {
        SDL_Log("Unable to connect to %ls on %u", host.c_str(), exploded_url.nPort);
        WinHttpCloseHandle(http);
        return 0;
    }
    else {
//        SDL_Log("Connected to %ls on %u", host.c_str(), exploded_url.nPort);
    }
//    std::wstring full_path = std::wstring(exploded_url.lpszUrlPath, exploded_url.dwUrlPathLength) +
//        std::wstring(exploded_url.lpszExtraInfo, exploded_url.dwExtraInfoLength);
    std::wstring full_path = std::wstring(exploded_url.lpszUrlPath, exploded_url.dwUrlPathLength);
    if (exploded_url.dwExtraInfoLength > 0) {
       full_path += std::wstring(exploded_url.lpszExtraInfo, exploded_url.dwExtraInfoLength);
    }
    DWORD flags = (exploded_url.nPort == INTERNET_DEFAULT_HTTPS_PORT) ? WINHTTP_FLAG_SECURE : 0;
//    SDL_Log("Dirty Full request path: \"%ls\" (len: %zu)", full_path.c_str(), full_path.length());
    // Trim to ensure it doesn't contain weird characters
    std::wstring sanitized;
    sanitized.clear();
    for (wchar_t ch : full_path) {
        if (ch >= 32 && ch != 127) {
            sanitized += ch;
        }
    }
    sanitized.push_back(L'\0');  // Ensure null-termination
//    SDL_Log("Sanitized path: \"%ls\" (len: %zu)", sanitized.c_str(), sanitized.length());
    full_path = sanitized;
    // Check length and print debug
//    SDL_Log("Full request path: \"%ls\" (len: %zu)", full_path.c_str(), full_path.length());

    // Optional: dump individual wchar_t codes
    for (size_t i = 0; i < full_path.length(); ++i) {
//        SDL_Log("char[%zu] = 0x%04X", i, full_path[i]);
    }
    http_request = WinHttpOpenRequest(http_connection, L"GET",
        full_path.c_str(),
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);
    //SDL_Log("Attemped request. (Error %u)", GetLastError());
    if (!http_request) {
        SDL_Log("Unable to request %ls", exploded_url.lpszUrlPath);
        WinHttpCloseHandle(http_connection);
        WinHttpCloseHandle(http);
        return 0;
    }
    else {
//        SDL_Log("Requested %ls", full_path.c_str());
    }
    read_result = WinHttpSendRequest(http_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (read_result) {
        read_result = WinHttpReceiveResponse(http_request, NULL);
//        SDL_Log("Sent Request");
    } else {
        SDL_Log("Unable to send request %ls", exploded_url.lpszUrlPath);
        WinHttpCloseHandle(http_request);
        WinHttpCloseHandle(http_connection);
        WinHttpCloseHandle(http);
        return 0;
    }
    std::string buffstr;
    delete[] utf8_url;
    buffstr.clear();
    if (read_result) {

        DWORD read_size = 1;
        LPSTR buffer;
        do {
            // Check for available data.
            read_size = 0;
            if (!WinHttpQueryDataAvailable(http_request, &read_size)) {
                SDL_Log("Error %u in WinHttpQueryDataAvailable.", GetLastError());
                break;
            } else {
                // allocate response space
                buffer = new char[read_size + 1];
                if (!buffer) {
                    SDL_Log("HTTP result MALLOC error\n");
                    break;
                } else { ZeroMemory(buffer, read_size + 1);  }

            }
            if (!WinHttpReadData(http_request, (LPVOID)buffer, read_size, NULL)) {
                SDL_Log("Error %u in WinHttpReadData.", GetLastError());
            } else {
//                SDL_Log("READ %s", buffer);
                cache_http_callback(buffer, 1, read_size, &buffstr);
                //SDL_Log("Stored %s", buffstr);
            }
            delete[] buffer;
        } while (read_size > 0);
        if (!buffstr.empty()) {

            *result = (char*)malloc(buffstr.size() + 1);

            if (*result) {
                // return our result text in *result
                memset(*result, 0, buffstr.size() + 1);
                memcpy(*result, buffstr.c_str(), buffstr.size());
                WinHttpCloseHandle(http_request);
                WinHttpCloseHandle(http_connection);
                WinHttpCloseHandle(http);
                return((int)buffstr.size());
            }
            else {
                SDL_Log("WinHttp result MALLOC error");
                WinHttpCloseHandle(http_request);
                WinHttpCloseHandle(http_connection);
                WinHttpCloseHandle(http);
                return 0;
            }

        }
    }
    WinHttpCloseHandle(http_request);
    WinHttpCloseHandle(http_connection);
    WinHttpCloseHandle(http);
    return 0;
#endif
}

Uint32 cache_loader(const enum mod_name owner, void** result, time_t *result_time) {
    Uint32 cache_size;
//    time_t cache_age;
    int cache_success = 0;
    *result_time = 0;
    // attempt to fetch from cache
    if (fetch_data_cache(owner, result_time, &cache_size, NULL)) {
         // cache hit
//        SDL_Log("Fetching %i Bytes from cache", cache_size);
        char *temp = (char*)realloc(*result, cache_size+1);
        if (temp) {
            *result = (void*)temp;
            memset(*result, 0, cache_size + 1);
            cache_success = fetch_data_cache(owner, result_time, &cache_size, *result);
            if (cache_success) {
                return (cache_size);
            } else {
                SDL_Log("Cache Loader fetch error!");
                free(*result);
                *result=nullptr;
                return 0;
            }
            // cache hit
        } else {
            SDL_Log("Cache loader MALLOC error");
            *result = nullptr;
            return 0;
        }
//        SDL_Log("Got from Cache %i Bytes", strlen(json_spots));

    } else {
        return 0; // cache miss
    }
}

