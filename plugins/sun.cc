#include "sun.h"
#include "utils/http_fetch.h"
#include "utils/celestials.h"
#include <sstream>
#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>

SDL_TimerID sdo_timer = 0;
SDL_Surface* SDO_Surface = nullptr;
aaediclock_host_api* host_api = nullptr;
uint16_t sun_tex_id = 0;
uint16_t sun_icon_id = 0;
bool refresh_icon_flag = false;

int SDLCALL  fetch_sdo (void *data) {
     (void)data;
     Uint64 data_size = 0;
     char* raw_image = 0 ;
     *(host_api->AaediHAM_LogDebug) << "Fetching SDO image from NASA --  ";
     data_size = http_loader("https://soho.nascom.nasa.gov/data/realtime/hmi_igr/1024/latest.jpg", (void**)&raw_image);                           // live
//     data_size = http_loader("https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_HMIIC.jpg", (void**)&raw_image);                           // live
     if (data_size > 10) {
     	SDL_Surface* temp;
          try {
          	if(SDO_Surface) {
               	SDL_DestroySurface(SDO_Surface);
                    SDO_Surface = nullptr;
               }

               *(host_api->AaediHAM_LogDebug) << "Loaded image size: " << data_size << " bytes\n";
               SDL_IOStream *imgdata = SDL_IOFromConstMem((void*)raw_image, static_cast<size_t>(data_size));

               temp = IMG_Load_IO(imgdata, true);
               // preconvert this in software because older versions of OpenGL can't handle it
               if (temp) {
               	SDO_Surface = SDL_ConvertSurface(temp, SDL_PIXELFORMAT_RGBA32);
                    if (SDO_Surface) {
                    	SDL_SetSurfaceColorKey(SDO_Surface, 1, 0);
                         refresh_icon_flag = true;
                    } else {
                         *(host_api->AaediHAM_LogDebug) << "Failed to convert SDL Surface for icon\n";
                    }
			} else {
				*(host_api->AaediHAM_LogDebug) << "Error Initializing SDO Icon  \n";
			}
		} catch (const std::exception& e){
			if(SDO_Surface) {
				SDL_DestroySurface(SDO_Surface);
				SDO_Surface = nullptr;
			}
			refresh_icon_flag = false;
			SDL_Log ("Error loading SDO Image  %s", e.what());
			*(host_api->AaediHAM_LogDebug) << "Error loading SDO Image  " << e.what() << "\n";
		}
		if (temp) {
			SDL_DestroySurface(temp);
		}
	} else {
		*(host_api->AaediHAM_LogDebug) << "SDO Fetch Failed\n";
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
              *(host_api->AaediHAM_LogDebug) << "Failed to Create SDO Fetch Thread\n";
          }
          return (1200000); // 2 hours
     } else {
          return 0;
     }
}

extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new sdo_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void sdo_plugin::plugin_init() const {
    if (!sdo_timer) {
        sdo_timer=SDL_AddTimer(60, fetch_sdo, NULL);
    }
    return;
}

void sdo_plugin::plugin_exit() const {
	if (sdo_timer) {
		SDL_RemoveTimer(sdo_timer);
	}
	if(SDO_Surface) {
		SDL_DestroySurface(SDO_Surface);
		SDO_Surface = nullptr;
	}
	if (host_api->AaediHAM_TextureCheck(sun_tex_id)) {
		host_api->AaediHAM_TextureDelete(sun_tex_id);
	}
	return;
}

