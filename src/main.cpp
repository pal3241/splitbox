#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")

namespace {

constexpr wchar_t kAppClass[] = L"SplitBoxMainWindow";
constexpr wchar_t kAddClass[] = L"SplitBoxAddAccountWindow";
constexpr wchar_t kVersion[] = L"v0.1.0";

enum ControlId : int {
    ID_TAB_ACCOUNTS = 100,
    ID_TAB_VIP,
    ID_TAB_ASSIGNMENTS,
    ID_TAB_PRESETS,
    ID_TAB_REJOIN,
    ID_TAB_SPLIT,
    ID_TAB_SETTINGS,

    ID_SEARCH = 200,
    ID_ADD_ACCOUNT,
    ID_ACCOUNT_LIST,
    ID_OPEN_ROBLOX,
    ID_REMOVE_ACCOUNT,
    ID_REFRESH,

    ID_ADD_USERNAME = 300,
    ID_ADD_DISPLAY,
    ID_ADD_OK,
    ID_ADD_CANCEL
};

struct Account {
    std::wstring username;
    std::wstring displayName;
};

HINSTANCE g_instance{};
HWND g_main{};
HWND g_search{};
HWND g_accountList{};
HWND g_addAccount{};
HWND g_title{};
HWND g_subtitle{};
HWND g_centerPrimary{};
HWND g_centerSecondary{};
HWND g_openRoblox{};
HWND g_removeAccount{};
HWND g_status{};
HWND g_count{};
HWND g_splitCard{};
std::vector<HWND> g_tabs;
std::vector<Account> g_accounts;
int g_activeTab = ID_TAB_ACCOUNTS;

HBRUSH g_bgBrush{};
HBRUSH g_panelBrush{};
HBRUSH g_controlBrush{};
HBRUSH g_accentBrush{};
HFONT g_font{};
HFONT g_fontLarge{};
HFONT g_fontSmall{};

constexpr COLORREF C_BG = RGB(24, 24, 24);
constexpr COLORREF C_PANEL = RGB(29, 29, 29);
constexpr COLORREF C_CONTROL = RGB(45, 45, 48);
constexpr COLORREF C_TEXT = RGB(220, 220, 220);
constexpr COLORREF C_MUTED = RGB(145, 145, 145);
constexpr COLORREF C_ACCENT = RGB(25, 109, 157);
constexpr COLORREF C_GREEN = RGB(40, 170, 85);

std::filesystem::path DataDir() {
    wchar_t path[MAX_PATH]{};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    std::filesystem::path p = n ? path : L".";
    p /= L"SplitBox";
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
}

std::filesystem::path AccountsFile() {
    return DataDir() / L"accounts.txt";
}

std::string NarrowUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring WideUtf8(const std::string& value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), size);
    return out;
}

void LoadAccounts() {
    g_accounts.clear();
    std::ifstream in(AccountsFile(), std::ios::binary);
    if (!in) return;

    std::string line;
    while (std::getline(in, line)) {
        auto sep = line.find('|');
        if (sep == std::string::npos) continue;
        Account a;
        a.username = WideUtf8(line.substr(0, sep));
        a.displayName = WideUtf8(line.substr(sep + 1));
        if (!a.username.empty()) g_accounts.push_back(std::move(a));
    }
}

void SaveAccounts() {
    std::ofstream out(AccountsFile(), std::ios::binary | std::ios::trunc);
    for (const auto& a : g_accounts) {
        out << NarrowUtf8(a.username) << "|" << NarrowUtf8(a.displayName) << "\n";
    }
}

std::wstring GetText(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    std::wstring text(static_cast<size_t>(len), L'\0');
    if (len) GetWindowTextW(hwnd, text.data(), len + 1);
    return text;
}

void SetControlFont(HWND hwnd, HFONT font = nullptr) {
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : g_font), TRUE);
}

