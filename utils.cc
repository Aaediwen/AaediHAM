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
#include <iostream>



int read_socket(dx_socket_t fd, std::string &result) {

    int bytesin = 7;
    char temp[10];
    temp[0]=0;
    int total = 0;
    pollfd poll_list;
    poll_list.fd=fd;
    poll_list.events = POLLIN;
    result.clear();
    int max_count = 0;
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
            } else {
//                SDL_Log ("Nothing to read");
                bytesin=-1;
            }
        } else if (poll_res < 0) {
            // error condition
            SDL_Log("Read Poll Error: %s", strerror(errno));
        } else {
//            SDL_Log("Read Poll Timeout");
        }
//        if (max_count > 5) {
//            bytesin=0;
//        }
    }
//    SDL_Log ("Returning %s", result.c_str());
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
/*
struct GeoCoord subsolar(const time_t now) {
    struct GeoCoord result;
    tm* utc = gmtime(&now);
    result.latitude = 23.44 * sin(2 * M_PI / 365 * (284+utc->tm_yday+1 ));
    result.longitude = 15 * (utc->tm_hour + utc->tm_min / 60.0 + utc->tm_sec / 3600.0);
    // wrap to [-180, 180)
    while (result.longitude < -180.0) result.longitude += 360.0;
    while (result.longitude >= 180.0) result.longitude -= 360.0;
    return (result);
}
*/
/*

n = -1.5+(Year - 2000)*365 + leap_yr_count + DOY + (fraction_of_day from 00:00Z)	(day)
corrected_mean_solar_lon = 280.46646 + 0.9856474n					(deg)
mean_anomaly = 357.528 + 0.9856003n							(deg)
g = mean_anomaly									(deg)
ecliptic_lon = corrected_mean_solar_lon + 1.915*sin(g)+0.020*sin(2g)			(deg)

obliquity_ecliptic = 23.440 - 0.0000004n						(deg)
decl = sin-1(sin(obliquity_ecliptic)*sin(ecliptic_lon))*180/pi				(deg)
right_ascension = tan-1(cos(obliquity_ecliptic)*tan(ecliptic_lon))*180/pi		(deg)

Emin = (corrected_mean_solar_lon - right_ascension)*4 					(min)

subsolar_lat = decl
subsolar_lon = -15(Tgmt - 12 + Emin/60)


Note that corrected_mean_solar_lon and mean_anomaly as well as ecliptic_lon given above can be either positive or negative,
but computationally they need to be put in the range 0°–360°
this can be accomplished by using the modulo function;

right_ascension needs to be in the same quadrant as ecliptic_lon
and this can be done by using the atan2 function,
which takes two arguments, instead of the atan function, which takes only one.
All these treatments have been properly taken care of in the code in Appendix A.

According to the Almanac,
the errors of the right ascension and declination of the Sun given by these formulas are less than (1/60)°,
and the error of the equation of time is less than 3.5 s, if the input year is between 1950 and 2050.

https://archive.org/details/astronomicalalgorithmsjeanmeeus1991/page/n155/mode/2up
Jean Meesus Astronomical Algorithms Ch 24 (1991)
https://www.sciencedirect.com/science/article/pii/S0960148121004031?via%3Dihub#sec2
A solar azimuth formula that renders circumstantial treatment unnecessary without compromising mathematical rigor: Mathematical setup, application and extension of a formula based on the subsolar point and atan2 function
Author links open overlay panelTaiping Zhang a
, Paul W. Stackhouse Jr. b, Bradley Macpherson c, J. Colleen Mikovitz a
(2021)

*/
struct GeoCoord subsolar (const time_t now) {
//https://archive.org/details/astronomicalalgorithmsjeanmeeus1991/page/n155/mode/2up
//Jean Meesus Astronomical Algorithms Ch 24 (1991)

    struct GeoCoord result;
    // convert to Julian Centuries since J2000 (January 2000)
     // divide Unix Time by seconds per day(86400), and adjust offset to 01-01-1970 (2440587.5)
     double jd = (now / 86400.0) + 2440587.5;
     // adjust again to January 2000 and divide by 36525 days/Julian century (MESSUS P 151 24.1) T
     double T = (jd - 2451545.0) / 36525.0;
     // error of 0.00001 in T == 0.37 days

