#ifndef GB_EMU_SCREEN_H
#define GB_EMU_SCREEN_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

typedef struct GameBoy_Display {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_Event event;
    bool is_running;
} GameBoy_Display;

struct GameBoy_Display* LCD;

void lcd_init();
void lcd_free();
void process_events();
void lcd_update_screen();
void lcd_update_pixel(PIXEL_DATA* pixel_data);
void lcd_update_screen();
#endif //GB_EMU_SCREEN_H
