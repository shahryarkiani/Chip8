#include <windows.h>
#define CHIP8_SCREEN_HEIGHT (32)
#define CHIP8_SCREEN_WIDTH  (64)

LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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

Framebuffer chip8Screen = {
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
        for (int i = 0; i < CHIP8_SCREEN_HEIGHT * CHIP8_SCREEN_WIDTH * 4; i++) {
            chip8Screen.pixels[(j * 17) % (CHIP8_SCREEN_HEIGHT * CHIP8_SCREEN_WIDTH * 4)] = j % 0xFF;
            j++;
        }

        InvalidateRect(hwnd, NULL, FALSE);
        Sleep(15);
    }

    free(chipState.MEM);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Register the window class.
    const wchar_t CLASS_NAME[] = L"CHIP8-EMULATOR";

    WNDCLASS wc = {0};

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

    chip8Screen.hBitmap = CreateDIBSection(hdc, &chip8Screen.bitmapInfo, DIB_RGB_COLORS, &chip8Screen.pixels, NULL, 0);
    if (chip8Screen.hBitmap == NULL) {
        return -1;
    }
    gameLoop(hwnd);

    ReleaseDC(hwnd, hdc);
    DeleteObject(chip8Screen.hBitmap);

    return 0;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {   
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
                chip8Screen.pixels, 
                &chip8Screen.bitmapInfo,
                DIB_RGB_COLORS,
                SRCCOPY
            );

            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}