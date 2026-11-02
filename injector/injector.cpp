// ============================================================================
//  Passive — Injector v1.0.1
//  Sleek, ultra-smooth desktop utility interface for Passive Client injection.
//  DX11 + ImGui, graphite/platinum aesthetic, multi-instance discovery, state cards.
// ============================================================================
#include "injector_settings.h"
#include "brand_logo.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <windows.h>
#include <shellapi.h>   // CommandLineToArgvW
#include <tlhelp32.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <mutex>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

// ---------------------------------------------------------------------------
// Injection State & Thread Synchronization
// ---------------------------------------------------------------------------
enum class State { Idle, Finding, Injecting, Success, Error };

static State       g_state = State::Idle;
static std::string g_error;
static std::string g_status = "waiting for minecraft";
static DWORD       g_pid = 0;
static std::string g_ver = "1.0.2";
static std::mutex  g_stateMutex;
struct StateView { State state; std::string status; std::string error; DWORD pid; };

static StateView stateView() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return {g_state, g_status, g_error, g_pid};
}

static void publishState(State state, const std::string& status, const std::string& error = {}, DWORD pid = MAXDWORD) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_state = state; g_status = status; g_error = error;
    if (pid != MAXDWORD) g_pid = pid;
}

// ---------------------------------------------------------------------------
// UI Palette & Animations
// ---------------------------------------------------------------------------
// Platinum: neutral graphite planes with brushed-metal highlights.
static const ImU32 kBgColor       = IM_COL32(10, 11, 13, 255);    // #0A0B0D Graphite
static const ImU32 kHeaderBg      = IM_COL32(15, 16, 19, 245);    // #0F1013 Title bar
static const ImU32 kCardBg        = IM_COL32(20, 21, 24, 255);    // #141518 Card background
static const ImU32 kCardBorder    = IM_COL32(58, 62, 68, 255);    // #3A3E44 Platinum hairline
static const ImU32 kAccentPlatinum= IM_COL32(198, 208, 220, 255); // #C6D0DC Primary accent
static const ImU32 kAccentBlue    = IM_COL32(150, 170, 195, 255); // #96AAC3 Secondary accent
static const ImU32 kTextPrimary   = IM_COL32(236, 238, 242, 255); // Primary text
static const ImU32 kTextSecondary = IM_COL32(140, 146, 156, 255); // Muted text
static const ImU32 kSuccessGreen  = IM_COL32(54, 211, 153, 255);  // Green status
static const ImU32 kErrorRed      = IM_COL32(248, 113, 113, 255); // Red status
static const ImU32 kWarnYellow    = IM_COL32(255, 184, 77, 255);  // Yellow status

static float  g_pulse = 0.f;          // 0..1 breathing phase
static float  g_hover = 0.f;          // hover anim
static float  g_press = 0.f;          // press anim
static float  g_spin  = 0.f;          // working spinner
static float  g_rippleT = -1.f;       // click ripple age, -1 = none
static ImVec2 g_ripplePos{};
static float  g_successT = 0.f;       // success burst anim

struct Particle { ImVec2 p, v; float life, maxLife, size; };
static std::vector<Particle> g_parts;
static std::mt19937 g_rng{ 1234567u };

static ImFont* g_fontReg = nullptr;
static ImFont* g_fontBig = nullptr;

static ImU32 alpha(ImU32 col, float a) {
    const unsigned oldA = (col >> IM_COL32_A_SHIFT) & 0xFFu;
    const unsigned newA = (unsigned)std::clamp((float)oldA * std::clamp(a, 0.f, 1.f), 0.f, 255.f);
    return (col & ~IM_COL32_A_MASK) | (newA << IM_COL32_A_SHIFT);
}

static ImU32 Accent(float a = 1.f) { return alpha(kAccentPlatinum, a); }

static float ease(float cur, float target, float speed, float dt) {
    const float t = 1.f - std::exp(-speed * dt);
    return cur + (target - cur) * t;
}
static float rand01() { return std::uniform_real_distribution<float>(0.f, 1.f)(g_rng); }

static ImFont* loadFontWide(const std::wstring& path, float size) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) return nullptr;
    _fseeki64(file, 0, SEEK_END);
    const __int64 length = _ftelli64(file);
    _fseeki64(file, 0, SEEK_SET);
    if (length <= 0 || length > 32 * 1024 * 1024) { fclose(file); return nullptr; }
    void* data = IM_ALLOC((size_t)length);
    const size_t read = fread(data, 1, (size_t)length, file);
    fclose(file);
    if (read != (size_t)length) { IM_FREE(data); return nullptr; }
    ImFontConfig cfg{};
    cfg.FontDataOwnedByAtlas = true;
    cfg.OversampleH = 3;
    cfg.OversampleV = 3;
    cfg.PixelSnapH = true;
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, (int)length, size, &cfg);
}

static std::wstring executableDirectory() {
    wchar_t path[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return L"";
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *(slash + 1) = 0;
    return path;
}

// ---------------------------------------------------------------------------
// Process discovery helpers
// ---------------------------------------------------------------------------
static bool processHasModule(DWORD pid, const wchar_t* dllName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    bool found = false;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, dllName) == 0) { found = true; break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

static bool agentMutexExists(DWORD pid) {
    wchar_t name[64]{};
    _snwprintf_s(name, _TRUNCATE, L"Passive_Agent_Mutex_%lu", (unsigned long)pid);
    HANDLE m = OpenMutexW(SYNCHRONIZE, FALSE, name);
    if (m) { CloseHandle(m); return true; }
    return false;
}

static bool agentAlreadyLoaded(DWORD pid) {
    return processHasModule(pid, L"passive_agent.dll");
}

// ---------------------------------------------------------------------------
// Auto-injection state
// The auto-injection thread and the two manual worker threads share one inject
// lock, so two LoadLibrary calls can never race into the same JVM. The PID sets
// are guarded by their own mutex because the settings view reads them every
// frame while the auto thread walks them.
// ---------------------------------------------------------------------------
static std::mutex g_injectMutex;
static std::mutex g_autoSetsMutex;
static std::unordered_set<DWORD> g_passiveSeen;    // instances Passive has been loaded into
static std::unordered_set<DWORD> g_autoSkipped;   // ones the user uninjected: never auto again

static void clearAutoSkipFor(DWORD pid) {
    std::lock_guard<std::mutex> lock(g_autoSetsMutex);
    g_autoSkipped.erase(pid);
}

int injectorAutoSkipCount() {
    std::lock_guard<std::mutex> lock(g_autoSetsMutex);
    return (int)g_autoSkipped.size();
}

int injectorAutoSeenCount() {
    std::lock_guard<std::mutex> lock(g_autoSetsMutex);
    return (int)g_passiveSeen.size();
}

void injectorClearAutoSkips() {
    std::lock_guard<std::mutex> lock(g_autoSetsMutex);
    g_autoSkipped.clear();
}

// Defined after the window handle exists; the workers call it on success so the
// "close after injection" setting does not need the handle this early.
static void maybeCloseAfterInject();

// ---------------------------------------------------------------------------
// Multi-instance discovery
// ---------------------------------------------------------------------------
struct InstanceInfo {
    DWORD        pid = 0;
    bool         windowed = false;
    bool         injected = false;
    std::wstring title;
};

static std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out((size_t)size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(), out.data(), size, nullptr, nullptr);
    return out;
}

struct WindowLookup { DWORD pid; std::wstring title; bool found; };

static BOOL CALLBACK windowLookupProc(HWND hwnd, LPARAM lParam) {
    WindowLookup* lookup = reinterpret_cast<WindowLookup*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != lookup->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) return TRUE;
    std::wstring buffer((size_t)length + 1, L'\0');
    const int got = GetWindowTextW(hwnd, buffer.data(), length + 1);
    if (got <= 0) return TRUE;
    buffer.resize((size_t)got);
    if (!buffer.empty()) { lookup->title = buffer; lookup->found = true; return FALSE; }
    return TRUE;
}

