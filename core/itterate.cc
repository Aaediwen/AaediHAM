#include "aaediclock.h"
#include "core/core.h"
#include "core/quit.h"
#include "core/event.h"
#include "core/user_log.h"
#include "utils/conversions.h"
#include <SDL3_image/SDL_image.h>
#include "sdl_callbacks.h"



struct color_pin {
	SDL_Texture*        texture;
	SDL_Color           color;
	Uint32              colorpack;
	SDL_Renderer*       renderer;
};

std::vector<struct color_pin> push_pins;
struct color_pin last_used_pin{};
int map_owner_id = 0;
std::atomic<bool> write_image{false};

void render_pin(ScreenFrame *panel, struct map_pin *current_pin) {
	if (!current_pin) {
		return;
	}
	const int unit_scale = static_cast<int>(panel->dims.w/100);
	SDL_Texture* old_render_target = SDL_GetRenderTarget(panel->GetRenderer());
	SDL_FRect icon_box;         // how big and where are we going to render the icon?
	SDL_Texture* icon_texture = nullptr;        // variable to store what icon texture we are going to use
	
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
	aaediclock_FPoint tgt_px;
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
	if (map_owner_id == clock_mouse_event.plugin_owner) {
		if ( clock_mouse_event.mod_cords.y >= icon_box.y &&  clock_mouse_event.mod_cords.y <= (icon_box.y + icon_box.h)
			&& clock_mouse_event.mod_cords.x >= icon_box.x &&  clock_mouse_event.mod_cords.x <= (icon_box.x + icon_box.w)   ) {
			const std::string dxstring = current_pin->label;
			clockconfig.set_DX(GeoCoord{current_pin->lat, current_pin->lon}, dxstring);
		}
	}
    return;
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
	overlays.reset_index();
	   /// draw the rest of the overlays, skip the map one
	for (uint8_t layer = 0 ; layer < 3 ; layer++) {
		ScreenFrame* render_overlay;
		render_overlay = overlays.next_overlay(layer);
		while (render_overlay) {
			SDL_SetTextureBlendMode(render_overlay->texture, SDL_BLENDMODE_BLEND);
			SDL_RenderTexture(panel.GetRenderer(), render_overlay->texture, NULL, NULL);
			render_overlay = overlays.next_overlay(layer);
		}
	}
	// reset the render target
	SDL_SetRenderTarget(panel.GetRenderer(), NULL);
	return;
}

std::string tempfile;
SDL_Surface* savesurface = nullptr;
std::mutex savemutex;
int SDLCALL write_image_thread (void* data) {
	(void)data;
	Uint64 savestart = SDL_GetTicks();
	if (tempfile.empty()) {
		tempfile = outfile + ".tmp";
	}
	const std::lock_guard<std::mutex>save_lock(savemutex);
	if (savesurface) {
		IMG_SaveJPG(savesurface, tempfile.c_str(), 50);
	#ifdef _WIN32
		MoveFileExA(tempfile.c_str(), outfile.c_str(), MOVEFILE_REPLACE_EXISTING);
	#else
		rename(tempfile.c_str(), outfile.c_str());
	#endif
		//  SDL_SaveBMP(savesurface, outfile.c_str());  // output_file_path from --output
		SDL_DestroySurface(savesurface);
		savesurface = nullptr;
		std::cout << "JPG_Write: " << std::to_string(SDL_GetTicks() - savestart) << " Ms\n";
	}
	return 0;
}

SDL_Thread* save_thread = nullptr;
std::atomic<bool> resizing {false};

