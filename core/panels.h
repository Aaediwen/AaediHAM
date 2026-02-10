#pragma once
#include "aaediclock.h"

extern std::string render_engine;
extern Uint16 interrupt_counter;

void panel_assignment(bool increment);

Uint32 SDLCALL master_clock (void *userdata, SDL_TimerID timerID, Uint32 interval);

void resize_panels(std::array<pager_node, 12>& panels);