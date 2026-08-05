#include "sat_tracker.h"
#include <ctime>
#include <algorithm>
#include <vector>
#include <fstream>
#include <SDL3_image/SDL_image.h>
#include <libxml/tree.h>
#ifdef _WIN32
#include <time.h>
#define timegm _mkgmtime
#endif
#include "utils/http_fetch.h"
#include "utils/conversions.h"

aaediclock_host_api* host_api = nullptr;
SDL_TimerID sat_timer = 0;
int fetch_result = 0;
uint16_t icon = 0;
std::mutex sat_tracker_mutex;
std::vector<OMMRecord> satlist;
bool fetch_active = false;

/*
    Note: here so far I am only adjusting using GMST to convert between Celestial and Terrestrial reference frames
    Because of this, there could be up to 1 degree of error introduced here
    in order to refine this, need to add Precession, Nuation, and polar motion vectors as well
    This could cause targetting error if using to directly target a satellite.

    For now, we are probably fine within most pixel resolution

*/
double get_GMST_rad(const time_t time) {
	constexpr double JD_UNIX_EPOCH              = 2440587.5;    // offset to 01-01-1970 (2440587.5)
	constexpr double SECONDS_PER_DAY            = 86400.0;      // seconds per calendar day
	constexpr double J2000                      = 2451545.0;    // Julian Date of J2000.0, which is January 1, 2000 at 12:00 TT
	double jd =  (static_cast<double>(time) / SECONDS_PER_DAY) + JD_UNIX_EPOCH; // convert to Julian date
	double d = jd - J2000;
	double GMST = fmod(280.46061837 + 360.98564736629 * d, 360.0);
	if (GMST < 0) GMST += 360.0;
	GMST *= M_PI/180.0;
	return GMST;
}

void apply_GMST (time_t time, const double RA_Rad, const double DEC_Rad, double& lat, double& lon) {
	if (time ==0) {
		return;
	}
	double GMST = get_GMST_rad(time);
	double lon_subsat = RA_Rad - GMST ;  // Earth-fixed longitude
	if (lon_subsat >  M_PI) lon_subsat -= 2*M_PI;
	if (lon_subsat < -M_PI) lon_subsat += 2*M_PI;

	double lat_subsat = DEC_Rad;
	// convert result to degrees
	lat = lat_subsat*180.0/M_PI;
	lon = lon_subsat*180.0/M_PI;

	return;
}

struct vector3 transform(const struct vector3 input, const struct vector3 matrix[3]) {
	struct vector3 result = {};
	if (!matrix) {
		result = input;
		return result;
	}
	result.x =  input.x*matrix[0].x;
	result.x += input.y*matrix[0].y;
	result.x += input.z*matrix[0].z;

	result.y =  input.x*matrix[1].x;
	result.y += input.y*matrix[1].y;
	result.y += input.z*matrix[1].z;

	result.z =  input.x*matrix[2].x;
	result.z += input.y*matrix[2].y;
	result.z += input.z*matrix[2].z;

	return result;
}

bool OMMRecord::sgp4_init() {
	const double epoch_1950 =  SGP4Parser::ISO8601_to_Julian("1950-01-01T00:00:00.0000");
	    //    const double epoch_J2000 = SGP4Parser::ISO8601_to_Julian("2000-01-01T12:00:00.0000");
	if (!valid) {
		*(host_api->AaediHAM_LogDebug) << "SGP4 Init called with incomplete ephemeris\n";
		return false;;
	}
	    //    *(host_api->AaediHAM_LogDebug) << "Vallado SGP4 test for " << name  << "\n";
	*(host_api->AaediHAM_LogDebug)
	<< "SGP4 Inputs:"
	<< " epoch1950=" << (epoch_jd - epoch_1950)
	<< " bstar="     << bstar
	<< " ecc="       << eccentricity
	<< " argp="      << arg_pericenter
	<< " inc="       << inclination
	<< " M="         << mean_anomaly
	<< " n="         << mean_motion
	<< " raan="      << right_ascension_of_ascending_node
	<< "\n";
	    //                Convert mean motion to rad/min for init per SGP4.cpp line 2328 example
	char norad_temp[6];
	size_t start = (norad_id.size() > 5) ? (norad_id.size() - 5) : 0;
	strncpy(norad_temp, (norad_id.c_str()+start),5);
	norad_temp[5]=0;
	double no_rad_min = mean_motion*2 * M_PI/(86400/60);
	bool initresult = SGP4Funcs::sgp4init(wgs72,
	                    'i',
	                    norad_temp,
	                    (epoch_jd-epoch_1950),
	                    bstar,
	                    mean_motion_dot,
	                    mean_motion_ddot,
	                    eccentricity,
	                    arg_pericenter,
	                    inclination,
	                    mean_anomaly,
	                    no_rad_min,
	                    right_ascension_of_ascending_node,
	                    satrec
	                    );
	if (!initresult) {
		valid = false;
		 *(host_api->AaediHAM_LogDebug) << "SGP4 Init error for " << name << "satrec error: " << satrec.error << "\n";
		return false;
	}
	*(host_api->AaediHAM_LogDebug)
	<< "satrec:"
	<< " a=" << satrec.a
	<< " altp=" << satrec.altp
	<< " alta=" << satrec.alta
	<< " ecco=" << satrec.ecco
	<< " inclo=" << satrec.inclo
	<< " no_kozai=" << satrec.no_kozai
	<< " no_unkozai=" << satrec.no_unkozai
	<< " nm=" << satrec.nm
	<< " period_sec=" << satrec.period_sec
	<< " radiusearthkm=" << satrec.radiusearthkm
	<< "\n";

	*(host_api->AaediHAM_LogDebug)
	<< "Mean Motion:"
	<< " input=" << mean_motion
	<< " no_kozai=" << satrec.no_kozai
	<< " no_unkozai=" << satrec.no_unkozai
	<< " nm=" << satrec.nm
	<< " mdot=" << satrec.mdot
	<< "\n";
	valid = true;
	return true;
}


