#pragma once

struct ModuleControl {
    bool draw_flag = true;
    ScreenFrame* panel = &winboxes[PANEL_NULL].panel;
};

/*
struct ModuleList {
    ModuleControl sat_tracker;
    ModuleControl dx_spots;
    ModuleControl callsign;
    ModuleControl de;
    ModuleControl dx;
    ModuleControl pota;
    ModuleControl ncdxf;
    ModuleControl map;
    ModuleControl clock;
    ModuleControl kindex;
    ModuleControl solar;
    ModuleControl wspr;
    ModuleControl lunar;
    ModuleControl psk;
    ModuleControl contests;
    ModuleControl rss;
    ModuleControl aurora;
} extern master_flags;			// set in init, used in panels. due for replacement
*/

extern SDL_Window* window;		// main window		--	set in init, used to create renderer in resize
extern SDL_TimerID flag_timer;		// main program timer	--	used in panel, set in init and panel
extern bool reload_flag;		// resize stress timer flag --	set in events, used in panel
extern std::string outfile;		// output file name	-- 	set in init, used in itterate
extern bool resizing;			// active resizing flag	-- 	set in event, used in itterate


