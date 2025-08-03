#include <stdint.h>
#include <stdlib.h>
#include <ppu.h>
#include <gb.h>
#include <queue.h>
#include <min_heap.h>
#include <lcd.h>

#define BITS_PER_TILE 16
#define CYCLES_PER_LINE 456
#define PIXELS_PER_TILE 8
#define OAM_BASE_ADDRESS 0xFE00

void ppu_init() {
    PPU = malloc(sizeof(PPU_STRUCT));
    PPU->CURRENT_OBJ = malloc(sizeof(OAM_STRUCT));
    PPU->PIXEL_DATA = calloc(PIXELS_PER_TILE, sizeof(PIXEL_DATA));
    PPU->STATE = V_BLANK;
    PPU->RENDER_LINE_CYCLE = 1;
    PPU->FETCH_TYPE = BACKGROUND;
    PPU->PENALTY = 0;
}

void ppu_free() {
    free(PPU->PIXEL_DATA);
    free(PPU->CURRENT_OBJ);
    free(PPU);
}

static void oam_search_validate() {
    uint16_t oam_address = OAM_BASE_ADDRESS + (PPU->RENDER_LINE_CYCLE - 1) * 2;
    uint8_t obj_y_pos = MEMORY[oam_address];
    uint8_t obj_x_pos = MEMORY[oam_address + 1];
    uint8_t lcd_y_position = MEMORY[LY];
    uint8_t obj_height = MEMORY[LCDC] & 0x04 ? 16 : 8;

    //an objects y position is equal to their vertical position on screen + 16
    if ((lcd_y_position + 16 >= obj_y_pos) && (lcd_y_position + 16 <= obj_y_pos + obj_height)) {
        printf("%X\n", oam_address);
        PPU->VALID_OAM = true;
        PPU->CURRENT_OBJ->y_pos = obj_y_pos;
        PPU->CURRENT_OBJ->x_pos = obj_x_pos;
        PPU->CURRENT_OBJ->address = oam_address;
    }
    else {
        PPU->VALID_OAM = false;
    }
}

static void oam_search_store() {
    if (PPU->VALID_OAM) {
        uint16_t oam_address = PPU->CURRENT_OBJ->address;
        uint8_t tile_index = MEMORY[oam_address + 2];
        uint8_t oam_attributes = MEMORY[oam_address + 3];
        PPU->CURRENT_OBJ->tile_index = tile_index;
        PPU->CURRENT_OBJ->priority = oam_attributes & 0x80;
        PPU->CURRENT_OBJ->y_flip = oam_attributes & 0x40;
        PPU->CURRENT_OBJ->x_flip = oam_attributes & 0x20;
        PPU->CURRENT_OBJ->palette = oam_attributes & 0x10;
        heap_insert(PPU->CURRENT_OBJ);
    }
    if (PPU->RENDER_LINE_CYCLE == 80) {
        PPU->STATE = PIXEL_TRANSFER;
        MEMORY[STAT] = (MEMORY[STAT] & 0xFC) | 0x03;
        PPU->NUM_SCROLL_PIXELS = MEMORY[SCX] % 8;
    }
}


