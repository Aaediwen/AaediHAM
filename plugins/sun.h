

#ifndef SUN_H
#define SUN_H
#include "aaediclock.h"
#include "plugin_api.h"


class DllExport sdo_plugin : public aaediclock_plugin_api {
        void plugin_init() const override;
        void plugin_main(const aaediclock_FRect& dims) const override;
        const char* getName() const override;
        void plugin_exit() const override;
        void set_host(aaediclock_host_api* host);
};

#endif