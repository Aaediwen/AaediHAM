#ifndef NCDXF_H

#include "../aaediclock.h"


#define NCDXF_H
struct beacon {
    char call[10];
    std::string location;
};

extern const struct beacon beacons[18];

extern const double beacon_freqs[5];

void ncdxf_module(ScreenFrame& panel);

#endif