#include "../aaediclock.h"

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




void dx_cluster (ScreenFrame& panel);