#pragma once
#include "aaediclock.h"
#include "plugin_api.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <iostream>

class debugbuf : public std::streambuf {
     public:
          std::string plugin_name;
     protected:
          int overflow(int c) override;
          std::streamsize xsputn (const char* s, std::streamsize n);
          int sync() override;
     private:
          std::string strbuf;
};

class HostAPI final : public aaediclock_host_api {

     public:
          HostAPI(int new_id);
          ~HostAPI();
          void AaediHAM_GraphicsDrawText(const char* string, const aaediclock_Color color, const aaediclock_FRect dims) override;
          void AaediHAM_GraphicsClear(const aaediclock_Color& color = {0, 0, 0, 255}) override;
          const char* AaediHAM_ConfigGetCall() override;
          void AaediHAM_MapPinDelete() override;
          void AaediHAM_MapPinAdd(struct aaediclock_map_pin) override;
          const struct plugin_mouse_event AaediHAM_GetMouseEvent() override;
          ScreenFrame*	panel = nullptr;
          void set_plugin_name(const std::string& new_name);
     private:
          debugbuf debug_log_buffer;
          std::istream* api_debug_log;
          int plugin_id;
};


struct PluginModule {
#ifdef _WIN32
    HINSTANCE                   library					=	nullptr;	// plugin file pointer
#else
    void*                       library 				= 	nullptr;	// plugin file pointer
#endif
    aaediclock_plugin_api*      plugin  				= 	nullptr;	// plugin call point
    aaediclock_plugin_api*      (*create)()     			= 	nullptr;	// plugin constructor
    void                        (*destroy)(aaediclock_plugin_api*) 	= 	nullptr;	// plugin destructor
    bool                        draw_flag 				= 	false;		// trigger plugin this frame?
    int				id 					=	0;		// plugin numeric ID
    int				position				=	0;		// panel ID to use
    HostAPI*			host_api;							// plugin host API instance
    std::string                 name;								// plugin description
};


extern std::vector<PluginModule> loaded_plugins;

bool unregister_module (struct PluginModule* module);

bool register_module(const std::string& module_lib);


