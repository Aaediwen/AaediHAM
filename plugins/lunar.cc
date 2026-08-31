#include "lunar.h"
#include <SDL3_image/SDL_image.h>
#include <mutex>
aaediclock_host_api* host_api = nullptr;
std::mutex moon_mutex;

uint16_t moon_tex_id = 0;
uint16_t moon_icon_id = 0;


// may need to restore this function later to take a crack at non-hard coded phase angle
/*double moon_phase_angle(const time_t& t) {
    double jd =  (t / LunarConstants::SECONDS_PER_DAY) + LunarConstants::JD_UNIX_EPOCH;
    // Days since known new moon (Jan 6, 2000 18:14 UT)
    double D = jd - LunarConstants::NEW_MOON;

    // Phase as fraction of synodic month [0,1)
    double phase = fmod(D,  LunarConstants::SYNODIC_MONTH) / LunarConstants::SYNODIC_MONTH;
    if (phase < 0) phase += 1.0;

    // Convert to phase angle [0, 2π]
    return phase * 2.0 * M_PI;
}
*/

SDL_Surface* gen_moon_phase_mask(aaediclock_FRect size) {

/*
    function to generate the lunar phase mask bitmap
    according to the data in the moon_illumination global
    and SDL_FRect size
*/
    if (size.w < 1.0 || size.h < 1.0) {
         *(host_api->AaediHAM_LogDebug) << "LUNAR: Invalid MASK DIMS\n";
        return (nullptr);
    }
    SDL_Surface* result = nullptr;
    if (moon_illumination.timestamp) {
        // moon_illumination global is valid
        result = SDL_CreateSurface(static_cast<int>(size.w), static_cast<int>(size.h), SDL_PIXELFORMAT_RGBA32);
        *(host_api->AaediHAM_LogDebug) << "LUNAR: Illumination Percent\t  " << moon_illumination.fraction *100 << "\n";
        *(host_api->AaediHAM_LogDebug) << "LUNAR: Illumination Angle\t  " << moon_illumination.angle << "\n";
        *(host_api->AaediHAM_LogDebug) << "LUNAR: Timestamp\t  " << moon_illumination.timestamp << "\n";
        if (result) {
            // we were able to create the target surface
            // now to render the mask to it

            const double cx=size.w/2.0f;
            const double cy=size.h/2.0f;
            const double r = cx;
            const SDL_PixelFormatDetails* dest_details = SDL_GetPixelFormatDetails(result->format);
            const Uint8 dest_bpp = dest_details->bytes_per_pixel;
            Uint8* alpha_pixels = (Uint8*)result->pixels;
            SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);

            float x, y;
            Uint8 alpha = 0;
            Uint8 red = 0;
            // calculate the mask pixel by pixel
            for (y = 0 ; y < size.h ; y++) {
                for (x = 0 ; x < size.w ; x++) {
                    double dx = x-cx;
                    double dy = y-cy;
                    double value = sqrt((dx)*(dx) + (dy)*(dy));
                    if (value < r) {
                        // inside if the lunar disc
                        // rotate point by moon illumination angle
                        double xr = (dx * cos(moon_illumination.angle)) - (dy * sin(moon_illumination.angle));
                        double yr = (dx * sin(moon_illumination.angle)) + (dy * cos(moon_illumination.angle));
                        /*
////                        at each Y coordinate of the image,
////                        we look at the rotated chord of the moon disc as a 1D object.
////                        then we shade it based on what percent of the line is represented
////                        by our current XR position as a linear scale.
////
////                        Since we are only looking at the chord of the circle defined by YR,
////                        the percentage shifts automagically according to our Y position
////                        and generates the appropriate crecent or gibbous shape.
////
////                        cut here, means the last X coordinate on the chord
////                        to the limit of the surface X resolution that represents
////                        moon_illumination.fraction of the chord.  We set our alpha
////                        mask according to are we greater or less than this point
////                        accross the rotation surface of the moon for each pixel of the image
                        */
                        // attempting to trap negative sqrt == NaN possability
                        double arg = (r*r) - (yr * yr);
                        if ( arg <= 0.0 ) arg = 0.0;
                        double chord_half = sqrt(arg);

                        double cut = ((2.0*moon_illumination.fraction)-1.0) * chord_half;
////                        if ((moon_illumination.fraction < 0.75) && (moon_illumination.fraction > 0.25)) {
////                        if (((int)floor(x) % 10)==0) {
////                            SDL_Log("Cut at XR=CX\t %3.5f\t YR\t %3.5f\t Chord Half: %3.5f", cut, yr, chord_half);
////                            SDL_Log("DX, DY\t %5.5f, %5.5f\t XR, YR\t %5.5f, %5.5f\t R: %5.5f", dx, dy, xr, yr, r);
////                        }
////                        }
                        if (xr <= cut) {
////                            //illuminated
////                            SDL_Log("LIT PIXEL!");
                            alpha = 0;
                        } else {
////                            // shadowd
////                            //alpha = 32;
////                            SDL_Log("Shaded Pixel!");
                            alpha=196;
                        }
////
////
                    } else {
                        red = 0;
                        alpha = 255; // outside of the lunar disc
                    }
                    // copy the pixel value into place
                    int dest_pixel_index =   static_cast<int>(( result->w * dest_bpp * y ) + ( dest_bpp * x ));
                    Uint32 dst_pixel_val = SDL_MapRGBA(dest_details, NULL, red, 0, 0, (alpha));
                    memcpy((alpha_pixels + dest_pixel_index), &dst_pixel_val, dest_bpp);
                }
             }
             *(host_api->AaediHAM_LogDebug) << "LUNAR: Done Creating Moon Phase Alpha mask \n";
        } else {
             *(host_api->AaediHAM_LogDebug) << "LUNAR: Error Creating Moon Phase Alpha mask: " << SDL_GetError() << "\n";
//             SDL_Log ("No MOON MASK!");
             return (nullptr);
        }
    } else {
        // moon_illumination global is invalid
        *(host_api->AaediHAM_LogUser)  << "No Lunar timestamp! skipping mask\n";
    }
    return (result);
}


