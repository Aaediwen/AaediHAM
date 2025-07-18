#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include "aaediclock.h"
#include "modules.h"
#include "utils.h"
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

#include "json.hpp"
using json = nlohmann::json;
// https://retrieve.pskreporter.info/query?senderCallsign=YOUR_CALLSIGN
// VOACAP (or VOACAPL)
//HamQSL.com / NOAA API
// https://www.dxfuncluster.com:8000/
// telnet:
//dxfun.com:8000
//dxc.k1ttt.net:23
//dxspots.com:7300

int dxsocket = 0;
std::vector<dxspot>dxspots;

void init_fd() {
        std::string serverip="dxfun.com";
        std::string serverport=std::to_string(8000);
//        std::cout << "Client connecting to server: "<< serverip << "\n";
        struct addrinfo *serveraddr;

        struct addrinfo hints;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;       // or AF_UNSPEC to allow IPv4/IPv6
        hints.ai_socktype = SOCK_STREAM;
        getaddrinfo(serverip.c_str(), serverport.c_str(), &hints, &serveraddr);
#ifdef _WIN32
        dxsocket = (int)socket(serveraddr->ai_family, serveraddr->ai_socktype, serveraddr->ai_protocol);
#else
        dxsocket = socket(serveraddr->ai_family, serveraddr->ai_socktype, serveraddr->ai_protocol);
#endif

        if ( connect(dxsocket, serveraddr->ai_addr, serveraddr->ai_addrlen) == -1) {
                std::cout << "server connect error on client: " << errno << "\n";
                shutdown (dxsocket, SHUT_RDWR);
                dxsocket=0;
        } else {
//                std::cout << "client reporting connected to server on fd " << dxsocket << "with errno: " << errno << "\n";
        }
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
//    SDL_Log ("Pushing Spot %s : Age: %li Seconds", needle.dx.c_str(), (time(NULL) - needle.timestamp)) ;
    dxspots.push_back(needle);
}


void dx_cluster (ScreenFrame& panel) {
    if (!dxsocket) {
        init_fd();
    }
//    SDL_Log("DX Cluster");
    // clear the box
    panel.Clear();
    const int max_age=1800;
    for (size_t c = dxspots.size() ; c-- > 0 ;) {
        if ((currenttime - dxspots[c].timestamp) > max_age) {
//            SDL_Log("Erasing entry %s", dxspots[c].dx.c_str());
            dxspots.erase(dxspots.begin()+c);
        }
    }

    if (!dxsocket) {
            SDL_Log("Error Connecting to DX Spot Telnet Session");
            SDL_Color tempcolor={128,0,0,0};
            SDL_FRect TextRect;
            TextRect.x=2;
            TextRect.y=2;
                    TextRect.w=panel.dims.w-4;
                    TextRect.h=panel.dims.h/7;
                    panel.render_text(TextRect, Sans, tempcolor, "NOT CONNECTED");
            return;
        }

    std::vector<std::string> dxbuffer;
    std::string tempstr;
    int readcount=0;
    int read_limit=0;
    while (!readcount && read_limit < 5) {
        read_limit++;
        readcount = read_socket(dxsocket, tempstr);
    }
    if (read_limit <5) {
//    SDL_Log ("Read input");
    while (readcount) {
        if (!tempstr.empty()) {
            dxbuffer.push_back(tempstr);
            tempstr.clear();;
        }
        readcount = read_socket(dxsocket, tempstr);
    }
//        SDL_Log ("DONE Reading %li lines of input", dxbuffer.size());
    for (std::string buffstr : dxbuffer) {
    // scan variables for line ID
    float freq;
    char call[32] = {0};
    char date[16] = {0};
    char timez[8] = {0};




        if (!buffstr.compare(1,5, "ogin:")) {
               send(dxsocket, clockconfig.CallSign().c_str(), clockconfig.CallSign().length(),0);
               send(dxsocket, "\n", 1,0);
//               SDL_Log ("Sent Callsign");
        } else if (!buffstr.compare(0,5, "Hello")) {
               send(dxsocket, "SH/DX 15\n", 9,0);
//               SDL_Log ("Sent SH15");
        } else if (buffstr.rfind("DX de ", 0) == 0) {
//               SDL_Log ("Got DX entry\n%s", buffstr.c_str());
               dxspot new_spot;
               int consumed = 0;
               std::string tempstring;
               size_t spotter_end = buffstr.find_first_of(':');
               new_spot.spotter = buffstr.substr(6, spotter_end-1 );
               tempstring=buffstr.substr(spotter_end+1,std::string::npos);
               buffstr= tempstring;
//               SDL_Log ("Extracted Spotter %s", new_spot.spotter.c_str());
               sscanf (buffstr.c_str(), "%lf %13s %n", &(new_spot.frequency), call, &consumed);
//               SDL_Log ("Extracted Frequency %f", new_spot.frequency);
              tempstring=buffstr.substr(consumed,std::string::npos);
              buffstr= tempstring;
              new_spot.dx=call;
//              SDL_Log ("Extracted DX %s", new_spot.dx.c_str());
              spotter_end = buffstr.find_last_of('Z', (std::string::npos));
              tempstring = buffstr.substr(spotter_end-4, spotter_end-1 );
              struct tm *new_time;
              std::memset(&new_time, 0, sizeof(new_time));
              new_time = gmtime(&currenttime);
              if (sscanf(tempstring.c_str(), "%2d%2d",
                &(new_time->tm_hour), &(new_time->tm_min)) != 2) {
                   SDL_Log("Date Parse Error %i %i", new_time->tm_hour, new_time->tm_min);
                 }
              new_spot.timestamp=0;
              new_spot.timestamp = timegm(new_time);
//              SDL_Log("Timestamp:%s \n Remaining:%s", tempstring.c_str(), buffstr.c_str());
              new_spot.note=buffstr.substr(0, spotter_end-4 );
              new_spot.find_mode();
              duplicate_spot(new_spot);
//              new_spot.fill_qrz();
//              dxspots.push_back(new_spot);
              // need routine here to find and merge duplicates instead of repeating fill qrz

        } else if (sscanf(buffstr.c_str(), "%f %31s %15s %7s", &freq, call, date, timez)==4) {
//              SDL_Log ("Got cached DX entry\n%s", buffstr.c_str());

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
                   SDL_Log("Date Parse Error\n %s\t%i, %s, %i %i %i", tempstring.c_str(),  new_time.tm_mday, month_str, year, new_time.tm_hour, new_time.tm_min);
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
//                new_spot.fill_qrz();
//                dxspots.push_back(new_spot);


        }
    }
    dxbuffer.clear();
    }
    int y=2;
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
            dx_pin.color 	   =		{128,0,0,255};
            dx_pin.tooltip[0]  = 	0;
            add_pin(&dx_pin);
        }
    }
