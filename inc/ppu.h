#ifndef GB_EMU_PPU_H
#define GB_EMU_PPU_H

enum PPU_STATE {
    OAM_SEARCH,
    PIXEL_TRANSFER,
    H_BLANK,
    V_BLANK
};

enum PIXEL_TRANSFER_STATE {
    GET_TILE,
    GET_DATA_LOW,
    GET_DATA_HIGH,
    SLEEP,
    PUSH
};

enum FETCH_SOURCE {
    BACKGROUND,
    WINDOW,
    OBJECT
};

enum PALETTE {
    OBP0,
    OBP1
};

typedef struct OAM_STRUCT {
    uint8_t y_pos;
    uint8_t x_pos;
    uint8_t tile_index;
    uint16_t address;
    bool priority;
    bool y_flip;
    bool x_flip;
    enum PALETTE palette;
} OAM_STRUCT;

typedef struct PIXEL_DATA {
    uint8_t* binary_data;
    enum FETCH_SOURCE source;
    bool palette;
} PIXEL_DATA;

typedef struct PPU_STRUCT {
    enum PPU_STATE STATE;
    enum PIXEL_TRANSFER_STATE PIXEL_TRANSFER_STATE;
    //RENDER DATA
    uint8_t RENDER_CYCLE;
    uint8_t RENDER_X;   //incremented per pixel pushed
    enum FETCH_SOURCE FETCH_TYPE;
    uint8_t NUM_SCROLL_PIXELS;
    //FETCHER DATA
    uint8_t FETCHER_Y;
    uint8_t FETCHER_X;  //incremented per 8 pixels fetched
    uint8_t TILE_NUMBER;
    uint8_t TILE_ADDRESS;
    uint8_t DATA_LOW;
    uint8_t DATA_HIGH;
    PIXEL_DATA* PIXEL_DATA;
    //OBJECT DATA
    struct OAM_STRUCT* CURRENT_OBJ;
    bool VALID_OAM;
} PPU_STRUCT;


PPU_STRUCT* PPU;

void ppu_init();
void ppu_free();
void oam_search();


#endif //GB_EMU_PPU_H
