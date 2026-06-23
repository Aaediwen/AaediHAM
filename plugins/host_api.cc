#include "host_api.h"
#include <iostream>
#ifndef _WIN32
#include <dlfcn.h>
#endif

//********************************************************************************
// Library Loading / Unloading
//********************************************************************************
bool unregister_module (struct PluginModule* module) {
    if (!module) {
        return false;
    }
    if (module->plugin) {
        try {
            module->plugin->plugin_exit();
            if (module->destroy) {
                module->destroy(module->plugin);
            }
        } catch (...) {
            std::cout << "Error Unloading module\n";
        }
        module->plugin = nullptr;
    }
    if (module->destroy) {
        module->destroy = nullptr;
    }
    if (module->create) {
        module->create = nullptr;
    }
    module->id = 0;
    module->position = 0;
    module->interval = 0;
    module->draw_flag = false;
    if (module->host_api) {
        module->host_api->panel = nullptr;
        delete (module->host_api);
        module->host_api = nullptr;
    }
    if (module->library) {
#ifdef _WIN32
        FreeLibrary(module->library);
#else
        dlclose(module->library);
#endif
        module->library = nullptr;
    }
    module->name.clear();
    return true;
}

bool register_module(const std::string& module_lib) {
    struct PluginModule new_plugin;
    char* library_error = nullptr;
    if (module_lib.empty()) {
        return false;
    }
    std::cout << "Loading Plugin: " << module_lib << "\n";
    std::cout.flush();
    // load library file
#ifdef _WIN32
    SetLastError(0);
    new_plugin.library = LoadLibraryA(module_lib.c_str());
    if (!new_plugin.library) {
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&library_error, 0, NULL);
    }
#else
    library_error = dlerror();
    new_plugin.library = dlopen(module_lib.c_str(), RTLD_LAZY);
    library_error = dlerror();
#endif
    if (!new_plugin.library) {
        std::cout << "Error Loading Plugin Library File: " << module_lib;
        if (library_error) {
            std::cout << " Error Code: " << library_error;
            #ifdef _WIN32
            LocalFree(library_error);
            library_error = nullptr;
            #endif
        }
        std::cout << "\n";
        unregister_module(&new_plugin);
        return false;
    }
    // load the constructor and destructor functions for the module
#ifdef _WIN32
    SetLastError(0);
    new_plugin.create   = (aaediclock_plugin_api * (*)())GetProcAddress(new_plugin.library, "createPlugin");
    if (!new_plugin.create) {
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&library_error, 0, NULL);
    }
#else
    library_error = dlerror();
    new_plugin.create =         (aaediclock_plugin_api*(*)())dlsym(new_plugin.library, "createPlugin");
    library_error = dlerror();
#endif
    if (!new_plugin.create) {
        std::cout << "Error Loading Plugin Constructor: " << module_lib;
        if (library_error) {
            std::cout << " Error Code: " << library_error;
            #ifdef _WIN32
            LocalFree(library_error);
            library_error = nullptr;
            #endif
        }
        std::cout << "\n";
        unregister_module(&new_plugin);
        return false;
    }
#ifdef _WIN32
    SetLastError(0);
    new_plugin.destroy = (void(*)(aaediclock_plugin_api*))GetProcAddress(new_plugin.library, "destroyPlugin");
    if (!new_plugin.destroy) {
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&library_error, 0, NULL);
    }
#else
    library_error = dlerror();
    new_plugin.destroy =        (void(*)(aaediclock_plugin_api*))dlsym(new_plugin.library, "destroyPlugin");
    library_error = dlerror();
