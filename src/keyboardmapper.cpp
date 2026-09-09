#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comdlg32.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#define SINGLE_INSTANCE_MUTEX_NAME L"Local\\KeyboardMapper_Final_Mutex"
#define WINDOW_CLASS_NAME          L"KeyboardMapperFinalClass"
#define REG_RUN_KEY                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define APP_REG_NAME               L"Keyboard Mapper"
#define DEFAULT_AUTOSAVE_FILE      L"default_mappings.kmp"

using fnSetPreferredAppMode = int (WINAPI*)(int);
using fnFlushMenuThemes = void (WINAPI*)();

// ID Kontrol
#define IDC_BTN_PICK_SRC   101
#define IDC_BTN_PICK_DST   102
#define IDC_BTN_ADD_MAP    103
#define IDC_BTN_DEL_MAP    104
#define IDC_BTN_TOGGLE     105
#define IDC_LIST_MAPPINGS  106
#define IDC_STATUS_TEXT    107
#define IDC_LBL_SRC        108
#define IDC_LBL_DST        109
#define IDC_LBL_LIST       110

// ID Menu Bar
#define IDM_LANG_ID        201
#define IDM_LANG_EN        202
#define IDM_THEME_SYSTEM   203
#define IDM_THEME_DARK     204
#define IDM_THEME_LIGHT    205
#define IDM_PRESET_SAVE    206
#define IDM_PRESET_LOAD    207
#define IDM_OPT_STARTUP    208
#define IDM_APP_EXIT       209

// ID Tray Icon
#define IDM_TRAY_SHOW      301
#define IDM_TRAY_TOGGLE    302
#define IDM_TRAY_EXIT      303

#define WM_KEY_RECORDED    (WM_USER + 1)
#define WM_TRAYICON        (WM_USER + 2)
#define ID_TRAY_ICON       1001

enum Language { LANG_ID = 0, LANG_EN = 1 };
enum ThemeChoice { THEME_CHOICE_SYSTEM = 0, THEME_CHOICE_DARK = 1, THEME_CHOICE_LIGHT = 2 };
enum VisualTheme { VISUAL_DARK = 0, VISUAL_LIGHT = 1 };
enum RecordMode { REC_NONE, REC_SOURCE, REC_TARGET };

struct ThemeColor {
    COLORREF bg;
    COLORREF cardBg;
    COLORREF cardBorder;
    COLORREF textPrimary;
    COLORREF textMuted;
    COLORREF accentBlue;
    COLORREF accentHover;
    COLORREF recording;
    COLORREF success;
    COLORREF danger;
    COLORREF badgeBg;
    COLORREF badgeBorder;
    COLORREF deleteBtnBg;
    COLORREF deleteBtnBorder;
    COLORREF deleteBtnText;
};

const ThemeColor PALETTE[2] = {
    // DARK THEME
    {
        RGB(20, 20, 24), RGB(32, 32, 38), RGB(54, 54, 64),
        RGB(244, 244, 246), RGB(156, 163, 175), RGB(59, 130, 246),
        RGB(96, 165, 250), RGB(217, 119, 6), RGB(16, 185, 129),
        RGB(239, 68, 68), RGB(45, 45, 54), RGB(65, 65, 78),
        RGB(45, 25, 25), RGB(239, 68, 68), RGB(248, 113, 113)
    },
    // LIGHT THEME
    {
        RGB(243, 244, 246), RGB(255, 255, 255), RGB(209, 213, 219),
        RGB(17, 24, 39), RGB(107, 114, 128), RGB(37, 99, 235),
        RGB(29, 78, 216), RGB(217, 119, 6), RGB(5, 150, 105),
        RGB(220, 38, 38), RGB(243, 244, 246), RGB(209, 213, 219),
        RGB(254, 242, 242), RGB(239, 68, 68), RGB(220, 38, 38)
    }
};

// Global State
HWND g_hWndMain = NULL;
HHOOK g_hHook = NULL;
RecordMode g_recMode = REC_NONE;
bool g_isMappingActive = false;
Language g_currentLang = LANG_EN; // DEFAULT: BAHASA INGGRIS
ThemeChoice g_themeChoice = THEME_CHOICE_SYSTEM;
VisualTheme g_activeVisual = VISUAL_DARK;
NOTIFYICONDATAW g_nid = { 0 };

DWORD g_srcVk = 0, g_dstVk = 0;
std::wstring g_srcRawName = L"";
std::wstring g_dstRawName = L"";