bool regen_moon_texture = false;
SDL_Surface* moon_image = nullptr;
time_t moon_surface_age = 0;
SDL_TimerID moon_timer = 0;
int moon_max_dims = 0;
int SDLCALL regen_lunar_surface(void* data) {
    (void)data;
//================================================================================
// background routine to reload the lunar images and regen the mask
//================================================================================

    //================================================================================
    // load the base moon image from disk
    //================================================================================
    float x, y;
    std::string asset_path = host_api->AaediHAM_ConfigGetAssetPath();
    asset_path += "PIA14011.jpg";

    SDL_Surface* image_load = IMG_Load(asset_path.c_str());
    SDL_Surface* image_surface = nullptr;
    const std::lock_guard<std::mutex>lunar_lock(moon_mutex);
    if (image_load) {
        if (moon_max_dims > image_load->w) {
            moon_max_dims = image_load->w;
        }
        if (moon_max_dims <=100) {
           moon_max_dims = image_load->w;
        }
        image_surface = SDL_ScaleSurface(image_load, moon_max_dims, moon_max_dims, SDL_SCALEMODE_LINEAR);
        SDL_DestroySurface(image_load);
    }

    static int bpp = 4;
    double surf_size_kb;
    double tex_size_kb;
    if (image_surface) { // able to load the image from disk
        // log the success
        x = static_cast<float>(image_surface->w);
        y = static_cast<float>(image_surface->h);
        surf_size_kb = (image_surface->pitch * image_surface->h) / 1024.0;
        tex_size_kb = (x * y * 4.0) / 1024.0; // assuming RGBA8888
        *(host_api->AaediHAM_LogDebug) << "LUNAR: Loaded Moon surface "
            << x << "x" << y << " "
            << bpp * 8 << "-bit surface ≈ " << surf_size_kb << " KB "
            << "=> GPU texture ≈ " << tex_size_kb << " KB "
            << "at " << static_cast<void*>(image_surface) << "\n";
        //================================================================================
        // re-create the cached panel moon CPU side SDL_Surface
        //================================================================================
        if (moon_image) {
            SDL_DestroySurface(moon_image);
            moon_image = 0;
        }
        moon_image = SDL_CreateSurface((image_surface->w), (image_surface->h), SDL_PIXELFORMAT_RGBA32);
        if (moon_image) { // able to re-create the moon SDL_Surface
            // log the success
            SDL_ClearSurface(moon_image, 0, 0, 0, 0);
            moon_surface_age = time(NULL);
            x = static_cast<float>(moon_image->w);
            y = static_cast<float>(moon_image->h);
            surf_size_kb = (moon_image->pitch * moon_image->h) / 1024.0;
            tex_size_kb = (x * y * 4.0) / 1024.0; // assuming RGBA8888
            *(host_api->AaediHAM_LogDebug) << "LUNAR: created moon_image surface "
                << x << "x" << y << " "
                << bpp * 8 << "-bit surface ≈ " << surf_size_kb << " KB "
                << "=> GPU texture ≈ " << tex_size_kb << " KB "
                << "at " << static_cast<void*>(image_surface) << "\n";
        }
    }
    //================================================================================
    // We have here been able to load the image from disk
    // and create the new CPU side SDL_Surface to render to
    // now we actually generate the image
    //================================================================================
    if (moon_image && image_surface) {
        // copy the loaded image to our output SDL_Surface
        if (SDL_BlitSurfaceScaled(image_surface, NULL, moon_image, NULL, SDL_SCALEMODE_LINEAR)) {
            // if successful, generate the phase mask
            aaediclock_FRect iconsize;
            iconsize.w = static_cast<float>(moon_image->w);
            iconsize.h = static_cast<float>(moon_image->h);
            *(host_api->AaediHAM_LogDebug) << "LUNAR: Generating moon mask ... ";
            SDL_Surface* moon_mask =  gen_moon_phase_mask(iconsize);
            if (moon_mask) {
                // able to create the phase mask, blit it onto our surface
                *(host_api->AaediHAM_LogDebug) << "Success\n";
                SDL_BlitSurface(moon_mask, nullptr, moon_image, nullptr);
                SDL_DestroySurface(moon_mask);
                regen_moon_texture = true;
            } else {
                // missing phase, no phase mask to show
                *(host_api->AaediHAM_LogDebug) << "We have no MOON MASK in the parent!\n";
                 *(host_api->AaediHAM_LogUser) <<"We have no MOON MASK in the parent!\n";
            }
            // set transparancy
            SDL_SetSurfaceColorKey(moon_image, 1, 0);
            regen_moon_texture = true;
        }
    } else {
        //================================================================================
        // Something went wrong loading the image from disk or creating the new SDL_Surface
        //================================================================================
        *(host_api->AaediHAM_LogDebug) << "LUNAR: Missing Moon image or image surface\n";
        if (image_surface) {
            *(host_api->AaediHAM_LogDebug) << "LUNAR: Loaded image from BMP\n";
        }
        if (moon_image) {
            SDL_DestroySurface(moon_image);
            moon_image = nullptr;
        }
    }
    // cleanup and exit
    if (image_surface) {
        *(host_api->AaediHAM_LogDebug) << "LUNAR: Cleaning up image_surface\n";
        SDL_DestroySurface(image_surface);
    }
    return 0;
}

