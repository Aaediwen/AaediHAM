#include "dx_spots.h"
#include "../aaediclock.h"
#include "../utils.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#define timegm _mkgmtime
#define SHUT_RDWR SD_BOTH
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

SDL_Mutex* dxspot_mutex = SDL_CreateMutex();
SDL_TimerID dxspot_timer = 0;
const int max_age=1800;

dxspot::dxspot() {
    qrz_valid = false;
    country.clear();
};

dxspot::~dxspot() {};

void dxspot::find_mode () {
    mode.clear();
    if (note.empty()) {
        return;
    }
    std::string mode_parent = note;
    std::transform(mode_parent.begin(), mode_parent.end(), mode_parent.begin(), ::toupper);
    if (mode_parent.find("FT8") != std::string::npos) mode =  "FT8";
    if (mode_parent.find("FT4") != std::string::npos) mode =  "FT4";
    if (mode_parent.find("CW") != std::string::npos) mode = "CW";
    if (mode_parent.find("SSB") != std::string::npos) mode =  "SSB";
    if (mode_parent.find("USB") != std::string::npos) mode =  "USB";
    if (mode_parent.find("LSB") != std::string::npos) mode =  "LSB";
    if (mode_parent.find("RTTY") != std::string::npos) mode =  "RTTY";
    return;
}

void dxspot::fill_qrz() {
    query_qrz();
}
void dxspot::display_spot(ScreenFrame& panel, int y, int max_age) {
    // add to screen list
       char tempstr[128];
       SDL_Color tempcolor={128,0,0,0};
       SDL_FRect TextRect;
       TextRect.x=2;
       TextRect.y= y * 1.0f;
       TextRect.w=panel.dims.w/4;
       TextRect.h=panel.dims.h/15;

       SDL_FRect age_rect;
       age_rect.h = TextRect.h/8;
       age_rect.y = y+((TextRect.h/8)*7);
       age_rect.x = 2;
       age_rect.w = (panel.dims.w-4)*(static_cast<float>(time(NULL)-timestamp)/max_age);
       debug_log << "DXSPOTS: Spot age: "<< (time(NULL)-timestamp) << " Seconds, Bar width: "<< age_rect.w<< " pixels\n";
       SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
       SDL_SetRenderDrawColor(panel.GetRenderer(), 128, 128, 0, 255);
       SDL_RenderFillRect(panel.GetRenderer(), &age_rect );
       SDL_SetRenderTarget(panel.GetRenderer(), NULL);
       panel.render_text(TextRect, Sans, tempcolor, dx.c_str());
       TextRect.x += (panel.dims.w/4)+2;
       sprintf(tempstr, "%4.3f", (frequency/1000));
       panel.render_text(TextRect, Sans, tempcolor, tempstr);
       TextRect.x += (panel.dims.w/4)+2;
       TextRect.w /=2;
       if (mode.size() >0) {
            panel.render_text(TextRect, Sans, tempcolor, mode.c_str());
       }
       TextRect.w += (panel.dims.w/4)-(panel.dims.w/20);
       TextRect.x += (panel.dims.w/8)+1;
       if (country.size() >0) {
       panel.render_text(TextRect, Sans, tempcolor, country.c_str());
       }
       TextRect.y += ((panel.dims.h/11)+(panel.dims.h/150));
}

void dxspot::print_spot() {
    char timestr[128];
    struct tm *clocktime;
    clocktime = gmtime(&timestamp);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M", clocktime);
    debug_log << "DXSPOTS: DX: " << dx.c_str() << "\t" << frequency << "\t Spotter: " << spotter.c_str()
          << "\nDXSPOTS: Loc: " << lat << "\t" << lon << "\tMode " << mode.c_str()
          << "\nDXSPOTS: Time " << timestr << "\nDXSPOTS: Note: "<< note.c_str() << "\n";
//    SDL_Log ("DX: %s\t%lf\t Spotter: %s", dx.c_str(), frequency, spotter.c_str());
//    SDL_Log ("Loc: %lf\t%lf\tMode %s", lat, lon, mode.c_str());
//    SDL_Log ("Time: %s\nNote: %s", timestr, note.c_str());
};