//    SDL_Log("Done with DX cluster");
}


void circle_helper(std::vector<SDL_FPoint> *circle_points, int radius, SDL_FPoint center, int segments) {
    for (int i = 0; i <= segments; ++i) {
        float theta = (2.0f * M_PI * i) / segments;
        SDL_FPoint pt = {
            center.x + radius * cosf(theta),
            center.y + radius * sinf(theta)
        };
        circle_points->push_back(pt);
    }
    return;
}

void pass_tracker(ScreenFrame& panel, TrackedSatellite& sat) {
    // clear the box
    panel.Clear();

    char tempstr[30];
    SDL_FRect TextRect;
    TextRect.w=panel.dims.w/2;
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    sprintf(tempstr, "%s", sat.get_name().c_str());
    panel.render_text(TextRect, Sans, sat.color, tempstr);
    time_t pass_time = sat.pass_start();
    if (pass_time) {
        tm* test_time;
        TextRect.y=panel.dims.h - (panel.dims.h/11)-4;
        TextRect.w=panel.dims.w /3;
        test_time = localtime(&pass_time);
        strftime(tempstr, 12, "%H:%M", test_time);
        panel.render_text(TextRect, Sans, sat.color, tempstr);
        TextRect.x=panel.dims.w - (panel.dims.w/3);
        pass_time = sat.pass_end();
        test_time = localtime(&pass_time);
        strftime(tempstr, 12, "%H:%M", test_time);
        panel.render_text(TextRect, Sans, sat.color, tempstr);
    }

    SDL_SetRenderTarget(surface, panel.texture);
    std::vector<SDL_FPoint> circle_pts;
    float radius = panel.dims.w/2;
    if (panel.dims.h < panel.dims.w) {
        radius = panel.dims.h/2;
    }
    radius *=.8;
    std::vector<SDL_FPoint> pass_pts;
    SDL_FRect pass_box;
    pass_box.x=(panel.dims.w - (2*radius))/2;
    pass_box.y=(panel.dims.h - (2*radius))/2;
    pass_box.w=2*radius;
    pass_box.h=2*radius;


    SDL_FPoint center = SDL_FPoint{panel.dims.w/2, panel.dims.h/2};
    SDL_SetRenderDrawColor(surface, 64, 64, 0, 255);
    SDL_RenderLine(surface, center.x, center.y, center.x-radius, center.y);
    SDL_RenderLine(surface, center.x, center.y, center.x+radius, center.y);
    SDL_RenderLine(surface, center.x, center.y, center.x, center.y+radius);
    SDL_RenderLine(surface, center.x, center.y, center.x, center.y-radius);
    SDL_SetRenderDrawColor(surface, 64, 0, 64, 255);
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(surface, circle_pts.data(), circle_pts.size());
    circle_pts.clear();
    radius /=2;
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(surface, circle_pts.data(), circle_pts.size());
    circle_pts.clear();
    radius /=2;
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(surface, circle_pts.data(), circle_pts.size());
    circle_pts.clear();
    radius *=3;
    circle_helper (&circle_pts, radius, center, 32);
    SDL_RenderLines(surface, circle_pts.data(), circle_pts.size());
    SDL_RenderPoint(surface, center.x, center.y);
    sat.draw_pass(sat.pass_start(), sat.pass_end(),  &pass_pts, &pass_box);
    SDL_SetRenderDrawColor(surface, 0, 128, 0, 255);
    SDL_RenderLines(surface, pass_pts.data(), pass_pts.size());


    return;
}

