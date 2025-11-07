#include "pskreporter.h"
#include "../aaediclock.h"
#include "../utils.h"
#include <iostream>
#include <sstream>
#include "MQTTClient.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

//https://retrieve.pskreporter.info/query?senderCallsign=requestedcall // appcontact=myemailaddress
// per https://groups.google.com/g/psk-reporter/c/iGbkpc9cpQ4
/*
Philip Gladstone
unread,
Jul 28, 2014, 8:10:32 AM
to psk-re...@googlegroups.com
That piece of the documentation is somewhat out of date. If you want to get what you are hearing, then you want

http://retrieve.pskreporter.info/query?receiverCallsign=k3uk&rronly=1
*/
// https://pskreporter.info/pskdev.html #Data Retrieval
//Users are encouraged to retrieve reception data no more often than once every five minutes.

//flowStartSeconds	A negative number of seconds to indicate how much data to retreive. This cannot be more than 24 hours.
// nolocator	If non zero, then include reception reports that do not include a locator.
// lastseqno	Limits search to records with a sequence number greater than or equal to this parameter. The last sequence number in the database is returned on each response.

/*
http://mqtt.pskreporter.info/
pskr/filter/v2/{band}/{mode}/{sendercall}/{receivercall}/{senderlocator}/{receiverlocator}/{sendercountry}/{receivercountry}
5BE","sa":291,"ra":291,"b":"15m"}
 mosquitto_sub -h mqtt.pskreporter.info -p 1883 -t "pskr/filter/v2/+/+/WB5XX/#"
{"sq":62145576626,"f":21074607,"md":"FT8","rp":17,"t":1762369046,"sc":"WB5XX","sl":"EM33qf01","rc":"W2JIT","rl":"FN23kd23","sa":291,"ra":291,"b":"15m"}
{"sq":62145576640,"f":21074595,"md":"FT8","rp":-13,"t":1762369016,"sc":"WB5XX","sl":"EM33qf01","rc":"KD3AN","rl":"EM66QH","sa":291,"ra":291,"b":"15m"}
{"sq":62145576654,"f":21074589,"md":"FT8","rp":-9,"t":1762369046,"sc":"WB5XX","sl":"EM33qf01","rc":"WT8P","rl":"CN97AN","sa":291,"ra":291,"b":"15m"}
{"sq":62145577269,"f":21074585,"md":"FT8","rp":-17,"t":1762369016,"sc":"WB5XX","sl":"EM33qf01","rc":"WA2TMC","rl":"FN03vg70cq","sa":291,"ra":291,"b":"15m"}
{"sq":62145577589,"f":21074604,"md":"FT8","rp":-10,"t":1762369046,"sc":"WB5XX","sl":"EM33qf01","rc":"KO4PC","rl":"EM63OI","sa":291,"ra":291,"b":"15m"}
{"sq":62145577935,"f":21074591,"md":"FT8","rp":-18,"t":1762369019,"sc":"WB5XX","sl":"EM33qf01","rc":"AE5FM","rl":"EM13OB","sa":291,"ra":291,"b":"15m"}
struct psk_spot {
        Uint32          sequence;
        Uint32          frequency;
        std::string     mode;
        std::string     report;
        time_t          timestamp;
        std::string     tx_call;
        std::string     rx_call;
        std::string     tx_loc;
        std::string     rc_loc;
        std::string     tx_country;
        std::string     rx_country;
        std::string     band;
};

https://github.com/eclipse-paho/paho.mqtt.c
*/
MQTTClient mqtt_client = 0;
const int max_age=1800;
SDL_Mutex* psk_mutex = nullptr;
std::vector<struct psk_spot>psk_reports;

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    debug_log << "PSK: MQTT Message arrived\t";
//    debug_log << "topic: "<< topicName;
//    debug_log << "\tmessage: " << (char*)(message->payload) << "\n";
    json psk_report;
    try {
        struct psk_spot new_spot;
        psk_report=json::parse((char*)(message->payload));
        if (psk_report.contains("sq") &&  psk_report["sq"].is_number()) {
              new_spot.sequence 	=	psk_report["sq"].get<uint64_t>();
        } else {
              new_spot.sequence 	= 	0;
        }
        if (psk_report.contains("f") &&  psk_report["f"].is_number()) {
              new_spot.frequency 	= 	psk_report["f"].get<uint64_t>();
        } else {
              new_spot.frequency	= 	0;
        }
        if (psk_report.contains("md")) {
              new_spot.mode		=	psk_report["md"].get<std::string>();
        } else {
              new_spot.mode		=	"";
        }
        if (psk_report.contains("rp") && psk_report["rp"].is_number()) {
              new_spot.report 		=	psk_report["rp"].get<int16_t>();
        } else {
              new_spot.report		=	0;
        }
        if (psk_report.contains("t") &&  psk_report["t"].is_number()) {
              new_spot.timestamp 	=	psk_report["t"].get<time_t>();
        } else {
              new_spot.timestamp	= 	0;
        }
        if (psk_report.contains("sc")) {
              new_spot.tx_call 		=	psk_report["sc"].get<std::string>();
        } else {
              new_spot.tx_call		=	"";
        }
        if (psk_report.contains("sc")) {
              new_spot.rx_call 		=	psk_report["rc"].get<std::string>();
        } else {
              new_spot.rx_call		=	"";
        }
        if (psk_report.contains("sl")) {
              new_spot.tx_loc 		=	psk_report["sl"].get<std::string>();
              new_spot.tx_geo		=	loc_to_geo(new_spot.tx_loc);
        } else {
              new_spot.tx_loc		=	"";
              new_spot.tx_geo		=	{0.0, 0.0};
        }
        if (psk_report.contains("rl")) {
              new_spot.rx_loc 		=	psk_report["rl"].get<std::string>();
              new_spot.rx_geo		=	loc_to_geo(new_spot.rx_loc);
        } else {
              new_spot.rx_loc		=	"";
              new_spot.rx_geo		=	{0.0, 0.0};
        }
        if (psk_report.contains("sa") &&  psk_report["sa"].is_number()) {
              new_spot.tx_dxcc 		=	psk_report["sa"].get<uint32_t>();
        } else {
              new_spot.tx_dxcc		=	0;
        }
        if (psk_report.contains("ra") &&  psk_report["ra"].is_number()) {
              new_spot.rx_dxcc 		=	psk_report["ra"].get<uint32_t>();
        } else {
              new_spot.rx_dxcc		=	0;
        }
        if (psk_report.contains("b")) {
              new_spot.band 		=	psk_report["b"].get<std::string>();
        } else {
              new_spot.band		=	"";
        }
        SDL_LockMutex(psk_mutex);
        SDL_Log("PSK Reporter adding contact %s", new_spot.rx_call.c_str());
        psk_reports.push_back(new_spot);

        SDL_UnlockMutex(psk_mutex);
    } catch (const json::parse_error &e) {
        debug_log << "PSK: Report JSON Parse Error " << strlen((char*)(message->payload)) << " bytes " << (char*)(message->payload) << "\n";
        return 0;
    }
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}