void dxspot::query_qrz () {
    // query QRZ for a call location
    char* xml = 0 ;
    lat = 0.0;
    lon = 0.0;
    country.clear();
    bool lat_valid, lon_valid;
    lat_valid=false;
    lon_valid=false;
    Uint32 xml_size;
    if (dx.empty()) {
        return;
    }
    if (!clockconfig.qrz_key().empty()) {
        debug_log << "DXSPOTS: Getting new QRZ Session Key\n";
        std::string url = "https://xmldata.qrz.com/xml?s=" + clockconfig.qrz_key() + ";callsign=" + dx;
        xml_size = http_loader(url.c_str(), (void**)&xml);
        if (xml_size) {
            // parse XML for session key
            std::istringstream stream(xml);
            std::string keyline;
            size_t tag_start, tag_stop;

            while (std::getline(stream, keyline)) {
                tag_start = keyline.find("<lat>");
                tag_stop = keyline.find("</lat>");
                if ((tag_start != std::string::npos) && (tag_stop != std::string::npos)) {
                    tag_start += 5;
                    lat = std::stod(keyline.substr(tag_start, tag_stop - tag_start));
                    lat_valid = true;
                }
                tag_start = keyline.find("<lon>");
                tag_stop = keyline.find("</lon>");
                if ((tag_start != std::string::npos) && (tag_stop != std::string::npos)) {
                    tag_start += 5;
                    lon = std::stod(keyline.substr(tag_start, tag_stop - tag_start));
                    lon_valid = true;
                }
                tag_start = keyline.find("<country>");
                tag_stop = keyline.find("</country>");
                if ((tag_start != std::string::npos) && (tag_stop != std::string::npos)) {
                    tag_start += 9;
                    country = keyline.substr(tag_start, tag_stop - tag_start);
                }

                tag_start = keyline.find("<Error>");
                tag_stop = keyline.find("</Error>");
                if ((tag_start != std::string::npos) && (tag_stop != std::string::npos)) {
                    tag_start += 7;
                    std::string QRZ_Err = keyline.substr(tag_start, tag_stop - tag_start);
                    printf("QRZ Call Lookup Error: %s\n", QRZ_Err.c_str());
                    debug_log << "DXSPOTS: QRZ Call Lookup Error: " << QRZ_Err.c_str() << "\n";
                    if (!QRZ_Err.compare(0,15, "Session Timeout")) {
                        clockconfig.qrz_key(1);
                    }
                }
            }
        }
    }
    if (lat_valid && lon_valid) {
      qrz_valid=true;
    }
//    SDL_Log("DX  Coords from QRZ: %s, %f, %f", dx.c_str(), lat, lon);
   return;
}








#ifdef _WIN32
SOCKET dxsocket = 0;
#else
int dxsocket = 0;
#endif
std::vector<dxspot>dxspots;

void init_fd() {
        std::string serverip=clockconfig.dxserver().name;
        std::string serverport=std::to_string(clockconfig.dxserver().port);
        struct addrinfo* serveraddr;
        struct addrinfo hints;
        dxsocket = 0;
        int res = 0;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;       // or AF_UNSPEC to allow IPv4/IPv6
        hints.ai_socktype = SOCK_STREAM;
#ifdef _WIN32
        WSADATA wsaData;
        debug_log << "DXSPOTS: WSAStartup ... ";
        res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (res != 0) {
            debug_log << "DXSPOTS: failed: " << res << "\n";
            freeaddrinfo(serveraddr);
            return;
        } else {
            debug_log << "DXSPOTS: Success" << "\n";
        }

        debug_log << "DXSPOTS: GetAddrInfo ... ";
        res = getaddrinfo(serverip.c_str(), serverport.c_str(), &hints, &serveraddr);
        if (res == 0) {
            debug_log << " Success" << "\n";
        } else {
            debug_log << " Failed " << WSAGetLastError() << "\n";
            WSACleanup();
            freeaddrinfo(serveraddr);
            return;
        }

        debug_log << "DXSPOTS: Getting DX Socket ... ";
        dxsocket = socket(serveraddr->ai_family, serveraddr->ai_socktype, serveraddr->ai_protocol);
        if (dxsocket == INVALID_SOCKET) {
            debug_log << "Bad DX socket " << WSAGetLastError() << "\n";
            WSACleanup();
            freeaddrinfo(serveraddr);
            return;
        } else {
            debug_og << "Got DX socket" << "\n";
        }
        debug_log << "DXSPOTS: Connecting to " << serverip << " " << serverport << "\n";
        if (connect(dxsocket, serveraddr->ai_addr, serveraddr->ai_addrlen) == SOCKET_ERROR) {
            debug_log << "DXSPOTS: server connect error on client: " << WSAGetLastError() << "\n";
            shutdown(dxsocket, SHUT_RDWR);
            WSACleanup();
            dxsocket = 0;
        } else {
            debug_log << "DXSPOTS: client reporting connected to server on fd " << dxsocket << "with errno: " << errno << "\n";
        }
#else
        int addrerr =getaddrinfo(serverip.c_str(), serverport.c_str(), &hints, &serveraddr);
        if (addrerr !=0) {
            debug_log << "DXSPOTS: DX Spots connection error: " << gai_strerror(addrerr) << "\n";
        }
        dxsocket = socket(serveraddr->ai_family, serveraddr->ai_socktype, serveraddr->ai_protocol);
        if (!dxsocket) {
            debug_log << "DXSPOTS: Bad DX socket" << errno << "\n";
        }

        if (connect(dxsocket, serveraddr->ai_addr, serveraddr->ai_addrlen) == -1) {
            debug_log << "DXSPOTS: server connect error on client: " << errno << "\n";
            debug_log << "DXSPOTS: Connecting to " << serverip << " " << serverport << "\n";
            shutdown(dxsocket, SHUT_RDWR);
            dxsocket = 0;
        }
        else {
            debug_log << "DXSPOTS: client reporting connected to server on fd " << dxsocket << "with errno: " << errno << "\n";
        }
#endif
        freeaddrinfo(serveraddr);
        return;
}