bool OMMRecord::generate_telemetry(int resolution_min) {
	double velocity[3];
	double position[3];
	if (epoch_jd == 0) {
	    return false;
	}
	if (!valid) {
	    *(host_api->AaediHAM_LogDebug) << "Invalid Satellite Elements\n";
	    return false;
	}
	if (resolution_min <= 0) {
	    *(host_api->AaediHAM_LogDebug) << "Invalid Telemetry resolution\n";
	    return false;
	}
	if (mean_motion <=0) {
	    *(host_api->AaediHAM_LogDebug) << "Invalid Mean Motion\n";
	    return false;
	}
	// Step 1, generate the timestamps to use (tsince and timestamp)
	*(host_api->AaediHAM_LogDebug) << "Sat Epoch JD: " <<  epoch_jd;
	time_t timestamp = time(NULL); // unmolested timestamp to use
	double tsince = static_cast<double>(timestamp);
	*(host_api->AaediHAM_LogDebug) << "\t Now Unix Time: " <<  tsince;
	// divide Unix Time by seconds per day(86400), and adjust offset to 01-01-1970 (2440587.5)
	double jd = (tsince / 86400.0) + 2440587.5;
	    //    *(host_api->AaediHAM_LogDebug) << "\t Now JD: " <<  jd << "\n";
	tsince = jd - epoch_jd;
	    //    *(host_api->AaediHAM_LogDebug) << "\t Tsince Days: " <<  tsince;
	tsince *= (24*60);
	    //    *(host_api->AaediHAM_LogDebug) << "\t Tsince: " <<  tsince << "\n";


	struct vector3 raw_coords;

	// step 4: figure out how many samples we need to cover one orbit
	double period = ((1440*60)/mean_motion)/60; // period in minutes
	int sample_count = static_cast<int>(floor(period/resolution_min));
	telemetry.clear();
	struct aaediclock_dx DE;
	DE = host_api->AaediHAM_ConfigGetDE();
	DE.lat *= M_PI/180.0;
	DE.lon *= M_PI/180.0;
	double Re = 6371.0;
	for (int c = 0 ; c < sample_count ; c++) {
	    // build the new telemetry data here
	    struct SatTelemetry new_telemetry;
	    // run Vallado's SGP4 code to get sat location in TEME
	    // this also returns velocity which is currently not used

	    // the input data is TEME
	    // Understanding so far is using a Geocentric reference
	    // Caresian X == intersecting line between Equitorial plane and celestial plane (+ faces sun at Sprint Equinox)
	    // cartesian Z Relative to Celestial plane + == North or Up
	    // Cartesian Y == Lateral movement perpendicular to X and Z, Positive to East
	    // Celestial X is Up/Down from surface reference
	    // Celestial Y is E/W from surface reference
	    // Celestial Z is N/S from sirface reference
	    // Declination is relative to Celestial Plane
	    // RA is Eastward from Cartesian X Positive Axis

	    // ultimate target is Geodetic terrestrial latitude and Longitude of the sub sat location

	    /*
	       this code treats TEME as ECI as GCRF
	       this is *technically* incorrect, and probably needs to be refined later
	       However, based on Vallado P 233 table 3-5,
	       it looks like this introduces up to half a KM of error.
	       At the scales we are operating here for display and maybe even for tracking
	       This is likely a good starting point.

	       Keep in mind that this then compounds later with the  error
	       when converting from GCRF to ITRF because the later only adjusts for GMST

	       This will need better refinement later, but may still be good enough for tracking
	       It'll almost certainly be good enough for the display right now.
	    */


	    bool sgp4_result = SGP4Funcs::sgp4(satrec, tsince, position, velocity);
	    if (!sgp4_result) {
	        *(host_api->AaediHAM_LogDebug) << "Invalid Telemetry result\n";
	        return false;
	    }
	    struct vector3 matrix[3];

	    // Approximate altitude in Km
	    double magnitude = sqrt( (position[0]*position[0]) + (position[1]*position[1]) + (position[2]*position[2]));
	    new_telemetry.alt 		= magnitude - Re;

	    // Declination and Right Ascention in Inertial Space, stored in Radians
	    new_telemetry.dec 		= atan2(position[2], sqrt(position[0]*position[0]+position[1]*position[1]));
	    new_telemetry.ra 		= atan2(position[1], position[0]);
	//      new_telemetry.dec *= (180/M_PI);
	//      new_telemetry.ra *= (180/M_PI);
	  // set the time data for the telemetry point
	  new_telemetry.timestamp 	= timestamp;
	  new_telemetry.tsince		= tsince;
	  // generate subsat location for ground track
	  apply_GMST (new_telemetry.timestamp, new_telemetry.ra, new_telemetry.dec, new_telemetry.lat, new_telemetry.lon);
	  // now to calculate the Azimuth and Elevation relative to DE
	  // convert from TEME to ECEF by applying GMST transform
	  double GMST =  get_GMST_rad(new_telemetry.timestamp);
	  matrix[0] = {cos(GMST), sin(GMST), 0.0};
	  matrix[1] = {0.0-sin(GMST), cos(GMST), 0.0};
	  matrix[2] = {0.0, 0.0, 1.0};
	  raw_coords.x=position[0];
	  raw_coords.y=position[1];
	  raw_coords.z=position[2];
	  struct vector3 sat_ecef = transform(raw_coords, matrix);

	  // convert the coordinate reference frame from ECEF to SEZ/NEZ relative to DE
	  /*
	Original manually multiplied rotation matrix
	      matrix[0] = {cos(DE.lat)*cos(DE.lon), cos(DE.lat)*(0.0-sin(DE.lon)), sin(DE.lat)};
	      matrix[1] = {sin(DE.lon), cos(DE.lon), 0.0};
	      matrix[2] = {(0.0-sin(DE.lat))*cos(DE.lon), (0.0-sin(DE.lat))*(0.0-sin(DE.lon)), cos(DE.lat)};

	    what X, Y, and Z mean is changing along with the rotation.
	    So the labeling changes as well.
	    A raw 3d rotational matrix assumes no change in the name
	    of what is X, Y, Z. But since those are changing roles,
	    the matrix has to be applied in a different order to reflect that

	    in ECEF, Z points at the North Pole, and X points to the vernal equinox.
	    From the observer's perspective, those are North and Up respectively
	    (so Z becomes Y and X becomes Z, which also means Y must become X)
	    that change must be accounted for
	    X matrix[0] (Vernal Equinox)  --> Z 2 (Up)
	    Y matrix[1] (East) --> X 0 (East)
	    Z matrix[2] (North Pole) --> Y 1 (Local North)

	    maybe one day I'll understand why the sign change on sin(DE.lon) has to exist
	    */


	    // rotate the context so that Z' points along the vector from Earth Center to DE
	    // we only actually change sat_local here, because after this, DE is considered to be 0,0,0

	    // generate the rotation matrix from DE
	    double slat = sin(DE.lat);
	    double clat = cos(DE.lat);
	    double slon = sin(DE.lon);
	    double clon = cos(DE.lon);

	    matrix[0] = {0.0-slon, clon, 0.0};
	    //           slon, clon, 0.0 // old matrix 1
	    matrix[1] = {0.0-slat * clon, 0.0-slat * slon, clat};
	    //	     -slat * clon, -slat * -slon, clat // old matrix 2
	    matrix[2] = {clat * clon, clat * slon, slat};
	    //           clat * clon, clat* -slon, slat // old matrix 0

	    // apply it to the satellite
	    struct vector3 sat_local = transform(sat_ecef, matrix);

	    // translate the origin along Z' (Original ECEF Z no longer exists) to place the origin at DE
	    sat_local.z -= Re;

	    // calculate Azimuth and Elevation
	    /*
	      Az = atan2(x, y)
	      El = atan2(z, sqrt(x²+y²))
	    */
	    new_telemetry.azimuth = (atan2(sat_local.x, sat_local.y))*180.0/M_PI;
	    new_telemetry.elevation = (atan2(sat_local.z, sqrt((sat_local.x*sat_local.x)+(sat_local.y*sat_local.y))))*180.0/M_PI;

	    telemetry.push_back(new_telemetry);
	    // prep for the next telemetry point
	    tsince += resolution_min;
	    timestamp += 60*resolution_min;
	}
	return true;
}

time_t OMMRecord::telemetry_age() {
    if (telemetry.empty()) {
        return 0;
    } else {
        return (telemetry.back().timestamp);
    }
}

