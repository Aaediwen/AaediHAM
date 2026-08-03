#include "muf.h"
#include "utils/http_fetch.h"
#include <fstream>
#include <algorithm>
#include <array>
#include <libxml/tree.h>


SDL_TimerID n0nbh_timer = 0;
aaediclock_host_api* host_api = nullptr;

enum bands {
	BAND_80_40,
	BAND_30_20,
	BAND_17_15,
	BAND_12_10,
	BAND_ENULL
};

enum condition {
	GOOD,
	FAIR,
	POOR,
	OPEN,
	CLOSED
};

struct band_conditions {
	enum bands id;
	enum condition day;
	enum condition night;
};

enum VHFLocations {
	AURORA,
	SKIP_NA,
	SKIP_EU_2M,
	SKIP_EU_4M,
	SKIP_EU_6M,
	VHF_ENULL
};

std::array<struct band_conditions,4>HF_Conditions;
std::array<std::string,5>VHF_Conditions;


void parse_n0nbh(xmlNode* start_node) {
	xmlNode* current_node = nullptr;
	for (current_node = start_node; current_node; current_node = current_node->next) {
		if (current_node->type == XML_ELEMENT_NODE) {
			std::string NodeName(reinterpret_cast<const char*>(current_node->name));
		//	*(host_api->AaediHAM_LogDebug) << "XML Node Name: "<< NodeName << "\n";
			std::transform(NodeName.begin(), NodeName.end(), NodeName.begin(), ::tolower);
			if ((NodeName == "solar") || NodeName == "solardata" || 
				NodeName == "calculatedconditions" || NodeName == "calculatedvhfconditions") {
				parse_n0nbh(current_node->children);
			} else if (NodeName == "band") {
				std::string xml_content;
				xmlChar* attribute;
				xmlNodeGetAttrValue(current_node, (const xmlChar*)"name", NULL, &attribute);
				enum bands band = bands::BAND_ENULL;
				if (attribute) {
					xml_content = reinterpret_cast<const char*>(attribute);
			
					if (xml_content == "80m-40m") {
						band = bands::BAND_80_40;
					} else if (xml_content == "30m-20m") {
						band = bands::BAND_30_20;
					} else if (xml_content == "17m-15m") {
						band = bands::BAND_17_15;
					} else if (xml_content == "12m-10m") {
						band = bands::BAND_12_10;
					}

                             /*
				MUF Module: 80m-40m     day     Poor
				MUF Module: 30m-20m     day     Good
				MUF Module: 17m-15m     day     Fair
				MUF Module: 12m-10m     day     Poor
				MUF Module: 80m-40m     night   Fair
				MUF Module: 30m-20m     night   Good
				MUF Module: 17m-15m     night   Fair
				MUF Module: 12m-10m     night   Poor
				MUF Module: vhf-aurora  northern_hemi   Band Closed
				MUF Module: E-Skip      europe  Band Closed
				MUF Module: E-Skip      north_america   Band Closed
				MUF Module: E-Skip      europe_6m       Band Closed
				MUF Module: E-Skip      europe_4m       Band Closed

				*/
					xml_content.clear();
					free(attribute);
					attribute = nullptr;
				}
				xmlNodeGetAttrValue(current_node, (const xmlChar*)"time", NULL, &attribute);
				xmlChar* xmlRaw;
				if (attribute) {
					xml_content = reinterpret_cast<const char*>(attribute);
					free(attribute);
					attribute = nullptr;
				}
				bool time = (xml_content == "day");
				xmlRaw = xmlNodeGetContent(current_node);
				if (xmlRaw) {
					xml_content = reinterpret_cast<const char*>(xmlRaw);
					xmlFree(xmlRaw);
				}
				enum condition band_cond;
				if (!xml_content.empty()) {
					if (xml_content == "Good") {
						band_cond = condition::GOOD;
					} else if (xml_content == "Fair") {
						band_cond = condition::FAIR;
					} else {
						band_cond = condition::POOR;
					}
					if (band != bands::BAND_ENULL) {
						if (time) {
							HF_Conditions[band].day = band_cond;
						} else {
							HF_Conditions[band].night = band_cond;
						}
						HF_Conditions[band].id=band;
					}
	//                         	*(host_api->AaediHAM_LogDebug) << band << "\t" << time << "\t" << condition << "\n";
				}

			} else if (NodeName == "phenomenon") {
				std::string xml_content;
				xmlChar* attribute;
				xmlNodeGetAttrValue(current_node, (const xmlChar*)"name", NULL, &attribute);
				if (attribute) {
					std::string attr_name = reinterpret_cast<const char*>(attribute);
					free(attribute);
					attribute = nullptr;
				}
				xmlNodeGetAttrValue(current_node, (const xmlChar*)"location", NULL, &attribute);
				enum VHFLocations location = VHFLocations::VHF_ENULL;
				if (attribute) {
					xml_content = reinterpret_cast<const char*>(attribute);
					if (xml_content == "northern_hemi") {
						location = VHFLocations::AURORA;
					} else if (xml_content == "europe") {
						location = VHFLocations::SKIP_EU_2M;
					} else if (xml_content == "europe_4m") {
						location = VHFLocations::SKIP_EU_4M;
					} else if (xml_content == "europe_6m") {
						location = VHFLocations::SKIP_EU_6M;
					}  else if (xml_content == "north_america") {
						location = VHFLocations::SKIP_NA;
					}
					free(attribute);
					attribute = nullptr;
				}
				if (location != VHFLocations::VHF_ENULL) {
					xmlChar* xmlRaw;
					xmlRaw = xmlNodeGetContent(current_node);
					if (xmlRaw) {
						VHF_Conditions[location] = reinterpret_cast<const char*>(xmlRaw);
						xmlFree(xmlRaw);
					}
				}
			}
		}
	}
}