/*
void thread_gen(TrackedSatellite sat, libsgp4::Observer obs) {
    sat.gen_telemetry(30, obs));
    return;
}
*/

Uint16 pass_pager[2] = {0,0};
std::vector<TrackedSatellite> satlist;
void sat_tracker (ScreenFrame& panel, TTF_Font* font, ScreenFrame& map) {
    char* amateur_tle = 0 ;
    Uint32 data_size;
    time_t cache_time;

    SDL_FRect TextRect;
//    SDL_Log ("In Sat TRracker Module");
    delete_owner_pins(MOD_SAT);
    bool reload_flag = false;
    data_size = cache_loader(MOD_SAT, &amateur_tle, &cache_time);
    if (!data_size) {
        reload_flag=true;
    } else if ((time(NULL) - cache_time) > 14400) {
        reload_flag=true;
    }

    if (reload_flag) {
//        data_size = http_loader("https://aaediwen.theaudioauthority.net/morse/celestrak", &amateur_tle);	// debug
	data_size = http_loader("https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=tle", &amateur_tle);	// live
	satlist.clear();
        if (data_size) {
            add_data_cache(MOD_SAT, data_size, amateur_tle);
        }
    }

//    SDL_Log ("Read Sat List");
    // clear the box
    panel.Clear();
//    SDL_SetRenderTarget(surface, panel.texture);

    // render the header
    TextRect.w=panel.dims.w/2-10;
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    panel.render_text(TextRect, font, {128,128,0,255}, "SAT TRACKERS");
    TextRect.w=panel.dims.w-10;
    TextRect.y += ((panel.dims.h/11)+(panel.dims.h/150));

    if (data_size) {
//    	SDL_Log ("We have tracking data: %i Bytes", data_size);
    } else {
        SDL_Log ("Tracking Data Fetch Error!");
    	return;
    }

    Uint32 read_flag = 1;
    std::istringstream iostring_buffer;
    std::string sanitized(amateur_tle);  // Make a copy (if amateur_tle is char*)
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\r'), sanitized.end());
    iostring_buffer.clear();
    iostring_buffer.str(sanitized);
    std::string temp[3]; // name line1, line 2

    libsgp4::Observer obs(clockconfig.DE().latitude, clockconfig.DE().longitude, 0.27); // need a way to manage altitude here (last arg)
    SDL_Color trackcols = {255,0,0,255};
    // read the TLE data from Celestrak
//    SDL_Log ("Reading Sat lists from Celestrak");
    while (read_flag) {
        // read the TLE for a sat
        std::getline(iostring_buffer, temp[0]);
	std::getline(iostring_buffer, temp[1]);
	std::getline(iostring_buffer, temp[2]);
	if (temp[0].length() && temp[1].length() && temp[2].length()) {
	    // is it one we want to show?
            bool draw_flag=false;
	    for (const std::string& stropt : clockconfig.Sats()) {
	        if (temp[0].compare(0,stropt.length(),stropt)==0) {
	            draw_flag=true;
                }
            }
            // check if the sat exists in satlist
             TrackedSatellite *nextsat = nullptr;
             if (draw_flag) {
                 for (TrackedSatellite& sat : satlist) {
                    if (temp[0].compare(0,sat.get_name().length(),sat.get_name())==0) {
                        nextsat = &sat;
                    }
                }
            }
            if (nextsat) {
//                nextsat->new_tracking(temp[0], temp[1], temp[2]);
//                nextsat->gen_telemetry(30, obs);
            } else {

                trackcols.r -= 20;
                trackcols.g += 20;
                trackcols.b += 10;
                // if so, calculate the track for it
                if (draw_flag) {
                    nextsat = new TrackedSatellite(temp[0], temp[1], temp[2]);
//                    TrackedSatellite nextsat(temp[0], temp[1], temp[2]);
                    nextsat->color=trackcols;
                    SDL_Log ("Regenerate track for %s", temp[0].c_str());
                    if (nextsat->gen_telemetry(30, obs)) {
                        satlist.push_back(*nextsat);
                    }
                }
            }
	} else { break; }
    } // read from Celestrak
    if (amateur_tle) {
        free(amateur_tle);
        amateur_tle = nullptr;
    }
//    SDL_Log("Displaying Selected Satellites");
    // display the selected satellites
    if (!satlist.empty()){
        if (pass_pager[0] >= satlist.size()) {
            pass_pager[0]=0;
        }
        pass_tracker(panel, satlist[pass_pager[0]]);
        if (pass_pager[1] >5) {
            pass_pager[0]++;
            pass_pager[1]=0;
        }
        pass_pager[1]++;
        for (TrackedSatellite& Sat : satlist) {
            bool draw_flag=false;
            for (const std::string& stropt : clockconfig.Sats()) {
                if (Sat.get_name().compare(0,stropt.length(),stropt)==0) {
                    draw_flag=true;
                }
            }
            if (draw_flag) {
                Sat.draw_telemetry(map);
                // plot the sat's current location
                struct map_pin sat_pin;
                SDL_FPoint sat_loc;
                Sat.location(&sat_loc);
                sat_pin.owner	=		MOD_SAT;
                sprintf(sat_pin.label, "%s", Sat.get_name().c_str());
                sat_pin.lat 	= 		sat_loc.x;
                sat_pin.lon 	= 		sat_loc.y;
                sat_pin.icon	=		0;
                sat_pin.color	=		Sat.color;;
                sat_pin.tooltip[0]	=		0;
                add_pin(&sat_pin);
            }
        }

//        SDL_Log("Loaded %li SATS", satlist.size());
    }
//    satlist.clear();
       // clean up
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));

    return;
}


