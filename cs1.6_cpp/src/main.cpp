#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <psapi.h>
#include <avrt.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "avrt.lib")

namespace {

constexpr wchar_t kHudClassName[] = L"CS16AHKCppHudWindow";
constexpr wchar_t kMenuClassName[] = L"CS16AHKCppMenuWindow";
constexpr wchar_t kMutexName[] = L"Local\\CS16AHK_CPP_SINGLE_INSTANCE";

constexpr int kHudWidth = 190;
constexpr int kHudHeight = 72;
constexpr int kMenuWidth = 320;
constexpr int kMenuHeight = 300;

constexpr int HOTKEY_MENU = 1;
constexpr int HOTKEY_EXIT = 2;

constexpr UINT WM_APP_UPDATE_UI = WM_APP + 1;

struct Vec2 { int dx; int dy; };

struct MenuItem {
    RECT rect{};
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

std::atomic<bool> g_running{true};
std::atomic<bool> g_nrnsEnabled{false};
std::atomic<bool> g_bhopEnabled{false};
std::atomic<bool> g_hudEnabled{true};
std::atomic<bool> g_useScreenShake{true};
std::atomic<int> g_currentProfile{1};
std::atomic<int> g_recoilDelayMs{20};
std::atomic<int> g_bhopDelayMs{20};
std::atomic<bool> g_menuVisible{false};

HWND g_hudWnd = nullptr;
HWND g_menuWnd = nullptr;
std::wstring g_configFile;
HANDLE g_singleInstanceMutex = nullptr;
std::vector<MenuItem> g_menuItems;

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

std::wstring GetExeBaseName(DWORD pid) {
    std::wstring result;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return result;
    wchar_t buffer[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) {
        std::wstring full(buffer, size);
        size_t pos = full.find_last_of(L"\\/");
        result = (pos == std::wstring::npos) ? full : full.substr(pos + 1);
    }
    CloseHandle(hProcess);
    return ToLower(result);
}

bool IsGameForeground() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    if (fg == g_menuWnd) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (!pid) return false;
    std::wstring exe = GetExeBaseName(pid);
    return exe == L"hl.exe" || exe == L"cstrike.exe" || exe == L"cs.exe";
}

bool IsMenuCursor() {
    CURSORINFO ci{};
    ci.cbSize = sizeof(ci);
    if (!GetCursorInfo(&ci)) return false;
    if (!(ci.flags & CURSOR_SHOWING)) return false;
    HCURSOR arrow = LoadCursorW(nullptr, IDC_ARROW);
    HCURSOR ibeam = LoadCursorW(nullptr, IDC_IBEAM);
    return ci.hCursor == arrow || ci.hCursor == ibeam;
}

bool CanTriggerRecoil() {
    if (!IsGameForeground()) return false;
    if (IsMenuCursor()) return false;
    return true;
}

bool CanTriggerBhop() {
    // Only require CS to be the foreground game; don't block bhop due to cursor state
    return IsGameForeground();
}

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
        inputs[0].ki.wScan = scanWord;
        inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE | ext | KEYEVENTF_KEYUP;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wScan = scanWord;
        inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE | ext;
    } else {
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wScan = scanWord;
        inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE | ext;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wScan = scanWord;
        inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE | ext | KEYEVENTF_KEYUP;
    }
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