HWND MakeControl(
    DWORD exStyle,
    const wchar_t* cls,
    const wchar_t* text,
    DWORD style,
    int x,
    int y,
    int w,
    int h,
    HWND parent,
    int id
) {
    HWND hwnd = CreateWindowExW(
        exStyle, cls, text, style,
        x, y, w, h,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_instance,
        nullptr
    );
    SetControlFont(hwnd);
    return hwnd;
}

void SetVisible(HWND hwnd, bool visible) {
    if (hwnd) ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

void ApplyDarkTitleBar(HWND hwnd) {
    BOOL enable = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &enable, sizeof(enable));
}

int CountRobloxWindows() {
    struct State { int count = 0; } state;

    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        if (!IsWindowVisible(hwnd)) return TRUE;

        DWORD pid{};
        GetWindowThreadProcessId(hwnd, &pid);
        if (!pid) return TRUE;

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) return TRUE;

        wchar_t path[MAX_PATH]{};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(process, 0, path, &size)) {
            const wchar_t* filename = PathFindFileNameW(path);
            if (_wcsicmp(filename, L"RobloxPlayerBeta.exe") == 0) {
                auto* s = reinterpret_cast<State*>(param);
                ++s->count;
            }
        }
        CloseHandle(process);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&state));

    return state.count;
}

void UpdateStatus() {
    int running = CountRobloxWindows();
    std::wstring text = L"Roblox instances detected: " + std::to_wstring(running);
    SetWindowTextW(g_status, text.c_str());

    std::wstring count = std::to_wstring(g_accounts.size()) + L" account(s)";
    SetWindowTextW(g_count, count.c_str());
}

void RefreshAccountList() {
    if (!g_accountList) return;

    std::wstring filter = GetText(g_search);
    std::wstring lowerFilter = filter;
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::towlower);

    SendMessageW(g_accountList, LB_RESETCONTENT, 0, 0);

    for (size_t i = 0; i < g_accounts.size(); ++i) {
        std::wstring hay = g_accounts[i].username + L" " + g_accounts[i].displayName;
        std::transform(hay.begin(), hay.end(), hay.begin(), ::towlower);
        if (!lowerFilter.empty() && hay.find(lowerFilter) == std::wstring::npos) continue;

        std::wstring line = g_accounts[i].username;
        if (!g_accounts[i].displayName.empty()) line += L"   ·   " + g_accounts[i].displayName;
        LRESULT pos = SendMessageW(g_accountList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
        SendMessageW(g_accountList, LB_SETITEMDATA, pos, static_cast<LPARAM>(i));
    }

    UpdateStatus();
}

void ShowEmptyAccountState() {
    SetWindowTextW(g_centerPrimary, L"No account selected");
    SetWindowTextW(g_centerSecondary, L"Pick an account in the sidebar to view it.");
    SetVisible(g_openRoblox, false);
    SetVisible(g_removeAccount, false);
}

void ShowSelectedAccount() {
    int pos = static_cast<int>(SendMessageW(g_accountList, LB_GETCURSEL, 0, 0));
    if (pos == LB_ERR) {
        ShowEmptyAccountState();
        return;
    }

    LRESULT data = SendMessageW(g_accountList, LB_GETITEMDATA, pos, 0);
    if (data == LB_ERR || data < 0 || static_cast<size_t>(data) >= g_accounts.size()) {
        ShowEmptyAccountState();
        return;
    }

    const auto& a = g_accounts[static_cast<size_t>(data)];
    SetWindowTextW(g_centerPrimary, a.username.c_str());

    std::wstring sub = a.displayName.empty()
        ? L"Local account profile"
        : a.displayName + L"  ·  Local account profile";
    SetWindowTextW(g_centerSecondary, sub.c_str());

    SetVisible(g_openRoblox, true);
    SetVisible(g_removeAccount, true);
}

void OpenRoblox() {
    ShellExecuteW(g_main, L"open", L"roblox://", nullptr, nullptr, SW_SHOWNORMAL);
}

