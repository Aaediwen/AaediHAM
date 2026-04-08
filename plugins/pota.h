#include "aaediclock.h"

#ifndef POTA_PLUGIN_H
#define POTA_PLUGIN_H
#include "plugin_api.h"


class DllExport pota_plugin : public aaediclock_plugin_api {
        void plugin_init() const override;
        void plugin_main(const aaediclock_FRect& dims) const override;
        const char* getName() const override;
        void plugin_exit() const override;
        void set_host(aaediclock_host_api* host);
};

#endif