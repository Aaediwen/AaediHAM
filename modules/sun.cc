#include "sun.h"
#include "../aaediclock.h"
#include "../utils.h"
#include <sstream>
#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>


void sdo_image(ScreenFrame& panel, time_t timestamp) {
    debug_log << "SOLAR: In SOLAR module\n";
    debug_log.flush();
    Uint32 data_size = 0;
    time_t cache_time;
    int reload_flag =0;
    char* raw_image = 0 ;
    if (timestamp ==0) {
        timestamp = time(NULL);
    }
    if (SDL_TryLockMutex(resize_mutex)) {
        SDL_UnlockMutex(resize_mutex);
    }
    else {
        SDL_Log("SDO Module during resize event!");
        return ;
    }
    data_size = cache_loader(MOD_SOLAR, (void**)&raw_image, &cache_time);
    if (!data_size) {
        reload_flag=1;
    } else if ((time(NULL) - cache_time) > 7200) { // 432000
        reload_flag=1;
        if (raw_image) {
        free (raw_image);
        raw_image=0;
        }
    }					// add valid JPEG check
    debug_log << "SOLAR: READ " << data_size << " FROM CACHE!!!!\n";
    debug_log.flush();
    bool goodread;
    goodread = true;
    if (reload_flag) {
        SDL_Log ("Fetching SDO image from NASA");
        debug_log << "SOLAR: Fetching SDO image from NASA\n";
         data_size = http_loader("https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_HMIIC.jpg", (void**)&raw_image);                           // live
         if (data_size) {
             debug_log << "SOLAR: Loaded image size: " << data_size << " bytes\n";
             add_data_cache(MOD_SOLAR, data_size, (void*)raw_image);
         }
    }

    if (data_size < 10) {
        goodread = false;
    }
    // clear the box
    panel.Clear();
    if (goodread) {
        SDL_Texture* SDO_Texture = nullptr;
        SDL_Surface* SDO_Surface = nullptr;
        struct map_pin solar_pin;
        solar_pin.owner=MOD_SOLAR;
        struct GeoCoord subsolar_point = subsolar(timestamp);
        solar_pin.lat = subsolar_point.latitude;
        solar_pin.lon=subsolar_point.longitude;
        solar_pin.icon = 0;
        solar_pin.color=SDL_Color{255,255,0,255};
        solar_pin.tooltip[0]=0;
        delete_owner_pins(MOD_SOLAR);
        debug_log << "SOLAR: Read " << data_size << " bytes of data\n";
        try {
           SDL_IOStream *imgdata = SDL_IOFromConstMem((void*)raw_image, data_size);
           SDO_Surface = IMG_Load_IO(imgdata, true);

           if (SDO_Surface) {
               SDO_Texture = SDL_CreateTextureFromSurface(panel.GetRenderer(), SDO_Surface);
               if (SDO_Texture) {
                    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
                    SDL_RenderTexture(panel.GetRenderer(), SDO_Texture, NULL, NULL);
                    if (reload_flag || !icon_bin.get_icon(map_icons::ICON_SUN) ) {
                        SDL_Surface* icon_surface = SDL_CreateSurface(100, 100, SDL_PIXELFORMAT_RGBA32);
                        if (SDL_BlitSurfaceScaled(SDO_Surface, NULL, icon_surface, NULL, SDL_SCALEMODE_NEAREST)) {
                            SDL_SetSurfaceColorKey(icon_surface, 1, 0);
                            icon_bin.set_dynamic(panel.GetRenderer(), icon_surface, map_icons::ICON_SUN);
                        }
                        SDL_DestroySurface(icon_surface);
                        // need to scale this down here
                    }
                    solar_pin.icon = icon_bin.get_icon(map_icons::ICON_SUN);
                    SDL_DestroyTexture(SDO_Texture);
               } else {
                    SDL_Log ("Unable to load SDO Image Texture");
                    debug_log << "SOLAR: Unable to load SDO Image Texture\n";
              }
              SDL_DestroySurface(SDO_Surface);
           } else {
          debug_log << "SOLAR: Unable to load SDO Image Surface\n";
        }
        } catch (const std::exception& e){
            SDL_Log ("Error loading SDO Image  %s", e.what());
            debug_log << "SOLAR: Error loading SDO Image  " << e.what() << "\n";
        }
        if (raw_image) {
            free (raw_image);
            raw_image=0;
        }
        add_pin(&solar_pin);

    } else {// good read
        panel.render_text(SDL_FRect{2,2,panel.dims.w,(panel.dims.h/10)}, Sans, SDL_Color{255,0,0,0}, "NO SDO DATA");
    }
    debug_log << "SOLAR: Exiting Solar module\n";
    debug_log.flush();
    return;
}
