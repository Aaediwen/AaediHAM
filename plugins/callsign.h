#include "aaediclock.h"

#ifndef CALLSIGN_H
#define CALLSIGN_H
#include <string>
#include "plugin_api.h"

#ifdef _WIN32
#define DllExport __declspec(dllexport)
#else
#define DLLExport
#endif


class DllExport callsign_plugin : public aaediclock_plugin_api {
        void plugin_init() const override;
        void plugin_main(const aaediclock_FRect& dims) const override;
        const char* getName() const override;
        void plugin_exit() const override;
        void set_host(aaediclock_host_api* host);


};

#endif