static std::vector<InstanceInfo> enumerateInstances() {
    std::vector<InstanceInfo> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"javaw.exe") != 0 &&
                _wcsicmp(pe.szExeFile, L"java.exe") != 0) continue;
            InstanceInfo info;
            info.pid = pe.th32ProcessID;
            WindowLookup lookup{ info.pid, {}, false };
            EnumWindows(&windowLookupProc, reinterpret_cast<LPARAM>(&lookup));
            info.windowed = lookup.found;
            info.title = lookup.title;
            info.injected = processHasModule(info.pid, L"passive_agent.dll");
            out.push_back(info);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    std::sort(out.begin(), out.end(), [](const InstanceInfo& a, const InstanceInfo& b) {
        if (a.windowed != b.windowed) return a.windowed;
        return a.pid < b.pid;
    });
    return out;
}

static bool findMinecraft(DWORD& outPid, std::string& err) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) { err = "could not enumerate processes"; return false; }
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    DWORD found = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"javaw.exe") == 0 ||
                _wcsicmp(pe.szExeFile, L"java.exe") == 0) {
                found = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (!found) { err = "minecraft not running (javaw.exe)"; return false; }
    outPid = found;
    return true;
}

static bool inject(DWORD pid, const std::wstring& dllPath, std::string& err) {
    HANDLE h = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                           PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                           FALSE, pid);
    if (!h) { err = "OpenProcess failed (run as admin?)"; return false; }

    const SIZE_T bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(h, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { CloseHandle(h); err = "VirtualAllocEx failed"; return false; }
    if (!WriteProcessMemory(h, remote, dllPath.c_str(), bytes, nullptr)) {
        VirtualFreeEx(h, remote, 0, MEM_RELEASE); CloseHandle(h); err = "WriteProcessMemory failed"; return false;
    }

    const HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    const auto loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(k32, "LoadLibraryW"));
    if (!loadLib) { VirtualFreeEx(h, remote, 0, MEM_RELEASE); CloseHandle(h); err = "LoadLibraryW not found"; return false; }

    HANDLE t = CreateRemoteThread(h, nullptr, 0, loadLib, remote, 0, nullptr);
    if (!t) { VirtualFreeEx(h, remote, 0, MEM_RELEASE); CloseHandle(h); err = "CreateRemoteThread failed"; return false; }

    const DWORD waitResult = WaitForSingleObject(t, 15000);
    if (waitResult == WAIT_TIMEOUT) {
        CloseHandle(t); CloseHandle(h);
        err = "LoadLibraryW did not finish within 15 seconds; restart Minecraft before retrying";
        return false;
    }
    if (waitResult != WAIT_OBJECT_0) {
        CloseHandle(t); CloseHandle(h);
        err = "could not wait for the target loader thread";
        return false;
    }
    DWORD exitCode = 0;
    const BOOL gotExitCode = GetExitCodeThread(t, &exitCode);
    CloseHandle(t);
    VirtualFreeEx(h, remote, 0, MEM_RELEASE);
    CloseHandle(h);

    if (!gotExitCode) { err = "could not read the target loader result"; return false; }
    if (exitCode == 0) { err = "DLL rejected by target (check agent dependencies)"; return false; }
    return true;
}

static void burst(ImVec2 center, int n) {
    for (int i = 0; i < n; ++i) {
        const float ang = rand01() * 6.28318f;
        const float spd = 40.f + rand01() * 200.f;
        Particle p;
        p.p = center;
        p.v = ImVec2(std::cos(ang) * spd, std::sin(ang) * spd - 40.f);
        p.maxLife = p.life = 0.6f + rand01() * 0.7f;
        p.size = 1.5f + rand01() * 2.5f;
        g_parts.push_back(p);
    }
}

// ---------------------------------------------------------------------------
// Inject Button Component
// ---------------------------------------------------------------------------
static bool DrawInjectButton(const char* label) {
    ImGuiIO& io = ImGui::GetIO();
    const float dt = io.DeltaTime;

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size{ 290.f, 54.f };
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 center{ pos.x + size.x * 0.5f, pos.y + size.y * 0.5f };

    StateView state = stateView();
    const bool enabled = (state.state == State::Idle || state.state == State::Error);
    ImGui::InvisibleButton(label, size, ImGuiButtonFlags_None);
    const bool hovered = enabled && ImGui::IsItemHovered();
    const bool clicked = enabled && ImGui::IsItemClicked();

    g_hover = ease(g_hover, hovered ? 1.f : 0.f, 14.f, dt);
    g_press = ease(g_press, (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)) ? 1.f : 0.f, 22.f, dt);
    g_pulse += dt;
    g_spin += dt * 4.5f;

    if (clicked) { g_rippleT = 0.f; g_ripplePos = io.MousePos; }
    if (g_rippleT >= 0.f) g_rippleT += dt;

    if (state.state == State::Success) {
        if (g_successT == 0.f) burst(center, 40);
        g_successT += dt;
        if (g_successT > 1.4f) {
            publishState(State::Idle, state.status, "", state.pid);
            state.state = State::Idle;
            g_successT = 0.f;
        }
    }

    const float breath = 0.5f + 0.5f * std::sin(g_pulse * 2.0f);
    for (int i = 3; i >= 1; --i) {
        const float grow = i * (4.f + breath * 2.f + g_hover * 3.f);
        const float a = (0.09f + 0.04f * breath + 0.09f * g_hover) / (float)i;
        dl->AddRectFilled(pos - ImVec2(grow, grow), pos + size + ImVec2(grow, grow),
                          Accent(a), 14.f + grow);
    }

    const ImU32 bodyCol = hovered ? IM_COL32(30, 32, 36, 255) : IM_COL32(23, 24, 27, 240);
    dl->AddRectFilled(pos, pos + size, bodyCol, 14.f);
    dl->AddRectFilled(pos + ImVec2(2, 2), ImVec2(pos.x + size.x - 2, pos.y + size.y * 0.45f),
                      IM_COL32(255, 255, 255, 12), 12.f, ImDrawFlags_RoundCornersTop);

    const ImU32 borderCol = enabled ? Accent(0.6f + 0.35f * g_hover + 0.15f * breath) : IM_COL32(72, 77, 84, 200);
    dl->AddRect(pos, pos + size, borderCol, 14.f, 0, 1.8f);

    if (g_rippleT >= 0.f && g_rippleT < 0.5f) {
        const float k = g_rippleT / 0.5f;
        dl->AddCircle(g_ripplePos, 10.f + k * 160.f, Accent(0.5f * (1.f - k)), 0, 3.f * (1.f - k) + 0.5f);
    }

    const char* text;
    switch (state.state) {
        case State::Finding:   text = "Scanning...";          break;
        case State::Injecting: text = "Injecting...";         break;
        case State::Success:   text = "Injected \xE2\x9C\x93"; break;
        case State::Error:     text = "Retry Injection";      break;
        default:               text = "Inject";               break;
    }
    const ImVec2 ts = ImGui::CalcTextSize(text);

    if (state.state == State::Finding || state.state == State::Injecting) {
        const ImVec2 sc{ center.x - ts.x * 0.5f - 20.f, center.y };
        dl->PathArcTo(sc, 7.5f, g_spin, g_spin + 4.5f, 24);
        dl->PathStroke(Accent(0.95f), 0, 2.4f);
    }

    dl->AddText(g_fontBig, 17.5f, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f - 1.f),
                enabled ? kTextPrimary : kTextSecondary, text);

    for (auto& p : g_parts) {
        p.life -= dt;
        p.p.x += p.v.x * dt; p.p.y += p.v.y * dt;
        p.v.y += 160.f * dt;
        const float a = p.life / p.maxLife;
        dl->AddCircleFilled(p.p, p.size, Accent(a));
    }
    g_parts.erase(std::remove_if(g_parts.begin(), g_parts.end(),
                  [](const Particle& p) { return p.life <= 0.f; }), g_parts.end());

    return clicked;
}

