#ifndef DX_CLUSTER_H
#define DX_CLUSTER_H
#include "aaediclock.h"
#include "plugin_api.h"
#include "utils/conversions.h"
#include "utils/socket.h"

class dxspot {
    public:
        std::string spotter;
        std::string dx;
        std::string note;
        std::string mode;
        std::string country;
        time_t timestamp;
        double frequency;
        double lat;
        double lon;
        bool qrz_valid;
        int entity;
        // country name?
        // flag?

        dxspot();
        ~dxspot();

        void find_mode();
        void fill_qrz();
        void display_spot(const aaediclock_FRect& dims, float y);
        void print_spot();
    private:
       void query_qrz ();

};




//void dx_cluster (ScreenFrame& panel);sample.h


class DllExport dx_cluster_plugin : public aaediclock_plugin_api {
        void plugin_init() const override;
        void plugin_main(const aaediclock_FRect& dims) const override;
        const char* getName() const override;
        void plugin_exit() const override;
        void set_host(aaediclock_host_api* host);
};

#endif