#include "map.h"
#include "core/utils.h"
#include "utils/celestials.h"
//#include "utils/conversions.h"

ScreenFrame DayMap;
ScreenFrame NightMap;
ScreenFrame CountriesMap;
SDL_Texture* mask_tex		= nullptr;
SDL_Surface* mask_surface 	= nullptr;
SDL_FRect old_dims 		= {0.0, 0.0, 0.0, 0.0};


enum map_overlays : uint16_t {
    map 	=	1001,
    pins	=	1002
};

void load_maps(SDL_Renderer* target_renderer, const SDL_FRect size) {
    (void)size;
    debug_log << "MAP: Reloading Maps\n";
    SDL_Surface* temp_surface = nullptr;
    SDL_LockMutex(mutexes[MUTEX_NIGHT_MASK]);    /// MUTEX LOCK
    // reset the panels to make sure they are in a good state
    DayMap.Reset();
    NightMap.Reset();
    CountriesMap.Reset();

    if (!target_renderer) {
        debug_log << "MAP: load_maps called with null renderer\n";
        return;
    }
    const SDL_Rect intsize = SDL_Rect{0,0,static_cast<int>(size.w), static_cast<int>(size.h)};
    // reset all the renderers
    DayMap.SetRenderer(target_renderer);
    NightMap.SetRenderer(target_renderer);
    CountriesMap.SetRenderer(target_renderer);
    // load the day map
    temp_surface = SDL_LoadBMP("images/Blue_Marble_2002.bmp");
    if (temp_surface) {
        DayMap.surface = SDL_CreateSurface(intsize.w, intsize.h, SDL_PIXELFORMAT_RGBA32);
        if (DayMap.surface) {
            if (!SDL_BlitSurfaceScaled(temp_surface, NULL, DayMap.surface, NULL, SDL_SCALEMODE_LINEAR)) {
                debug_log << "MAP: Error scaling Day Map to DayMap surface!\n";
                SDL_DestroySurface(DayMap.surface);
                DayMap.surface = nullptr;
            }
        } else {
            debug_log << "MAP: Error Creating Day Map Surface!\n";
        }
        SDL_DestroySurface(temp_surface);
        temp_surface = nullptr;
    } else {
        debug_log << "MAP: Error Loading Day Map File!\n";
    }
    // create the DayMap Texture
    if (DayMap.surface) {
        DayMap.texture = SDL_CreateTextureFromSurface(target_renderer,DayMap.surface);
        if (!DayMap.texture) {
            debug_log << "Unable to load DayMap Texture: " << SDL_GetError() << "\n";
            SDL_DestroySurface(DayMap.surface);
        }
    }

    // load the Night Map
    temp_surface = SDL_LoadBMP("images/Black_Marble_2016.bmp");
    if (temp_surface) {
        NightMap.surface = SDL_CreateSurface(intsize.w, intsize.h, SDL_PIXELFORMAT_RGBA32);
        if (NightMap.surface) {
            if (!SDL_BlitSurfaceScaled(temp_surface, NULL, NightMap.surface, NULL, SDL_SCALEMODE_LINEAR)) {
                debug_log << "MAP: Error scaling Night Map to NightMap surface!\n";
                SDL_DestroySurface(NightMap.surface);
                NightMap.surface = nullptr;
            }
        } else {
            debug_log << "MAP: Error Creating Night Map Surface!\n";
        }
        SDL_DestroySurface(temp_surface);
        temp_surface = nullptr;
    } else {
        debug_log << "MAP: Error Loading Night Map File!\n";
    }
    SDL_UnlockMutex(mutexes[MUTEX_NIGHT_MASK]);    /// MUTEX LOCK
    // load the Countries Map
    temp_surface = SDL_LoadBMP("images/outline.bmp");
    if (temp_surface) {
        CountriesMap.surface = SDL_CreateSurface(intsize.w, intsize.h, SDL_PIXELFORMAT_RGBA32);
        if (CountriesMap.surface) {
            if (!SDL_BlitSurfaceScaled(temp_surface, NULL, CountriesMap.surface, NULL, SDL_SCALEMODE_LINEAR)) {
                debug_log << "MAP: Error scaling Countries Map to CountriesMap surface!\n";
                SDL_DestroySurface(CountriesMap.surface);
                CountriesMap.surface = nullptr;
            }
        } else {
            debug_log << "MAP: Error Creating Countries Map Surface!\n";
        }
        SDL_DestroySurface(temp_surface);
        temp_surface = nullptr;
    } else {
        debug_log << "MAP: Error Loading Countries Map File!\n";
    }

    if (CountriesMap.surface) {
        // alpha mask the political map
        int x, y;
        x=0;
        y=0;
        Uint8 cg, cr, cb;
        Uint8* country_pixels = (Uint8*)CountriesMap.surface->pixels;
        const Uint8 bpp = SDL_GetPixelFormatDetails(CountriesMap.surface->format)->bytes_per_pixel;
    /*
            about the following FOR loop:
            --------------------------------

            I am specifically leaving this manual code in here and NOT subsequently cleaning up CountriesMap.surface

            SDL_SetSurfaceColorKey() can do this kind of Alpha keying
            However, it does not obey anti-aliasing which exists on the country borders
            This code DOES work with the anti-aliasing.

            After CountriesMap.texture is created, no other code actually cares about CountriesMap.surface
            However, Other code does check for its existance as a sanity check that this routine ran correctly
            I *could* nuke the surface and create a stub 1x1 surface, but I don't think it's worth the savings in system RAM
        */
        for ( y = 0; y < CountriesMap.surface->h ; y++) {
            for (x = 0 ; x < CountriesMap.surface->w ; x++) {
                // get where the pixel lives
                int pixel_index = CountriesMap.surface->pitch*y + ( bpp * x );
                // read its color values
                Uint32 *pixel_val=(Uint32*)(pixel_index+country_pixels);
                SDL_GetRGBA( *pixel_val, SDL_GetPixelFormatDetails(CountriesMap.surface->format), NULL, &cr, &cg, &cb, NULL);
                // write the new value back
                Uint32 pixel_val_out = SDL_MapRGBA(SDL_GetPixelFormatDetails(CountriesMap.surface->format), NULL, 0, 0, 0, (255-cg));
                memcpy((country_pixels + pixel_index), &pixel_val_out, bpp);
            }
        }
        CountriesMap.texture = SDL_CreateTextureFromSurface(target_renderer,CountriesMap.surface);
        if (!CountriesMap.texture) {
            debug_log << "MAP: Unable to load Country texture: " << SDL_GetError() << "\n";

        }
    }



    // set blend modes
    SDL_SetTextureBlendMode(DayMap.texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureBlendMode(NightMap.texture, SDL_BLENDMODE_NONE);
    old_dims = size;
    return;
}


void regen_mask (SDL_Surface* source, SDL_Surface* dest, const SDL_FRect& panel_dims) {
    // sanity checks
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]) ;
    } else {
        debug_log << "MAP_REGEN: Regen Mask During Resize\n";
        return ;
    }
    if (!source) {
        debug_log << "MAP_REGEN: Mask regen with invalid source\n";
        return;
    }
    if (!dest) {
        debug_log << "MAP_REGEN: Mask regen with invalid dest\n";
        return;
    }
    //get current time
    const time_t nowtime = time(nullptr);
    debug_log << "MAP_REGEN: Regenerating Night Mask at "<< nowtime << "\n";
    struct tm utc;