SDL_AppResult SDL_AppIterate(void *appstate) {
	(void)appstate;
	SDL_Delay(10);                      // slow down the program
#ifdef _WIN32
#ifdef _DEBUG
	_ASSERTE(_CrtCheckMemory());
#endif
#endif
	if (interrupt_flag) {
		window_destroy();
		return(SDL_APP_SUCCESS);
	}
	if (reload_flag) {
		config_reload();
		reload_flag = false;
	}
	//mutex_checker();
	if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
		SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
	}
	else {
		SDL_Log("Itterate during resize event!");
		return (SDL_APP_CONTINUE);
	}
	if (!resizing) {
		const Uint64 StartTicks = SDL_GetTicks();	// timer for how long this has taken
		SDL_LockMutex(mutexes[MUTEX_MASTER_CLOCK]);
		
		if (!loaded_plugins.empty()) {
			for (struct PluginModule& plugin : loaded_plugins ) {
				if (plugin.draw_flag) {
					debug_log << "ITTERATE: Calling "<< plugin.name 
						<<" with panel " << plugin.host_api->panel << "\n";
					try {
						aaediclock_FRect module_dims;
						if (plugin.host_api->panel) {
							module_dims.w = plugin.host_api->panel->dims.w;
							module_dims.h = plugin.host_api->panel->dims.h;
							module_dims.x = plugin.host_api->panel->dims.x;
							module_dims.y = plugin.host_api->panel->dims.y;
							plugin.plugin->plugin_main(module_dims);
						}
					} catch (std::exception& e) {
						debug_log << "Module Exception in "<< plugin.name << ": "<< e.what() << "\n";
					} catch (...) {
						debug_log << "Unknown Exception in "<< plugin.name << "\n";
					}
					SDL_SetRenderTarget(clock_renderer, NULL);
					debug_log << "ITTERATE: Module "<< plugin.name<<" -- " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
					debug_log.flush();
					plugin.draw_flag = false;
				}
			}
		}
		winboxes[PANEL_MAP].panel.draw_border();
		//        winboxes[PANEL_CALLSIGN].panel.draw_border();
		//        winboxes[PANEL_CLOCK].panel.draw_border();
		//        winboxes[PANEL_MAP].panel.draw_border();
		//        winboxes[PANEL_DE].panel.draw_border();
		//        winboxes[PANEL_DX].panel.draw_border();
		//        winboxes[PANEL_FLEXBOX1].panel.draw_border();
		//        winboxes[PANEL_FLEXBOX2].panel.draw_border();
		//        winboxes[PANEL_FLEXBOX3].panel.draw_border();
		//        winboxes[PANEL_FLEXBOX4].panel.draw_border();
		//        winboxes[PANEL_FLEXBOX5].panel.draw_border();



		// new plugin pins
		ScreenFrame* pin_overlay = overlays.get_overlay(clock_renderer, 1002,  winboxes[PANEL_MAP].panel.dims, OVERLAY_BASE);
		if (pin_overlay) {
			pin_overlay->Clear(SDL_Color{0,0,0,0});
			if (!winboxes[PANEL_MAP].plugin_sequence.empty()) {
				map_owner_id = winboxes[PANEL_MAP].plugin_sequence[winboxes[PANEL_MAP].plugin_index];
				if (!plugin_map_pins.empty()) {
					for (auto& map_pin : plugin_map_pins) {
						render_pin(pin_overlay, &map_pin);
					}
				}
			}
		}
		if (map_owner_id == clock_mouse_event.plugin_owner) {
			clock_mouse_event.plugin_owner = -1;
		}
		draw_overlays(winboxes[PANEL_MAP].panel);
		
		winboxes[PANEL_CALLSIGN].panel.present();
		winboxes[PANEL_CLOCK].panel.present();
		winboxes[PANEL_MAP].panel.present();
		winboxes[PANEL_DE].panel.present();
		winboxes[PANEL_DX].panel.present();
		winboxes[PANEL_FLEXBOX1].panel.present();
		winboxes[PANEL_FLEXBOX2].panel.present();
		winboxes[PANEL_FLEXBOX3].panel.present();
		winboxes[PANEL_FLEXBOX4].panel.present();
		winboxes[PANEL_FLEXBOX5].panel.present();
		SDL_UnlockMutex(mutexes[MUTEX_MASTER_CLOCK]);
		SDL_RenderPresent(clock_renderer);

		//if (headless && (!outfile.empty())) {
		// this if (write_surface && savesurface) acts to drop disk frames if the system is having trouble keeping up with disk output
		if (write_image && savesurface) {
		//  std::cout << "JPEG Skipping Frame\n";
			write_image = false;
		}
		if (write_image) {
			if (!outfile.empty()) {
			// dump surface to image file here
			// this new code replaces the below by spawning a worker thread for the disk write
			// doing so avoids blocking the main render thread
			
			int width, height;
			SDL_GetCurrentRenderOutputSize(clock_renderer, &width, &height);
			if (save_thread) {
				SDL_WaitThread(save_thread, nullptr);
				save_thread = nullptr;
			}
			const std::lock_guard<std::mutex>save_lock(savemutex);
			savesurface = SDL_RenderReadPixels(clock_renderer, NULL);
			if (savesurface) {
				save_thread = SDL_CreateThread(write_image_thread, "Disk Writer", (void*)savesurface);
			}
			
			    // dump surface to image file here
		/*      if (tempfile.empty()) {
		            tempfile = outfile + ".tmp";
		        }
		        int width, height;
		        SDL_GetCurrentRenderOutputSize(clock_renderer, &width, &height);
		        SDL_Surface* savesurface = SDL_RenderReadPixels(clock_renderer, NULL);
		        IMG_SaveJPG(savesurface, tempfile.c_str(), 75);
		        #ifdef _WIN32
		        MoveFileExA(tempfile.c_str(), outfile.c_str(), MOVEFILE_REPLACE_EXISTING);
		        #else
		        rename(tempfile.c_str(), outfile.c_str());
		        #endif
		//    	SDL_SaveBMP(savesurface, outfile.c_str());  // output_file_path from --output
		        SDL_DestroySurface(savesurface); */
			}
			write_image = false;
		}
		debug_log << "ITTERATE: Took " << (SDL_GetTicks() - StartTicks) << " MIlliseconds\n";
	}
	return(SDL_APP_CONTINUE);
}

