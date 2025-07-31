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
        SDL_Color color;
        TrackedSatellite(const std::string& source_name, const std::string& l1, const std::string& l2);
        ~TrackedSatellite();
        TrackedSatellite(TrackedSatellite&& source) noexcept;	// move constructor
        TrackedSatellite& operator=(TrackedSatellite&& source) noexcept;     // move with replace
        TrackedSatellite(const TrackedSatellite& source);		// copy to new
        TrackedSatellite& operator=(const TrackedSatellite& source);	// copy over existing
        std::string& get_name();
        void new_tracking(const std::string& source_name, const std::string& l1, const std::string& l2);
        time_t pass_start();
        time_t pass_end();
        time_t telemetry_age();
        void draw_pass(const time_t pass_start, const time_t pass_end,  std::vector<SDL_FPoint> *pass_pts, const SDL_FRect *size);
        void add_telemetry(const double lat,const double lon, const double elevation, const double azimuth, const time_t timestamp);
        void location (SDL_FPoint *result);
        bool gen_telemetry(const int resolution, libsgp4::Observer& obs);
        void draw_telemetry(ScreenFrame& map);
};



void sat_tracker (ScreenFrame& panel, TTF_Font* font, ScreenFrame& map);