void OMMRecord::draw_pass(const time_t pass_start, const time_t pass_end,  std::vector<aaediclock_FPoint> *pass_pts, const aaediclock_FRect *size) {
    // generate the pass line for a satellite pass
    // this loads that line's coordinates into pass_pts for the caller to render
    if (!pass_pts) {
        *(host_api->AaediHAM_LogDebug) << "No Pass Points store submitted\n";
        return;
    }
    pass_pts->clear();
    if (telemetry.empty()) {
        *(host_api->AaediHAM_LogDebug) << "No Telemetry Present. No pass\n";
        return;
    }
    if (!size) {
        *(host_api->AaediHAM_LogDebug) << "Invalid Window size\n";
        return;
    }
    if (size->w <=2 || size->h <=2) {
        *(host_api->AaediHAM_LogDebug) << "Invalid Window size\n";
        return;
    }
    float max_radius = size->w/2;
    if (size->h < size->w) {
        max_radius = size->h/2;
    }
    aaediclock_FPoint center, new_point;
    center.x = (size->w/2)+size->x;
    center.y = (size->h/2)+size->y;
    for (const SatTelemetry& point : this->telemetry) {
        if ((point.timestamp >= pass_start) && (point.timestamp <= pass_end)) {
            float radius = max_radius * (1- static_cast<float>(point.elevation)/90.0f);
            new_point.x = center.x + radius * sinf(static_cast<float>(point.azimuth)*(static_cast<float>(M_PI)/180.0f));
            new_point.y = center.y - radius * cosf(static_cast<float>(point.azimuth)*(static_cast<float>(M_PI)/180.0f));
//            *(host_api->AaediHAM_LogDebug) << "SAT_TRACKER: AZ: " << point.azimuth << ", EL: " << point.elevation << " Radius " << radius << "\n";
            pass_pts->push_back(new_point);
        }
    }
    return;
}

void OMMRecord::location (aaediclock_FPoint *result) {
	// get the current lat/lon over which the satellite currently is
	if (!result) {
	    *(host_api->AaediHAM_LogDebug) << "No Result passed\n";
	    return;
	}
	if (!valid) {
	    *(host_api->AaediHAM_LogDebug) << "Invalid Telemetry Data\n";
	    return;
	}
	double velocity[3];
	double position[3];
	time_t timestamp = time(NULL); // unmolested timestamp to use
	double tsince = static_cast<double>(timestamp);

	double jd = (tsince / 86400.0) + 2440587.5;
	tsince = jd - epoch_jd;
	tsince *= (24*60);

	SGP4Funcs::sgp4(satrec, tsince, position, velocity);
	double dec = atan2(position[2], sqrt(position[0]*position[0]+position[1]*position[1]));
	double ra = atan2(position[1], position[0]);

	double lat = 0;
	double lon = 0;
	apply_GMST (timestamp, ra, dec, lat, lon);
	result->x = static_cast<float>(lat);
	result->y = static_cast<float>(lon);
	return ;
}

time_t OMMRecord::pass_start() {
	// get the start time for a satellite pass
	if (telemetry.empty()) {
	    return 0;
	}
	for (const SatTelemetry& point : this->telemetry) {
	    if (point.elevation >0) {
	        return point.timestamp;
	    }
	}
	return 0;
}

time_t OMMRecord::pass_end() {
	// get the end time for a satellite pass
	if (telemetry.empty()) {
	    return 0;
	}
	bool started_flag=false;
	for (const SatTelemetry& point : this->telemetry) {
	    if (point.elevation >0) {
	        started_flag = true;
	    }
	    if (started_flag && point.elevation < 0) {
	        return point.timestamp;
	    }
	}
	if (started_flag) {
	    return (this->telemetry.back().timestamp);
	} else {
	    return 0;
	}
}



// XML function to handle an individual XML node
void SGP4Parser::XML::process_node(xmlNode* start_node, OMMRecord& result) {
    if (!start_node) {
        return;
    }
    xmlNode* current_node = nullptr;
    aaediclock_Color trackcols = {255,0,0,255};
//    const double epoch_1950 =  ISO8601_to_Julian("1950-01-01T00:00:00.0000");
//    const double epoch_J2000 = ISO8601_to_Julian("2000-01-01T12:00:00.0000");
//    *(host_api->AaediHAM_LogDebug) << "Epoch 1950: " << epoch_1950 << " \n";
//    *(host_api->AaediHAM_LogDebug) << "J2000: " << epoch_J2000 << " \n";
//    printf("%.10f\n", epoch_1950);
//    *(host_api->AaediHAM_LogDebug) << "J2000: " << epoch_J2000 << " \n";
//    printf("1900-01-01: %.0f\n",
//    ISO8601_to_Julian("1900-01-01T00:00:00"));
//    printf("Unix Epoch: %.0f\n",
//    ISO8601_to_Julian("1970-01-01T00:00:00"));
//    printf("%.10f\n", epoch_J2000);
    for (current_node = start_node; current_node; current_node = current_node->next) {
        std::string xml_content;
        xmlChar* xml_raw = 0;
        xml_content.clear();
        if (current_node->type == XML_ELEMENT_NODE) {
            std::string NodeName(reinterpret_cast<const char*>(current_node->name));
            std::transform(NodeName.begin(), NodeName.end(), NodeName.begin(), ::tolower);
            if (NodeName == "omm") {
                if (!result.name.empty()) {
                    result.valid = true;
                }
                if (result.valid) {
                    trackcols.r -= 20;
                    trackcols.g += 20;
                    trackcols.b += 10;
                    int sat_count = host_api->AaediHAM_ConfigGetSatCount();
                    for (int x = 0 ; x < sat_count ; x++) {
                        std::string instring;
                        instring = result.name;
                        const std::string stropt = host_api->AaediHAM_ConfigGetSat(x);
                        if (instring.compare(0,stropt.length(),stropt)==0) {
                            result.color = trackcols;
                            satlist.push_back(result);
                            satlist.back().sgp4_init();
                            satlist.back().generate_telemetry(1);
                        }
                    }
                }
                result = {};
                process_node(current_node->children, result);
            }  else if (NodeName == "object_name") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    try {
                        result.name =  reinterpret_cast<const char*>(xml_raw);
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Satellite Name: " << e.what() << "\n";
                        result.name = "UNKNOWN";
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            } else if (NodeName == "object_id") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    try {
                        result.object_id =  reinterpret_cast<const char*>(xml_raw);
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Object ID: " << e.what() << "\n";
                        result.object_id="UNKNOWN";
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            } else if (NodeName == "norad_cat_id") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    try {
                        xml_content = reinterpret_cast<const char*>(xml_raw);
                        result.norad_id =  xml_content;
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid NORAD Catalog ID: " << e.what() << "\n";
                        result.norad_id = "00000";
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            } else if (NodeName == "element_set_no") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    try {
                        xml_content = reinterpret_cast<const char*>(xml_raw);
                        result.element_set_no =  std::stoul(xml_content);
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Element Set Number: " << e.what() << "\n";
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }  else if (NodeName == "classification_type") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    xml_content = reinterpret_cast<const char*>(xml_raw);
                    if (!xml_content.empty()) {
                        result.class_type =  xml_content.front();
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }  else if (NodeName == "epoch") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    result.epoch_raw = reinterpret_cast<const char*>(xml_raw);
                    result.epoch_jd =  ISO8601_to_Julian(result.epoch_raw);
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }  else if (NodeName == "mean_motion") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    try {
                        xml_content = reinterpret_cast<const char*>(xml_raw);
                        result.mean_motion =  std::stod(xml_content);
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Mean Motion: " << e.what() << "\n";
                        result.valid = false;
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
//                	double conversion = result.mean_motion * 2 * M_PI / 86400; // convert to rad/s
//                	result.mean_motion = conversion;
//                	double conversion = result.mean_motion * 360;	// convert to deg/day
//                	result.mean_motion = conversion;
                }
            }  else if (NodeName == "eccentricity") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    try {
                        xml_content = reinterpret_cast<const char*>(xml_raw);
                        result.eccentricity =  std::stod(xml_content);
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Eccentricity: " << e.what() << "\n";
                        result.valid = false;
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }  else if (NodeName == "inclination") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    xml_content = reinterpret_cast<const char*>(xml_raw);
                    try {
                        result.inclination =  std::stod(xml_content);
                        double conversion = result.inclination * M_PI / 180.0;
                        result.inclination = conversion;
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Inclination: " << e.what() << "\n";
                        result.valid = false;
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }  else if (NodeName == "ra_of_asc_node") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    xml_content = reinterpret_cast<const char*>(xml_raw);
                    try {
                        result.right_ascension_of_ascending_node =  std::stod(xml_content);
                        double conversion = result.right_ascension_of_ascending_node * M_PI / 180.0;
                        result.right_ascension_of_ascending_node = conversion;
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid RA of Ascending Node: " << e.what() << "\n";
                        result.valid = false;
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }   else if (NodeName == "arg_of_pericenter") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    xml_content = reinterpret_cast<const char*>(xml_raw);
                    try {
                        result.arg_pericenter =  std::stod(xml_content);
                        double conversion = result.arg_pericenter * M_PI / 180.0;
                        result.arg_pericenter = conversion;
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Argument of Pericenter: " << e.what() << "\n";
                        result.valid = false;
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }   else if (NodeName == "mean_anomaly") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    xml_content = reinterpret_cast<const char*>(xml_raw);
                    try {
                        result.mean_anomaly =  std::stod(xml_content);
                        double conversion = result.mean_anomaly * M_PI / 180.0;
                        result.mean_anomaly = conversion;
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Mean Anomaly: " << e.what() << "\n";
                        result.valid = false;
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }   else if (NodeName == "ephemeris_type") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    xml_content = reinterpret_cast<const char*>(xml_raw);
                    try {
                        result.ephemeris_type =  std::stoi(xml_content);
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Ephemeris Type: " << e.what() << "\n";
                        result.valid = false;
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }   else if (NodeName == "revolution_at_epoch") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    xml_content = reinterpret_cast<const char*>(xml_raw);
                    try {
                        result.revolution_at_epoch =  std::stoul(xml_content);
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Revolution: " << e.what() << "\n";
                        result.valid = false;
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }   else if (NodeName == "bstar") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    xml_content = reinterpret_cast<const char*>(xml_raw);
                    try {
                        result.bstar =  std::stod(xml_content);
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid B-Star: " << e.what() << "\n";
                        result.valid = false;
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }   else if (NodeName == "mean_motion_dot") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    xml_content = reinterpret_cast<const char*>(xml_raw);
                    try {
                        result.mean_motion_dot =  std::stod(xml_content);
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Mean Motion DOT: " << e.what() << "\n";
                        result.valid = false;
                    }
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            }   else if (NodeName == "mean_motion_ddot") {
                xml_raw = xmlNodeGetContent(current_node);
                if (xml_raw) {
                    try {
                        xml_content = reinterpret_cast<const char*>(xml_raw);
                    } catch (std::exception &e) {
                        *(host_api->AaediHAM_LogDebug) << "Invalid Mean Motion DDOT: " << e.what() << "\n";
                        result.valid = false;
                    }
                    result.mean_motion_ddot =  std::stod(xml_content);
                    xmlFree(xml_raw);
                    xml_raw = nullptr;
                }
            } else {
                // FIX ME!!!!
                // right now this ASSUMES LibXML2 won't give me a bad pointer for children
                if (current_node->children) {
                    process_node(current_node->children, result);
                }
            }
        }
        // clean up
        xml_content.clear();
        if (xml_raw) {
            xmlFree(xml_raw);
            xml_raw = nullptr;
        }
    }
    return;
}

