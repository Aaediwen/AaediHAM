#include "aaediclock.h"
#include "dx_cluster.h"
#include "utils/http_fetch.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <mutex>
#ifdef _WIN32
//#include <winsock2.h>
//#include <ws2tcpip.h>
#include <time.h>
#define timegm _mkgmtime
#define SHUT_RDWR SD_BOTH
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
//#include <sys/socket.h>
//#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif


std::mutex dxspot_mutex;
SDL_TimerID dxspot_timer = 0;
std::atomic<int>exit_shutdown;
const int max_age=1800;
aaediclock_host_api* host_api = nullptr;


dxspot::dxspot() {
    qrz_valid = false;
    spotter.clear();
    dx.clear();
    note.clear();
    mode.clear();
    country.clear();
    timestamp = 0;
    frequency = 0.0;
    lat = 0.0;
    lon = 0.0;
    entity = 0;
};

dxspot::~dxspot() {};

void dxspot::find_mode () {
    mode.clear();
    if (note.empty()) {
        return;
    }
    std::string mode_parent = note;
    std::transform(mode_parent.begin(), mode_parent.end(), mode_parent.begin(), ::toupper);
    static const char* known_modes[] = { "FT8", "FT4", "CW", "USB", "LSB", "SSB", "RTTY" };
    for (const char* m : known_modes) {
        if (mode_parent.find(m) != std::string::npos) {
            mode = m;
            break;
        }
    }

    return;
}

void dxspot::fill_qrz() {
    query_qrz();
}
aaediclock_Color dxspot::band_color() {
    const double raw_frequency = frequency/1000;
    aaediclock_Color result = {128,128,128,0};
    if (raw_frequency >=1.8 && raw_frequency <=2.0) {
        //160M
        result = {139,69,19,0};
    } else if (raw_frequency >=3.5 && raw_frequency <=4.0) {
        // 80M
        result = {220,50,47,0};
    } else if (raw_frequency >=5.3 && raw_frequency <=5.5) {
        // 60M
        result = {205,92,30,0};
    }  else if (raw_frequency >=7.0 && raw_frequency <=7.3) {
        // 40M
        result = {230,200,40,0};
    } else if (raw_frequency >=10.1 && raw_frequency <=10.15) {
        // 30M
        result = {50,100,75,0};
    }  else if (raw_frequency >=14.0 && raw_frequency <=14.350) {
        // 20M
        result = {50,100,75,0};
    }  else if (raw_frequency >=18.068 && raw_frequency <=18.168) {
        // 17M
        result = {0,180,180,0};
    }  else if (raw_frequency >=21.0 && raw_frequency <=21.45) {
        // 15M
        result = {65,105,225,0};
    } else if (raw_frequency >=24.89 && raw_frequency <=24.99) {
        // 12M
        result = {138,43,226,0};
    }  else if (raw_frequency >=28.0 && raw_frequency <=29.7) {
        // 10M
        result = {200,0,200,0};
    }
    return result;
}

void dxspot::display_spot(const aaediclock_FRect& dims, float y) {
    // add to screen list
       if (y < 0) {
           *(host_api->AaediHAM_LogDebug) << "DXSPOTS: Invalid Y coordinate\n";
           return;
       }
       if (dims.w <= 0 || dims.h <= 0 ) {
           *(host_api->AaediHAM_LogDebug) << "DXSPOTS: Invalid Panel size\n";
           return;
       }
       char tempstr[128];
       aaediclock_Color tempcolor={128,0,0,0};
       tempcolor = band_color();
       aaediclock_FRect TextRect;
       TextRect.x	=	2;
       TextRect.y	=	y * 1.0f;
       TextRect.w	=	dims.w/4;
       TextRect.h	=	dims.h/15;

       aaediclock_FRect age_rect;
       age_rect.h 	= 	TextRect.h/8;
       age_rect.y 	= 	y + (( TextRect.h / 8 ) * 7 );
       age_rect.x 	= 	2;
       age_rect.w 	= 	( dims.w - 4 ) * ( static_cast<float>(time(NULL) - timestamp ) / max_age );
       if (age_rect.w > dims.w-8) {
           age_rect.w = dims.w-8;
       }
       *(host_api->AaediHAM_LogDebug) << "DXSPOTS: Spot age: "<< (time(NULL) - timestamp) << " Seconds, Bar width: "<< age_rect.w << " pixels\n";
         host_api->AaediHAM_GraphicsDrawRect(aaediclock_Color{128, 128, 0, 255}, age_rect, true);
       host_api->AaediHAM_GraphicsDrawText(dx.c_str(), tempcolor, TextRect);
       TextRect.x 	+= 	( dims.w / 4 ) + 2;
       sprintf(tempstr, "%4.3f", (frequency/1000));
       host_api->AaediHAM_GraphicsDrawText(tempstr, tempcolor, TextRect);
       TextRect.x 	+= 	( dims.w / 4 ) + 2;
       TextRect.w 	/=	2;
       if (mode.size() >0) {
           host_api->AaediHAM_GraphicsDrawText(mode.c_str(), tempcolor, TextRect);
       }
       TextRect.w 	+= 	( dims.w / 4 ) - ( dims.w / 20 );
       TextRect.x 	+= 	( dims.w / 8 ) + 1;
       if (country.size() >0) {
           host_api->AaediHAM_GraphicsDrawText(country.c_str(), tempcolor, TextRect);
       }
       return;
}

