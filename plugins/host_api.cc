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
        loaded_plugins.back().plugin->plugin_exit();
        if (module->destroy) {
            module->destroy(module->plugin);
        }
        module->plugin = nullptr;
    }
    if (module->destroy) {
        module->destroy = nullptr;
    }
    if (module->create) {
        module->create = nullptr;
    }
    if (module->library) {
#ifdef _WIN32
        FreeLibrary(module->library);
#else
        dlclose(module->library);
#endif
        module->library = nullptr;
    }
    if (module->host_api) {
        module->host_api->panel = nullptr;
        delete (module->host_api);
        module->host_api = nullptr;
    }
    module->name.clear();
    return true;
}

bool register_module(const std::string& module_lib) {
    struct PluginModule new_plugin;
    char* library_error = nullptr;
    // load library file
#ifdef _WIN32
    GetLastError();
    new_plugin.library = LoadLibraryA(module_lib.c_str());
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&library_error, 0, NULL);
#else
    library_error = dlerror();
    new_plugin.library = dlopen(module_lib.c_str(), RTLD_LAZY);
    library_error = dlerror();
#endif
    if (!new_plugin.library) {
        std::cout << "Error Loading Plugin Library File: " << module_lib;
        if (library_error) {
            std::cout << " Error Code" << library_error;
        }
        std::cout << "\n";
        unregister_module(&new_plugin);
        return false;
    }
    // load the constructor and destructor functions for the module
#ifdef _WIN32
    GetLastError();
    new_plugin.create   = (aaediclock_plugin * (*)())GetProcAddress(new_plugin.library, "createPlugin");
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&library_error, 0, NULL);
#else
    library_error = dlerror();
    new_plugin.create =         (aaediclock_plugin_api*(*)())dlsym(new_plugin.library, "createPlugin");
    library_error = dlerror();
#endif
    if (!new_plugin.create) {
        std::cout << "Error Loading Plugin Constructor: " << module_lib;
        if (library_error) {
            std::cout << " Error Code" << library_error;
        }
        std::cout << "\n";
        unregister_module(&new_plugin);
        return false;
    }
#ifdef _WIN32
    GetLastError();
    new_plugin.destroy = (void(*)(aaediclock_plugin*))GetProcAddress(new_plugin.library, "destroyPlugin");
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&library_error, 0, NULL);
#else
    library_error = dlerror();
    new_plugin.destroy =        (void(*)(aaediclock_plugin_api*))dlsym(new_plugin.library, "destroyPlugin");
    library_error = dlerror();
#endif
    if (!new_plugin.destroy) {
        std::cout << "Error Loading Plugin Destructor: " << module_lib;
        if (library_error) {
            std::cout << " Error Code" << library_error;
        }
        std::cout << "\n";
        unregister_module(&new_plugin);
        return false;
    }
    // create the plugin object
    new_plugin.plugin = new_plugin.create();
    if (!new_plugin.plugin) {
        std::cout << "Error Creating plugin object: " << module_lib;
        unregister_module(&new_plugin);
        return false;
    }
    // get the plugin name
    new_plugin.name = new_plugin.plugin->getName();

    // assign an ID
    if (loaded_plugins.empty()) {
        new_plugin.id = 0;
    } else {
        new_plugin.id = loaded_plugins.size();
    }
    // make it official
    loaded_plugins.push_back(new_plugin);
    loaded_plugins.back().host_api = new HostAPI(new_plugin.id);
    // init panel
    loaded_plugins.back().host_api->panel = nullptr;
    loaded_plugins.back().host_api->set_plugin_name(loaded_plugins.back().name);
    loaded_plugins.back().plugin->set_host((loaded_plugins.back().host_api));
    loaded_plugins.back().plugin->plugin_init();
    std::cout << "Loaded symbols from "<< module_lib << "\n";
    return true;
}

//********************************************************************************
// Debug Log
//********************************************************************************

int debugbuf::overflow(int c) {
    strbuf.push_back(static_cast<char>(c));
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
    for (std::streamsize i = 0; i < n; i++) {
        int result = overflow(static_cast<unsigned char>(s[i]));
        if (result != EOF) {
            count++;
        }
    }
    return count;
}

int debugbuf::sync() {
    debug_log << plugin_name << ": " << strbuf << "\n";
    strbuf.clear();
    debug_log.flush();
    return 0;
}


//********************************************************************************
// Utility Functions
//********************************************************************************

HostAPI::HostAPI(int new_id) {
    api_debug_log 	= new std::istream(&debug_log_buffer);
    AaediHAM_LogDebug 	= new std::ostream(&debug_log_buffer);
    plugin_id = new_id;
    texture_cache.clear();
}

HostAPI::~HostAPI() {
    if (api_debug_log) {
        delete (api_debug_log);
    }
    if (AaediHAM_LogDebug) {
        delete (AaediHAM_LogDebug);
    }
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
    SDL_SetRenderTarget(this->panel->GetRenderer(), this->panel->texture);
    return;
}

//********************************************************************************
// Graphics Calls
//********************************************************************************

void HostAPI::AaediHAM_GraphicsDrawText (const char* string, const aaediclock_Color color, const aaediclock_FRect dims) {
//    SDL_Log("Attempting Plugin Text write");
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

    this->panel->render_text(textbox, Sans, textcolor, string);
    return;
}

void HostAPI::AaediHAM_GraphicsDrawRect(const aaediclock_Color color, const aaediclock_FRect dims, bool filled) {

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
       SDL_SetRenderDrawColor(this->panel->GetRenderer(), color.r, color.g, color.b, color.a);
       SDL_RenderLine (this->panel->GetRenderer(), line.x, line.y, line.w, line.h);
       return;
}

