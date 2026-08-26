#include "syslog.h"

aaediclock_host_api* host_api = nullptr;
bool show_log = false;
time_t last_event = 0;


extern "C" DllExport aaediclock_plugin_api* createPlugin() {
    return new syslog_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
    if (target) {
        delete target;
    }
}

void syslog_plugin::plugin_init() const {
    return;
}

void syslog_plugin::plugin_exit() const {
    return;
}
uint16_t log_tex_id = 0;
uint16_t test_tex_id = 0;
size_t toast_length = 0;
time_t toast_age = 0;
void syslog_plugin::plugin_main(const aaediclock_FRect& dims) const {
	(void)dims;
	aaediclock_FRect map_dims = host_api->AaediHAM_GetMapSize();
	host_api->AaediHAM_OverlaySet(map_dims, OVERLAY_FOREGROUND);
	host_api->AaediHAM_OverlayClear(aaediclock_Color{0,0,0,0});
    // reset the mouse event
	struct plugin_mouse_event mouse_event = host_api->AaediHAM_GetMouseEvent();
//	std::cout <<"keystamp: "<< mouse_event.timestamp << "\n";
	if (mouse_event.timestamp > last_event) {
		last_event = mouse_event.timestamp;
//		std::cout << "Updated keycode timestamp\n";
		if (mouse_event.keycode == SDLK_L) {
			std::cout << "Toggling log\n";
			show_log = !show_log;
			if (test_tex_id) {
				host_api->AaediHAM_TextureDelete(test_tex_id);
				test_tex_id = 0;
			}

		}
	}
	aaediclock_FRect bar_box;
	bar_box.y=map_dims.y+10;
	if (toast_age >0) {
		if ((time(NULL) - toast_age) > 5) {
			host_api->AaediHAM_TextureDelete(log_tex_id);
			log_tex_id = 0;
			toast_length = 0;
			toast_age = 0;
		}
	}
	if (show_log && !test_tex_id) {

		uint16_t logsize = host_api->AaediHAM_LogGetCount();
		std::string logstring;
		uint16_t logstart = 0;
		if (logsize > 25) {
			logstart = logsize - 25;
		}
		for (uint16_t log_index = logstart; log_index < logsize ; log_index++) {
			if (!logstring.empty() && logstring.back() != '\n') {
				logstring += "\n";
			}
			std::string buffer =  host_api->AaediHAM_LogGetIndex(log_index);

			logstring += buffer.substr(0,64);
		}
		test_tex_id =  host_api->AaediHAM_TextureCreateString (logstring.c_str(), aaediclock_Color({200,0,0,255}));

	}
	if (!log_tex_id) {
		const char* lastlog = host_api->AaediHAM_LogGetNew();
		if (lastlog && lastlog[0]) {
			log_tex_id = host_api->AaediHAM_TextureCreateString (lastlog, aaediclock_Color({200,0,0,255}));
			toast_length = strlen(lastlog);
			toast_age = time(NULL);
//			std::cout << "Creating Texture for " << lastlog << "\n";
		}
	
	}
	bar_box.y = map_dims.h*0.8;
	float height_multiplier = 0.06;
	bar_box.x = map_dims.w*2;
	
	if (log_tex_id) {
		while (bar_box.x + bar_box.w > map_dims.w) {
			bar_box.h = map_dims.h*height_multiplier;
			bar_box.w = bar_box.h* toast_length;
			bar_box.x = (map_dims.w - bar_box.w) /2;
			height_multiplier *=0.9;
		}
		host_api->AaediHAM_GraphicsDrawRect (aaediclock_Color({128,128,156,128}), bar_box, 1);
		host_api->AaediHAM_GraphicsDrawImage      (log_tex_id, &bar_box);

	}
	if (test_tex_id && show_log) {
		bar_box.x=map_dims.w/2;
		bar_box.y = 10;
		bar_box.w = map_dims.w/2;
		bar_box.h = map_dims.h*0.9;
		host_api->AaediHAM_GraphicsDrawRect (aaediclock_Color({128,128,156,128}), bar_box, 1);
		host_api->AaediHAM_GraphicsDrawImage      (test_tex_id, &bar_box);

	}
    return;
}

const char* syslog_plugin::getName() const {
    return "Syslog Module";
}

void syslog_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}

