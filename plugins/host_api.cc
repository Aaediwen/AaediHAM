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
}

HostAPI::~HostAPI() {
    if (api_debug_log) {
        delete (api_debug_log);
    }
    if (AaediHAM_LogDebug) {
        delete (AaediHAM_LogDebug);
    }
}

void HostAPI::set_plugin_name(const std::string& new_name) {
    debug_log_buffer.plugin_name = new_name;
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


void HostAPI::AaediHAM_GraphicsClear(const aaediclock_Color& color) {
//    SDL_Log("Attempting Plugin Panel Clear");
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

//********************************************************************************
// MAP Pins
//********************************************************************************

void HostAPI::AaediHAM_MapPinDelete() {
    for (size_t index = plugin_map_pins.size() ; index >0 ; index--) {
        if (plugin_map_pins[index-1].owner == plugin_id) {
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
    host_pin.icon		= static_cast<SDL_Texture*>(new_pin.icon);
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
