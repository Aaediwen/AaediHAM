#ifndef MODULES_H

#define MODULES_H

#include <libsgp4/SGP4.h>
#include <libsgp4/Observer.h>

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
        void display_spot(ScreenFrame& panel, int y, int max_age);
        void print_spot();
    private:
       void query_qrz ();

};


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
        libsgp4::Tle sat_tle;
        libsgp4::SGP4 sgp4;
        std::vector<struct SatTelemetry> telemetry;

    public:
        SDL_Color color;
        TrackedSatellite(const std::string& source_name, const std::string& l1, const std::string& l2);
        ~TrackedSatellite();
        std::string& get_name();
        void new_tracking(const std::string& source_name, const std::string& l1, const std::string& l2);
        time_t pass_start();
        time_t pass_end();
        void draw_pass(const time_t pass_start, const time_t pass_end,  std::vector<SDL_FPoint> *pass_pts, const SDL_FRect *size);
        void add_telemetry(const double lat,const double lon, const double elevation, const double azimuth, const time_t timestamp);
        void location (SDL_FPoint *result);
        bool gen_telemetry(const int resolution, libsgp4::Observer& obs);
        void draw_telemetry(ScreenFrame& map);
};




void dx_cluster (ScreenFrame& panel);
void draw_de_dx(ScreenFrame& panel, TTF_Font* font, double lat, double lon, int de_dx);
void draw_callsign(ScreenFrame& panel, TTF_Font* font, const char* callsign);
void load_maps();
int draw_map(ScreenFrame& panel);
int draw_clock(ScreenFrame& panel, TTF_Font* font);
void pota_spots(ScreenFrame& panel, TTF_Font* font);
void sat_tracker (ScreenFrame& panel, TTF_Font* font, ScreenFrame& map);


#endif

