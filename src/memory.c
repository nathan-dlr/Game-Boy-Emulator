#include <common.h>
#include <cpu.h>
#include <gb.h>
#include <lcd.h>
#include <memory.h>

void ram_enable() {
    CARTRIDGE->RAM_ENABLE = (CPU->DATA_BUS & 0x0A) == 0x0A;
}

static void set_rom_bank() {
    uint8_t bank_num = CPU->DATA_BUS & 0x1F;
    if (bank_num == 0) {
        bank_num = 1;
    }
    if (bank_num > CARTRIDGE->BANKS_IN_CART) {
        uint8_t mask = 0x01;
        for (uint8_t bit = 0x04; bit <= 0x10; bit = bit << 1) {
            if (!(CARTRIDGE->BANKS_IN_CART & bit)) {
                break;
            }
            mask = (mask << 1) | 0x01;
        }
        bank_num &= mask;
    }
    CARTRIDGE->CART_ROM_BANK = bank_num;
}

static void set_RAM_UPPER_ROM() {
    CARTRIDGE->RAM_UPPER_ROM = CPU->DATA_BUS & 0x03;
}

static void set_banking_mode() {
    CARTRIDGE->BANK_MODE = (bool) CPU->DATA_BUS;
}

static void read_bank_00() {
    if (!CARTRIDGE->BANK_MODE) {
        CPU->DATA_BUS = CARTRIDGE->MEMORY[CPU->ADDRESS_BUS];
    }
    else if (CARTRIDGE->BANKS_IN_CART >= 64) {
        CPU->DATA_BUS = CARTRIDGE->MEMORY[(CARTRIDGE->RAM_UPPER_ROM << 19) | CPU->ADDRESS_BUS];
    }
    else {
        perror("Error in read bank 00");
    }
}

static void read_bank_0x() {
    uint32_t address = (CARTRIDGE->CART_ROM_BANK << 14) | CPU->ADDRESS_BUS & 0x3FFF;
    if (CARTRIDGE->BANKS_IN_CART >= 64) {
        address |= CARTRIDGE->RAM_UPPER_ROM << 19;
    }
    else {
        address &= CARTRIDGE->ROM_SIZE - 1;
    }
    CPU->DATA_BUS = CARTRIDGE->MEMORY[address];
}

static void write_ram() {
    if (!CARTRIDGE->RAM_ENABLE) {
        CPU->DATA_BUS = 0xFF;
    }
    if (!CARTRIDGE->BANK_MODE) {
        CARTRIDGE->MEMORY[CPU->ADDRESS_BUS] = CPU->DATA_BUS;
    }
    else {
        CARTRIDGE->MEMORY[(CARTRIDGE->RAM_UPPER_ROM << 13) | (CPU->ADDRESS_BUS & 0x1FFF)] = CPU->DATA_BUS;
    }
}

/*
 * Reads byte pointed to by CPU->ADDRESS_BUS onto CPU->DATA_BUS
 */
void read_memory(uint8_t UNUSED) {
    (void)UNUSED;
    if (CARTRIDGE->HEADER == MBC0 && CPU->ADDRESS_BUS <= 0x8000) {
        CPU->DATA_BUS = CARTRIDGE->MEMORY[CPU->ADDRESS_BUS];
        return;
    }
    else if (CPU->ADDRESS_BUS <= 0x4000) {
        read_bank_00();
        return;
    }
    else if (CPU->ADDRESS_BUS <= 0x8000) {
        read_bank_0x();
        return;
    }

    if (CPU->ADDRESS_BUS == P1) {
        uint8_t inputs = MEMORY[P1];
        if ((inputs & 0x10) == 0x10) {
            CPU->DATA_BUS = 0x10 | (LCD->buttons & 0x0F);
        }
        else if ((inputs & 0x20) == 0x20) {
            CPU->DATA_BUS = 0x20 | (LCD->d_pad & 0x0F);
        }
        else {
            CPU->DATA_BUS = 0xFF;
        }
        return;
    }
    if (CPU->ADDRESS_BUS >= 0xA000 && CPU->ADDRESS_BUS < 0xC000) {
        write_ram();
        return;
    }
    if (CPU->ADDRESS_BUS == SB) {
        CPU->DATA_BUS = 0xFF;
        return;
    }
//    if ((PPU->STATE == OAM_SEARCH) && (CPU->ADDRESS_BUS >= 0xFE00) && CPU->ADDRESS_BUS <= 0xFE9F) {
//        CPU->DATA_BUS = 0xFF;
//        return;
//    }
//    if ((PPU->STATE == PIXEL_TRANSFER) && (CPU->ADDRESS_BUS >= 0x8000) && (CPU->ADDRESS_BUS <= 0x9FFF)) {
//        CPU->DATA_BUS = 0xFF;
//        return;
//    }

    CPU->DATA_BUS = MEMORY[CPU->ADDRESS_BUS];
}

/*
 * Writes byte in CPU->DATA_BUS into the memory location
 * pointed to by CPU->ADDRESS_BUS
 */
void write_memory(uint8_t UNUSED) {
    (void)UNUSED;
    if (CARTRIDGE->HEADER == MBC0 && CPU->ADDRESS_BUS <= 0x8000) {
        return;
    }
    else if (CPU->ADDRESS_BUS < 0x2000) {
        ram_enable();
        return;
    }
    else if (CPU->ADDRESS_BUS < 0x4000) {
        set_rom_bank();
        return;
    }
    else if (CPU->ADDRESS_BUS < 0x6000) {
        set_RAM_UPPER_ROM();
        return;
    }
    else if (CPU->ADDRESS_BUS < 0x8000) {
        set_banking_mode();
        return;
    }
    //TODO 2 CYCLE DELAY FOR OAM DMA?
    if (CPU->ADDRESS_BUS == DMA) {
        CPU->STATE = OAM_DMA_TRANSFER;
        CPU->DMA_CYCLE = 0;
    }
    if (CPU->ADDRESS_BUS < 0x8000) {
        return;
    }

//    if ((PPU->STATE == OAM_SEARCH) && (CPU->ADDRESS_BUS >= 0xFE00) && CPU->ADDRESS_BUS <= 0xFE9F) {
//        return;
//    }
//    if ((PPU->STATE == PIXEL_TRANSFER) && (CPU->ADDRESS_BUS >= 0x8000) && (CPU->ADDRESS_BUS <= 0x9FFF)) {
//        return;
//    }

    if (CPU->ADDRESS_BUS == STAT) {
        MEMORY[STAT] = (MEMORY[STAT] & 0x07) | (CPU->DATA_BUS & 0xF8);
        return;
    }
    if (CPU->ADDRESS_BUS == P1) {
        MEMORY[P1] = (MEMORY[P1] & 0x0F) | (CPU->DATA_BUS & 0xF0);
        return;
    }

    MEMORY[CPU->ADDRESS_BUS] = CPU->DATA_BUS;

    if (CPU->ADDRESS_BUS == TAC) {
        set_tac();
    }
    if (CPU->ADDRESS_BUS == DIV) {
        MEMORY[DIV] = 0x00;
    }
}