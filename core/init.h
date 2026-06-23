#include <string>

extern std::string configfile;
extern bool                            interrupt_flag;
extern bool                            reload_flag;

namespace AaediClock_Init {
    SDL_AppResult cmd_line_parser(int argc, char **argv);
    SDL_AppResult System_Init();
    SDL_AppResult Global_Init();
    SDL_AppResult Window_Init(int x, int y);
    std::string FindFont(const char* fontname);
    SDL_AppResult Font_Init();
    SDL_AppResult Init_System_Timer();
    SDL_AppResult Plugin_Loader();
};