int SDLCALL fetch_HamQSL (void* data) {
	(void)data;
	char* fetch_spots = 0 ;
	Uint64 data_size = 0;
	bool file_valid = false;
	std::fstream disk_file;
	SDL_PathInfo fileinfo;
	std::string full_cache_path = host_api->AaediHAM_ConfigGetCachePath();
	full_cache_path += "n0nbh.cache";
	std::string error_string;
	data_size = disk_cache_read (full_cache_path, (void**)&fetch_spots, 3 * HR_NS, error_string);
	if (data_size == 0) {
		*(host_api->AaediHAM_LogDebug) <<"Cache Result: " << error_string << "\n";
	} else {
		*(host_api->AaediHAM_LogDebug) <<"Reading Band Data from N0NBH Disk Cache via timer\n";
		file_valid = true;
	}
	if (!file_valid) {
		*(host_api->AaediHAM_LogDebug) <<"Fetching Contests from HamQSO (N0NBH) via timer\n";
		SDL_Log("Fetching contests from HamQSL (N0NBH) via timer");
		data_size = http_loader("https://www.hamqsl.com/solarxml.php", (void**)&fetch_spots);
	}
	if (data_size) {
		if (!file_valid) {
			disk_file.open(full_cache_path.c_str(), (std::fstream::binary | std::fstream::out | std::fstream::trunc));
			if (disk_file.is_open()) {
				disk_file.write(fetch_spots, data_size);
				if (!disk_file.good()) {
					*(host_api->AaediHAM_LogDebug) << "Cache write failed\n";
				}
			}
			disk_file.close();
		}
		xmlDocPtr xml_tree = 0;
		xml_tree = xmlReadMemory(fetch_spots, static_cast<int>(data_size), nullptr, nullptr, 0);
		if (!xml_tree) {
			*(host_api->AaediHAM_LogDebug) << "Failed to parse Cond Condition XML\n";
		} else {
			parse_n0nbh(xmlDocGetRootElement(xml_tree));
			xmlFreeDoc (xml_tree);
			xml_tree = nullptr;
		}
	}
	if(fetch_spots) {
		free (fetch_spots);
		fetch_spots=0;
	}
	return 0;
}

Uint32 SDLCALL fetch_HamQSL (void *userdata, SDL_TimerID timerID, Uint32 interval) {
	(void)interval;
	(void)userdata;
	if (timerID) {
		SDL_Thread* thread = SDL_CreateThread(fetch_HamQSL, "HamQSL Fetcher", nullptr);
		if (thread) {
			SDL_DetachThread(thread);
		} else {
			*(host_api->AaediHAM_LogDebug) << "Failed to Create HamQSL Fetch Thread\n";
		}
		return (10800000); // 3 Hrs
	} else {
		return 0;
	}
}



extern "C" DllExport aaediclock_plugin_api* createPlugin() {
	return new muf_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
	if (target) {
        	delete target;
	}
}

void muf_plugin::plugin_init() const {
	if (!n0nbh_timer) {
		n0nbh_timer = SDL_AddTimer(30, fetch_HamQSL, NULL);
	}
	return;
}

void muf_plugin::plugin_exit() const {
	if (n0nbh_timer) {
		SDL_RemoveTimer(n0nbh_timer);
	}
	return;
}