     // Sun mean anomaly (deg) (MEESUS P151 24.3) M
     double M = fmod(357.52910 + (35999.05030*T) - (0.0001559*T*T) - (0.00000048*T*T*T), 360.0);
     double M_rad = M * M_PI / 180.0;

     // Solar Equation of Center C (MEESUS P152)
     // ChatGPT gave slightly different values:
//         double C = (1.914602 - 0.004817*T - 0.000014*T*T)*sin(M*M_PI/180.0)
//             + (0.019993 - 0.000101*T)*sin(2*M*M_PI/180.0)
//             + 0.000289*sin(3*M*M_PI/180.0);
     // here we use values per Meesus
     double C = (1.914600 - (0.004817*T) - (0.000014*T*T)) * sin(M_rad)
                + (0.019993 - (0.000101*T)) * sin(2*M_rad)
                + 0.000290 * sin(3*M_rad);

     // Sun mean longitude (deg) (MEESUS P151 24.2) L0
     // per ScienceDirect article 2.1, needs to be in range 0 - 360, hence fmod 360.0
     double L0 = fmod(280.46645 + 36000.76983*T + 0.0003032*T*T, 360.0);
     // sun's true geometric Longitude and anomaly (MEESUS P152)
     double corrected_mean_solar_lon = L0 + C;
     double corrected_mean_solar_lon_rad = corrected_mean_solar_lon * M_PI/180.0;
     double corrected_mean_anomaly   = M + C;

     // calculate apparent longitude (MEESUS P152)
     double omega = 125.04 - 1934.136 * T;
     double apparent_longitude = corrected_mean_solar_lon - 0.00569 - 0.00478 * sin(omega * M_PI/180);
     double apparent_longitude_rad = apparent_longitude * M_PI/180.0;
     // obliquity of the eleptic per MEESUS 21.2
     // deg + min/60 + sec/3600
     double obliquity = (23.0 + (26.0/60.0) + (21.448/3600.0) )
                    - ((46.8150/3600.0) * T)
                    - ((0.00059/3600.0) * T * T)
                    + ((0.001813/3600.0) * T * T * T);
     double obliquity_rad = obliquity * M_PI / 180.0;
     // solar latitude (MEESUS P153)
     // meesus 24.6 + see note after 24.8
     double right_ascension_rad = atan2(cos(obliquity_rad) * sin(apparent_longitude_rad), cos(apparent_longitude_rad));
//     double right_ascension = cot((cos(obliquity_rad) * sin(corrected_mean_solar_lon_rad))
//                              / cos(corrected_mean_solar_lon_rad));
     double right_ascension = right_ascension_rad * (180.0/M_PI);
     if(right_ascension < 0) right_ascension += 360.0;  // normalize to [0,360)
     // meesus 24.7
     double declination_rad = asin(sin(obliquity_rad) * sin(apparent_longitude_rad));
     double declination = declination_rad * (180.0/M_PI);

     // adjust coordinate system from Celestial to geographical relative to Greenwich
//      Meesus P89
        // Greenwich Mean Sidereal Time (deg)
     double d = jd - 2451545.0;
     double GMST = fmod(280.46061837 + 360.98564736629*d, 360.0);

      // Subsolar longitude
      double lon = fmod(right_ascension - GMST, 360.0);
      if (lon < -180) lon += 360;
      if (lon > 180) lon -= 360;

     result.longitude = lon;
     result.latitude = declination;

     g_celestials.sun.timestamp=time(NULL);
     g_celestials.sun.Lat = result.latitude;
     g_celestials.sun.Lon = result.longitude;
     g_celestials.sun.RA  = right_ascension_rad;
     g_celestials.sun.Dec = declination_rad;
    return (result);
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
    struct map_pin* current_pin;
    struct map_pin* next_pin;
    struct map_pin* last_pin;
    struct map_pin* old_pin;
    if (map_pins) {
        current_pin=map_pins;
        last_pin=0;
        while (current_pin) {
            if (!current_pin) {
                debug_log << "MAP PIN: Null current_pin!\n";
                break;
            }

            next_pin = current_pin->next;
            if (current_pin->owner == owner) {
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
                last_pin = current_pin;
                current_pin=next_pin;
            }
        }
    }
    return(0);
}


