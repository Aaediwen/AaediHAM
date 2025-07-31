#include "../aaediclock.h"

struct beacon {
    char call[10];
    std::string location;
} const extern beacons[18];

const extern double beacon_freqs[5];

void ncdxf_module(ScreenFrame& panel);