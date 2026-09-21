#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "split_screen.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Shell32.lib")

namespace {

using splitbox::SplitLayout;
using splitbox::SplitScreenManager;

constexpr wchar_t kAppClass[] = L"SplitBoxMainWindow";
constexpr wchar_t kAddClass[] = L"SplitBoxAddAccountWindow";
constexpr wchar_t kVersion[] = L"v0.2.0";

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

    ID_ADD_USERNAME = 300,
    ID_ADD_DISPLAY,
    ID_ADD_OK,
    ID_ADD_CANCEL,

    ID_SPLIT_REFRESH = 400,
    ID_P1_WINDOW,
    ID_P2_WINDOW,
    ID_P1_KEYBOARD,
    ID_P1_MOUSE,
    ID_P2_KEYBOARD,
    ID_P2_MOUSE,
    ID_SPLIT_LAYOUT,
    ID_EXPERIMENTAL_ROUTING,
    ID_SPLIT_START,
    ID_SPLIT_STOP
};

struct Account {
    std::wstring username;
    std::wstring displayName;
};

HINSTANCE g_instance{};
HWND g_main{};
int g_activeTab = ID_TAB_ACCOUNTS;

HBRUSH g_bgBrush{};
HBRUSH g_panelBrush{};
HBRUSH g_controlBrush{};
HBRUSH g_accentBrush{};
HFONT g_font{};
HFONT g_fontLarge{};
HFONT g_fontSmall{};
HFONT g_fontSection{};

constexpr COLORREF C_BG = RGB(24, 24, 24);
constexpr COLORREF C_PANEL = RGB(29, 29, 29);
constexpr COLORREF C_CONTROL = RGB(45, 45, 48);
constexpr COLORREF C_TEXT = RGB(220, 220, 220);
constexpr COLORREF C_MUTED = RGB(145, 145, 145);
constexpr COLORREF C_ACCENT = RGB(25, 109, 157);

std::vector<HWND> g_tabs;
std::vector<HWND> g_accountControls;
std::vector<HWND> g_splitControls;
std::vector<HWND> g_placeholderControls;
std::vector<Account> g_accounts;

HWND g_count{};
HWND g_globalStatus{};

HWND g_search{};
HWND g_accountList{};
HWND g_addAccount{};
HWND g_accountTitle{};
HWND g_centerPrimary{};
HWND g_centerSecondary{};
HWND g_openRoblox{};
HWND g_removeAccount{};

HWND g_placeholderTitle{};
HWND g_placeholderText{};

HWND g_splitTitle{};
HWND g_splitHelp{};
HWND g_splitRefresh{};
HWND g_p1Window{};
HWND g_p2Window{};
HWND g_p1Keyboard{};
HWND g_p2Keyboard{};
HWND g_p1Mouse{};
HWND g_p2Mouse{};
HWND g_splitLayout{};
HWND g_experimentalRouting{};
HWND g_splitStart{};
HWND g_splitStop{};
HWND g_splitStatus{};
HWND g_inputStatus{};

SplitScreenManager g_split;

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
    int chars = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(chars), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), chars, nullptr, nullptr);
    return out;
}

std::wstring WideUtf8(const std::string& value) {
    if (value.empty()) return {};
    int chars = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), chars);
    return out;
}

void ApplyDarkTitleBar(HWND hwnd) {
    BOOL enable = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &enable, sizeof(enable));
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
    int id,
    HFONT font = nullptr
) {
    HWND hwnd = CreateWindowExW(
        exStyle, cls, text, style,
        x, y, w, h,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_instance,
        nullptr
    );
    SetControlFont(hwnd, font);
    return hwnd;
}