void muf_plugin::plugin_main(const aaediclock_FRect& dims) const {
	host_api->AaediHAM_GraphicsClear();
	aaediclock_Color good, fair, poor, header;
	good = {0,255,0,255};
	fair = {255,255,0,255};
	poor = {255, 0,0,255};
	header = {128,128,255,255};
	float text_height = dims.h/15;
	aaediclock_FRect TextRect;
	TextRect.x=2;
	TextRect.y=2;
	TextRect.h=text_height;

	TextRect.w=(dims.w)-4;
	host_api->AaediHAM_GraphicsDrawText("HAMQSL Conditions", header, TextRect);
	TextRect.y += text_height;
	TextRect.w=(dims.w)/3;
	host_api->AaediHAM_GraphicsDrawText("BAND", header, TextRect);
	TextRect.x = (dims.w)/2;
	TextRect.w = (dims.w)/5;
	host_api->AaediHAM_GraphicsDrawText("Day", header, TextRect);
	TextRect.x = ((dims.w)/4)*3;
	host_api->AaediHAM_GraphicsDrawText("Night", header, TextRect);
	TextRect.y += text_height;
	for (auto& band : HF_Conditions) {
		TextRect.x=2;
		TextRect.h=text_height;
		TextRect.w=(dims.w)/3;
		std::string band_string;
		if (band.id == bands::BAND_80_40) {
			band_string = "80M-40M";
		} else if (band.id == bands::BAND_30_20) {
			band_string = "30M-20M";
		} else if (band.id == bands::BAND_17_15) {
			band_string = "17M-15M";
		} else if (band.id == bands::BAND_12_10) {
			band_string = "12M-10M";
		}
		host_api->AaediHAM_GraphicsDrawText(band_string.c_str(), header, TextRect);
		host_api->AaediHAM_SetTarget();
		TextRect.x = (dims.w)/2;
		TextRect.w = (dims.w)/5;
		aaediclock_Color bar_color;
		switch (band.day) {
			case (condition::GOOD):
				bar_color = good;
				break;
			case (condition::FAIR):
				bar_color = fair;
				break;
			case (condition::POOR):
				bar_color = poor;
		}
		host_api->AaediHAM_GraphicsDrawRect (bar_color, TextRect, 1);

		TextRect.x = ((dims.w)/4)*3;
		switch (band.night) {
			case (condition::GOOD):
				bar_color = good;
				break;
			case (condition::FAIR):
				bar_color = fair;
				break;
			case (condition::POOR):
				bar_color = poor;
		}
		host_api->AaediHAM_GraphicsDrawRect (bar_color, TextRect, 1);
		TextRect.y += text_height;
	}
	TextRect.w=(dims.w)-4;
	TextRect.x=2;
	TextRect.y += text_height;
	host_api->AaediHAM_GraphicsDrawText("VHF Conditions", header, TextRect);
	TextRect.y += text_height;
	for (uint8_t vhf = 0 ; vhf < 5 ; vhf++) {
		TextRect.x=2;
		TextRect.h=text_height;
		TextRect.w=(dims.w)/4;
		/*
		if (xml_content == "northern_hemi") {
			location = VHFLocations::AURORA;
		} else if (xml_content == "europe") {
			location = VHFLocations::SKIP_EU_2M;
		} else if (xml_content == "europe_4m") {
			location = VHFLocations::SKIP_EU_4M;
		} else if (xml_content == "europe_6m") {
			location = VHFLocations::SKIP_EU_6M;
		}  else if (xml_content == "north_america") {
			location = VHFLocations::SKIP_NA;
		}
		*/
		std::string location_string;
		if (vhf == VHFLocations::AURORA) {
			location_string = "AURORA";
		} else if (vhf == VHFLocations::SKIP_EU_2M) {
			location_string = "EU 2M";
		} else if (vhf == VHFLocations::SKIP_EU_4M) {
			location_string = "EU 4M";
		} else if (vhf == VHFLocations::SKIP_EU_6M) {
			location_string = "EU 6M";
		} else if (vhf == VHFLocations::SKIP_NA) {
			location_string = "NOR AM";
		}
		host_api->AaediHAM_GraphicsDrawText(location_string.c_str(), header, TextRect);
		TextRect.x = (dims.w)/3;
		host_api->AaediHAM_GraphicsDrawText(VHF_Conditions[vhf].c_str(), good, TextRect);
		TextRect.y += text_height;
	}
}

const char* muf_plugin::getName() const {
	return "MUF Module";
}

void muf_plugin::set_host(aaediclock_host_api* host) {
	host_api = host;
}

