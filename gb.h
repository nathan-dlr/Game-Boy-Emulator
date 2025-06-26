#ifndef GB_EMU_GB_H
#define GB_EMU_GB_H


uint8_t* MEMORY;
uint8_t read_memory(uint16_t address);
void write_memory(uint16_t address, uint8_t value);
uint8_t read_memory2();
void write_memory2();

#endif //GB_EMU_GB_H