void HostAPI::AaediHAM_GraphicsDrawLines(const aaediclock_Color color, const aaediclock_FPoint* point_list, int count) {
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
    index--;
    if (texture_cache[index]) {
        debug_log << "HostAPI: Drawing Texture ID: "<< index << " for plugin "<< plugin_id << "\n";
        SDL_RenderTexture(this->panel->GetRenderer(), texture_cache[index], nullptr, nullptr);
    }
    return;
}


void HostAPI::AaediHAM_GraphicsClear(const aaediclock_Color& color) {
//    SDL_Log("Attempting Plugin Panel Clear");
    SDL_Color textcolor;
    textcolor.r = color.r;
    textcolor.g = color.g;
    textcolor.b = color.b;
    textcolor.a = color.a;
    this->panel->Clear(textcolor);
}
/*
struct aaediclock_image AaediHAM_GraphicsGetText      (const char* string, const aaediclock_Color foreground, const aaediclock_Color background) {
    struct aaediclock_image result;
    std::string text = string;
    SDL_Surface* text_surface =  TTF_RenderText_Shaded(Sans, text.c_str(), text.size(), static_cast<SDL_Color>(foreground), static_cast<SDL_Color>(background));
    if (text_surface) {
        result.height = text_surface->h;
        result.width = test_surface->w;
        result.pixels = static_cast<uint8_t*>(text_surface->pixels);
    }
    return result;
}
*/
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
    result.name = clockconfig.dxserver().name;
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
    result.label = clockconfig.DXmsg();
    return result;
}

struct aaediclock_dx HostAPI::AaediHAM_ConfigGetDE() {
    struct aaediclock_dx result;
    const GeoCoord dx_coords = clockconfig.DE();
    result.lat = dx_coords.latitude;
    result.lon = dx_coords.longitude;
    result.label = "";
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
    result.callsign.clear();
    result.band = 0;
    if (clockconfig.next_wspr(&callsign, &band)) {
        result.callsign = callsign;
        result.band = band;
    }
    return result;
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
    uint16_t owner = plugin_id + 32; // +32 goes bye bye with the final old module
    return (overlays.overlay_check(static_cast<enum mod_name>(owner)));
}

void HostAPI::AaediHAM_OverlaySet(aaediclock_FRect dims) {
    uint16_t owner = plugin_id + 32; // +32 goes bye bye with the final old module
    SDL_FRect host_dims;
    host_dims.x = dims.x;
    host_dims.y = dims.y;
    host_dims.h = dims.h;
    host_dims.w = dims.w;
    ScreenFrame* overlay = overlays.get_overlay(this->panel->GetRenderer(), static_cast<enum mod_name>(owner), host_dims);
    if (overlay && overlay->texture) {
        SDL_SetRenderTarget(this->panel->GetRenderer(), overlay->texture);
    }
    return;
}

void HostAPI::AaediHAM_OverlayRemove() {
    uint16_t owner = plugin_id + 32; // +32 goes bye bye with the final old module
    overlays.remove_overlay(static_cast<enum mod_name>(owner));
    return;
}

void HostAPI::AaediHAM_OverlayClear(const aaediclock_Color& color) {
    uint16_t owner = plugin_id + 32; // +32 goes bye bye with the final old module
//    SDL_Log("Attempting Plugin Panel Clear");
    SDL_Color clearcolor;
    clearcolor.r = color.r;
    clearcolor.g = color.g;
    clearcolor.b = color.b;
    clearcolor.a = color.a;
    if (overlays.overlay_check(static_cast<enum mod_name>(owner))) {
        ScreenFrame* overlay = overlays.get_overlay(this->panel->GetRenderer(), owner, SDL_FRect{0,0,0,0});
        overlay->Clear(clearcolor);
    }
    return;
}

//********************************************************************************
// Icon Calls
//********************************************************************************

bool HostAPI::AaediHAM_IconCheck (uint16_t icon_index) {
    uint16_t owner = plugin_id + 32; // +32 goes bye bye with the final old module
    if (!icon_index) {
        return false;
    }
    icon_index--;
    return (icon_bin.icon_check(icon_index, owner));
}

uint16_t HostAPI::AaediHAM_IconCreate (const aaediclock_image& image_data) {
    if ((image_data.width < 1) || (image_data.height < 1)) {
        debug_log << "Icon API: no image size to create icon\n";
        return 0;

    }
    if (!image_data.pixels) {
        debug_log << "Icon API: no image data to create icon\n";
        return 0;
    }
    uint16_t owner = plugin_id + 32; // +32 goes bye bye with the final old module
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
    if ((image_data.width < 1) || (image_data.height < 1)) {
        return false;
    }
    if (!image_data.pixels) {
        return false;
    }
    uint16_t owner = plugin_id + 32; // +32 goes bye bye with the final old module
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
    uint16_t owner = plugin_id + 32; // +32 goes bye bye with the final old module
    icon_index--;
    icon_bin.icon_delete(icon_index, owner);
    return;
}

//********************************************************************************
// Texsture Cache Calls
//********************************************************************************

bool HostAPI::AaediHAM_TextureCheck (uint16_t index) {
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
          // everythign seem legit
          return (SDL_UpdateTexture(texture_cache[index], NULL, image_data.pixels, image_data.width*4));
    }
    return false;
}

void HostAPI::AaediHAM_TextureDelete(uint16_t index) {
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
/*

//     this code here is a flag for stability on Pi2
//     May need to keep a host-side cache of textures to prevent churn
//     much like overlays or icons are handled
//

    SDL_Texture* image_tex = SDL_CreateTextureFromSurface(this->panel->GetRenderer(), new_image);
    SDL_RenderTexture(this->panel->GetRenderer(), image_tex, nullptr, nullptr);
    SDL_DestroyTexture(image_tex);
    return;
}
*/
