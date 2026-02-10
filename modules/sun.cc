#include "sun.h"
#include "aaediclock.h"
#include "core/utils.h"
#include <sstream>
#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>

SDL_TimerID sdo_timer = 0;
SDL_Texture* SDO_Texture = nullptr;


bool refresh_icon_flag = false;
int SDLCALL  fetch_sdo (void *data) {
         (void)data;
         Uint64 data_size = 0;
         char* raw_image = 0 ;
        SDL_Log ("Fetching SDO image from NASA");
        debug_log << "SOLAR: Fetching SDO image from NASA --  ";
         data_size = http_loader("https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_HMIIC.jpg", (void**)&raw_image);                           // live
         if (data_size) {
             debug_log << "SOLAR: Loaded image size: " << data_size << " bytes\n";
             add_data_cache(MOD_SOLAR, data_size, (void*)raw_image);
             refresh_icon_flag = true;
         } else {
             debug_log << "Failed\n";
             debug_log << "SDO Fetch Failed\n";
         }
         if (raw_image) {
             free(raw_image);
             raw_image = 0;
         }
         return 0;
}

Uint32 SDLCALL fetch_sdo (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)interval;
    (void)userdata;
     if (timerID) {
          SDL_Thread* thread = SDL_CreateThread(fetch_sdo, "SDO Fetcher", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              debug_log << "Failed to Create SDO Fetch Thread\n";
          }
          return (1200000); // 2 hours
     } else {
          return 0;
     }
}

void sdo_image(ScreenFrame& panel, time_t timestamp) {
    debug_log << "SOLAR: In SOLAR module\n";
    debug_log.flush();
    Uint64 data_size = 0;
    time_t cache_time;
    char* raw_image = 0 ;
    if (!sdo_timer) {
        sdo_timer=SDL_AddTimer(60, fetch_sdo, NULL);
    }
    if (timestamp ==0) {
        timestamp = time(NULL);
    }
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    }
    else {
        SDL_Log("SDO Module during resize event!");
        return ;
    }


    // clear the box
    panel.Clear();

    struct map_pin solar_pin;
    sprintf(solar_pin.label, "SUB SOLAR POINT");
    solar_pin.owner=MOD_SOLAR;
    struct GeoCoord subsolar_point = subsolar(timestamp);
    solar_pin.lat = subsolar_point.latitude;
    solar_pin.lon=subsolar_point.longitude;
    solar_pin.icon = 0;
    solar_pin.color=SDL_Color{255,255,0,255};
    solar_pin.tooltip[0]=0;
    delete_owner_pins(MOD_SOLAR);
    SDL_Surface* SDO_Surface = nullptr;
    try {
        // load the image from cache
        if (refresh_icon_flag || !icon_bin.get_icon(map_icons::ICON_SUN) ) {
            data_size = cache_loader(MOD_SOLAR, (void**)&raw_image, &cache_time);
            if (data_size > 10) {
                SDL_IOStream *imgdata = SDL_IOFromConstMem((void*)raw_image, static_cast<size_t>(data_size));
                SDL_Surface* temp = IMG_Load_IO(imgdata, true);
                // preconvert this in software because older versions of OpenGL can't handle it
                SDO_Surface = SDL_ConvertSurface(temp, SDL_PIXELFORMAT_RGBA32);
                SDL_DestroySurface(temp);
            }
        }
        // recreate the SDO GPU texture as needed
        if (refresh_icon_flag) {
            if (SDO_Texture) {
                SDL_DestroyTexture(SDO_Texture);
                SDO_Texture=0;
            }
            if (SDO_Surface) {
                SDL_Log ("Loaded SDO Texture");
                SDO_Texture = SDL_CreateTextureFromSurface(panel.GetRenderer(), SDO_Surface);
            }
        }
        // update the icon as needed
        if (refresh_icon_flag || !icon_bin.get_icon(map_icons::ICON_SUN) ) {
            if (SDO_Surface) {
                SDL_Surface* icon_surface = SDL_CreateSurface(100, 100, SDL_PIXELFORMAT_RGBA32);
                if (SDL_BlitSurfaceScaled(SDO_Surface, NULL, icon_surface, NULL, SDL_SCALEMODE_NEAREST)) {
                    SDL_SetSurfaceColorKey(icon_surface, 1, 0);
                    icon_bin.set_dynamic(panel.GetRenderer(), icon_surface, map_icons::ICON_SUN);
                }
                SDL_DestroySurface(icon_surface);
                refresh_icon_flag = false;
            }
       }
       // set the pin icon
       solar_pin.icon = icon_bin.get_icon(map_icons::ICON_SUN);
    } catch (const std::exception& e){
        SDL_Log ("Error loading SDO Image  %s", e.what());
        debug_log << "SOLAR: Error loading SDO Image  " << e.what() << "\n";
    }
    // render the sun to the panel
    if (SDO_Texture) {
        SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
        SDL_RenderTexture(panel.GetRenderer(), SDO_Texture, NULL, NULL);
    } else {
        panel.render_text(SDL_FRect{2,2,panel.dims.w,(panel.dims.h/10)}, Sans, SDL_Color{255,0,0,0}, "NO SDO DATA");
    }
    if (SDO_Surface) {
        SDL_DestroySurface(SDO_Surface);
        SDO_Surface = 0;
    }
    if (raw_image) {
        free (raw_image);
        raw_image=0;
    }
    add_pin(&solar_pin);

    debug_log << "SOLAR: Exiting Solar module\n";
    debug_log.flush();
    return;
}