#ifdef _WIN32
    gmtime_s(&utc, &nowtime);
#else
    gmtime_r(&nowtime, &utc);
#endif
    // variables
    SDL_Rect panel_cords, source_cords;

    // constants
    const double softness = 10.0;
    const double solar_decl = 23.45 * (sin( (2 * M_PI/365) * (284+(utc.tm_yday+1)) ));

    SDL_LockMutex(mutexes[MUTEX_NIGHT_MASK]);    /// MUTEX LOCK
    // collect image data
    Uint8* alpha_pixels = (Uint8*)dest->pixels;
    const Uint8* source_pixels = (Uint8*)source->pixels;
    const SDL_PixelFormatDetails* source_details = SDL_GetPixelFormatDetails(source->format);
    const SDL_PixelFormatDetails* dest_details = SDL_GetPixelFormatDetails(dest->format);
    const Uint8 dest_bpp = dest_details->bytes_per_pixel;
    const Uint8 source_bpp = source_details->bytes_per_pixel;

    // generate the night terminator alpha mask
    for (panel_cords.y=0 ; panel_cords.y < floor(panel_dims.h) ; panel_cords.y++) {
        source_cords.y = static_cast<int>((panel_cords.y/panel_dims.h)*source->h);
        double lat = 90.0 - (180.0 * panel_cords.y / (double)panel_dims.h);
        for (panel_cords.x=0 ; panel_cords.x < floor(panel_dims.w) ; panel_cords.x++) {
            Uint8 r, g, b;
            double lon = -180.0 + (360.0 * panel_cords.x / (double)panel_dims.w);
            double alt = solar_altitude(lat, lon, &utc, solar_decl);
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

            source_cords.x = static_cast<int>((panel_cords.x/panel_dims.w)*source->w);
            int source_pixel_index = ( source->w * source_bpp * source_cords.y ) + ( source_bpp * source_cords.x );
            int dest_pixel_index =   ( dest->w * dest_bpp * panel_cords.y ) + ( dest_bpp * panel_cords.x );
            Uint32 *source_pixel_val=(Uint32*)(source_pixel_index+source_pixels);
            SDL_GetRGBA( *source_pixel_val, source_details, NULL, &r, &g, &b, NULL);
            Uint32 dst_pixel_val = SDL_MapRGBA(dest_details, NULL, r, g, b, (255 - alpha));
            memcpy((alpha_pixels + dest_pixel_index), &dst_pixel_val, dest_bpp);

        }
    }
    debug_log << "MAP_REGEN: Night Mask Regen Complete\n";
    SDL_UnlockMutex(mutexes[MUTEX_NIGHT_MASK]);    /// MUTEX LOCK
    return;
}