void RemoveSelectedAccount() {
    int pos = static_cast<int>(SendMessageW(g_accountList, LB_GETCURSEL, 0, 0));
    if (pos == LB_ERR) return;
    LRESULT data = SendMessageW(g_accountList, LB_GETITEMDATA, pos, 0);
    if (data == LB_ERR || data < 0 || static_cast<size_t>(data) >= g_accounts.size()) return;

    const auto& account = g_accounts[static_cast<size_t>(data)];
    std::wstring prompt = L"Remove " + account.username + L" from SplitBox?\n\nThis only removes the local SplitBox entry.";
    if (MessageBoxW(g_main, prompt.c_str(), L"Remove account", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    g_accounts.erase(g_accounts.begin() + data);
    SaveAccounts();
    RefreshAccountList();
    ShowEmptyAccountState();
}

struct AddDialogState {
    HWND username{};
    HWND display{};
    bool accepted = false;
    Account account;
};

LRESULT CALLBACK AddAccountProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<AddDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<AddDialogState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        ApplyDarkTitleBar(hwnd);

        HWND label1 = MakeControl(0, L"STATIC", L"Username", WS_CHILD | WS_VISIBLE, 24, 25, 310, 22, hwnd, 0);
        state->username = MakeControl(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 24, 50, 330, 34, hwnd, ID_ADD_USERNAME);
        HWND label2 = MakeControl(0, L"STATIC", L"Display name (optional)", WS_CHILD | WS_VISIBLE, 24, 100, 310, 22, hwnd, 0);
        state->display = MakeControl(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 24, 125, 330, 34, hwnd, ID_ADD_DISPLAY);
        HWND ok = MakeControl(0, L"BUTTON", L"Add account", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 184, 186, 170, 36, hwnd, ID_ADD_OK);
        HWND cancel = MakeControl(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 24, 186, 140, 36, hwnd, ID_ADD_CANCEL);
        SetControlFont(label1);
        SetControlFont(label2);
        SetControlFont(ok);
        SetControlFont(cancel);
        SetFocus(state->username);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_ADD_OK:
            if (state) {
                state->account.username = GetText(state->username);
                state->account.displayName = GetText(state->display);
                if (state->account.username.empty()) {
                    MessageBoxW(hwnd, L"Username cannot be empty.", L"SplitBox", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                state->accepted = true;
            }
            DestroyWindow(hwnd);
            return 0;
        case ID_ADD_CANCEL:
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_BG);
        return reinterpret_cast<LRESULT>(g_bgBrush);
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_CONTROL);
        return reinterpret_cast<LRESULT>(g_controlBrush);
    }
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, g_bgBrush);
        return 1;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void AddAccountDialog() {
    AddDialogState state{};

    HWND dlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kAddClass,
        L"Add Account",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 398, 285,
        g_main,
        nullptr,
        g_instance,
        &state
    );
    if (!dlg) return;

    RECT parent{}, child{};
    GetWindowRect(g_main, &parent);
    GetWindowRect(dlg, &child);
    int w = child.right - child.left;
    int h = child.bottom - child.top;
    SetWindowPos(dlg, HWND_TOP,
        parent.left + ((parent.right - parent.left) - w) / 2,
        parent.top + ((parent.bottom - parent.top) - h) / 2,
        0, 0,
        SWP_NOSIZE | SWP_SHOWWINDOW);

    EnableWindow(g_main, FALSE);

    MSG msg{};
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(g_main, TRUE);
    SetForegroundWindow(g_main);

    if (state.accepted) {
        auto exists = std::find_if(g_accounts.begin(), g_accounts.end(), [&](const Account& a) {
            return _wcsicmp(a.username.c_str(), state.account.username.c_str()) == 0;
        });
        if (exists != g_accounts.end()) {
            MessageBoxW(g_main, L"That account already exists in SplitBox.", L"SplitBox", MB_OK | MB_ICONINFORMATION);
            return;
        }
        g_accounts.push_back(std::move(state.account));
        SaveAccounts();
        RefreshAccountList();
    }
}

