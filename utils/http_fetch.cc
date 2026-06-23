#include "http_fetch.h"
#include <iostream>

const std::string hexencode (const std::string source) {
    std::string result;
    result.clear();
    result.reserve(source.size()*3);
    for (unsigned int pos = 0 ; pos < source.size() ; pos++) {
        unsigned char c = source.at(pos);
        // we have an already URL Clean character
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
            c == '*' || c == '\'' || c == '(' || c == ')') {
            result.push_back(c);
        } else {
             // something else
             result.push_back('%');
             char hex;
             hex = c / 16;
             hex += hex <= 9 ? '0' : 'a' - 10;
             result.push_back(hex);
             hex = c % 16;
             hex += hex <= 9 ? '0' : 'a' - 10;
             result.push_back(hex);
        }
    }
    return result;
}

std::string url_encode(const std::string& input) {
    static const char hex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(input.size() * 3);

    for (unsigned char c : input) {
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9')) {
            result.push_back(c);
        } else {
            result.push_back('%');
            result.push_back(hex[c >> 4]);
            result.push_back(hex[c & 0xF]);
        }
    }
//    SDL_Log ("DEBUG URL_Encoded string: %s", result.c_str());
    return result;
}


const std::string URL_Encode(const char* source) {
    std::string result;
    result.clear();
    if (!source || !source[0]) {
        return (result);
    }
    std::string working(source);
    size_t tag_start, tag_stop, equalpos;
    tag_start=0;
    // copy in the base of the URL
    tag_stop = working.find("?");
    if (tag_stop != std::string::npos) {
        result = working.substr(0,tag_stop+1);
        tag_start = tag_stop+1;
        //if we have arguments encode them
        while (tag_start != std::string::npos) {
            // grab the next segment
            tag_stop = working.find_first_of("&;",tag_start+1);
            std::string argument;
            if (tag_stop != std::string::npos) {
                argument = working.substr(tag_start, tag_stop - tag_start);
            } else {
                argument = working.substr(tag_start, tag_stop);
            }
            equalpos = argument.find("=");
            if (equalpos != std::string::npos) {
                // we have a key=value pair, copy the key and encode the value
                result += argument.substr(0,equalpos+1);
                // temporarilly just copying this , need to URLencode this piece
                result += hexencode(argument.substr(equalpos+1,std::string::npos));
            } else {
                // whatever is in argument has no value. just copy it
                result += argument.substr(0,equalpos);
            }
            tag_start = tag_stop;
        }
    } else {
        // there is nothing to do here
        result = working;
    }
    return result;
}

size_t cache_http_callback( char* in, size_t size, size_t nmemb, void* out) {
    std::string* buffer = static_cast<std::string*>(out);
    buffer->append(in, (size*nmemb));
    return (size*nmemb);
}

uint64_t http_loader(const char* source_url, void** result, const std::string& user_agent) {
    if (source_url == 0) {
        return 0;
    }
    if (source_url[0] == 0) {
        return 0;
    }
    std::string encoded_url = URL_Encode(source_url);
#ifndef _WIN32          // *NIX version starts here
    CURLcode curlres;
    std::string httpbuffer;
//    std::cout << "HTTP: Fetching data from "<< source_url << "\n";
    CURL *curl = curl_easy_init();
    if (curl) {
//        curl_easy_setopt(curl, CURLOPT_URL, source_url);
        curl_easy_setopt(curl, CURLOPT_URL, encoded_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION,
                        (long)CURL_HTTP_VERSION_3);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15);
//        std::string user_agent = clockconfig.CallSign()+"-clock-Agent/1.0";
        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cache_http_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&httpbuffer);
//            std::cout << "HTTP: Calling CURL fetch \n";
        curlres = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (!curlres) {
//            std::cout << "HTTP: Fetched "<< httpbuffer.size() <<" Bytes\n";
            void* temp = result;
            *result = realloc(*result, httpbuffer.size()+1);

            if (*result) {
                // return our result text in *result
                memset(*result, 0, httpbuffer.size() + 1);
                memcpy(*result, httpbuffer.c_str(), httpbuffer.size());
////                SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
//                std::cout << "HTTP: Returning "<< httpbuffer.size() << "\n";
                return(httpbuffer.size());
            } else {
//                std::cout << "HTTP: Curl result MALLOC error\n";
                if (temp) {
                    free(temp);
                }
////                SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
                return 0;
            }
        } else {
            std::cout << "Curl Fetch Error: "<< curl_easy_strerror(curlres) << "\n";
////            SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
            return 0;
        }
    } else {
        std::cout << "Failed to init Curl!\n";
////        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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
    // call once to get the result size
//    int len = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, source_url, -1, NULL, 0);
    int len = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, encoded_url.c_str(), -1, NULL, 0);
    if (len == 0) {
////        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
        return 0;
    }

    // actually convert to UTF8
    LPWSTR utf8_url = new wchar_t[len];
//    if (MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, source_url, -1, utf8_url, len) == 0) {
    if (MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, encoded_url.c_str(), -1, utf8_url, len) == 0) {
        delete[] utf8_url;
////        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
        return 0;
    }
    std::string narrow = user_agent;
    len = MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), -1, nullptr, 0);
    std::wstring user_agent_wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), -1, &user_agent_wide[0], len);
    HINTERNET http, http_connection, http_request;
    // open an http session
    http = WinHttpOpen(user_agent_wide.c_str(), WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, NULL);
//    SDL_Log("Wide version URL (len=%zu): %ls", wcslen(utf8_url), utf8_url);
    if (!WinHttpCrackUrl(
        utf8_url,
        0,
        0,
        &exploded_url )) {
 //       SDL_Log("Error %u trying to Split URL", GetLastError());
////        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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
//        SDL_Log("Unable to Init HTTP");
////        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
        return 0;
    }
    else {
//        SDL_Log("Initialized HTTP correctly");
    }

    http_connection = WinHttpConnect(http, host.c_str(),
        exploded_url.nPort, 0);
//    SDL_Log("Attemped to connect to server. %s (Error %u)", host.c_str(), GetLastError());
    if (!http_connection) {
//        SDL_Log("Unable to connect to %ls on %u", host.c_str(), exploded_url.nPort);
        WinHttpCloseHandle(http);
////        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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
//        SDL_Log("Unable to request %ls", exploded_url.lpszUrlPath);
        WinHttpCloseHandle(http_connection);
        WinHttpCloseHandle(http);
////        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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
//        SDL_Log("Unable to send request %ls", exploded_url.lpszUrlPath);
        WinHttpCloseHandle(http_request);
        WinHttpCloseHandle(http_connection);
        WinHttpCloseHandle(http);
////        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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
//                SDL_Log("Error %u in WinHttpQueryDataAvailable.", GetLastError());
                break;
            } else {
                // allocate response space
                buffer = new char[read_size + 1];
                if (!buffer) {
//                    SDL_Log("HTTP result MALLOC error\n");
                    break;
                } else { ZeroMemory(buffer, read_size + 1);  }

            }
            if (!WinHttpReadData(http_request, (LPVOID)buffer, read_size, NULL)) {
//                SDL_Log("Error %u in WinHttpReadData.", GetLastError());
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
////                SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
                return((int)buffstr.size());
            }
            else {
//                SDL_Log("WinHttp result MALLOC error");
                WinHttpCloseHandle(http_request);
                WinHttpCloseHandle(http_connection);
                WinHttpCloseHandle(http);
////                SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
                return 0;
            }

        }
    }
    WinHttpCloseHandle(http_request);
    WinHttpCloseHandle(http_connection);
    WinHttpCloseHandle(http);
////    SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
    return 0;
#endif
}