int pota_page[2]={0,2};
void pota_spots(ScreenFrame& panel, TTF_Font* font) {
//    SDL_Log("Drawing POTA");
    char* json_spots = 0 ;
//    char** cache_data_address;

    int c, tot;
    c=0;
    tot=0;


    char tempstr[64];
    SDL_FRect TextRect;

    SDL_Color pota_color;
    pota_color.r = 0;
    pota_color.g = 128;
    pota_color.b = 0;
    pota_color.a = 200;
    Uint32 data_size;
    time_t cache_time;
    int reload_flag =0;
    // fetch the POTA spot data
    delete_owner_pins(MOD_POTA);
    data_size = cache_loader(MOD_POTA, &json_spots, &cache_time);
    if (!data_size) {
        reload_flag=1;
    } else if ((time(NULL) - cache_time) > 300) {
        reload_flag=1;
    }
    if (reload_flag) {
         data_size = http_loader("https://api.pota.app/spot/activator", &json_spots);				// live
//         data_size = http_loader("https://aaediwen.theaudioauthority.net/morse/activator", &json_spots);	// debug
         if (data_size) {
             add_data_cache(MOD_POTA, data_size, json_spots);
         }
    }

//    json_spots = *cache_data_address;
//    SDL_Log("POTA Cache loader call complete");
    // convert the POTA JSON to an object
    int goodread;
    goodread = 1;
    json spot_list;
    if (data_size && json_spots) {
//        SDL_Log("WE have SPOT data: %li bytes", strlen(json_spots));
        try {
            spot_list=json::parse(json_spots);
        } catch (const json::parse_error &e) {
            SDL_Log("POTA Json Parse Error %li bytes %s\n", strlen(json_spots), json_spots);
            goodread=0;
        }
        free(json_spots);
        json_spots = nullptr;
    } else {
        SDL_Log("POTA Json FETCH Error");
        goodread=0;
    }
//    SDL_Log("Parsed JSON Data");
    // clear the box
    panel.Clear();

    // render the header
    TextRect.w=panel.dims.w/2-10;
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    pota_color.a = 0;
    panel.render_text(TextRect, font, pota_color, "POTA ACTIVATORS");

    // set up for rendering the lista and submitting the pins
    pota_color.a = 200;
    TextRect.w=(panel.dims.w/4)-10;
    TextRect.h=panel.dims.h/11;
    TextRect.x=5;
    TextRect.y=((panel.dims.h/11)+(panel.dims.h/150));;
//    SDL_Log("rendered header");
    if (goodread) {
        for (auto spot : spot_list) {
            if (spot.contains("latitude") &&
                spot["latitude"].is_number() &&
                spot.contains("longitude") &&
                spot["longitude"].is_number() &&
                spot.contains("activator") &&
                spot.contains("frequency") &&
                spot.contains("reference") &&
                spot.contains("mode") ) {

                tot++;
                // submit the map pin
//                SDL_Log("adding pin");
                struct map_pin pota_pin;

                pota_pin.owner	=		MOD_POTA;
                std::string tempstdstring	=		spot["activator"].template get<std::string>();
                sprintf(pota_pin.label, "%s", tempstdstring.c_str());

                pota_pin.lat 	= 		spot["latitude"].template get<double>();
                pota_pin.lon	=  		spot["longitude"].template get<double>();
                pota_pin.icon	=		0;
                pota_pin.color	=		pota_color;
                pota_pin.tooltip[0]=		0;
                add_pin(&pota_pin);
//                SDL_Log("pin added");
                // add to screen list
                if ((c >= pota_page[0]*9) && (c<(pota_page[0]*9)+9)) {
//                    SDL_Log("adding list");
                    std::string mode = spot["mode"].template get<std::string>();
                    std::string strfreq = spot["frequency"].template get<std::string>();
                    double freq = stod(strfreq)/1000;
                    std::string park = spot["reference"].template get<std::string>();





                    pota_color.a = 0;
                    panel.render_text(TextRect, font, pota_color, pota_pin.label);
                    TextRect.x += (panel.dims.w/4)+2;
                    sprintf(tempstr, "%4.3f", (freq));
                    panel.render_text(TextRect, font, pota_color, tempstr);
                    TextRect.x += (panel.dims.w/4)+2;
                    if (mode.size() >0) {
                        panel.render_text(TextRect, font, pota_color, mode.c_str());
                    }
                    TextRect.x += (panel.dims.w/4);
                    panel.render_text(TextRect, font, pota_color, park.c_str());
                    TextRect.x = 5;
                    TextRect.y += ((panel.dims.h/11)+(panel.dims.h/150));
                    pota_color.a = 200;
//                    SDL_Log("added list");
                } // add to list?
                c++;
            } // spot valid?
        } // foreach spot
        pota_page[1]++;
        if (pota_page[1] > 5) {
            pota_page[0]++;
            pota_page[1]=0;
        }
        if (pota_page[0] > (tot/9)) {
            pota_page[0]=0;
        }
//        SDL_Log("rendering count");
        // render the total count of POTA activators
        TextRect.w=panel.dims.w/2-10;
        TextRect.h=panel.dims.h/11;
        TextRect.x=5+(panel.dims.w/2);
        TextRect.y=2;
        pota_color.a = 0;
        sprintf(tempstr, "%i", tot);
        panel.render_text(TextRect, font, pota_color, tempstr);
//        SDL_Log("done rendering spots");
    } // good read
    // clean up
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
}