double SGP4Parser::ISO8601_to_Julian(const std::string input) {
    // variable init
    double result = 0;
    size_t consumed = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    int doy = 0;
    int hour = 0;
    int minute = 0;
    double second = 0.0;
    if (input.empty()) {
        return 0;
    }
    // here we are just using this struct tm as a container to work in
    // think of it like a magnetic screw tray for time bits
    struct tm epoch_calc        =       {};
    std::string tempstr;

    // -- basic sanity checks --
    if (input.size() < 14) {
        // to short for a date
        return 0;
    }
    size_t t_index = input.find("T");
    if (t_index == std::string::npos || t_index < 8 || t_index > 12) {
        // invalid time
        return 0;
    }
    *(host_api->AaediHAM_LogDebug) << "ISO8601 Input: " << input << "\n";
    // -- extract the date --
    // extract the year
    size_t int_sanity = 0;
    tempstr = input.substr(0,4);
    try {
        year = std::stoi(tempstr.c_str(),&int_sanity, 10);
    } catch (std::exception& e) {
        (void)e;
        *(host_api->AaediHAM_LogDebug) << "Invalid Epoch Year "<< tempstr <<"\n";
        return 0;
    }
    if (year <1 || int_sanity <4) {
        *(host_api->AaediHAM_LogDebug) << "Invalid Epoch Year" << tempstr <<"\n";
        return 0;
    }
    consumed = int_sanity+1;
    if (consumed > (input.size()-4)) {
        *(host_api->AaediHAM_LogDebug) << "Invalid Epoch Year" << tempstr <<"\n";
        return 0;
    }
    // check for MM-DD vs DDD
    tempstr= input.substr (consumed, 4);
    size_t format_flag = tempstr.find("-");
    if (format_flag == std::string::npos) {

        // YYYY-DDD format
        tempstr = input.substr(consumed,3);
        try {
            // extract DOY
            doy = std::stoi(tempstr.c_str(),&int_sanity, 10);
            doy--;
            consumed +=int_sanity;
            if (int_sanity >3) {
                *(host_api->AaediHAM_LogDebug) << "Invalid Day of Year\n";
                return 0;
            }
            month=1;
            day = 1;
            // convert to MM-DD
            doy_to_mmdd(year, doy, &(month), &(day));
        } catch (std::exception& e) {
            (void)e;
            *(host_api->AaediHAM_LogDebug) << "Invalid Epoch Day of year\n";
        }
    } else {

        // YYYY-MM-DD format
        tempstr = input.substr(consumed,2);
        try {
            // extract month
            month = std::stoi(tempstr.c_str(),&int_sanity, 10);
  //          epoch_calc.tm_mon--;
            if (int_sanity >2) {
                *(host_api->AaediHAM_LogDebug) << "Invalid Epoch Month\n";
                return 0;
            }
            consumed +=int_sanity+1;
            // extract DOM
            tempstr = input.substr(consumed,2);
            day = std::stoi(tempstr.c_str(),&int_sanity, 10);
            consumed +=int_sanity;
            if (int_sanity >2) {
                *(host_api->AaediHAM_LogDebug) << "Invalid Epoch Day\n";
                return 0;
            }
        } catch (std::exception& e) {
            (void)e;
            *(host_api->AaediHAM_LogDebug) << "Invalid Epoch Month or Day\n";
            return 0;
        }
    }

    // -- extract the time --
    consumed = t_index+1;

    // extract hour
    tempstr = input.substr(consumed,2);
    try {
        hour = std::stoi(tempstr.c_str(),&int_sanity, 10);
        consumed +=int_sanity+1;
        if (int_sanity != 2) {
                *(host_api->AaediHAM_LogDebug) << "Invalid hour \n";
                return 0;
            }
    } catch (std::exception& e) {
        (void)e;
        *(host_api->AaediHAM_LogDebug) << "Invalid Epoch Hour\n";
        return 0;
    }
    // extract minute
    tempstr = input.substr(consumed,2);
    try {
        minute = std::stoi(tempstr.c_str(),&int_sanity, 10);
        consumed +=int_sanity+1;
        if (int_sanity != 2) {
                *(host_api->AaediHAM_LogDebug) << "Invalid minute\n";
                return 0;
        }
    } catch (std::exception& e) {
        (void)e;
        *(host_api->AaediHAM_LogDebug) << "Invalid Epoch minute\n";
        return 0;
    }
    // extract second
    tempstr = input.substr(consumed);
    try {
        second = std::stod(tempstr.c_str(),&int_sanity);
    } catch (std::exception& e) {
        (void)e;
        *(host_api->AaediHAM_LogDebug) << "Invalid Epoch second\n";
        return 0;
    }
    // convert to Julian date
    *(host_api->AaediHAM_LogDebug) <<"Extracted Time: " << year << "-" << month << "-" << day << "   " << hour << ":" << minute << ":" << second << "\n";
    result = meeusJD (year, month, day, hour, minute, second);
    *(host_api->AaediHAM_LogDebug) << "converted result: " << result << "\n";
    return result;
}