void SetTabVisibility() {
    bool accounts = g_activeTab == ID_TAB_ACCOUNTS;
    bool split = g_activeTab == ID_TAB_SPLIT;

    SetVisible(g_search, accounts);
    SetVisible(g_addAccount, accounts);
    SetVisible(g_accountList, accounts);
    SetVisible(g_centerPrimary, accounts);
    SetVisible(g_centerSecondary, accounts);
    SetVisible(g_openRoblox, accounts && SendMessageW(g_accountList, LB_GETCURSEL, 0, 0) != LB_ERR);
    SetVisible(g_removeAccount, accounts && SendMessageW(g_accountList, LB_GETCURSEL, 0, 0) != LB_ERR);
    SetVisible(g_splitCard, split);

    if (!accounts && !split) {
        SetVisible(g_title, true);
        SetVisible(g_subtitle, true);
        SetWindowTextW(g_title, L"Coming soon");
        SetWindowTextW(g_subtitle, L"This section is scaffolded for the next SplitBox milestone.");
    } else {
        SetVisible(g_title, false);
        SetVisible(g_subtitle, false);
    }

    for (HWND tab : g_tabs) {
        InvalidateRect(tab, nullptr, TRUE);
    }
}

void SwitchTab(int id) {
    g_activeTab = id;
    SetTabVisibility();
}

void Layout(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    int W = rc.right;
    int H = rc.bottom;

    constexpr int top = 42;
    constexpr int sidebar = 330;
    constexpr int pad = 12;

    int x = 10;
    const int tabY = 4;
    const int tabH = 34;
    const int widths[] = {118, 118, 155, 105, 130, 125, 105};
    for (size_t i = 0; i < g_tabs.size(); ++i) {
        MoveWindow(g_tabs[i], x, tabY, widths[i], tabH, TRUE);
        x += widths[i] + 4;
    }

    MoveWindow(g_count, W - 150, 8, 130, 25, TRUE);
    MoveWindow(g_search, 12, top + 54, sidebar - 24, 34, TRUE);
    MoveWindow(g_addAccount, 12, top + 96, 150, 34, TRUE);
    MoveWindow(g_accountList, 12, top + 142, sidebar - 24, H - top - 190, TRUE);

    int centerX = sidebar;
    int centerW = W - sidebar;
    MoveWindow(g_centerPrimary, centerX + 40, H / 2 - 60, centerW - 80, 45, TRUE);
    MoveWindow(g_centerSecondary, centerX + 40, H / 2 - 10, centerW - 80, 30, TRUE);
    MoveWindow(g_openRoblox, centerX + centerW / 2 - 150, H / 2 + 50, 140, 38, TRUE);
    MoveWindow(g_removeAccount, centerX + centerW / 2 + 10, H / 2 + 50, 140, 38, TRUE);

    MoveWindow(g_title, centerX + 40, H / 2 - 50, centerW - 80, 45, TRUE);
    MoveWindow(g_subtitle, centerX + 40, H / 2 + 4, centerW - 80, 30, TRUE);

    MoveWindow(g_splitCard, centerX + 45, top + 55, centerW - 90, H - top - 120, TRUE);
    MoveWindow(g_status, 12, H - 34, W - 24, 22, TRUE);
}

