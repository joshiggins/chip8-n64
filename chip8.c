#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

static resolution_t res = RESOLUTION_640x480;
static bitdepth_t bit = DEPTH_32_BPP;

unsigned char fontset[80] =
{
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

int cpu_running;

unsigned short opcode;
unsigned char memory[4096];
unsigned char V[16];
unsigned short I;
unsigned short pc;

unsigned char graphics[64*32];

unsigned char delay_timer;
unsigned char sound_timer;

unsigned short stack[16];
unsigned short sp;

unsigned char keypad[16];

int main(void)
{
	// enable interrupts on the CPU
	init_interrupts();

	// initialize peripherals
	display_init( res, bit, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
	dfs_init( DFS_DEFAULT_LOCATION );
	static display_context_t disp = 0;

	char tStr[256];

	// the chip 8 display window pos
	int wx = 0;
	int wy = 100;
	uint32_t pixel_color = 0;
	int screen_damage = 0;

	// the chip 8 cpu starts here

	// clear the stack, registers, graphics, memory
	void secure_memzero(void *buf, size_t len) {
    volatile unsigned char *p = buf;
    while (len--) {
        *p++ = 0;
    }
}

int main() {
    unsigned char stack[SIZE];
    int V[SIZE];
    char memory[SIZE];

    // securely overwrite sensitive information in the buffers
    secure_memzero(graphics, sizeof(graphics));
    secure_memzero(stack, sizeof(stack));
    secure_memzero(V, sizeof(V));
    secure_memzero(memory, sizeof(memory));

    // program counter starts at 0x200
    // code continues...
    
    return 0;
}
	pc = 0x200;

	// reset opcode
	opcode = 0;

	// reset index register
	I = 0;

	// reset stack pointer
	sp = 0;

	// load in the fontset
	for (int i = 0; i < 80; i++) {
		memory[i] = fontset[i];
	}

	char otest;

	// load in the rom from dfs
	int dfp = dfs_open("blinky.ch8");
	int fsize = dfs_size(dfp);

	int fload = dfs_read(memory+512, 1, fsize, dfp);

	cpu_running = 1;

	while(cpu_running) {

		// fetch opcode
		opcode = memory[pc] << 8 | memory[pc + 1];

		// decode
		// we mask the first four bits
		switch(opcode & 0xF000)
		{

			// sometimes the first four bits is not enough to distinguish opcodes
			case 0x0000:
				switch(opcode & 0x000F)
				{
					case 0x0000: // 00E0: clear the screen
						memset(graphics, 0, sizeof(graphics));
						pc += 2;
						break;

					case 0x000E: // 00EE: return from a subroutine
						pc = stack[(--sp)&0xF]+2;
						break;

					default:
						cpu_running = 0;
				}
				break;

			case 0x1000: // 1NNN: Jump to NNN
				pc = opcode & 0x0FFF;
				break;

			case 0x2000: // 2NNN: Call subroutine at NNN
				// put current position on top of stack
				stack[(sp++)&0xF] = pc;
				// move to NNN
				pc = opcode & 0x0FFF;
				break;

			case 0x3000: // 3XNN: Skips the next instruction if VX equals NN
				if(V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF)) {
					pc += 4;
				} else {
					pc += 2;
				}
				break;

			case 0x4000: // 4XNN Skips the next instruction if VX doesn't equal NN
				if(V[(opcode & 0x0F00) >> 8] != (opcode &0x00FF)) {
					pc += 4;
				} else {
					pc += 2;
				}
				break;

			case 0x5000: // 5XY0 Skips the next instruction if VX equals VY
				if(V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4]) {
					pc += 4;
				} else {
					pc += 2;
				}
				break;

			case 0x6000: // 6XNN: Set VX to NN
				// mask off the F digit, and move it to the first digit to get X value for V[X] on its own
				V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
				pc += 2;
				break;

			case 0x7000: // 7XNN: Adds NN to VX
				V[(opcode & 0x0F00) >> 8] += (opcode & 0x00FF);
				pc += 2;
				break;

			case 0x8000:

				switch(opcode & 0x000F) {

					case 0x0000: // 8XY0 Sets VX to the value of VY
						V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4];
						pc += 2;
						break;

					case 0x0001: // 8XY1 Sets VX to VX or VY
						V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] | V[(opcode & 0x00F0) >> 4];
						pc += 2;
						break;

					case 0x0002: // 8XY2 Sets VX to VX and VY
						V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] & V[(opcode & 0x00F0) >> 4];
						pc += 2;
						break;

					case 0x0003: // 8XY3 Sets VX to VX xor VY
						V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] ^ V[(opcode & 0x00F0) >> 4];
						pc += 2;
						break;

					case 0x0004: // 8XY4 Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't
						if(((int)V[(opcode & 0x0F00) >> 8 ] + (int)V[(opcode & 0x00F0) >> 4]) < 256) {
							V[0xF] &= 0;
						} else {
							V[0xF] = 1;
						}
						// add
						V[(opcode & 0x0F00) >> 8] += V[(opcode & 0x00F0) >> 4];
						pc += 2;
						break;

					case 0x0005: // 8XY5 VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't
						if(((int)V[(opcode & 0x0F00) >> 8 ] - (int)V[(opcode & 0x00F0) >> 4]) >= 0) {
							V[0xF] = 1;
						} else {
							V[0xF] &= 0;
						}
						// subtract
						V[(opcode & 0x0F00) >> 8] -= V[(opcode & 0x00F0) >> 4];
						pc += 2;
						break;

					case 0x0006: // 8XY6 Shifts VX right by one. VF is set to the value of the least significant bit of VX before the shift
						V[0xF] = V[(opcode & 0x0F00) >> 8] & 7;
						V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] >> 1;
						pc += 2;
						break;

					case 0x0007: // 8XY7 Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't
						if(((int)V[(opcode & 0x0F00) >> 8] - (int)V[(opcode & 0x00F0) >> 4]) > 0) {
							V[0xF] = 1;
						} else {
							V[0xF] &= 0;
						}
						// subtract
						V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4] - V[(opcode & 0x0F00) >> 8];
						pc += 2;
						break;

					case 0x000E: // 8XYE Shifts VX left by one. VF is set to the value of the most significant bit of VX before the shift
						V[0xF] = V[(opcode & 0x0F00) >> 8] >> 7;
						V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x0F00) >> 8] << 1;
						pc += 2;
						break;

					default:
						cpu_running = 0;

				}
				break;

			case 0x9000: // 9XY0: Skips the next instruction if VX doesn't equal VY
				if(V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4]) {
					pc += 4;
				} else {
					pc += 2;
				}
				break;

			case 0xA000: // ANNN: Sets I to the address NNN
				I = opcode & 0x0FFF;
				pc += 2;
				break;

			case 0xB000: // BNNN: Jumps to the address NNN plus V0
				pc = (opcode & 0x0FFF) + V[0];
				break;

			case 0xC000: // CXNN: Sets VX to a random number and NN
				V[(opcode & 0x0F00) >> 8] = rand() & (opcode & 0x00FF);
				pc += 2;
				break;

			case 0xD000: // DXYN: Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels
				; //quirk
				unsigned short x = V[(opcode & 0x0F00) >> 8];
				unsigned short y = V[(opcode & 0x00F0) >> 4];
				unsigned short height = opcode & 0x000F;
				unsigned short pixel;

				V[0xF] = 0;
				for (int yline = 0; yline < height; yline++) {
					pixel = memory[I + yline];
					for(int xline = 0; xline < 8; xline++) {
						if((pixel & (0x80 >> xline)) != 0) {
							if(graphics[(x + xline + ((y + yline) * 64))] == 1) {
								V[0xF] = 1;
							}
							graphics[x + xline + ((y + yline) * 64)] ^= 1;
						}
					}
				}
				screen_damage = 1;
				pc += 2;
				break;

			case 0xE000:
				switch(opcode & 0x000F) {

					case 0x000E: // EX9E: Skips the next instruction if the key stored in VX is pressed
						// keys not implemented yet
						pc += 2;
						break;

					case 0x0001: // EXA1: Skips the next instruction if the key stored in VX isn't pressed
						// keys not implemented yet
						pc += 4;
						break;

					default:
						cpu_running = 0;

				}
				break;

			case 0xF000:
				switch(opcode & 0x00FF) {

					case 0x0007: // FX07: Sets VX to the value of the delay timer
						V[(opcode & 0x0F00) >> 8] = delay_timer;
						pc += 2;
						break;

					case 0x000A: // FX0A: A key press is awaited, and then stored in VX
						// keys not implemented yet
						//pc += 2;
						break;

					case 0x0015: // FX15: Sets the delay timer to VX
						delay_timer = V[(opcode & 0x0F00) >> 8];
						pc += 2;
						break;

					case 0x0018: // FX18: Sets the sound timer to VX
						sound_timer = V[(opcode & 0x0F00) >> 8];
						pc += 2;
						break;

					case 0x001E: // FX1E: Adds VX to I
						I += V[(opcode & 0x0F00) >> 8];
						pc += 2;
						break;

					case 0x0029: // FX29: Sets I to the location of the sprite for the character in VX. Characters 0-F (in hexadecimal) are represented by a 4x5 font
						I = V[(opcode & 0x0F00) >> 8] * 5;
						pc += 2;
						break;

					case 0x0033: // FX33: Stores the Binary-coded decimal representation of VX, with the most significant of three digits at the address in I, the middle digit at I plus 1, and the least significant digit at I plus 2
						memory[I] = V[(opcode & 0x0F00) >> 8] / 100;
						memory[I+1] = (V[(opcode & 0x0F00) >> 8] / 10) % 10;
						memory[I+2] = V[(opcode & 0x0F00) >> 8] % 10;
						pc += 2;
						break;

					case 0x0055: // FX55: Stores V0 to VX in memory starting at address I
						for(int i = 0; i <= ((opcode & 0x0F00) >> 8); i++) {
							memory[I+i] = V[i];
						}
						pc += 2;
						break;

					case 0x0065: // FX65: Fills V0 to VX with values from memory starting at address I
						for(int i = 0; i <= ((opcode & 0x0F00) >> 8); i++) {
							V[i] = memory[I + i];
						}
						pc += 2;
						break;

					default:
						cpu_running = 0;
				}
				break;

			default:
				cpu_running = 0;
		}

		if (screen_damage == 1) {

		// draw the screen
		while( !(disp = display_lock()) );
		graphics_fill_screen( disp, 0 );

		sprintf(tStr, "CHIP8 emulator for N64");
		graphics_draw_text( disp, 20, 20, tStr );
        	sprintf(tStr, "ROM: %d/%d bytes\n", fsize, fload);
        	graphics_draw_text( disp, 20, 30, tStr );
        	sprintf(tStr, "PC: %d\n", pc);
        	graphics_draw_text( disp, 20, 40, tStr );
        	sprintf(tStr, "Last opcode: %02X\n", opcode);
        	graphics_draw_text( disp, 20, 50, tStr );
		sprintf(tStr, "Memory: ");
		for (int i = pc - 10; i < pc+10; i = i +2) {
			sprintf(tStr+strlen(tStr), "%02X%02X ", memory[i], memory[i+1]);
		}
		graphics_draw_text (disp, 20, 60, tStr );

		//for(int i = 0; i < 256; i++)
	        //{
		//	graphics_draw_box(disp, 20 + i, 70, 1, 20, graphics_make_color(i, i, i, 255));
        	//}

			for(int h = 0; h < 32; h++) {
				for(int w = 0; w < 64; w++) {
					pixel_color = graphics[w+h*64] ? graphics_make_color(255,255,255,255) : graphics_make_color(0,0,0,255);
					//pixel_color = graphics[w+h*64] ? graphics_make_color(0,0,0,255) : graphics_make_color(255,255,255,255);
					//graphics_draw_pixel(disp, wx+w, wy+h, graphics_make_color(255,255,255,255));
					graphics_draw_box(disp, wx+(w*10), wy+(h*10), 10, 10, pixel_color);
				}
			}
		screen_damage = 0;
		display_show(disp);

		}

		// update timers
		if (delay_timer > 0) {
			--delay_timer;
		}
		if (sound_timer > 0) {
			--sound_timer;
		}

	}

	while( !(disp = display_lock()) );
	sprintf(tStr, "                               HALT invalid opcode %04X", opcode);
       	graphics_draw_text(disp, 20, 20, tStr);
	display_show(disp);
}