bool SGP4Parser::fromXML(std::string& input) {
	// prep the XML Tree
	input.erase(std::remove(input.begin(), input.end(), '\r'), input.end());
	if (input.empty()) {
		return false;
	}
	xmlDocPtr xml_tree = 0;
	xml_tree = xmlReadMemory(input.data(), static_cast<int>(input.size()), nullptr, nullptr, 0);
	if (!xml_tree) {
		*(host_api->AaediHAM_LogDebug) << "Failed to parse Satellite XML\n";
		return false;
	} else {
		// holding area for the OMM Record
		OMMRecord result;
		SGP4Parser::XML::process_node(xmlDocGetRootElement(xml_tree), result);
		xmlFreeDoc (xml_tree);
		xml_tree = nullptr;
		return true;
	}
}

void SGP4Parser::CSV::parse_headers(const std::string& input, std::vector<std::string>& headers) {
	headers.clear();
	if (input.empty()) {
		*(host_api->AaediHAM_LogDebug) << "Empty CSV header line\n";
		return;
	}
	if (input.find (',') == std::string::npos) {
		*(host_api->AaediHAM_LogDebug) << "Not a  CSV header line\n";
		return;

	}
	size_t consumed = 0; 
	size_t position = 0;
	std::string header_name;
	*(host_api->AaediHAM_LogDebug) << "parsing CSV header line\n";

	while (position != std::string::npos) {
		position = input.find (',',consumed);
		if (position != std::string::npos) {
			header_name = input.substr(consumed, position-consumed);
		} else {
			header_name = input.substr(consumed);

		}
		consumed += header_name.size()+1;
		if (!header_name.empty()) {
			headers.push_back(header_name);
			*(host_api->AaediHAM_LogDebug) << "Header: "<< header_name << "\n";
		}
	}
	*(host_api->AaediHAM_LogDebug) << "done parsing CSV header line\n";

	return;
}

bool SGP4Parser::fromCSV(std::string& input) {
	std::vector<std::string> headers;
	size_t consumed = 0;
	input.erase(std::remove(input.begin(), input.end(), '\r'), input.end());
	if (input.empty()) {
		*(host_api->AaediHAM_LogDebug) << "Empty CSV\n";
		return false;
	}
	std::istringstream csv_stream(input);
	std::string csv_line;
	std::getline(csv_stream, csv_line);
	if (csv_line.empty()) {
		*(host_api->AaediHAM_LogDebug) << "Empty CSV, bad first line\n";
		return false;
	}
	SGP4Parser::CSV::parse_headers(csv_line, headers);
	if (headers.empty()) {
		*(host_api->AaediHAM_LogDebug) << "Error parsing CSV headers\n";
		return false;
	}
	consumed += csv_line.size();
	aaediclock_Color trackcols = {255,0,0,255};

	while (!csv_line.empty()) {
		OMMRecord result;
		result = {};

		std::getline(csv_stream, csv_line);
		if (!csv_line.empty()) {
			std::string content_buffer;
			std::vector<std::string>fields;
			fields.clear();
			size_t consumed = 0;
			size_t position = 0;
			std::string field_buffer;
			// explode the row into an array matching the header fields
			while (position != std::string::npos) {
				position = csv_line.find (',',consumed);
				if (position != std::string::npos) {
					field_buffer = csv_line.substr(consumed, position-consumed);
				} else {
					field_buffer = csv_line.substr(consumed);

				}
				consumed += field_buffer.size()+1;
				if (field_buffer.size() > 1024) {
					field_buffer.resize(1024);
					*(host_api->AaediHAM_LogDebug) << "Oversize field buffer!!";
				}
				fields.push_back(field_buffer);
			}
			// go through headers matching with the fields
			for (size_t header_index = 0 ; header_index < headers.size(); header_index++) {
				if (header_index < fields.size()) {
					if (headers[header_index] == "OBJECT_NAME") {
						result.name = fields[header_index];
						if (result.name.empty()) {
							result.name = "UNKNOWN";
						}
					} else if (headers[header_index] == "OBJECT_ID") {
 						result.object_id =  fields[header_index];
						if (result.object_id.empty()) {
							result.object_id = "UNKNOWN";
						}
					} else if (headers[header_index] == "EPOCH") {
						result.epoch_raw = fields[header_index];
						result.epoch_jd =  ISO8601_to_Julian(result.epoch_raw);
					} else if (headers[header_index] == "MEAN_MOTION") {
						try {
			                        	content_buffer = fields[header_index];
							result.mean_motion =  std::stod(content_buffer);
						} catch (std::exception &e) {
							*(host_api->AaediHAM_LogDebug) << "Invalid Mean Motion: " << e.what() << "\n";
							result.valid = false;
						}
					} else if (headers[header_index] == "ECCENTRICITY") {
						try {
							content_buffer = fields[header_index];
							result.eccentricity =  std::stod(content_buffer);
						} catch (std::exception &e) {
							*(host_api->AaediHAM_LogDebug) << "Invalid Eccentricity: " << e.what() << "\n";
							result.valid = false;
						}
					} else if (headers[header_index] == "INCLINATION") {
						content_buffer = fields[header_index];
						try {
						    result.inclination =  std::stod(content_buffer);
								double conversion = result.inclination * M_PI / 180.0;
								result.inclination = conversion;
						} catch (std::exception &e) {
								*(host_api->AaediHAM_LogDebug) << "Invalid Inclination: " << e.what() << "\n";
								result.valid = false;
						}
					} else if (headers[header_index] == "RA_OF_ASC_NODE") {
						content_buffer = fields[header_index];
						try {
							result.right_ascension_of_ascending_node =  std::stod(content_buffer);
                        				double conversion = result.right_ascension_of_ascending_node * M_PI / 180.0;
                        				result.right_ascension_of_ascending_node = conversion;
        					} catch (std::exception &e) {
                        				*(host_api->AaediHAM_LogDebug) << "Invalid RA of Ascending Node: " << e.what() << "\n";
                        				result.valid = false;
                    				}
					} else if (headers[header_index] == "ARG_OF_PERICENTER") {
						content_buffer = fields[header_index];
						try {
							result.arg_pericenter =  std::stod(content_buffer);
							double conversion = result.arg_pericenter * M_PI / 180.0;
							result.arg_pericenter = conversion;
						} catch (std::exception &e) {
							*(host_api->AaediHAM_LogDebug) << "Invalid Argument of Pericenter: " << e.what() << "\n";
							result.valid = false;
						}

					} else if (headers[header_index] == "MEAN_ANOMALY") {
						content_buffer = fields[header_index];
						try {
							result.mean_anomaly =  std::stod(content_buffer);
							double conversion = result.mean_anomaly * M_PI / 180.0;
							result.mean_anomaly = conversion;
						} catch (std::exception &e) {
							*(host_api->AaediHAM_LogDebug) << "Invalid Mean Anomaly: " << e.what() << "\n";
							result.valid = false;
						}
					} else if (headers[header_index] == "EPHEMERIS_TYPE") {
						content_buffer = fields[header_index];
						try {
							result.ephemeris_type =  std::stoi(content_buffer);
						} catch (std::exception &e) {
							*(host_api->AaediHAM_LogDebug) << "Invalid Ephemeris Type: " << e.what() << "\n";
							result.valid = false;
						}
					} else if (headers[header_index] == "CLASSIFICATION_TYPE") {
						content_buffer = fields[header_index];
						if (!content_buffer.empty()) {
							result.class_type =  content_buffer.front();
						}
					} else if (headers[header_index] == "NORAD_CAT_ID") {
						result.norad_id = fields[header_index];
						if (result.norad_id.empty()) {
							*(host_api->AaediHAM_LogDebug) << "Invalid NORAD Catalog ID\n";
							result.norad_id = "00000";
						}
					} else if (headers[header_index] == "ELEMENT_SET_NO") {
						content_buffer = fields[header_index];
						try { 
							result.element_set_no =  std::stoul(content_buffer);

						} catch (std::exception &e) {
							*(host_api->AaediHAM_LogDebug) << "Invalid Element Set Number: " << e.what() << "\n";

						}
					} else if (headers[header_index] == "REV_AT_EPOCH") {
						content_buffer = fields[header_index];
						try {
							result.revolution_at_epoch =  std::stoul(content_buffer);
						} catch (std::exception &e) {
						    *(host_api->AaediHAM_LogDebug) << "Invalid Revolution: " << e.what() << "\n";
						    result.valid = false;
						}
					} else if (headers[header_index] == "BSTAR") {
						try {
							content_buffer = fields[header_index];
							result.bstar =  std::stod(content_buffer);
						} catch (std::exception &e) {
							*(host_api->AaediHAM_LogDebug) << "Invalid Eccentricity: " << e.what() << "\n";
							result.valid = false;
						}
					} else if (headers[header_index] == "MEAN_MOTION_DOT") {
						content_buffer = fields[header_index];
						try {
					 		result.mean_motion_dot =  std::stod(content_buffer);
						} catch (std::exception &e) {
							*(host_api->AaediHAM_LogDebug) << "Invalid Mean Motion DOT: " << e.what() << "\n";
							result.valid = false;
						}
					} else if (headers[header_index] == "MEAN_MOTION_DDOT") {
						content_buffer = fields[header_index];
						try {
					 		result.mean_motion_ddot =  std::stod(content_buffer);
						} catch (std::exception &e) {
							*(host_api->AaediHAM_LogDebug) << "Invalid Mean Motion DDOT: "<< content_buffer<<" -- " << e.what() << "\n";
							result.valid = false;
						}
					}
				}
			}

 			if (!result.name.empty()) {
				result.valid = true;
			}
			if (result.valid) {
				trackcols.r -= 20;
				trackcols.g += 20;
				trackcols.b += 10;
				int sat_count = host_api->AaediHAM_ConfigGetSatCount();
				for (int x = 0 ; x < sat_count ; x++) {
					std::string instring;
					instring = result.name;
					const std::string stropt = host_api->AaediHAM_ConfigGetSat(x);
					if (instring.compare(0,stropt.length(),stropt)==0) {
						result.color = trackcols;
						satlist.push_back(result);
						satlist.back().sgp4_init();
						satlist.back().generate_telemetry(1);
					}
				}
			}

		}

	}
	return true;
}





