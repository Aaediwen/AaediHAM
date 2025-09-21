#include "sun.h"
#include "../aaediclock.h"
#include "../utils.h"
#include <sstream>
#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>


void sdo_image(ScreenFrame& panel) {
    Uint32 data_size;
    time_t cache_time;
    int reload_flag =0;
    char* raw_image = 0 ;
    data_size = cache_loader(MOD_SOLAR, (void**)&raw_image, &cache_time);
    if (!data_size) {
        reload_flag=1;
    } else if ((time(NULL) - cache_time) > 7200) { // 432000
        reload_flag=1;
    }					// add valid JPEG check
//    SDL_Log ("READ %i FROM CACHE!!!!", data_size);
    bool goodread;
    goodread = true;
    if (reload_flag) {
         data_size = http_loader("https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_HMIIC.jpg", (void**)&raw_image);                           // live
         if (data_size) {
//             SDL_Log("Loaded image size: %zu bytes", data_size);
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
        struct GeoCoord subsolar_point = subsolar(time(NULL));
        solar_pin.lat=subsolar_point.latitude;
        solar_pin.lon=subsolar_point.longitude;
        solar_pin.icon = 0;
        solar_pin.color=SDL_Color{255,255,0,255};
        solar_pin.tooltip[0]=0;
        delete_owner_pins(MOD_SOLAR);
//        SDL_Log ("Read %zu bytes of data", data_size);
        try {
           SDL_IOStream *imgdata = SDL_IOFromConstMem((void*)raw_image, data_size);
           SDO_Surface = IMG_Load_IO(imgdata, true);
           if (SDO_Surface) {
               SDO_Texture = SDL_CreateTextureFromSurface(panel.GetRenderer(), SDO_Surface);
               if (SDO_Texture) {
                    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
                    SDL_RenderTexture(panel.GetRenderer(), SDO_Texture, NULL, NULL);
                    if (reload_flag || !icon_bin.get_icon(map_icons::ICON_SUN) ) {
                        SDL_SetSurfaceColorKey(SDO_Surface, 1, 0);
                        icon_bin.set_dynamic(panel.GetRenderer(), SDO_Surface, map_icons::ICON_SUN);
                    }
                    solar_pin.icon = icon_bin.get_icon(map_icons::ICON_SUN);
               } else {
                    SDL_Log ("Unable to load SDO Image Texture");
              }

           } else {
          SDL_Log ("Unable to load SDO Image Surface");
        }
        } catch (const std::exception& e){
        SDL_Log ("Error loading SDO Image  %s", e.what());
        }

        add_pin(&solar_pin);

    } else {// good read
        panel.render_text(SDL_FRect{2,2,panel.dims.w,(panel.dims.h/10)}, Sans, SDL_Color{255,0,0,0}, "NO SDO DATA");
    }
    return;
}