void draw_de_dx(ScreenFrame& panel, TTF_Font* font, double lat, double lon, int de_dx) {

    if (!surface) {
        SDL_Log("Missing Surface!");
        return ;
    }
    if (!panel.texture) {
        SDL_Log("Missing PANEL!");
        return ;
    }
//    SDL_Log("Drawing DE_DX");
    char tempstr[64];
    SDL_FRect TextRect;
    SDL_Color fontcolor;
    if (de_dx) {
        fontcolor.r=128;
        fontcolor.g=255;
        fontcolor.b=128;
        fontcolor.a=0;
    } else {
        fontcolor.r=255;
        fontcolor.g=128;
        fontcolor.b=128;
        fontcolor.a=0;
    }
    if (!font) {
        printf("No font defined\n");
        return;
    }
    float oldsize = TTF_GetFontSize(font);
    TTF_SetFontSize(font,72);
//    SDL_Log("Set Font");

    // blank the box
    panel.Clear();
//    SDL_SetRenderTarget(surface, panel.texture);

    time_t sunrise;
    time_t sunset;
    double solar_alt;
    // find the next zero crossing for sunrise if current alt <0

    sun_times(lat, lon, &sunrise, &sunset, &solar_alt, currenttime);
    // render the header
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h)/4;
    TextRect.w=(panel.dims.w)-4;
    struct map_pin de_dx_pin;
    if (de_dx) {
        panel.render_text(TextRect, font, fontcolor, "DE:");
        de_dx_pin.owner=MOD_DE;
        sprintf(de_dx_pin.label, "DE");

    } else {
        panel.render_text(TextRect, font, fontcolor, "DX:");
        de_dx_pin.owner=MOD_DX;
        sprintf(de_dx_pin.label, "DX");
    }
    de_dx_pin.lat=lat;
    de_dx_pin.lon=lon;
    de_dx_pin.icon=0;
    de_dx_pin.color=fontcolor;
    de_dx_pin.color.a=255;
    de_dx_pin.tooltip[0]=0;
    delete_owner_pins(de_dx_pin.owner);
    add_pin(&de_dx_pin);

    // generate maidenhead grid square

    char latstr[2];
    char lonstr[2];
    char maiden[7];
    maidenhead(lat, lon, maiden);
    // lat/lon string generation
    if (lat < 0) {
        lat *= -1;
        latstr[0]='S';
        latstr[1]=0;
    } else {
        latstr[0]='N';
        latstr[1]=0;
    }

    if (lon < 0) {
        lon *= -1;
        lonstr[0]='W';
        lonstr[1]=0;
    } else {
        lonstr[0]='E';
        lonstr[1]=0;
    }
    sprintf(tempstr, "%2.2f%s %2.2f%s", lat, latstr, lon, lonstr);
    // render lat/lon
    TextRect.x=2;
    TextRect.y=(panel.dims.h)/4;
    TextRect.h=(panel.dims.h)/4;
    TextRect.w=panel.dims.w-4;
    panel.render_text(TextRect, font, fontcolor, tempstr);

    // render maidenhead
    TextRect.x=2;
    TextRect.y=(panel.dims.h)/2;
    TextRect.h=(panel.dims.h)/4;
    TextRect.w=panel.dims.w-4;
    sprintf(tempstr, "%s", maiden);
    panel.render_text(TextRect, font, fontcolor, tempstr);

    // render sunrise time
    TextRect.x=2;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/8;
    TextRect.w=(panel.dims.w/3)-4;
    tm* test_time = localtime(&sunrise);
    strftime(tempstr, 12, "R%H:%M", test_time);
    panel.render_text(TextRect, font, fontcolor, tempstr);

    // render solar angle
    TextRect.x=(panel.dims.w/3)+4;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/8;
    TextRect.w=(panel.dims.w/3)-8;
    test_time = localtime(&sunset);
    sprintf (tempstr, "%2.2f", solar_alt);
    panel.render_text(TextRect, font, fontcolor, tempstr);

    // render sunset time
    TextRect.x=(panel.dims.w/3)*2;
    TextRect.y=((panel.dims.h)/4)*3;
    TextRect.h=(panel.dims.h)/8;
    TextRect.w=(panel.dims.w/3)-4;
    test_time = localtime(&sunset);
    strftime(tempstr, 12, "S%H:%M", test_time);
    panel.render_text(TextRect, font, fontcolor, tempstr);

    // clean up
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    TTF_SetFontSize(font,oldsize);

