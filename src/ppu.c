#include <stdint.h>
#include <stdlib.h>
#include <ppu.h>
#include <gb.h>
#include <queue.h>
#include <min_heap.h>

#define BITS_PER_TILE 16
#define CYCLES_PER_LINE 456
#define PIXELS_PER_TILE 8

void ppu_init() {
    PPU = malloc(sizeof(PPU_STRUCT));
    PPU->CURRENT_OBJ = malloc(sizeof(OAM_STRUCT));
    PPU->PIXEL_DATA = calloc(PIXELS_PER_TILE, sizeof(PIXEL_DATA));
    PPU->STATE = OAM_SEARCH;
    PPU->RENDER_CYCLE = 0;
    PPU->FETCH_TYPE = BACKGROUND;
}

void ppu_free() {
    free(PPU->PIXEL_DATA);
    free(PPU->CURRENT_OBJ);
    free(PPU);
}

void oam_search_validate() {
    uint8_t oam_index = PPU->RENDER_CYCLE / 2;
    uint8_t obj_y_pos = MEMORY[oam_index];
    uint8_t obj_x_pos = MEMORY[oam_index + 1];
    uint8_t lcd_y_position = MEMORY[LY];
    uint8_t obj_height = MEMORY[LCDC] & 0x02 ? 16 : 8;

    if ((lcd_y_position + 16 >= obj_y_pos) && (lcd_y_position + 16 <= obj_y_pos + obj_height)) {
        PPU->VALID_OAM = true;
        PPU->CURRENT_OBJ->y_pos = obj_y_pos;
        PPU->CURRENT_OBJ->x_pos = obj_x_pos;
        PPU->CURRENT_OBJ->address = oam_index;
    }
    else {
        PPU->VALID_OAM = false;
    }
}

void oam_search_store() {
    if (PPU->VALID_OAM) {
        uint8_t oam_index = PPU->RENDER_CYCLE / 2;
        uint8_t tile_index = MEMORY[oam_index];
        uint8_t oam_attributes = MEMORY[oam_index + 1];
        PPU->CURRENT_OBJ->tile_index = tile_index;
        PPU->CURRENT_OBJ->priority = oam_attributes & 0x80;
        PPU->CURRENT_OBJ->y_flip = oam_attributes & 0x40;
        PPU->CURRENT_OBJ->x_flip = oam_attributes & 0x20;
        PPU->CURRENT_OBJ->palette = oam_attributes & 0x10;
        heap_insert(PPU->CURRENT_OBJ);
    }
    if (PPU->RENDER_CYCLE == 80) {
        PPU->STATE = PIXEL_TRANSFER;
        PPU->NUM_SCROLL_PIXELS = MEMORY[SCX] % 8;
    }
}


void fetch_tile() {
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

void get_tile_data_low() {
    uint16_t base_address = MEMORY[LCDC] & 0x10 || PPU->FETCH_TYPE == OBJECT ? 0x8000 : 0x8800;
    PPU->TILE_ADDRESS = base_address + (PPU->TILE_NUMBER * BITS_PER_TILE); //get tile
    PPU->TILE_ADDRESS += 2 * (PPU->FETCHER_Y % 8);                         //get line of pixels within the tile
    PPU->DATA_LOW = MEMORY[PPU->TILE_ADDRESS];
    PPU->PIXEL_TRANSFER_STATE = GET_DATA_HIGH;
}

void get_tile_data_high() {
    PPU->DATA_HIGH = MEMORY[PPU->TILE_ADDRESS+1];
    PPU->PIXEL_TRANSFER_STATE = PPU->FETCH_TYPE != OBJECT ? SLEEP : PUSH;
}

void construct_pixel_data() {
    uint8_t data_low = PPU->DATA_LOW;
    uint8_t data_high = PPU->DATA_HIGH;
    uint8_t pixel;
    for (int8_t i = 7, j = 0; i >= 0; i--, j++) {
        pixel = ((data_high & (0x01 << i)) >> (i - 1)) | ((data_low & (0x01 << i)) >> i);
        PPU->PIXEL_DATA->binary_data[j] = pixel;
    }
    PPU->PIXEL_TRANSFER_STATE = PUSH;
}

void pixel_push() {
    if (PPU->FETCH_TYPE != OBJECT) {
        if (pixel_fifo_size() <= 8) {
            pixel_fifo_push(PPU->PIXEL_DATA);
            PPU->STATE = H_BLANK;
        }
    }
    else {
        //TODO MAYBE ENABLE/DISABLE PIXEL POPPING
        //TODO YFLIP
        construct_pixel_data();
        pixel_fifo_merge_object(PPU->PIXEL_DATA);
        heap_delete_min();
        uint8_t obj_x_coord = heap_peek_x_pos();
        if (obj_x_coord == PPU->RENDER_X) {
            PPU->FETCH_TYPE = OBJECT;
            PPU->PIXEL_TRANSFER_STATE = GET_TILE;
            return;
        }
        else {
            //TODO POP TILE
        }

    }
}

void h_blank() {
    if (PPU->RENDER_CYCLE != CYCLES_PER_LINE) {
        PPU->RENDER_CYCLE++;
        return;
    }
    else if (MEMORY[LY] > 143) {
        PPU->STATE = OAM_SEARCH;
    }
    else {
        MEMORY[LY]++;
        PPU->STATE = V_BLANK;
    }
}

void v_blank() {
    if (PPU->RENDER_CYCLE != CYCLES_PER_LINE) {
        PPU->RENDER_CYCLE++;
    }
    else if (MEMORY[LY] > 154) {
        MEMORY[LY]++;
        PPU->RENDER_CYCLE = 0;
    }
    else {
        MEMORY[LY] = 0;
        PPU->RENDER_CYCLE = 0;
        PPU->STATE = OAM_SEARCH;
    }
}
void pop_pixel() {
    //Check for window
    if ((MEMORY[WX] == PPU->RENDER_X) && (PPU->FETCH_TYPE == BACKGROUND) && (MEMORY[LCDC] & 0x20)) {
        pixel_fifo_clear();
        PPU->PIXEL_TRANSFER_STATE = GET_TILE;
        PPU->FETCH_TYPE = WINDOW;
        return;
    }
    //check for object
    uint8_t obj_x_coord = heap_peek_x_pos();
    if (obj_x_coord == PPU->RENDER_X) {
        PPU->FETCH_TYPE = OBJECT;
        PPU->PIXEL_TRANSFER_STATE = GET_TILE;
        return;
    }
    //pop_pixel if enough data in fifo
    if (pixel_fifo_size() >= 8) {
        if (PPU->NUM_SCROLL_PIXELS) {
            pixel_fifo_pop();
            PPU->NUM_SCROLL_PIXELS--;
        } else {
            PIXEL_DATA pixel_data = pixel_fifo_pop();
            //add to screen
        }
        PPU->RENDER_X++;
        if (PPU->RENDER_X == 160) {
            pixel_fifo_clear();
            PPU->STATE = H_BLANK;
        }
    }
}
