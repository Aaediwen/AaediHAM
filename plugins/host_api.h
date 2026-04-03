#pragma once
#include "aaediclock.h"
#include "plugin_api.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <iostream>
#include <mutex>

class debugbuf : public std::streambuf {
     public:
          std::string plugin_name;
     protected:
          int overflow(int c) override;
          std::streamsize xsputn (const char* s, std::streamsize n);
          int sync() override;
     private:
          std::string strbuf;
          std::recursive_mutex debug_lock;
};

class HostAPI final : public aaediclock_host_api {

     public:
          HostAPI(uint16_t new_id);
          ~HostAPI();
          // graphics calls
          void AaediHAM_SetTarget				() override;
          void AaediHAM_GraphicsDrawText			(const char* string, const aaediclock_Color color, const aaediclock_FRect dims) override;
          void AaediHAM_GraphicsDrawRect			(const aaediclock_Color color, const aaediclock_FRect dims, bool filled) override;
          void AaediHAM_GraphicsDrawLine			(const aaediclock_Color color, const aaediclock_FRect line) override;
          void AaediHAM_GraphicsDrawLines			(const aaediclock_Color color, const aaediclock_FPoint* point_list, int count) override;
          void AaediHAM_GraphicsDrawImage      			(uint16_t index) override;
          void AaediHAM_GraphicsClear				(const aaediclock_Color& color = {0, 0, 0, 255}) override;
//          struct aaediclock_image AaediHAM_GraphicsGetText	(const char* string, const aaediclock_Color foreground, const aaediclock_Color background) override;
          // config calls
          const char* AaediHAM_ConfigGetQRZKey			(bool refresh = false) override;
          const char* AaediHAM_ConfigGetCall			() override;
          const char* AaediHAM_ConfigGetPSKCall       		() override;

          struct aaediclock_dx AaediHAM_ConfigGetDE		() override;
          void AaediHAM_ConfigSetDX				(struct aaediclock_dx new_dx) override;
          struct aaediclock_dx AaediHAM_ConfigGetDX		() override;
          struct plugin_server_info AaediHAM_ConfigGetDXServer	() override;
          struct plugin_wspr_station AaediHAM_ConfigGetNextWspr () override;
          const char* AaediHAM_ConfigGetNextRss       		() override;
          int AaediHAM_ConfigGetSatCount			() override;
          const char* AaediHAM_ConfigGetSat			(int index) override;
          // map pins
          void AaediHAM_MapPinDelete				() override;
          void AaediHAM_MapPinAdd				(struct aaediclock_map_pin) override;
          // program state requests
          const struct aaediclock_FRect AaediHAM_GetMapSize	() override;
          const struct plugin_mouse_event AaediHAM_GetMouseEvent() override;
          // overlay calls
          bool AaediHAM_OverlayCheck				() override;
          void AaediHAM_OverlaySet				(aaediclock_FRect dims) override;
          void AaediHAM_OverlayRemove				() override;
          void AaediHAM_OverlayClear				(const aaediclock_Color& color = {0, 0, 0, 255}) override;
          // icon calls
          bool AaediHAM_IconCheck				(uint16_t icon_index) override;
          uint16_t AaediHAM_IconCreate				(const aaediclock_image& image_data) override;
          bool AaediHAM_IconUpdate				(uint16_t icon_index, const aaediclock_image& image_data) override;
          void AaediHAM_IconDelete				(uint16_t icon_index) override;
          // texture cache calls
          bool AaediHAM_TextureCheck                            (uint16_t index) override;
          uint16_t AaediHAM_TextureCreate                       (const aaediclock_image& image_data) override;
          bool AaediHAM_TextureUpdate                           (uint16_t index, const aaediclock_image& image_data) override;
          void AaediHAM_TextureDelete                           (uint16_t index) override;
          // scroller calls
          const struct aaediclock_FRect AaediHAM_ScrollerInit	(const char* string, aaediclock_Color fg, aaediclock_Color bg) override;
          void AaediHAM_ScrollerPosition       			(const aaediclock_FRect source, const aaediclock_FRect dest) override;
          void AaediHAM_ScrollerDelete				() override;

          // internal state
          ScreenFrame*	panel = nullptr;
          void set_plugin_name(const std::string& new_name);
     private:
          debugbuf debug_log_buffer;
          std::istream* api_debug_log;
          SDL_Surface*  text_surface;
          uint16_t plugin_id;
          std::vector<SDL_Texture*>texture_cache;
          size_t rss_feed_index;
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
    uint16_t			id 					=	0;		// plugin numeric ID
    uint16_t			position				=	0;		// panel ID to use
    uint16_t			interval				=	0;		// how often to trigger in tenths of a second
    HostAPI*			host_api				= 	nullptr;	// plugin host API instance
    std::string                 name;								// plugin description
};


extern std::vector<PluginModule> loaded_plugins;

bool unregister_module (struct PluginModule* module);

bool register_module(const std::string& module_lib);