//    SDL_Log("Done drawing DE/DX");
}

void draw_callsign(ScreenFrame& panel, TTF_Font* font, const char* callsign) {
    if (!surface) {
        SDL_Log("Missing Surface!");
        return ;
    }
    if (!panel.texture) {
        SDL_Log("Missing PANEL!");
        return ;
    }
    panel.Clear();
//    SDL_Log("Rendering Callsign");
    SDL_Color fontcolor;
    fontcolor.r=128;
    fontcolor.g=128;
    fontcolor.b=255;
    fontcolor.a=0;
    if (!font) {
        printf("No font defined\n");
        return;
    }

    SDL_FRect TextRect;
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h)-4;
    TextRect.w=(panel.dims.w)-4;
    panel.render_text(TextRect, font, fontcolor, callsign);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
}

void load_maps() {
//    SDL_Log("Reloading Maps");
    DayMap.surface = SDL_LoadBMP("images/Blue_Marble_2002.bmp");
    if (DayMap.surface) {
        DayMap.texture = SDL_CreateTextureFromSurface(surface,DayMap.surface);
        if (!DayMap.texture) {
            SDL_Log("Unable to load DayMap Texture: %s\n", SDL_GetError());
            exit(1);
        }
    } else {
        SDL_Log("Unable to load DayMap Surface: %s\n", SDL_GetError());
        exit(1);
    }
    NightMap.surface = SDL_LoadBMP("images/Black_Marble_2016.bmp");
    if (NightMap.surface) {
        NightMap.texture = SDL_CreateTextureFromSurface(surface,NightMap.surface);
        if (!NightMap.texture) {
        SDL_Log("Unable to load NightMap Texture: %s\n", SDL_GetError());
        }
    } else {
        SDL_Log("Unable to load NightMap Surface: %s\n", SDL_GetError());
        exit(1);
    }

    if ((!DayMap.texture) || (!NightMap.texture)) {
        SDL_Log("Unable to load Maps");
    }

    SDL_SetTextureBlendMode(DayMap.texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureBlendMode(NightMap.texture, SDL_BLENDMODE_NONE);


    CountriesMap.surface = SDL_LoadBMP("images/outline.bmp");
    if (CountriesMap.surface) {
        int x, y;
        Uint8 cg, cr, cb;
        Uint8* country_pixels = (Uint8*)CountriesMap.surface->pixels;
        const Uint8 bpp = SDL_GetPixelFormatDetails(CountriesMap.surface->format)->bytes_per_pixel;

        for ( y = 0; y < CountriesMap.surface->h ; y++) {
            for (x=0 ; x < CountriesMap.surface->w ; x++) {
                int pixel_index = ( CountriesMap.surface->w * bpp * y ) + ( bpp * x );
                Uint32 *pixel_val=(Uint32*)(pixel_index+country_pixels);

                SDL_GetRGBA( *pixel_val, SDL_GetPixelFormatDetails(CountriesMap.surface->format), NULL, &cr, &cg, &cb, NULL);
                Uint32 pixel_val_out = SDL_MapRGBA(SDL_GetPixelFormatDetails(CountriesMap.surface->format), NULL, 0, 0, 0, (255-cg));
                memcpy((country_pixels + pixel_index), &pixel_val_out, bpp);
            }
        }
        CountriesMap.texture = SDL_CreateTextureFromSurface(surface,CountriesMap.surface);
        if (!CountriesMap.texture) {
                SDL_Log("Unable to load Country texture: %s\n", SDL_GetError());
                exit(1);
        }
    } else {
        SDL_Log("Unable to load Country Surface: %s\n", SDL_GetError());
        exit(1);
    }
//    SDL_Log("ALL MAPS LOADED %s\n", SDL_GetError());
    return;

}


void render_pin(ScreenFrame *panel, struct map_pin *current_pin) {

    SDL_Texture* icon_tex = SDL_CreateTexture(surface, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 16, 16);
    if (!icon_tex) {
        SDL_Log("Failed to create icon texture: %s", SDL_GetError());
        return ;
    }
    // render the icon
     SDL_SetRenderTarget(surface, icon_tex);
    if (current_pin->icon) {
        SDL_RenderTexture(surface, current_pin->icon, NULL, NULL);
    } else {
         SDL_FRect pin_rect = {4.0f, 4.0f, 8.0f, 8.0f};
         SDL_SetRenderDrawColor(surface, 16, 16, 16, 128);
         SDL_RenderFillRect(surface, NULL);
         SDL_SetRenderDrawColor(surface, current_pin->color.r, current_pin->color.g, current_pin->color.b, current_pin->color.a);
         SDL_RenderFillRect(surface, &pin_rect );
    }
    SDL_FRect target_rect;
    target_rect.h=8;
    target_rect.w=8;
    SDL_FPoint tgt_px;
    cords_to_px(current_pin->lat, current_pin->lon, (int)panel->texture->w, (int)panel->texture->h, &tgt_px);
    target_rect.x=tgt_px.x;
    target_rect.x -= (target_rect.w/2);
    target_rect.y=tgt_px.y;
    target_rect.y -= (target_rect.h/2);
    if ((target_rect.x+ target_rect.w)  > panel->dims.w) {
        (target_rect.x -= target_rect.w);
    }
    if (target_rect.x <=0) {
        target_rect.x += target_rect.w;
    }
    SDL_SetTextureBlendMode(icon_tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(surface, panel->texture);
    SDL_RenderTexture(surface, icon_tex, NULL, &target_rect);
    SDL_SetRenderTarget(surface, NULL);
    SDL_DestroyTexture(icon_tex);
//    SDL_Log("DONE RENDERING PIN");
    return;
}


void regen_mask (SDL_Surface* source, SDL_Surface* dest, const SDL_FRect& panel_dims) {
    tm* utc = gmtime(&currenttime);
    SDL_Rect panel_cords, source_cords;
    double softness = 10.0;
    double solar_decl = 23.45 * (sin( (2 * M_PI/365) * (284+(utc->tm_yday+1)) ));
    SDL_Log("Regen Terminator Alpha Mask — source: %p, dest: %p, dims: %.1fx%.1f",
    (void*)source, (void*)dest,
    panel_dims.w, panel_dims.h);

    SDL_LockMutex(night_mask_mutex);	/// MUTEX LOCK

    Uint8* alpha_pixels = (Uint8*)dest->pixels;
    Uint8* source_pixels = (Uint8*)source->pixels;
    const Uint8 dest_bpp = SDL_GetPixelFormatDetails(dest->format)->bytes_per_pixel;
    const Uint8 source_bpp = SDL_GetPixelFormatDetails(source->format)->bytes_per_pixel;

    for (panel_cords.y=0 ; panel_cords.y < floor(panel_dims.h) ; panel_cords.y++) {
    //        SDL_Log("Calculating alpha row %i of %f", panel_cords.y, panel_dims.h);
            double lat = 90.0 - (180.0 * panel_cords.y / (double)panel_dims.h);
            for (panel_cords.x=0 ; panel_cords.x < floor(panel_dims.w) ; panel_cords.x++) {
                Uint8 r, g, b;
                double lon = -180.0 + (360.0 * panel_cords.x / (double)panel_dims.w);
                double alt = solar_altitude(lat, lon, utc, solar_decl);
                // calculate per pixel alpha
                Uint8 alpha;
                if (alt > softness) {
                    alpha = 255;
                } else if (alt < -softness) {
                    alpha = 0;
                } else {
                    alpha = (Uint8)(255.0 * (alt + softness) / (2.0 * softness));
                }
                // Write a pixel with the computed alpha
                source_cords.y = (panel_cords.y/panel_dims.h)*NightMap.surface->h;
                source_cords.x = (panel_cords.x/panel_dims.w)*NightMap.surface->w;
                int source_pixel_index = ( source->w * source_bpp * source_cords.y ) + ( source_bpp * source_cords.x );
                int dest_pixel_index =   ( dest->w * dest_bpp * panel_cords.y ) + ( dest_bpp * panel_cords.x );

                Uint32 *source_pixel_val=(Uint32*)(source_pixel_index+source_pixels);
                SDL_GetRGBA( *source_pixel_val, SDL_GetPixelFormatDetails(source->format), NULL, &r, &g, &b, NULL);
                Uint32 dst_pixel_val = SDL_MapRGBA(SDL_GetPixelFormatDetails(dest->format), NULL, r, g, b, (255 - alpha));
                memcpy((alpha_pixels + dest_pixel_index), &dst_pixel_val, dest_bpp);

            }
        }

    SDL_UnlockMutex(night_mask_mutex);	/// MUTEX UNLOCK
    return;
}

Uint32 SDLCALL regen_mask (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    if (timerID) {
        struct regen_mask_args* args = (struct regen_mask_args*)userdata;

        SDL_Log("Timer Callback — source: %p, dest: %p, dims: %.1fx%.1f",
            (void*)args->source, (void*)args->dest,
            args->panel_dims.w, args->panel_dims.h);


        regen_mask (args->source, args->dest, args->panel_dims);
        return (interval);
    } else {
        return 0;
    }
}


SDL_Surface* night_mask = nullptr;
SDL_Renderer* old_renderer = nullptr;
time_t alpha_age = 0;
int draw_map(ScreenFrame& panel) {

    bool regen_mask_flag = false;
//    SDL_Log("Drawing Map ");
    if (!surface) {
        SDL_Log("Missing Surface!");
        return 1;
    }
    if (!panel.texture) {
        SDL_Log("Missing PANEL!");
        return 1;
    }

    // blank the box
    panel.Clear();

    // start with the day map
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, DayMap.texture, NULL, NULL);
//    SDL_Log("Rendered Daymap to texture");

    // init the night map alpha mask
    if (!night_mask || (old_renderer != panel.renderer)) {
        if (night_mask) {
            SDL_DestroySurface(night_mask);
        }
        night_mask = SDL_CreateSurface(panel.dims.w, panel.dims.h, SDL_PIXELFORMAT_RGBA32);
        if (!night_mask) {
            SDL_Log("Failed to create mask surface: %s", SDL_GetError());
            return 1;
        }

        night_mask_args->source = NightMap.surface;
        night_mask_args->dest = night_mask;
        night_mask_args->panel_dims = panel.dims;
        if (map_timer) {
            SDL_RemoveTimer(map_timer);
            map_timer = 0;
        }
        map_timer = SDL_AddTimer(30000, regen_mask, night_mask_args);
        old_renderer = panel.renderer;
        SDL_Log("Regen NightMask -- bad renderer");
        regen_mask_flag = true;
    }
//    if ((time(NULL) - alpha_age) > 60) {
//        regen_mask_flag = true;

//    }
//    Uint8* alpha_pixels = (Uint8*)night_mask->pixels;
//    Uint8* source_pixels = (Uint8*)NightMap.surface->pixels;
//    const Uint8 dest_bpp = SDL_GetPixelFormatDetails(night_mask->format)->bytes_per_pixel;
//    const Uint8 source_bpp = SDL_GetPixelFormatDetails(NightMap.surface->format)->bytes_per_pixel;
    if (regen_mask_flag) {
        SDL_Log("Regen NightMask");
         regen_mask (NightMap.surface, night_mask, panel.dims);
//        alpha_age=time(NULL);
        // calculate the NightMap Alpha mask

    }
    // render the masked NightMap to the panel
//    SDL_Log("render the masked NightMap to the panel");
    SDL_LockMutex(night_mask_mutex);	/// MUTEX LOCK
    SDL_Texture* mask_tex = SDL_CreateTextureFromSurface(surface, night_mask);
    SDL_UnlockMutex(night_mask_mutex);	/// MUTEX UNLOCK
//    SDL_DestroySurface(night_mask);
    if (!mask_tex) {
        SDL_Log("Failed to create mask texture: %s", SDL_GetError());
        return 1;
    }
    //set the blend mode for the alpha overlay of Night Map
    SDL_SetTextureBlendMode(mask_tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(surface, panel.texture);
    SDL_RenderTexture(surface, mask_tex, NULL, NULL);
    SDL_DestroyTexture(mask_tex);
    SDL_RenderTexture(surface, CountriesMap.texture, NULL, NULL);
    // draw equator and tropics
    SDL_SetRenderDrawColor(surface, 128,128,128,64);
    SDL_RenderLine(surface, 0,(panel.dims.h/2), panel.dims.w, (panel.dims.h/2));
    SDL_SetRenderDrawColor(surface, 128,0,0,64);
    int tropic;
    tropic = ((-23.4+90) * panel.dims.h)/180;
    SDL_RenderLine(surface, 0,tropic, panel.dims.w, tropic);
    tropic = ((23.4+90) * panel.dims.h)/180;
    SDL_RenderLine(surface, 0,tropic, panel.dims.w, tropic);
//    SDL_Log("draw map pins");
    if (map_pins) {
        struct map_pin* current_pin;
        current_pin=map_pins;
        while (current_pin) {
//            SDL_Log("rendering pin %i, %i, %i, %i for %i", current_pin->color.r, current_pin->color.g, current_pin->color.b, current_pin->color.a, current_pin->owner);
            render_pin(&panel, current_pin);
            current_pin=current_pin->next;
        }
    }


    // map the result to the window
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
//    SDL_Log("Drawing Map Complete");
    return 0;
}


int draw_clock(ScreenFrame& panel, TTF_Font* font) {

//    SDL_Log("Drawing Clock");
    SDL_Color fontcolor;
    fontcolor.r=128;
    fontcolor.g=128;
    fontcolor.b=255;
    fontcolor.a=0;
    char timestr[64];
    SDL_FRect TextRect;

    float oldsize = TTF_GetFontSize(font);
    TTF_SetFontSize(font,72);
    if (!font) {
        printf("No font defined\n");
        return 1;
    }

    // blank the box
    panel.Clear();

    // generate the time strings
     // utc
    TextRect.x=2;
    TextRect.y=2;
    TextRect.h=(panel.dims.h/5)*2;
    TextRect.w=((panel.dims.w/5)*2)-4;
    struct tm* clocktime = gmtime(&currenttime);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d", clocktime);
    panel.render_text(TextRect, font, fontcolor, timestr);
    TextRect.x=((panel.dims.w/5)*3)-4;;
    strftime(timestr, sizeof(timestr), "%H:%M:%S %Z", clocktime);
    panel.render_text(TextRect, font, fontcolor, timestr);


     // local
    TextRect.x=2;
    TextRect.y=(panel.dims.h/5)*2;
    clocktime = localtime(&currenttime);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d", clocktime);
    panel.render_text(TextRect, font, fontcolor, timestr);
    TextRect.x=((panel.dims.w/5)*3)-4;;
    strftime(timestr, sizeof(timestr), "%H:%M:%S %Z", clocktime);
    panel.render_text(TextRect, font, fontcolor, timestr);
    SDL_SetRenderTarget(surface, NULL);
    SDL_RenderTexture(surface, panel.texture, NULL, &(panel.dims));
    TTF_SetFontSize(font,oldsize);
//    SDL_Log("Done Drawing Clock");
    return 0;
}