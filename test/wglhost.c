// ============================================================================
//  Tyrant test host — a minimal stand-in for Minecraft's render loop.
//  Creates a hidden window with a real OpenGL context and swaps frames, so
//  the injected agent's swap hooks fire on THIS machine without the game.
//  Prints its PID so the injector CLI can target it. Exits when done.
// ============================================================================
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")

static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_RSHIFT) {
        printf("HOST: WM_KEYDOWN VK_RSHIFT arrived (extended=%d)\n", (int)((l >> 24) & 1));
        fflush(stdout);
    }
    return DefWindowProcW(h, m, w, l);
}

int main(void) {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = Proc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.lpszClassName = L"TyrantTestGL";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Tyrant WGL Host",
                                WS_OVERLAPPEDWINDOW, 40, 40, 640, 480,
                                NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) { printf("HOST: CreateWindow failed\n"); return 1; }
    ShowWindow(hwnd, SW_HIDE);

    HDC dc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24;
    int pf = ChoosePixelFormat(dc, &pfd);
    if (!SetPixelFormat(dc, pf, &pfd)) { printf("HOST: SetPixelFormat failed\n"); return 1; }
    HGLRC rc = wglCreateContext(dc);
    if (!rc || !wglMakeCurrent(dc, rc)) { printf("HOST: GL context failed\n"); return 1; }

    printf("HOST: GL ready pid=%lu — swapping 3600 frames\n", GetCurrentProcessId());
    fflush(stdout);
    for (int i = 0; i < 3600; ++i) {
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        Sleep(33);
        if (!SwapBuffers(dc)) { printf("HOST: SwapBuffers failed\n"); return 1; }
    }
    printf("HOST: 3600 frames swapped — sleeping 3s then exiting\n");
    fflush(stdout);
    Sleep(3000);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    ReleaseDC(hwnd, dc);
    return 0;
}
