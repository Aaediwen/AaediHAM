#include "aaediclock.h"
#include "utils/http_fetch.h"
#include "contests.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <mutex>
#include <libxml/tree.h>

SDL_TimerID contest_timer = 0;
std::vector<struct contest> contest_feed;
std::mutex contest_mutex;
aaediclock_host_api* host_api = nullptr;

struct contest temp;
bool in_item = false;
void parse_contests(xmlNode* start_node) {
	xmlNode* current_node = nullptr;
//	const std::lock_guard<std::mutex>contest_lock(contest_mutex);
	for (current_node = start_node; current_node; current_node = current_node->next) {
		if (current_node->type == XML_ELEMENT_NODE) {
			std::string NodeName(reinterpret_cast<const char*>(current_node->name));
			//*(host_api->AaediHAM_LogDebug) << "XML Node Name: "<< NodeName << "\n";
			std::transform(NodeName.begin(), NodeName.end(), NodeName.begin(), ::tolower);
			if ((NodeName == "rss") || NodeName == "channel") {
				if (current_node->children) {
					parse_contests(current_node->children);
				}
			}
			if (NodeName == "item") {
				temp.title.clear();
				temp.link.clear();
				temp.description.clear();
				temp.guid.clear();
				in_item = true;
				if (current_node->children) {
					parse_contests(current_node->children);

					//*(host_api->AaediHAM_LogDebug) << "Parsed Contest ITEM tag " << temp.title << "\n";
					in_item = false;
					bool found = false;
					if (!contest_feed.empty()) {
						for (const auto& contest : contest_feed) {
							if (contest.guid == temp.guid) {
					//			*(host_api->AaediHAM_LogDebug) << "found existing contest guid: " << contest.guid << "\n";
								found = true;
								break;
							}
						 }
					}
					if (!found) {
						 contest_feed.push_back(temp);
					//	*(host_api->AaediHAM_LogDebug) << "adding contest entry " << temp.title << "\n";
					}
				}
			} else if (NodeName == "title") {
				if (in_item) {
					std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
					temp.title = xml_content;
				}
			} else if (NodeName == "link") {
				if (in_item) {
					std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
					temp.link = xml_content;
				}
			} else if (NodeName == "description") {
				if (in_item) {
					std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
					temp.description = xml_content;
				}
			} else if (NodeName == "guid") {
				if (in_item) {
					std::string xml_content(reinterpret_cast<const char*>(xmlNodeGetContent(current_node)));
					temp.guid = xml_content;
				}
			}
		}
	}
}

int SDLCALL fetch_contests (void* data) {
	(void)data;
	char* fetch_spots = 0 ;
	Uint64 data_size = 0;
	bool file_valid = false;
	std::fstream disk_file;
	std::string full_cache_path = host_api->AaediHAM_ConfigGetCachePath();
	full_cache_path += "contests.cache";
	if (contest_feed.empty()) {
		std::string error_string;
		*(host_api->AaediHAM_LogDebug) <<"Reading Contests from WA7BNM Disk Cache via timer\n";
		// 6 hour max cache age
		data_size = disk_cache_read (full_cache_path, (void**)&fetch_spots, 6 * HR_NS, error_string);
		if (data_size == 0) {
			*(host_api->AaediHAM_LogDebug) <<"Cache Result: " << error_string << "\n";
		} else {
			file_valid = true;
		}
	}
	if (!file_valid) {
		std::string web_source = host_api->AaediHAM_ConfigGetSiteCache();
		if (web_source.empty()) {
			web_source = "https://www.contestcalendar.com/calendar.rss";
		} else {
			web_source += "contests.rss";
		}

		*(host_api->AaediHAM_LogDebug) <<"Fetching Contests from WA7BNM via timer\n";
		*(host_api->AaediHAM_LogUser) << "Fetching contests from WA7BNM via timer\n";
		struct http_payload payload;
		payload.source_url = web_source;
		payload.result = (void**)&fetch_spots;
		data_size = http_loader(payload);
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
			*(host_api->AaediHAM_LogDebug) << "Failed to parse Context Feed XML\n";
		} else {
		*(host_api->AaediHAM_LogDebug) << "Parsing contest feed\n";
		const std::lock_guard<std::mutex>contest_lock(contest_mutex);
		parse_contests(xmlDocGetRootElement(xml_tree));
		xmlFreeDoc (xml_tree);
		xml_tree = nullptr;
		}
		  //parse_contests(fetch_spots);
	}
	if(fetch_spots) {
	     free (fetch_spots);
	     fetch_spots=0;
	}
	return 0;
}


