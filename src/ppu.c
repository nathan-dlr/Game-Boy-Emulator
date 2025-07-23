#include <stdint.h>
#include <stdlib.h>
#include <ppu.h>
#include <gb.h>
#include <queue.h>

#define OBP0 0
#define OBP 1

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
        //TODO Needs to be a min heap beased off x coord
        object_queue_push(PPU->CURRENT_OBJ);
    }
}

void fetch_tile() {
    uint16_t tile_map_address;
    if (MEMORY[WX] >= PPU->PIXEL_X_VALUE) {
        //get window tile
    }
    else {
        tile_map_address = MEMORY[LCDC] & 0x08 ? 0x9800 : 0x9C00;
    }
}

void pop_pixel() {
    uint8_t obj_x_coord; //peek object's x
    /*
     * additionally make sure our object heap is caught up to our scanline
     * if a object in the list is not shown the screen, its x position wont ever be equal to the screen's x position so pop it from heap if its xpos is less than ppu
     */

    if (obj_x_coord == PPU->PIXEL_X_VALUE) {
        //replace all pixels with that of object, how many cycles does this take?
        //there's a penalty for objects, so maybe add a penalty attribute to the ppu that can be subtracted for each dot that passes
    }
    else if (MEMORY[WX] == PPU->PIXEL_X_VALUE) {
        //wipe fifo, reset fetched. this only happens when we first hit window coordinate
    }
    //pop_pixel
    if (MEMORY[SCX] > PPU->PIXEL_X_VALUE) {
        //throw away pixel until our x pixel is past the scroll
    }
    else {
        //add to screen
    }
}