int SDLCALL regen_mask_spawn(void* userdata) {
    struct regen_mask_args* args = (struct regen_mask_args*)userdata;
    regen_mask (args->source, args->dest, args->panel_dims);
    return 0;
}


Uint32 SDLCALL regen_mask_timer (void *userdata, SDL_TimerID timerID, Uint32 interval) {
    (void)interval;
    if (timerID) {
          SDL_Thread* thread = SDL_CreateThread(regen_mask_spawn, "Map Mask Regen", userdata);
          if (thread) {
              SDL_DetachThread(thread);
          } else {
              debug_log << "MAP: Failed to Create Map Mask Regen Thread\n";
          }

        return (30000);
    } else {
        return 0;
    }
}

struct color_pin {
    SDL_Texture* 	texture;
    SDL_Color 		color;
    Uint32		colorpack;
    SDL_Renderer*	renderer;
};

std::vector<struct color_pin> push_pins;
struct color_pin last_used_pin{};

void render_pin(ScreenFrame *panel, struct map_pin *current_pin) {
    if (!current_pin) {
        return;
    }
    const int unit_scale = static_cast<int>(panel->dims.w/100);
    SDL_Texture* old_render_target = SDL_GetRenderTarget(panel->GetRenderer());;
    SDL_FRect icon_box;		// how big and where are we going to render the icon?
    SDL_Texture* icon_texture = nullptr;	// variable to store what icon texture we are going to use

    // If conditional to find what texture to use for the icon and set icon_texture and scaling appropriately
    if (current_pin->icon) {
        // pin is asking for a specific icon graphic from the icon cache
        // scaling of 1/50 of the panel width
        icon_box.h = unit_scale * 2.0f;
        icon_box.w = unit_scale * 2.0f;
        icon_texture = current_pin->icon;
    } else {
        // pin is just accepting a generic icon
        // scaling of 1/200 of the panel width
        icon_box.h = unit_scale / 2.0f;
        icon_box.w = unit_scale / 2.0f;


        /*
         have now added new code to cache colored pins and check for using the last one.
         this will dynamically push one pin of each color to VRAM and keep it there
         See the comment below about pin lifetime to prevent memory leaks
        */


        // pack the color map to check for existing textures to match
        Uint32 current_pin_pack = (Uint32(current_pin->color.a) << 24) | (Uint32(current_pin->color.b) << 16) | (Uint32(current_pin->color.g) << 8)  | Uint32(current_pin->color.r);
        if (last_used_pin.texture && last_used_pin.colorpack == current_pin_pack && last_used_pin.renderer == panel->GetRenderer()) {
            // the texture we need is the same as the last one we used
            icon_texture = last_used_pin.texture;
        } else {
            // it's not the last one we used, check the cache
            if (!push_pins.empty()) {
                for (auto& check_pin : push_pins) {
                    if (check_pin.colorpack == current_pin_pack && check_pin.renderer == panel->GetRenderer()) {
                        // found an entry
                        icon_texture = check_pin.texture;
                        last_used_pin = check_pin;
                        break;
                    }
                }
            }
        }
        if (!icon_texture) {
            // we didn't find an existing pin texture to use. let's make one
            // build the structure to hold the pin for later
            struct color_pin new_color;
            new_color.renderer= panel->GetRenderer();
            new_color.color = current_pin->color;
            new_color.colorpack = current_pin_pack;
            new_color.texture = SDL_CreateTexture(panel->GetRenderer(), SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, unit_scale, unit_scale);
            if (!new_color.texture) {
                debug_log << "MAP: Failed to create icon texture: " <<  SDL_GetError() << "\n";
                return;
            }
            icon_texture = new_color.texture;

            // prepare to draw a new pin

            SDL_SetRenderTarget(panel->GetRenderer(), new_color.texture);

            // clear the icon
            SDL_SetRenderDrawColor(panel->GetRenderer(), 0, 0, 0, 0);
            SDL_RenderClear(panel->GetRenderer());

            // draw the shadow
            SDL_SetRenderDrawColor(panel->GetRenderer(), 16, 16, 16, 128);
            SDL_RenderFillRect(panel->GetRenderer(), NULL);

            // draw the color swatch
            const SDL_FRect pin_rect = { unit_scale/5.0f, unit_scale/5.0f, unit_scale*0.6f, unit_scale*0.6f };
            SDL_SetRenderDrawColor(panel->GetRenderer(), current_pin->color.r, current_pin->color.g, current_pin->color.b, current_pin->color.a);
            SDL_RenderFillRect(panel->GetRenderer(), &pin_rect );



            // done drawing the new pin. Now store it
            push_pins.push_back(new_color);
            last_used_pin=push_pins.back();
            /*
                Let's keep up to 128 color pins handy to use in the GPU
                On even a 16K panel, this maxes out at around 12MB of VRAM
                and only 128 or less entries to check for on a color change
            */
            if (push_pins.size() > 128) {
                SDL_DestroyTexture(push_pins.front().texture);
                push_pins.erase(push_pins.begin());
            }
        }
    }
    // we should now have the icon texture to use in SDL_Texture * icon_texture.
    // time to actually plot it on the map

    // convert the pin lat/lon to panel pixel coordinates
    SDL_FPoint tgt_px;
    cords_to_px(current_pin->lat, current_pin->lon, (int)panel->texture->w, (int)panel->texture->h, &tgt_px);
    icon_box.x=tgt_px.x;
    icon_box.x -= (icon_box.w/2);
    icon_box.y=tgt_px.y;
    icon_box.y -= (icon_box.h/2);
    // traps to avoid crossing a map boundary
    if ((icon_box.x + icon_box.w)  > panel->dims.w) {
        (icon_box.x -= icon_box.w);
    }
    if (icon_box.x <=0) {
        icon_box.x += icon_box.w;
    }
    // less likely to hit this on latitude
    if ((icon_box.y+ icon_box.h)  > panel->dims.h) {
        (icon_box.y -= icon_box.h);
    }
    if (icon_box.y <=0) {
        icon_box.y += icon_box.h;
    }

    // map the icon texture to the map
    SDL_SetTextureBlendMode(icon_texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(panel->GetRenderer(), panel->texture);
    SDL_RenderTexture(panel->GetRenderer(), icon_texture, NULL, &icon_box);

    // reset the render target
    SDL_SetRenderTarget(panel->GetRenderer(), old_render_target);

    // process the mouse event if relivant
    // this is here because it is the best place where we have the mouse target for an icon on the map
    if (clock_mouse_event.mod_owner == MOD_MAP) {
        if ( clock_mouse_event.mod_cords.y >= icon_box.y &&  clock_mouse_event.mod_cords.y <= (icon_box.y + icon_box.h)
            && clock_mouse_event.mod_cords.x >= icon_box.x &&  clock_mouse_event.mod_cords.x <= (icon_box.x + icon_box.w)   ) {
                const std::string dxstring = current_pin->label;
                clockconfig.set_DX(GeoCoord{current_pin->lat, current_pin->lon}, dxstring);
        }
    }
    return;
}


int draw_map(ScreenFrame& panel) {
// input sanity checks
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]) ;
    } else {
        debug_log << "MAP: Draw during resize event!\n";
        return 0;
    }
    if (!panel.GetRenderer()) {
        debug_log << "MAP: Map called with bad renderer\n";
        return 0;
    }
    if (!panel.texture) {
        debug_log << "MAP: Map called with bad panel texture\n";
        return 0;
    }

