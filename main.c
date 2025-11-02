#include <windows.h>
#define CHIP8_SCREEN_HEIGHT (32)
#define CHIP8_SCREEN_WIDTH  (64)

LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

typedef struct {
    BOOL isKeyDown[16];
} KeyInfo;

typedef struct {
    UINT8*  MEM;        // Memory
    UINT16  I;          // Address Register
    UINT8   REG[16];    // General Purpose Registers
    UINT16  PC;         // Program Counter
    UINT16  DTIMER;     // Delay Timer
    UINT16  STIMER;     // Sound Timer
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
        case 0x44: return 11; // ('D') -> 10
        case 0x46: return 12; // ('F') -> 11
        case 0x5A: return 13; // ('Z') -> 12
        case 0x58: return 14; // ('X') -> 13
        case 0x43: return 15; // ('C') -> 14
        case 0x56: return 16; // ('V') -> 15
    }

    return 0xFF;
}

void updateState(Chip8State* chipState) {

}

void gameLoop(HWND hwnd) {
    Chip8State chipState = {
        .MEM = malloc(4096),
        .I = 0,
        .REG = {0},
        .PC = 0x200,
        .DTIMER = 0,
        .STIMER = 0,
        .STACK = {0},
        .SP = 0
    };

    int j = 0;

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
        updateState(&chipState);

        // Update Screen
        for (int i = 0; i < 64; i += 4) {
            chipScreen.pixels[i] = keyInfo.isKeyDown[i / 4] * 0xFF;
            chipScreen.pixels[i + 1] = keyInfo.isKeyDown[i / 4] * 0xFF;
            chipScreen.pixels[i + 2] = keyInfo.isKeyDown[i / 4] * 0xFF;
            chipScreen.pixels[i + 3] = 0xFF;
        }

        InvalidateRect(hwnd, NULL, FALSE);
        Sleep(15);
    }

    free(chipState.MEM);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_KEYDOWN: {
            UINT8 keyCode = mapKeyCode(wParam);
            if (keyCode == 0xFF) return;
            keyInfo.isKeyDown[keyCode] = TRUE;
            return 0;
        }
        case WM_KEYUP: {
            UINT8 keyCode = mapKeyCode(wParam);
            if (keyCode == 0xFF) return;
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
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