void duplicate_spot(dxspot& needle) {
    size_t old_index;
    bool found=false;
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
    } else {
        needle.fill_qrz();
    }
    debug_log << "DXSPOTS: Pushing Spot " << needle.dx.c_str() << " : Age: " << (time(NULL) - needle.timestamp)<< " Seconds\n" ;
    dxspots.push_back(needle);
//    SDL_Log ("Stored: %i DX Spots", dxspots.size());
}

void fetch_dxspots() {
    time_t currenttime = time(NULL);
    SDL_LockMutex(dxspot_mutex);
    // check for old entries

    if (!dxspots.empty()) {
            for (size_t c = dxspots.size() ; c-- > 0 ;) {
                    if ((currenttime - dxspots[c].timestamp) > max_age) {
                        debug_log << "DXSPOTS: Erasing entry "<< dxspots[c].dx.c_str() << "\n";
                            dxspots.erase(dxspots.begin()+c);
                    }
            }
    }
    if (dxsocket) { // we have a valid? socket to read from
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
    //			dxsocket = 0;
                            readcount = 0;
                    }
        }

        if (read_limit <5) {

            while (readcount) {
                if (!tempstr.empty()) {
                    dxbuffer.push_back(tempstr);
                    tempstr.clear();;
                }
                readcount = read_socket(dxsocket, tempstr);
            }
            debug_log << "DXSPOTS: DONE Reading " << dxbuffer.size()<< " lines of input\n";

            for (std::string buffstr : dxbuffer) {
                // scan variables for line ID
                float freq;
                char call[32] = {0};
                char date[16] = {0};
                char timez[8] = {0};


                // process the input line
                debug_log << "DXSPOTS: buffstr " << buffstr.c_str() << "\n";
                if (!buffstr.compare(1,5, "ogin:")) {
                    send(dxsocket, clockconfig.CallSign().c_str(), clockconfig.CallSign().length(),0);
                    send(dxsocket, "\n", 1,0);
                    debug_log << "DXSPOTS: Sent Callsign\n";
                } else if (!buffstr.compare(0,5, "Hello")) {
                    send(dxsocket, "SH/DX 15\n", 9,0);
                    debug_log << "DXSPOTS: Sent SH15\n";
                } else if (buffstr.rfind("DX de ", 0) == 0) {
                    debug_log << "DXSPOTS: Got DX entry\nDXSPOTS: %s" <<  buffstr.c_str() << "\n";
                    dxspot new_spot;
                    int consumed = 0;
                    std::string tempstring;
                    size_t spotter_end = buffstr.find_first_of(':');
                    new_spot.spotter = buffstr.substr(6, spotter_end-1 );
                    tempstring=buffstr.substr(spotter_end+1,std::string::npos);
                    buffstr= tempstring;
                    debug_log << "DXSPOTS: Extracted Spotter " << new_spot.spotter.c_str() << "\n";
                    sscanf (buffstr.c_str(), "%lf %13s %n", &(new_spot.frequency), call, &consumed);
                    debug_log << "DXSPOTS: Extracted Frequency " << new_spot.frequency << "\n";
                    tempstring=buffstr.substr(consumed,std::string::npos);
                    buffstr= tempstring;
                    new_spot.dx=call;
                    debug_log << "DXSPOTS: Extracted DX " << new_spot.dx.c_str() << "\n";
                    spotter_end = buffstr.find_last_of('Z', (std::string::npos));
                    if (spotter_end != std::string::npos) {
                        tempstring = buffstr.substr(spotter_end-4, spotter_end-1 );
                        struct tm *new_time;
                        std::memset(&new_time, 0, sizeof(new_time));
                        new_time = gmtime(&currenttime);
                        if (sscanf(tempstring.c_str(), "%2d%2d",
                            &(new_time->tm_hour), &(new_time->tm_min)) != 2) {
                            debug_log << "DXSPOTS: Date Parse Error " << new_time->tm_hour << " " << new_time->tm_min << "\n";
                        }
                        new_spot.timestamp=0;
                        new_spot.timestamp = timegm(new_time);
                        debug_log << "DXSPOTS: Timestamp:" << tempstring.c_str() << " \nDXSPOTS:  Remaining:" << buffstr.c_str() << "\n";
                        new_spot.note=buffstr.substr(0, spotter_end-4 );
                    } else {
                        new_spot.timestamp = time(NULL);
                        new_spot.note="";
                    }
                    new_spot.find_mode();
                    duplicate_spot(new_spot);
                } else if (sscanf(buffstr.c_str(), "%f %31s %15s %7s", &freq, call, date, timez)==4) {
                    if (date[0] =='U') {
    //              	SDL_Log("Skipping line with unexpected keyword: %s", buffstr.c_str());
                    } else {
                        debug_log << "DXSPOTS: Got cached DX entry\n" <<  buffstr.c_str() << "\n";
                        dxspot new_spot;
                        int consumed=0;
                        std::string tempstring;
                        sscanf (buffstr.c_str(), "%lf %13s %n", &(new_spot.frequency), call, &consumed);
                        tempstring=buffstr.substr(consumed,std::string::npos);
                        buffstr= tempstring;
                        new_spot.dx=call;

                        consumed = buffstr.find('Z');
                        tempstring=buffstr.substr(0,consumed);

                        struct tm new_time;
                        std::memset(&new_time, 0, sizeof(new_time));
                        char month_str[4];
                        int year;
                        if (sscanf(tempstring.c_str(), "%d-%3s-%d %2d%2d",
                            &new_time.tm_mday, month_str, &year, &new_time.tm_hour, &new_time.tm_min) != 5) {

                            debug_log << "DXSPOTS:Date Parse Error"
                                << "\nDXSPOTS: "<< tempstring.c_str() << "\t"
                                << new_time.tm_mday << ", "
                                << month_str << ", "
                                << year <<" "
                                <<  new_time.tm_hour << " " << new_time.tm_min <<"\n";
                        }
                        new_time.tm_year = year - 1900;
                        new_time.tm_mon = month_to_int(month_str);
                        new_spot.timestamp = timegm(&new_time);

                        tempstring=buffstr.substr(consumed,std::string::npos);
                        buffstr= tempstring;
                        size_t note_end = buffstr.find_last_of('<', (std::string::npos-1));
                        size_t spotter_end = buffstr.find_last_of('>', (std::string::npos));
                        new_spot.spotter = buffstr.substr(note_end+1, spotter_end-1 );
                        new_spot.note=buffstr.substr(1, note_end-1 );
                        new_spot.find_mode();
                        duplicate_spot(new_spot);
                    }


                } else {
    //          	SDL_Log("Ignoring line");
                }
            }
            dxbuffer.clear();
        }
        SDL_UnlockMutex(dxspot_mutex);
    }
}