void SetVisible(HWND hwnd, bool visible) {
    if (hwnd) ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

void ShowGroup(const std::vector<HWND>& controls, bool visible) {
    for (HWND hwnd : controls) SetVisible(hwnd, visible);
}

std::wstring GetText(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    std::wstring text(static_cast<size_t>(len) + 1, L'\0');
    if (len) GetWindowTextW(hwnd, text.data(), len + 1);
    text.resize(static_cast<size_t>(len));
    return text;
}

void LoadAccounts() {
    g_accounts.clear();
    std::ifstream in(AccountsFile(), std::ios::binary);
    if (!in) return;

    std::string line;
    while (std::getline(in, line)) {
        auto sep = line.find('|');
        if (sep == std::string::npos) continue;

        Account account;
        account.username = WideUtf8(line.substr(0, sep));
        account.displayName = WideUtf8(line.substr(sep + 1));
        if (!account.username.empty()) g_accounts.push_back(std::move(account));
    }
}

void SaveAccounts() {
    std::ofstream out(AccountsFile(), std::ios::binary | std::ios::trunc);
    for (const auto& account : g_accounts) {
        out << NarrowUtf8(account.username) << "|" << NarrowUtf8(account.displayName) << "\n";
    }
}

void UpdateGlobalStatus() {
    g_split.refreshWindows();
    std::wstring status =
        L"Roblox windows: " + std::to_wstring(g_split.windows().size()) +
        L"  ·  Split screen: " + (g_split.active() ? L"ACTIVE" : L"off");
    SetWindowTextW(g_globalStatus, status.c_str());

    std::wstring count = std::to_wstring(g_accounts.size()) + L" account(s)";
    SetWindowTextW(g_count, count.c_str());

    if (g_split.active()) {
        SetWindowTextW(g_inputStatus, g_split.inputStatus().c_str());
    }
}

void RefreshAccountList() {
    if (!g_accountList) return;

    std::wstring filter = GetText(g_search);
    std::transform(filter.begin(), filter.end(), filter.begin(), ::towlower);

    SendMessageW(g_accountList, LB_RESETCONTENT, 0, 0);

    for (size_t i = 0; i < g_accounts.size(); ++i) {
        std::wstring searchable = g_accounts[i].username + L" " + g_accounts[i].displayName;
        std::transform(searchable.begin(), searchable.end(), searchable.begin(), ::towlower);

        if (!filter.empty() && searchable.find(filter) == std::wstring::npos) continue;

        std::wstring line = g_accounts[i].username;
        if (!g_accounts[i].displayName.empty()) line += L"   ·   " + g_accounts[i].displayName;

        LRESULT row = SendMessageW(g_accountList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
        SendMessageW(g_accountList, LB_SETITEMDATA, row, static_cast<LPARAM>(i));
    }

    UpdateGlobalStatus();
}

void ShowEmptyAccountState() {
    SetWindowTextW(g_centerPrimary, L"No account selected");
    SetWindowTextW(g_centerSecondary, L"Pick an account in the sidebar to view it.");
    SetVisible(g_openRoblox, false);
    SetVisible(g_removeAccount, false);
}

void ShowSelectedAccount() {
    int row = static_cast<int>(SendMessageW(g_accountList, LB_GETCURSEL, 0, 0));
    if (row == LB_ERR) {
        ShowEmptyAccountState();
        return;
    }

    LRESULT data = SendMessageW(g_accountList, LB_GETITEMDATA, row, 0);
    if (data == LB_ERR || data < 0 || static_cast<size_t>(data) >= g_accounts.size()) {
        ShowEmptyAccountState();
        return;
    }

    const auto& account = g_accounts[static_cast<size_t>(data)];
    SetWindowTextW(g_centerPrimary, account.username.c_str());

    std::wstring sub = account.displayName.empty()
        ? L"Local SplitBox profile"
        : account.displayName + L"  ·  Local SplitBox profile";
    SetWindowTextW(g_centerSecondary, sub.c_str());

    SetVisible(g_openRoblox, true);
    SetVisible(g_removeAccount, true);
}

void OpenRoblox() {
    ShellExecuteW(g_main, L"open", L"roblox://", nullptr, nullptr, SW_SHOWNORMAL);
}

void RemoveSelectedAccount() {
    int row = static_cast<int>(SendMessageW(g_accountList, LB_GETCURSEL, 0, 0));
    if (row == LB_ERR) return;

    LRESULT data = SendMessageW(g_accountList, LB_GETITEMDATA, row, 0);
    if (data == LB_ERR || data < 0 || static_cast<size_t>(data) >= g_accounts.size()) return;

    const auto& account = g_accounts[static_cast<size_t>(data)];
    std::wstring prompt =
        L"Remove " + account.username +
        L" from SplitBox?\n\nThis only removes the local SplitBox entry.";

    if (MessageBoxW(g_main, prompt.c_str(), L"Remove account", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    g_accounts.erase(g_accounts.begin() + data);
    SaveAccounts();
    RefreshAccountList();
    ShowEmptyAccountState();
}

struct AddDialogState {
    HWND username{};
    HWND display{};
    bool accepted{};
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

        MakeControl(0, L"STATIC", L"Username", WS_CHILD | WS_VISIBLE, 24, 25, 330, 22, hwnd, 0);
        state->username = MakeControl(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 24, 50, 330, 34, hwnd, ID_ADD_USERNAME);
        MakeControl(0, L"STATIC", L"Display name (optional)", WS_CHILD | WS_VISIBLE, 24, 100, 330, 22, hwnd, 0);
        state->display = MakeControl(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 24, 125, 330, 34, hwnd, ID_ADD_DISPLAY);
        MakeControl(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 24, 186, 140, 36, hwnd, ID_ADD_CANCEL);
        MakeControl(0, L"BUTTON", L"Add account", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 184, 186, 170, 36, hwnd, ID_ADD_OK);
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
    int width = child.right - child.left;
    int height = child.bottom - child.top;

    SetWindowPos(
        dlg,
        HWND_TOP,
        parent.left + ((parent.right - parent.left) - width) / 2,
        parent.top + ((parent.bottom - parent.top) - height) / 2,
        0, 0,
        SWP_NOSIZE | SWP_SHOWWINDOW
    );

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

    if (!state.accepted) return;

    auto exists = std::find_if(g_accounts.begin(), g_accounts.end(), [&](const Account& account) {
        return _wcsicmp(account.username.c_str(), state.account.username.c_str()) == 0;
    });

    if (exists != g_accounts.end()) {
        MessageBoxW(g_main, L"That account already exists in SplitBox.", L"SplitBox", MB_OK | MB_ICONINFORMATION);
        return;
    }

    g_accounts.push_back(std::move(state.account));
    SaveAccounts();
    RefreshAccountList();
}

void ResetCombo(HWND combo) {
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
}

void AddComboItem(HWND combo, const std::wstring& text, LPARAM data) {
    LRESULT row = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(combo, CB_SETITEMDATA, row, data);
}

int ComboSelection(HWND combo) {
    return static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
}

LPARAM ComboData(HWND combo) {
    int row = ComboSelection(combo);
    if (row == CB_ERR) return -1;
    return SendMessageW(combo, CB_GETITEMDATA, row, 0);
}

void RefreshSplitSelectors() {
    g_split.refreshWindows();
    g_split.refreshDevices();

    ResetCombo(g_p1Window);
    ResetCombo(g_p2Window);

    const auto& windows = g_split.windows();
    for (size_t i = 0; i < windows.size(); ++i) {
        AddComboItem(g_p1Window, windows[i].label, static_cast<LPARAM>(i));
        AddComboItem(g_p2Window, windows[i].label, static_cast<LPARAM>(i));
    }

    if (!windows.empty()) {
        SendMessageW(g_p1Window, CB_SETCURSEL, 0, 0);
        SendMessageW(g_p2Window, CB_SETCURSEL, windows.size() > 1 ? 1 : 0, 0);
    }

    ResetCombo(g_p1Keyboard);
    ResetCombo(g_p2Keyboard);
    const auto& keyboards = g_split.keyboards();
    for (size_t i = 0; i < keyboards.size(); ++i) {
        AddComboItem(g_p1Keyboard, keyboards[i].label, static_cast<LPARAM>(i));
        AddComboItem(g_p2Keyboard, keyboards[i].label, static_cast<LPARAM>(i));
    }
    if (!keyboards.empty()) {
        SendMessageW(g_p1Keyboard, CB_SETCURSEL, 0, 0);
        SendMessageW(g_p2Keyboard, CB_SETCURSEL, keyboards.size() > 1 ? 1 : 0, 0);
    }

    ResetCombo(g_p1Mouse);
    ResetCombo(g_p2Mouse);
    const auto& mice = g_split.mice();
    for (size_t i = 0; i < mice.size(); ++i) {
        AddComboItem(g_p1Mouse, mice[i].label, static_cast<LPARAM>(i));
        AddComboItem(g_p2Mouse, mice[i].label, static_cast<LPARAM>(i));
    }
    if (!mice.empty()) {
        SendMessageW(g_p1Mouse, CB_SETCURSEL, 0, 0);
        SendMessageW(g_p2Mouse, CB_SETCURSEL, mice.size() > 1 ? 1 : 0, 0);
    }

    std::wstring status =
        L"Detected " + std::to_wstring(windows.size()) + L" Roblox window(s), " +
        std::to_wstring(keyboards.size()) + L" keyboard(s), " +
        std::to_wstring(mice.size()) + L" mouse/mice.";
    SetWindowTextW(g_splitStatus, status.c_str());

    SetWindowTextW(g_inputStatus, g_split.inputStatus().c_str());
    UpdateGlobalStatus();
}

bool ReadSplitAssignments() {
    LPARAM p1Window = ComboData(g_p1Window);
    LPARAM p2Window = ComboData(g_p2Window);
    if (p1Window < 0 || p2Window < 0) {
        MessageBoxW(g_main, L"Launch two Roblox clients first, then press Refresh.", L"SplitBox", MB_OK | MB_ICONWARNING);
        return false;
    }

    if (p1Window == p2Window) {
        MessageBoxW(g_main, L"Player 1 and Player 2 must use different Roblox windows.", L"SplitBox", MB_OK | MB_ICONWARNING);
        return false;
    }

    HANDLE p1Keyboard{};
    HANDLE p2Keyboard{};
    HANDLE p1Mouse{};
    HANDLE p2Mouse{};

    LPARAM p1Kb = ComboData(g_p1Keyboard);
    LPARAM p2Kb = ComboData(g_p2Keyboard);
    LPARAM p1Ms = ComboData(g_p1Mouse);
    LPARAM p2Ms = ComboData(g_p2Mouse);

    if (p1Kb >= 0 && static_cast<size_t>(p1Kb) < g_split.keyboards().size()) p1Keyboard = g_split.keyboards()[static_cast<size_t>(p1Kb)].handle;
    if (p2Kb >= 0 && static_cast<size_t>(p2Kb) < g_split.keyboards().size()) p2Keyboard = g_split.keyboards()[static_cast<size_t>(p2Kb)].handle;
    if (p1Ms >= 0 && static_cast<size_t>(p1Ms) < g_split.mice().size()) p1Mouse = g_split.mice()[static_cast<size_t>(p1Ms)].handle;
    if (p2Ms >= 0 && static_cast<size_t>(p2Ms) < g_split.mice().size()) p2Mouse = g_split.mice()[static_cast<size_t>(p2Ms)].handle;

    g_split.setAssignments(p1Keyboard, p1Mouse, p2Keyboard, p2Mouse);

    bool experimental = SendMessageW(g_experimentalRouting, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_split.setExperimentalRouting(experimental);
    return true;
}

void StartSplitScreen() {
    if (!ReadSplitAssignments()) return;

    LPARAM p1Window = ComboData(g_p1Window);
    LPARAM p2Window = ComboData(g_p2Window);

    int layoutSelection = ComboSelection(g_splitLayout);
    SplitLayout layout = layoutSelection == 1 ? SplitLayout::TopBottom : SplitLayout::SideBySide;

    std::wstring error;
    if (!g_split.start(
        static_cast<size_t>(p1Window),
        static_cast<size_t>(p2Window),
        layout,
        error
    )) {
        MessageBoxW(g_main, error.c_str(), L"SplitBox", MB_OK | MB_ICONERROR);
        return;
    }

    SetWindowTextW(
        g_splitStatus,
        g_split.experimentalRouting()
            ? L"Split screen ACTIVE. Experimental per-device keyboard/button routing is enabled."
            : L"Split screen ACTIVE. Windows are isolated visually; Raw Input assignments are being monitored."
    );

    EnableWindow(g_splitStart, FALSE);
    EnableWindow(g_splitStop, TRUE);

    // Put the game windows above the manager without hiding the manager from Alt+Tab.
    SetWindowPos(g_main, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    UpdateGlobalStatus();
}

void StopSplitScreen() {
    g_split.stop();
    EnableWindow(g_splitStart, TRUE);
    EnableWindow(g_splitStop, FALSE);
    SetWindowTextW(g_splitStatus, L"Split screen stopped. Original Roblox window positions restored.");
    UpdateGlobalStatus();
}

void SetTabVisibility() {
    bool accounts = g_activeTab == ID_TAB_ACCOUNTS;
    bool split = g_activeTab == ID_TAB_SPLIT;
    bool placeholder = !accounts && !split;

    ShowGroup(g_accountControls, accounts);
    ShowGroup(g_splitControls, split);
    ShowGroup(g_placeholderControls, placeholder);

    if (accounts) {
        int row = static_cast<int>(SendMessageW(g_accountList, LB_GETCURSEL, 0, 0));
        SetVisible(g_openRoblox, row != LB_ERR);
        SetVisible(g_removeAccount, row != LB_ERR);
    }

    if (placeholder) {
        const wchar_t* section = L"Coming soon";
        switch (g_activeTab) {
        case ID_TAB_VIP: section = L"VIP Servers"; break;
        case ID_TAB_ASSIGNMENTS: section = L"VIP Assignments"; break;
        case ID_TAB_PRESETS: section = L"Presets"; break;
        case ID_TAB_REJOIN: section = L"Auto-Rejoin"; break;
        case ID_TAB_SETTINGS: section = L"Settings"; break;
        default: break;
        }

        SetWindowTextW(g_placeholderTitle, section);
        SetWindowTextW(g_placeholderText, L"This area is reserved for the next SplitBox milestone.");
    }

    for (HWND tab : g_tabs) InvalidateRect(tab, nullptr, TRUE);
}

void SwitchTab(int id) {
    g_activeTab = id;
    SetTabVisibility();
    if (id == ID_TAB_SPLIT) RefreshSplitSelectors();
}

void Layout(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    int W = rc.right;
    int H = rc.bottom;

    constexpr int top = 42;
    constexpr int sidebar = 330;

    int tabX = 10;
    const int widths[] = {118, 118, 155, 105, 130, 125, 105};
    for (size_t i = 0; i < g_tabs.size(); ++i) {
        MoveWindow(g_tabs[i], tabX, 4, widths[i], 34, TRUE);
        tabX += widths[i] + 4;
    }

    MoveWindow(g_count, W - 160, 9, 140, 24, TRUE);
    MoveWindow(g_globalStatus, 12, H - 31, W - 24, 20, TRUE);

    // Accounts tab.
    MoveWindow(g_accountTitle, 12, top + 10, 300, 34, TRUE);
    MoveWindow(g_search, 12, top + 54, sidebar - 24, 34, TRUE);
    MoveWindow(g_addAccount, 12, top + 96, 150, 34, TRUE);
    MoveWindow(g_accountList, 12, top + 142, sidebar - 24, H - top - 190, TRUE);

    int centerX = sidebar;
    int centerW = W - sidebar;
    MoveWindow(g_centerPrimary, centerX + 40, H / 2 - 60, centerW - 80, 45, TRUE);
    MoveWindow(g_centerSecondary, centerX + 40, H / 2 - 10, centerW - 80, 30, TRUE);
    MoveWindow(g_openRoblox, centerX + centerW / 2 - 150, H / 2 + 50, 140, 38, TRUE);
    MoveWindow(g_removeAccount, centerX + centerW / 2 + 10, H / 2 + 50, 140, 38, TRUE);

    // Placeholder tabs.
    MoveWindow(g_placeholderTitle, 60, H / 2 - 60, W - 120, 45, TRUE);
    MoveWindow(g_placeholderText, 60, H / 2 - 5, W - 120, 30, TRUE);

    // Split tab.
    int left = 40;
    int right = W - 40;
    int half = W / 2;
    int comboW = std::max(280, half - 110);

    MoveWindow(g_splitTitle, left, top + 20, W - 80, 40, TRUE);
    MoveWindow(g_splitHelp, left, top + 62, W - 80, 46, TRUE);
    MoveWindow(g_splitRefresh, right - 150, top + 112, 150, 34, TRUE);

    int y = top + 160;
    const int rowGap = 62;

    // Labels are positioned when created and remain in the two columns; combos move here.
    MoveWindow(g_p1Window, left, y + 24, comboW, 250, TRUE);
    MoveWindow(g_p2Window, half + 20, y + 24, comboW, 250, TRUE);

    y += rowGap;
    MoveWindow(g_p1Keyboard, left, y + 24, comboW, 250, TRUE);
    MoveWindow(g_p2Keyboard, half + 20, y + 24, comboW, 250, TRUE);

    y += rowGap;
    MoveWindow(g_p1Mouse, left, y + 24, comboW, 250, TRUE);
    MoveWindow(g_p2Mouse, half + 20, y + 24, comboW, 250, TRUE);

    y += rowGap + 12;
    MoveWindow(g_splitLayout, left, y + 24, 250, 200, TRUE);
    MoveWindow(g_experimentalRouting, left + 280, y + 22, W - left - 320, 48, TRUE);

    y += 76;
    MoveWindow(g_splitStart, left, y, 180, 42, TRUE);
    MoveWindow(g_splitStop, left + 195, y, 180, 42, TRUE);
    MoveWindow(g_splitStatus, left + 400, y - 4, W - left - 440, 46, TRUE);

    y += 58;
    MoveWindow(g_inputStatus, left, y, W - 80, 26, TRUE);
}

HWND MakeLabel(const wchar_t* text, int x, int y, int w, int h, std::vector<HWND>& group, HFONT font = nullptr) {
    HWND label = MakeControl(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, g_main, 0, font);
    group.push_back(label);
    return label;
}

void CreateAccountUi(HWND hwnd) {
    g_accountTitle = MakeControl(0, L"STATIC", L"Accounts", WS_CHILD | WS_VISIBLE, 12, 52, 300, 34, hwnd, 0, g_fontLarge);
    g_search = MakeControl(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 12, 96, 300, 34, hwnd, ID_SEARCH);
    SendMessageW(g_search, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Search accounts..."));

    g_addAccount = MakeControl(0, L"BUTTON", L"+  Add Account", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 12, 138, 150, 34, hwnd, ID_ADD_ACCOUNT);

    g_accountList = MakeControl(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
        12, 184, 300, 500,
        hwnd,
        ID_ACCOUNT_LIST
    );

    g_centerPrimary = MakeControl(0, L"STATIC", L"No account selected", WS_CHILD | WS_VISIBLE | SS_CENTER, 350, 300, 500, 45, hwnd, 0, g_fontLarge);
    g_centerSecondary = MakeControl(0, L"STATIC", L"Pick an account in the sidebar to view it.", WS_CHILD | WS_VISIBLE | SS_CENTER, 350, 350, 500, 30, hwnd, 0);
    g_openRoblox = MakeControl(0, L"BUTTON", L"Open Roblox", WS_CHILD | BS_PUSHBUTTON, 500, 420, 140, 38, hwnd, ID_OPEN_ROBLOX);
    g_removeAccount = MakeControl(0, L"BUTTON", L"Remove", WS_CHILD | BS_PUSHBUTTON, 655, 420, 140, 38, hwnd, ID_REMOVE_ACCOUNT);

    g_accountControls = {
        g_accountTitle, g_search, g_addAccount, g_accountList,
        g_centerPrimary, g_centerSecondary, g_openRoblox, g_removeAccount
    };
}

void CreateSplitUi(HWND hwnd) {
    g_splitTitle = MakeControl(
        0, L"STATIC", L"Split Screen", WS_CHILD | SS_LEFT,
        40, 65, 500, 40, hwnd, 0, g_fontLarge
    );

    g_splitHelp = MakeControl(
        0, L"STATIC",
        L"Select two running Roblox clients. SplitBox can tile them borderlessly and identify separate physical keyboards/mice using Windows Raw Input.",
        WS_CHILD | SS_LEFT,
        40, 105, 900, 46, hwnd, 0
    );

    g_splitRefresh = MakeControl(0, L"BUTTON", L"Refresh devices", WS_CHILD | BS_PUSHBUTTON, 1000, 150, 150, 34, hwnd, ID_SPLIT_REFRESH);

    // Labels are manually added to split group.
    auto add = [&](HWND h) { g_splitControls.push_back(h); return h; };
    add(g_splitTitle);
    add(g_splitHelp);
    add(g_splitRefresh);

    add(MakeControl(0, L"STATIC", L"PLAYER 1", WS_CHILD, 40, 190, 300, 28, hwnd, 0, g_fontSection));
    add(MakeControl(0, L"STATIC", L"PLAYER 2", WS_CHILD, 750, 190, 300, 28, hwnd, 0, g_fontSection));

    add(MakeControl(0, L"STATIC", L"Roblox window", WS_CHILD, 40, 225, 300, 22, hwnd, 0));
    add(MakeControl(0, L"STATIC", L"Roblox window", WS_CHILD, 750, 225, 300, 22, hwnd, 0));
    g_p1Window = add(MakeControl(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 40, 250, 500, 250, hwnd, ID_P1_WINDOW));
    g_p2Window = add(MakeControl(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 750, 250, 500, 250, hwnd, ID_P2_WINDOW));

    add(MakeControl(0, L"STATIC", L"Keyboard", WS_CHILD, 40, 287, 300, 22, hwnd, 0));
    add(MakeControl(0, L"STATIC", L"Keyboard", WS_CHILD, 750, 287, 300, 22, hwnd, 0));
    g_p1Keyboard = add(MakeControl(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 40, 312, 500, 250, hwnd, ID_P1_KEYBOARD));
    g_p2Keyboard = add(MakeControl(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 750, 312, 500, 250, hwnd, ID_P2_KEYBOARD));

    add(MakeControl(0, L"STATIC", L"Mouse", WS_CHILD, 40, 349, 300, 22, hwnd, 0));
    add(MakeControl(0, L"STATIC", L"Mouse", WS_CHILD, 750, 349, 300, 22, hwnd, 0));
    g_p1Mouse = add(MakeControl(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 40, 374, 500, 250, hwnd, ID_P1_MOUSE));
    g_p2Mouse = add(MakeControl(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 750, 374, 500, 250, hwnd, ID_P2_MOUSE));

    add(MakeControl(0, L"STATIC", L"Layout", WS_CHILD, 40, 435, 300, 22, hwnd, 0));
    g_splitLayout = add(MakeControl(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST, 40, 460, 250, 180, hwnd, ID_SPLIT_LAYOUT));
    SendMessageW(g_splitLayout, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Side by side"));
    SendMessageW(g_splitLayout, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Top / bottom"));
    SendMessageW(g_splitLayout, CB_SETCURSEL, 0, 0);

    g_experimentalRouting = add(MakeControl(
        0, L"BUTTON",
        L"Experimental per-device input routing (keyboard + mouse buttons; mouse-look remains focus-sensitive)",
        WS_CHILD | BS_AUTOCHECKBOX,
        320, 458, 760, 48,
        hwnd,
        ID_EXPERIMENTAL_ROUTING
    ));

    g_splitStart = add(MakeControl(0, L"BUTTON", L"Start Split Screen", WS_CHILD | BS_PUSHBUTTON, 40, 535, 180, 42, hwnd, ID_SPLIT_START));
    g_splitStop = add(MakeControl(0, L"BUTTON", L"Stop / Restore", WS_CHILD | BS_PUSHBUTTON, 235, 535, 180, 42, hwnd, ID_SPLIT_STOP));
    EnableWindow(g_splitStop, FALSE);

    g_splitStatus = add(MakeControl(
        0, L"STATIC",
        L"Press Refresh after both Roblox clients are open.",
        WS_CHILD | SS_LEFT,
        440, 535, 750, 46,
        hwnd,
        0
    ));

    g_inputStatus = add(MakeControl(0, L"STATIC", L"Raw input: P1 0 events  ·  P2 0 events", WS_CHILD | SS_LEFT, 40, 595, 900, 26, hwnd, 0, g_fontSmall));
}

void CreatePlaceholderUi(HWND hwnd) {
    g_placeholderTitle = MakeControl(0, L"STATIC", L"Coming soon", WS_CHILD | SS_CENTER, 60, 300, 800, 45, hwnd, 0, g_fontLarge);
    g_placeholderText = MakeControl(0, L"STATIC", L"This area is reserved for the next SplitBox milestone.", WS_CHILD | SS_CENTER, 60, 355, 800, 30, hwnd, 0);
    g_placeholderControls = {g_placeholderTitle, g_placeholderText};
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
        HWND tab = MakeControl(
            0, L"BUTTON", tabNames[i],
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 100, 34,
            hwnd,
            tabIds[i]
        );
        g_tabs.push_back(tab);
    }

    g_count = MakeControl(0, L"STATIC", L"0 account(s)", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0, 140, 24, hwnd, 0);
    g_globalStatus = MakeControl(0, L"STATIC", L"Roblox windows: 0  ·  Split screen: off", WS_CHILD | WS_VISIBLE, 12, 700, 700, 20, hwnd, 0, g_fontSmall);

    CreateAccountUi(hwnd);
    CreateSplitUi(hwnd);
    CreatePlaceholderUi(hwnd);

    LoadAccounts();
    RefreshAccountList();
    ShowEmptyAccountState();

    g_split.registerRawInput(hwnd);
    RefreshSplitSelectors();
    SetTabVisibility();
}

void PaintMain(HWND hwnd, HDC dc) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    FillRect(dc, &rc, g_bgBrush);

    RECT top{0, 0, rc.right, 42};
    FillRect(dc, &top, g_panelBrush);

    if (g_activeTab == ID_TAB_ACCOUNTS) {
        RECT sidebar{0, 42, 330, rc.bottom};
        FillRect(dc, &sidebar, g_panelBrush);

        HPEN pen = CreatePen(PS_SOLID, 1, RGB(58, 58, 58));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        MoveToEx(dc, 329, 42, nullptr);
        LineTo(dc, 329, rc.bottom);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }

    HPEN line = CreatePen(PS_SOLID, 1, RGB(58, 58, 58));
    HGDIOBJ old = SelectObject(dc, line);
    MoveToEx(dc, 0, 41, nullptr);
    LineTo(dc, rc.right, 41);
    SelectObject(dc, old);
    DeleteObject(line);
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
    RECT rect = dis->rcItem;
    DrawTextW(dis->hDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return TRUE;
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_main = hwnd;
        ApplyDarkTitleBar(hwnd);
        CreateUi(hwnd);
        SetTimer(hwnd, 1, 1000, nullptr);
        return 0;

    case WM_SIZE:
        Layout(hwnd);
        return 0;

    case WM_TIMER:
        if (wParam == 1) UpdateGlobalStatus();
        return 0;

    case WM_INPUT:
        g_split.handleRawInput(lParam);
        if (g_split.active()) SetWindowTextW(g_inputStatus, g_split.inputStatus().c_str());
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

        case ID_SPLIT_REFRESH:
            RefreshSplitSelectors();
            return 0;

        case ID_SPLIT_START:
            StartSplitScreen();
            return 0;

        case ID_SPLIT_STOP:
            StopSplitScreen();
            return 0;
        }

        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND child = reinterpret_cast<HWND>(lParam);

        SetBkMode(dc, TRANSPARENT);
        if (
            child == g_globalStatus ||
            child == g_count ||
            child == g_centerSecondary ||
            child == g_placeholderText ||
            child == g_splitHelp ||
            child == g_splitStatus ||
            child == g_inputStatus
        ) {
            SetTextColor(dc, C_MUTED);
        } else {
            SetTextColor(dc, C_TEXT);
        }

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

    case WM_CLOSE:
        if (g_split.active()) g_split.stop();
        DestroyWindow(hwnd);
        return 0;

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

    lf.lfHeight = -20;
    lf.lfWeight = FW_SEMIBOLD;
    g_fontSection = CreateFontIndirectW(&lf);
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
    DeleteObject(g_fontSection);
    DeleteObject(g_bgBrush);
    DeleteObject(g_panelBrush);
    DeleteObject(g_controlBrush);
    DeleteObject(g_accentBrush);

    return static_cast<int>(msg.wParam);
}
