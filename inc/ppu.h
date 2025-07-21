#ifndef GB_EMU_PPU_H
#define GB_EMU_PPU_H

enum PPU_STATE {
    OAM_SEARCH,
    PIXEL_TRANSFER,
    H_BLANK,
    V_BLANK
};

typedef struct OAM_STRUCT {
    uint8_t y_pos;
    uint8_t x_pos;
    uint8_t tile_index;
    bool priority;
    bool y_flip;
    bool x_flip;
    bool palette;
} OAM_STRUCT;

typedef struct PPU_STRUCT {
    uint8_t RENDER_CYCLE;
    enum PPU_STATE STATE;
    bool VALID_OAM;
    struct OAM_STRUCT* CURRENT_OBJ;
} PPU_STRUCT;


PPU_STRUCT* PPU;

void ppu_init();
void oam_search();


#endif //GB_EMU_PPU_H