Uint32 SDLCALL regen_lunar_surface (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)interval;
    (void)userdata;
     if (timerID) {
          SDL_Thread* thread = SDL_CreateThread(regen_lunar_surface, "Lunar Regen", nullptr);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              *(host_api->AaediHAM_LogDebug) << "LUNAR: Failed to Create Lunar Regen Thread\n";
          }
          return (30000);
     } else {
          return 0;
     }
}



extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new lunar_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void lunar_plugin::plugin_init() const {
    if (!moon_timer) {
        aaediclock_FRect map_size =  host_api->AaediHAM_GetMapSize();
        int max_w = static_cast<int>(map_size.w);
        int max_h = static_cast<int>(map_size.h);
        moon_max_dims = max_h;
        if (max_w > max_h) {
            moon_max_dims = max_h;
        }
        moon_max_dims /=4;
        moon_timer = SDL_AddTimer(10, regen_lunar_surface, NULL);
    }
    return;
}

void lunar_plugin::plugin_exit() const {
    if (moon_timer) {
        SDL_RemoveTimer(moon_timer);
    }
    if (host_api->AaediHAM_IconCheck(moon_icon_id)) {
        host_api->AaediHAM_IconDelete(moon_icon_id);
        moon_icon_id=0;
    }
    if (host_api->AaediHAM_TextureCheck(moon_tex_id)) {
        host_api->AaediHAM_TextureDelete(moon_tex_id);
        moon_tex_id = 0;
    }
    if (moon_image) {
        SDL_DestroySurface(moon_image);
        moon_image = 0;
    }
    return;
}