static void fetch_tile() {
    uint8_t tile_x;
    uint16_t base_address;
    uint16_t tile_map_address;
    if (PPU->FETCH_TYPE == WINDOW) {
        base_address = MEMORY[LCDC] & 0x40 ? 0x9C00 : 0x9800;
        tile_x = PPU->FETCHER_X;
        PPU->FETCHER_Y = MEMORY[LY] - MEMORY[WY];
        tile_map_address = base_address + tile_x + ((PPU->FETCHER_Y / 8) * 32);
        PPU->TILE_NUMBER = MEMORY[tile_map_address];
        PPU->PIXEL_DATA->source = WINDOW;
    }
    else if (PPU->FETCH_TYPE == BACKGROUND) {
        base_address = MEMORY[LCDC] & 0x08 ? 0x9C00 : 0x9800;
        tile_x = ((MEMORY[SCX] / 8) + PPU->FETCHER_X) & 0x1F;
        PPU->FETCHER_Y = (MEMORY[LY] + MEMORY[SCY]) & 0xFF;
        tile_map_address = base_address + tile_x + ((PPU->FETCHER_Y / 8) * 32);
        //TODO IF VRAM IS BLOCKED THAN TILE NUMBER WILL BE READ AS 0xFF
        PPU->TILE_NUMBER = MEMORY[tile_map_address];
        PPU->PIXEL_DATA->source = BACKGROUND;
    }
    else {
        PPU->TILE_NUMBER = heap_peek_tile_num();
        PPU->PIXEL_DATA->source = OBJECT;
    }
    PPU->PIXEL_TRANSFER_STATE = GET_DATA_LOW;
}

static void get_tile_data_low() {
    uint16_t base_address = ((MEMORY[LCDC] & 0x10) || (PPU->FETCH_TYPE == OBJECT)) ? 0x8000 : 0x8800;
    PPU->TILE_ADDRESS = base_address + (PPU->TILE_NUMBER * BITS_PER_TILE); //get tile
    PPU->TILE_ADDRESS += 2 * (PPU->FETCHER_Y % 8);                         //get line of pixels within the tile
    PPU->DATA_LOW = MEMORY[PPU->TILE_ADDRESS];
    PPU->PIXEL_TRANSFER_STATE = GET_DATA_HIGH;
}

static void get_tile_data_high() {
    PPU->DATA_HIGH = MEMORY[PPU->TILE_ADDRESS+1];
    PPU->PIXEL_TRANSFER_STATE = PPU->FETCH_TYPE != OBJECT ? SLEEP : PUSH;
}

static void construct_pixel_data() {
    uint8_t data_low = PPU->DATA_LOW;
    uint8_t data_high = PPU->DATA_HIGH;
    uint8_t pixel;
    for (int8_t i = 7, j = 0; i >= 0; i--, j++) {
        pixel = ((data_high & (0x01 << i)) >> (i - 1)) | ((data_low & (0x01 << i)) >> i);
        PPU->PIXEL_DATA[j].binary_data = pixel;
    }
    PPU->PIXEL_TRANSFER_STATE = PUSH;
}

static void pixel_push() {
    if ((PPU->FETCH_TYPE != OBJECT) && (pixel_fifo_size() <= 8)) {
        pixel_fifo_push(PPU->PIXEL_DATA);
        PPU->FETCHER_X++;
        PPU->PIXEL_TRANSFER_STATE = FETCH_TILE;
    }
    else if (PPU->FETCH_TYPE == OBJECT) {
        //TODO MAYBE ENABLE/DISABLE PIXEL POPPING
        //TODO YFLIP
        construct_pixel_data();
        pixel_fifo_merge_object(PPU->PIXEL_DATA);
        heap_delete_min();
        PPU->PIXEL_TRANSFER_STATE = FETCH_TILE;

        uint8_t obj_x_coord = heap_peek_x_pos();
        if (obj_x_coord == PPU->RENDER_X) {
            PPU->FETCH_TYPE = OBJECT;
            return;
        }
        else if ((MEMORY[WX] >= PPU->FETCHER_X) && (MEMORY[LCDC] & 0x20)) {
            PPU->FETCH_TYPE = WINDOW;
        }
        else {
            PPU->FETCH_TYPE = BACKGROUND;
        }

        PIXEL_DATA pixel_data;
        pixel_fifo_pop(&pixel_data);
        lcd_update_pixel(&pixel_data);
        PPU->RENDER_X++;
    }
}