#endif
    if (!new_plugin.destroy) {
        std::cout << "Error Loading Plugin Destructor: " << module_lib;
        if (library_error) {
            std::cout << " Error Code: " << library_error;
            #ifdef _WIN32
            LocalFree(library_error);
            library_error = nullptr;
            #endif
        }
        std::cout << "\n";
        unregister_module(&new_plugin);
        return false;
    }
    // create the plugin object
    try {
        new_plugin.plugin = new_plugin.create();
    } catch (std::exception& e) {
        std::cout << "Exception " << e.what() << " while creating plugin: " << module_lib << "\n";
        unregister_module(&new_plugin);
        return false;
    } catch (...) {
        std::cout << "Unknown Exception while creating plugin: " << module_lib << "\n";
        unregister_module(&new_plugin);
        return false;
    }
    if (!new_plugin.plugin) {
        std::cout << "Plugin Error Creating plugin object: " << module_lib << "\n";
        unregister_module(&new_plugin);
        return false;
    }
    // get the plugin name
    try {
        new_plugin.name = new_plugin.plugin->getName();
    } catch (...) {
        std::cout << "Plugin Error Getting Plugin Name: " << module_lib << "\n";
        unregister_module(&new_plugin);
        return false;
    }

    // assign an ID
    if (loaded_plugins.empty()) {
        new_plugin.id = 0;
    } else {
        new_plugin.id = static_cast<uint16_t>(loaded_plugins.size());
    }
    // make it official
    loaded_plugins.push_back(new_plugin);
    loaded_plugins.back().host_api = new HostAPI(new_plugin.id);
    // init panel
    loaded_plugins.back().host_api->panel = nullptr;
    loaded_plugins.back().host_api->set_plugin_name(loaded_plugins.back().name);
    loaded_plugins.back().plugin->set_host((loaded_plugins.back().host_api));
    loaded_plugins.back().plugin->plugin_init();
    std::cout << "Loaded "<< loaded_plugins.back().name << "\n";
    std::cout.flush();
    return true;
}

//********************************************************************************
// Debug Log
//********************************************************************************

int debugbuf::overflow(int c) {
    const std::lock_guard<std::recursive_mutex>char_lock(debug_lock);
    if (c != EOF) {
        strbuf.push_back(static_cast<char>(c));
    }
    if (c == '\n') {
        debug_log << plugin_name << ": " << strbuf;
        strbuf.clear();
    }
    if (strbuf.size() > 1024) {
        debug_log << plugin_name << ": " << strbuf << "\n";
        strbuf.clear();
    }
    if (c == EOF) {
        debug_log << plugin_name << ": " << strbuf << "\n";
        strbuf.clear();
    }
    return c;
}

std::streamsize debugbuf::xsputn (const char* s, std::streamsize n) {
    std::streamsize count = 0;
    const std::lock_guard<std::recursive_mutex>str_lock(debug_lock);
    for (std::streamsize i = 0; i < n; i++) {
        int result = overflow(static_cast<unsigned char>(s[i]));
        if (result != EOF) {
            count++;
        }
    }
    return count;
}

int debugbuf::sync() {
    if (!strbuf.empty()) {
        debug_log << plugin_name << ": " << strbuf << "\n";
        strbuf.clear();
    }
    debug_log.flush();
    return 0;
}


//********************************************************************************
// Utility Functions
//********************************************************************************

HostAPI::HostAPI(uint16_t new_id) {
    api_debug_log 	= new std::istream(&debug_log_buffer);
    AaediHAM_LogDebug 	= new std::ostream(&debug_log_buffer);
    plugin_id = new_id;
    texture_cache.clear();
    text_surface = nullptr;
    rss_feed_index = 0;
}

HostAPI::~HostAPI() {
    if (api_debug_log) {
        delete (api_debug_log);
    }
    if (AaediHAM_LogDebug) {
        delete (AaediHAM_LogDebug);
    }
    if (text_surface) {
        SDL_DestroySurface(text_surface);
    }
    text_surface = nullptr;
    if (!texture_cache.empty()) {
        for (SDL_Texture*& tex : texture_cache) {
            if (tex) {
                SDL_DestroyTexture(tex);
                tex = nullptr;
            }
        }
        texture_cache.clear();
    }
}

void HostAPI::set_plugin_name(const std::string& new_name) {
    debug_log_buffer.plugin_name = new_name;
    return;
}

void HostAPI::AaediHAM_SetTarget() {
    if (SDL_GetCurrentThreadID() != main_thread_id) {
	debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
	return;
    }
    SDL_SetRenderTarget(this->panel->GetRenderer(), this->panel->texture);
    return;
}

//********************************************************************************
// Graphics Calls
//********************************************************************************

void HostAPI::AaediHAM_GraphicsDrawText (const char* string, const aaediclock_Color color, const aaediclock_FRect dims) {
//    SDL_Log("Attempting Plugin Text write");
    if ((!string) || (!string[0])) {
        return;
    }
    if ((dims.h <= 0) || (dims.w <= 0)) {
        return;
    }
    SDL_FRect textbox;
    textbox.x = dims.x;
    textbox.y = dims.y;
    textbox.h = dims.h;
    textbox.w = dims.w;
    SDL_Color textcolor;
    textcolor.r = color.r;
    textcolor.g = color.g;
    textcolor.b = color.b;
    textcolor.a = color.a;
    if (SDL_GetCurrentThreadID() != main_thread_id) {
	debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
	return;
    }
    this->panel->render_text(textbox, Sans, textcolor, string);
    return;
}