// ---------------------------------------------------------------------------
// Secondary Action Button Component
// ---------------------------------------------------------------------------
static bool DrawSecondaryButton(const char* label, const ImVec2& size, bool disabled = false) {
    ImGuiIO& io = ImGui::GetIO();
    const float dt = io.DeltaTime;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton(label, size, ImGuiButtonFlags_None);
    const bool enabled = !disabled;
    const bool hovered = enabled && ImGui::IsItemHovered();
    const bool clicked = enabled && ImGui::IsItemClicked();

    struct AnimState { float hover = 0.f; };
    static std::unordered_map<std::string, AnimState> s_anims;
    AnimState& anim = s_anims[label];
    anim.hover = ease(anim.hover, hovered ? 1.f : 0.f, 14.f, dt);

    const ImU32 bgCol = disabled ? IM_COL32(17, 18, 20, 200) : (hovered ? IM_COL32(36, 38, 42, 255) : IM_COL32(25, 26, 29, 240));
    const ImU32 borderCol = disabled ? IM_COL32(52, 56, 62, 150) : alpha(kAccentPlatinum, 0.4f + 0.5f * anim.hover);

    dl->AddRectFilled(pos, pos + size, bgCol, 8.f);
    dl->AddRect(pos, pos + size, borderCol, 8.f, 0, 1.2f);

    const ImVec2 ts = ImGui::CalcTextSize(label);
    const ImVec2 center{ pos.x + size.x * 0.5f, pos.y + size.y * 0.5f };
    const ImU32 textCol = disabled ? IM_COL32(104, 108, 114, 255) : (hovered ? IM_COL32(255, 255, 255, 255) : kTextPrimary);
    dl->AddText(g_fontReg, 13.5f, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f - 1.f), textCol, label);

    return clicked;
}

// ---------------------------------------------------------------------------
// Worker threads
// ---------------------------------------------------------------------------
struct WorkItem { std::wstring dll; };
static WorkItem g_work;
static DWORD    g_workPid = 0;
static HANDLE   g_thread = nullptr;

static DWORD WINAPI Worker(LPVOID) {
    std::string err;
    DWORD pid = 0;
    publishState(State::Finding, "Scanning for Minecraft process...", {}, 0);
    if (!findMinecraft(pid, err)) {
        publishState(State::Error, err, err, 0);
        return 1;
    }
    publishState(State::Finding, "Minecraft found; verifying instance state...", {}, pid);
    clearAutoSkipFor(pid);   // an explicit Inject always wins over the uninject skip list

    if (agentAlreadyLoaded(pid)) {
        if (agentMutexExists(pid)) publishState(State::Success, "Passive is already running in Minecraft", {}, pid);
        else {
            err = "Passive's DLL is loaded, but overlay startup pending. Restart Minecraft.";
            publishState(State::Error, err, err, pid);
        }
        return 0;
    }

    publishState(State::Injecting, "Loading Passive into Minecraft...", {}, pid);
    bool injected = false;
    {
        std::lock_guard<std::mutex> lock(g_injectMutex);   // never race the auto thread
        injected = inject(pid, g_work.dll, err);
    }
    if (!injected) {
        publishState(State::Error, err, err, pid);
        return 1;
    }

    bool mapped = false;
    for (int i = 0; i < 20; ++i) {
        if (processHasModule(pid, L"passive_agent.dll")) { mapped = true; break; }
        Sleep(100);
    }
    if (!mapped) {
        err = "DLL mapped failed or uninjected. Inspect passive_agent.log.";
        publishState(State::Error, err, err, pid);
        return 1;
    }

    bool ok = false;
    for (int i = 0; i < 100; ++i) {
        if (agentMutexExists(pid)) { ok = true; break; }
        Sleep(100);
    }
    if (ok) {
        publishState(State::Success, "Injected successfully — press RSHIFT in Minecraft", {}, pid);
        maybeCloseAfterInject();
    } else {
        err = "DLL loaded, but overlay handshake timed out. Inspect passive_agent.log.";
        publishState(State::Error, err, err, pid);
    }
    return 0;
}

static DWORD WINAPI WorkerPid(LPVOID) {
    std::string err;
    const DWORD pid = g_workPid;
    if (!pid) { publishState(State::Error, "Select an instance first", "Select an instance first", 0); return 1; }
    publishState(State::Finding, "Checking selected instance...", {}, pid);
    if (agentAlreadyLoaded(pid)) {
        if (agentMutexExists(pid)) publishState(State::Success, "Passive is already running in this instance", {}, pid);
        else {
            err = "Passive's DLL is loaded, but overlay failed. Restart the instance.";
            publishState(State::Error, err, err, pid);
        }
        return 0;
    }
    publishState(State::Injecting, "Loading Passive into selected instance...", {}, pid);
    bool injected = false;
    {
        std::lock_guard<std::mutex> lock(g_injectMutex);   // never race the auto thread
        injected = inject(pid, g_work.dll, err);
    }
    if (!injected) { publishState(State::Error, err, err, pid); return 1; }

    bool mapped = false;
    for (int i = 0; i < 20; ++i) {
        if (processHasModule(pid, L"passive_agent.dll")) { mapped = true; break; }
        Sleep(100);
    }
    if (!mapped) {
        err = "DLL mapped failed. Inspect passive_agent.log.";
        publishState(State::Error, err, err, pid);
        return 1;
    }
    bool ok = false;
    for (int i = 0; i < 100; ++i) {
        if (agentMutexExists(pid)) { ok = true; break; }
        Sleep(100);
    }
    if (ok) {
        publishState(State::Success, "Injected successfully — press RSHIFT in Minecraft", {}, pid);
        maybeCloseAfterInject();
    } else {
        err = "DLL loaded, but overlay failed. Inspect passive_agent.log.";
        publishState(State::Error, err, err, pid);
    }
    return 0;
}

static void StartWorkForPid(DWORD pid) {
    if (g_thread) {
        const DWORD result = WaitForSingleObject(g_thread, 0);
        if (result == WAIT_TIMEOUT) return;
        CloseHandle(g_thread); g_thread = nullptr;
    }
    clearAutoSkipFor(pid);   // injecting by hand after an uninject is allowed again
    g_workPid = pid;
    g_successT = 0.f;
    publishState(State::Finding, "Starting injection...", {}, pid);
    g_thread = CreateThread(nullptr, 0, &WorkerPid, nullptr, 0, nullptr);
    if (!g_thread) publishState(State::Error, "Could not start injector thread", "CreateThread failed", pid);
}

static void StartWork() {
    if (g_thread) {
        const DWORD result = WaitForSingleObject(g_thread, 0);
        if (result == WAIT_TIMEOUT) return;
        CloseHandle(g_thread); g_thread = nullptr;
    }
    g_successT = 0.f;
    publishState(State::Finding, "Starting injection...", {}, 0);
    g_thread = CreateThread(nullptr, 0, &Worker, nullptr, 0, nullptr);
    if (!g_thread) publishState(State::Error, "Could not start injector thread", "CreateThread failed", 0);
}

// ---------------------------------------------------------------------------
// CLI Mode
// ---------------------------------------------------------------------------
static int runCli(int argc, wchar_t** argv) {
    DWORD pid = 0;
    for (int i = 1; i + 1 < argc; ++i) {
        if (_wcsicmp(argv[i], L"--pid") == 0) pid = (DWORD)_wtoi(argv[i + 1]);
    }
    if (!pid) { fprintf(stderr, "usage: --pid <process id>\n"); return 2; }

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dll(exePath);
    size_t slash = dll.find_last_of(L"\\/");
    dll = dll.substr(0, slash + 1) + L"passive_agent.dll";
    if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr, L"agent dll not found: %ls\n", dll.c_str()); return 2;
    }

    std::string err;
    if (!inject(pid, dll, err)) {
        fprintf(stderr, "INJECT-FAIL: %s\n", err.c_str()); return 1;
    }
    bool mapped = false;
    for (int i = 0; i < 20; ++i) {
        if (processHasModule(pid, L"passive_agent.dll")) { mapped = true; break; }
        Sleep(100);
    }
    if (!mapped) { fprintf(stderr, "INJECT-FAIL: dll absent from target\n"); return 1; }
    bool ok = false;
    for (int i = 0; i < 100; ++i) {
        if (agentMutexExists(pid)) { ok = true; break; }
        Sleep(100);
    }
    printf("INJECT-%s dll mapped%s\n", ok ? "OK" : "FAIL",
           ok ? "; overlay handshake ok" : "; overlay did not initialize");
    return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Win32 & DirectX 11 Boilerplate