void CreateUi(HWND hwnd) {
    const wchar_t* tabNames[] = {
        L"◇ Accounts",
        L"♙ VIP Servers",
        L"◎ VIP Assignments",
        L"☆ Presets",
        L"↻ Auto-Rejoin",
        L"▣ Split Screen",
        L"⚙ Settings"
    };
    const int tabIds[] = {
        ID_TAB_ACCOUNTS,
        ID_TAB_VIP,
        ID_TAB_ASSIGNMENTS,
        ID_TAB_PRESETS,
        ID_TAB_REJOIN,
        ID_TAB_SPLIT,
        ID_TAB_SETTINGS
    };

    for (int i = 0; i < 7; ++i) {
        HWND btn = MakeControl(0, L"BUTTON", tabNames[i],
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 100, 34, hwnd, tabIds[i]);
        g_tabs.push_back(btn);
    }

    g_count = MakeControl(0, L"STATIC", L"0 account(s)",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        0, 0, 120, 22, hwnd, 0);

    HWND accountsLabel = MakeControl(0, L"STATIC", L"Accounts",
        WS_CHILD | WS_VISIBLE,
        12, 52, 300, 34, hwnd, 0);
    SetControlFont(accountsLabel, g_fontLarge);

    g_search = MakeControl(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 100, 34, hwnd, ID_SEARCH);
    SendMessageW(g_search, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Search accounts..."));

    g_addAccount = MakeControl(0, L"BUTTON", L"+  Add Account",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 150, 34, hwnd, ID_ADD_ACCOUNT);

    g_accountList = MakeControl(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
        0, 0, 200, 300, hwnd, ID_ACCOUNT_LIST);

    g_centerPrimary = MakeControl(0, L"STATIC", L"No account selected",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 0, 400, 45, hwnd, 0);
    SetControlFont(g_centerPrimary, g_fontLarge);

    g_centerSecondary = MakeControl(0, L"STATIC", L"Pick an account in the sidebar to view it.",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 0, 500, 30, hwnd, 0);

    g_openRoblox = MakeControl(0, L"BUTTON", L"Open Roblox",
        WS_CHILD | BS_PUSHBUTTON,
        0, 0, 140, 38, hwnd, ID_OPEN_ROBLOX);

    g_removeAccount = MakeControl(0, L"BUTTON", L"Remove",
        WS_CHILD | BS_PUSHBUTTON,
        0, 0, 140, 38, hwnd, ID_REMOVE_ACCOUNT);

    g_title = MakeControl(0, L"STATIC", L"Coming soon",
        WS_CHILD | SS_CENTER,
        0, 0, 400, 45, hwnd, 0);
    SetControlFont(g_title, g_fontLarge);

    g_subtitle = MakeControl(0, L"STATIC", L"This section is scaffolded for the next SplitBox milestone.",
        WS_CHILD | SS_CENTER,
        0, 0, 500, 30, hwnd, 0);

    g_splitCard = MakeControl(0, L"STATIC",
        L"Split Screen\n\n"
        L"Planned architecture:\n"
        L"  • Player 1 / Player 2 account assignment\n"
        L"  • Keyboard + mouse device binding\n"
        L"  • Roblox PID/HWND pairing\n"
        L"  • Side-by-side / top-bottom layouts\n"
        L"  • Raw Input based device discovery\n\n"
        L"Input isolation is intentionally not enabled in v0.1.0.",
        WS_CHILD | SS_LEFT,
        0, 0, 500, 300, hwnd, 0);
    SetControlFont(g_splitCard);

    g_status = MakeControl(0, L"STATIC", L"Roblox instances detected: 0",
        WS_CHILD | WS_VISIBLE,
        0, 0, 500, 22, hwnd, 0);
    SetControlFont(g_status, g_fontSmall);

    LoadAccounts();
    RefreshAccountList();
    ShowEmptyAccountState();
    SetTabVisibility();
}

void PaintMain(HWND hwnd, HDC dc) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    FillRect(dc, &rc, g_bgBrush);

    RECT top{0, 0, rc.right, 42};
    FillRect(dc, &top, g_panelBrush);

    RECT sidebar{0, 42, 330, rc.bottom};
    FillRect(dc, &sidebar, g_panelBrush);

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(58, 58, 58));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, 329, 42, nullptr);
    LineTo(dc, 329, rc.bottom);
    MoveToEx(dc, 0, 41, nullptr);
    LineTo(dc, rc.right, 41);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