Uint32 SDLCALL fetch_contests (void *userdata, SDL_TimerID timerID, Uint32 interval) {
	(void)interval;
	(void)userdata;
	if (timerID) {
		SDL_Thread* thread = SDL_CreateThread(fetch_contests, "Contest Fetcher", nullptr);
		if (thread) {
			SDL_DetachThread(thread);
		} else {
			*(host_api->AaediHAM_LogDebug) << "Failed to Create Contest Fetch Thread\n";
		}
		return (3600000); // 1 Hr
	} else {
		return 0;
	}
}


extern "C" DllExport aaediclock_plugin_api* createPlugin() {
	return new contest_plugin();
}
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* target) {
	if (target) {
		delete target;
	}
}

void contest_plugin::plugin_init() const {
	if (!contest_timer) {
		contest_timer = SDL_AddTimer(30, fetch_contests, NULL);
	}
	return;
}

void contest_plugin::plugin_exit() const {
	if (contest_timer) {
		SDL_RemoveTimer(contest_timer);
	}
	return;
}
size_t contest_page[2]={0,2};
bool cycle = true;

void contest_plugin::plugin_main(const aaediclock_FRect& dims) const {
	//    const size_t contest_start = contest_page[0]*15;
	const int page_size = 7;
	if ((dims.h < 10) || (dims.w < 10)) {
	    return;
	}
	size_t contest_index = 0;
	struct plugin_mouse_event mouse_event = host_api->AaediHAM_GetMouseEvent();
	if (mouse_event.valid) {
	    SDL_Log ("Click event in Contest module at %f, %f", mouse_event.coords.x, mouse_event.coords.y);
	    cycle = !cycle;
	    if (!cycle) {
	         contest_page[0]=0;
	    }
	}

	aaediclock_FRect TextRect;
	aaediclock_Color title_color;
	aaediclock_Color sched_color = {128, 128, 0, 255};
	 if (cycle) {
	     title_color = {0, 128, 128, 255};
	 } else {
	     title_color = {0, 128, 200, 255};
	 }

	 host_api->AaediHAM_GraphicsClear();

	 float unitx = dims.w/20;
	 float unity = dims.h/15;
	 TextRect.w=unitx*18;
	 TextRect.h=unity;
	 TextRect.x=unitx/2;
	 TextRect.y=2;
	 host_api->AaediHAM_GraphicsDrawText("WA7BNM Contest Calendar", title_color, TextRect);
	 TextRect.y += unity;
	//     const std::lock_guard<std::mutex>contest_lock(contest_mutex);
	if (!contest_feed.empty()) {
		for (const auto& contest : contest_feed) {
			if ((TextRect.y <= unity*15) && (contest_index >= contest_page[0]*page_size) && (contest_index<(contest_page[0]*page_size)+page_size)) {
				TextRect.x = 2;
				TextRect.w = unitx*15;
				if (!contest.title.empty()) {
					host_api->AaediHAM_GraphicsDrawText(contest.title.c_str(), title_color, TextRect);
				}
				TextRect.y += unity;
				TextRect.x = unitx*7;
				TextRect.w = unitx*12;
				if (!contest.description.empty()) {
					std::string tempdesc = contest.description;
					size_t resize_point = tempdesc.size();
					if (resize_point > 38) { resize_point = 38; }
					if (resize_point > 20) {
						TextRect.x = unitx*3;
						TextRect.w = unitx*16;
					}
					tempdesc.resize(resize_point);
					host_api->AaediHAM_GraphicsDrawText(tempdesc.c_str(), sched_color, TextRect);
				}
				TextRect.y += unity;
			}
			contest_index++;
		}
		if (cycle) {
			contest_page[1]++;
			if (contest_page[1] > 5) {
				contest_page[0]++;
				contest_page[1]=0;
				if (contest_page[0] > (contest_feed.size()/page_size)) {
					contest_page[0]=0;
				}
			}
		}
	} else {
		*(host_api->AaediHAM_LogDebug) << "Empty Contest Feed\n";
	}

	return;
}

const char* contest_plugin::getName() const {
	return "Contest Module";
}

void contest_plugin::set_host(aaediclock_host_api* host) {
	host_api = host;
}

