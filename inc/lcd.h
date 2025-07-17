#ifndef GB_EMU_SCREEN_H
#define GB_EMU_SCREEN_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

typedef struct GameBoy_Display {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Event event;
    bool is_running;
} GameBoy_Display;

struct GameBoy_Display* LCD;

void lcd_init();
void lcd_free();
void process_events();
void lcd_update();

#endif //GB_EMU_SCREEN_H