static void pop_pixel() {
    //Check for window
    if ((MEMORY[WX] == PPU->RENDER_X) && (PPU->FETCH_TYPE == BACKGROUND) && (MEMORY[LCDC] & 0x20)) {
        pixel_fifo_clear();
        PPU->PIXEL_TRANSFER_STATE = FETCH_TILE;
        PPU->FETCH_TYPE = WINDOW;
        return;
    }
    //check for object
    uint8_t obj_x_coord = heap_peek_x_pos();
    if (obj_x_coord == PPU->RENDER_X) {
        PPU->FETCH_TYPE = OBJECT;
        PPU->PIXEL_TRANSFER_STATE = FETCH_TILE;
        return;
    }
    //pop_pixel if enough data in fifo
    if (pixel_fifo_size() >= 8) {
        PIXEL_DATA pixel_data;
        if (PPU->NUM_SCROLL_PIXELS) {
            pixel_fifo_pop(&pixel_data);
            PPU->NUM_SCROLL_PIXELS--;
        } else {
            pixel_fifo_pop(&pixel_data);
            lcd_update_pixel(&pixel_data);
        }
        PPU->RENDER_X++;
        if (PPU->RENDER_X == 160) {
            PPU->RENDER_X = 0;
            PPU->FETCHER_X = 0;
            pixel_fifo_clear();
            PPU->STATE = H_BLANK;
            MEMORY[STAT] = (MEMORY[STAT] & 0xFC);
        }
    }
}

void h_blank() {
    if (PPU->RENDER_LINE_CYCLE == CYCLES_PER_LINE) {
        MEMORY[LY]++;
        PPU->RENDER_LINE_CYCLE = 0;
        PPU->STATE = MEMORY[LY] < 143 ? OAM_SEARCH : V_BLANK;
        MEMORY[STAT] = MEMORY[LY] < 143 ? 0x02 : 0x01;
    }
}

void v_blank() {
    if (PPU->RENDER_LINE_CYCLE != CYCLES_PER_LINE) {
        return;
    }
    else if (MEMORY[LY] > 154) {
        MEMORY[LY]++;
        PPU->RENDER_LINE_CYCLE = 0;
    }
    else {
        MEMORY[LY] = 0;
        PPU->RENDER_LINE_CYCLE = 0;
        PPU->STATE = OAM_SEARCH;
        MEMORY[STAT] = (MEMORY[STAT] & 0xFC) | 0x02;
        heap_clear();
        process_events();
        lcd_update_screen();
    }
}

void execute_next_PPU_cycle() {
    if ((PPU->RENDER_LINE_CYCLE > 369) && (PPU->STATE == PIXEL_TRANSFER)) {
        perror("Pixel transfer exceeded max cycles");
        free_resources();
        exit(1);
    }
    if (PPU->RENDER_LINE_CYCLE > CYCLES_PER_LINE) {
        perror("Render line cycle greater than cycles per line");
        free_resources();
        exit(1);
    }
    if (PPU->PENALTY) {
        PPU->PENALTY--;
        PPU->RENDER_LINE_CYCLE++;
        return;
    }

    switch (PPU->STATE) {
        case OAM_SEARCH:
            PPU->RENDER_LINE_CYCLE % 2 ? oam_search_validate() : oam_search_store();
            break;
        case PIXEL_TRANSFER:
            pop_pixel();
            switch (PPU->PIXEL_TRANSFER_STATE) {
                case FETCH_TILE:
                    fetch_tile();
                    PPU->PENALTY++;
                    break;
                case GET_DATA_LOW:
                    get_tile_data_low();
                    PPU->PENALTY++;
                    break;
                case GET_DATA_HIGH:
                    get_tile_data_high();
                    PPU->PENALTY++;
                    break;
                case SLEEP:
                    construct_pixel_data();
                    PPU->PENALTY++;
                    break;
                case PUSH:
                    pixel_push();
                    break;
            }
            break;
        case H_BLANK:
            h_blank();
            break;
        case V_BLANK:
            v_blank();
            break;
    }
    PPU->RENDER_LINE_CYCLE++;
}