void HostAPI::AaediHAM_GraphicsDrawRect(const aaediclock_Color color, const aaediclock_FRect dims, bool filled) {
    if ((dims.h <= 0) || (dims.w <= 0)) {
        return;
    }
    if (SDL_GetCurrentThreadID() != main_thread_id) {
	debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
	return;
    }
    SDL_FRect host_dims;
    host_dims.x = dims.x;
    host_dims.y = dims.y;
    host_dims.h = dims.h;
    host_dims.w = dims.w;
//       debug_log << "Plugin DrawRect: " << dims.x << ", "  << dims.y << ", "  << dims.h << ", "  << dims.w << "\n";
//       debug_log << "Plugin DrawRect: " << host_dims.x << ", "  << host_dims.y << ", "  << host_dims.h << ", "  << host_dims.w << "\n";
    SDL_SetRenderDrawColor(this->panel->GetRenderer(), color.r, color.g, color.b, color.a);
    if (filled) {
        SDL_RenderFillRect(this->panel->GetRenderer(), &host_dims );
    } else {
        SDL_RenderRect(this->panel->GetRenderer(), &host_dims );
    }
    return;
}

void HostAPI::AaediHAM_GraphicsDrawLine(const aaediclock_Color color, const aaediclock_FRect line) {
       if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return;
       }
       SDL_SetRenderDrawColor(this->panel->GetRenderer(), color.r, color.g, color.b, color.a);
       SDL_RenderLine (this->panel->GetRenderer(), line.x, line.y, line.w, line.h);
       return;
}

void HostAPI::AaediHAM_GraphicsDrawLines(const aaediclock_Color color, const aaediclock_FPoint* point_list, int count) {
    if (!point_list || count <= 0) {
        return;
    }
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return;
    }
    std::vector<SDL_FPoint>new_points;
    for (int c=0 ; c < count ; c++) {
        SDL_FPoint new_point;
        new_point.x = point_list[c].x;
        new_point.y = point_list[c].y;
        new_points.push_back(new_point);
    }
    SDL_SetRenderDrawColor(this->panel->GetRenderer(), color.r, color.g, color.b, color.a);
    SDL_RenderLines(this->panel->GetRenderer(), new_points.data(), count);
    return;
}

void HostAPI::AaediHAM_GraphicsDrawImage (uint16_t index) {
    if ((static_cast<size_t>(index) > texture_cache.size()) || texture_cache.empty()) {
        return;
    }
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return;
    }
    index--;
    if (texture_cache[index]) {
        debug_log << "HostAPI: Drawing Texture ID: "<< index << " for plugin "<< plugin_id << "\n";
        SDL_RenderTexture(this->panel->GetRenderer(), texture_cache[index], nullptr, nullptr);
    }
    return;
}


void HostAPI::AaediHAM_GraphicsClear(const aaediclock_Color& color) {
//    SDL_Log("Attempting Plugin Panel Clear");
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return;
    }
    SDL_Color textcolor;
    textcolor.r = color.r;
    textcolor.g = color.g;
    textcolor.b = color.b;
    textcolor.a = color.a;
    this->panel->Clear(textcolor);
}

//********************************************************************************
// Host Queries
//********************************************************************************

const struct plugin_mouse_event HostAPI::AaediHAM_GetMouseEvent() {
    struct plugin_mouse_event result;
    result.coords.x = clock_mouse_event.mod_cords.x;
    result.coords.y = clock_mouse_event.mod_cords.y;
    result.coords.h = 0.0f;
    result.coords.w = 0.0f;
    result.click_count = clock_mouse_event.mod_count;
    result.valid = (clock_mouse_event.plugin_owner == plugin_id);
    if (result.valid) {
        clock_mouse_event.plugin_owner = -1;
        clock_mouse_event.mod_owner = MOD_NULL;
    }
    return result;
}

const char* HostAPI::AaediHAM_ConfigGetCall() {
    return(clockconfig.CallSign().c_str());
}