void lunar_plugin::plugin_main(const aaediclock_FRect& dims) const {
    // init
    aaediclock_FRect TextRect;
    const Uint64 StartTicks = SDL_GetTicks();
    *(host_api->AaediHAM_LogDebug) << "LUNAR: In Lunar Module\n";
    if (dims.h < 10 || dims.h < 10 ) {
        return;
    }
    struct plugin_mouse_event mouse_event = host_api->AaediHAM_GetMouseEvent();
    if (mouse_event.valid) {
        SDL_Log ("Click event in Lunar module at %f, %f", mouse_event.coords.x, mouse_event.coords.y);
    }
    *(host_api->AaediHAM_LogDebug) << "LUNAR: panel_dims "<< dims.w << " " << dims.h<< "\n";
    float unitx=dims.w/20;
    float unity=dims.h/20;
    *(host_api->AaediHAM_LogDebug) << "LUNAR: got units\n";
    time_t timestamp = time(NULL);
    // get the sublunar point
    struct GeoCoord sublunar_point = sublunar(timestamp);
    const std::lock_guard<std::mutex>lunar_lock(moon_mutex);
    *(host_api->AaediHAM_LogDebug) << "LUNAR: locked moon mutex in parent -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
    if ((SDL_GetTicks() - StartTicks) > 200) {
        moon_max_dims -= 10;
        if (moon_max_dims < 100) {
            moon_max_dims = 100;
        }
    }
    // check for image refresh
    if (moon_image) {
        if (regen_moon_texture) {
            if (host_api->AaediHAM_IconCheck(moon_icon_id)) {
               host_api->AaediHAM_IconDelete (moon_icon_id);
               moon_icon_id = 1024;
            }
            *(host_api->AaediHAM_LogDebug) << "Loaded Moon Texture\n";
            aaediclock_image new_image;
            new_image.width = moon_image->w;
            new_image.height = moon_image->h;
            new_image.pixels = static_cast<uint8_t*>(moon_image->pixels);
            if (host_api->AaediHAM_TextureCheck(moon_tex_id)) {
                host_api->AaediHAM_TextureUpdate(moon_tex_id, new_image);
            } else {
                moon_tex_id = host_api->AaediHAM_TextureCreate(new_image);
            }
            *(host_api->AaediHAM_LogDebug) << "Got moon texture id: "<< moon_tex_id << "\n";
            SDL_Surface* temp = SDL_ConvertSurface(moon_image, SDL_PIXELFORMAT_RGBA8888);
            if (temp) {
                new_image.pixels = static_cast<uint8_t*>(temp->pixels);
                if (host_api->AaediHAM_IconCheck(moon_icon_id)) {
                    host_api->AaediHAM_IconUpdate (moon_icon_id, new_image);
                } else {
                    moon_icon_id = host_api->AaediHAM_IconCreate(new_image);
                }
                SDL_DestroySurface(temp);
            }
            *(host_api->AaediHAM_LogDebug) << "Got moon icon id: "<< moon_icon_id << "\n";
            regen_moon_texture = false;
        }
    } else {
        if (moon_timer) {
            SDL_RemoveTimer(moon_timer);
        }
        moon_timer = SDL_AddTimer(500, regen_lunar_surface, NULL);
    }

    // draw the panel
    host_api->AaediHAM_GraphicsClear();
    aaediclock_Color lunar_text_color = aaediclock_Color{ 255,200,200,0 };
    aaediclock_Color lunar_shadow_color = aaediclock_Color{ 32,32,32,0 };
    float offsetx=unitx/10;
    float offsety=unity/10;
//    *(host_api->AaediHAM_LogDebug) << "LUNAR: apply the moon image to the panel -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
    if (host_api->AaediHAM_TextureCheck(moon_tex_id)) {
        // apply the moon image to the panel
        host_api->AaediHAM_SetTarget();
        host_api->AaediHAM_GraphicsDrawImage(moon_tex_id);
    } else {
        TextRect.x = unitx;
        TextRect.y = unity*3;
        TextRect.w = unitx*15;
        TextRect.h = unity*2;
        host_api->AaediHAM_GraphicsDrawText("MISSING MOON TEXTURE", lunar_text_color, TextRect);
    }
    // apply text overlays
    char boxtext[64];
    sprintf (boxtext, "Ill: %2.2f", (moon_illumination.fraction*100));
    TextRect.x = unitx+offsetx;
    TextRect.y = unity+offsety;
    TextRect.w = unitx*8;
    TextRect.h = unity;
    host_api->AaediHAM_GraphicsDrawText(boxtext, lunar_shadow_color, TextRect);
    TextRect.x = unitx;
    TextRect.y = unity;
    TextRect.w = unitx*8;
    TextRect.h = unity;
    host_api->AaediHAM_GraphicsDrawText(boxtext, lunar_text_color, TextRect);
    *(host_api->AaediHAM_LogDebug) << "LUNAR: " << boxtext << "\n";
    if (moon_illumination.fraction > 0.98) {
        sprintf (boxtext, "FULL");
    } else if (moon_illumination.fraction < 0.01) {
        sprintf (boxtext, "NEW");
    } else if ( moon_illumination.fraction > 0.48 && moon_illumination.fraction < 0.52) {
        sprintf (boxtext, "HALF");
    } else if (moon_illumination.fraction < 0.5) {
        if (moon_illumination.i > 0) {
            sprintf (boxtext, "WAXING CRESCENT");
        } else {
            sprintf (boxtext, "WANING CRESCENT");
        }
    } else if (moon_illumination.fraction > 0.5) {
        if (moon_illumination.i > 0) {
            sprintf (boxtext, "WAXING GIBBOUS");
        } else {
            sprintf (boxtext, "WANING GIBBOUS");
        }
    }

    TextRect.x = (unitx*10)+offsetx;
    TextRect.y = unity+offsety;
    TextRect.w = unitx*8;
    TextRect.h = unity;
    host_api->AaediHAM_GraphicsDrawText(boxtext, lunar_shadow_color, TextRect);
    TextRect.x = unitx*10;
    TextRect.y = unity;
    TextRect.w = unitx*8;
    TextRect.h = unity;
    host_api->AaediHAM_GraphicsDrawText(boxtext, lunar_text_color, TextRect);

    struct tm* clocktime = gmtime(&timestamp);
    strftime(boxtext, sizeof(boxtext), "%Y-%m-%d", clocktime);


    TextRect.x = (unitx*10)+offsetx;
    TextRect.y = (unity*19)+offsety;
    TextRect.w = unitx*8;
    TextRect.h = unity;
    host_api->AaediHAM_GraphicsDrawText(boxtext, lunar_shadow_color, TextRect);
    TextRect.x = unitx*10;
    TextRect.y = unity*19;
    TextRect.w = unitx*8;
    TextRect.h = unity;
    host_api->AaediHAM_GraphicsDrawText(boxtext, lunar_text_color, TextRect);

    // submit the map pin
    struct aaediclock_map_pin moon_pin;
    moon_pin.owner=0;
    sprintf(moon_pin.label, "SUB LUNAR POINT");
    moon_pin.lat=sublunar_point.latitude;
    moon_pin.lon=sublunar_point.longitude;
    moon_pin.color=lunar_text_color;
    moon_pin.tooltip[0]=0;
    host_api->AaediHAM_MapPinDelete();
    moon_pin.icon = 0;
    if (host_api->AaediHAM_IconCheck(moon_icon_id)) {
        moon_pin.icon = moon_icon_id;
    } else {
        if (moon_image) {
            aaediclock_image new_image;
            new_image.width = moon_image->w;
            new_image.height = moon_image->h;
            SDL_Surface* temp = SDL_ConvertSurface(moon_image, SDL_PIXELFORMAT_RGBA8888);
            if (temp) {
                new_image.pixels = static_cast<uint8_t*>(temp->pixels);
                moon_icon_id = host_api->AaediHAM_IconCreate(new_image);
                SDL_DestroySurface(temp);
            }
            *(host_api->AaediHAM_LogDebug) << "Got moon icon id: "<< moon_icon_id << "\n";

            if (moon_icon_id) {
                moon_pin.icon = moon_icon_id;
            }
        }
    }
    host_api->AaediHAM_MapPinAdd(moon_pin);
    *(host_api->AaediHAM_LogDebug) << "LUNAR: Done with Lunar Module -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
    return;
}

const char* lunar_plugin::getName() const {
    return "Lunar Module";
}

void lunar_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