struct MapEntry {
    DWORD dstVk;
    std::wstring srcName;
    std::wstring dstName;
};
std::unordered_map<DWORD, MapEntry> g_mapList;
std::vector<DWORD> g_mapKeysOrder;

HFONT hFontRegular = NULL, hFontBold = NULL, hFontSmall = NULL, hFontKeyBadge = NULL;
HBRUSH hBrushBg = NULL, hBrushCard = NULL;

HWND hLblSrc, hLblDst, hLblList;
HWND hBtnPickSrc, hBtnPickDst, hBtnAdd, hBtnDel, hBtnToggle, hListMap, hStatus;

struct LocalizedStrings {
    const wchar_t* winTitle;
    const wchar_t* menuOptions;
    const wchar_t* menuLang;
    const wchar_t* menuTheme;
    const wchar_t* menuPresetSave;
    const wchar_t* menuPresetLoad;
    const wchar_t* menuExit;
    const wchar_t* optStartup;
    const wchar_t* themeSystem;
    const wchar_t* themeDark;
    const wchar_t* themeLight;
    const wchar_t* lblSrc;
    const wchar_t* lblDst;
    const wchar_t* lblList;
    const wchar_t* btnPickSrcDefault;
    const wchar_t* btnPickDstDefault;
    const wchar_t* btnPressAnyKey;
    const wchar_t* btnAdd;
    const wchar_t* btnDel;
    const wchar_t* btnStart;
    const wchar_t* btnStop;
    const wchar_t* statusIdle;
    const wchar_t* statusActive;
    const wchar_t* msgEmpty;
    const wchar_t* msgSameKey;
    const wchar_t* msgTitleInfo;
    const wchar_t* msgTitleWarn;
    const wchar_t* msgSaved;
    const wchar_t* msgLoaded;
    const wchar_t* msgSaveEmpty;
    const wchar_t* trayShow;
    const wchar_t* trayHide;
    const wchar_t* trayExit;
    const wchar_t* trayTip;
} g_i18n[2] = {
    // ID (Index 0)
    {
        L"Keyboard Mapper",
        L"Opsi",
        L"Bahasa / Language",
        L"Tema / Theme",
        L"Simpan Preset ke File... (Export)",
        L"Impor Preset dari File... (Import)",
        L"Keluar Aplikasi",
        L"Jalankan Otomatis saat Startup Windows",
        L"Ikuti Sistem (Otomatis)",
        L"Mode Gelap (Dark)",
        L"Mode Terang (Light)",
        L"Tombol Asal (Fisik):",
        L"Ganti Menjadi:",
        L"Daftar Mapping Aktif:",
        L"Klik & Tekan Tombol",
        L"Klik & Tekan Tombol",
        L"Tekan Tombol Apapun...",
        L"+ Simpan",
        L"Hapus Terpilih",
        L"MULAI (AKTIFKAN MAPPING)",
        L"BERHENTI (NONAKTIFKAN)",
        L"Status: Siap (Mapping belum berjalan)",
        L"Status: AKTIF (Mapping tombol berjalan di background)",
        L"Silakan tentukan tombol Asal dan Pengganti terlebih dahulu!",
        L"Tombol asal dan pengganti tidak boleh sama!",
        L"Pemberitahuan",
        L"Peringatan",
        L"Preset berhasil disimpan ke file!",
        L"Preset berhasil dimuat!",
        L"Tidak ada tombol yang terdaftar untuk disimpan!",
        L"Buka Antarmuka",
        L"Sembunyikan ke Tray",
        L"Keluar Aplikasi",
        L"Keyboard Mapper"
    },
    // EN (Index 1)
    {
        L"Keyboard Mapper",
        L"Options",
        L"Language / Bahasa",
        L"Theme / Tema",
        L"Save Preset to File... (Export)",
        L"Import Preset from File... (Import)",
        L"Exit Application",
        L"Run Automatically on Windows Startup",
        L"Follow System (Auto)",
        L"Dark Mode",
        L"Light Mode",
        L"Source Key (Physical):",
        L"Remap To:",
        L"Active Mappings List:",
        L"Click & Press Key",
        L"Click & Press Key",
        L"Press Any Key...",
        L"+ Save",
        L"Delete Selected",
        L"START (ENABLE MAPPING)",
        L"STOP (DISABLE MAPPING)",
        L"Status: Ready (Mapping is inactive)",
        L"Status: ACTIVE (Mapping is running in background)",
        L"Please record both Source and Target keys first!",
        L"Source and Target keys cannot be identical!",
        L"Notification",
        L"Warning",
        L"Preset successfully saved to file!",
        L"Preset successfully imported!",
        L"No mappings available to save!",
        L"Open Interface",
        L"Hide to Tray",
        L"Exit Application",
        L"Keyboard Mapper"
    }
};