void connlost(void *context, char *cause)
{
    debug_log << "PSK: MQTT Connection lost";
    debug_log << "\tcause: "<< cause << "\n";
    mqtt_client = 0;
}

void psk_cleanup() {
     if (mqtt_client) {
          int rc;
          if ((rc = MQTTClient_disconnect(mqtt_client, 500)) != MQTTCLIENT_SUCCESS) {
               printf("Failed to disconnect, return code %d\n", rc);
               rc = EXIT_FAILURE;
          }
     }
     return;
}

void init_mqtt() {
     MQTTClient_init_options inits;
     strcpy (inits.struct_id, "MQTG");
     inits.struct_version = 0;
     inits.do_openssl_init = 0;
     MQTTClient_global_init (&inits);
     MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
     int rc;
     std::string mqtt_client_id = clockconfig.CallSign() + "-clock-Agent-";
     std::string mqtt_topic;
     if (clockconfig.PSKCall().empty()) {
         mqtt_topic = "pskr/filter/v2/+/+/"+clockconfig.CallSign()+"/#";
     } else {
         mqtt_topic = "pskr/filter/v2/+/+/"+clockconfig.PSKCall()+"/#";
     }
//     std::string mqtt_topic = "pskr/filter/v2/+/+/K1KPC/#";
     // create MQTT object
     if ((rc = MQTTClient_create(&mqtt_client, "mqtt.pskreporter.info", mqtt_client_id.c_str(),
        MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
             debug_log << "PSK: Unable to create MQTT client.\n";
             mqtt_client = 0;
             return;
     }
     // set callback functions
     if ((rc = MQTTClient_setCallbacks(mqtt_client, NULL, connlost, msgarrvd, NULL)) != MQTTCLIENT_SUCCESS) {
             debug_log << "PSK: Failed to set callbacks, return code " << rc << "\n";
             rc = EXIT_FAILURE;
             MQTTClient_destroy(&mqtt_client);
             mqtt_client = 0;
             return;
     }
     // connect to MQTT
     conn_opts.keepAliveInterval = 20;
     conn_opts.cleansession = 1;
     if ((rc = MQTTClient_connect(mqtt_client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
             debug_log << "PSK: MQTT Failed to connect, return code "<< rc << "\n";
             rc = EXIT_FAILURE;
             MQTTClient_destroy(&mqtt_client);
             mqtt_client = 0;
             return;
    }
    // subscribe
    debug_log << "PSK: Subscribing to topic -"<< mqtt_topic<<"- for client -"<< mqtt_client_id << "- \n";
    if ((rc = MQTTClient_subscribe(mqtt_client, mqtt_topic.c_str(), 0)) != MQTTCLIENT_SUCCESS)
    {
        debug_log << "PSK: Failed to subscribe, return code "<< rc << "\n";
        rc = EXIT_FAILURE;
        MQTTClient_destroy(&mqtt_client);
        mqtt_client = 0;
        return;

    }
}

void display_spot(ScreenFrame& panel, int y, int max_age,  struct psk_spot spot) {
    // add to screen list
       char tempstr[128];
       SDL_Color tempcolor={128,128,0,0};
       SDL_FRect TextRect;
       TextRect.x=2;
       TextRect.y= y * 1.0f;
       TextRect.w=panel.dims.w/4;
       TextRect.h=panel.dims.h/16;

       SDL_FRect age_rect;
       age_rect.h = TextRect.h/8;
       age_rect.y = y+((TextRect.h/8)*7);
       age_rect.x = 2;
       age_rect.w = (panel.dims.w-4)*(static_cast<float>(time(NULL)-spot.timestamp)/max_age);
       debug_log << "PSK: Spot age: "<< (time(NULL)-spot.timestamp) << " Seconds, Bar width: "<< age_rect.w<< " pixels\n";
       SDL_SetRenderTarget(panel.GetRenderer(), panel.texture);
       SDL_SetRenderDrawColor(panel.GetRenderer(), 128, 128, 0, 255);
       SDL_RenderFillRect(panel.GetRenderer(), &age_rect );
       SDL_SetRenderTarget(panel.GetRenderer(), NULL);
       panel.render_text(TextRect, Sans, tempcolor, spot.rx_call.c_str());
       TextRect.x += (panel.dims.w/4)+2;
       sprintf(tempstr, "%4.3f", (spot.frequency/1000000.0));
       panel.render_text(TextRect, Sans, tempcolor, tempstr);
       TextRect.x += (panel.dims.w/4)+2;
       TextRect.w /=2;
       if (spot.mode.size() >0) {
            panel.render_text(TextRect, Sans, tempcolor, spot.mode.c_str());
       }
       TextRect.x += (panel.dims.w/8)+2;
       TextRect.w *=2;
       panel.render_text(TextRect, Sans, tempcolor, spot.tx_call.c_str());
       TextRect.w += (panel.dims.w/4)-(panel.dims.w/20);
       TextRect.x += (panel.dims.w/8)+1;
       TextRect.y += ((panel.dims.h/11)+(panel.dims.h/150));
}


void psk_reporter(ScreenFrame& panel) {
    if (!psk_mutex) {
         psk_mutex = SDL_CreateMutex();
    }
    if (!mqtt_client) {
         init_mqtt();
    }
    time_t currenttime = time(NULL);
    SDL_LockMutex(psk_mutex);
    if (!psk_reports.empty()) {
         for (size_t c = psk_reports.size() ; c-- > 0 ;) {
              if ((currenttime - psk_reports[c].timestamp) > max_age) {
                   debug_log << "PSK: Erasing entry "<< psk_reports[c].rx_call.c_str() << "\n";
                   psk_reports.erase(psk_reports.begin()+c);
              }
         }
    }

    if (SDL_TryLockMutex(mutexes[MUTEX_RESIZE])) {
         SDL_UnlockMutex(mutexes[MUTEX_RESIZE]);
    } else {
         SDL_Log("PSK Reporter DRAW during resize event!");
         return;
    }

    panel.Clear();
    // display to panel
    panel.render_text(SDL_FRect{2,2, panel.dims.w, panel.dims.h/16}, Sans, SDL_Color{128,128,0,255}, "PSK Reporter");
    int y=2+panel.dims.h/16;
    size_t start = psk_reports.size() > 15 ? psk_reports.size() - 15 : 0;
    for (size_t n=start ; n < psk_reports.size(); n++) {
        if (y < panel.dims.h) {
//            dxspots[n].display_spot(panel, y, max_age);
            display_spot(panel, y, max_age,  psk_reports[n]);
            y+= panel.dims.h/16;
        }
    }



    // submit map pins
    delete_owner_pins(MOD_PSK);
    if (!psk_reports.empty()) {
        for (auto& current_spot : psk_reports) {
            if (current_spot.rx_geo.latitude && current_spot.rx_geo.longitude ) {
                struct map_pin psk_pin;
                psk_pin.owner       =               MOD_PSK;
                sprintf(psk_pin.label, "%s", current_spot.rx_call.c_str());
                psk_pin.lat         =               current_spot.rx_geo.latitude;
                psk_pin.lon         =               current_spot.rx_geo.longitude;
                psk_pin.icon        =               0;
                psk_pin.color           =            {128,128,0,255};
                psk_pin.tooltip[0]  =        0;
                add_pin(&psk_pin);

            }
        }

    }


    SDL_UnlockMutex(psk_mutex);
    return;
}