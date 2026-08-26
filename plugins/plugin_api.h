#ifndef AAEDICLOCK_API_H
#define AAEDICLOCK_API_H
#ifdef _WIN32
#define DllExport __declspec(dllexport)
#else
#define DllExport
#endif

#include <cstdint>
#include <sstream>
#include <deque>

static const uint8_t OVERLAY_BACKGROUND = 0;
static const uint8_t OVERLAY_BASE       = 1;
static const uint8_t OVERLAY_DEFAULT	= 1;
static const uint8_t OVERLAY_FOREGROUND = 2;
constexpr uint64_t HR_NS = 3600000000000;
constexpr uint32_t HR_MS = 3600000000;
struct aaediclock_FRect {
    float x;
    float y;
    float h;
    float w;
};

struct aaediclock_FPoint {
    float x;
    float y;
};

struct aaediclock_Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct aaediclock_map_pin {
    int owner;
    double lat;
    double lon;
    uint16_t icon;
    aaediclock_Color color;
    char label[32];
    char tooltip[512];
};

struct aaediclock_image {
    // image width, height, and RGBA format
    uint16_t width;
    uint16_t height;
    uint8_t*  pixels;
};

struct aaediclock_dx {
    double lat;
    double lon;
    char label[32];
};

struct plugin_mouse_event {
	struct aaediclock_FRect coords;
	int click_count;
	bool valid = false;
	uint64_t timestamp;
	uint32_t keycode;
	uint16_t keymod;
};

struct plugin_server_info {
    char name[128];
    uint16_t port;
};

struct plugin_wspr_station {
    char callsign[32];
    uint16_t band;
};

class aaediclock_host_api {
    public:
        // graphics calls
        virtual void 				AaediHAM_SetTarget		() 										= 0;
        virtual void 				AaediHAM_GraphicsDrawText	(const char* string, const aaediclock_Color color, const aaediclock_FRect dims) = 0;
        virtual void 				AaediHAM_GraphicsDrawRect	(const aaediclock_Color color, const aaediclock_FRect dims, bool filled) 	= 0;
        virtual void 				AaediHAM_GraphicsDrawLine	(const aaediclock_Color color, const aaediclock_FRect line) 			= 0;
        virtual void 				AaediHAM_GraphicsDrawLines	(const aaediclock_Color color, const aaediclock_FPoint* point_list, int count)	= 0;
        virtual void 				AaediHAM_GraphicsClear		(const aaediclock_Color& color = {0, 0, 0, 255}) 				= 0;
        virtual void				AaediHAM_GraphicsDrawImage	(uint16_t index, aaediclock_FRect* destrect = nullptr)				= 0;
        // config calls
        virtual const char* 			AaediHAM_ConfigGetQRZKey	(bool refresh) 									= 0;
        virtual const char* 			AaediHAM_ConfigGetCall		() 										= 0;
        virtual const char*			AaediHAM_ConfigGetCachePath	()										= 0;
        virtual const char*			AaediHAM_ConfigGetAssetPath	()										= 0;
	virtual const char*			AaediHAM_ConfigGetSiteCache	()										= 0;
        virtual const char* 			AaediHAM_ConfigGetPSKCall	() 										= 0;
        virtual struct aaediclock_dx 		AaediHAM_ConfigGetDE		() 										= 0;
        virtual void 				AaediHAM_ConfigSetDX		(struct aaediclock_dx new_dx) 							= 0;
        virtual struct aaediclock_dx 		AaediHAM_ConfigGetDX		() 										= 0;
        virtual struct plugin_server_info	AaediHAM_ConfigGetDXServer	() 										= 0;
        virtual struct plugin_wspr_station	AaediHAM_ConfigGetNextWspr	()										= 0;
        virtual const char*			AaediHAM_ConfigGetNextRss	()										= 0;
        virtual int				AaediHAM_ConfigGetSatCount	()										= 0;
        virtual const char*			AaediHAM_ConfigGetSat		(int index)									= 0;
        // program state calls
        virtual const struct plugin_mouse_event AaediHAM_GetMouseEvent		() 										= 0;
        virtual const struct aaediclock_FRect 	AaediHAM_GetMapSize		() 										= 0;
        // map pins
        virtual void 				AaediHAM_MapPinDelete		() 										= 0;
        virtual void 				AaediHAM_MapPinAdd		(struct aaediclock_map_pin) 							= 0;
        // overlay calls
        virtual bool 				AaediHAM_OverlayCheck		()		 								= 0;
        virtual void 				AaediHAM_OverlaySet		(aaediclock_FRect dims, uint8_t z_layer = 1)					= 0;
        virtual void 				AaediHAM_OverlayRemove		() 										= 0;
        virtual void				AaediHAM_OverlayClear		(const aaediclock_Color& color = {0, 0, 0, 255}) 				= 0;
        // icon calls
        virtual bool				AaediHAM_IconCheck		(uint16_t icon_index)								= 0;
        virtual uint16_t			AaediHAM_IconCreate		(const aaediclock_image& image_data)						= 0;
        virtual bool				AaediHAM_IconUpdate		(uint16_t index, const aaediclock_image& image_data)				= 0;
        virtual void				AaediHAM_IconDelete		(uint16_t index)								= 0;
        // texture cache calls
        virtual bool				AaediHAM_TextureCheck		(uint16_t icon_index)								= 0;
        virtual uint16_t			AaediHAM_TextureCreate		(const aaediclock_image& image_data)						= 0;
	virtual uint16_t			AaediHAM_TextureCreateString	(const char* string, const aaediclock_Color& foreground)			= 0;
        virtual bool				AaediHAM_TextureUpdate		(uint16_t index, const aaediclock_image& image_data)				= 0;
        virtual void				AaediHAM_TextureDelete		(uint16_t index)								= 0;
        // scroller calls
        virtual const struct aaediclock_FRect	AaediHAM_ScrollerInit		(const char* string, aaediclock_Color fg, aaediclock_Color bg)			= 0;
        virtual void				AaediHAM_ScrollerPosition	(const aaediclock_FRect source, const aaediclock_FRect dest)			= 0;
        virtual void				AaediHAM_ScrollerDelete		()										= 0;
	// user log reads
	virtual const char*			AaediHAM_LogGetNew		()										= 0;
	virtual uint16_t			AaediHAM_LogGetCount		()										= 0;
	virtual const char*			AaediHAM_LogGetIndex		(uint16_t index)								= 0;


	std::ostream* AaediHAM_LogUser = nullptr;
        std::ostream* AaediHAM_LogDebug = nullptr;
        const uint32_t API_VERSION = 0050;
};


class DllExport aaediclock_plugin_api {
    public:
        virtual ~aaediclock_plugin_api();
        virtual void plugin_init() const = 0;
        virtual void plugin_main(const aaediclock_FRect& dims) const = 0;
        virtual void set_host(aaediclock_host_api* host) = 0;

        virtual const char* getName() const = 0;
        virtual void plugin_exit() const = 0;
};

inline aaediclock_plugin_api::~aaediclock_plugin_api() {}
extern "C" DllExport aaediclock_plugin_api* createPlugin();
extern "C" DllExport void destroyPlugin(aaediclock_plugin_api* plugin);

#endif