void dxspot::print_spot() {
    char timestr[128];
    struct tm *clocktime;
    clocktime = gmtime(&timestamp);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M", clocktime);
    *(host_api->AaediHAM_LogDebug) << "DXSPOTS: DX: " << dx.c_str() << "\t" << frequency << "\t Spotter: " << spotter.c_str()
          << "\nDXSPOTS: Loc: " << lat << "\t" << lon << "\tMode " << mode.c_str()
          << "\nDXSPOTS: Time " << timestr << "\nDXSPOTS: Note: "<< note.c_str() << "\n";
//    SDL_Log ("DX: %s\t%lf\t Spotter: %s", dx.c_str(), frequency, spotter.c_str());
//    SDL_Log ("Loc: %lf\t%lf\tMode %s", lat, lon, mode.c_str());
//    SDL_Log ("Time: %s\nNote: %s", timestr, note.c_str());
};

//bool lat_valid, lon_valid;
void dxspot::parse_qrz(xmlNode* start_node) {
     xmlNode* current_node = nullptr;
     for (current_node = start_node; current_node; current_node = current_node->next) {
          if (current_node->type == XML_ELEMENT_NODE) {
               std::string NodeName(reinterpret_cast<const char*>(current_node->name));
               *(host_api->AaediHAM_LogDebug) << "XML Node Name: "<< NodeName << "\n";
               std::transform(NodeName.begin(), NodeName.end(), NodeName.begin(), ::tolower);
               if (NodeName == "lat") {
                     try {
                          std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
                          lat = std::stod(xml_content);
                          lat_valid = true;
                     }  catch (std::exception& e) {
                          (void) e;
                          lat = 0;
                          lat_valid = false;
                     }
                } else if (NodeName == "lon") {
                     try {
                          std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
                          lon = std::stod(xml_content);
                          lon_valid = true;
                     }  catch (std::exception& e) {
                          (void) e;
                          lon = 0;
                          lon_valid = false;
                     }
                } else if (NodeName == "country") {
                     std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
                     country = xml_content;
                } else if (NodeName == "error") {
                     std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
                     std::string QRZ_Err = xml_content;
                     printf("QRZ Call Lookup Error: %s\n", QRZ_Err.c_str());
                     *(host_api->AaediHAM_LogDebug) << "QRZ Call Lookup Error: " << QRZ_Err.c_str() << "\n";
                     if (!QRZ_Err.compare(0,15, "Session Timeout")) {
                         *(host_api->AaediHAM_LogDebug) << "Getting new QRZ session key\n";
                         host_api->AaediHAM_ConfigGetQRZKey(true);
                     }
                } else {
                     parse_qrz(current_node->children);
                }

          }
     }

    return;
}

void dxspot::query_qrz () {
    // query QRZ for a call location
    char* xml = 0 ;
    lat = 0.0;
    lon = 0.0;
    country.clear();
    lat_valid=false;
    lon_valid=false;
    Uint64 xml_size;
    if (dx.empty()) {
        return;
    }
    std::string qrz_key = host_api->AaediHAM_ConfigGetQRZKey(false);
    if (!qrz_key.empty()) {
        *(host_api->AaediHAM_LogDebug) << "checking QRZ for "<< dx << "\n";
        std::string url = "https://xmldata.qrz.com/xml?s=" + qrz_key + ";callsign=" + dx;
        xml_size = http_loader(url.c_str(), (void**)&xml, 5);
        if (xml_size) {
          xmlDocPtr xml_tree = 0;
          xml_tree = xmlReadMemory(xml, static_cast<int>(xml_size), nullptr, nullptr, 0);
          if (!xml_tree) {
               *(host_api->AaediHAM_LogDebug) << "Failed to parse QRZ XML\n";
          } else {
               parse_qrz(xmlDocGetRootElement(xml_tree));
               xmlFreeDoc (xml_tree);
               xml_tree = nullptr;
          }


        }
        if (xml) {
            free(xml);
            xml = 0;
        }
    }
    if (lat_valid && lon_valid) {
      qrz_valid=true;
    }
//    SDL_Log("DX  Coords from QRZ: %s, %f, %f", dx.c_str(), lat, lon);
   return;
}




