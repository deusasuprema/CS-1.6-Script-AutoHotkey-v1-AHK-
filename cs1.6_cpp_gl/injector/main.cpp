#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <cwctype>
#include <string>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

namespace {

std::wstring ToLower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}

bool IsTargetProcessName(const std::wstring& exeName) {
    std::wstring n = ToLower(exeName);
    return n == L"hl.exe" || n == L"cstrike.exe" || n == L"cs.exe";
}

DWORD FindTargetPid() {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (IsTargetProcessName(pe.szExeFile)) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

std::wstring GetSelfDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path);
    size_t pos = p.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
}

bool InjectDll(DWORD pid, const std::wstring& dllPath) {
    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) return false;

    SIZE_T bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(hProc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { CloseHandle(hProc); return false; }

    bool ok = false;
    if (WriteProcessMemory(hProc, remote, dllPath.c_str(), bytes, nullptr)) {
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        auto loadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(k32, "LoadLibraryW");
        if (loadLib) {
            HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, loadLib, remote, 0, nullptr);
            if (hThread) {
                WaitForSingleObject(hThread, 5000);
                DWORD exitCode = 0;
                GetExitCodeThread(hThread, &exitCode);
                ok = (exitCode != 0);
                CloseHandle(hThread);
            }
        }
    }

    VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return ok;
}

bool IsModuleLoaded(DWORD pid, const std::wstring& moduleNameLower) {
    bool loaded = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{}; me.dwSize = sizeof(me);
    if (Module32FirstW(snap, &me)) {
        do {
            if (ToLower(me.szModule) == moduleNameLower) { loaded = true; break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return loaded;
}

} // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    std::wstring dllPath = GetSelfDir() + L"\\cs16_overlay.dll";

    // Check DLL exists
    DWORD attr = GetFileAttributesW(dllPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(nullptr, L"cs16_overlay.dll nao encontrado na mesma pasta do injetor.", L"CS 1.6 Overlay Injector", MB_ICONERROR | MB_OK);
        return 1;
    }

    // Wait for game process (up to ~60s)
    DWORD pid = 0;
    for (int i = 0; i < 120; ++i) {
        pid = FindTargetPid();
        if (pid) break;
        Sleep(500);
    }
    if (!pid) {
        MessageBoxW(nullptr, L"Jogo (hl.exe/cstrike.exe/cs.exe) nao encontrado. Abra o CS 1.6 e tente novamente.", L"CS 1.6 Overlay Injector", MB_ICONWARNING | MB_OK);
        return 2;
    }

    // Avoid double inject
    if (IsModuleLoaded(pid, L"cs16_overlay.dll")) {
        return 0;
    }

    if (!InjectDll(pid, dllPath)) {
        MessageBoxW(nullptr, L"Falha ao injetar cs16_overlay.dll. Tente executar como Administrador e verifique se o CS e o injetor sao da mesma arquitetura (32-bit).", L"CS 1.6 Overlay Injector", MB_ICONERROR | MB_OK);
        return 3;
    }

    return 0;
}