int SDLCALL fetch_celestrak(void* data) {
	(void) data;
	char* amateur_tle = 0 ;
	Uint64 data_size = 0;
	SDL_PathInfo fileinfo;
	std::string error_string;
	bool file_valid = false;
	std::fstream disk_file;
	std::string full_cache_path = host_api->AaediHAM_ConfigGetCachePath();
	full_cache_path += "celestrak.cache";
	data_size = disk_cache_read (full_cache_path, (void**)&amateur_tle, 6 * HR_NS, error_string);
	if (data_size == 0) {
 		*(host_api->AaediHAM_LogDebug) <<"Cache Result: " << error_string << "\n";
 	} else {
 		file_valid = true;
 	}
    if (!file_valid) {
        SDL_Log ("Fetching Satellite telemetry from Celestrak");
        *(host_api->AaediHAM_LogDebug) << "Fetching Satellite telemetry from Celestrak\n";

        if (amateur_tle) {
            free(amateur_tle);
            amateur_tle = nullptr;
        }

	std::string web_source = host_api->AaediHAM_ConfigGetSiteCache();
	if (web_source.empty()) {
//		std::cout << "Empty Local Site Cache path\n";
		web_source = "https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=csv";
	} else {
//		web_source += "celestrak.xml";
	web_source += "celestrak.csv";

//		std::cout << "Using Local Site Cache Path "<< web_source << "\n";
	}
	struct http_payload payload;
	payload.source_url = web_source;
	payload.result = (void**)&amateur_tle;
	payload.timeout_s = 60;
	data_size = http_loader(payload);
	if (payload.http_code != 200) {
		 *(host_api->AaediHAM_LogDebug) << "Celestrak Fetch returned HTTP code " << payload.http_code << "\n";
		std::cout << "Celestrak Fetch for HTTP code " << payload.http_code << ", Unable to get new data. Killing fetch loop.\n";
		fetch_result = payload.http_code;
		data_size = 0;
	}
//        data_size = http_loader(web_source.c_str(), (void**)&amateur_tle, 60);   //
        *(host_api->AaediHAM_LogDebug) << "Celestrak Fetch returned\n";
        if (data_size > 256) {
            disk_file.open(full_cache_path.c_str(), (std::fstream::binary | std::fstream::out | std::fstream::trunc));
            if (disk_file.is_open()) {
                disk_file.write(amateur_tle, data_size);
                if (!disk_file.good()) {
                     *(host_api->AaediHAM_LogDebug) << "Cache write failed\n";

                }
            }
            disk_file.close();
        }
    }
    if (data_size > 256) {
        bool parse_result = false;
        *(host_api->AaediHAM_LogDebug) << "Fetched New Sat data\n";
//        const std::lock_guard<std::mutex>sat_lock(sat_tracker_mutex);
        satlist.clear();
        std::string raw_xml = amateur_tle;
	// XML detector
	bool is_xml = false;
	size_t start_bracket = 0;
	size_t stop_bracket = 0;
	size_t next_start = 0;
	start_bracket = raw_xml.find('<');
	if (start_bracket != std::string::npos) {
		next_start = raw_xml.find('<', start_bracket+1);
		stop_bracket =  raw_xml.find('>', start_bracket+1);
		if ((stop_bracket != std::string::npos) &&
		    (next_start != std::string::npos)   &&
		    (next_start > stop_bracket)) {
			*(host_api->AaediHAM_LogDebug) << "probably XML, Trying XML Parser\n";
			parse_result = SGP4Parser::fromXML(raw_xml);
			if (!parse_result || satlist.empty()) {
				is_xml = false;
			} else {
				is_xml = true;
			}

		} else {
			is_xml = false;
		}

	} else { is_xml = false; }
	// CSV handler
	if (!is_xml) {
		*(host_api->AaediHAM_LogDebug) << "Trying CSV Parser\n";
		parse_result = SGP4Parser::fromCSV(raw_xml);
		if (!parse_result) {
			*(host_api->AaediHAM_LogDebug) << "CSV Parse error\n";
		} else {
			*(host_api->AaediHAM_LogDebug) << "parsed CSV \n";
		}

	}

        if (amateur_tle) {
            free(amateur_tle);
            amateur_tle=0;
        }
        if (satlist.empty()) {
             *(host_api->AaediHAM_LogDebug) << "XML parsed but no valid satellites found\n";
        }
	if (fetch_result < 100) {
        	fetch_result = 2;
	}
    } // we got input data
    else {
        *(host_api->AaediHAM_LogDebug) << "No New Sat data from Celestrak\n";
	if (fetch_result < 100) {
        	fetch_result = 3;
	}
    }

    if (amateur_tle) {
        free(amateur_tle);
        amateur_tle = nullptr;
    }

    *(host_api->AaediHAM_LogDebug) << "Fetch returned "<< data_size <<" bytes\n";
    return 0;
}