dx_socket_t dxsocket = 0;
std::vector<dxspot>dxspots;
unsigned int rand_seed = 0;

void duplicate_spot(dxspot& needle) {
    size_t old_index = 0;
    bool found=false;
    {
        const std::lock_guard<std::mutex>cluster_lock(dxspot_mutex);
        for (size_t c = 0 ; c < dxspots.size() ; c++) {
            if (dxspots[c].dx == needle.dx) {
                old_index = c;
                found=true;
                break;
            }
        }
        if (found) {
            if (needle.mode.empty()) {
                needle.mode = dxspots[old_index].mode;
            }
            needle.lat = dxspots[old_index].lat;
            needle.lon = dxspots[old_index].lon;
            needle.qrz_valid=dxspots[old_index].qrz_valid;
            needle.country = dxspots[old_index].country;
            dxspots.erase(dxspots.begin() + old_index);
        }
    }
    if (!found) {
        needle.fill_qrz();
    }
    const std::lock_guard<std::mutex>cluster_lock(dxspot_mutex);
    *(host_api->AaediHAM_LogDebug) << "DXSPOTS: Pushing Spot " << needle.dx.c_str() << " : Age: " << (time(NULL) - needle.timestamp)<< " Seconds\n" ;
    dxspots.push_back(needle);
//    SDL_Log ("Stored: %i DX Spots", dxspots.size());
}

