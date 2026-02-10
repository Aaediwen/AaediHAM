#include "host_api.h"
#include <iostream>
#ifndef _WIN32
#include <dlfcn.h>
#endif

//std::vector<PluginModule> loaded_plugins;

bool unregister_module (struct PluginModule* module) {
    if (module->destroy) {
        module->destroy(module->plugin);
        module->destroy = nullptr;
        module->plugin = nullptr;
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
    module->name.clear();
    return true;
}


void HostAPI::AaediHAM_GraphicsDrawText (const char* string, const aaediclock_Color color, const aaediclock_FRect dims) {
    SDL_Log("Attempting Plugin Text write");
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
    SDL_Log("Attempting Plugin Panel Clear");
    SDL_Color textcolor;
    textcolor.r = color.r;
    textcolor.g = color.g;
    textcolor.b = color.b;
    textcolor.a = color.a;
    this->panel->Clear(textcolor);
}

const char* HostAPI::AaediHAM_ConfigGetCall() {
    return(clockconfig.CallSign().c_str());
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
    new_plugin.plugin->set_host(&new_plugin.host_api);
    // init panel
    new_plugin.host_api.panel = nullptr;
    // assign an ID
    if (loaded_plugins.empty()) {
        new_plugin.id = 1;
    } else {
        new_plugin.id = loaded_plugins.back().id + 1;
    }
    // make it official
    loaded_plugins.push_back(new_plugin);
    std::cout << "Loaded symbols from "<< module_lib << "\n";
    return true;
}
