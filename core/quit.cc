#include "aaediclock.h"
#include "core/core.h"
//#include "modules/pskreporter.h"
#include "sdl_callbacks.h"

int window_destroy() {
	debug_log << "EXIT: Exiting Normally.\n\n";
	SDL_Quit();
	return 0;
}

    /* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
	(void)appstate;
	(void)result;
	debug_log << "EXIT: Killing System Timers.\n\n";
	if (flag_timer) {
		SDL_RemoveTimer(flag_timer);
		flag_timer = 0;
	}
	    /*    if (map_timer) {
	    	SDL_RemoveTimer(map_timer);
	            map_timer = 0;
		}
	        free (night_mask_args);
	        night_mask_args=nullptr;
	    */
	debug_log << "EXIT: Disabling Draw flags and panels.\n\n";
	for (struct PluginModule& plugin : loaded_plugins ) {
		plugin.draw_flag = false;
		//plugin.host_api->panel = nullptr;
		unregister_module(&plugin);
	}

	debug_log << "EXIT: Cleaning Mutexes.\n\n";
	for (SDL_Mutex*& mtx : mutexes) {
		if (mtx) {
			SDL_LockMutex(mtx);
			SDL_UnlockMutex(mtx);
			SDL_DestroyMutex(mtx);
			mtx = nullptr;
		}
	}
	debug_log << "EXIT: Cleaning SDL Panels.\n\n";
	overlays.clear();
	winboxes[PANEL_CALLSIGN].panel.Reset();
	winboxes[PANEL_NULL].panel.Reset();
	winboxes[PANEL_DE].panel.Reset();
	winboxes[PANEL_DX].panel.Reset();
	winboxes[PANEL_CLOCK].panel.Reset();
	winboxes[PANEL_FLEXBOX1].panel.Reset();
	winboxes[PANEL_FLEXBOX2].panel.Reset();
	winboxes[PANEL_FLEXBOX3].panel.Reset();
	winboxes[PANEL_FLEXBOX4].panel.Reset();
	winboxes[PANEL_FLEXBOX5].panel.Reset();
	winboxes[PANEL_MAP].panel.Reset();
	debug_log << "EXIT: PSKreporter Cleanup.\n\n";
	//    psk_cleanup();
	debug_log << "EXIT: Destroying Window.\n\n";
	debug_log.flush();
	/* SDL will clean up the window/renderer for us. */
}

