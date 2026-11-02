// ============================================================================
//  Tyrant test host (CORE profile) — mirrors Minecraft's context:
//  GL 3.2 CORE profile + forward compatible, created with
//  wglCreateContextAttribsARB. The original wglhost.exe uses the legacy
//  compatibility context from ChoosePixelFormat, so nothing it renders proves
//  anything about the core-profile path the game actually runs on.
//
//  Prints its PID so the injector CLI can target it. ImGui shader compile/link
//  errors are printed to stderr by the backend, which is where the interesting
//  evidence shows up.
// ============================================================================
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")

#ifndef GL_SHADING_LANGUAGE_VERSION
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#endif

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_FLAGS_ARB         0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB    0x0002

typedef HGLRC(WINAPI* PFNWGLCreateContextAttribsARB)(HDC, HGLRC, const int*);

static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcW(h, m, w, l);
}

int main(void) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc   = Proc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.lpszClassName = L"TyrantCoreGL";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Tyrant Core Host",
                                WS_OVERLAPPEDWINDOW, 40, 40, 640, 480,
                                NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) { printf("HOST: CreateWindow failed\n"); return 1; }
    ShowWindow(hwnd, SW_HIDE);

    HDC dc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    int pf = ChoosePixelFormat(dc, &pfd);
    if (!SetPixelFormat(dc, pf, &pfd)) { printf("HOST: SetPixelFormat failed\n"); return 1; }

    // A throwaway context is required only so the ARB entry point resolves.
    HGLRC tmp = wglCreateContext(dc);
    if (!tmp || !wglMakeCurrent(dc, tmp)) { printf("HOST: temp GL context failed\n"); return 1; }
    PFNWGLCreateContextAttribsARB createAttribs =
        (PFNWGLCreateContextAttribsARB)wglGetProcAddress("wglCreateContextAttribsARB");

    HGLRC rc = NULL;
    if (createAttribs) {
        const int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 2,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            WGL_CONTEXT_FLAGS_ARB,         WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
            0
        };
        rc = createAttribs(dc, NULL, attribs);
    }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(tmp);
    if (!rc) { printf("HOST: no 3.2 core context available\n"); fflush(stdout); return 1; }
    if (!wglMakeCurrent(dc, rc)) { printf("HOST: wglMakeCurrent(core) failed\n"); return 1; }

    printf("HOST: CORE profile ready pid=%lu version=%s glsl=%s\n",
           GetCurrentProcessId(),
           (const char*)glGetString(GL_VERSION),
           (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    fflush(stdout);

    const int W = 640, H = 480;
    unsigned char* px = (unsigned char*)malloc((size_t)W * H * 4);
    for (int i = 0; i < 900; ++i) {
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        Sleep(33);
        if (!SwapBuffers(dc)) { printf("HOST: SwapBuffers failed\n"); return 1; }

        // After 300 frames (the overlay has long since initialised) sample the
        // presented image: count pixels that are neither the clear colour nor
        // the overlaid backdrop, i.e. evidence the interface actually rasterised.
        if (i == 300 || i == 600) {
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_FRONT);
            glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
            long drawn = 0;
            for (long k = 0; k < (long)W * H; ++k) {
                const int r = px[k * 4], g = px[k * 4 + 1], b = px[k * 4 + 2];
                if (abs(r - 26) > 8 || abs(g - 51) > 8 || abs(b - 77) > 8) ++drawn;
            }
            printf("HOST: frame %d non-background pixels=%ld / %ld\n", i, drawn, (long)W * H);
            fflush(stdout);
            glReadBuffer(GL_BACK);
        }
    }
    printf("HOST: done\n");
    fflush(stdout);
    Sleep(1000);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    ReleaseDC(hwnd, dc);
    return 0;
}
