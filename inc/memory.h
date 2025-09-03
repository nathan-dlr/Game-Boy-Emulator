#ifndef GB_EMU_MEMORY_H
#define GB_EMU_MEMORY_H

enum HEADERS {
    MBC0,
    MBC1
};

typedef struct CARTRIDGE_STRUCT {
    enum HEADERS HEADER;
    uint8_t* MEMORY;
    uint32_t ROM_SIZE;
    uint32_t RAM_SIZE;
    uint8_t CART_ROM_BANK;
    uint8_t RAM_UPPER_ROM;
    uint8_t RAM_BANK;
    bool BANK_MODE;
    bool RAM_ENABLE;
    const uint16_t BANKS_IN_CART;
} CARTRIDGE_STRUCT;

uint8_t* MEMORY;
CARTRIDGE_STRUCT* CARTRIDGE;
void read_memory(uint8_t UNUSED);
void write_memory(uint8_t UNUSED);
#endif //GB_EMU_MEMORY_H
