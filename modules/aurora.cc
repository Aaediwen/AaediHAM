#include "aurora.h"
#include "aaediclock.h"
#include "core/utils.h"
#include <sstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
SDL_TimerID aurora_timer = 0;
SDL_Surface* aurora_map = nullptr;
SDL_Texture* aurora_map_tex = nullptr;
bool update_map_tex_flag = false;
void aurora_json_parser(const char* input_string) {
    // sanity check the JSON input
    if (!input_string || !(input_string[0])) {
         debug_log << "AURORA: NULL Json INPUT Error \n";
         return;
    }
    // parse the JSON object
    json spot_list;
    try {
        spot_list=json::parse(input_string);
    } catch (const json::parse_error &e) {
        (void)e;
        debug_log << "AURORA: Json Parse Error " << strlen(input_string) << " bytes " << input_string << "\n";
        return;
    }

    SDL_LockMutex(mutexes[MUTEX_AURORA]);
    // recreate a new aurora map surface
    if (aurora_map) {
        SDL_DestroySurface(aurora_map);
        aurora_map = nullptr;
    }
    aurora_map = SDL_CreateSurface(360, 181, SDL_PIXELFORMAT_RGBA8888);
    // copy the OVATION JSON data to the image map
    if (spot_list.contains("coordinates")) {
         if (aurora_map) {
//             debug_log << "AURORA: Json Parse Complete \n";
             // some values for where and how to structure the data
             const SDL_PixelFormatDetails* dest_details = SDL_GetPixelFormatDetails(aurora_map->format);
             const Uint8 dest_bpp = dest_details->bytes_per_pixel;
             Uint8* alpha_pixels = (Uint8*)aurora_map->pixels;
             SDL_SetSurfaceBlendMode(aurora_map, SDL_BLENDMODE_BLEND);
//             debug_log << "AURORA: generating auroral map\n";
//             debug_log.flush();
             // itterate through the JSON here for each lat/lon coordinate
             for (auto spot : spot_list["coordinates"]) {
                // fetch the raw values from the JSON
                int latitude = spot[1].template get<int>();
                int longitude = spot[0].template get<int>();
                int aurora = spot[2].template get<int>();
//              convert latitude from geo coordinates to image Y coordinate
                latitude += 90;
                latitude = 180-latitude;
//		convert longitude from geo coordinates relative to Genwich
//		to image X position starting at antimeridian
                longitude += 180;
                if (longitude >= 360) {
                   longitude -=360;
                }
                // color coding between Green, Yellow, and Red
                Uint8 red, green;
                red = 128;
                green = 255;
                if (aurora <= 50) {
                   red += static_cast<Uint8>(128.0*(aurora/50.0)) ;
                } else {
                   red = 255;
                   green -= static_cast<Uint8>(128.0*((aurora-50)/50.0)) ;
                }
                // alpha blending
                Uint8 value = static_cast<Uint8>(sqrt(aurora / 100.0) * 128.0);
                // copy the pixel value into place
                int dest_pixel_index =  latitude * aurora_map->pitch ;
                dest_pixel_index += longitude * dest_bpp;
                Uint32 dst_pixel_val = SDL_MapRGBA(dest_details, NULL, red, green, 0, (value));
                memcpy((alpha_pixels + dest_pixel_index), &dst_pixel_val, dest_bpp);
//                debug_log << "AURORA: Longitude: "<< longitude << "\t Latitude" << latitude << "\t Value" << value << " \n";
//                debug_log.flush();
             }
             debug_log << "AURORA: Map Rebuilt \n";
         }
    }
    update_map_tex_flag = true;
    SDL_UnlockMutex(mutexes[MUTEX_AURORA]);
    return;
}


int SDLCALL fetch_aurora (void* data) {
     (void)data;
     char* aurora_data = 0 ;
     Uint64 data_size = 0;
     debug_log <<"AURORA: Data from NOAA via timer\n";
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

Uint32 SDLCALL fetch_aurora (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)interval;
    (void)userdata;
     if (timerID) {
          SDL_Thread* thread = SDL_CreateThread(fetch_aurora, "AURORA Fetcher", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              debug_log << "Failed to Create AURORA Fetch Thread\n";
          }
          return (300000);
     } else {
          return 0;
     }
}



void aurora_spots(ScreenFrame& panel) {
     if (!aurora_timer) {
          aurora_timer = SDL_AddTimer(30, fetch_aurora, NULL);
     }
     if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
         SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
     }
     else {
         SDL_Log("AURORA DRAW during resize event!");
         return;
     }
    if (!panel.GetRenderer()) {
        debug_log << "AURORA: Missing Renderer!\n";
        return;
    }
    // get a map overlay to use
    ScreenFrame* overlay = overlays.get_overlay(panel.GetRenderer(), MOD_AURORA, panel.dims);
    overlay->Clear(SDL_Color{0,0,0,0});
    SDL_LockMutex(mutexes[MUTEX_AURORA]);
    // if we currently have auroral data
    if (aurora_map) {
        // and we need to update the texture
        if (update_map_tex_flag) {
            // blow out the old texture and re-create from aurora_map
            if (aurora_map_tex) {
                SDL_DestroyTexture(aurora_map_tex);
                aurora_map_tex = nullptr;
            }
            aurora_map_tex = SDL_CreateTextureFromSurface(panel.GetRenderer(), aurora_map);
            update_map_tex_flag = false;
        }
//        debug_log << "AURORA: Copying Map to overlay \n";
        if (aurora_map_tex) {
            SDL_SetRenderTarget(panel.GetRenderer(), overlay->texture);
            SDL_RenderTexture(panel.GetRenderer(), aurora_map_tex, nullptr, nullptr);
            SDL_SetRenderTarget(panel.GetRenderer(), nullptr);
        }
//        debug_log.flush();

//        debug_log << "AURORA: Overlay Updated \n";
        debug_log.flush();
    } else {
        debug_log << "AURORA Missing Aurora Map (" << aurora_map << ") or Overlay Surface ( "<<overlay->surface<<")\n";
    }
    SDL_UnlockMutex(mutexes[MUTEX_AURORA]);
    // reset the mouse event
    if (clock_mouse_event.mod_owner == MOD_AURORA) {
     clock_mouse_event.mod_owner = MOD_NULL;
    }
//    debug_log << "AURORA: Complete\n";
    debug_log.flush();
    return;
}