const char* HostAPI::AaediHAM_ConfigGetCachePath() {
    return(clockconfig.CachePath().c_str());
}

const char* HostAPI::AaediHAM_ConfigGetAssetPath() {
    return(clockconfig.AssetPath().c_str());
}

const char* HostAPI::AaediHAM_ConfigGetPSKCall() {
    return(clockconfig.PSKCall().c_str());
}

const struct aaediclock_FRect HostAPI::AaediHAM_GetMapSize() {
    aaediclock_FRect result;
    result.x = winboxes[PANEL_MAP].panel.dims.x;
    result.y = winboxes[PANEL_MAP].panel.dims.y;
    result.h = winboxes[PANEL_MAP].panel.dims.h;
    result.w = winboxes[PANEL_MAP].panel.dims.w;
    return result;
}

struct plugin_server_info HostAPI::AaediHAM_ConfigGetDXServer() {
    struct plugin_server_info result;
//    result.name = clockconfig.dxserver().name;
    memset(result.name, 0, 128);
    strncpy(result.name, clockconfig.dxserver().name.c_str(),127);
    result.name[127]=0;
    result.port = clockconfig.dxserver().port;
    return result;
}

const char* HostAPI::AaediHAM_ConfigGetQRZKey(bool refresh) {
    if (refresh) {
        clockconfig.qrz_key(1);
    }
    return(clockconfig.qrz_key().c_str());
}

void HostAPI::AaediHAM_ConfigSetDX(struct aaediclock_dx new_dx) {
    struct GeoCoord temp_coords;
    temp_coords.latitude = new_dx.lat;
    temp_coords.longitude = new_dx.lon;
    clockconfig.set_DX(temp_coords, new_dx.label);
    return;
}

struct aaediclock_dx HostAPI::AaediHAM_ConfigGetDX() {
    struct aaediclock_dx result;
    const GeoCoord dx_coords = clockconfig.DX();
    result.lat = dx_coords.latitude;
    result.lon = dx_coords.longitude;
    strncpy(result.label, clockconfig.DXmsg().c_str(),31);
    result.label[31]=0;
//    result.label = clockconfig.DXmsg();
    return result;
}

struct aaediclock_dx HostAPI::AaediHAM_ConfigGetDE() {
    struct aaediclock_dx result;
    const GeoCoord dx_coords = clockconfig.DE();
    result.lat = dx_coords.latitude;
    result.lon = dx_coords.longitude;
//    result.label = "";
    result.label[0]=0;
    return result;
}

int HostAPI::AaediHAM_ConfigGetSatCount() {
    return (static_cast<int>(clockconfig.Sats().size()));
}

const char* HostAPI::AaediHAM_ConfigGetSat(int index) {
    if ((index < 0) || (static_cast<size_t>(index) >= clockconfig.Sats().size())) {
        return nullptr;
    } else {
        return (clockconfig.Sats()[index].c_str());
    }
}

struct plugin_wspr_station HostAPI::AaediHAM_ConfigGetNextWspr() {
    std::string callsign;
    int band;
    struct plugin_wspr_station result;
//    result.callsign.clear();
    memset(result.callsign,0,32);
    result.band = 0;
    if (clockconfig.next_wspr(&callsign, &band)) {
        memset(result.callsign, 0, 32);
        strncpy(result.callsign, callsign.c_str(), 31);
//        result.callsign = callsign;
        result.callsign[31]=0;
        result.band = static_cast<uint16_t>(band);
    }
    return result;
}

const char* HostAPI::AaediHAM_ConfigGetNextRss() {
    if ((clockconfig.Rss().empty()) || (rss_feed_index >= clockconfig.Rss().size())) {
        rss_feed_index = 0;
        return 0;
    } else {
        const char* result = clockconfig.Rss()[rss_feed_index].c_str();
        rss_feed_index++;
        return result;
    }
}



//********************************************************************************
// MAP Pins
//********************************************************************************

void HostAPI::AaediHAM_MapPinDelete() {
    for (size_t index = plugin_map_pins.size() ; index >0 ; index--) {
        if (plugin_map_pins[index-1].plugin_owner == plugin_id) {
            plugin_map_pins.erase(plugin_map_pins.begin()+index-1);
        }
    }
    return;
}

