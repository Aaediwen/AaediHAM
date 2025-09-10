
double time_to_julian(time_t& t) {

    return (t / 86400000) + 2440587.5;
}