#include <stdlib.h>
#include <stdbool.h>
#include <lcd.h>

#define WINDOW_WIDTH 160
#define WINDOW_HEIGHT 144
void lcd_init() {
    LCD = malloc(sizeof(GameBoy_Display));

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

void lcd_update() {
    SDL_RenderClear(LCD->renderer);
    SDL_RenderPresent(LCD->renderer);
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