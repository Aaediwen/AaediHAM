#ifndef AAEDICLOCK_API_H
#define AAEDICLOCK_API_H
#ifdef _WIN32
#define DllExport __declspec(dllexport)
#else
#define DllExport
#endif

#include <cstdint>
#include <sstream>

struct aaediclock_FRect {
    float x;
    float y;
    float h;
    float w;
};

struct aaediclock_Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};


class aaediclock_host_api {
    public:

        virtual void AaediHAM_GraphicsDrawText(const char* string, const aaediclock_Color color, const aaediclock_FRect dims) = 0;
        virtual void AaediHAM_GraphicsClear(const aaediclock_Color& color = {0, 0, 0, 255}) = 0;
        virtual const char* AaediHAM_ConfigGetCall() = 0;
        std::ostream* AaediHAM_LogDebug = nullptr;
        const uint32_t API_VERSION = 0001;
};


class aaediclock_plugin_api {
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
