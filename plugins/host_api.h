#pragma once
#include "aaediclock.h"
#include "plugin_api.h"
#ifdef _WIN32
#include <windows.h>
#endif

class HostAPI final : public aaediclock_host_api {
     public:
          void AaediHAM_GraphicsDrawText(const char* string, const aaediclock_Color color, const aaediclock_FRect dims) override;
          void AaediHAM_GraphicsClear(const aaediclock_Color& color = {0, 0, 0, 255}) override;

          const char* AaediHAM_ConfigGetCall() override;
          ScreenFrame*	panel = nullptr;
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
    HostAPI			host_api;							// plugin host API instance
    std::string                 name;								// plugin description
};


extern std::vector<PluginModule> loaded_plugins;

bool unregister_module (struct PluginModule* module);

bool register_module(const std::string& module_lib);