int delete_mod_cache(enum mod_name owner) {
    struct data_blob* current_chunk;
    struct data_blob* next_chunk;
    struct data_blob* last_chunk;
    struct data_blob* old_chunk;
    if (!mutexes[MUTEX_CACHE]) {
        mutexes[MUTEX_CACHE] = SDL_CreateMutex();
    }
    SDL_LockMutex(mutexes[MUTEX_CACHE]);
    if (data_cache) {
        current_chunk=data_cache;
        last_chunk=0;
        while (current_chunk) {
            if (!current_chunk) {
                debug_log << "CACHE: Null current chunk!\n";
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
    SDL_UnlockMutex(mutexes[MUTEX_CACHE]);
    return(0);

}

void dump_cache() {

    struct data_blob* current_chunk;
     if (data_cache) {
           std::ofstream out("cache.dump", std::ios::binary);
           current_chunk = data_cache;
           while (current_chunk) {
               out.write(reinterpret_cast<const char*>(current_chunk), sizeof(struct data_blob));
               out.write(reinterpret_cast<const char*>(current_chunk->data), current_chunk->size);
               current_chunk = current_chunk->next;
           }
           out.close();
     }
     return;
}


int add_data_cache(enum mod_name owner, const Uint32 size, const void* data) {
    delete_mod_cache(owner);
    if (!mutexes[MUTEX_CACHE]) {
        mutexes[MUTEX_CACHE] = SDL_CreateMutex();
    }
    SDL_LockMutex(mutexes[MUTEX_CACHE]);
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
        debug_log << "CACHE: Allocation Error!\n";
        SDL_UnlockMutex(mutexes[MUTEX_CACHE]);
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
        SDL_UnlockMutex(mutexes[MUTEX_CACHE]);
        return (0);
    }
//    dump_cache();
//    printf ("Test stored data\n %s \n -----------\n",(char*)empty_locker->data);
    SDL_UnlockMutex(mutexes[MUTEX_CACHE]);
    return (1);

}

int fetch_data_cache(enum mod_name owner, time_t *age, Uint32 *size, void* data) {
    // function to check for and return locally cached web data
    if (!age || !size) {
        SDL_Log("VERY BAD Data Cache call! No return values!");
        debug_log << "VERY BAD Data Cache call! No return values!\n";
        return 0;
    }
    if (data_cache) {
        if (!mutexes[MUTEX_CACHE]) {
            mutexes[MUTEX_CACHE] = SDL_CreateMutex();
        }

        SDL_LockMutex(mutexes[MUTEX_CACHE]);
        struct data_blob* current = data_cache;
        while (current) {
            if (current->owner == owner) {
//                printf ("Test fetched data\n %s \n -----------\n",(char*)current->data);
                memcpy(age, &(current->fetch_time), sizeof(time_t));
                memcpy(size, &(current->size), sizeof(Uint32));
                if (data != NULL) {
                    memcpy(data, current->data, current->size);
                    debug_log << "CACHE: returning " << current->size << " bytes\n";
                }
                SDL_UnlockMutex(mutexes[MUTEX_CACHE]);
                return (1);
            }	// found a cache hit
            current = current->next;
        }	// itterate through the current cache
        SDL_UnlockMutex(mutexes[MUTEX_CACHE]);
    } // do we have anything at all cached yet?
    debug_log << "CACHE: Cache miss\n";
    return (0);
}


size_t cache_http_callback( char* in, size_t size, size_t nmemb, void* out) {
    std::string* buffer = static_cast<std::string*>(out);
    buffer->append(in, (size*nmemb));
    return (size*nmemb);
}

int http_loader(const char* source_url, void** result) {
    if (!mutexes[MUTEX_HTTP]) {
        mutexes[MUTEX_HTTP] = SDL_CreateMutex();
    }
    SDL_LockMutex(mutexes[MUTEX_HTTP]);
#ifndef _WIN32          // *NIX version starts here
    CURLcode curlres;
    std::string httpbuffer;
//    std::cout << "HTTP: Fetching data from "<< source_url << "\n";
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
//            std::cout << "HTTP: Calling CURL fetch \n";
        curlres = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (!curlres) {
//            std::cout << "HTTP: Fetched "<< httpbuffer.size() <<" Bytes\n";
            *result = realloc(*result, httpbuffer.size()+1);

            if (*result) {
                // return our result text in *result
                memset(*result, 0, httpbuffer.size() + 1);
                memcpy(*result, httpbuffer.c_str(), httpbuffer.size());
                SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
//                std::cout << "HTTP: Returning "<< httpbuffer.size() << "\n";
                return(httpbuffer.size());
            } else {
//                std::cout << "HTTP: Curl result MALLOC error\n";
                SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
                return 0;
            }
        } else {
            SDL_Log ("Curl Fetch Error: %s", curl_easy_strerror(curlres));
            SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
            return 0;
        }
    } else {
        SDL_Log("Failed to init Curl!");
        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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
        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
        return 0;
    }
    if (source_url[0] == 0) {
        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
        return 0;
    }
    // call once to get the result size
    int len = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, source_url, -1, NULL, 0);
    if (len == 0) {
        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
        return 0;
    }

    // actually convert to UTF8
    LPWSTR utf8_url = new wchar_t[len];
    if (MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, source_url, -1, utf8_url, len) == 0) {
        delete[] utf8_url;
        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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
        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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
        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
        return 0;
    }
    else {
//        SDL_Log("Initialized HTTP correctly");
    }

    http_connection = WinHttpConnect(http, host.c_str(),
        exploded_url.nPort, 0);
    SDL_Log("Attemped to connect to server. %s (Error %u)", host.c_str(), GetLastError());
    if (!http_connection) {
        SDL_Log("Unable to connect to %ls on %u", host.c_str(), exploded_url.nPort);
        WinHttpCloseHandle(http);
        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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
        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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
        SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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
                SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
                return((int)buffstr.size());
            }
            else {
                SDL_Log("WinHttp result MALLOC error");
                WinHttpCloseHandle(http_request);
                WinHttpCloseHandle(http_connection);
                WinHttpCloseHandle(http);
                SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
                return 0;
            }

        }
    }
    WinHttpCloseHandle(http_request);
    WinHttpCloseHandle(http_connection);
    WinHttpCloseHandle(http);
    SDL_UnlockMutex(mutexes[MUTEX_HTTP]);
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