int SDLCALL fetch_dxspots(void* data) {
    (void)data;
    exit_shutdown = 5;
    time_t currenttime = time(NULL);
    bool clean_socket = true;
    *(host_api->AaediHAM_LogDebug) << "Locking Mutex -- checking for stale spots\n";
    dxspot_mutex.lock();
// check for valid connection
    try {
        if (!dxsocket) {
            *(host_api->AaediHAM_LogDebug) << "Connecting to DX Cluster\n";
            struct plugin_server_info dx_server = host_api->AaediHAM_ConfigGetDXServer();
            std::string serverip=dx_server.name;
            std::string serverport=std::to_string(dx_server.port);
            dxsocket = init_fd(dx_server, host_api);
        }
        // check for old entries
        if (!dxspots.empty()) {
            for (size_t c = dxspots.size() ; c-- > 0 ;) {
                if ((currenttime - dxspots[c].timestamp) > max_age) {
                    *(host_api->AaediHAM_LogDebug) << "DXSPOTS: Erasing entry "<< dxspots[c].dx.c_str() << "\n";
                    dxspots.erase(dxspots.begin()+c);
                }
            }
        }
    }  catch (std::exception& e) {
        *(host_api->AaediHAM_LogDebug) << "Exception "<< e.what() << "Checking old entries\n";
    }
    dxspot_mutex.unlock();
    if (exit_shutdown < 5) {
        exit_shutdown = 0;
        return 0;
    }
    *(host_api->AaediHAM_LogDebug) << "UnLocking Mutex -- checking for stale spots\n";
    if (dxsocket) { // we have a valid? socket to read from
        *(host_api->AaediHAM_LogDebug) << "Reading from socket\n";
        std::vector<std::string> dxbuffer;
        std::string tempstr;
        int readcount=0;
        int read_limit=0;

        while (!readcount && read_limit < 5) {
            read_limit++;
            if (dxsocket) {
                readcount = read_socket(dxsocket, tempstr);
            }
            if (readcount < 0) {
                clean_socket = false;
                readcount = 0;
            }
        }

        if (read_limit <5) {
            *(host_api->AaediHAM_LogDebug) << "Read has data waiting\n";
            while (readcount) {
                if (!tempstr.empty()) {
                    dxbuffer.push_back(tempstr);
                    tempstr.clear();
                }
                if (dxsocket) {
                    readcount = read_socket(dxsocket, tempstr);
                }
                if (readcount < 0) {
                    clean_socket = false;
                    readcount = 0;
                }
            }
            *(host_api->AaediHAM_LogDebug) << "DONE Reading " << dxbuffer.size()<< " lines of input\n";
#ifdef _WIN32
            int send_result = 0;
#else
            ssize_t send_result = 0;
#endif
            for (std::string& buffstr : dxbuffer) {
                if (exit_shutdown < 5) {
                    exit_shutdown = 0;
                    return 0;
                }
                // scan variables for line ID
                float freq;
                char call[32] = {0};
                char date[16] = {0};
                char timez[8] = {0};

                // process the input line
                *(host_api->AaediHAM_LogDebug) << "buffstr " << buffstr.c_str() << "\n";
                if (buffstr.size() >= 6 && (!buffstr.compare(1,5, "ogin:")) || (buffstr.find("enter your call") != std::string::npos)) {
                    std::string callsign =  host_api->AaediHAM_ConfigGetCall();
                    if (!rand_seed) {
                        rand_seed = static_cast<unsigned int>(time(0));
                    }
#ifdef _WIN32
                    std::srand((unsigned)rand_seed);
                    callsign += "-" + std::to_string(rand() % 100);
#else
                    callsign += "-" + std::to_string(rand_r(&rand_seed) % 100);
#endif
                    send_result = send(dxsocket, callsign.c_str(), static_cast<int>(callsign.length()),0);
                    send_result = send(dxsocket, "\n", 1,0);
                    *(host_api->AaediHAM_LogDebug) << "Sent Callsign " << callsign << "\n";
                } else if (buffstr.size() >= 5 &&  !buffstr.compare(0,5, "Hello")) {
                    send_result = send(dxsocket, "SH/DX 15\n", 9,0);
                    *(host_api->AaediHAM_LogDebug) << "Sent SH15\n";
                } else if (buffstr.rfind("DX de ", 0) == 0) { // these cases need seperate helpers to clean up the code
                    *(host_api->AaediHAM_LogDebug) << "DXSPOTS: Got DX entry\nDXSPOTS: %s" <<  buffstr.c_str() << "\n";
                    dxspot new_spot;
                    int consumed = 0;
                    bool valid_spot = true;
                    std::string tempstring;
                    size_t spotter_end = buffstr.find_first_of(':');
//                    new_spot.spotter = buffstr.substr(6, spotter_end-1 );
                    if (spotter_end == std::string::npos || spotter_end <7) {
                        valid_spot = false;
                        *(host_api->AaediHAM_LogDebug) << "Invalid Spotter, ignoring spot \n";
                    }
                    if (valid_spot) {
                        new_spot.spotter = buffstr.substr(6, spotter_end-7 );
                        tempstring=buffstr.substr(spotter_end+1,std::string::npos);
                        buffstr = tempstring;
                        *(host_api->AaediHAM_LogDebug) << "Extracted Spotter " << new_spot.spotter.c_str() << "\n";
                    }
                    if (valid_spot) {
                        if (sscanf(buffstr.c_str(), "%lf %13s %n", &(new_spot.frequency), call, &consumed)==2) {
                            *(host_api->AaediHAM_LogDebug) << "Extracted Frequency " << new_spot.frequency << "\n";
                            tempstring = buffstr.substr(consumed,std::string::npos);
                            buffstr = tempstring;
                            new_spot.dx = call;
                            *(host_api->AaediHAM_LogDebug) << "Extracted DX " << new_spot.dx.c_str() << "\n";
                        } else {
                            valid_spot = false;
                            *(host_api->AaediHAM_LogDebug) << "Invalid Frequency, ignoring spot \n";
                        }

                    }
                    spotter_end = buffstr.find_last_of('Z', (std::string::npos));
                    if (spotter_end != std::string::npos && spotter_end >= 4) {
                        tempstring = buffstr.substr(spotter_end-4, 4 );
                        struct tm *new_time;
                        new_time = gmtime(&currenttime);
                        if (sscanf(tempstring.c_str(), "%2d%2d",
                            &(new_time->tm_hour), &(new_time->tm_min)) != 2) {
                            *(host_api->AaediHAM_LogDebug) << "Date Parse Error " << new_time->tm_hour << " " << new_time->tm_min << "\n";
                        }
                        new_spot.timestamp=0;
                        new_spot.timestamp = timegm(new_time);
                        *(host_api->AaediHAM_LogDebug) << "Timestamp:" << tempstring.c_str() << " \nDXSPOTS:  Remaining:" << buffstr.c_str() << "\n";
                        new_spot.note=buffstr.substr(0, spotter_end-4 );
                    } else {
                        new_spot.timestamp = time(NULL);
                        new_spot.note="";
                    }
                    if (valid_spot) {
                        dxspot_mutex.lock();
                        new_spot.find_mode();
                        dxspot_mutex.unlock();
                            duplicate_spot(new_spot);
                    }

                } else if (sscanf(buffstr.c_str(), "%f %31s %15s %7s", &freq, call, date, timez)==4) {
                    if (date[0] =='U') {
    //              	SDL_Log("Skipping line with unexpected keyword: %s", buffstr.c_str());
                    } else {
                        *(host_api->AaediHAM_LogDebug) << "Got cached DX entry\n" <<  buffstr.c_str() << "\n";
                        bool valid_spot = true;
                        dxspot new_spot;
                        size_t consumed=0;
                        std::string tempstring;
                        int scansize;
                        if (sscanf (buffstr.c_str(), "%lf %13s %n", &(new_spot.frequency), call, &scansize)==2) {
                            if (scansize >0 && (scansize < buffstr.size())) {
                            consumed += scansize;
                            tempstring=buffstr.substr(consumed,std::string::npos);
                            buffstr= tempstring;
                            new_spot.dx=call;
                            } else {
                                valid_spot = false;
                            }
                        } else {
                            valid_spot = false;
                        }
                        consumed = buffstr.find('Z');
                        tempstring=buffstr.substr(0,consumed);

                        struct tm new_time;
                        std::memset(&new_time, 0, sizeof(new_time));
                        char month_str[4];
                        int year;
                        if (sscanf(tempstring.c_str(), "%d-%3s-%d %2d%2d",
                            &new_time.tm_mday, month_str, &year, &new_time.tm_hour, &new_time.tm_min) != 5) {

                            *(host_api->AaediHAM_LogDebug) << "Date Parse Error"
                                << "\nDXSPOTS: "<< tempstring.c_str() << "\t"
                                << new_time.tm_mday << ", "
                                << month_str << ", "
                                << year <<" "
                                <<  new_time.tm_hour << " " << new_time.tm_min <<"\n";
                        }
                        new_time.tm_year = year - 1900;
                        new_time.tm_mon = month_to_int(month_str);
                        new_spot.timestamp = timegm(&new_time);
                        if ((consumed != std::string::npos) && (consumed < buffstr.size())) {
                            tempstring=buffstr.substr(consumed,std::string::npos);
                            buffstr= tempstring;
                            size_t note_end = buffstr.find_last_of('<', (std::string::npos-1));
                            size_t spotter_end = buffstr.find_last_of('>', (std::string::npos));
                            new_spot.spotter = buffstr.substr(note_end+1, (spotter_end-note_end+1) );
                            new_spot.note=buffstr.substr(1, note_end-1 );
                            new_spot.find_mode();
                            duplicate_spot(new_spot);
                        } else {
                            *(host_api->AaediHAM_LogDebug) << "Invalid Spot: " << buffstr << "\n";
                        }
                    }


                } else {
              	    *(host_api->AaediHAM_LogDebug) << "Ignoring line "<< buffstr << "\n";
                }
            }
            if (send_result < 0) {
                clean_socket = false;
                *(host_api->AaediHAM_LogDebug) << "Failed to send Command: " << strerror(errno) << "\n";
            }
            dxbuffer.clear();
        }
        if (!clean_socket) {
            *(host_api->AaediHAM_LogDebug) << "read_socket indicates failure; will close socket\n";
#ifdef _WIN32
            shutdown(dxsocket, SHUT_RDWR);
            closesocket(dxsocket);
            WSACleanup();
#else
            shutdown(dxsocket, SHUT_RDWR);
            close(dxsocket);
#endif
            dxsocket = 0;
        }
    }
    exit_shutdown = 0;
    return 0;
}

