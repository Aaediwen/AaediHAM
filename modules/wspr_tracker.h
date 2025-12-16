#include <memory>
#include "../aaediclock.h"


class TrackedWSPR {
    public:
        enum class Band : int16_t {
            LF   = -1,
            MF   = 0,
            M160 = 1,
            M80  = 3,
            M60  = 5,
            M40  = 7,
            M30  = 10,
            M20  = 14,
            M17  = 18,
            M15  = 21,
            M12  = 24,
            M10  = 28,
            M6   = 50,
            M4   = 70,
            M2   = 144,
            CM70 = 432,
            CM23 = 1296
        };
        SDL_Color m_color;
        TrackedWSPR(const std::string& tx_call, Band band, time_t start);
        ~TrackedWSPR();
        TrackedWSPR(TrackedWSPR&& source) noexcept;	// move constructor
        TrackedWSPR& operator=(TrackedWSPR&& source) noexcept;     // move with replace
        TrackedWSPR(const TrackedWSPR& source);		// copy to new
        TrackedWSPR& operator=(const TrackedWSPR& source);	// copy over existing
        const std::string& get_name() const;
        time_t telemetry_age();
        const struct GeoCoord location () const;
        bool gen_telemetry();
        void draw_telemetry(ScreenFrame& map);
        void serialize(std::ostream& output);

    private:
        struct WSPRTelemetry {
            uint64_t id;
            struct GeoCoord tx_loc;
            struct GeoCoord rx_loc;
            char rx_sign[32];
            char tx_grid[10];
            char rx_grid[10];
            double tx_power;
            time_t timestamp;
        };

        std::string m_tx_sign;
        time_t m_start_time;
        Band m_band;
        std::vector<struct WSPRTelemetry> m_telemetry;
        void save_cache();
        bool check_cache(const std::string& data, std::string& telemetry_str);
        void wspr_live_update();
        void load_new_telemetry(std::istream& input);
        void load_telemetry(std::istream& input);


};



void wspr_tracker (ScreenFrame& panel, ScreenFrame& map);
