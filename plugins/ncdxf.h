#ifndef NCDXF_H
#define NCDXF_H
#include <string>
#include "plugin_api.h"

#ifdef _WIN32
#define DllExport __declspec(dllexport)
#else
#define DLLExport
#endif

struct beacon {
    char call[10];
    std::string location;
};

extern const struct beacon beacons[18];

extern const double beacon_freqs[5];

class DllExport ncdxf_plugin : public aaediclock_plugin_api {

        void plugin_init() const override;
        void plugin_main(const aaediclock_FRect& dims) const override;
        const char* getName() const override;
        void plugin_exit() const override;
        void set_host(aaediclock_host_api* host);
        uint32_t API_VERSION = 001;

};
#endif