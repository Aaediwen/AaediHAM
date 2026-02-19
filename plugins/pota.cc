#include "pota.h"
#include "aaediclock.h"
//#include "core/utils.h"
#include "utils/http_fetch.h"
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
SDL_TimerID pota_timer = 0;
std::mutex pota_mutex;

aaediclock_host_api* host_api = nullptr;

struct pota_spot {
    char activator[32];
    char mode[16];
    char park[16];
    double	latitude;
    double	longitude;
    double      frequency;
};
std::vector <struct pota_spot> active_spots;

void pota_json_parser(const char* input_string) {
    if (!input_string || !(input_string[0])) {
         *(host_api->AaediHAM_LogDebug) << "POTA: NULL Json INPUT Error \n";
         return;
    }
    json spot_list;
    try {
        spot_list=json::parse(input_string);
    } catch (const json::parse_error &e) {
        (void)e;
        *(host_api->AaediHAM_LogDebug) << "POTA: Json Parse Error " << strlen(input_string) << " bytes " << input_string << "\n";
        return;
    }
    const std::lock_guard<std::mutex>pota_lock(pota_mutex);
    active_spots.clear();
    for (auto spot : spot_list) {
        if (spot.contains("latitude") &&
                spot["latitude"].is_number() &&
                spot.contains("longitude") &&
                spot["longitude"].is_number() &&
                spot.contains("activator") &&
                spot.contains("frequency") &&
                spot.contains("reference") &&
                spot.contains("mode") ) {

            struct pota_spot new_cache;
            memset(&new_cache, 0, sizeof(new_cache));
            std::string instring;
            instring 		= spot["activator"].template get<std::string>();
            strncpy(new_cache.activator, instring.c_str(), 31);
            new_cache.activator[31] = 0;
            instring		= spot["mode"].template get<std::string>();
            strncpy(new_cache.mode, instring.c_str(), 15);
            new_cache.mode[15] = 0;
            instring		= spot["reference"].template get<std::string>();
            strncpy(new_cache.park, instring.c_str(), 15);
            new_cache.park[15] = 0;
            new_cache.latitude  = spot["latitude"].template get<double>();
            new_cache.longitude = spot["longitude"].template get<double>();
            instring            = spot["frequency"].template get<std::string>();
            try {
                 new_cache.frequency = stod(instring)/1000;
            } catch (std::exception& e) {
               (void)e;
               new_cache.frequency = 0;
            }
            active_spots.push_back(new_cache);
            instring.clear();
        }
    }
    return ;
}

int SDLCALL fetch_pota (void* data) {
     (void)data;
     char* json_spots = 0 ;
     Uint64 data_size = 0;
     *(host_api->AaediHAM_LogDebug) <<"POTA: Fetching Spots from pota.app via timer\n";
     SDL_Log("Fetching Spots from pota.app via timer");
     std::string user_agent = host_api->AaediHAM_ConfigGetCall();
     user_agent += "-clock-Agent/1.0";
     data_size = http_loader("https://api.pota.app/spot/activator", (void**)&json_spots, user_agent);                           // live

     if (data_size) {
          pota_json_parser(json_spots);
          if(json_spots) {
               free (json_spots);
               json_spots=0;
          }
     }
     return 0;
}

Uint32 SDLCALL fetch_pota (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)interval;
    (void)userdata;
     if (timerID) {
          SDL_Thread* thread = SDL_CreateThread(fetch_pota, "POTA Fetcher", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              *(host_api->AaediHAM_LogDebug) << "Failed to Create POTA Fetch Thread\n";
          }
          return (300000);
     } else {
          return 0;
     }
}



int pota_page[2]={0,2};

extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new pota_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void pota_plugin::plugin_init() const {
     if (!pota_timer) {
          pota_timer = SDL_AddTimer(30, fetch_pota, NULL);
     }
    return;
}

void pota_plugin::plugin_exit() const {
    const std::lock_guard<std::mutex>pota_lock(pota_mutex);
     if (pota_timer) {
         SDL_RemoveTimer(pota_timer);
     }
    return;
}