SDL_Texture* SDLCLOCK_CreateTexture(SDL_Renderer* renderer, SDL_PixelFormat format, SDL_TextureAccess access, int w, int h, const char* owner = "unknown", const char* where = "") {
    SDL_Texture* t = SDL_CreateTexture(renderer, format, access, w, h);
    if (!t) {
        SDL_Log("CreateTexture failed (%s): %s", owner, SDL_GetError());
        debug_log << "CREATE FAILED : tex=" << (void*)t
            << " w=" << w << " h=" << h
            << " owner=" << owner << " " << where << "\n";
        return nullptr;
    }
    else {
        debug_log << "CREATE: tex=" << (void*)t
            << " w=" << w << " h=" << h
            << " owner=" << owner << " " << where << "\n";
        return t;
    }
}

void SDLCLOCK_DestroyTexture(SDL_Texture* t, const char* where = "") {
    if (!t) return;
    debug_log << "DESTROY: tex=" << (void*)t
        << " at " << where << "\n";
    SDL_DestroyTexture(t);
    return;
}

void mutex_checker() {
    int index = 0;
    for (auto* mtx : mutexes) {
        if (!mtx) {
            SDL_Log("Mutex[%d]: nullptr", index);
        } else {
            if (SDL_TryLockMutex(mtx)) {
                SDL_Log("Mutex[%d]: UNLOCKED", index);
                SDL_UnlockMutex(mtx);
            } else {
                SDL_Log("Mutex[%d]: LOCKED", index);
            }
        }
        index++;
    }
}