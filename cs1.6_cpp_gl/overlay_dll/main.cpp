#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <psapi.h>
#include <avrt.h>
#include <gl/GL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cwctype>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "avrt.lib")

namespace {

void Log(const char* msg) {
    char path[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableA("APPDATA", path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;
    char filePath[MAX_PATH] = {};
    wsprintfA(filePath, "%s\\cs16ahk\\overlay_gl.log", path);
    CreateDirectoryA((std::string(path) + "\\cs16ahk").c_str(), nullptr);
    HANDLE f = CreateFileA(filePath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME st{}; GetLocalTime(&st);
    char line[512] = {};
    wsprintfA(line, "[ %02u:%02u:%02u.%03u pid=%lu ] %s\r\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentProcessId(), msg);
    DWORD written = 0;
    WriteFile(f, line, (DWORD)lstrlenA(line), &written, nullptr);
    CloseHandle(f);
}

constexpr int kMenuWidth = 320;
constexpr int kMenuHeight = 300;
constexpr int kHudWidth = 190;
constexpr int kHudHeight = 72;

struct Vec2 { int dx; int dy; };

struct MenuItem {
    RECT rect{}; // client coords in menu
    int id = 0;
    std::wstring label;
    bool isCheckbox = false;
    bool isProfile = false;
};

enum ItemId {
    ID_NRNS = 1,
    ID_PROFILE_PREV,
    ID_PROFILE_NEXT,
    ID_BHOP,
    ID_HUD,
    ID_SHAKE,
    ID_EXIT
};

// Shared state
std::atomic<bool> g_running{true};
std::atomic<bool> g_nrnsEnabled{false};
std::atomic<bool> g_bhopEnabled{false};
std::atomic<bool> g_hudEnabled{true};
std::atomic<bool> g_useScreenShake{true};
std::atomic<int> g_currentProfile{1};
std::atomic<int> g_recoilDelayMs{20};
std::atomic<int> g_bhopDelayMs{20};
std::atomic<bool> g_menuVisible{false};
std::atomic<bool> g_unloadRequested{false};

HMODULE g_hModule = nullptr;
HANDLE g_workerThread = nullptr;

HWND g_gameWnd = nullptr;
WNDPROC g_origWndProc = nullptr;
bool g_wndUnicode = true;
std::wstring g_configFile;
std::vector<MenuItem> g_menuItems;

// Menu position (movable)
int g_menuX = 50;
int g_menuY = 50;
bool g_draggingMenu = false;
POINT g_dragOffset{};
bool g_prevMenuLButton = false;
bool g_wndProcHooked = false;

// GL font
GLuint g_fontBase = 0;
bool g_fontReady = false;
HGLRC g_fontCtx = nullptr;

// Hook
using SwapBuffersFn = BOOL(WINAPI*)(HDC);
SwapBuffersFn g_origSwapBuffers = nullptr;
BYTE g_origBytes[16] = {};
BYTE g_hookBytes[16] = {};
void* g_hookTarget = nullptr;
bool g_hooked = false;
DWORD g_hookSize = 5;

const wchar_t* ProfileName(int profile) {
    switch (profile) {
        case 1: return L"AK-47";
        case 2: return L"M4A1";
        case 3: return L"SMG";
        case 4: return L"Pistola";
        default: return L"AK-47";
    }
}

Vec2 GetRecoilStep(int profile, int step) {
    int dx = 0, dy = 0;
    if (profile == 1) {
        dy = 4;
        if (step <= 4) dx = 0;
        else if (step <= 8) dx = -1;
        else if (step <= 12) dx = -2;
        else if (step <= 16) dx = -1;
        else if (step <= 20) dx = 1;
        else if (step <= 24) dx = 2;
        else if (step <= 28) dx = 1;
        else dx = 0;
    } else if (profile == 2) {
        dy = 3;
        if (step <= 6) dx = 0;
        else if (step <= 12) dx = -1;
        else if (step <= 18) dx = 1;
        else dx = 0;
    } else if (profile == 3) {
        dy = 2;
        if (step % 6 == 0) dx = -1;
        else if (step % 3 == 0) dx = 1;
        else dx = 0;
    } else {
        dy = 1;
        dx = 0;
    }
    return {dx, dy};
}

void SendMouseMove(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
}

void SendKeyTap(WORD vk, bool upThenDown = false) {
    UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    WORD scanWord = static_cast<WORD>(scan);
    DWORD ext = (scan & 0xFF00) ? KEYEVENTF_EXTENDEDKEY : 0;

    INPUT inputs[2]{};
    if (upThenDown) {
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = vk;
        inputs[0].ki.wScan = scanWord;
        inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE | ext | KEYEVENTF_KEYUP;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = vk;
        inputs[1].ki.wScan = scanWord;
        inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE | ext;
    } else {
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = vk;
        inputs[0].ki.wScan = scanWord;
        inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE | ext;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = vk;
        inputs[1].ki.wScan = scanWord;
        inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE | ext | KEYEVENTF_KEYUP;
    }
    SendInput(2, inputs, sizeof(INPUT));
}

void SendBhopJump() {
    UINT scan = MapVirtualKeyW(VK_SPACE, MAPVK_VK_TO_VSC);
    WORD scanWord = static_cast<WORD>(scan);

    // GoldSrc bhop: reliable scancode release+press tap via SendInput.
    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = scanWord;
    inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wScan = scanWord;
    inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE;
    SendInput(2, inputs, sizeof(INPUT));
}

std::wstring GetConfigPath() {
    wchar_t appData[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L"";
    std::wstring dir = std::wstring(appData) + L"\\cs16ahk";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\settings.ini";
}

void SaveSettings() {
    if (g_configFile.empty()) return;
    auto writeInt = [](const wchar_t* key, int value) {
        wchar_t buf[32];
        wsprintfW(buf, L"%d", value);
        WritePrivateProfileStringW(L"Settings", key, buf, g_configFile.c_str());
    };
    writeInt(L"CurrentProfile", g_currentProfile.load());
    writeInt(L"UseHUD", g_hudEnabled.load() ? 1 : 0);
    writeInt(L"UseScreenShake", g_useScreenShake.load() ? 1 : 0);
    writeInt(L"RecoilDelay", g_recoilDelayMs.load());
    writeInt(L"BhopDelay", g_bhopDelayMs.load());
    writeInt(L"NrnsEnabled", g_nrnsEnabled.load() ? 1 : 0);
    writeInt(L"BhopEnabled", g_bhopEnabled.load() ? 1 : 0);
}

void LoadSettings() {
    g_configFile = GetConfigPath();
    if (g_configFile.empty()) return;

    int profile = GetPrivateProfileIntW(L"Settings", L"CurrentProfile", 1, g_configFile.c_str());
    if (profile < 1 || profile > 4) profile = 1;
    g_currentProfile.store(profile);

    g_hudEnabled.store(GetPrivateProfileIntW(L"Settings", L"UseHUD", 1, g_configFile.c_str()) != 0);
    g_useScreenShake.store(GetPrivateProfileIntW(L"Settings", L"UseScreenShake", 1, g_configFile.c_str()) != 0);

    int recoilDelay = GetPrivateProfileIntW(L"Settings", L"RecoilDelay", 20, g_configFile.c_str());
    g_recoilDelayMs.store(std::clamp(recoilDelay, 1, 500));

    int bhopDelay = GetPrivateProfileIntW(L"Settings", L"BhopDelay", 20, g_configFile.c_str());
    g_bhopDelayMs.store(std::clamp(bhopDelay, 1, 500));

    g_nrnsEnabled.store(GetPrivateProfileIntW(L"Settings", L"NrnsEnabled", 0, g_configFile.c_str()) != 0);
    g_bhopEnabled.store(GetPrivateProfileIntW(L"Settings", L"BhopEnabled", 0, g_configFile.c_str()) != 0);
}

void BuildMenuItems() {
    g_menuItems.clear();
    int x = 24;
    int y = 60;
    int rowH = 34;
    auto addCheck = [&](int id, const std::wstring& label) {
        MenuItem it; it.id = id; it.label = label; it.isCheckbox = true;
        it.rect = {x, y, kMenuWidth - 24, y + 26};
        g_menuItems.push_back(it);
        y += rowH;
    };

    addCheck(ID_NRNS, L"NR/NS (NoRecoil + NoSpread)");

    MenuItem prof; prof.id = 0; prof.isProfile = true; prof.label = L"Perfil";
    prof.rect = {x, y, kMenuWidth - 24, y + 26};
    g_menuItems.push_back(prof);
    MenuItem prev; prev.id = ID_PROFILE_PREV; prev.label = L"<"; prev.rect = {170, y, 198, y + 26};
    MenuItem next; next.id = ID_PROFILE_NEXT; next.label = L">"; next.rect = {284, y, 312, y + 26};
    g_menuItems.push_back(prev);
    g_menuItems.push_back(next);
    y += rowH;

    addCheck(ID_BHOP, L"Bunny Hop");
    addCheck(ID_HUD, L"HUD Overlay");
    addCheck(ID_SHAKE, L"Screen Shake");

    MenuItem exitBtn; exitBtn.id = ID_EXIT; exitBtn.label = L"Fechar/Desinjetar (F12)"; exitBtn.isCheckbox = false;
    exitBtn.rect = {x, y + 8, kMenuWidth - 24, y + 34};
    g_menuItems.push_back(exitBtn);
}

bool GetCheckboxState(int id) {
    switch (id) {
        case ID_NRNS: return g_nrnsEnabled.load();
        case ID_BHOP: return g_bhopEnabled.load();
        case ID_HUD: return g_hudEnabled.load();
        case ID_SHAKE: return g_useScreenShake.load();
        default: return false;
    }
}

void ToggleCheckbox(int id) {
    switch (id) {
        case ID_NRNS: g_nrnsEnabled.store(!g_nrnsEnabled.load()); break;
        case ID_BHOP: g_bhopEnabled.store(!g_bhopEnabled.load()); break;
        case ID_HUD: g_hudEnabled.store(!g_hudEnabled.load()); break;
        case ID_SHAKE: g_useScreenShake.store(!g_useScreenShake.load()); break;
        default: break;
    }
    SaveSettings();
}

void ChangeProfile(int delta) {
    int p = g_currentProfile.load() + delta;
    if (p < 1) p = 4;
    if (p > 4) p = 1;
    g_currentProfile.store(p);
    SaveSettings();
}

bool IsGameForeground() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

bool IsInGameplay() {
    if (!IsGameForeground()) return false;
    CURSORINFO ci{};
    ci.cbSize = sizeof(ci);
    if (!GetCursorInfo(&ci)) return false;
    // In CS 1.6 gameplay the cursor is hidden/captured; menus/console show cursor.
    if (ci.flags & CURSOR_SHOWING) return false;

    // Optional guard: if cursor clip is clearly outside the game client, treat as menu.
    RECT clip{};
    if (GetClipCursor(&clip) && g_gameWnd) {
        RECT client{}; GetClientRect(g_gameWnd, &client);
        POINT tl{0,0}, br{client.right, client.bottom};
        ClientToScreen(g_gameWnd, &tl);
        ClientToScreen(g_gameWnd, &br);
        const LONG pad = 8;
        bool clippedToClient = clip.left >= tl.x - pad && clip.top >= tl.y - pad &&
                               clip.right <= br.x + pad && clip.bottom <= br.y + pad &&
                               (clip.right - clip.left) <= (br.x - tl.x + pad) &&
                               (clip.bottom - clip.top) <= (br.y - tl.y + pad);
        bool clippedToScreen = (clip.left <= 0 && clip.top <= 0 &&
                                clip.right >= GetSystemMetrics(SM_CXSCREEN) - 1 &&
                                clip.bottom >= GetSystemMetrics(SM_CYSCREEN) - 1);
        if (!clippedToClient && !clippedToScreen) return false;
    }
    return true;
}

// --- Minimal x86 inline hook for wglSwapBuffers ---
BOOL WINAPI HookedSwapBuffers(HDC hdc);
void RenderOverlay(HDC hdc);

static bool IsRelativeBranchOpcode(const BYTE* p) {
    BYTE b = p[0];
    if (b == 0xE8 || b == 0xE9 || b == 0xEB) return true;
    if (b >= 0x70 && b <= 0x7F) return true;
    if (b >= 0xE0 && b <= 0xE3) return true;
    if (b == 0x0F) {
        BYTE b2 = p[1];
        if (b2 >= 0x80 && b2 <= 0x8F) return true;
    }
    return false;
}

bool InstallSwapBuffersHook() {
    if (g_hooked) return true;
    HMODULE ogl = GetModuleHandleW(L"opengl32.dll");
    if (!ogl) ogl = LoadLibraryW(L"opengl32.dll");
    if (!ogl) { Log("opengl32.dll not found"); return false; }
    BYTE* target = (BYTE*)GetProcAddress(ogl, "wglSwapBuffers");
    if (!target) { Log("wglSwapBuffers not found"); return false; }

    char logBuf[160] = {};
    wsprintfA(logBuf, "wglSwapBuffers prologue: %02X %02X %02X %02X %02X %02X",
              target[0], target[1], target[2], target[3], target[4], target[5]);
    Log(logBuf);

    g_hookTarget = target;
    g_hookSize = 5;

    bool isRelativeJmpHook = false;
    BYTE* existingJmpTarget = nullptr;

    if (target[0] == 0xE9) {
        // Already hooked by another overlay/capture. Chain through it.
        int32_t rel = *(int32_t*)(target + 1);
        existingJmpTarget = target + 5 + rel;
        isRelativeJmpHook = true;
        Log("Detected existing E9 hook; chaining");
    } else if (target[0] == 0x8B && target[1] == 0xFF) {
        g_hookSize = 5; // mov edi,edi; push ebp; mov ebp,esp
    } else if (target[0] == 0x55 && target[1] == 0x8B && target[2] == 0xEC) {
        g_hookSize = 5; // push ebp; mov ebp,esp
    } else if (target[0] == 0x55 && target[1] == 0x89 && target[2] == 0xE5) {
        g_hookSize = 5; // push ebp; mov ebp,esp
    } else {
        Log("Unsafe/unknown wglSwapBuffers prologue; skipping hook");
        return false;
    }

    if (!isRelativeJmpHook) {
        for (DWORD i = 0; i < g_hookSize; ++i) {
            if (IsRelativeBranchOpcode(target + i)) {
                Log("Relative branch in prologue; skipping hook");
                return false;
            }
        }
    }

    DWORD oldProt = 0;
    if (!VirtualProtect(target, g_hookSize, PAGE_EXECUTE_READWRITE, &oldProt)) {
        Log("VirtualProtect failed");
        return false;
    }
    memcpy(g_origBytes, target, g_hookSize);

    BYTE* tramp = (BYTE*)VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) {
        VirtualProtect(target, g_hookSize, oldProt, &oldProt);
        Log("VirtualAlloc trampoline failed");
        return false;
    }

    if (isRelativeJmpHook) {
        // Trampoline jumps to the existing hook target (preserve hook chain).
        tramp[0] = 0xE9;
        *(int32_t*)(tramp + 1) = (int32_t)(existingJmpTarget - (tramp + 5));
    } else {
        memcpy(tramp, g_origBytes, g_hookSize);
        DWORD relBack = (DWORD)(target + g_hookSize - (tramp + g_hookSize + 5));
        tramp[g_hookSize] = 0xE9;
        *(DWORD*)(tramp + g_hookSize + 1) = relBack;
    }
    FlushInstructionCache(GetCurrentProcess(), tramp, g_hookSize + 5);
    g_origSwapBuffers = (SwapBuffersFn)tramp;

    DWORD relHook = (DWORD)((BYTE*)&HookedSwapBuffers - (target + 5));
    BYTE patch[16] = {0};
    patch[0] = 0xE9;
    *(DWORD*)(patch + 1) = relHook;
    for (DWORD i = 5; i < g_hookSize; ++i) patch[i] = 0x90;
    memcpy(target, patch, g_hookSize);
    memcpy(g_hookBytes, patch, g_hookSize);

    VirtualProtect(target, g_hookSize, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), target, g_hookSize);
    g_hooked = true;
    Log("SwapBuffers hook installed");
    return true;
}

void RemoveSwapBuffersHook() {
    if (!g_hooked || !g_hookTarget) return;
    DWORD oldProt;
    if (VirtualProtect(g_hookTarget, g_hookSize, PAGE_EXECUTE_READWRITE, &oldProt)) {
        memcpy(g_hookTarget, g_origBytes, g_hookSize);
        VirtualProtect(g_hookTarget, g_hookSize, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), g_hookTarget, g_hookSize);
    }
    g_hooked = false;
}

// --- GL drawing helpers ---
void EnsureFont(HDC hdc) {
    HGLRC cur = wglGetCurrentContext();
    if (g_fontReady && g_fontCtx == cur) return;
    if (g_fontBase == 0) g_fontBase = glGenLists(96);

    HDC fontDC = hdc;
    HDC wndDC = nullptr;
    if (g_gameWnd) {
        wndDC = GetDC(g_gameWnd);
        if (wndDC) fontDC = wndDC;
    }

    HFONT font = CreateFontA(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             FF_DONTCARE | DEFAULT_PITCH, "Arial");
    HFONT oldFont = (HFONT)SelectObject(fontDC, font);
    BOOL ok = wglUseFontBitmapsA(fontDC, 32, 96, g_fontBase);
    SelectObject(fontDC, oldFont);
    DeleteObject(font);

    if (!ok) {
        HFONT sys = (HFONT)GetStockObject(SYSTEM_FONT);
        HFONT old2 = (HFONT)SelectObject(hdc, sys);
        ok = wglUseFontBitmapsA(hdc, 32, 96, g_fontBase);
        SelectObject(hdc, old2);
    }

    if (wndDC) ReleaseDC(g_gameWnd, wndDC);

    if (ok) {
        g_fontReady = true;
        g_fontCtx = cur;
        Log("Font bitmaps created");
    } else {
        char buf[128];
        sprintf_s(buf, "wglUseFontBitmapsA failed err=%lu", GetLastError());
        Log(buf);
    }
}

void DrawTextGL(float x, float y, const char* text) {
    if (!text || !g_fontReady) return;
    glRasterPos2f(x, y);
    glListBase(g_fontBase - 32);
    glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, text);
}

void DrawRectGL(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void DrawBorderGL(float x, float y, float w, float h, float r, float g, float b, float a, float lineW = 1.0f) {
    glColor4f(r, g, b, a);
    glLineWidth(lineW);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

std::string Narrow(const std::wstring& ws) {
    if (ws.empty()) return {};
    std::string s;
    s.reserve(ws.size());
    for (wchar_t c : ws) s.push_back((c >= 32 && c < 127) ? (char)c : '?');
    return s;
}

void DrawHudOverlay(int vpW, int /*vpH*/) {
    if (!g_hudEnabled.load()) return;
    float x = (float)(vpW - kHudWidth - 30);
    float y = 30.0f;
    float w = (float)kHudWidth, h = (float)kHudHeight;

    DrawRectGL(x, y, w, h, 0.07f, 0.07f, 0.07f, 0.86f);
    DrawBorderGL(x, y, w, h, 0.7f, 0.0f, 0.0f, 1.0f, 1.5f);

    bool nrns = g_nrnsEnabled.load();
    bool bhop = g_bhopEnabled.load();
    std::string p = std::string("[") + Narrow(ProfileName(g_currentProfile.load())) + "]";

    if (nrns) glColor3f(1.0f, 0.24f, 0.24f); else glColor3f(0.47f, 0.47f, 0.47f);
    DrawTextGL(x + 45, y + 18, nrns ? "NR/NS [ON]" : "NR/NS [OFF]");

    glColor3f(0.86f, 0.86f, 0.86f);
    DrawTextGL(x + 65, y + 38, p.c_str());

    if (bhop) glColor3f(1.0f, 0.24f, 0.24f); else glColor3f(0.47f, 0.47f, 0.47f);
    DrawTextGL(x + 48, y + 58, bhop ? "Bhop [ON]" : "Bhop [OFF]");
}

void DrawMenuOverlay(int /*vpW*/, int /*vpH*/) {
    if (!g_menuVisible.load()) return;
    float x = (float)g_menuX, y = (float)g_menuY;
    float w = (float)kMenuWidth, h = (float)kMenuHeight;

    // Background and border
    DrawRectGL(x, y, w, h, 0.047f, 0.047f, 0.047f, 0.96f);
    DrawBorderGL(x, y, w, h, 0.7f, 0.0f, 0.0f, 1.0f, 2.0f);
    // Title bar
    DrawRectGL(x + 2, y + 2, w - 4, 38, 0.157f, 0.0f, 0.0f, 1.0f);
    glColor3f(1.0f, 0.2f, 0.2f);
    DrawTextGL(x + 70, y + 26, "CS 1.6 MOD MENU (OpenGL)");

    for (const auto& it : g_menuItems) {
        float ix = x + it.rect.left;
        float iy = y + it.rect.top;
        float iw = (float)(it.rect.right - it.rect.left);
        float ih = (float)(it.rect.bottom - it.rect.top);
        if (it.isCheckbox) {
            bool checked = GetCheckboxState(it.id);
            // box
            DrawRectGL(ix, iy + 3, 18, 18, 0.08f, 0.08f, 0.08f, 1.0f);
            DrawBorderGL(ix, iy + 3, 18, 18, 0.7f, 0.0f, 0.0f, 1.0f, 1.5f);
            if (checked) DrawRectGL(ix + 4, iy + 7, 10, 10, 0.86f, 0.12f, 0.12f, 1.0f);
            if (checked) glColor3f(0.94f, 0.94f, 0.94f); else glColor3f(0.67f, 0.67f, 0.67f);
            DrawTextGL(ix + 28, iy + 18, Narrow(it.label).c_str());
        } else if (it.isProfile) {
            glColor3f(0.78f, 0.78f, 0.78f);
            DrawTextGL(ix, iy + 18, "Perfil:");
            // buttons
            for (const auto& b : g_menuItems) {
                if (b.id == ID_PROFILE_PREV || b.id == ID_PROFILE_NEXT) {
                    float bx = x + b.rect.left, by = y + b.rect.top;
                    float bw = (float)(b.rect.right - b.rect.left), bh = (float)(b.rect.bottom - b.rect.top);
                    DrawRectGL(bx, by, bw, bh, 0.2f, 0.04f, 0.04f, 1.0f);
                    DrawBorderGL(bx, by, bw, bh, 0.7f, 0.0f, 0.0f, 1.0f, 1.0f);
                    glColor3f(1.0f, 0.31f, 0.31f);
                    DrawTextGL(bx + 10, by + 18, Narrow(b.label).c_str());
                }
            }
            glColor3f(1.0f, 0.27f, 0.27f);
            DrawTextGL(x + 210, iy + 18, Narrow(ProfileName(g_currentProfile.load())).c_str());
        } else if (it.id == ID_EXIT) {
            DrawRectGL(ix, iy, iw, ih, 0.24f, 0.0f, 0.0f, 1.0f);
            DrawBorderGL(ix, iy, iw, ih, 0.78f, 0.0f, 0.0f, 1.0f, 1.0f);
            glColor3f(1.0f, 0.78f, 0.78f);
            DrawTextGL(ix + 35, iy + 18, Narrow(it.label).c_str());
        }
    }

    glColor3f(0.55f, 0.55f, 0.55f);
    DrawTextGL(x + 12, y + kMenuHeight - 10, "Insert: menu | Arraste pela barra | F12: fechar");
}

void HandleMenuInputPoll() {
    if (!g_menuVisible.load() || g_wndProcHooked || !g_gameWnd) return;

    POINT pt{};
    GetCursorPos(&pt);
    ScreenToClient(g_gameWnd, &pt);

    bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool pressed = down && !g_prevMenuLButton;
    bool released = !down && g_prevMenuLButton;
    g_prevMenuLButton = down;

    if (pressed) {
        if (pt.x >= g_menuX && pt.x < g_menuX + kMenuWidth && pt.y >= g_menuY && pt.y < g_menuY + 40) {
            g_draggingMenu = true;
            g_dragOffset.x = pt.x - g_menuX;
            g_dragOffset.y = pt.y - g_menuY;
            return;
        }
        POINT local{pt.x - g_menuX, pt.y - g_menuY};
        for (const auto& it : g_menuItems) {
            if (it.isProfile) continue;
            if (PtInRect(&it.rect, local)) {
                if (it.isCheckbox) ToggleCheckbox(it.id);
                else if (it.id == ID_PROFILE_PREV) ChangeProfile(-1);
                else if (it.id == ID_PROFILE_NEXT) ChangeProfile(1);
                else if (it.id == ID_EXIT) g_unloadRequested.store(true);
                return;
            }
        }
    }

    if (released) {
        g_draggingMenu = false;
    }

    if (g_draggingMenu && down) {
        g_menuX = pt.x - g_dragOffset.x;
        g_menuY = pt.y - g_dragOffset.y;
    }
}

void RenderOverlay(HDC hdc) {
    // Save GL state
    GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
    int vpW = viewport[2], vpH = viewport[3];
    if (vpW <= 0 || vpH <= 0) return;

    HandleMenuInputPoll();

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0, vpW, vpH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    EnsureFont(hdc);

    DrawHudOverlay(vpW, vpH);
    DrawMenuOverlay(vpW, vpH);

    // Simple cursor when menu open
    if (g_menuVisible.load()) {
        POINT pt; GetCursorPos(&pt);
        if (g_gameWnd) ScreenToClient(g_gameWnd, &pt);
        float cx = (float)pt.x, cy = (float)pt.y;
        glColor3f(1.0f, 0.2f, 0.2f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2f(cx - 6, cy); glVertex2f(cx + 6, cy);
        glVertex2f(cx, cy - 6); glVertex2f(cx, cy + 6);
        glEnd();
    }

    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}

BOOL WINAPI HookedSwapBuffers(HDC hdc) {
    if (g_running.load(std::memory_order_relaxed) && g_origSwapBuffers) {
        RenderOverlay(hdc);
    }
    return g_origSwapBuffers ? g_origSwapBuffers(hdc) : SwapBuffers(hdc);
}

// --- Minimal x86 inline hook (32-bit) for cursor control while menu is open ---
struct InlineHook32 {
    void* target = nullptr;
    void* detour = nullptr;
    void* trampoline = nullptr;
    BYTE original[5]{};
    bool installed = false;
};

static InlineHook32 g_hookSetCursorPos{};
static InlineHook32 g_hookClipCursor{};

static bool InstallInlineHook32(InlineHook32& h, void* target, void* detour) {
    if (!target || !detour || h.installed) return h.installed;
    BYTE* src = reinterpret_cast<BYTE*>(target);
    // Accept common 5-byte prologues: hotpatch (8B FF 55 8B EC) or push ebp; mov ebp,esp; sub/push imm8
    bool prologueOk = (src[0] == 0x8B && src[1] == 0xFF && src[2] == 0x55 && src[3] == 0x8B && src[4] == 0xEC) ||
                      (src[0] == 0x55 && src[1] == 0x8B && src[2] == 0xEC && src[3] == 0x83 && src[4] == 0xEC) ||
                      (src[0] == 0x55 && src[1] == 0x8B && src[2] == 0xEC && src[3] == 0x6A);
    if (!prologueOk) {
        char buf[160];
        sprintf_s(buf, "InlineHook32: unsupported prologue %02X %02X %02X %02X %02X", src[0], src[1], src[2], src[3], src[4]);
        Log(buf);
        return false;
    }
    DWORD oldProt = 0;
    if (!VirtualProtect(src, 5, PAGE_EXECUTE_READWRITE, &oldProt)) return false;

    memcpy(h.original, src, 5);
    BYTE* tramp = reinterpret_cast<BYTE*>(VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp) { VirtualProtect(src, 5, oldProt, &oldProt); return false; }

    memcpy(tramp, h.original, 5);
    DWORD trampRel = (DWORD)src + 5 - ((DWORD)tramp + 5 + 5);
    tramp[5] = 0xE9;
    memcpy(tramp + 6, &trampRel, 4);

    DWORD rel = (DWORD)detour - ((DWORD)src + 5);
    src[0] = 0xE9;
    memcpy(src + 1, &rel, 4);

    VirtualProtect(src, 5, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), src, 5);

    h.target = target;
    h.detour = detour;
    h.trampoline = tramp;
    h.installed = true;
    return true;
}

static void RemoveInlineHook32(InlineHook32& h) {
    if (!h.installed || !h.target) return;
    BYTE* src = reinterpret_cast<BYTE*>(h.target);
    DWORD oldProt = 0;
    if (VirtualProtect(src, 5, PAGE_EXECUTE_READWRITE, &oldProt)) {
        memcpy(src, h.original, 5);
        VirtualProtect(src, 5, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), src, 5);
    }
    if (h.trampoline) { VirtualFree(h.trampoline, 0, MEM_RELEASE); h.trampoline = nullptr; }
    h.installed = false;
}

using SetCursorPosFn = BOOL(WINAPI*)(int, int);
using ClipCursorFn = BOOL(WINAPI*)(const RECT*);

static BOOL WINAPI HookedSetCursorPos(int X, int Y) {
    if (g_menuVisible.load()) return TRUE; // block game recenter while menu open
    auto fn = reinterpret_cast<SetCursorPosFn>(g_hookSetCursorPos.trampoline);
    return fn ? fn(X, Y) : TRUE;
}

static BOOL WINAPI HookedClipCursor(const RECT* lpRect) {
    if (g_menuVisible.load()) {
        auto fn = reinterpret_cast<ClipCursorFn>(g_hookClipCursor.trampoline);
        return fn ? fn(nullptr) : TRUE; // keep cursor unclipped
    }
    auto fn = reinterpret_cast<ClipCursorFn>(g_hookClipCursor.trampoline);
    return fn ? fn(lpRect) : TRUE;
}

static void InstallCursorHooks() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    InstallInlineHook32(g_hookSetCursorPos, reinterpret_cast<void*>(GetProcAddress(user32, "SetCursorPos")), reinterpret_cast<void*>(&HookedSetCursorPos));
    InstallInlineHook32(g_hookClipCursor, reinterpret_cast<void*>(GetProcAddress(user32, "ClipCursor")), reinterpret_cast<void*>(&HookedClipCursor));
}

static void RemoveCursorHooks() {
    RemoveInlineHook32(g_hookSetCursorPos);
    RemoveInlineHook32(g_hookClipCursor);
}

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Consume our hotkeys so the game doesn't react to them (prevents minimize/other binds)
    if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {
        if (wParam == VK_INSERT || wParam == VK_F12) {
            return 0;
        }
    }

    if (g_menuVisible.load()) {
        if (msg == WM_SETCURSOR) {
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            return TRUE;
        }
        switch (msg) {
            case WM_LBUTTONDOWN: {
                POINT pt{(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};
                bool insideMenu = (pt.x >= g_menuX && pt.x < g_menuX + kMenuWidth && pt.y >= g_menuY && pt.y < g_menuY + kMenuHeight);
                if (!insideMenu) break; // let game handle clicks outside overlay menu

                // title bar drag
                if (pt.y < g_menuY + 40) {
                    g_draggingMenu = true;
                    g_dragOffset.x = pt.x - g_menuX;
                    g_dragOffset.y = pt.y - g_menuY;
                    SetCapture(hwnd);
                    return 0;
                }
                // items
                POINT local{pt.x - g_menuX, pt.y - g_menuY};
                for (const auto& it : g_menuItems) {
                    if (it.isProfile) continue;
                    if (PtInRect(&it.rect, local)) {
                        if (it.isCheckbox) ToggleCheckbox(it.id);
                        else if (it.id == ID_PROFILE_PREV) ChangeProfile(-1);
                        else if (it.id == ID_PROFILE_NEXT) ChangeProfile(1);
                        else if (it.id == ID_EXIT) g_unloadRequested.store(true);
                        return 0;
                    }
                }
                // swallow clicks inside menu background to avoid firing in game
                return 0;
            }
            case WM_LBUTTONUP:
                if (g_draggingMenu) { g_draggingMenu = false; ReleaseCapture(); return 0; }
                break; // let game receive mouse up when not dragging overlay
            case WM_MOUSEMOVE:
                if (g_draggingMenu) {
                    POINT pt{(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};
                    g_menuX = pt.x - g_dragOffset.x;
                    g_menuY = pt.y - g_dragOffset.y;
                }
                return 0; // block game mouselook while menu open
            case WM_INPUT:
                return 0; // block raw input mouselook while menu open
            case WM_RBUTTONDOWN: case WM_RBUTTONUP:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP: {
                POINT pt{(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};
                bool insideMenu = (pt.x >= g_menuX && pt.x < g_menuX + kMenuWidth && pt.y >= g_menuY && pt.y < g_menuY + kMenuHeight);
                if (insideMenu) return 0; // block only inside overlay menu
                break;
            }
            case WM_MOUSEWHEEL: {
                POINT pt{(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};
                ScreenToClient(hwnd, &pt);
                bool insideMenu = (pt.x >= g_menuX && pt.x < g_menuX + kMenuWidth && pt.y >= g_menuY && pt.y < g_menuY + kMenuHeight);
                if (insideMenu) return 0;
                break;
            }
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                // hotkeys already consumed above; block other keys from game while menu open
                return 0;
            default:
                break;
        }
    }
    return g_wndUnicode
        ? CallWindowProcW(g_origWndProc, hwnd, msg, wParam, lParam)
        : CallWindowProcA(g_origWndProc, hwnd, msg, wParam, lParam);
}

DWORD WINAPI WorkerLoop(LPVOID) {
    Log("Worker thread started");
    bool prevInsert = false;
    bool prevF12 = false;
    bool prevNrns = false;
    bool prevExitCombo = false;
    bool lWasDown = false;
    bool aimActive = false;
    int shotStep = 0;
    ULONGLONG nextRecoilMs = 0;
    ULONGLONG nextBhopMs = 0;
    ULONGLONG lastBhopLogMs = 0;
    bool bhopRepressPending = false;
    ULONGLONG bhopRepressMs = 0;

    for (;;) {
        if (g_unloadRequested.load()) break;

        bool insertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (insertDown && !prevInsert) {
            bool nowVisible = !g_menuVisible.load();
            g_menuVisible.store(nowVisible);
            g_draggingMenu = false;
            g_prevMenuLButton = false;
            ClipCursor(nullptr);
            if (nowVisible) {
                while (ShowCursor(TRUE) < 0) {}
            } else {
                while (ShowCursor(FALSE) >= 0) {}
            }
        }
        prevInsert = insertDown;

        bool f12Down = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
        if (f12Down && !prevF12) g_unloadRequested.store(true);
        prevF12 = f12Down;

        bool exitCombo = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_END) & 0x8000);
        if (exitCombo && !prevExitCombo) g_unloadRequested.store(true);
        prevExitCombo = exitCombo;

        bool nrnsHotkey = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
        if (nrnsHotkey && !prevNrns) {
            ToggleCheckbox(ID_NRNS);
        }
        prevNrns = nrnsHotkey;

        ULONGLONG now = GetTickCount64();
        bool canRecoil = !g_menuVisible.load() && IsInGameplay();
        bool canBhop = IsGameForeground();
        bool nrnsOn = g_nrnsEnabled.load();
        bool lDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        if (nrnsOn && lDown && !lWasDown && canRecoil) {
            aimActive = true;
            shotStep = 0;
            nextRecoilMs = now;
        }
        if (!lDown && lWasDown) {
            aimActive = false;
            shotStep = 0;
        }
        lWasDown = lDown;

        if (aimActive) {
            if (!nrnsOn || !canRecoil) {
                aimActive = false;
                shotStep = 0;
            } else if (now >= nextRecoilMs) {
                ++shotStep;
                Vec2 mv = GetRecoilStep(g_currentProfile.load(), shotStep);
                SendMouseMove(mv.dx, mv.dy);
                if (g_useScreenShake.load()) {
                    int rx = (shotStep % 3) - 1;
                    int ry = ((shotStep / 2) % 3) - 1;
                    SendMouseMove(rx, ry);
                }
                int delay = std::clamp(g_recoilDelayMs.load(), 1, 500);
                nextRecoilMs = now + (ULONGLONG)delay;
            }
        }

        bool bhopOn = g_bhopEnabled.load() && !g_menuVisible.load() && IsGameForeground();
        bool spaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        if (spaceDown && (now - lastBhopLogMs) >= 1000) {
            char bLog[160];
            wsprintfA(bLog, "bhop diag: enabled=%d menu=%d fg=%d space=%d wnd=%p",
                      g_bhopEnabled.load() ? 1 : 0, g_menuVisible.load() ? 1 : 0,
                      IsGameForeground() ? 1 : 0, spaceDown ? 1 : 0, g_gameWnd);
            Log(bLog);
            lastBhopLogMs = now;
        }

        auto sendSpaceScancode = [&](DWORD flags) {
            UINT scan = MapVirtualKeyW(VK_SPACE, MAPVK_VK_TO_VSC);
            INPUT in{};
            in.type = INPUT_KEYBOARD;
            in.ki.wVk = VK_SPACE;
            in.ki.wScan = static_cast<WORD>(scan);
            in.ki.dwFlags = KEYEVENTF_SCANCODE | flags;
            SendInput(1, &in, sizeof(in));
        };

        if (bhopOn && spaceDown) {
            int delay = std::clamp(g_bhopDelayMs.load(), 10, 500);
            int gapMs = std::clamp(delay / 4, 4, 10);
            if (bhopRepressPending) {
                if (now >= bhopRepressMs) {
                    sendSpaceScancode(0); // repress after brief release gap
                    bhopRepressPending = false;
                    nextBhopMs = now;
                }
            } else if ((now - nextBhopMs) >= (ULONGLONG)delay) {
                sendSpaceScancode(KEYEVENTF_KEYUP); // brief release for jump edge
                bhopRepressPending = true;
                bhopRepressMs = now + (ULONGLONG)gapMs;
            }
        } else {
            if (bhopRepressPending) {
                // Only restore press if space is still physically held (e.g. menu opened / bhop disabled).
                // If user released space, leave it released to avoid unintended jump.
                if (spaceDown) sendSpaceScancode(0);
                bhopRepressPending = false;
            }
            nextBhopMs = now;
        }

        Sleep(1);
    }
    Log("Worker thread exiting");
    return 0;
}

struct FindWindowCtx { HWND hwnd = nullptr; };
BOOL CALLBACK FindGameWindowEnumProc(HWND hwnd, LPARAM lp) {
    auto* c = reinterpret_cast<FindWindowCtx*>(lp);
    if (!IsWindowVisible(hwnd)) return TRUE;
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return TRUE;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return TRUE;
    wchar_t path[MAX_PATH]{}; DWORD sz = MAX_PATH;
    bool match = false;
    if (QueryFullProcessImageNameW(h, 0, path, &sz)) {
        wchar_t* name = wcsrchr(path, L'\\'); name = name ? name + 1 : path;
        match = (_wcsicmp(name, L"hl.exe") == 0 || _wcsicmp(name, L"cstrike.exe") == 0 || _wcsicmp(name, L"cs.exe") == 0);
    }
    CloseHandle(h);
    if (match) { c->hwnd = hwnd; return FALSE; }
    return TRUE;
}
HWND FindGameWindow() {
    FindWindowCtx ctx{};
    EnumWindows(FindGameWindowEnumProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.hwnd;
}
DWORD WINAPI MainThread(LPVOID) {
    Log("MainThread start");
    g_gameWnd = FindGameWindow();
    if (!g_gameWnd) {
        Log("Game window not found, unloading");
        FreeLibraryAndExitThread(g_hModule, 0);
        return 0;
    }

    LoadSettings();
    BuildMenuItems();
    Log("Settings/menu loaded");

    g_origWndProc = reinterpret_cast<WNDPROC>(
        g_wndUnicode ? SetWindowLongPtrW(g_gameWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc))
                     : SetWindowLongPtrA(g_gameWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));
    if (g_origWndProc) {
        g_wndProcHooked = true;
        Log(g_wndUnicode ? "WndProc hooked (W)" : "WndProc hooked (A)");
    } else {
        char buf[128];
        sprintf_s(buf, "WndProc hook failed err=%lu unicode=%d", GetLastError(), g_wndUnicode ? 1 : 0);
        Log(buf);
    }

    bool hookOk = false;
    for (int i = 0; i < 200 && !g_unloadRequested.load(); ++i) {
        if (InstallSwapBuffersHook()) { hookOk = true; break; }
        Sleep(100);
    }
    if (!hookOk) {
        Log("SwapBuffers hook install failed");
        if (g_origWndProc) {
            if (g_wndUnicode) SetWindowLongPtrW(g_gameWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origWndProc));
            else SetWindowLongPtrA(g_gameWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origWndProc));
            g_origWndProc = nullptr;
            g_wndProcHooked = false;
        }
        FreeLibraryAndExitThread(g_hModule, 0);
        return 0;
    }

    InstallCursorHooks();

    HANDLE worker = CreateThread(nullptr, 0, WorkerLoop, nullptr, 0, nullptr);
    if (worker) {
        Log("Worker thread started handle ok");
        while (!g_unloadRequested.load()) Sleep(50);
        WaitForSingleObject(worker, 2000);
        CloseHandle(worker);
    } else {
        Log("Worker thread create failed");
    }

    RemoveSwapBuffersHook();
    RemoveCursorHooks();
    if (g_origWndProc) {
        if (g_wndUnicode) SetWindowLongPtrW(g_gameWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origWndProc));
        else SetWindowLongPtrA(g_gameWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origWndProc));
        g_origWndProc = nullptr;
        g_wndProcHooked = false;
    }
    SaveSettings();
    Log("MainThread cleanup done, unloading");
    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
}
} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