//    // clear the panel
//    panel.Clear();

    // verify we have maps to work with, this also handles resize events
    if ((panel.dims.w != old_dims.w) || (panel.dims.h != old_dims.h) ||
        (!NightMap.surface) || (!DayMap.surface) || (!CountriesMap.surface) ) {
        debug_log << "MAP: Regenerating Map Mask Surface\n";
        // reload maps
        load_maps(panel.GetRenderer(), panel.dims);
        SDL_LockMutex(mutexes[MUTEX_NIGHT_MASK]);    /// MUTEX LOCK
        // kill the timer
        if (map_timer) {
            SDL_RemoveTimer(map_timer);
            map_timer = 0;
        }

        // create a new surface
        if (mask_surface) {
            SDL_DestroySurface(mask_surface);
            mask_surface = nullptr;
        }
        mask_surface = SDL_CreateSurface(static_cast<int>(panel.dims.w), static_cast<int>(panel.dims.h), SDL_PIXELFORMAT_RGBA32);
        if (!mask_surface) {
            debug_log << "MAP: Failed to create mask surface: " << SDL_GetError() << "\n";
        }

        // set night mask arguments
        night_mask_args->source = NightMap.surface;
        night_mask_args->dest = mask_surface;
        night_mask_args->panel_dims = panel.dims;
        // restart the timer
        map_timer = SDL_AddTimer(30, regen_mask_timer, night_mask_args);
        SDL_UnlockMutex(mutexes[MUTEX_NIGHT_MASK]);    /// MUTEX LOCK
    }
    if ((!NightMap.surface) || (!DayMap.surface) || (!CountriesMap.surface)) {
        debug_log << "MAP: Unable to load map textures!\n";
        return 0;
    }

    // get an overlay
    SDL_FRect mapsize ;
    mapsize.w = panel.dims.w;
    mapsize.h = panel.dims.h;
    ScreenFrame* map_overlay = overlays.get_overlay(panel.GetRenderer(), map_overlays::map, mapsize);
    map_overlay->Clear(SDL_Color{0,0,0,255});
    // set the render target
    if (!SDL_SetRenderTarget(panel.GetRenderer(), map_overlay->texture)) {
//    if (!SDL_SetRenderTarget(panel.GetRenderer(), panel.texture)) {
        debug_log << "MAP: Failed to set render target: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_RenderClear(panel.GetRenderer());
    // render the Day Map
    if (DayMap.texture) {
        SDL_RenderTexture(panel.GetRenderer(), DayMap.texture, NULL, NULL);
    }

    // night mask stuff here
    // attempt to update the Night texture
    if (SDL_TryLockMutex(mutexes[MUTEX_NIGHT_MASK])) {  /// MUTEX LOCK
        if (mask_surface) {
            if (mask_tex) {
                if ((mask_tex->w != mask_surface->w) || (mask_tex->h != mask_surface->h)) {
//                  // replace the old texture
                    debug_log << "MAP: Replacing Night Mask Texture\n";
                    SDL_DestroyTexture(mask_tex);
                    mask_tex = SDL_CreateTextureFromSurface(panel.GetRenderer(), mask_surface);
                } else {
                    // sizes match, update the existing texture
                    debug_log << "MAP: Updating Existing Night Mask Texture\n";
                    // verify the forrmats match (fixes software rendering on Pi2)
                    if (mask_surface->format != mask_tex->format) {
                        SDL_Surface* old = mask_surface;
                        mask_surface = SDL_ConvertSurface(old, mask_tex->format);
                        if (mask_surface) {
                            SDL_DestroySurface(old);
                        } else {
                            mask_surface = old;
                        }
                    }
                    // update the existing texture
                    SDL_UpdateTexture(mask_tex, nullptr, mask_surface->pixels, mask_surface->pitch);
                }
            } else {
                // create new mask_tex
                debug_log << "MAP: Creating new Night Mask Texture\n";
                mask_tex = SDL_CreateTextureFromSurface(panel.GetRenderer(), mask_surface);
            }
        } else {
            debug_log << "MAP: Missing Night Mask Surface\n";
        }
        SDL_UnlockMutex(mutexes[MUTEX_NIGHT_MASK]);  /// MUTEX UNLOCK
    }
    // if we have a valid mask_texture after the above, map it
    if (mask_tex) {
        SDL_SetTextureBlendMode(mask_tex, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(panel.GetRenderer(), mask_tex, NULL, NULL);
    } else {
        debug_log << "MAP: Missing Night Mask Texture\n";
    }

    // countries map here
    if (CountriesMap.texture) {
        SDL_RenderTexture(panel.GetRenderer(), CountriesMap.texture, NULL, NULL);
    }

    // draw equator and tropics
    SDL_SetRenderDrawColor(panel.GetRenderer(), 128,128,128,64);
    SDL_RenderLine(panel.GetRenderer(), 0,(panel.dims.h/2), panel.dims.w, (panel.dims.h/2));
    SDL_SetRenderDrawColor(panel.GetRenderer(), 128,0,0,64);
    float tropic;
    tropic = ((-23.4f+90.0f) * panel.dims.h)/180.0f;
    SDL_RenderLine(panel.GetRenderer(), 0,tropic, panel.dims.w, tropic);
    tropic = ((23.4f+90.0f) * panel.dims.h)/180.0f;
    SDL_RenderLine(panel.GetRenderer(), 0,tropic, panel.dims.w, tropic);
    ScreenFrame* pin_overlay = overlays.get_overlay(panel.GetRenderer(), map_overlays::pins, mapsize);
    pin_overlay->Clear(SDL_Color{0,0,0,0});
    // set the render target
    SDL_SetRenderTarget(panel.GetRenderer(), pin_overlay->texture);

    // draw map pins
    // old pins
    if (map_pins) {
        struct map_pin* current_pin;
        current_pin=map_pins;
        while (current_pin) {
//            render_pin(&panel, current_pin);
            render_pin(pin_overlay, current_pin);
            current_pin=current_pin->next;
        }
    }
    // new plugin pins
    if (!plugin_map_pins.empty()) {
        for (auto& map_pin : plugin_map_pins) {
            render_pin(pin_overlay, &map_pin);
        }
    }
/*
    // draw map overlays
       /// start with the map itself

       /// draw the rest of the overlays, skip the map one
    overlays.reset_index();
    ScreenFrame* render_overlay = overlay;
    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
    SDL_SetTextureBlendMode(render_overlay->texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(panel.GetRenderer(), render_overlay->texture, NULL, NULL);
    while (render_overlay) {
         if (render_overlay != overlay) {
              SDL_SetTextureBlendMode(render_overlay->texture, SDL_BLENDMODE_BLEND);
              SDL_RenderTexture(panel.GetRenderer(), render_overlay->texture, NULL, NULL);
         }
         render_overlay = overlays.next_overlay();
    }
*/
    // reset the render target
    SDL_SetRenderTarget(panel.GetRenderer(), NULL);

    // reset the mouse event if needed
    if (clock_mouse_event.mod_owner == MOD_MAP) {
        clock_mouse_event.mod_owner = MOD_NULL;
    }

    return 1;
}

void draw_overlays(ScreenFrame& panel) {

// input sanity checks
    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
        SDL_UnlockMutex(mutexes[MUTEX_RESIZE]) ;
    } else {
        debug_log << "MAP: Draw during resize event!\n";
        return;
    }
    if (!panel.GetRenderer()) {
        debug_log << "MAP: Map called with bad renderer\n";
        return;
    }
    if (!panel.texture) {
        debug_log << "MAP: Map called with bad panel texture\n";
        return;
    }

    // clear the panel
    panel.Clear();
    // draw map overlays
       /// start with the map itself
    // get an overlay
    SDL_FRect mapsize ;
    mapsize.w = panel.dims.w;
    mapsize.h = panel.dims.h;
    ScreenFrame* overlay = overlays.get_overlay(panel.GetRenderer(), map_overlays::map, mapsize);
    ScreenFrame* pins_overlay = overlays.get_overlay(panel.GetRenderer(), map_overlays::pins, mapsize);
       /// draw the rest of the overlays, skip the map one
    overlays.reset_index();
    ScreenFrame* render_overlay = overlay;
    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
    SDL_SetTextureBlendMode(render_overlay->texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(panel.GetRenderer(), render_overlay->texture, NULL, NULL);
    render_overlay = pins_overlay;
    SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
    SDL_SetTextureBlendMode(render_overlay->texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(panel.GetRenderer(), render_overlay->texture, NULL, NULL);
    while (render_overlay) {
         if ((render_overlay != overlay) && (render_overlay != pins_overlay)) {
              SDL_SetTextureBlendMode(render_overlay->texture, SDL_BLENDMODE_BLEND);
              SDL_RenderTexture(panel.GetRenderer(), render_overlay->texture, NULL, NULL);
         }
         render_overlay = overlays.next_overlay();
    }

    // reset the render target
    SDL_SetRenderTarget(panel.GetRenderer(), NULL);
    return;

}