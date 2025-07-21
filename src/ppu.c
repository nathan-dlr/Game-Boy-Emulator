#include <stdint.h>
#include <stdlib.h>
#include <ppu.h>
#include <gb.h>

#define OBP0 0
#define OBP 1

void ppu_init() {
    PPU = malloc(sizeof(PPU_STRUCT));
    PPU->CURRENT_OBJ = malloc(sizeof(OAM_STRUCT));
    PPU->STATE = OAM_SEARCH;
    PPU->RENDER_CYCLE = 0;
}

void oam_search1() {
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


}