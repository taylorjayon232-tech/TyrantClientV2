// ============================================================================
//  Tyrant test helper — what does the agent's window lookup actually see?
//  Prints every top-level window (pid, class, title) and the result of the
//  desktop-wide FindWindowW(L"GLFW30") the agent used to use at install time.
// ============================================================================
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>

static BOOL CALLBACK scan(HWND h, LPARAM l) {
    (void)l;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    wchar_t cls[128] = L"";
    GetClassNameW(h, cls, 128);
    wchar_t title[256] = L"";
    GetWindowTextW(h, title, 256);
    const int visible = IsWindowVisible(h) ? 1 : 0;
    // Only interesting windows: those that actually own a GLFW/OpenGL surface or
    // carry a title (skip the thousands of invisible helper windows).
    if (title[0] || wcsstr(cls, L"GLFW") || wcsstr(cls, L"Tyrant")) {
        wprintf(L"pid=%-7lu vis=%d class=%-24s title=%s\n", (unsigned long)pid, visible, cls, title);
    }
    return TRUE;
}

int main(void) {
    wprintf(L"--- top-level windows ---\n");
    EnumWindows(&scan, 0);
    HWND fw = FindWindowW(L"GLFW30", NULL);
    wprintf(L"--- FindWindowW(L\"GLFW30\", nullptr) = %p ---\n", (void*)fw);
    if (fw) {
        DWORD pid = 0; GetWindowThreadProcessId(fw, &pid);
        wprintf(L"    belongs to pid %lu (this process is %lu)\n", (unsigned long)pid, (unsigned long)GetCurrentProcessId());
    }
    return 0;
}