Uint32 SDLCALL fetch_dxspots (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)interval;
    (void)userdata;
     if (timerID) {
          SDL_Thread* thread = SDL_CreateThread(fetch_dxspots, "DX Spot Fetcher", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              *(host_api->AaediHAM_LogDebug) << "Failed to Create DX Spot Fetch Thread\n";
          }
          return (10000);
     } else {
          return 0;
     }
}

extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new dx_cluster_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void dx_cluster_plugin::plugin_init() const {
    exit_shutdown = 0;
    if (!dxspot_timer) {
        dxspot_timer = SDL_AddTimer(1000, fetch_dxspots, NULL);
    }
    return;
}

void dx_cluster_plugin::plugin_exit() const {
    *(host_api->AaediHAM_LogDebug) << "exiting module\n";
    if (dxspot_timer) {
        SDL_RemoveTimer(dxspot_timer);
    }
    int limit = 0;
    exit_shutdown = 2;
    while (exit_shutdown && limit < 1000) {
        SDL_Delay(10);
        limit ++;
    }
    if (exit_shutdown) {
        *(host_api->AaediHAM_LogDebug) << "Worker failed to return on shutdown\n";
    }
    const std::lock_guard<std::mutex>cluster_lock(dxspot_mutex);
    if (dxsocket) {
#ifdef _WIN32
        shutdown(dxsocket, SHUT_RDWR);
        closesocket(dxsocket);
        WSACleanup();
    #else
        shutdown(dxsocket, SHUT_RDWR);
        close(dxsocket);
#endif
    }
    dxsocket = 0;
    return;
}