void sdo_plugin::plugin_main(const aaediclock_FRect& dims) const {

	*(host_api->AaediHAM_LogDebug) << "In SOLAR module\n";
	time_t timestamp = time(NULL);

	// clear the box
	host_api->AaediHAM_GraphicsClear();

	struct aaediclock_map_pin solar_pin;
	sprintf(solar_pin.label, "SUB SOLAR POINT");
	solar_pin.owner=0;
	struct GeoCoord subsolar_point = subsolar(timestamp);
	solar_pin.lat = subsolar_point.latitude;
	solar_pin.lon=  subsolar_point.longitude;
	solar_pin.icon = 0;
	solar_pin.color=aaediclock_Color{255,255,0,255};
	solar_pin.tooltip[0]=0;
	host_api->AaediHAM_MapPinDelete();

	// recreate the SDO GPU texture as needed
	if (refresh_icon_flag && SDO_Surface) {
		*(host_api->AaediHAM_LogDebug) << "Loaded SDO Texture\n";
		aaediclock_image new_image;
		new_image.width = SDO_Surface->w;
		new_image.height = SDO_Surface->h;
		new_image.pixels = static_cast<uint8_t*>(SDO_Surface->pixels);
		if (host_api->AaediHAM_TextureCheck(sun_tex_id)) {
			host_api->AaediHAM_TextureUpdate(sun_tex_id, new_image);
		} else {
			sun_tex_id = host_api->AaediHAM_TextureCreate(new_image);
		}
		*(host_api->AaediHAM_LogDebug) << "Got sun texture id: "<< sun_tex_id << "\n";
		SDL_Surface* temp = SDL_ConvertSurface(SDO_Surface, SDL_PIXELFORMAT_RGBA8888);
		if (temp) {
			new_image.pixels = static_cast<uint8_t*>(temp->pixels);
			if (host_api->AaediHAM_IconCheck(sun_icon_id)) {
				host_api->AaediHAM_IconUpdate (sun_icon_id, new_image);
			} else {
				sun_icon_id = host_api->AaediHAM_IconCreate(new_image);
			}
			SDL_DestroySurface(temp);
		}
		*(host_api->AaediHAM_LogDebug) << "Got sun icon id: "<< sun_icon_id << "\n";
		refresh_icon_flag = false;
	}
	// set the pin icon
	if (host_api->AaediHAM_IconCheck(sun_icon_id)) {
		*(host_api->AaediHAM_LogDebug) << "Requesting to use sun icon id: "<< sun_icon_id << "\n";
		solar_pin.icon = sun_icon_id;
	}
	host_api->AaediHAM_MapPinAdd(solar_pin);
	// render the sun to the panel
	if (!host_api->AaediHAM_IconCheck(sun_icon_id)) {
		// for some reason we don't have a valid sun icon. attempt to refresh
		*(host_api->AaediHAM_LogDebug) << "Bad Sun Icon ID. "<< sun_icon_id<<" Attempting to regen\n";
		if (SDO_Surface) {
			SDL_Surface* temp = SDL_ConvertSurface(SDO_Surface, SDL_PIXELFORMAT_RGBA8888);
			if (temp) {
				aaediclock_image new_image;
				new_image.width = temp->w;
				new_image.height = temp->h;
				new_image.pixels = static_cast<uint8_t*>(temp->pixels);
				sun_icon_id = host_api->AaediHAM_IconCreate(new_image);
				SDL_DestroySurface(temp);
			} else {
				*(host_api->AaediHAM_LogDebug) << "Icon conversion failure\n";
			}
			*(host_api->AaediHAM_LogDebug) << "Got sun icon id: "<< sun_icon_id << "\n";
		} else {
			*(host_api->AaediHAM_LogDebug) << "NO SDO Surface\n";
		}
	}
	host_api->AaediHAM_SetTarget();
	if (host_api->AaediHAM_TextureCheck(sun_tex_id)) {
		host_api->AaediHAM_GraphicsDrawImage(sun_tex_id);
	} else {
		host_api->AaediHAM_GraphicsDrawText("NO SDO", aaediclock_Color{255,0,0,0}, aaediclock_FRect{2,2,dims.h/5,dims.w});
		host_api->AaediHAM_GraphicsDrawText("IMAGE", aaediclock_Color{255,0,0,0}, aaediclock_FRect{2,2+(dims.h/5),dims.h/5,dims.w});
    }
    *(host_api->AaediHAM_LogDebug) << "SOLAR: Exiting Solar module\n";
    return;
}

const char* sdo_plugin::getName() const {
    return "Solar Module";
}

void sdo_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}