void HostAPI::AaediHAM_MapPinAdd(struct aaediclock_map_pin new_pin){
    struct map_pin host_pin;
    host_pin.owner 		=  MOD_NULL;
    host_pin.plugin_owner	=plugin_id;
    host_pin.lat		=new_pin.lat;
    host_pin.lon		=new_pin.lon;
    if (new_pin.icon) {
       host_pin.icon		=icon_bin.get_icon(new_pin.icon);
    } else {
       host_pin.icon 		= nullptr;
    }
    host_pin.color.r		=new_pin.color.r;
    host_pin.color.g		=new_pin.color.g;
    host_pin.color.b		=new_pin.color.b;
    host_pin.color.a		=new_pin.color.a;
    memset 		(host_pin.label,0,16);
    memcpy		(host_pin.label, new_pin.label, 15);
    memset 		(host_pin.tooltip,0,512);
    memcpy		(host_pin.tooltip, new_pin.tooltip, 511);
    host_pin.next 		= nullptr;
    plugin_map_pins.push_back(host_pin);
    return;
}

//********************************************************************************
// Overlay Calls
//********************************************************************************

bool HostAPI::AaediHAM_OverlayCheck() {
    uint16_t owner = plugin_id;
    return (overlays.overlay_check(static_cast<enum mod_name>(owner)));
}

void HostAPI::AaediHAM_OverlaySet(aaediclock_FRect dims, uint8_t z_layer) {
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return;
    }
    uint16_t owner = plugin_id;
    SDL_FRect host_dims;
    host_dims.x = dims.x;
    host_dims.y = dims.y;
    host_dims.h = dims.h;
    host_dims.w = dims.w;
    ScreenFrame* overlay = overlays.get_overlay(this->panel->GetRenderer(), static_cast<enum mod_name>(owner), host_dims, z_layer);
    if (overlay && overlay->texture) {
        SDL_SetRenderTarget(this->panel->GetRenderer(), overlay->texture);
    }
    return;
}

void HostAPI::AaediHAM_OverlayRemove() {
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return;
    }
    uint16_t owner = plugin_id;
    overlays.remove_overlay(static_cast<enum mod_name>(owner));
    return;
}

void HostAPI::AaediHAM_OverlayClear(const aaediclock_Color& color) {
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return;
    }
    uint16_t owner = plugin_id ;
//    SDL_Log("Attempting Plugin Panel Clear");
    SDL_Color clearcolor;
    clearcolor.r = color.r;
    clearcolor.g = color.g;
    clearcolor.b = color.b;
    clearcolor.a = color.a;
    if (overlays.overlay_check(static_cast<enum mod_name>(owner))) {
        ScreenFrame* overlay = overlays.get_overlay(this->panel->GetRenderer(), owner, SDL_FRect{0,0,0,0}, OVERLAY_DEFAULT);
        overlay->Clear(clearcolor);
    }
    return;
}

//********************************************************************************
// Icon Calls
//********************************************************************************

bool HostAPI::AaediHAM_IconCheck (uint16_t icon_index) {
    uint16_t owner = plugin_id;
    if (!icon_index) {
        return false;
    }
    icon_index--;
    return (icon_bin.icon_check(icon_index, owner));
}

uint16_t HostAPI::AaediHAM_IconCreate (const aaediclock_image& image_data) {
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return 0;
    }
    if ((image_data.width < 1) || (image_data.height < 1)) {
        debug_log << "Icon API: no image size to create icon\n";
        return 0;

    }
    if (!image_data.pixels) {
        debug_log << "Icon API: no image data to create icon\n";
        return 0;
    }
    uint16_t owner = plugin_id ;
    debug_log << "Icon API: Creating API Surface for new icon\n";
    SDL_Surface* new_icon = SDL_CreateSurfaceFrom( image_data.width, image_data.height, SDL_PIXELFORMAT_RGBA8888, image_data.pixels, image_data.width*4);
    uint16_t result = 0;
    if (new_icon) {
        result = icon_bin.icon_create(owner, new_icon);
        debug_log << "Icon API: Created ICON Id "<< result<<"\n";
        SDL_DestroySurface(new_icon);
    } else {
        debug_log << "Icon API: Unable to create temp icon surface\n";
    }
    debug_log << "Icon API: returning ICON Id "<< result<<"\n";
    return (result);
}

