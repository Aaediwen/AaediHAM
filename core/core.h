#pragma once

extern SDL_Window* window;		// main window		--	set in init, used to create renderer in resize
extern SDL_TimerID flag_timer;		// main program timer	--	used in panel, set in init and panel
extern bool reload_flag;		// resize stress timer flag --	set in events, used in panel
extern std::string outfile;		// output file name	-- 	set in init, used in itterate
extern bool resizing;			// active resizing flag	-- 	set in event, used in itterate


