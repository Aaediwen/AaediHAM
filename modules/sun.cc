#include "sun.h"
#include "../aaediclock.h"
#include "../utils.h"
#include <sstream>
#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>

SDL_TimerID sdo_timer = 0;
SDL_Mutex *sdo_mutex =0;
SDL_Texture* SDO_Texture = nullptr;
SDL_Surface* SDO_Surface = nullptr;

bool refresh_icon_flag = false;
void fetch_sdo () {
         Uint32 data_size = 0;
         char* raw_image = 0 ;
        SDL_Log ("Fetching SDO image from NASA");
        debug_log << "SOLAR: Fetching SDO image from NASA --  ";
         data_size = http_loader("https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_HMIIC.jpg", (void**)&raw_image);                           // live
         if (data_size) {
             debug_log << "SOLAR: Loaded image size: " << data_size << " bytes\n";
             add_data_cache(MOD_SOLAR, data_size, (void*)raw_image);
             SDL_LockMutex(sdo_mutex);
             SDL_IOStream *imgdata = SDL_IOFromConstMem((void*)raw_image, data_size);
             SDO_Surface = IMG_Load_IO(imgdata, true);
             if (SDO_Surface) {
                 refresh_icon_flag = true;
             }
             SDL_UnlockMutex(sdo_mutex);

         } else {
             debug_log << "Failed\n";
             debug_log << "SDO Fetch Failed\n";
         }
}

Uint32 SDLCALL fetch_sdo (void *userdata, SDL_TimerID timerID, Uint32 interval) {
     if (timerID) {
          fetch_sdo();
          return (1200000); // 2 hours
     } else {
          return 0;
     }
}

void sdo_image(ScreenFrame& panel, time_t timestamp) {
    debug_log << "SOLAR: In SOLAR module\n";
    debug_log.flush();
    Uint32 data_size = 0;
    time_t cache_time;
    if (!sdo_mutex) {
        sdo_mutex=SDL_CreateMutex();
    }
//    int reload_flag =0;
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

/*    if (!data_size) {
        reload_flag=1;
    } else if ((time(NULL) - cache_time) > 7200) { // 432000
        reload_flag=1;
        if (raw_image) {
        free (raw_image);
        raw_image=0;
        }
    }					// add valid JPEG check */
/*    debug_log << "SOLAR: READ " << data_size << " FROM CACHE!!!!\n";
    debug_log.flush();
    bool goodread;
    goodread = true;

    if (data_size < 10) {
        goodread = false;
        if (raw_image) {
            free(raw_image);
        }
        raw_image = 0;
    }
    */
    // clear the box
    panel.Clear();
//    if (goodread) {

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
        debug_log << "SOLAR: Read " << data_size << " bytes of data\n";
        SDL_LockMutex(sdo_mutex);
        try {
            // recreate the SDO GPU texture as needed
           if (refresh_icon_flag && SDO_Surface) {
               if (SDO_Texture) {
                   SDL_DestroyTexture(SDO_Texture);
                   SDO_Texture=0;
               }
               SDO_Texture = SDL_CreateTextureFromSurface(panel.GetRenderer(), SDO_Surface);
           }
           // render the sun to the panel
           if (SDO_Texture) {
               SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
               SDL_RenderTexture(panel.GetRenderer(), SDO_Texture, NULL, NULL);
           } else {
               panel.render_text(SDL_FRect{2,2,panel.dims.w,(panel.dims.h/10)}, Sans, SDL_Color{255,0,0,0}, "NO SDO DATA");
           }
           // update the icon as needed
           if (refresh_icon_flag || !icon_bin.get_icon(map_icons::ICON_SUN) ) {
                if (!SDO_Surface) {
                    debug_log << "SOLAR: Refreshing sun icon\n";
                    data_size = cache_loader(MOD_SOLAR, (void**)&raw_image, &cache_time);
                    if (data_size > 10) {
                        SDL_IOStream *imgdata = SDL_IOFromConstMem((void*)raw_image, data_size);
                        SDO_Surface = IMG_Load_IO(imgdata, true);
                    }
                }

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
        if (SDO_Surface) {
            SDL_DestroySurface(SDO_Surface);
            SDO_Surface = 0;
        }
        SDL_UnlockMutex(sdo_mutex);
        if (raw_image) {
            free (raw_image);
            raw_image=0;
        }
        add_pin(&solar_pin);

    debug_log << "SOLAR: Exiting Solar module\n";
    debug_log.flush();
    return;
}
