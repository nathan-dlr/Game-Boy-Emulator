#include <stdint.h>
#include <stdlib.h>
#include <ppu.h>
#include <gb.h>
#include <queue.h>

#define OBP0 0
#define OBP 1
#define BITS_PER_TILE 16

void ppu_init() {
    PPU = malloc(sizeof(PPU_STRUCT));
    PPU->CURRENT_OBJ = malloc(sizeof(OAM_STRUCT));
    PPU->STATE = OAM_SEARCH;
    PPU->RENDER_CYCLE = 0;
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
    }
    else {
        PPU->VALID_OAM = false;
    }
    if (PPU->RENDER_CYCLE == 80) {
        PPU->STATE = PIXEL_TRANSFER;
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
        //TODO Needs to be a min heap based off x coord
        object_queue_push(PPU->CURRENT_OBJ);
    }
}


void fetch_tile() {
    uint8_t tile_x;
    uint16_t base_address;
    uint16_t tile_map_address;
    if ((MEMORY[WX] >= PPU->FETCHER_X) && (MEMORY[LCDC] & 0x20)) {
        base_address = MEMORY[LCDC] & 0x40 ? 0x9C00 : 0x9800;
        tile_x = PPU->FETCHER_X;
        PPU->FETCHER_Y = MEMORY[LY] - MEMORY[WY];
        tile_map_address = base_address + tile_x + ((PPU->FETCHER_Y / 8) * 32);
        PPU->TILE_NUMBER = MEMORY[tile_map_address];
    }
    else {
        base_address = MEMORY[LCDC] & 0x08 ? 0x9C00 : 0x9800;
        tile_x = ((MEMORY[SCX] / 8) + PPU->FETCHER_X) & 0x1F;
        PPU->FETCHER_Y = (MEMORY[LY] + MEMORY[SCY]) & 0xFF;
        tile_map_address = base_address + tile_x + ((PPU->FETCHER_Y / 8) * 32);
        //TODO IF VRAM IS BLOCKED THAN TILE NUMBER WILL BE READ AS 0xFF
        PPU->TILE_NUMBER = MEMORY[tile_map_address];
    }
    PPU->PIXEL_TRANSFER_STATE = GET_DATA_LOW;
}

void get_tile_data_low() {
    uint16_t base_address = MEMORY[LCDC] & 0x10 ? 0x8000 : 0x8800;
    PPU->TILE_ADDRESS = base_address + (PPU->TILE_NUMBER * BITS_PER_TILE); //get tile
    PPU->TILE_ADDRESS += 2 * (PPU->FETCHER_Y % 8);                         //get line of pixels within the tile
    PPU->DATA_LOW = MEMORY[PPU->TILE_ADDRESS];
    PPU->PIXEL_TRANSFER_STATE = GET_DATA_HIGH;
}

void get_tile_data_high() {
    PPU->DATA_HIGH = MEMORY[PPU->TILE_ADDRESS+1];
    PPU->PIXEL_TRANSFER_STATE = SLEEP;
}

/*
 * Make the array of pixels once in sleep, so that if pixels arent ready to be pushed,
 * theres no need to keep checking if the array has been made already
 */
void pixel_transfer_sleep() {
    PPU->PIXEL_TRANSFER_STATE = PUSH;
    uint8_t data_low = PPU->DATA_LOW;
    uint8_t data_high = PPU->DATA_HIGH;
    uint8_t pixel;
    for (int i = 8; i > 0; i / 2) {
        pixel = ((data_high & (0x01 << i)) >> (i - 1)) | ((data_low & (0x01 << i)) >> i);
    }
}

void pixel_push() {
    //if FIFO.size <= 8 then push
}

void pop_pixel() {
    //TODO ONLY PUSH IF AT LEAST 8 BITS in FIFO
    if (MEMORY[WX] == MEMORY[LY]) {
        //WINDOW CONDITION IS TRUE
    }
    uint8_t obj_x_coord; //peek object's x
    /*
     * additionally make sure our object heap is caught up to our scanline
     * if a object in the list is not shown the screen, its x position wont ever be equal to the screen's x position so pop it from heap if its xpos is less than ppu
     */

    if (obj_x_coord == PPU->RENDER_X) {
        //replace all pixels with that of object, how many cycles does this take?
        //there's a penalty for objects, so maybe add a penalty attribute to the ppu that can be subtracted for each dot that passes
    }
    else if ((MEMORY[WX] == PPU->RENDER_X) && (MEMORY[LCDC] & 0x20)) {
        PPU->FETCHER_X = PPU->RENDER_X;
        //wipe fifo, reset fetched. this only happens when we first hit window coordinate
    }
    //pop_pixel
    if (MEMORY[SCX] > PPU->RENDER_X) {
        //throw away pixel until our x pixel is past the scroll
    }
    else {
        //add to screen
    }
}