void UpdateHudWindow() {
    if (!g_hudWnd) return;
    bool showHud = g_hudEnabled.load() && IsGameForeground();
    if (showHud) {
        ShowWindow(g_hudWnd, SW_SHOWNOACTIVATE);
        SetWindowPos(g_hudWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        ShowWindow(g_hudWnd, SW_HIDE);
    }
    InvalidateRect(g_hudWnd, nullptr, FALSE);
}

void RefreshUI() {
    UpdateHudWindow();
    if (g_menuWnd) {
        InvalidateRect(g_menuWnd, nullptr, TRUE);
        if (g_menuVisible.load()) {
            SetWindowPos(g_menuWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
}

LRESULT CALLBACK HudWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH bg = CreateSolidBrush(RGB(18, 18, 18));
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);

            HPEN pen = CreatePen(PS_SOLID, 1, RGB(180, 0, 0));
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);

            SetBkMode(hdc, TRANSPARENT);
            HFONT font = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT oldFont = (HFONT)SelectObject(hdc, font);

            bool nrns = g_nrnsEnabled.load();
            bool bhop = g_bhopEnabled.load();
            int profile = g_currentProfile.load();

            RECT r1 = {0, 6, kHudWidth, 26};
            RECT r2 = {0, 26, kHudWidth, 46};
            RECT r3 = {0, 46, kHudWidth, 66};

            SetTextColor(hdc, nrns ? RGB(255, 60, 60) : RGB(120, 120, 120));
            DrawTextW(hdc, nrns ? L"NR/NS [ON]" : L"NR/NS [OFF]", -1, &r1, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

            wchar_t profileLine[64];
            wsprintfW(profileLine, L"[%s]", ProfileName(profile));
            SetTextColor(hdc, RGB(220, 220, 220));
            DrawTextW(hdc, profileLine, -1, &r2, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

            SetTextColor(hdc, bhop ? RGB(255, 60, 60) : RGB(120, 120, 120));
            DrawTextW(hdc, bhop ? L"Bhop [ON]" : L"Bhop [OFF]", -1, &r3, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

            SelectObject(hdc, oldFont);
            DeleteObject(font);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_APP_UPDATE_UI:
            UpdateHudWindow();
            return 0;
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
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

    MenuItem exitBtn; exitBtn.id = ID_EXIT; exitBtn.label = L"Fechar programa (F12)"; exitBtn.isCheckbox = false;
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
    RefreshUI();
}

void ChangeProfile(int delta) {
    int p = g_currentProfile.load() + delta;
    if (p < 1) p = 4;
    if (p > 4) p = 1;
    g_currentProfile.store(p);
    SaveSettings();
    RefreshUI();
}

void RequestExit();

LRESULT CALLBACK MenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            BuildMenuItems();
            return 0;
        case WM_NCHITTEST: {
            POINT pt{(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};
            ScreenToClient(hwnd, &pt);
            RECT rc{};
            GetClientRect(hwnd, &rc);
            // Allow dragging by the title bar area
            if (pt.y >= 0 && pt.y < 40 && pt.x >= 0 && pt.x < rc.right) {
                return HTCAPTION;
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);

            HBRUSH bg = CreateSolidBrush(RGB(12, 12, 12));
            FillRect(hdc, &rc, bg);
            DeleteObject(bg);

            HPEN border = CreatePen(PS_SOLID, 2, RGB(180, 0, 0));
            HGDIOBJ oldPen = SelectObject(hdc, border);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(border);

            RECT titleRc = {2, 2, rc.right - 2, 40};
            HBRUSH titleBg = CreateSolidBrush(RGB(40, 0, 0));
            FillRect(hdc, &titleRc, titleBg);
            DeleteObject(titleBg);

            SetBkMode(hdc, TRANSPARENT);
            HFONT titleFont = CreateFontW(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                          DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT uiFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT smallFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                          DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);
            SetTextColor(hdc, RGB(255, 50, 50));
            DrawTextW(hdc, L"CS 1.6 MOD MENU", -1, &titleRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

            SelectObject(hdc, uiFont);
            for (const auto& it : g_menuItems) {
                if (it.isCheckbox) {
                    bool checked = GetCheckboxState(it.id);
                    RECT box = {it.rect.left, it.rect.top + 3, it.rect.left + 18, it.rect.top + 21};
                    HPEN boxPen = CreatePen(PS_SOLID, 2, RGB(180, 0, 0));
                    HGDIOBJ op = SelectObject(hdc, boxPen);
                    HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    Rectangle(hdc, box.left, box.top, box.right, box.bottom);
                    if (checked) {
                        HBRUSH fill = CreateSolidBrush(RGB(220, 30, 30));
                        RECT inner = {box.left + 4, box.top + 4, box.right - 4, box.bottom - 4};
                        FillRect(hdc, &inner, fill);
                        DeleteObject(fill);
                    }
                    SelectObject(hdc, ob);
                    SelectObject(hdc, op);
                    DeleteObject(boxPen);

                    RECT textRc = {it.rect.left + 28, it.rect.top, it.rect.right, it.rect.bottom};
                    SetTextColor(hdc, checked ? RGB(240, 240, 240) : RGB(170, 170, 170));
                    DrawTextW(hdc, it.label.c_str(), -1, &textRc, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                } else if (it.isProfile) {
                    SetTextColor(hdc, RGB(200, 200, 200));
                    RECT labelRc = {it.rect.left, it.rect.top, it.rect.left + 120, it.rect.bottom};
                    DrawTextW(hdc, L"Perfil:", -1, &labelRc, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

                    for (const auto& b : g_menuItems) {
                        if (b.id == ID_PROFILE_PREV || b.id == ID_PROFILE_NEXT) {
                            HBRUSH bbg = CreateSolidBrush(RGB(50, 10, 10));
                            FillRect(hdc, &b.rect, bbg);
                            DeleteObject(bbg);
                            HPEN bpen = CreatePen(PS_SOLID, 1, RGB(180, 0, 0));
                            HGDIOBJ op2 = SelectObject(hdc, bpen);
                            HGDIOBJ ob2 = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                            Rectangle(hdc, b.rect.left, b.rect.top, b.rect.right, b.rect.bottom);
                            SelectObject(hdc, ob2); SelectObject(hdc, op2); DeleteObject(bpen);
                            SetTextColor(hdc, RGB(255, 80, 80));
                            RECT br = b.rect;
                            DrawTextW(hdc, b.label.c_str(), -1, &br, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                        }
                    }
                    RECT nameRc = {202, it.rect.top, 280, it.rect.bottom};
                    SetTextColor(hdc, RGB(255, 70, 70));
                    DrawTextW(hdc, ProfileName(g_currentProfile.load()), -1, &nameRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                } else if (it.id == ID_EXIT) {
                    HBRUSH bbg = CreateSolidBrush(RGB(60, 0, 0));
                    FillRect(hdc, &it.rect, bbg);
                    DeleteObject(bbg);
                    HPEN bpen = CreatePen(PS_SOLID, 1, RGB(200, 0, 0));
                    HGDIOBJ op3 = SelectObject(hdc, bpen);
                    HGDIOBJ ob3 = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    Rectangle(hdc, it.rect.left, it.rect.top, it.rect.right, it.rect.bottom);
                    SelectObject(hdc, ob3); SelectObject(hdc, op3); DeleteObject(bpen);
                    SetTextColor(hdc, RGB(255, 200, 200));
                    RECT er = it.rect;
                    DrawTextW(hdc, it.label.c_str(), -1, &er, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                }
            }

            SelectObject(hdc, smallFont);
            SetTextColor(hdc, RGB(140, 140, 140));
            RECT hint = {10, kMenuHeight - 28, kMenuWidth - 10, kMenuHeight - 6};
            DrawTextW(hdc, L"Insert: abrir/fechar menu  |  Arraste pela barra vermelha", -1, &hint, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

            SelectObject(hdc, oldFont);
            DeleteObject(titleFont);
            DeleteObject(uiFont);
            DeleteObject(smallFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT pt{(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};
            for (const auto& it : g_menuItems) {
                if (PtInRect(&it.rect, pt)) {
                    if (it.isCheckbox) {
                        ToggleCheckbox(it.id);
                    } else if (it.id == ID_PROFILE_PREV) {
                        ChangeProfile(-1);
                    } else if (it.id == ID_PROFILE_NEXT) {
                        ChangeProfile(1);
                    } else if (it.id == ID_EXIT) {
                        RequestExit();
                    }
                    break;
                }
            }
            return 0;
        }
        case WM_APP_UPDATE_UI:
            if (g_menuVisible.load()) {
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_menuWnd = nullptr;
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

HWND CreateHudWindow(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HudWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kHudClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    int x = GetSystemMetrics(SM_CXSCREEN) - kHudWidth - 30;
    int y = GetSystemMetrics(SM_CYSCREEN) - kHudHeight - 120;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        kHudClassName, L"CS 1.6 HUD",
        WS_POPUP, x, y, kHudWidth, kHudHeight,
        nullptr, nullptr, hInst, nullptr);
    if (hwnd) {
        SetLayeredWindowAttributes(hwnd, 0, 220, LWA_ALPHA);
        g_hudWnd = hwnd;
        UpdateHudWindow();
    }
    return hwnd;
}

HWND CreateMenuWindow(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MenuWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kMenuClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    int x = (sx - kMenuWidth) / 2;
    int y = (sy - kMenuHeight) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kMenuClassName, L"CS 1.6 Mod Menu",
        WS_POPUP, x, y, kMenuWidth, kMenuHeight,
        nullptr, nullptr, hInst, nullptr);
    g_menuWnd = hwnd;
    return hwnd;
}

void ToggleMenu() {
    if (!g_menuWnd) return;
    bool vis = !g_menuVisible.load();
    g_menuVisible.store(vis);
    if (vis) {
        ShowWindow(g_menuWnd, SW_SHOW);
        SetWindowPos(g_menuWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetForegroundWindow(g_menuWnd);
        InvalidateRect(g_menuWnd, nullptr, TRUE);
    } else {
        ShowWindow(g_menuWnd, SW_HIDE);
    }
}

void RequestExit() {
    SaveSettings();
    g_running.store(false);
    PostQuitMessage(0);
}

void WorkerLoop() {
    timeBeginPeriod(1);

    // Elevate scheduling for lower-latency timing (best effort)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    DWORD mmcssTaskIndex = 0;
    HANDLE mmcssHandle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &mmcssTaskIndex);
    if (!mmcssHandle) {
        mmcssHandle = AvSetMmThreadCharacteristicsW(L"Games", &mmcssTaskIndex);
    }
    if (mmcssHandle) {
        AvSetMmThreadPriority(mmcssHandle, AVRT_PRIORITY_HIGH);
    }

    using clock = std::chrono::steady_clock;
    auto nextRecoil = clock::now();
    auto nextBhop = clock::now();
    bool bhopRepressPending = false;
    auto bhopRepressDue = clock::now();
    auto lastTopmostRefresh = clock::now();
    bool lWasDown = false;
    bool aimActive = false;
    int shotStep = 0;

    auto sendSpaceScancode = [](DWORD flags) {
        UINT scan = MapVirtualKeyW(VK_SPACE, MAPVK_VK_TO_VSC);
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wScan = static_cast<WORD>(scan);
        in.ki.dwFlags = KEYEVENTF_SCANCODE | flags;
        SendInput(1, &in, sizeof(in));
    };

    HANDLE hTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!hTimer) {
        hTimer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }

    auto wait_until = [&](clock::time_point target) {
        for (;;) {
            auto now = clock::now();
            if (now >= target) return;
            auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(target - now).count();
            if (remaining <= 200) {
                // short spin for final precision
                std::this_thread::yield();
                continue;
            }
            if (hTimer && remaining > 1500) {
                // sleep most of the remaining time, leave margin for spin
                LONGLONG due100ns = -((remaining - 800) * 10); // relative, 100ns units
                LARGE_INTEGER due{};
                due.QuadPart = due100ns;
                if (SetWaitableTimer(hTimer, &due, 0, nullptr, nullptr, FALSE)) {
                    WaitForSingleObject(hTimer, INFINITE);
                    continue;
                }
            }
            if (remaining > 2000) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        }
    };

    while (g_running.load(std::memory_order_relaxed)) {
        auto now = clock::now();
        bool canRecoil = CanTriggerRecoil();
        bool canBhop = CanTriggerBhop();

        // Periodically reassert topmost/refresh overlay visibility (helps in some game modes)
        if (now - lastTopmostRefresh >= std::chrono::milliseconds(500)) {
            lastTopmostRefresh = now;
            if (g_hudWnd) {
                PostMessageW(g_hudWnd, WM_APP_UPDATE_UI, 0, 0);
            }
            if (g_menuWnd && g_menuVisible.load()) {
                PostMessageW(g_menuWnd, WM_APP_UPDATE_UI, 0, 0);
            }
        }

        bool nrnsOn = g_nrnsEnabled.load();
        SHORT lState = GetAsyncKeyState(VK_LBUTTON);
        bool lDown = (lState & 0x8000) != 0;

        if (nrnsOn && lDown && !lWasDown && canRecoil) {
            aimActive = true;
            shotStep = 0;
            nextRecoil = now;
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
            } else if (now >= nextRecoil) {
                ++shotStep;
                Vec2 mv = GetRecoilStep(g_currentProfile.load(), shotStep);
                SendMouseMove(mv.dx, mv.dy);
                if (g_useScreenShake.load()) {
                    int rx = (shotStep % 3) - 1;
                    int ry = ((shotStep / 2) % 3) - 1;
                    SendMouseMove(rx, ry);
                }
                int delay = std::clamp(g_recoilDelayMs.load(), 1, 500);
                nextRecoil += std::chrono::milliseconds(delay);
                if (nextRecoil < now - std::chrono::milliseconds(delay)) {
                    nextRecoil = now + std::chrono::milliseconds(delay);
                }
            }
        }

        bool bhopOn = g_bhopEnabled.load() && canBhop;
        SHORT spaceState = GetAsyncKeyState(VK_SPACE);
        bool spaceDown = (spaceState & 0x8000) != 0;
        if (bhopOn && spaceDown) {
            int delay = std::clamp(g_bhopDelayMs.load(), 1, 500);
            int gapMs = std::clamp(delay / 4, 4, 10);
            if (bhopRepressPending) {
                if (now >= bhopRepressDue) {
                    sendSpaceScancode(0); // repress after brief release gap
                    bhopRepressPending = false;
                    nextBhop = now;
                }
            } else if (now >= nextBhop) {
                sendSpaceScancode(KEYEVENTF_KEYUP); // brief release for jump edge
                bhopRepressPending = true;
                bhopRepressDue = now + std::chrono::milliseconds(gapMs);
            }
        } else {
            if (bhopRepressPending) {
                // Only restore press if space is still physically held (e.g. menu opened / bhop disabled).
                // If user released space, leave it released to avoid unintended jump.
                if (spaceDown) sendSpaceScancode(0);
                bhopRepressPending = false;
            }
            nextBhop = clock::now();
        }

        // Determine next wake time for precise scheduling
        clock::time_point nextWake = clock::now() + std::chrono::milliseconds(2);
        bool hasWork = false;
        if (aimActive) {
            nextWake = nextRecoil;
            hasWork = true;
        }
        if (bhopOn && spaceDown) {
            auto bhopWake = bhopRepressPending ? bhopRepressDue : nextBhop;
            nextWake = hasWork ? std::min(nextWake, bhopWake) : bhopWake;
            hasWork = true;
        }
        if (!hasWork) {
            // idle: low CPU, still responsive
            nextWake = clock::now() + std::chrono::milliseconds(2);
        }

        auto wakeCap = clock::now() + std::chrono::milliseconds(2);
        if (nextWake > wakeCap) nextWake = wakeCap;
        wait_until(nextWake);
    }

    if (hTimer) CloseHandle(hTimer);
    if (mmcssHandle) AvRevertMmThreadCharacteristics(mmcssHandle);
    timeEndPeriod(1);
}

} // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    g_singleInstanceMutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (g_singleInstanceMutex) CloseHandle(g_singleInstanceMutex);
        return 0;
    }

    LoadSettings();

    if (!CreateHudWindow(hInstance)) {
        if (g_singleInstanceMutex) CloseHandle(g_singleInstanceMutex);
        return 1;
    }
    if (!CreateMenuWindow(hInstance)) {
        if (g_singleInstanceMutex) CloseHandle(g_singleInstanceMutex);
        return 1;
    }

    RegisterHotKey(nullptr, HOTKEY_MENU, MOD_NOREPEAT, VK_INSERT);
    RegisterHotKey(nullptr, HOTKEY_EXIT, MOD_NOREPEAT, VK_F12);

    Beep(800, 120);
    std::thread worker(WorkerLoop);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_HOTKEY) {
            switch (msg.wParam) {
                case HOTKEY_MENU: ToggleMenu(); break;
                case HOTKEY_EXIT: RequestExit(); break;
                default: break;
            }
        } else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    g_running.store(false);
    if (worker.joinable()) worker.join();

    UnregisterHotKey(nullptr, HOTKEY_MENU);
    UnregisterHotKey(nullptr, HOTKEY_EXIT);
    SaveSettings();

    if (g_singleInstanceMutex) {
        ReleaseMutex(g_singleInstanceMutex);
        CloseHandle(g_singleInstanceMutex);
    }
    return 0;
}
