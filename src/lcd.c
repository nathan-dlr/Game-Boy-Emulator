#include <stdlib.h>
#include <stdbool.h>
#include <common.h>
#include <gb.h>
#include <ppu.h>
#include <lcd.h>

#define SCALE 4

static const uint32_t COLORS_RGB[4] = {
        0xFFFFFFFF, // White
        0xAAAAAAFF, // Light Gray
        0x555555FF, // Dark Gray
        0x000000FF  // Black
};

void lcd_init() {
    LCD = (GameBoy_Display*) malloc(sizeof(GameBoy_Display));

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Error initializing SDL3: %s\n", SDL_GetError());
        exit(1);
    }

    LCD->window = SDL_CreateWindow("Game Boy", WINDOW_WIDTH * SCALE, WINDOW_HEIGHT * SCALE, 0);
    if (!LCD->window) {
        fprintf(stderr, "Error creating window: %s\n", SDL_GetError());
        exit(1);
    }

    LCD->renderer = SDL_CreateRenderer(LCD->window, NULL);
    if (!LCD->renderer) {
        fprintf(stderr, "Error creating Renderer: %s\n", SDL_GetError());
        exit(1);
    }

    LCD->surface = SDL_CreateSurface(WINDOW_WIDTH * SCALE, WINDOW_HEIGHT * SCALE, SDL_PIXELFORMAT_RGBA8888);
    LCD->texture = SDL_CreateTextureFromSurface(LCD->renderer, LCD->surface);

    LCD->is_running = true;
}

void process_events() {
    while (SDL_PollEvent(&LCD->event)) {
        switch (LCD->event.type) {
            case SDL_EVENT_QUIT:
                LCD->is_running = false;
                break;
            default:
                break;
        }
    }
}

void lcd_update_screen() {
    SDL_UpdateTexture(LCD->texture, NULL, LCD->surface->pixels, LCD->surface->pitch);
    SDL_RenderClear(LCD->renderer);
    SDL_RenderTexture(LCD->renderer, LCD->texture, NULL, NULL);
    SDL_RenderPresent(LCD->renderer);
}

void lcd_update_pixel(PIXEL_DATA* pixel_data) {
    uint8_t palette;
    uint8_t pixel_color;
    uint32_t pixel_rgb;
    uint8_t color_index = pixel_data->binary_data;
    enum FETCH_SOURCE source = pixel_data->source;

    if (((source == BACKGROUND || source == WINDOW)) && (MEMORY[LCDC] & 0x01)) {
        palette = MEMORY[BGP];
        pixel_color = (palette >> (color_index * 2)) & 0x03;
        pixel_rgb = COLORS_RGB[pixel_color];
    }
    else if (source == OBJECT) {
        palette = pixel_data->palette ? MEMORY[OBP1] : MEMORY[OBP0];
        pixel_color = (palette >> (color_index * 2)) & 0x03;
        pixel_rgb = COLORS_RGB[pixel_color];
    }
    else {
        pixel_rgb = COLORS_RGB[0];
    }

    uint32_t* surface_pixels = (uint32_t*)LCD->surface->pixels;
    int dist_between_rows = LCD->surface->pitch / sizeof(uint32_t);

    int gb_x = PPU->RENDER_X;
    int gb_y = MEMORY[LY];
    
    for (int dy = 0; dy < SCALE; dy++) {
        for (int dx = 0; dx < SCALE; dx++) {
            int x_offset = gb_x * SCALE + dx;
            int y_offset = gb_y * SCALE + dy;
            surface_pixels[y_offset * dist_between_rows + x_offset] = pixel_rgb;
        }
    }
}

void lcd_free() {
    if (LCD) {
        if (LCD->renderer) {
            SDL_DestroyRenderer(LCD->renderer);
            LCD->renderer = NULL;
        }
        if (LCD->window) {
            SDL_DestroyWindow(LCD->window);
            LCD->window = NULL;
        }

        SDL_Quit();
        free(LCD);
    }
}