void pota_plugin::plugin_main(const aaediclock_FRect& dims) const {

    int c, tot;
    c=0;
    tot=0;

    Uint8 pin_alpha = 192;
    char tempstr[64];
    aaediclock_FRect TextRect;

    aaediclock_Color pota_color;
    pota_color.r = 0;
    pota_color.g = 128;
    pota_color.b = 0;
    pota_color.a = 0;
    Uint64 data_size;
    int reload_flag =0;
    std::istringstream spots_raw;
    // fetch the POTA spot data
    host_api->AaediHAM_MapPinDelete();
    // convert the POTA JSON to an object
    int goodread;
    goodread = 1;

    // clear the box
    host_api->AaediHAM_GraphicsClear();
    // render the header
    TextRect.w=dims.w/2-10;
    TextRect.h=dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    host_api->AaediHAM_GraphicsDrawText("POTA ACTIVATORS", pota_color, TextRect);
    // set up for rendering the lista and submitting the pins
    TextRect.w=(dims.w/4)-(dims.w/20);
    TextRect.h=dims.h/11;
    TextRect.x=5;
    TextRect.y=((dims.h/11)+(dims.h/150));
    const std::lock_guard<std::mutex>pota_lock(pota_mutex);
    if (!active_spots.empty()) {
        struct plugin_mouse_event mouse_event = host_api->AaediHAM_GetMouseEvent();
        // we have legitimate data
        for (struct pota_spot& spot : active_spots) {
             // loop through the cached POTA spots
             struct aaediclock_map_pin pota_pin;
             pota_pin.owner  	=       0;
             int length = static_cast<int>(strlen(spot.activator));
             memset (pota_pin.label,0,16);
             if (length > 15) {
                 length = 15;
             }
             memcpy(pota_pin.label, spot.activator, length);
             pota_pin.lat    	=       spot.latitude;
             pota_pin.lon 	=       spot.longitude;
             pota_pin.icon	=       0;
             pota_color.a 	= 	pin_alpha;
             pota_pin.color  	=       pota_color;
             pota_color.a 	= 	0;
             pota_pin.tooltip[0]=       0;
             host_api->AaediHAM_MapPinAdd(pota_pin);
             if ((c >= pota_page[0]*9) && (c<(pota_page[0]*9)+9)) {
                 host_api->AaediHAM_GraphicsDrawText(pota_pin.label, pota_color, TextRect);
                 TextRect.x += (dims.w/4)+2;
                 sprintf(tempstr, "%4.3f", (spot.frequency));
                 host_api->AaediHAM_GraphicsDrawText(tempstr, pota_color, TextRect);
                 TextRect.x += (dims.w/4)+2;
                 if (spot.mode[0]) {
                    host_api->AaediHAM_GraphicsDrawText(spot.mode, pota_color, TextRect);
                 }
                 TextRect.x += (dims.w/4);
                 if (spot.park[0]) {
                    host_api->AaediHAM_GraphicsDrawText(spot.park, pota_color, TextRect);
                 }
                 TextRect.x = 5;
                 if (mouse_event.valid) {
                      if ( mouse_event.coords.y >=TextRect.y &&  mouse_event.coords.y <= (TextRect.y+TextRect.h)) {
                           const std::string dxlabel = pota_pin.label;
                           // need to add the set_DX API call
                           struct aaediclock_dx new_dx;
                           new_dx.lat = pota_pin.lat;
                           new_dx.lon = pota_pin.lon;
                           new_dx.label = dxlabel;
                           host_api->AaediHAM_ConfigSetDX(new_dx);
                      }
                 }
                 TextRect.y += ((dims.h/11)+(dims.h/150));
             }
             c++;
        }

        pota_page[0]++;
        pota_page[1]=0;
        if (pota_page[0] > (tot/9)) {
            pota_page[0]=0;
        }
        // render the total count of POTA activators
        TextRect.w=dims.w/2-10;
        TextRect.h=dims.h/11;
        TextRect.x=5+(dims.w/2);
        TextRect.y=2;
        sprintf(tempstr, "%i", active_spots.size());
        host_api->AaediHAM_GraphicsDrawText(tempstr, pota_color, TextRect);
    } else {
        // no legitimate POTA Data :(
        TextRect.w=dims.w-10;
        if (TextRect.w > 2) {
           host_api->AaediHAM_GraphicsDrawText("NO POTA DATA", pota_color, TextRect);
        }
    }
    return;
}

const char* pota_plugin::getName() const {
    return "POTA Module";
}

void pota_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