bool HostAPI::AaediHAM_IconUpdate (uint16_t icon_index, const aaediclock_image& image_data) {
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return false;
    }
    if ((image_data.width < 1) || (image_data.height < 1)) {
        return false;
    }
    if (!image_data.pixels) {
        return false;
    }
    uint16_t owner = plugin_id ;
    icon_index--;
    if (icon_bin.icon_check(icon_index, owner)) {
        SDL_Surface* new_icon = SDL_CreateSurfaceFrom( image_data.width, image_data.height, SDL_PIXELFORMAT_RGBA8888, image_data.pixels, image_data.width*4);
        if (new_icon) {
           icon_bin.icon_update(owner, icon_index, new_icon);
           SDL_DestroySurface(new_icon);
           return true;
        } else {
           return false;
        }
    } else {
        return false;
    }
}

void HostAPI::AaediHAM_IconDelete (uint16_t icon_index) {
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return;
    }
    uint16_t owner = plugin_id;
    icon_index--;
    icon_bin.icon_delete(icon_index, owner);
    return;
}

//********************************************************************************
// Texsture Cache Calls
//********************************************************************************

bool HostAPI::AaediHAM_TextureCheck (uint16_t index) {
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return false;
    }
    if ((static_cast<size_t>(index) > texture_cache.size()) || texture_cache.empty()) {
        return false;
    }
    if (!index) {
        return false;
    }
    index--;
    if (texture_cache[index]) {
        return true;
    }
    return false;
}

uint16_t HostAPI::AaediHAM_TextureCreate (const aaediclock_image& image_data) {
    uint16_t result = 0;
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return result;
    }
    if ((image_data.width < 1) || (image_data.height < 1)) {
        return result;
    }
    if (!image_data.pixels) {
        return result;
    }
//    SDL_Surface* new_image = SDL_CreateSurfaceFrom( image_data.width, image_data.height, SDL_PIXELFORMAT_RGBA8888, image_data.pixels, image_data.width*4);
//    if (new_image) {
        SDL_Texture* image_tex = SDL_CreateTexture(this->panel->GetRenderer(), SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, image_data.width, image_data.height);
        SDL_SetTextureBlendMode(image_tex, SDL_BLENDMODE_BLEND);
        if (image_tex) {
            if (SDL_UpdateTexture(image_tex, NULL, image_data.pixels, image_data.width*4)) {
                texture_cache.push_back(image_tex);
                result = static_cast<uint16_t>(texture_cache.size());
                debug_log << "HostAPI: Returning Texture ID: "<< result << " for plugin "<< plugin_id << "\n";
            } else {
                SDL_DestroyTexture(image_tex);
            }
        }
//        SDL_DestroySurface(new_image);
//    }
    return result;
}

bool HostAPI::AaediHAM_TextureUpdate (uint16_t index, const aaediclock_image& image_data) {
    uint16_t result = 0;
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return result;
    }
    if ((image_data.width < 1) || (image_data.height < 1)) {
        return result;
    }
    if (!image_data.pixels) {
        return result;
    }

    if ((static_cast<size_t>(index) > texture_cache.size()) || texture_cache.empty()) {
        return false;
    }
    index--;
    if (texture_cache[index]) {
          // everything seems legit
          if ((static_cast<int>(image_data.width) == texture_cache[index]->w) && (static_cast<int>(image_data.height) == texture_cache[index]->h)) {
              return (SDL_UpdateTexture(texture_cache[index], NULL, image_data.pixels, image_data.width*4));
          }
    }
    return false;
}

void HostAPI::AaediHAM_TextureDelete(uint16_t index) {
    if (SDL_GetCurrentThreadID() != main_thread_id) {
           debug_log << debug_log_buffer.plugin_name << ": Texture call from non-parent thread. ignoring\n";
           return;
    }
    if ((static_cast<size_t>(index) > texture_cache.size()) || texture_cache.empty()) {
        return;
    }
    index--;
    if (texture_cache[index]) {
        SDL_DestroyTexture(texture_cache[index]);
        texture_cache[index] = nullptr;
    }
    return;
}

//********************************************************************************
// Scroller Calls
//********************************************************************************