std::string WStringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size, NULL, NULL);
    return str;
}

std::wstring Utf8ToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    if (size <= 0) return L"";
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size);
    return wstr;
}

void RefreshMappingList();

bool SaveMappingsToFile(const std::wstring& filePath) {
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    std::string content = "";
    for (const auto& item : g_mapList) {
        content += std::to_string(item.first) + "|"
                 + std::to_string(item.second.dstVk) + "|"
                 + WStringToUtf8(item.second.srcName) + "|"
                 + WStringToUtf8(item.second.dstName) + "\r\n";
    }

    DWORD written = 0;
    BOOL res = WriteFile(hFile, content.c_str(), (DWORD)content.size(), &written, NULL);
    CloseHandle(hFile);
    return (res != FALSE);
}

bool LoadMappingsFromFile(const std::wstring& filePath) {
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE || fileSize == 0) {
        CloseHandle(hFile);
        return false;
    }

    std::string content(fileSize, 0);
    DWORD bytesRead = 0;
    ReadFile(hFile, &content[0], fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    g_mapList.clear();
    std::stringstream ss(content);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::stringstream lineStream(line);
        std::string sSrcVk, sDstVk, srcNameUtf8, dstNameUtf8;

        if (std::getline(lineStream, sSrcVk, '|') &&
            std::getline(lineStream, sDstVk, '|') &&
            std::getline(lineStream, srcNameUtf8, '|') &&
            std::getline(lineStream, dstNameUtf8)) {
            try {
                DWORD srcVk = (DWORD)std::stoul(sSrcVk);
                DWORD dstVk = (DWORD)std::stoul(sDstVk);
                g_mapList[srcVk] = { dstVk, Utf8ToWString(srcNameUtf8), Utf8ToWString(dstNameUtf8) };
            } catch (...) {}
        }
    }
    RefreshMappingList();
    return true;
}

std::wstring GetDefaultConfigPath() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring path(exePath);
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return path.substr(0, lastSlash + 1) + DEFAULT_AUTOSAVE_FILE;
    }
    return DEFAULT_AUTOSAVE_FILE;
}

bool IsRunAtStartupEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0;
        LONG res = RegQueryValueExW(hKey, APP_REG_NAME, NULL, &type, NULL, NULL);
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS);
    }
    return false;
}

bool SetRunAtStartup(bool enable) {
    HKEY hKey;
    DWORD disp;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, NULL, &hKey, &disp) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            std::wstring cmd = L"\"" + std::wstring(exePath) + L"\" --startup";
            RegSetValueExW(hKey, APP_REG_NAME, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, APP_REG_NAME);
        }
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

bool IsWindowsSystemInDarkMode() {
    DWORD val = 0;
    DWORD size = sizeof(DWORD);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return val == 0;
        }
        RegCloseKey(hKey);
    }
    return true;
}

VisualTheme GetCurrentEffectiveTheme() {
    if (g_themeChoice == THEME_CHOICE_SYSTEM) {
        return IsWindowsSystemInDarkMode() ? VISUAL_DARK : VISUAL_LIGHT;
    }
    return (g_themeChoice == THEME_CHOICE_DARK) ? VISUAL_DARK : VISUAL_LIGHT;
}

void EnableUxDarkMenu(bool isDark) {
    HMODULE hUxtheme = LoadLibraryW(L"uxtheme.dll");
    if (hUxtheme) {
        fnSetPreferredAppMode SetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
        fnFlushMenuThemes FlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136));
        if (SetPreferredAppMode) SetPreferredAppMode(isDark ? 2 : 0);
        if (FlushMenuThemes) FlushMenuThemes();
        FreeLibrary(hUxtheme);
    }
}