// ---------------------------------------------------------------------------
static HWND g_hwnd = nullptr;
static ID3D11Device* g_dev = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static IDXGISwapChain* g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static char g_stage[128] = "starting";
static LONG WINAPI CrashLogger(EXCEPTION_POINTERS* ep) {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (char* s = strrchr(path, '\\')) *(s + 1) = 0;
    strcat_s(path, "passive_injector_crash.txt");
    char buf[256];
    snprintf(buf, sizeof(buf), "code=0x%08lX addr=%p stage=%s\n",
             (unsigned long)ep->ExceptionRecord->ExceptionCode,
             ep->ExceptionRecord->ExceptionAddress, g_stage);
    HANDLE f = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) { DWORD wr = 0; WriteFile(f, buf, (DWORD)strlen(buf), &wr, nullptr); CloseHandle(f); }
    return EXCEPTION_CONTINUE_SEARCH;
}

static bool g_backendReady = false;

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (g_backendReady && ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return TRUE;
    switch (m) {
        case WM_NCHITTEST: {
            POINT pt{ LOWORD(l), HIWORD(l) };
            ScreenToClient(h, &pt);
            // Allow native OS window dragging on title bar (y: 0..38, x < 400)
            // The title bar is draggable, but not under the gear / minimize / close buttons.
            if (pt.y >= 0 && pt.y <= 38 && pt.x < 340) {
                return HTCAPTION;
            }
            break;
        }
        case WM_SIZE: {
            if (g_dev && g_swap && w != 0 && l != 0) {
                if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
                g_swap->ResizeBuffers(0, LOWORD(l) ? LOWORD(l) : 1,
                                      HIWORD(l) ? HIWORD(l) : 1,
                                      DXGI_FORMAT_UNKNOWN, 0);
                ID3D11Texture2D* bb = nullptr;
                if (SUCCEEDED(g_swap->GetBuffer(0, IID_PPV_ARGS(&bb)))) {
                    g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv);
                    bb->Release();
                }
            }
            return 0;
        }
        case WM_CLOSE: PostQuitMessage(0); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static bool CreateD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sc{};
    sc.BufferCount = 2;
    sc.BufferDesc.Width = 0; sc.BufferDesc.Height = 0;
    sc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sc.BufferDesc.RefreshRate.Numerator = 60; sc.BufferDesc.RefreshRate.Denominator = 1;
    sc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.OutputWindow = hwnd;
    sc.SampleDesc.Count = 1;
    sc.Windowed = TRUE;
    sc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL flOut;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               fls, 2, D3D11_SDK_VERSION, &sc,
                                               &g_swap, &g_dev, &flOut, &g_ctx);
    return SUCCEEDED(hr);
}

static void CreateRT() {
    ID3D11Texture2D* bb = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&bb));
    g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv);
    bb->Release();
}

// ---------------------------------------------------------------------------
// Settings side effects & the auto-injection engine
// ---------------------------------------------------------------------------
static void maybeCloseAfterInject() {
    if (!injectorSettings().closeAfterInject || !g_hwnd || !IsWindow(g_hwnd)) return;
    PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
}