const struct aaediclock_FRect HostAPI::AaediHAM_ScrollerInit (const char* string, aaediclock_Color fg, aaediclock_Color bg) {
     aaediclock_FRect result;
     result = aaediclock_FRect{0.0, 0.0, 0.0, 0.0};
     if (!string || !string[0]) {
         return result;
     }
     std::string str = string;
     debug_log << "Created Scroller stack for " << str << "\n";
     SDL_Color foreground, background;
     foreground.r = fg.r;
     foreground.g = fg.g;
     foreground.b = fg.b;
     foreground.a = fg.a;
     background.r = bg.r;
     background.g = bg.g;
     background.b = bg.b;
     background.a = bg.a;
     this->AaediHAM_ScrollerDelete();
//     text_surface = TTF_RenderText_Shaded(Sans, str.c_str(), str.size(), foreground, background);
     text_surface = TTF_RenderText_Blended(Sans, str.c_str(), str.size(), foreground);
     background = background;
     if (text_surface) {
         result.x = 0;
         result.w = static_cast<float>(text_surface->w);
         result.y = 0;
         result.h = static_cast<float>(text_surface->h);

         int offset = 0;
         int width = 500;
         while (offset < text_surface->w) {
             if (offset + width > text_surface->w) {
                 width = text_surface->w - offset;
             }
             SDL_Texture* new_segment_tex = nullptr;
             SDL_Surface* new_segment_surf = nullptr;
             new_segment_surf = SDL_CreateSurface(width, text_surface->h, text_surface->format);
             if (new_segment_surf) {
                 SDL_Rect srcrect;
                 srcrect.x=offset;
                 srcrect.y = 0;
                 srcrect.w = width;
                 srcrect.h = text_surface->h;
                 SDL_BlitSurfaceScaled(text_surface, &srcrect, new_segment_surf, NULL, SDL_SCALEMODE_LINEAR);
                 new_segment_tex = SDL_CreateTextureFromSurface(clock_renderer, new_segment_surf);
                 if (new_segment_tex) {
                     struct scroller_section new_section ;
                     new_section.segment = new_segment_tex;
                     SDL_SetTextureBlendMode(new_section.segment, SDL_BLENDMODE_BLEND);
                     scroll_buffer.emplace_back(new_section);
                 }
                 SDL_DestroySurface(new_segment_surf);
             }
             offset += width;
         }
     } else {
         text_surface = nullptr;
     }
     return result;
}


void HostAPI::AaediHAM_ScrollerPosition(const aaediclock_FRect source, const aaediclock_FRect dest) {
    if ((source.x < 0)||(source.y < 0)) {
        return;
    }
    if (!text_surface) {
        return;
    }
    (void)source;
    SDL_FRect target_source;
    SDL_FRect target_dest;
    target_dest.x = dest.x;
    target_dest.y = dest.y;
    target_source.x = 0;
    target_source.y = 0;
    float string_offset = source.x;
    debug_log << "Showing Scroller Overlay\n";
    SDL_Texture* current_texture = SDL_GetRenderTarget(this->panel->GetRenderer());
    for (auto& segment : scroll_buffer) {
        target_source.x = 0;
        target_source.y = 0;
        target_dest.w = segment.segment->w;
        target_dest.h = dest.h;
        target_source.w = segment.segment->w;
        target_source.h = segment.segment->h;
        float segment_offset = 0;
        if (string_offset >0) {
            if (string_offset > target_source.w) {
                segment_offset = target_source.w;
                string_offset -= segment_offset;
                target_source.x = segment_offset;
                target_source.w = 0;
                target_dest.w = 0;
            } else {
                segment_offset = string_offset;
                string_offset = 0;
                target_source.x = segment_offset;
                target_source.w -= segment_offset;
                target_dest.w -= segment_offset;
            }
        }

//        SDL_SetRenderDrawColor(this->panel->GetRenderer(), 255, 0, 0, 64);
//        SDL_RenderFillRect(this->panel->GetRenderer(), &target_dest);
//        debug_log << "Showing Scroller segment\n";

        if (current_texture && target_dest.x <= current_texture->w) {
            SDL_RenderTexture(this->panel->GetRenderer(), segment.segment, &target_source, &target_dest);
//          float rendered_width = target_source.w - segment_offset;
            target_dest.x += target_source.w;
        }
    }

    return;
}

void HostAPI::AaediHAM_ScrollerDelete() {
    if (text_surface) {
        SDL_DestroySurface(text_surface);
        text_surface = nullptr;
    }
    for (auto& segment : scroll_buffer) {
        if (segment.segment) {
            SDL_DestroyTexture(segment.segment);
            segment.offset = 0;
        }
    }
    scroll_buffer.clear();
    return;
}