#include "aurora.h"
#include "utils/http_fetch.h"
#include <sstream>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

SDL_TimerID aurora_timer = 0;
uint8_t* aurora_map = nullptr;
bool update_map_tex_flag = false;
std::mutex aurora_mutex;
aaediclock_host_api* host_api = nullptr;

void aurora_json_parser(const char* input_string) {
    // sanity check the JSON input
    if (!input_string || !(input_string[0])) {
         *(host_api->AaediHAM_LogDebug) << "NULL Json INPUT Error \n";
         return;
    }
    // parse the JSON object
    json spot_list;
    try {
        spot_list=json::parse(input_string);
    } catch (const json::parse_error &e) {
        (void)e;
        *(host_api->AaediHAM_LogDebug) << "Json Parse Error " << strlen(input_string) << " bytes " << input_string << "\n";
        return;
    }

    *(host_api->AaediHAM_LogDebug) << "Locking to parse Aurora data\n";
    const std::lock_guard<std::mutex>aurora_lock(aurora_mutex);
    // recreate a new aurora map surface
    if (!aurora_map) {
        aurora_map = static_cast<uint8_t*>(malloc(360*181*4));
        if (aurora_map) {
            memset(aurora_map, 255, 360*181*4);
        }
    }

    // copy the OVATION JSON data to the image map
    if (spot_list.contains("coordinates") && aurora_map) {
        memset(aurora_map, 0, 360*181*4);
        // some values for where and how to structure the data
        const int dest_bpp = 4;
        const int stride = 360 * 4;
        uint8_t* alpha_pixels = aurora_map;
        *(host_api->AaediHAM_LogDebug) << "generating auroral map\n";
        // itterate through the JSON here for each lat/lon coordinate
        for (auto spot : spot_list["coordinates"]) {
           // fetch the raw values from the JSON
           int latitude = spot[1].template get<int>();
           int longitude = spot[0].template get<int>();
           int aurora = spot[2].template get<int>();
//         convert latitude from geo coordinates to image Y coordinate
           latitude += 90;
           latitude = 180-latitude;
//	convert longitude from geo coordinates relative to Genwich
//	to image X position starting at antimeridian
           longitude += 180;
           if (longitude >= 360) {
              longitude -=360;
           }
           // color coding between Green, Yellow, and Red
           uint8_t red, green;
           red = 128;
           green = 255;
           if (aurora <= 50) {
              red += static_cast<uint8_t>(128.0*(aurora/50.0)) ;
           } else {
              red = 255;
              green -= static_cast<uint8_t>(128.0*((aurora-50)/50.0)) ;
           }
           // alpha blending
           uint8_t value = static_cast<uint8_t>(sqrt(aurora / 100.0) * 128.0);
           // copy the pixel value into place
           int dest_pixel_index =  latitude * stride ;
           dest_pixel_index += longitude * dest_bpp;
           *(alpha_pixels + dest_pixel_index) = red;
           *(alpha_pixels + dest_pixel_index+1) = green;
           *(alpha_pixels + dest_pixel_index+2) = 0;
           *(alpha_pixels + dest_pixel_index+3) = value;
        }
        *(host_api->AaediHAM_LogDebug) << "AURORA: Map Rebuilt \n";
    }
    update_map_tex_flag = true;
    return;
}


static int SDLCALL fetch_aurora (void* data) {
     (void)data;
     std::cout << "THREAD fetch_aurora entered\n";
     std::cout.flush();

     char* aurora_data = 0 ;
     Uint64 data_size = 0;
     *(host_api->AaediHAM_LogDebug) <<"AURORA: Data from NOAA via timer\n";
     data_size = http_loader("https://services.swpc.noaa.gov/json/ovation_aurora_latest.json", (void**)&aurora_data);                           // live

     if (data_size) {
          aurora_json_parser(aurora_data);
          SDL_Log("finished aurora parser AURORA");
          if(aurora_data) {
               free (aurora_data);
               aurora_data=0;
          }
     }
     return 0;
}

static Uint32 SDLCALL fetch_aurora (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)interval;
    (void)userdata;
     if (timerID) {
          SDL_Thread* thread = SDL_CreateThread(fetch_aurora, "AURORA Fetcher", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              *(host_api->AaediHAM_LogDebug) << "Failed to Create AURORA Fetch Thread\n";
          }
          return (300000);
     } else {
          return 0;
     }
}






extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new aurora_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void aurora_plugin::plugin_init() const {
    aurora_timer = SDL_AddTimer(3000, fetch_aurora, NULL);
    return;
}

void aurora_plugin::plugin_exit() const {
    const std::lock_guard<std::mutex>aurora_lock(aurora_mutex);
    if (aurora_timer) {
        SDL_RemoveTimer(aurora_timer);
    }
    if (aurora_map) {
       free(aurora_map);
    }
    return;
}
uint16_t aurora_tex_id = 0;
void aurora_plugin::plugin_main(const aaediclock_FRect& dims) const {
(void)dims;

    aaediclock_FRect map_dims = host_api->AaediHAM_GetMapSize();
    // if we currently have auroral data
    if (!host_api->AaediHAM_OverlayCheck() && aurora_map) {
        update_map_tex_flag = true;
    }
    if (update_map_tex_flag) {
        *(host_api->AaediHAM_LogDebug) << "Update flag is set\n";

        if (aurora_map) {
            const std::lock_guard<std::mutex>aurora_lock(aurora_mutex);
            *(host_api->AaediHAM_LogDebug) << "Locked to Update Aurora\n";
            *(host_api->AaediHAM_LogDebug) << "Sending Aurora Map texture (" << aurora_map << ") to host\n";
            aaediclock_image new_image;
            new_image.width = 360;
            new_image.height = 181;
            new_image.pixels = aurora_map;
            if (aurora_tex_id) {
               host_api->AaediHAM_TextureUpdate(aurora_tex_id, new_image);
            } else {
               aurora_tex_id = host_api->AaediHAM_TextureCreate(new_image);
            }
        } else {
            *(host_api->AaediHAM_LogDebug) << "Missing Aurora Map (" << aurora_map << ")\n";
        }
    }
    if (aurora_tex_id) {
         host_api->AaediHAM_OverlaySet(map_dims, OVERLAY_BASE);
         host_api->AaediHAM_OverlayClear(aaediclock_Color{0,0,0,0});
         host_api->AaediHAM_GraphicsDrawImage(aurora_tex_id);
    }
    // reset the mouse event
    struct plugin_mouse_event mouse_event = host_api->AaediHAM_GetMouseEvent();
    *(host_api->AaediHAM_LogDebug) << "AURORA: Complete\n";
    return;
}

const char* aurora_plugin::getName() const {
    return "Aurora Module";
}

void aurora_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