std::wstring GetKeyDisplayName(DWORD vkCode, DWORD scanCode, bool isExtended) {
    LONG lParam = (scanCode << 16);
    if (isExtended) lParam |= (1 << 24);

    wchar_t keyName[128] = { 0 };
    if (GetKeyNameTextW(lParam, keyName, 128) > 0) {
        return std::wstring(keyName);
    }

    switch (vkCode) {
    case VK_LSHIFT:   return L"Left Shift";
    case VK_RSHIFT:   return L"Right Shift";
    case VK_LCONTROL: return L"Left Ctrl";
    case VK_RCONTROL: return L"Right Ctrl";
    case VK_LMENU:    return L"Left Alt";
    case VK_RMENU:    return L"Right Alt";
    case VK_LWIN:     return L"Left Win";
    case VK_RWIN:     return L"Right Win";
    default: {
        wchar_t fallback[32];
        swprintf_s(fallback, L"Key 0x%02X", vkCode);
        return fallback;
    }
    }
}

void SendSimulatedKey(DWORD vkCode, bool isKeyUp) {
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(vkCode);
    if (isKeyUp) {
        input.ki.dwFlags = KEYEVENTF_KEYUP;
    }
    SendInput(1, &input, sizeof(INPUT));
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKey = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        if (g_recMode != REC_NONE && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
            PostMessage(g_hWndMain, WM_KEY_RECORDED, pKey->vkCode, (pKey->scanCode & 0xFF) | ((pKey->flags & LLKHF_EXTENDED) ? 0x100 : 0));
            return 1;
        }

        if (g_isMappingActive && !(pKey->flags & LLKHF_INJECTED)) {
            auto it = g_mapList.find(pKey->vkCode);
            if (it != g_mapList.end()) {
                bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
                SendSimulatedKey(it->second.dstVk, isKeyUp);
                return 1;
            }
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

void DrawModernButton(HDC hdc, RECT rc, COLORREF bgCol, COLORREF borderCol, COLORREF textCol, const wchar_t* text, HFONT font, bool isPressed) {
    COLORREF actualBg = isPressed ? RGB(GetRValue(bgCol) * 0.85, GetGValue(bgCol) * 0.85, GetBValue(bgCol) * 0.85) : bgCol;

    HBRUSH hBr = CreateSolidBrush(actualBg);
    HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
    HGDIOBJ oldBr = SelectObject(hdc, hBr);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textCol);
    SelectObject(hdc, font);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void RefreshMappingList() {
    SendMessage(hListMap, LB_RESETCONTENT, 0, 0);
    g_mapKeysOrder.clear();
    for (const auto& item : g_mapList) {
        g_mapKeysOrder.push_back(item.first);
        SendMessageW(hListMap, LB_ADDSTRING, 0, (LPARAM)L"");
    }
}

void BuildUnifiedMenuBar(HWND hwnd) {
    HMENU oldMenu = GetMenu(hwnd);

    HMENU hNewMenuBar  = CreateMenu();
    HMENU hOptionsMenu = CreatePopupMenu();
    HMENU hLangMenu    = CreatePopupMenu();
    HMENU hThemeMenu   = CreatePopupMenu();

    const auto& str = g_i18n[g_currentLang];

    // Submenu Language
    AppendMenuW(hLangMenu, MF_STRING | (g_currentLang == LANG_ID ? MF_CHECKED : MF_UNCHECKED), IDM_LANG_ID, L"Bahasa Indonesia");
    AppendMenuW(hLangMenu, MF_STRING | (g_currentLang == LANG_EN ? MF_CHECKED : MF_UNCHECKED), IDM_LANG_EN, L"English");
    AppendMenuW(hOptionsMenu, MF_POPUP, (UINT_PTR)hLangMenu, str.menuLang);

    // Submenu Theme
    AppendMenuW(hThemeMenu, MF_STRING | (g_themeChoice == THEME_CHOICE_SYSTEM ? MF_CHECKED : MF_UNCHECKED), IDM_THEME_SYSTEM, str.themeSystem);
    AppendMenuW(hThemeMenu, MF_STRING | (g_themeChoice == THEME_CHOICE_DARK   ? MF_CHECKED : MF_UNCHECKED), IDM_THEME_DARK,   str.themeDark);
    AppendMenuW(hThemeMenu, MF_STRING | (g_themeChoice == THEME_CHOICE_LIGHT  ? MF_CHECKED : MF_UNCHECKED), IDM_THEME_LIGHT,  str.themeLight);
    AppendMenuW(hOptionsMenu, MF_POPUP, (UINT_PTR)hThemeMenu, str.menuTheme);

    AppendMenuW(hOptionsMenu, MF_SEPARATOR, 0, NULL);

    // Preset Options
    AppendMenuW(hOptionsMenu, MF_STRING, IDM_PRESET_SAVE, str.menuPresetSave);
    AppendMenuW(hOptionsMenu, MF_STRING, IDM_PRESET_LOAD, str.menuPresetLoad);

    AppendMenuW(hOptionsMenu, MF_SEPARATOR, 0, NULL);

    // Startup
    AppendMenuW(hOptionsMenu, MF_STRING | (IsRunAtStartupEnabled() ? MF_CHECKED : MF_UNCHECKED), IDM_OPT_STARTUP, str.optStartup);

    AppendMenuW(hOptionsMenu, MF_SEPARATOR, 0, NULL);

    // Exit
    AppendMenuW(hOptionsMenu, MF_STRING, IDM_APP_EXIT, str.menuExit);

    AppendMenuW(hNewMenuBar, MF_POPUP, (UINT_PTR)hOptionsMenu, str.menuOptions);

    SetMenu(hwnd, hNewMenuBar);
    if (oldMenu) DestroyMenu(oldMenu);
    DrawMenuBar(hwnd);
}

void ApplyThemeAndLanguage(HWND hwnd) {
    g_activeVisual = GetCurrentEffectiveTheme();
    const auto& c = PALETTE[g_activeVisual];
    const auto& str = g_i18n[g_currentLang];

    BOOL dark = (g_activeVisual == VISUAL_DARK);
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    EnableUxDarkMenu(dark);

    if (hBrushBg) DeleteObject(hBrushBg);
    if (hBrushCard) DeleteObject(hBrushCard);
    hBrushBg = CreateSolidBrush(c.bg);
    hBrushCard = CreateSolidBrush(c.cardBg);

    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrushBg);

    SetWindowTextW(hwnd, str.winTitle);

    BuildUnifiedMenuBar(hwnd);

    SetWindowTextW(hLblSrc, str.lblSrc);
    SetWindowTextW(hLblDst, str.lblDst);
    SetWindowTextW(hLblList, str.lblList);
    SetWindowTextW(hStatus, g_isMappingActive ? str.statusActive : str.statusIdle);

    wcscpy_s(g_nid.szTip, str.trayTip);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);

    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void ShowTrayContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    const auto& str = g_i18n[g_currentLang];

    bool isVisible = IsWindowVisible(hwnd) && !IsIconic(hwnd);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHOW, isVisible ? str.trayHide : str.trayShow);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_TOGGLE, g_isMappingActive ? str.btnStop : str.btnStart);
    AppendMenuW(hMenu, MF_STRING | (IsRunAtStartupEnabled() ? MF_CHECKED : MF_UNCHECKED), IDM_OPT_STARTUP, str.optStartup);
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, str.trayExit);

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        hFontRegular   = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hFontBold      = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hFontSmall     = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hFontKeyBadge  = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        hLblSrc  = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 30, 20, 240, 20, hwnd, (HMENU)IDC_LBL_SRC, NULL, NULL);
        hLblDst  = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 310, 20, 240, 20, hwnd, (HMENU)IDC_LBL_DST, NULL, NULL);
        hLblList = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD, 30, 155, 240, 20, hwnd, (HMENU)IDC_LBL_LIST, NULL, NULL);

        hBtnPickSrc = CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 30, 44, 240, 46, hwnd, (HMENU)IDC_BTN_PICK_SRC, NULL, NULL);
        hBtnPickDst = CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 310, 44, 240, 46, hwnd, (HMENU)IDC_BTN_PICK_DST, NULL, NULL);
        hBtnAdd     = CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 30, 102, 520, 38, hwnd, (HMENU)IDC_BTN_ADD_MAP, NULL, NULL);

        hListMap    = CreateWindowW(L"LISTBOX", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED, 30, 180, 380, 190, hwnd, (HMENU)IDC_LIST_MAPPINGS, NULL, NULL);
        hBtnDel     = CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 425, 180, 125, 42, hwnd, (HMENU)IDC_BTN_DEL_MAP, NULL, NULL);

        hBtnToggle  = CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 30, 390, 520, 48, hwnd, (HMENU)IDC_BTN_TOGGLE, NULL, NULL);
        hStatus     = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD | SS_CENTER, 30, 450, 520, 22, hwnd, (HMENU)IDC_STATUS_TEXT, NULL, NULL);

        SendMessage(hLblSrc, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        SendMessage(hLblDst, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        SendMessage(hLblList, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        SendMessage(hStatus, WM_SETFONT, (WPARAM)hFontSmall, TRUE);
        SendMessage(hListMap, LB_SETITEMHEIGHT, 0, 40);

        g_nid.cbSize = sizeof(NOTIFYICONDATAW);
        g_nid.hWnd = hwnd;
        g_nid.uID = ID_TRAY_ICON;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wcscpy_s(g_nid.szTip, g_i18n[g_currentLang].trayTip);
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        LoadMappingsFromFile(GetDefaultConfigPath());

        ApplyThemeAndLanguage(hwnd);
        break;
    }

    case WM_SETTINGCHANGE: {
        if (g_themeChoice == THEME_CHOICE_SYSTEM) {
            ApplyThemeAndLanguage(hwnd);
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        const auto& c = PALETTE[g_activeVisual];
        SetBkMode(hdcStatic, TRANSPARENT);

        if (hCtrl == hStatus) {
            SetTextColor(hdcStatic, g_isMappingActive ? c.success : c.textMuted);
        } else {
            SetTextColor(hdcStatic, c.textPrimary);
        }
        return (INT_PTR)hBrushBg;
    }

    case WM_CTLCOLORLISTBOX: {
        HDC hdcListBox = (HDC)wParam;
        const auto& c = PALETTE[g_activeVisual];
        SetBkColor(hdcListBox, c.cardBg);
        return (INT_PTR)hBrushCard;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* pDIS = (DRAWITEMSTRUCT*)lParam;
        const auto& c = PALETTE[g_activeVisual];
        const auto& str = g_i18n[g_currentLang];
        bool isDown = (pDIS->itemState & ODS_SELECTED);

        if (pDIS->CtlID == IDC_BTN_PICK_SRC) {
            bool isRec = (g_recMode == REC_SOURCE);
            COLORREF bg = isRec ? c.recording : c.cardBg;
            COLORREF border = isRec ? c.recording : (g_srcVk ? c.accentBlue : c.cardBorder);
            std::wstring txt = isRec ? str.btnPressAnyKey : (g_srcVk ? g_srcRawName : str.btnPickSrcDefault);
            DrawModernButton(pDIS->hDC, pDIS->rcItem, bg, border, c.textPrimary, txt.c_str(), g_srcVk ? hFontKeyBadge : hFontRegular, isDown);
            return TRUE;
        }

        if (pDIS->CtlID == IDC_BTN_PICK_DST) {
            bool isRec = (g_recMode == REC_TARGET);
            COLORREF bg = isRec ? c.recording : c.cardBg;
            COLORREF border = isRec ? c.recording : (g_dstVk ? c.accentBlue : c.cardBorder);
            std::wstring txt = isRec ? str.btnPressAnyKey : (g_dstVk ? g_dstRawName : str.btnPickDstDefault);
            DrawModernButton(pDIS->hDC, pDIS->rcItem, bg, border, c.textPrimary, txt.c_str(), g_dstVk ? hFontKeyBadge : hFontRegular, isDown);
            return TRUE;
        }

        if (pDIS->CtlID == IDC_BTN_ADD_MAP) {
            DrawModernButton(pDIS->hDC, pDIS->rcItem, c.accentBlue, c.accentHover, RGB(255, 255, 255), str.btnAdd, hFontBold, isDown);
            return TRUE;
        }

        if (pDIS->CtlID == IDC_BTN_DEL_MAP) {
            DrawModernButton(pDIS->hDC, pDIS->rcItem, c.deleteBtnBg, c.deleteBtnBorder, c.deleteBtnText, str.btnDel, hFontBold, isDown);
            return TRUE;
        }

        if (pDIS->CtlID == IDC_BTN_TOGGLE) {
            COLORREF bg = g_isMappingActive ? c.danger : c.success;
            const wchar_t* txt = g_isMappingActive ? str.btnStop : str.btnStart;
            DrawModernButton(pDIS->hDC, pDIS->rcItem, bg, bg, RGB(255, 255, 255), txt, hFontBold, isDown);
            return TRUE;
        }

        if (pDIS->CtlID == IDC_LIST_MAPPINGS && pDIS->itemID != (UINT)-1) {
            HDC hdc = pDIS->hDC;
            RECT rc = pDIS->rcItem;
            bool isSel = (pDIS->itemState & ODS_SELECTED);

            COLORREF rowBg = isSel ? (g_activeVisual == VISUAL_DARK ? RGB(45, 55, 72) : RGB(229, 231, 235)) : c.cardBg;
            HBRUSH hRowBg = CreateSolidBrush(rowBg);
            FillRect(hdc, &rc, hRowBg);
            DeleteObject(hRowBg);

            if (pDIS->itemID < g_mapKeysOrder.size()) {
                DWORD srcVk = g_mapKeysOrder[pDIS->itemID];
                const auto& item = g_mapList[srcVk];

                SetBkMode(hdc, TRANSPARENT);

                RECT rcSrc = { rc.left + 10, rc.top + 6, rc.left + 140, rc.bottom - 6 };
                DrawModernButton(hdc, rcSrc, c.badgeBg, c.badgeBorder, c.textPrimary, item.srcName.c_str(), hFontKeyBadge, false);

                RECT rcArrow = { rc.left + 145, rc.top + 6, rc.left + 185, rc.bottom - 6 };
                SetTextColor(hdc, c.accentBlue);
                SelectObject(hdc, hFontBold);
                DrawTextW(hdc, L"➔", -1, &rcArrow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                RECT rcDst = { rc.left + 190, rc.top + 6, rc.left + 320, rc.bottom - 6 };
                DrawModernButton(hdc, rcDst, c.badgeBg, c.badgeBorder, c.textPrimary, item.dstName.c_str(), hFontKeyBadge, false);
            }
            return TRUE;
        }
        break;
    }

    case WM_KEY_RECORDED: {
        DWORD vk = (DWORD)wParam;
        DWORD scanCode = (DWORD)(lParam & 0xFF);
        bool isExtended = (lParam & 0x100) != 0;

        std::wstring keyName = GetKeyDisplayName(vk, scanCode, isExtended);

        if (g_recMode == REC_SOURCE) {
            g_srcVk = vk;
            g_srcRawName = keyName;
        } else if (g_recMode == REC_TARGET) {
            g_dstVk = vk;
            g_dstRawName = keyName;
        }
        g_recMode = REC_NONE;
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        const auto& str = g_i18n[g_currentLang];

        switch (wmId) {
        case IDM_LANG_ID:
            g_currentLang = LANG_ID;
            ApplyThemeAndLanguage(hwnd);
            break;

        case IDM_LANG_EN:
            g_currentLang = LANG_EN;
            ApplyThemeAndLanguage(hwnd);
            break;

        case IDM_THEME_SYSTEM:
            g_themeChoice = THEME_CHOICE_SYSTEM;
            ApplyThemeAndLanguage(hwnd);
            break;

        case IDM_THEME_DARK:
            g_themeChoice = THEME_CHOICE_DARK;
            ApplyThemeAndLanguage(hwnd);
            break;

        case IDM_THEME_LIGHT:
            g_themeChoice = THEME_CHOICE_LIGHT;
            ApplyThemeAndLanguage(hwnd);
            break;

        // EKSPOR / SIMPAN PRESET
        case IDM_PRESET_SAVE: {
            if (g_mapList.empty()) {
                MessageBoxW(hwnd, str.msgSaveEmpty, str.msgTitleWarn, MB_ICONWARNING);
                break;
            }
            OPENFILENAMEW ofn = { 0 };
            wchar_t szFile[MAX_PATH] = L"MyPreset.kmp";
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
            ofn.lpstrFilter = L"Keyboard Mapper Preset (*.kmp)\0*.kmp\0Semua File (*.*)\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrDefExt = L"kmp";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

            if (GetSaveFileNameW(&ofn)) {
                if (SaveMappingsToFile(ofn.lpstrFile)) {
                    MessageBoxW(hwnd, str.msgSaved, str.msgTitleInfo, MB_ICONINFORMATION);
                }
            }
            break;
        }

        // IMPOR / MUAT PRESET
        case IDM_PRESET_LOAD: {
            OPENFILENAMEW ofn = { 0 };
            wchar_t szFile[MAX_PATH] = L"";
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
            ofn.lpstrFilter = L"Keyboard Mapper Preset (*.kmp)\0*.kmp\0Semua File (*.*)\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameW(&ofn)) {
                if (LoadMappingsFromFile(ofn.lpstrFile)) {
                    SaveMappingsToFile(GetDefaultConfigPath());
                    InvalidateRect(hListMap, NULL, TRUE);
                    MessageBoxW(hwnd, str.msgLoaded, str.msgTitleInfo, MB_ICONINFORMATION);
                }
            }
            break;
        }

        case IDM_OPT_STARTUP: {
            bool currentStatus = IsRunAtStartupEnabled();
            SetRunAtStartup(!currentStatus);
            ApplyThemeAndLanguage(hwnd);
            break;
        }

        case IDM_APP_EXIT:
            DestroyWindow(hwnd);
            break;

        case IDC_BTN_PICK_SRC:
            g_recMode = (g_recMode == REC_SOURCE) ? REC_NONE : REC_SOURCE;
            InvalidateRect(hwnd, NULL, FALSE);
            break;

        case IDC_BTN_PICK_DST:
            g_recMode = (g_recMode == REC_TARGET) ? REC_NONE : REC_TARGET;
            InvalidateRect(hwnd, NULL, FALSE);
            break;

        case IDC_BTN_ADD_MAP:
            if (g_srcVk == 0 || g_dstVk == 0) {
                MessageBoxW(hwnd, str.msgEmpty, str.msgTitleInfo, MB_ICONINFORMATION);
            } else if (g_srcVk == g_dstVk) {
                MessageBoxW(hwnd, str.msgSameKey, str.msgTitleWarn, MB_ICONWARNING);
            } else {
                g_mapList[g_srcVk] = { g_dstVk, g_srcRawName, g_dstRawName };
                RefreshMappingList();
                SaveMappingsToFile(GetDefaultConfigPath());

                g_srcVk = 0; g_dstVk = 0;
                g_srcRawName = L""; g_dstRawName = L"";
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;

        case IDC_BTN_DEL_MAP: {
            int curSel = SendMessage(hListMap, LB_GETCURSEL, 0, 0);
            if (curSel != LB_ERR && curSel < (int)g_mapKeysOrder.size()) {
                DWORD vkToDelete = g_mapKeysOrder[curSel];
                g_mapList.erase(vkToDelete);
                RefreshMappingList();
                SaveMappingsToFile(GetDefaultConfigPath());
                InvalidateRect(hListMap, NULL, TRUE);
            }
            break;
        }

        case IDC_BTN_TOGGLE:
        case IDM_TRAY_TOGGLE:
            g_isMappingActive = !g_isMappingActive;
            SetWindowTextW(hStatus, g_isMappingActive ? str.statusActive : str.statusIdle);
            InvalidateRect(hwnd, NULL, FALSE);
            break;

        case IDM_TRAY_SHOW: {
            if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
                ShowWindow(hwnd, SW_HIDE);
            } else {
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
            }
            break;
        }

        case IDM_TRAY_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        break;
    }

    case WM_TRAYICON: {
        if (lParam == WM_LBUTTONUP) {
            if (IsWindowVisible(hwnd) && !IsIconic(hwnd) && GetForegroundWindow() == hwnd) {
                ShowWindow(hwnd, SW_HIDE);
            } else {
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
            }
        } else if (lParam == WM_RBUTTONUP) {
            ShowTrayContextMenu(hwnd);
        }
        break;
    }

    case WM_SYSCOMMAND: {
        if ((wParam & 0xFFF0) == SC_MINIMIZE) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    case WM_CLOSE: {
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        if (g_hHook != NULL) UnhookWindowsHookEx(g_hHook);
        if (hFontRegular) DeleteObject(hFontRegular);
        if (hFontBold) DeleteObject(hFontBold);
        if (hFontSmall) DeleteObject(hFontSmall);
        if (hFontKeyBadge) DeleteObject(hFontKeyBadge);
        if (hBrushBg) DeleteObject(hBrushBg);
        if (hBrushCard) DeleteObject(hBrushCard);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE hMutex = CreateMutexW(NULL, TRUE, SINGLE_INSTANCE_MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existingWnd = FindWindowW(WINDOW_CLASS_NAME, NULL);
        if (existingWnd) {
            ShowWindow(existingWnd, SW_RESTORE);
            SetForegroundWindow(existingWnd);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    g_activeVisual = GetCurrentEffectiveTheme();

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(PALETTE[g_activeVisual].bg);

    RegisterClassW(&wc);

    g_hWndMain = CreateWindowExW(
        0, WINDOW_CLASS_NAME,
        g_i18n[g_currentLang].winTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 595, 540,
        NULL, NULL, hInstance, NULL
    );

    if (g_hWndMain == NULL) {
        if (hMutex) {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }
        return 0;
    }

    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);

    bool startMinimized = (wcsstr(GetCommandLineW(), L"--startup") != NULL);
    if (!startMinimized) {
        ShowWindow(g_hWndMain, nCmdShow);
        UpdateWindow(g_hWndMain);
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return (int)msg.wParam;
}