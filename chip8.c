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

	// the chip 8 cpu starts here

	// clear the stack, registers, graphics, memory
	memset(graphics, 0, sizeof(graphics));
	memset(stack, 0, sizeof(stack));
	memset(V, 0, sizeof(V));
	memset(memory, 0, sizeof(memory));

	// program counter starts at 0x200
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

	// load in the rom from dfs
	int dfp = dfs_open("pong.ch8");
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
			case 0xA000: // ANNN: Set I to NNN
				I = opcode & 0xFFF;
				pc += 2;
				break;

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

			default:
				cpu_running = 0;
		}

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

		for(int i = 0; i < 256; i++)
	        {
			graphics_draw_box(disp, 20 + i, 70, 1, 20, graphics_make_color(i, i, i, 255));
        	}

		for(int h = 0; h < 32; h++) {
			for(int w = 0; w < 64; w++) {
				pixel_color = graphics[w+h*64] ? graphics_make_color(255,255,255,255) : graphics_make_color(0,0,0,255);
				//pixel_color = graphics[w+h*64] ? graphics_make_color(0,0,0,255) : graphics_make_color(255,255,255,255);
				//graphics_draw_pixel(disp, wx+w, wy+h, graphics_make_color(255,255,255,255));
				graphics_draw_box(disp, wx+(w*10), wy+(h*10), 10, 10, pixel_color);
			}
		}

		display_show(disp);

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