Uint32 SDLCALL fetch_dxspots (void *userdata, SDL_TimerID timerID, Uint32 interval) {
     if (timerID) {
          fetch_dxspots();
          return (interval);
     } else {
          return 0;
     }
}



void dx_cluster (ScreenFrame& panel) {

    if (!dxsocket) {
        init_fd();
    }
    if (!dxspot_timer) {
        fetch_dxspots();
        dxspot_timer = SDL_AddTimer(10000, fetch_dxspots, NULL);
    }
    // clear the box
    panel.Clear();
    time_t currenttime = time(NULL);

    if (!dxsocket) {
        SDL_Log("Error Connecting to DX Spot Telnet Session");
        debug_log << "DXSPOTS: Error Connecting to DX Spot Telnet Session\n";
        SDL_Color tempcolor={128,0,0,0};
        SDL_FRect TextRect;
        TextRect.x=2;
        TextRect.y=2;
        TextRect.w=panel.dims.w-4;
        TextRect.h=panel.dims.h/7;
        panel.render_text(TextRect, Sans, tempcolor, "NOT CONNECTED");
        return;
    }


    int y=2;
    SDL_LockMutex(dxspot_mutex);
    size_t start = dxspots.size() > 15 ? dxspots.size() - 15 : 0;
    for (size_t n=start ; n < dxspots.size(); n++) {
        if (y < panel.dims.h) {
            dxspots[n].display_spot(panel, y, max_age);
            y+= panel.dims.h/15;
        }
    }
    delete_owner_pins(MOD_DXSPOT);
    for (auto& current_spot : dxspots) {
        if (current_spot.qrz_valid) {
            struct map_pin dx_pin;
            dx_pin.owner       =               MOD_DXSPOT;
            sprintf(dx_pin.label, "%s", current_spot.dx.c_str());
            dx_pin.lat         =               current_spot.lat;
            dx_pin.lon         =               current_spot.lon;
            dx_pin.icon        =               0;
            dx_pin.color           =            {128,0,0,255};
            dx_pin.tooltip[0]  =        0;
            add_pin(&dx_pin);
        }
    }
    SDL_UnlockMutex(dxspot_mutex);
}

