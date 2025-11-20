#include <windows.h>

#define CHIP8_SCREEN_HEIGHT (32)
#define CHIP8_SCREEN_WIDTH  (64)
#define PROGRAM_START 0x200
#define MEMORY_SIZE 4096

#define INSTRUCTIONS_PER_TICK 10
#define MEM_AFFECT_I TRUE

const char fontData[] = {
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

LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

typedef struct {
	BOOL isKeyDown[16];
} KeyInfo;

typedef struct {
	UINT8* MEM;        // Memory
	UINT16  I;          // Address Register
	UINT8   REG[16];    // General Purpose Registers
	UINT16  PC;         // Program Counter
	UINT8   DTIMER;     // Delay Timer
	UINT8   STIMER;     // Sound Timer
	UINT16  STACK[8];   // PC Stack
	UINT8   SP;         // Stack Pointer
} Chip8State;

typedef struct {
	UINT8* pixels;
	BITMAPINFO bitmapInfo;
	HBITMAP hBitmap;

} Framebuffer;

Framebuffer chipScreen = {
	.pixels = NULL,
	.bitmapInfo = {
		.bmiHeader = {
			.biSize = sizeof(BITMAPINFOHEADER),
			.biWidth = CHIP8_SCREEN_WIDTH,
			.biHeight = -CHIP8_SCREEN_HEIGHT,
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB
		}
	},
	.hBitmap = NULL
};

KeyInfo keyInfo = { 0 };

// Needed to block for a key press
static UINT8 pressedKeyCode = 0xFF;
static BOOL needsKeyCode = FALSE;

UINT8 mapKeyCode(WPARAM virtualKeyCode) {
	switch (virtualKeyCode) {
	case 0x31: return 0;  // ('1') -> 0
	case 0x32: return 1;  // ('2') -> 1
	case 0x33: return 2;  // ('3') -> 2
	case 0x34: return 3;  // ('4') -> 3
	case 0x51: return 4;  // ('Q') -> 4
	case 0x57: return 5;  // ('W') -> 5
	case 0x45: return 6;  // ('E') -> 6
	case 0x52: return 7;  // ('R') -> 7
	case 0x41: return 8;  // ('A') -> 8
	case 0x53: return 9;  // ('S') -> 9
	case 0x44: return 10; // ('D') -> 10
	case 0x46: return 11; // ('F') -> 11
	case 0x5A: return 12; // ('Z') -> 12
	case 0x58: return 13; // ('X') -> 13
	case 0x43: return 14; // ('C') -> 14
	case 0x56: return 15; // ('V') -> 15
	}

	return 0xFF;
}

void startSound() {
	// Run this tone on loop
	PlaySound(TEXT("250hz.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
}

void endSound() {
	// End all playing sounds
	PlaySound(NULL, 0, 0);
}

void updateTimers(Chip8State* chipState) {
	if (chipState->DTIMER) --chipState->DTIMER;
	// Decrement the soundTimer, but if it's already 0, stop any playing sounds
	if (chipState->STIMER) --chipState->STIMER;
	else endSound();
}

int updateState(Chip8State* chipState) {
	UINT16 instr = (chipState->MEM[chipState->PC] << 8) | (chipState->MEM[chipState->PC + 1]);
	UINT8 opcode = instr >> 12 & 0xF;
	UINT8 reg1 = (instr >> 8) & 0xF;
	UINT8 reg2 = (instr >> 4) & 0xF;
	UINT16 lower12 = instr & 0xFFF;
	UINT8 lower8 = instr & 0xFF;
	UINT8 lower4 = instr & 0xF;

	BOOL updatePC = TRUE;
	switch (opcode) {
	case 0: {
		if (reg1 != 0) return -1; // Unimplemented

		if (lower8 == 0xE0) { // Clear Screen
			memset(chipScreen.pixels, 0, CHIP8_SCREEN_HEIGHT * CHIP8_SCREEN_WIDTH * 4);
		}
		else if (lower8 == 0xEE) { // Return from subroutine call
			--chipState->SP;
			chipState->PC = chipState->STACK[chipState->SP];
		}
	} break;
	case 1: {
		updatePC = FALSE;
		chipState->PC = lower12;
	} break;
	case 2: {
		updatePC = FALSE;
		chipState->STACK[chipState->SP] = chipState->PC;
		++chipState->SP;
		chipState->PC = lower12;
	} break;
	case 3: {
		UINT8 val = chipState->REG[reg1];
		if (val == lower8) chipState->PC += 2;
	} break;
	case 4: {
		UINT8 val = chipState->REG[reg1];
		if (val != lower8) chipState->PC += 2;
	} break;
	case 5: {
		UINT8 val1 = chipState->REG[reg1];
		UINT8 val2 = chipState->REG[reg2];
		if (val1 == val2) chipState->PC += 2;
	} break;
	case 6: {
		chipState->REG[reg1] = lower8;
	} break;
	case 7: {
		chipState->REG[reg1] += lower8;
	} break;
	case 8: {
		switch (lower4) {
		case 0: {
			chipState->REG[reg1] = chipState->REG[reg2];
		} break;
		case 1: {
			chipState->REG[reg1] |= chipState->REG[reg2];
		} break;
		case 2: {
			chipState->REG[reg1] &= chipState->REG[reg2];
		} break;
		case 3: {
			chipState->REG[reg1] ^= chipState->REG[reg2];
		} break;
		case 4: {
			UINT8 prev = chipState->REG[reg1];
			chipState->REG[reg1] += chipState->REG[reg2];
			if (prev > chipState->REG[reg1]) chipState->REG[0xF] = 1;
			else chipState->REG[0xF] = 0;
		} break;
		case 5: {
			if (chipState->REG[reg1] > chipState->REG[reg2]) chipState->REG[0xF] = 1;
			else chipState->REG[0xF] = 0;
			chipState->REG[reg1] -= chipState->REG[reg2];
		} break;
		case 6: {
			UINT8 lsb = chipState->REG[reg2] & 1;
			chipState->REG[reg1] = chipState->REG[reg2] >> 1;
			chipState->REG[0xF] = lsb;
		} break;
		case 7: {
			if (chipState->REG[reg2] > chipState->REG[reg1]) chipState->REG[0xF] = 1;
			else chipState->REG[0xF] = 0;
			chipState->REG[reg1] = chipState->REG[reg2] - chipState->REG[reg1];
		} break;
		case 0xE: {
			UINT8 msb = (chipState->REG[reg2] >> 7) & 1;
			chipState->REG[reg1] = chipState->REG[reg2] << 1;
			chipState->REG[0xF] = msb;
		} break;
		}
	} break;
	case 9: {
		UINT8 val1 = chipState->REG[reg1];
		UINT8 val2 = chipState->REG[reg2];
		if (val1 != val2) chipState->PC += 2;
	} break;
	case 0xA: {
		chipState->I = lower12;
	} break;
	case 0xB: {
		updatePC = FALSE;
		chipState->PC = chipState->REG[0] + lower12;
	} break;
	case 0xC: { // TODO: make this more random
		chipState->REG[reg1] = (0xAB * (chipState->REG[reg1] ^ chipState->PC) * 17) % 255;
	} break;
	case 0xD: {
		int x = chipState->REG[reg1] % 64;
		int y = chipState->REG[reg2] % 32;
		int n = lower4;
		chipState->REG[0xF] = 0;
		for (int ypos = y; ypos < y + n && ypos < 32; ypos++) {
			UINT8 data = chipState->MEM[chipState->I + (ypos - y)];
			UINT8 mask = 0x80;
			for (int xpos = 0; xpos < 8 && x + xpos < 64; xpos++) {
				BOOL color = (data & mask);
				mask >>= 1;
				UINT8 xorVal = color ? 255 : 0;
				int pixelIdx = ypos * 64 * 4 + (x + xpos) * 4;
				if (color && chipScreen.pixels[pixelIdx]) chipState->REG[0xF] = 1;
				chipScreen.pixels[pixelIdx + 0] ^= xorVal;
				chipScreen.pixels[pixelIdx + 1] ^= xorVal;
				chipScreen.pixels[pixelIdx + 2] ^= xorVal;
				chipScreen.pixels[pixelIdx + 3] ^= xorVal;
			}
		}
	} break;
	case 0xE: {
		BOOL keyPressed = keyInfo.isKeyDown[chipState->REG[reg1]];
		if (lower8 == 0x9E && keyPressed) {
			chipState->PC += 2;
		}
		else if (lower8 == 0xA1 && !keyPressed) {
			chipState->PC += 2;
		}
	} break;
	case 0xF: {
		switch (lower8) {
		case 0x7: {
			chipState->REG[reg1] = chipState->DTIMER;
		} break;
		case 0xA: {// Use globals to communicate with event processor
			// 2 variables, keyCode = 0xFF and needsKey = False
			// when this needs a value, it sets needsKey to True, and doesn't update the PC, and does this
			// in a loop while we wait for input
			// the input processing sets keyCode to the first keyDown it gets IF needsKey == True
			// then when this instruction runs again it can consume the keyCode, clear it and set needsKey to false
			if (pressedKeyCode != 0xFF) {
				needsKeyCode = FALSE;
				chipState->REG[reg1] = pressedKeyCode;
				pressedKeyCode = 0xFF;
			}
			else {
				needsKeyCode = TRUE;
			}
		} break;
		case 0x15: {
			chipState->DTIMER = chipState->REG[reg1];
		} break;
		case 0x18: {
			chipState->STIMER = chipState->REG[reg1];
			if (chipState->STIMER) startSound();
		} break;
		case 0x1E: {
			chipState->I += chipState->REG[reg1];
		} break;
		case 0x29: {
			chipState->I = chipState->REG[reg1] * 5;
		} break;
		case 0x33: {
			int x = chipState->REG[reg1];
			for (int i = 0; i < 3; i++) {
				UINT8 digit = x % 10;
				x /= 10;
				chipState->MEM[chipState->I + (2 - i)] = digit;
			}
		} break;
		case 0x55: {
			int x = reg1;
			for (int i = 0; i <= x; i++) {
				chipState->MEM[chipState->I + i] = chipState->REG[i];
			}
			if (MEM_AFFECT_I) chipState->I += x + 1;
		} break;
		case 0x65: {
			int x = reg1;
			for (int i = 0; i <= x; i++) {
				chipState->REG[i] = chipState->MEM[chipState->I + i];
			}
			if (MEM_AFFECT_I) chipState->I += x + 1;
		} break;
		}
	} break;
	}

	if (updatePC) chipState->PC += 2;

	return 0;
}

void loadProgram(Chip8State* chipState, HWND hwnd) {
	HANDLE hFile = CreateFileA(
		"PONG2",
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hFile == INVALID_HANDLE_VALUE) {
		MessageBoxA(hwnd, "Failed to open ROM file!", NULL, MB_ICONERROR);
		PostQuitMessage(0);
		return;
	}

	DWORD fileSize = GetFileSize(hFile, NULL);
	if (fileSize == INVALID_FILE_SIZE) {
		MessageBoxA(hwnd, "Failed to get file size!", NULL, MB_ICONERROR);
		CloseHandle(hFile);
		PostQuitMessage(0);
		return;
	}

	BOOL success = ReadFile(
		hFile,
		chipState->MEM + PROGRAM_START,
		fileSize,
		NULL,
		NULL
	);
	CloseHandle(hFile);

	if (!success) {
		MessageBoxW(hwnd, L"Failed to read file!", NULL, MB_ICONERROR);
		PostQuitMessage(0);
		return;
	}

	memcpy(chipState->MEM, fontData, 80);
}

void gameLoop(HWND hwnd) {
	Chip8State chipState = {
		.MEM = malloc(MEMORY_SIZE),
		.I = 0,
		.REG = {0},
		.PC = 0x200,
		.DTIMER = 0,
		.STIMER = 0,
		.STACK = {0},
		.SP = 0
	};

	loadProgram(&chipState, hwnd);

	for (;;) {
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				free(chipState.MEM);
				return;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// Update Game State
		for (int i = 0; i < INSTRUCTIONS_PER_TICK; i++)
			updateState(&chipState);
		// Update Timer States
		updateTimers(&chipState);

		// TODO: maybe need to update screen after every DRAW instruction
		// not just after a batch of instructions
		InvalidateRect(hwnd, NULL, FALSE);
		Sleep(15); // TODO: Adjust sleep based on how long the emulation updates took
	}

	free(chipState.MEM);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_KEYDOWN: {
		UINT8 keyCode = mapKeyCode(wParam);
		if (keyCode == 0xFF) return 0;
		if (needsKeyCode) pressedKeyCode = keyCode;
		keyInfo.isKeyDown[keyCode] = TRUE;
		return 0;
	}
	case WM_KEYUP: {
		UINT8 keyCode = mapKeyCode(wParam);
		if (keyCode == 0xFF) return 0;
		keyInfo.isKeyDown[keyCode] = FALSE;
		return 0;
	}
	case WM_DESTROY: {
		PostQuitMessage(0);
		return 0;
	}
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		RECT clientRect;
		GetClientRect(hwnd, &clientRect);

		StretchDIBits(
			hdc,
			0,
			0,
			clientRect.right - clientRect.left,
			clientRect.bottom - clientRect.top,
			0,
			0,
			CHIP8_SCREEN_WIDTH,
			CHIP8_SCREEN_HEIGHT,
			chipScreen.pixels,
			&chipScreen.bitmapInfo,
			DIB_RGB_COLORS,
			SRCCOPY
		);

		EndPaint(hwnd, &ps);
		return 0;
	}
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR pCmdLine,
	_In_ int nCmdShow
) {
	// Register the window class.
	const wchar_t CLASS_NAME[] = L"CHIP8-EMULATOR";

	WNDCLASS wc = { 0 };

	wc.lpfnWndProc = windowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,                              // Optional window styles.
		CLASS_NAME,                     // Window class
		CLASS_NAME,                     // Window text
		WS_OVERLAPPEDWINDOW,            // Window style
		// Size and position
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,       // Parent window    
		NULL,       // Menu
		hInstance,  // Instance handle
		NULL        // Additional application data
	);

	if (hwnd == NULL) return 0xDEADBEEF;

	ShowWindow(hwnd, nCmdShow);

	HDC hdc = GetDC(hwnd);

	chipScreen.hBitmap = CreateDIBSection(hdc, &chipScreen.bitmapInfo, DIB_RGB_COLORS, &chipScreen.pixels, NULL, 0);
	if (chipScreen.hBitmap == NULL) {
		ReleaseDC(hwnd, hdc);
		return -1;
	}
	gameLoop(hwnd);

	ReleaseDC(hwnd, hdc);
	DeleteObject(chipScreen.hBitmap);

	return 0;
}