static void applyAlwaysOnTop() {
    if (!g_hwnd || !IsWindow(g_hwnd)) return;
    SetWindowPos(g_hwnd, injectorSettings().alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static HANDLE g_autoThread = nullptr;
static volatile LONG g_autoStop = 0;

// One pass of auto-injection. Runs on its own thread, so it never blocks the UI
// and never races a manual injection (both take g_injectMutex).
static void autoInjectPass() {
    InjectorSettings& s = injectorSettings();
    if (!s.autoInject) return;
    if (g_thread) {
        const DWORD running = WaitForSingleObject(g_thread, 0);
        if (running == WAIT_TIMEOUT) return;      // a manual injection is in flight
    }
    const std::vector<InstanceInfo> instances = enumerateInstances();

    // Drop instances that closed, and treat an injection that is no longer loaded
    // as a manual uninject: that PID is skipped from now on.
    {
        std::lock_guard<std::mutex> lock(g_autoSetsMutex);
        for (auto it = g_passiveSeen.begin(); it != g_passiveSeen.end();) {
            const DWORD pid = *it;
            const InstanceInfo* live = nullptr;
            for (const InstanceInfo& info : instances)
                if (info.pid == pid) { live = &info; break; }
            if (!live) { it = g_passiveSeen.erase(it); g_autoSkipped.erase(pid); continue; }
            if (!live->injected) {
                if (s.autoInjectSkipUninjected) g_autoSkipped.insert(pid);
                it = g_passiveSeen.erase(it);
                continue;
            }
            ++it;
        }
        // Remember every instance Passive runs in, whoever injected it, so a later
        // uninject is recognised even when the user injected by hand.
        for (const InstanceInfo& info : instances)
            if (info.injected && agentMutexExists(info.pid)) g_passiveSeen.insert(info.pid);
    }

    for (const InstanceInfo& info : instances) {
        if (g_autoStop) return;
        if (info.injected) continue;
        if (s.autoInjectWindowedOnly && !info.windowed) continue;
        {
            std::lock_guard<std::mutex> lock(g_autoSetsMutex);
            if (g_autoSkipped.count(info.pid)) continue;
        }

        std::string err;
        bool injected = false;
        {
            std::lock_guard<std::mutex> lock(g_injectMutex);
            injected = inject(info.pid, g_work.dll, err);
        }
        if (!injected) continue;

        bool ready = false;
        for (int i = 0; i < 60 && !g_autoStop; ++i) {
            if (agentMutexExists(info.pid)) { ready = true; break; }
            Sleep(100);
        }
        if (!ready) continue;
        {
            std::lock_guard<std::mutex> lock(g_autoSetsMutex);
            g_passiveSeen.insert(info.pid);
        }
        publishState(State::Success, "Auto-injected \xE2\x80\x94 press RSHIFT in Minecraft", {}, info.pid);
        maybeCloseAfterInject();
    }
}

static DWORD WINAPI AutoWorker(LPVOID) {
    while (!g_autoStop) {
        const int delay = std::clamp(injectorSettings().autoInjectDelayMs, 250, 10000);
        for (int waited = 0; waited < delay && !g_autoStop; waited += 100) Sleep(100);
        if (g_autoStop) break;
        autoInjectPass();
    }
    return 0;
}

// Starts the auto thread when the setting is on and stops it when it is off.
static void syncAutoThread() {
    const bool want = injectorSettings().autoInject;
    if (want && !g_autoThread) {
        InterlockedExchange(&g_autoStop, 0);
        g_autoThread = CreateThread(nullptr, 0, &AutoWorker, nullptr, 0, nullptr);
    } else if (!want && g_autoThread) {
        InterlockedExchange(&g_autoStop, 1);
        WaitForSingleObject(g_autoThread, 4000);
        CloseHandle(g_autoThread);
        g_autoThread = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Settings view (the gear in the title bar)
// ---------------------------------------------------------------------------
static bool   g_settingsDirty = false;
static double g_settingsSavedAt = 0.0;

static void drawSettingsView(const ImVec2& ds, ImDrawList* dl, bool& back) {
    InjectorSettings& s = injectorSettings();
    const ImVec2 cardPos(20.f, 92.f);
    const ImVec2 cardSize(ds.x - 40.f, ds.y - 44.f - 92.f);
    dl->AddRectFilled(cardPos, cardPos + cardSize, kCardBg, 12.f);
    dl->AddRect(cardPos, cardPos + cardSize, kCardBorder, 12.f, 0, 1.f);

    ImGui::SetCursorPos(ImVec2(cardPos.x + 16.f, cardPos.y + 12.f));
    if (ImGui::Button("< Back", ImVec2(74.f, 26.f))) back = true;
    ImGui::SameLine();
    ImGui::SetCursorPosY(cardPos.y + 17.f);
    ImGui::TextColored(ImVec4(0.78f, 0.82f, 0.88f, 1.f), "Settings");

    ImGui::SetCursorPos(ImVec2(cardPos.x + 14.f, cardPos.y + 48.f));
    ImGui::BeginChild("##settings-scroll", ImVec2(cardSize.x - 28.f, cardSize.y - 60.f), false,
                      ImGuiWindowFlags_AlwaysUseWindowPadding);

    const ImVec4 dim(0.56f, 0.58f, 0.62f, 1.f);
    const ImVec4 head(0.80f, 0.84f, 0.90f, 1.f);
    bool changed = false;

    ImGui::TextColored(head, "AUTO INJECTION");
    changed |= ImGui::Checkbox("Auto Injection (every Minecraft instance)", &s.autoInject);
    if (s.autoInject) {
        ImGui::SetNextItemWidth(250.f);
        changed |= ImGui::SliderInt("Inject delay (ms)", &s.autoInjectDelayMs, 250, 10000);
        changed |= ImGui::Checkbox("Only windowed Minecraft instances", &s.autoInjectWindowedOnly);
        changed |= ImGui::Checkbox("Don't re-inject after I uninject", &s.autoInjectSkipUninjected);
        ImGui::TextColored(dim, "%d skipped, %d known injected", injectorAutoSkipCount(), injectorAutoSeenCount());
        if (ImGui::Button("Clear uninject skip list")) injectorClearAutoSkips();
    }
    ImGui::Separator();
    ImGui::TextColored(head, "APPLICATION");
    changed |= ImGui::Checkbox("Close injector after a successful injection", &s.closeAfterInject);
    changed |= ImGui::Checkbox("Always on top", &s.alwaysOnTop);
    changed |= ImGui::Checkbox("Remember last tab", &s.rememberTab);
    changed |= ImGui::Checkbox("Ask before injecting", &s.confirmBeforeInject);
    ImGui::Separator();
    ImGui::TextColored(head, "MULTI-INSTANCE LIST");
    changed |= ImGui::Checkbox("Auto refresh instance list", &s.autoRefreshList);
    if (s.autoRefreshList) {
        ImGui::SetNextItemWidth(250.f);
        changed |= ImGui::SliderInt("Refresh interval (ms)", &s.refreshIntervalMs, 500, 10000);
    }
    changed |= ImGui::Checkbox("Show full window titles", &s.showFullTitles);
    ImGui::Separator();
    ImGui::TextColored(dim, "Settings are saved automatically to passive_injector.ini.");
    ImGui::EndChild();

    if (changed) {
        // Sliders fire a change every frame they are dragged, so the file write is throttled in the
        // main loop instead of being repeated per frame.
        g_settingsDirty = true;
        applyAlwaysOnTop();
    }
}

static const wchar_t* kClass = L"PassiveInjector";
static const wchar_t* kTitle = L"Passive Client Injector";

int APIENTRY wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    AddVectoredExceptionHandler(0, &CrashLogger);

    {
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            for (int i = 1; i < argc; ++i) {
                if (_wcsicmp(argv[i], L"--pid") == 0) {
                    int rc = runCli(argc, argv);
                    LocalFree(argv);
                    return rc;
                }
            }
            LocalFree(argv);
        }
    }

    snprintf(g_stage, sizeof(g_stage), "imgui-create-context");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    snprintf(g_stage, sizeof(g_stage), "register-class");
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);

    snprintf(g_stage, sizeof(g_stage), "create-window");
    const int W = 480, H = 480;
    RECT r{ 0, 0, W, H };
    AdjustWindowRectExForDpi(&r, WS_POPUP, FALSE, WS_EX_APPWINDOW, USER_DEFAULT_SCREEN_DPI);
    g_hwnd = CreateWindowExW(WS_EX_APPWINDOW, kClass, kTitle, WS_POPUP,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             r.right - r.left, r.bottom - r.top,
                             nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) {
        MessageBoxW(nullptr, L"Could not create the injector window.", L"Passive", MB_ICONERROR);
        return 1;
    }

    // Hardware-accelerated rounded window borders and DWM dark theme
    HRGN hRgn = CreateRoundRectRgn(0, 0, W + 1, H + 1, 16, 16);
    SetWindowRgn(g_hwnd, hRgn, TRUE);

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(g_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    DWORD cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(g_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    snprintf(g_stage, sizeof(g_stage), "d3d11-init");
    if (!CreateD3D(g_hwnd)) {
        MessageBoxW(nullptr, L"Direct3D 11 initialization failed.\nUpdate your GPU drivers and try again.", L"Passive", MB_ICONERROR);
        return 1;
    }
    snprintf(g_stage, sizeof(g_stage), "render-target");
    CreateRT();
    ShowWindow(g_hwnd, SW_SHOW);

    snprintf(g_stage, sizeof(g_stage), "fonts");
    const std::wstring fontDir = executableDirectory() + L"assets\\fonts\\";
    g_fontBig = loadFontWide(fontDir + L"Inter-SemiBold.ttf", 18.f);
    g_fontReg = loadFontWide(fontDir + L"Inter-Regular.ttf", 13.5f);
    if (!g_fontBig) g_fontBig = loadFontWide(L"C:\\Windows\\Fonts\\segoeuib.ttf", 18.f);
    if (!g_fontReg) g_fontReg = loadFontWide(L"C:\\Windows\\Fonts\\segoeui.ttf", 13.5f);
    if (!g_fontBig) g_fontBig = io.Fonts->AddFontDefault();
    if (!g_fontReg) g_fontReg = io.Fonts->AddFontDefault();

    snprintf(g_stage, sizeof(g_stage), "imgui-backend-init");
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);
    g_backendReady = true;

    snprintf(g_stage, sizeof(g_stage), "main-loop");

    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 14.f; st.FrameRounding = 8.f; st.PopupRounding = 10.f;
    st.GrabRounding = 6.f; st.ScrollbarRounding = 4.f; st.TabRounding = 8.f;
    st.WindowPadding = ImVec2(20, 20); st.FramePadding = ImVec2(10, 8);
    st.ItemSpacing = ImVec2(10, 8); st.ScrollbarSize = 6.f;

    st.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.055f, 0.058f, 0.066f, 0.5f);
    st.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.32f, 0.34f, 0.38f, 0.6f);
    st.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.55f, 0.58f, 0.63f, 0.8f);
    st.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.72f, 0.75f, 0.80f, 1.0f);

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dll = exePath;
    const size_t slash = dll.find_last_of(L"\\/");
    dll = dll.substr(0, slash + 1) + L"passive_agent.dll";
    g_work.dll = dll;

    // Settings live next to the exe so a portable copy keeps its own preferences.
    const std::wstring iniPath = dll.substr(0, dll.find_last_of(L"\\/") + 1) + L"passive_injector.ini";
    loadInjectorSettings(iniPath);
    applyAlwaysOnTop();

    publishState(GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES ? State::Error : State::Idle,
                 GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES
                    ? "passive_agent.dll not found next to injector"
                    : "waiting for Minecraft",
                 GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES
                    ? "passive_agent.dll missing" : "", 0);

    static int s_tab = injectorSettings().rememberTab ? injectorSettings().lastTab : 0;
    static float s_animTabX = 24.f;
    static float s_animTabW = 208.f;

    static float s_hovCloseAnim = 0.f;
    static float s_hovMinAnim = 0.f;
    static float s_hovGearAnim = 0.f;
    static bool  s_settingsView = false;
    static bool  s_confirmPending = false;
    static DWORD s_pendingInjectPid = 0;

    static std::vector<InstanceInfo> s_instances;
    static int s_selected = -1;
    static double s_nextScan = 0.0;
    static bool s_scannedOnce = false;

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        snprintf(g_stage, sizeof(g_stage), "frame-new");
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.043f, 0.045f, 0.051f, 1.f));
        ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoBringToFrontOnFocus |
                                          ImGuiWindowFlags_NoSavedSettings);

        snprintf(g_stage, sizeof(g_stage), "bg-draw");
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 ds = io.DisplaySize;
        const float dt = io.DeltaTime;

        // Background subtle radial platinum glow
        {
            const ImVec2 c{ ds.x * 0.5f, ds.y * 0.22f };
            for (int i = 12; i >= 1; --i) {
                const float rr = 30.f + i * 24.f;
                dl->AddCircleFilled(c, rr, IM_COL32(198, 208, 220, 3), 40);
            }
        }

        // Custom Title Bar (Height: 38px)
        snprintf(g_stage, sizeof(g_stage), "titlebar");
        {
            dl->AddRectFilled(ImVec2(0, 0), ImVec2(ds.x, 38.f), kHeaderBg);
            dl->AddLine(ImVec2(0, 38.f), ImVec2(ds.x, 38.f), IM_COL32(36, 38, 43, 255), 1.f);

            // Official Passive mark. The lettered tile is only a fallback for a
            // build whose asset is missing; the logo is the brand surface.
            const ImVec2 logoPos(12.f, 5.f);
            const ImVec2 logoSize(28.f, 28.f);
            const ImTextureID brand = tyr::injector::logoTexture(g_dev);
            if (brand) {
                const float breathe = 0.92f + 0.08f * (0.5f + 0.5f * std::sin(g_pulse * 2.f));
                dl->AddImage(brand, logoPos, logoPos + logoSize,
                             ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), IM_COL32(255, 255, 255,
                             (int)(246.f * breathe)));
            } else {
                dl->AddRectFilled(logoPos, logoPos + logoSize, IM_COL32(198, 208, 220, 240), 6.f);
                if (g_fontBig) ImGui::PushFont(g_fontBig);
                dl->AddText(ImVec2(logoPos.x + 9.f, logoPos.y + 3.f), IM_COL32(16, 18, 22, 255), "P");
                if (g_fontBig) ImGui::PopFont();
            }

            // App Title & Version
            dl->AddText(g_fontBig, 16.f, ImVec2(46.f, 9.f), kTextPrimary, "PASSIVE");
            dl->AddText(g_fontReg, 13.f, ImVec2(116.f, 11.f), kTextSecondary, (std::string("v") + g_ver).c_str());

            // Window Controls (Minimize & Close)
            const ImVec2 closePos(ds.x - 34.f, 6.f);
            const ImVec2 minPos(ds.x - 64.f, 6.f);
            const ImVec2 btnSize(26.f, 26.f);

            const bool hovClose = ImGui::IsMouseHoveringRect(closePos, closePos + btnSize);
            const bool hovMin = ImGui::IsMouseHoveringRect(minPos, minPos + btnSize);

            s_hovCloseAnim = ease(s_hovCloseAnim, hovClose ? 1.f : 0.f, 16.f, dt);
            s_hovMinAnim = ease(s_hovMinAnim, hovMin ? 1.f : 0.f, 16.f, dt);

            // Minimize Button
            if (s_hovMinAnim > 0.01f) {
                dl->AddRectFilled(minPos, minPos + btnSize, IM_COL32(34, 36, 41, (int)(220 * s_hovMinAnim)), 6.f);
            }
            dl->AddLine(ImVec2(minPos.x + 8.f, minPos.y + 14.f), ImVec2(minPos.x + 18.f, minPos.y + 14.f),
                        alpha(kTextSecondary, 0.7f + 0.3f * s_hovMinAnim), 1.8f);

            if (hovMin && ImGui::IsMouseClicked(0)) {
                ShowWindow(g_hwnd, SW_MINIMIZE);
            }

            // Settings gear, left of minimize. Drawn from primitives so it needs no icon
            // font; it turns platinum while the settings view is open.
            const ImVec2 gearPos(ds.x - 94.f, 6.f);
            const bool hovGear = ImGui::IsMouseHoveringRect(gearPos, gearPos + btnSize);
            s_hovGearAnim = ease(s_hovGearAnim, hovGear ? 1.f : 0.f, 16.f, dt);
            if (s_hovGearAnim > 0.01f) {
                dl->AddRectFilled(gearPos, gearPos + btnSize, IM_COL32(34, 36, 41, (int)(220 * s_hovGearAnim)), 6.f);
            }
            {
                const ImVec2 gc(gearPos.x + btnSize.x * 0.5f, gearPos.y + btnSize.y * 0.5f);
                const ImU32 gearCol = s_settingsView ? kAccentPlatinum
                                                     : alpha(kTextSecondary, 0.7f + 0.3f * s_hovGearAnim);
                dl->AddCircle(gc, 6.5f, gearCol, 16, 1.8f);
                dl->AddCircle(gc, 2.4f, gearCol, 12, 1.6f);
                for (int i = 0; i < 8; ++i) {
                    const float a = (float)i * 0.7853981634f;
                    dl->AddLine(ImVec2(gc.x + std::cos(a) * 6.5f, gc.y + std::sin(a) * 6.5f),
                                ImVec2(gc.x + std::cos(a) * 9.5f, gc.y + std::sin(a) * 9.5f), gearCol, 1.6f);
                }
            }
            if (hovGear && ImGui::IsMouseClicked(0)) s_settingsView = !s_settingsView;

            // Close Button
            if (s_hovCloseAnim > 0.01f) {
                dl->AddRectFilled(closePos, closePos + btnSize, IM_COL32(235, 60, 60, (int)(220 * s_hovCloseAnim)), 6.f);
            }
            const ImVec2 xSize = ImGui::CalcTextSize("\xC3\x97");
            dl->AddText(closePos + ImVec2((btnSize.x - xSize.x) * 0.5f, (btnSize.y - xSize.y) * 0.5f - 1.f),
                        s_hovCloseAnim > 0.01f ? IM_COL32(255, 255, 255, 255) : kTextSecondary, "\xC3\x97");

            if (hovClose && ImGui::IsMouseClicked(0)) done = true;
        }

        // Navigation Tabs (y: 46px to 80px)
        snprintf(g_stage, sizeof(g_stage), "tabs");
        {
            const float tabY = 46.f;
            const float tabH = 34.f;
            const float tabW = (ds.x - 48.f) * 0.5f;

            dl->AddRectFilled(ImVec2(20.f, tabY), ImVec2(ds.x - 20.f, tabY + tabH), IM_COL32(16, 17, 20, 255), 8.f);
            dl->AddRect(ImVec2(20.f, tabY), ImVec2(ds.x - 20.f, tabY + tabH), IM_COL32(34, 36, 40, 255), 8.f);

            const float targetTabX = 24.f + s_tab * (tabW + 4.f);
            const float targetTabW = tabW - 4.f;
            s_animTabX = ease(s_animTabX, targetTabX, 16.f, dt);
            s_animTabW = ease(s_animTabW, targetTabW, 16.f, dt);

            // Active Tab Animated Background & Highlight Bar
            const ImVec2 atPos(s_animTabX, tabY + 2.f);
            const ImVec2 atSize(s_animTabW, tabH - 4.f);
            dl->AddRectFilled(atPos, atPos + atSize, IM_COL32(34, 36, 41, 255), 6.f);
            dl->AddRect(atPos, atPos + atSize, IM_COL32(150, 162, 178, 180), 6.f);
            dl->AddRectFilled(ImVec2(atPos.x + 12.f, atPos.y + atSize.y - 3.f),
                              ImVec2(atPos.x + atSize.x - 12.f, atPos.y + atSize.y - 1.f),
                              kAccentPlatinum, 2.f);

            const char* tabNames[] = { "Inject", "Multi Instance" };
            for (int i = 0; i < 2; ++i) {
                const ImVec2 tPos(24.f + i * (tabW + 4.f), tabY + 2.f);
                const ImVec2 tSize(tabW - 4.f, tabH - 4.f);
                const bool active = (s_tab == i);
                const bool hov = ImGui::IsMouseHoveringRect(tPos, tPos + tSize);

                if (hov && ImGui::IsMouseClicked(0)) {
                    s_settingsView = false;
                    s_tab = i;
                    if (injectorSettings().rememberTab) {
                        injectorSettings().lastTab = s_tab;
                        saveInjectorSettings();
                    }
                    if (s_tab == 1 && !s_scannedOnce) {
                        s_instances = enumerateInstances();
                        s_scannedOnce = true;
                        s_nextScan = ImGui::GetTime() + 2.0;
                    }
                }

                if (!active && hov) {
                    dl->AddRectFilled(tPos, tPos + tSize, IM_COL32(24, 26, 30, 180), 6.f);
                }

                const ImVec2 ts = ImGui::CalcTextSize(tabNames[i]);
                const ImU32 col = active ? kTextPrimary : (hov ? IM_COL32(218, 222, 230, 255) : kTextSecondary);
                dl->AddText(tPos + ImVec2((tSize.x - ts.x) * 0.5f, (tSize.y - ts.y) * 0.5f), col, tabNames[i]);
            }
        }

        if (s_settingsView) {
            snprintf(g_stage, sizeof(g_stage), "settings-view");
            drawSettingsView(ds, dl, s_settingsView);
        } else if (s_tab == 0) {
            snprintf(g_stage, sizeof(g_stage), "main-inject-tab");
            const StateView state = stateView();

            // Auto-detect Minecraft process if idle
            if (state.state == State::Idle) {
                DWORD currentPid = 0;
                std::string tmpErr;
                if (findMinecraft(currentPid, tmpErr)) {
                    if (g_pid != currentPid) g_pid = currentPid;
                } else {
                    if (g_pid != 0) g_pid = 0;
                }
            }

            // Main Status Area Card
            const ImVec2 cardPos(20.f, 92.f);
            const ImVec2 cardSize(ds.x - 40.f, 74.f);
            dl->AddRectFilled(cardPos, cardPos + cardSize, kCardBg, 12.f);
            dl->AddRect(cardPos, cardPos + cardSize, kCardBorder, 12.f, 0, 1.f);

            ImU32 dotColor = kWarnYellow;
            std::string statusTitle = "Ready when you are";
            std::string statusSubtext = "Launch Minecraft 1.21.11, then click Inject.";

            switch (state.state) {
                case State::Idle:
                    if (state.pid > 0 && agentMutexExists(state.pid)) {
                        // Passive is already in this JVM: the platinum pulse says so.
                        dotColor = kAccentPlatinum;
                        statusTitle = "Passive is injected";
                        char buf[128];
                        snprintf(buf, sizeof(buf), "Running in javaw.exe (PID %lu) — press RSHIFT in Minecraft", (unsigned long)state.pid);
                        statusSubtext = buf;
                    } else if (state.pid > 0) {
                        dotColor = kSuccessGreen;
                        statusTitle = "Minecraft Detected";
                        char buf[128];
                        snprintf(buf, sizeof(buf), "javaw.exe running (PID %lu) — ready to inject", (unsigned long)state.pid);
                        statusSubtext = buf;
                    } else {
                        dotColor = kWarnYellow;
                        statusTitle = "Ready when you are";
                        statusSubtext = "Launch Minecraft 1.21.11, then click Inject.";
                    }
                    break;
                case State::Finding:
                    dotColor = kAccentBlue;
                    statusTitle = "Scanning Processes";
                    statusSubtext = state.status.empty() ? "Searching for target javaw.exe..." : state.status;
                    break;
                case State::Injecting:
                    dotColor = kAccentPlatinum;
                    statusTitle = "Welcome To Passive";
                    statusSubtext = state.status.empty() ? "Mapping passive_agent.dll into target JVM..." : state.status;
                    break;
                case State::Success:
                    dotColor = kSuccessGreen;
                    statusTitle = "Injection Successful";
                    statusSubtext = state.status.empty() ? "Press RSHIFT in Minecraft to open menu" : state.status;
                    break;
                case State::Error:
                    dotColor = kErrorRed;
                    statusTitle = "Injection Failed";
                    statusSubtext = state.error.empty() ? state.status : state.error;
                    break;
            }

            // Status Dot with smooth pulse animation
            static float s_dotTimer = 0.f;
            s_dotTimer += dt * 3.f;
            const float dotPulse = 0.25f + 0.15f * std::sin(s_dotTimer);
            const float dotRadius = 8.f + 1.5f * std::sin(s_dotTimer);

            const ImVec2 dotCenter(cardPos.x + 24.f, cardPos.y + 37.f);
            dl->AddCircleFilled(dotCenter, dotRadius, alpha(dotColor, dotPulse));
            dl->AddCircleFilled(dotCenter, 4.5f, dotColor);

            // Title & Subtext
            dl->AddText(g_fontBig, 16.f, ImVec2(cardPos.x + 42.f, cardPos.y + 16.f), kTextPrimary, statusTitle.c_str());
            dl->AddText(g_fontReg, 13.f, ImVec2(cardPos.x + 42.f, cardPos.y + 40.f),
                        state.state == State::Error ? kErrorRed : kTextSecondary,
                        statusSubtext.c_str(), nullptr, cardSize.x - 52.f);

            // Main Inject Button
            snprintf(g_stage, sizeof(g_stage), "inject-button");
            ImGui::SetCursorPos(ImVec2((ds.x - 290.f) * 0.5f, 184.f));
            if (DrawInjectButton("Inject")) {
                if (injectorSettings().confirmBeforeInject) { s_pendingInjectPid = 0; s_confirmPending = true; }
                else StartWork();
            }

            // Client Info Card
            snprintf(g_stage, sizeof(g_stage), "info-card");
            const ImVec2 infoPos(20.f, 260.f);
            const ImVec2 infoSize(ds.x - 40.f, 170.f);
            dl->AddRectFilled(infoPos, infoPos + infoSize, kCardBg, 12.f);
            dl->AddRect(infoPos, infoPos + infoSize, kCardBorder, 12.f, 0, 1.f);

            const char* rowLabels[] = { "Target Engine", "Module DLL", "In-Game Menu Bind", "Injection Method", "Target Version" };
            const char* rowValues[] = { "Minecraft (javaw.exe)", "passive_agent.dll", "RIGHT SHIFT", "JNI Remote Loader", "1.21.11 (Fabric)" };

            for (int row = 0; row < 5; ++row) {
                const float ry = infoPos.y + 12.f + row * 30.f;
                dl->AddText(g_fontReg, 13.f, ImVec2(infoPos.x + 18.f, ry), kTextSecondary, rowLabels[row]);
                const ImVec2 valSize = ImGui::CalcTextSize(rowValues[row]);
                dl->AddText(g_fontReg, 13.f, ImVec2(infoPos.x + infoSize.x - 18.f - valSize.x, ry),
                            row == 2 ? kAccentPlatinum : kTextPrimary, rowValues[row]);
                if (row < 4) {
                    dl->AddLine(ImVec2(infoPos.x + 18.f, ry + 24.f), ImVec2(infoPos.x + infoSize.x - 18.f, ry + 24.f),
                                IM_COL32(36, 38, 43, 255), 1.f);
                }
            }

        } else {
            // Multi-Instance Tab
            snprintf(g_stage, sizeof(g_stage), "multi-instance-tab");
            const double now = ImGui::GetTime();
            const double refreshInterval = injectorSettings().autoRefreshList
                ? (double)injectorSettings().refreshIntervalMs / 1000.0 : 0.0;
            if (refreshInterval > 0.0 && now >= s_nextScan) {
                s_instances = enumerateInstances();
                s_nextScan = now + refreshInterval;
            }

            ImGui::SetCursorPos(ImVec2(22.f, 92.f));
            ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.98f, 1.f), "Multi-Instance Injection Manager");
            ImGui::SetCursorPos(ImVec2(22.f, 112.f));
            ImGui::TextColored(ImVec4(0.56f, 0.58f, 0.62f, 1.f), "Select a specific running Minecraft client process to inject into.");

            ImGui::SetCursorPos(ImVec2(20.f, 134.f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075f, 0.078f, 0.088f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.17f, 0.18f, 0.21f, 1.f));
            ImGui::BeginChild("##instances", ImVec2(ds.x - 40.f, 226.f), true, ImGuiWindowFlags_AlwaysUseWindowPadding);

            if (s_instances.empty()) {
                ImGui::SetCursorPos(ImVec2(20.f, 90.f));
                ImGui::TextColored(ImVec4(0.56f, 0.58f, 0.62f, 1.f), "No running Minecraft / Java instances detected.");
                ImGui::SetCursorPos(ImVec2(20.f, 110.f));
                ImGui::TextColored(ImVec4(0.42f, 0.44f, 0.48f, 1.f), "Start Minecraft 1.21.11 and click Refresh below.");
            } else {
                for (int i = 0; i < (int)s_instances.size(); ++i) {
                    const InstanceInfo& info = s_instances[i];
                    ImGui::PushID(i);

                    const bool selected = (s_selected == i);
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    const float w = ImGui::GetContentRegionAvail().x;
                    const float h = 42.f;

                    if (ImGui::Selectable("##inst", selected, 0, ImVec2(w, h))) {
                        s_selected = i;
                    }

                    ImDrawList* cdl = ImGui::GetWindowDrawList();
                    const bool hovRow = ImGui::IsItemHovered();
                    if (selected) {
                        cdl->AddRectFilled(p, p + ImVec2(w, h), IM_COL32(34, 36, 41, 255), 8.f);
                        cdl->AddRect(p, p + ImVec2(w, h), kAccentPlatinum, 8.f, 0, 1.2f);
                    } else if (hovRow) {
                        cdl->AddRectFilled(p, p + ImVec2(w, h), IM_COL32(26, 28, 32, 255), 8.f);
                        cdl->AddRect(p, p + ImVec2(w, h), IM_COL32(44, 47, 52, 255), 8.f);
                    } else {
                        cdl->AddRectFilled(p, p + ImVec2(w, h), IM_COL32(18, 19, 22, 255), 8.f);
                        cdl->AddRect(p, p + ImVec2(w, h), IM_COL32(36, 38, 43, 255), 8.f);
                    }

                    // PID Badge
                    const ImVec2 badgePos(p.x + 10.f, p.y + 10.f);
                    const ImVec2 badgeSize(70.f, 22.f);
                    cdl->AddRectFilled(badgePos, badgePos + badgeSize, IM_COL32(30, 32, 36, 255), 5.f);
                    cdl->AddRect(badgePos, badgePos + badgeSize, IM_COL32(52, 55, 61, 255), 5.f);
                    char pidBuf[32];
                    snprintf(pidBuf, sizeof(pidBuf), "PID %lu", (unsigned long)info.pid);
                    const ImVec2 pidTextSize = ImGui::CalcTextSize(pidBuf);
                    cdl->AddText(g_fontReg, 12.f, badgePos + ImVec2((badgeSize.x - pidTextSize.x) * 0.5f, 3.f),
                                 kAccentPlatinum, pidBuf);

                    // Window Title
                    std::string titleStr = info.windowed && !info.title.empty() ? wideToUtf8(info.title) : "Minecraft (No Window Title)";
                    if (!injectorSettings().showFullTitles && titleStr.size() > 34)
                        titleStr = titleStr.substr(0, 31) + "...";
                    cdl->AddText(g_fontReg, 13.f, ImVec2(p.x + 90.f, p.y + 12.f), kTextPrimary, titleStr.c_str());

                    // Status Badge
                    const char* statusTag = info.injected ? "[ INJECTED ]" : "[ READY ]";
                    const ImU32 tagColor = info.injected ? kSuccessGreen : kAccentBlue;
                    const ImVec2 tagSize = ImGui::CalcTextSize(statusTag);
                    cdl->AddText(g_fontReg, 12.f, ImVec2(p.x + w - tagSize.x - 12.f, p.y + 13.f), tagColor, statusTag);

                    ImGui::PopID();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);

            // Controls: Custom Styled Refresh & Inject Selected Buttons
            ImGui::SetCursorPos(ImVec2(20.f, 374.f));
            if (DrawSecondaryButton("Refresh", ImVec2(130.f, 38.f))) {
                s_instances = enumerateInstances();
                s_nextScan = now + (refreshInterval > 0.0 ? refreshInterval : 2.0);
                if (s_selected >= (int)s_instances.size()) s_selected = -1;
            }

            const bool canInject = s_selected >= 0 && s_selected < (int)s_instances.size();
            const StateView view = stateView();
            const bool busy = view.state == State::Finding || view.state == State::Injecting;

            ImGui::SameLine();
            ImGui::SetCursorPosX(160.f);
            if (DrawSecondaryButton(busy ? "Injecting..." : "Inject Into Selected", ImVec2(300.f, 38.f), !canInject || busy)) {
                const DWORD pid = s_instances[(size_t)s_selected].pid;
                if (injectorSettings().confirmBeforeInject) { s_pendingInjectPid = pid; s_confirmPending = true; }
                else StartWorkForPid(pid);
            }
        }

        // Bottom Status Footer Bar (Height: 34px)
        snprintf(g_stage, sizeof(g_stage), "footer");
        {
            dl->AddRectFilled(ImVec2(0, ds.y - 34.f), ds, IM_COL32(12, 13, 16, 255));
            dl->AddLine(ImVec2(0, ds.y - 34.f), ImVec2(ds.x, ds.y - 34.f), IM_COL32(26, 28, 32, 255), 1.f);

            const StateView view = stateView();
            std::string footerText;
            ImU32 footerDotCol = kTextSecondary;

            if (view.state == State::Success) {
                footerDotCol = kSuccessGreen;
                footerText = "Status: Injected into Minecraft (PID " + std::to_string(view.pid) + ")";
            } else if (view.state == State::Error) {
                footerDotCol = kErrorRed;
                footerText = "Status: " + (view.error.empty() ? view.status : view.error);
            } else if (view.state == State::Finding || view.state == State::Injecting) {
                footerDotCol = kAccentPlatinum;
                footerText = "Status: " + view.status;
            } else if (view.pid > 0) {
                footerDotCol = kSuccessGreen;
                footerText = "Target: javaw.exe (PID " + std::to_string(view.pid) + ")";
            } else {
                footerDotCol = kWarnYellow;
                footerText = "Target: Searching for Minecraft...";
            }

            const ImVec2 fDot(16.f, ds.y - 17.f);
            dl->AddCircleFilled(fDot, 3.5f, footerDotCol);
            dl->AddText(g_fontReg, 12.f, ImVec2(26.f, ds.y - 24.f), kTextSecondary, footerText.c_str(), nullptr, ds.x - 36.f);
        }

        // The auto-injection thread follows the setting every frame, and "Ask before
        // injecting" asks once for whichever Inject button was pressed.
        syncAutoThread();
        if (g_settingsDirty && ImGui::GetTime() - g_settingsSavedAt > 0.4) {
            saveInjectorSettings();
            g_settingsSavedAt = ImGui::GetTime();
            g_settingsDirty = false;
        }
        if (s_confirmPending) { ImGui::OpenPopup("##confirm-inject"); s_confirmPending = false; }
        if (ImGui::BeginPopupModal("##confirm-inject", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Inject Passive into Minecraft now?");
            ImGui::Spacing();
            if (ImGui::Button("Inject", ImVec2(120.f, 30.f))) {
                if (s_pendingInjectPid) StartWorkForPid(s_pendingInjectPid);
                else StartWork();
                s_pendingInjectPid = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.f, 30.f))) {
                s_pendingInjectPid = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End();
        ImGui::PopStyleColor();

        snprintf(g_stage, sizeof(g_stage), "render-present");
        ImGui::Render();

        const float clearCol[4] = { 0.043f, 0.043f, 0.063f, 1.f };
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clearCol);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap->Present(1, 0);
    }

    InterlockedExchange(&g_autoStop, 1);
    if (g_autoThread) { WaitForSingleObject(g_autoThread, 4000); CloseHandle(g_autoThread); g_autoThread = nullptr; }
    if (g_thread) { WaitForSingleObject(g_thread, 4000); CloseHandle(g_thread); }
    saveInjectorSettings();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_rtv) g_rtv->Release();
    if (g_swap) g_swap->Release();
    if (g_ctx) g_ctx->Release();
    if (g_dev) g_dev->Release();
    DestroyWindow(g_hwnd);
    return 0;
}
