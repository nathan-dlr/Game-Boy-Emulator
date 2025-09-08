# ByteBoy
<div align="center">
  <img src="https://github.com/user-attachments/assets/db3bd892-d6aa-4c26-ab39-03696bd8fc08" width="45%" />
  <img src="https://github.com/user-attachments/assets/a05bdfa8-1c40-4306-acea-14ae4ef51980" width="45%" />
</div>
<div align="center">
  <img src="https://github.com/user-attachments/assets/fe8a81f2-aa9f-46d9-8d39-12989f238f5a" />
</div>


## Overview
ByteBoy is an emulator for the original Game Boy (DMG-01) written in C. So far, this project consists of the CPU and PPU (Picture Processing Unit).
This emulator can currently play MBC1 games. SDL3 is used for the front-end.
## CPU 
The Game Boy uses the Sharp SM83 as its processor. The Sharp SM83 has a 16-bit address space and is byte addressable. Additionally, 
the SM83 uses a variable-length instruction set consisting of either a byte-long opcode or the prefix 0xCB followed by the opcode. The CPU runs
at approximately 1.049 MHz, and the cycles are referred to as M-cycles.
### Decode 
Instructions that do the same operation on different operands don't have the same opcode; each instruction has a unique opcode. Therefore, rather than having
a switch statement with 512 cases, my emulator follows the findings ["DECODING Gameboy Z80 OPCODES"](https://archive.gbdev.io/salvage/decoding_gbz80_opcodes/Decoding%20Gamboy%20Z80%20Opcodes.html) 
by Scott Mansell to algorithmically decode the Game Boy's opcodes.  The opcodes are broken into their octal digits to index into a function look-up table; these functions use 
steering bits to decode the operation and its operands.
### Cycle-Accurate Execution
A function queue was used to achieve cycle-accurate execution. The first cycle of every instruction decodes the instruction. Once the instruction is decoded, the rest of the cycles
needed for the decoded instruction are pushed into the function queue. Each pushed function does an M-cycle worth of some general operations on registers, memory locations, the data bus, or the address bus.
The main CPU function simply pops a function from the function queue to execute the next cycle of the instruction, or if the queue is empty, it fetches the next opcode to decode.
## PPU 
The PPU cycles through four states: OAM search, pixel fetch, h-blank, and v-blank. The PPU draws one line at a time pixel-by-pixel until the whole screen is filled up. The screen is 144x160 pixels.
Although the time it takes to draw a line can depend on factors such as the window and sprite tiles, h-blank and v-blank mode ensure that the screen refreshes at a constant rate of just under 60 Hz.
The PPU runs on T-cycles that are 4 times the speed of an M-cycle. Picture is displayed through 8x8 squares of pixels called tiles. There are four possible colors for each pixel; thus, it takes two bits to 
indicate the color of a pixel. There are two tile maps in VRAM for the  background and window display, each are filled with tile IDs to index into where tile data is stored. 
### OAM Search
The first state of the PPU searches for 10 available sprites to display on the current line being drawn and takes 80 T-cycles to complete. 
### Fetch Tile
The Game Boy PPU uses two pixel FIFOs; the background FIFO collects data from the background and window tile maps, and the sprite FIFO collects data from objects. The PPU fetches
tiles and loads the tile's pixels into a FIFO and simultaneously pushes the pixels from the FIFO onto the screen. There are 8 pixels in a tile row and 2 bits of color per pixel; 
Thus, a row of pixels in a tile is two bytes. To fetch a row of pixels from a tile to put into a pixel FIFO, the PPU switches between four states.
1. Index into a tile map with an internal fetch counter (Two T-Cycles)
2. Get lower byte of data (Two T-Cycles)
3. Get higher byte of data (Two T-Cycles)
4. Push data into a FIFO (One T-Cycle, only executes when FIFO is empty)

On every T-cycle, the PPU also pushes a pixel to the screen using an internal render
counter if the FIFO isn't empty. The render counter also checks for objects and window tiles and communicates to the fetcher what type of data to fetch and which FIFO to push data into. 
This state takes 172-289 T-Cycles to complete.
### H-blank 
After all 160 pixels are drawn in a line, the PPU stalls until 456 T-cycles have elapsed since the start of the line; this can range from 87-204 T-cycles to complete.
### V-blank
After all 144 scan lines are drawn, the PPU waits for 10 scan lines worth of cycles before starting again at the first scanline.
## Memory
Currently, the ByteBoy supports MBC1 and ROM-only titles. When a game attempts to write in ROM, the data being written is instead used to update internal MBC registers that hold information 
such as ROM/RAM bank number, RAM-enable, and banking mode. When accessing areas of memory that are sourced from the cartridge's external memory, the bank numbers held in the MBC registers are 
concatenated onto the original address to create the new address needed to index into cartridge memory.
## Testing 
The CPU was tested using [Blargg's test ROMS](https://gbdev.gg8.se/files/roms/blargg-gb-tests/) aided by [GameBoy Doctor](https://robertheaton.com/gameboy-doctor/).
The PPU was tested with [dmg-acid2](https://github.com/mattcurrie/dmg-acid2) by Matt Currie.

| Test| Status |
|-----|--------|
| cpu_instr | ✅      |
| instr_timing| ✅      |
 |memory_timing| ❌      |

## Controls
| Game Boy | Byte Boy |
|----------|----------|
|Up|W|
|Left|A|
|Down|S|
|Right|D|
|A|H|
|B|J|
|Select|K|
|Start|L|

### Resources
- [Pan Docs](https://gbdev.io/pandocs/) 
- [DECODING Gameboy Z80 OPCODES](https://archive.gbdev.io/salvage/decoding_gbz80_opcodes/Decoding%20Gamboy%20Z80%20Opcodes.html)
- [Opcode Table](https://gbdev.io/gb-opcodes/optables/)
- [CPU opcode reference](https://rgbds.gbdev.io/docs/v0.9.2/gbz80.7#JP_n16)
- [Same Boy](https://sameboy.github.io/)
- [Blargg's Test Roms](https://gbdev.gg8.se/files/roms/blargg-gb-tests/)
- [Gameboy Doctor](https://robertheaton.com/gameboy-doctor/)
- [The Ultimate Game Boy Talk](https://www.youtube.com/watch?v=HyzD8pNlpwI)
- [The Gameboy Emulator Development Guide](https://hacktix.github.io/GBEDG/ppu/)
- [dmg-acid2](https://github.com/mattcurrie/dmg-acid2) and [improved README](https://github.com/MarcAlx/dmg-acid2)

