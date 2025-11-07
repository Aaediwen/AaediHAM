#include "../aaediclock.h"

//3809-KINDEX: Fetched Sources23833:PSK: MQTT Message arrived	topic: pskr/filter/v2/10m/FT8/K1KPC/AE7U/EM77/CN87/291/291
//message: {"sq":62168950971,"f":28075532,"md":"FT8","rp":-16,"t":1762451636,"sc":"K1KPC","sl":"EM77AU","rc":"AE7U","rl":"CN87XI","sa":291,"ra":291,"b":"10m"}
struct psk_spot {
        uint64_t	sequence;
        uint64_t	frequency;
        std::string	mode;
        int16_t		report;
        time_t		timestamp;
        std::string	tx_call;
        std::string	rx_call;
        std::string	tx_loc;
        std::string 	rx_loc;
        struct GeoCoord tx_geo;
        struct GeoCoord rx_geo;
        uint32_t	tx_dxcc;
        uint32_t	rx_dxcc;
        std::string	band;
};

void psk_reporter(ScreenFrame& panel);
void psk_cleanup();