LRESULT DrawOwnerButton(LPDRAWITEMSTRUCT dis) {
    bool selected = static_cast<int>(dis->CtlID) == g_activeTab;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;

    HBRUSH brush = selected ? g_accentBrush : (pressed ? g_controlBrush : g_panelBrush);
    FillRect(dis->hDC, &dis->rcItem, brush);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, selected ? RGB(225, 245, 255) : C_TEXT);
    SelectObject(dis->hDC, g_font);

    wchar_t text[128]{};
    GetWindowTextW(dis->hwndItem, text, 128);
    RECT r = dis->rcItem;
    DrawTextW(dis->hDC, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return TRUE;
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_main = hwnd;
        ApplyDarkTitleBar(hwnd);
        CreateUi(hwnd);
        SetTimer(hwnd, 1, 2000, nullptr);
        return 0;

    case WM_SIZE:
        Layout(hwnd);
        return 0;

    case WM_TIMER:
        if (wParam == 1) UpdateStatus();
        return 0;

    case WM_DRAWITEM:
        return DrawOwnerButton(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id >= ID_TAB_ACCOUNTS && id <= ID_TAB_SETTINGS) {
            SwitchTab(id);
            return 0;
        }

        switch (id) {
        case ID_SEARCH:
            if (code == EN_CHANGE) RefreshAccountList();
            return 0;
        case ID_ADD_ACCOUNT:
            AddAccountDialog();
            return 0;
        case ID_ACCOUNT_LIST:
            if (code == LBN_SELCHANGE) ShowSelectedAccount();
            return 0;
        case ID_OPEN_ROBLOX:
            OpenRoblox();
            return 0;
        case ID_REMOVE_ACCOUNT:
            RemoveSelectedAccount();
            return 0;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND child = reinterpret_cast<HWND>(lParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, child == g_status || child == g_count || child == g_centerSecondary || child == g_subtitle ? C_MUTED : C_TEXT);
        SetBkColor(dc, C_BG);
        return reinterpret_cast<LRESULT>(g_bgBrush);
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_CONTROL);
        return reinterpret_cast<LRESULT>(g_controlBrush);
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        PaintMain(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool RegisterClasses() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = g_instance;
    wc.lpfnWndProc = MainProc;
    wc.lpszClassName = kAppClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = g_bgBrush;
    if (!RegisterClassExW(&wc)) return false;

    WNDCLASSEXW add = wc;
    add.lpfnWndProc = AddAccountProc;
    add.lpszClassName = kAddClass;
    return RegisterClassExW(&add) != 0;
}

void CreateFonts() {
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

    LOGFONTW lf = ncm.lfMessageFont;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    lf.lfHeight = -18;
    lf.lfWeight = FW_NORMAL;
    g_font = CreateFontIndirectW(&lf);

    lf.lfHeight = -28;
    lf.lfWeight = FW_LIGHT;
    g_fontLarge = CreateFontIndirectW(&lf);

    lf.lfHeight = -15;
    lf.lfWeight = FW_NORMAL;
    g_fontSmall = CreateFontIndirectW(&lf);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_instance = instance;

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    g_bgBrush = CreateSolidBrush(C_BG);
    g_panelBrush = CreateSolidBrush(C_PANEL);
    g_controlBrush = CreateSolidBrush(C_CONTROL);
    g_accentBrush = CreateSolidBrush(C_ACCENT);
    CreateFonts();

    if (!RegisterClasses()) {
        MessageBoxW(nullptr, L"Failed to register SplitBox window classes.", L"SplitBox", MB_OK | MB_ICONERROR);
        return 1;
    }

    std::wstring title = L"SB | SplitBox ";
    title += kVersion;

    HWND hwnd = CreateWindowExW(
        0,
        kAppClass,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1440,
        920,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!hwnd) return 1;

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_font);
    DeleteObject(g_fontLarge);
    DeleteObject(g_fontSmall);
    DeleteObject(g_bgBrush);
    DeleteObject(g_panelBrush);
    DeleteObject(g_controlBrush);
    DeleteObject(g_accentBrush);

    return static_cast<int>(msg.wParam);
}