Uint32 SDLCALL fetch_celestrak (void *userdata, SDL_TimerID timerID, Uint32 interval) {
	(void)userdata;
	(void)interval;
	if (timerID) {
		//const std::lock_guard<std::mutex>sat_lock(sat_tracker_mutex);
		fetch_result = 10;
		SDL_Thread* thread = SDL_CreateThread(fetch_celestrak, "Celestrak Fetcher", nullptr);
		if (thread) {
			SDL_DetachThread(thread);
		} else {
			*(host_api->AaediHAM_LogDebug) << "Failed to Create Sat Data Fetch Thread\n";
			fetch_result = 3;
		}
	}
	sat_timer = 0;
	return 0;
}

extern "C" DllExport aaediclock_plugin_api* createPlugin() {
	return new new_sat_tracker_plugin();
}

extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
	if (target) {
		delete target;
	}
}

void new_sat_tracker_plugin::plugin_init() const {
	return;
}

void new_sat_tracker_plugin::plugin_exit() const {
//    const std::lock_guard<std::mutex>sat_lock(sat_tracker_mutex);
	if (sat_timer) {
		SDL_RemoveTimer(sat_timer);
	}
	fetch_active = false;
//    if (host_api->AaediHAM_IconCheck(icon)) {
//        host_api->AaediHAM_IconDelete(icon);
//    }
	return;
}

void circle_helper(std::vector<aaediclock_FPoint> *circle_points, float radius, aaediclock_FPoint center, int segments) {
	for (int i = 0; i <= segments; ++i) {
		float theta = (2.0f * static_cast<float>(M_PI) * i) / segments;
		aaediclock_FPoint pt = {
		    center.x + radius * cosf(theta),
		    center.y + radius * sinf(theta)
		};
		circle_points->push_back(pt);
	}
	return;
}


void draw_pass_tracker(const aaediclock_FRect dims, OMMRecord& sat) {

    if (dims.w < 1 || dims.h < 1) {
        return;
    }
    if (!sat.valid) {
        return;
    }
    char tempstr[30];
    // clear the box
    host_api->AaediHAM_GraphicsClear();
    aaediclock_FRect TextRect;
    TextRect.w=dims.w/2;
    TextRect.h=dims.h/11;
    TextRect.x=5;
    TextRect.y=2;
    // render the satellite name
    host_api->AaediHAM_GraphicsDrawText(sat.name.c_str(), sat.color, TextRect);
    // if we have a pass coming, render its start and end times
    time_t pass_time = sat.pass_start();
    if (pass_time) {
        tm* test_time = new tm;
	TextRect.y=dims.h - (dims.h/11)-4;
        TextRect.w=dims.w /3;
        #ifdef _WIN32
            localtime_s(test_time, &pass_time);
        #else
            localtime_r(&pass_time, test_time);
        #endif
//        test_time = localtime(&pass_time);
        if (test_time) {
            strftime(tempstr, 12, "%H:%M", test_time);
            host_api->AaediHAM_GraphicsDrawText(tempstr, sat.color, TextRect);
        }

        TextRect.x=dims.w - (dims.w/3);
        pass_time = sat.pass_end();
        #ifdef _WIN32
            localtime_s(test_time, &pass_time);
        #else
            localtime_r(&pass_time, test_time);
        #endif
//        test_time = localtime(&pass_time);
        if (test_time) {
            strftime(tempstr, 12, "%H:%M", test_time);
            host_api->AaediHAM_GraphicsDrawText(tempstr, sat.color, TextRect);
        }
    }
    // render the crosshairs and target pass chart
    host_api->AaediHAM_SetTarget();
    std::vector<aaediclock_FPoint> circle_pts;
    float radius = dims.w/2;
    if (dims.h < dims.w) {
        radius = dims.h/2;
    }
    radius *= 0.8f;
    std::vector<aaediclock_FPoint> pass_pts;
    aaediclock_FRect pass_box;
    pass_box.x=(dims.w - (2*radius))/2;
    pass_box.y=(dims.h - (2*radius))/2;
    pass_box.w=2*radius;
    pass_box.h=2*radius;

    // cross hairs
    aaediclock_Color draw_color = aaediclock_Color{64, 64, 0 ,255};
    aaediclock_FRect line;
    aaediclock_FPoint center = aaediclock_FPoint{dims.w/2, dims.h/2};
    line.x = center.x-radius;
    line.y = center.y;
    line.w = center.x+radius;
    line.h = center.y;
    host_api->AaediHAM_GraphicsDrawLine(draw_color, line);
    line.x = center.x;
    line.y = center.y-radius;
    line.w = center.x;
    line.h = center.y+radius;
    host_api->AaediHAM_GraphicsDrawLine(draw_color, line);

    // concentric degree circles
    draw_color = aaediclock_Color{64, 0, 64 ,255};
    circle_helper (&circle_pts, radius, center, 32);
    host_api->AaediHAM_GraphicsDrawLines(draw_color,circle_pts.data(), static_cast<int>(circle_pts.size()));
    circle_pts.clear();
    radius /=2;
    circle_helper (&circle_pts, radius, center, 32);
    host_api->AaediHAM_GraphicsDrawLines(draw_color,circle_pts.data(), static_cast<int>(circle_pts.size()));
    circle_pts.clear();
    radius /=2;
    circle_helper (&circle_pts, radius, center, 32);
    host_api->AaediHAM_GraphicsDrawLines(draw_color,circle_pts.data(), static_cast<int>(circle_pts.size()));
    circle_pts.clear();
    radius *=3;
    circle_helper (&circle_pts, radius, center, 32);
    host_api->AaediHAM_GraphicsDrawLines(draw_color,circle_pts.data(), static_cast<int>(circle_pts.size()));

    // draw the pass trajectory
    sat.draw_pass(sat.pass_start(), sat.pass_end(),  &pass_pts, &pass_box);
    draw_color = aaediclock_Color{64, 128, 0, 255};
    *(host_api->AaediHAM_LogDebug) << "Drawing pass of  " << pass_pts.size() << " points\n";
    host_api->AaediHAM_GraphicsDrawLines(draw_color, pass_pts.data(), static_cast<int>(pass_pts.size()));

    return;
}