void dx_cluster_plugin::plugin_main(const aaediclock_FRect& dims) const {

    const char* callsign = host_api->AaediHAM_ConfigGetCall();
    if (!dxsocket) {
        std::cout << "Error Connecting to DX Spot Telnet Session\n";
        *(host_api->AaediHAM_LogDebug) << "Error Connecting to DX Spot Telnet Session\n";
        aaediclock_Color tempcolor={128,0,0,0};
        aaediclock_FRect TextRect;
        TextRect.x=2;
        TextRect.y=2;
        TextRect.w=dims.w-4;
        TextRect.h=dims.h/7;
        host_api->AaediHAM_GraphicsClear();
        host_api->AaediHAM_GraphicsDrawText("NOT CONNECTED", tempcolor, TextRect);
        return;
    }

    float y = 2.0f;
    *(host_api->AaediHAM_LogDebug) << "Locking Mutex\n";
    if (dxspot_mutex.try_lock()) {
        host_api->AaediHAM_GraphicsClear();
        size_t start = dxspots.size() > 15 ? dxspots.size() - 15 : 0;
        struct plugin_mouse_event mouse_event = host_api->AaediHAM_GetMouseEvent();
        *(host_api->AaediHAM_LogDebug) << "Setting DX based on mouse event, and populating panel\n";

        for (size_t n=start ; n < dxspots.size(); n++) {
            if (y < dims.h) {
                host_api->AaediHAM_SetTarget();
                dxspots[n].display_spot(dims, y);


                if (mouse_event.valid) {
    //                SDL_Log ("Click event in  module at %f, %f", mouse_event.coords.x, mouse_event.coords.y);
                    if ( mouse_event.coords.y >=y &&  mouse_event.coords.y <= (y + dims.h/15)) {
                          struct aaediclock_dx new_dx;
                          new_dx.lat = dxspots[n].lat;
                          new_dx.lon = dxspots[n].lon;
    //                      new_dx.label = dxspots[n].dx;
                          strncpy(new_dx.label, dxspots[n].dx.c_str(),31);
                          new_dx.label[31]=0;
                          host_api->AaediHAM_ConfigSetDX(new_dx);
                    }

                }

                y+= static_cast<int>(dims.h)/15;
            }
        }
        *(host_api->AaediHAM_LogDebug) << "Setting pins\n";
        host_api->AaediHAM_MapPinDelete();
        for (auto& current_spot : dxspots) {
            if (current_spot.qrz_valid) {
                struct aaediclock_map_pin dx_pin;
                dx_pin.owner     =       0;


                sprintf(dx_pin.label, "%s", current_spot.dx.c_str());
                dx_pin.lat         =               current_spot.lat;
                dx_pin.lon         =               current_spot.lon;
                dx_pin.icon        =               0;
                dx_pin.color           =            {128,0,0,255};
                dx_pin.tooltip[0]  =        0;
                host_api->AaediHAM_MapPinAdd(dx_pin);
            }
        }
        dxspot_mutex.unlock();
        *(host_api->AaediHAM_LogDebug) << "Mutex Unlock, exiting\n";
    } else {
        *(host_api->AaediHAM_LogDebug) << "DX Cluster lock failed, exiting\n";
    }
    return;
}

const char* dx_cluster_plugin::getName() const {
    return "DX Cluster Module";
}

void dx_cluster_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

