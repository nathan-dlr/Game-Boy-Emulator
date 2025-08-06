#include <stdlib.h>
#include <stdbool.h>
#include <common.h>
#include <gb.h>
#include <ppu.h>
#include <lcd.h>

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

    LCD->window = SDL_CreateWindow("Game Boy", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!LCD->window) {
        fprintf(stderr, "Error creating window: %s\n", SDL_GetError());
        exit(1);
    }

    LCD->renderer = SDL_CreateRenderer(LCD->window, NULL);
    if (!LCD->renderer) {
        fprintf(stderr, "Error creating Renderer: %s\n", SDL_GetError());
        exit(1);
    }

    LCD->surface = SDL_CreateSurface(WINDOW_WIDTH, WINDOW_HEIGHT, SDL_PIXELFORMAT_RGBA8888);
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

    if (source == BACKGROUND || source == WINDOW) {
        palette = MEMORY[BGP];
    }
    else {
        palette = pixel_data->palette == OBJ_P0 ? MEMORY[OBP0] : MEMORY[OBP1];
    }
    pixel_color = (palette >> (color_index * 2)) & 0x03;
    pixel_rgb = COLORS_RGB[pixel_color];

    uint32_t* surface_pixels = (uint32_t*)LCD->surface->pixels;
    uint32_t y_offset = MEMORY[LY] * (LCD->surface->pitch / sizeof(uint32_t));
    surface_pixels[y_offset + PPU->RENDER_X] = pixel_rgb;
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