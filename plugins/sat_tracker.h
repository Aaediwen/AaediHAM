#ifndef SAT_TRACKER_H
#define SAT_TRACKER_H
#include "plugin_api.h"


#include <libsgp4/SGP4.h>
#include <libsgp4/Observer.h>
#include <memory>
#include "../aaediclock.h"


class TrackedSatellite {

    private:
        struct SatTelemetry {
            double lat;
            double lon;
            double azimuth;     // compass direction from DE
            double elevation;   // elevation above horizon relative to DE
            time_t timestamp;
        };

        std::string name;
        std::string tle1;
        std::string tle2;
        libsgp4::Tle* sat_tle;
        libsgp4::SGP4* sgp4;
        std::vector<struct SatTelemetry> telemetry;

    public:
        aaediclock_Color color;
        TrackedSatellite(const std::string& source_name, const std::string& l1, const std::string& l2);
        ~TrackedSatellite();
        TrackedSatellite(TrackedSatellite&& source) noexcept;	// move constructor
        TrackedSatellite& operator=(TrackedSatellite&& source) noexcept;     // move with replace
        TrackedSatellite(const TrackedSatellite& source);		// copy to new
        TrackedSatellite& operator=(const TrackedSatellite& source);	// copy over existing
        const std::string& get_name() const;
        void new_tracking(const std::string& source_name, const std::string& l1, const std::string& l2);
        time_t pass_start();
        time_t pass_end();
        time_t telemetry_age();
        void draw_pass(const time_t pass_start, const time_t pass_end,  std::vector<aaediclock_FPoint> *pass_pts, const aaediclock_FRect *size);
        void add_telemetry(const double lat,const double lon, const double elevation, const double azimuth, const time_t timestamp);
        void location (aaediclock_FPoint *result);
        bool gen_telemetry(const int resolution, libsgp4::Observer& obs);
        void draw_telemetry(aaediclock_FRect& dims);
};

//void sat_tracker (ScreenFrame& panel, TTF_Font* font, ScreenFrame& map);

class DllExport sat_tracker_plugin : public aaediclock_plugin_api {
        void plugin_init() const override;
        void plugin_main(const aaediclock_FRect& dims) const override;
        const char* getName() const override;
        void plugin_exit() const override;
        void set_host(aaediclock_host_api* host);
};

#endif