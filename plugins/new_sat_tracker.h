#include "aaediclock.h"
#include <libxml/parser.h>
#include "SGP4/SGP4.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


struct vector3 {
    double x;
    double y;
    double z;
};



       struct SatTelemetry {
            double lat		= 0.0;		// Terrestrial latitude
            double lon		= 0.0;		// Terrestrial longitude
            double alt		= 0.0;		// Topocentric altitude
            double dec		= 0.0;		// declination in RADIANS
            double ra		= 0.0;		// right ascention in RADIANS
            double azimuth      = 0.0;		// compass direction from DE
            double elevation    = 0.0;		// elevation above horizon relative to DE
            time_t timestamp 	= 0;		// Seconds since Unix Epoch
            double tsince 	= 0.0;		// Julian Date minutes since OMM Record Epoch
        };




// core Sat data struct

class OMMRecord {
    public:
        // internal state
        bool                valid   = false;
        std::string         message;
        // object identification
        std::string         name;
        std::string         object_id;
        uint32_t            norad_id;
        uint32_t            element_set_no;
        char                class_type;
        aaediclock_Color	color;
        // Earth relative orbital details
        std::string		epoch_raw;
        double              epoch_jd;
        double              mean_motion;
        double              eccentricity;
        double              inclination;
        double              right_ascension_of_ascending_node;
        double              arg_pericenter;
        double              mean_anomaly;
        int                 ephemeris_type;
        uint32_t            revolution_at_epoch;
        double              bstar;                  // atmospheric drag //  SGP4 drag term
        double              mean_motion_dot;        // first derivative
        double              mean_motion_ddot;       // second derivative // normally 0
        elsetrec 		satrec = {};
        std::vector<struct SatTelemetry> telemetry;
        bool generate_telemetry(int resolution_min);
        time_t telemetry_age();
        void draw_pass(const time_t pass_start, const time_t pass_end,  std::vector<aaediclock_FPoint> *pass_pts, const aaediclock_FRect *size);

        time_t pass_start();
        time_t pass_end();
        bool sgp4_init();
        void location (aaediclock_FPoint *result);

};



// everything that parses input from Celestrak goes here
namespace SGP4Parser {
    namespace XML {
        void process_node(xmlNode* start_node, struct OMMRecord& result);
    }
    double ISO8601_to_Julian(std::string);
    void fromXML(std::string& input);
}

class DllExport new_sat_tracker_plugin : public aaediclock_plugin_api {
        void plugin_init() const override;
        void plugin_main(const aaediclock_FRect& dims) const override;
        const char* getName() const override;
        void plugin_exit() const override;
        void set_host(aaediclock_host_api* host);
};