void draw_sat_groundtrack(OMMRecord& sat, aaediclock_FRect& mapsize) {
	host_api->AaediHAM_OverlaySet(mapsize);
	if (sat.telemetry.empty()) {
		return;
	}
	aaediclock_FPoint* SDLPoints = (aaediclock_FPoint*)malloc(sizeof(aaediclock_FPoint)*sat.telemetry.size());
	if (!SDLPoints) {
		*(host_api->AaediHAM_LogDebug) << "Memory Allocation error drawing ground track \n";
		return;
	}
	int xt, yt;
	xt = static_cast<int>(mapsize.w);
	yt = static_cast<int>(mapsize.h);
	int render_size = 0;
	int index = 0;
	for (auto& telemetry_point : sat.telemetry ) {
		double lat = 0;
		double lon = 0;
		lat = telemetry_point.lat;
		lon = telemetry_point.lon;
		
		cords_to_px(lat, lon, xt, yt, &(SDLPoints[index]));
		if (telemetry_point.elevation >0) {
			aaediclock_Color draw_color = aaediclock_Color{0,0,128,255};
			aaediclock_FRect visirect = {SDLPoints[index].x, SDLPoints[index].y, 3.0, 3.0};
			host_api->AaediHAM_GraphicsDrawRect(draw_color, visirect, 1);
		}
		if (index >1) { // if we have a last point to compare to
			if (abs(SDLPoints[index-1].x - SDLPoints[index].x) > (xt/4)) {      // and the delta is greater than 100
				//render the current segment
				host_api->AaediHAM_GraphicsDrawLines(sat.color, SDLPoints, render_size-1);
				//reset the index
				index=0;
				render_size=1;
				// re-gen the current pixel
				lat = telemetry_point.lat;
				lon = telemetry_point.lon;
				cords_to_px(lat, lon, xt, yt, &(SDLPoints[index]));
			
			}
		}
		
		index++;
		render_size++;
	}
	if (render_size >1) {
	    host_api->AaediHAM_GraphicsDrawLines(sat.color, SDLPoints, render_size-1);
	}
	free (SDLPoints);
	return;
}


Uint16 pass_pager[2] = {0,0};
void new_sat_tracker_plugin::plugin_main(const aaediclock_FRect& dims) const {
	if (!sat_timer) {
		switch (fetch_result) {
			case 0:
			        sat_timer = SDL_AddTimer(10000, fetch_celestrak, NULL);
				fetch_active = true;
			        break;
			case 10:
			        break;
			case 2:
			        sat_timer = SDL_AddTimer(HR_MS * 12, fetch_celestrak, NULL);
				fetch_active = true;
			        break;
			case 3:
			        sat_timer = SDL_AddTimer(HR_MS * 4, fetch_celestrak, NULL);
				fetch_active = true;
			        break;
			default:
				fetch_active = false;
				if (sat_timer) {
					SDL_RemoveTimer(sat_timer);
				}
				fetch_active = false;
				break;
		}
	}
	host_api->AaediHAM_MapPinDelete();
	
	
	// clear the box
	host_api->AaediHAM_GraphicsClear();
	if (!fetch_active) {
		host_api->AaediHAM_GraphicsDrawRect (aaediclock_Color{255,0,0,128}, aaediclock_FRect{0,0,dims.w, dims.h}, 1);
		host_api->AaediHAM_GraphicsDrawText("CELESTRAK", aaediclock_Color{0,0,0,0}, aaediclock_FRect{2,2,dims.h/5,dims.w*0.9f});
		host_api->AaediHAM_GraphicsDrawText("HTTP ERROR", aaediclock_Color{0,0,0,0}, aaediclock_FRect{2,2+(dims.h/5),dims.h/5,dims.w*0.9f});
		char errstring[10];
		snprintf(errstring, 5, "%i", fetch_result);
		host_api->AaediHAM_GraphicsDrawText(errstring, aaediclock_Color{0,0,0,0}, aaediclock_FRect{2,2+(2*(dims.h/5)),dims.h/7,dims.w*0.5f});
		host_api->AaediHAM_GraphicsDrawText("Restart module", aaediclock_Color{0,0,0,0}, aaediclock_FRect{2,2+3*((dims.h/5)),dims.h/6,dims.w*0.9f});
		host_api->AaediHAM_GraphicsDrawText("when resolved", aaediclock_Color{0,0,0,0}, aaediclock_FRect{2,2+4*((dims.h/5)),dims.h/6,dims.w*0.9f});


	}
	aaediclock_FRect mapsize = host_api->AaediHAM_GetMapSize();
	mapsize.h /=2;
	mapsize.w /=2;
	bool redraw_flag = false;
	redraw_flag = (!host_api->AaediHAM_OverlayCheck());
//    aaediclock_FRect textrect;
//    textrect.x=2;

	const std::lock_guard<std::mutex>sat_lock(sat_tracker_mutex);
	if (!satlist.empty()) {
		if (pass_pager[0] >= satlist.size()) {
		    pass_pager[0]=0;
		}
		draw_pass_tracker(dims, satlist[pass_pager[0]]);
		if (pass_pager[1] >5) {
		    pass_pager[0]++;
		    pass_pager[1]=0;
		}
		pass_pager[1]++;
	}

//    textrect.w=dims.w/3;
//    textrect.h=dims.h/10;
//    textrect.y=2;
//    textrect.y=dims.h/10;
    host_api->AaediHAM_OverlayClear(aaediclock_Color{0,0,0,0});
    time_t time_now = time(NULL);
    if (!host_api->AaediHAM_IconCheck(icon)) {
        std::string asset_path = host_api->AaediHAM_ConfigGetAssetPath();
        asset_path += "satellite.png";
        SDL_Surface* loadsurface = IMG_Load(asset_path.c_str());
        if (loadsurface) {
             aaediclock_image new_icon;
             new_icon.width = loadsurface->w;
             new_icon.height = loadsurface->h;
             new_icon.pixels = static_cast<uint8_t*>(loadsurface->pixels);
             icon = host_api->AaediHAM_IconCreate(new_icon);
             SDL_DestroySurface(loadsurface);
        } else {
            icon = 0;
        }
    }
	for (auto& sat : satlist) {
		if ((sat.telemetry_age() - time_now) < 60) {
		    sat.generate_telemetry(1);
			// redraw_flag = true;
		}
		
		draw_sat_groundtrack(sat, mapsize);
		struct aaediclock_map_pin sat_pin;
		aaediclock_FPoint sat_loc;
		sat.location(&sat_loc);
		sat_pin.owner   =               0;
		memset (sat_pin.label,0,32);
		int length = sat.name.size();
		if (length > 31) {
		    length = 31;
		}
		memcpy(sat_pin.label, sat.name.c_str(), length);
		sat_pin.lat     =               sat_loc.x;
		sat_pin.lon     =               sat_loc.y;
		sat_pin.icon = 0;
		sat_pin.icon        =       icon;
		sat_pin.color   =               sat.color;
		sat_pin.tooltip[0]      =               0;
		host_api->AaediHAM_MapPinAdd(sat_pin);
		//  textrect.y += dims.h/10;
	}
}

const char* new_sat_tracker_plugin::getName() const {
    return "Sat Tracker Plugin";
}

void new_sat_tracker_plugin::set_host(aaediclock_host_api* host) {
    host_api = host;
}
