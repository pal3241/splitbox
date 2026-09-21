#include "split_screen.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <vector>

namespace splitbox {

namespace {

std::wstring HexHandle(HANDLE handle) {
    std::wstringstream ss;
    ss << L"0x" << std::hex << reinterpret_cast<UINT_PTR>(handle);
    return ss.str();
}

} // namespace

bool SplitScreenManager::isRobloxProcess(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;

    wchar_t path[32768]{};
    DWORD size = static_cast<DWORD>(std::size(path));
    bool result = false;

    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        const wchar_t* file = wcsrchr(path, L'\\');
        file = file ? file + 1 : path;
        result = _wcsicmp(file, L"RobloxPlayerBeta.exe") == 0;
    }

    CloseHandle(process);
    return result;
}

std::wstring SplitScreenManager::windowTitle(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return L"Roblox";
    std::wstring title(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(hwnd, title.data(), len + 1);
    title.resize(static_cast<size_t>(len));
    return title;
}

void SplitScreenManager::refreshWindows() {
    windows_.clear();

    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) return TRUE;
        if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

        DWORD pid{};
        GetWindowThreadProcessId(hwnd, &pid);
        if (!pid || !SplitScreenManager::isRobloxProcess(pid)) return TRUE;

        RECT rc{};
        if (!GetWindowRect(hwnd, &rc)) return TRUE;
        if ((rc.right - rc.left) < 100 || (rc.bottom - rc.top) < 100) return TRUE;

        auto* list = reinterpret_cast<std::vector<RobloxWindow>*>(param);
        RobloxWindow w;
        w.hwnd = hwnd;
        w.pid = pid;
        w.title = SplitScreenManager::windowTitle(hwnd);
        w.label = L"PID " + std::to_wstring(pid) + L"  ·  " + w.title;
        list->push_back(std::move(w));
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windows_));

    std::sort(windows_.begin(), windows_.end(), [](const RobloxWindow& a, const RobloxWindow& b) {
        return a.pid < b.pid;
    });
}

std::wstring SplitScreenManager::deviceName(HANDLE handle) {
    UINT chars = 0;
    if (GetRawInputDeviceInfoW(handle, RIDI_DEVICENAME, nullptr, &chars) == static_cast<UINT>(-1) || chars == 0) {
        return L"";
    }

    std::wstring name(chars, L'\0');
    if (GetRawInputDeviceInfoW(handle, RIDI_DEVICENAME, name.data(), &chars) == static_cast<UINT>(-1)) {
        return L"";
    }

    while (!name.empty() && name.back() == L'\0') name.pop_back();
    return name;
}

std::wstring SplitScreenManager::friendlyDeviceLabel(HANDLE handle, DWORD type, size_t ordinal) {
    std::wstring path = deviceName(handle);
    std::wstring kind = type == RIM_TYPEKEYBOARD ? L"Keyboard " : L"Mouse ";
    std::wstring label = kind + std::to_wstring(ordinal + 1);

    if (!path.empty()) {
        auto pos = path.find(L"VID_");
        if (pos != std::wstring::npos) {
            auto end = path.find(L"#{", pos);
            std::wstring id = path.substr(pos, end == std::wstring::npos ? 24 : std::min<size_t>(24, end - pos));
            std::replace(id.begin(), id.end(), L'&', L' ');
            label += L"  ·  " + id;
        } else {
            label += L"  ·  " + HexHandle(handle);
        }
    }
    return label;
}

void SplitScreenManager::refreshDevices() {
    keyboards_.clear();
    mice_.clear();

    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0) return;

    std::vector<RAWINPUTDEVICELIST> devices(count);
    UINT read = GetRawInputDeviceList(devices.data(), &count, sizeof(RAWINPUTDEVICELIST));
    if (read == static_cast<UINT>(-1)) return;

    size_t keyboardOrdinal = 0;
    size_t mouseOrdinal = 0;

    for (UINT i = 0; i < read; ++i) {
        const auto& raw = devices[i];
        if (raw.dwType != RIM_TYPEKEYBOARD && raw.dwType != RIM_TYPEMOUSE) continue;

        InputDevice d;
        d.handle = raw.hDevice;
        d.type = raw.dwType;
        d.path = deviceName(raw.hDevice);

        if (raw.dwType == RIM_TYPEKEYBOARD) {
            d.label = friendlyDeviceLabel(raw.hDevice, raw.dwType, keyboardOrdinal++);
            keyboards_.push_back(std::move(d));
        } else {
            d.label = friendlyDeviceLabel(raw.hDevice, raw.dwType, mouseOrdinal++);
            mice_.push_back(std::move(d));
        }
    }
}

bool SplitScreenManager::registerRawInput(HWND sink) {
    RAWINPUTDEVICE rid[2]{};

    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x02;
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = sink;

    rid[1].usUsagePage = 0x01;
    rid[1].usUsage = 0x06;
    rid[1].dwFlags = RIDEV_INPUTSINK;
    rid[1].hwndTarget = sink;

    return RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)) == TRUE;
}

void SplitScreenManager::setAssignments(HANDLE p1Keyboard, HANDLE p1Mouse, HANDLE p2Keyboard, HANDLE p2Mouse) {
    p1Keyboard_ = p1Keyboard;
    p1Mouse_ = p1Mouse;
    p2Keyboard_ = p2Keyboard;
    p2Mouse_ = p2Mouse;
}

void SplitScreenManager::makeBorderless(HWND hwnd, const RECT& target) {
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_POPUP;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

    SetWindowPos(
        hwnd,
        HWND_TOP,
        target.left,
        target.top,
        target.right - target.left,
        target.bottom - target.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW
    );
}

void SplitScreenManager::restoreWindow(const WindowState& state) {
    if (!state.valid || !IsWindow(state.hwnd)) return;

    SetWindowLongPtrW(state.hwnd, GWL_STYLE, state.style);
    SetWindowLongPtrW(state.hwnd, GWL_EXSTYLE, state.exStyle);
    SetWindowPlacement(state.hwnd, &state.placement);
    SetWindowPos(
        state.hwnd,
        nullptr,
        state.rect.left,
        state.rect.top,
        state.rect.right - state.rect.left,
        state.rect.bottom - state.rect.top,
        SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW
    );
}

bool SplitScreenManager::start(size_t p1WindowIndex, size_t p2WindowIndex, SplitLayout layout, std::wstring& error) {
    if (active_) stop();
    refreshWindows();

    if (p1WindowIndex >= windows_.size() || p2WindowIndex >= windows_.size()) {
        error = L"Select two valid Roblox windows.";
        return false;
    }

    if (p1WindowIndex == p2WindowIndex || windows_[p1WindowIndex].hwnd == windows_[p2WindowIndex].hwnd) {
        error = L"Player 1 and Player 2 must use different Roblox windows.";
        return false;
    }

    p1Window_ = windows_[p1WindowIndex].hwnd;
    p2Window_ = windows_[p2WindowIndex].hwnd;

    auto captureState = [](HWND hwnd) {
        WindowState s;
        s.hwnd = hwnd;
        s.style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        s.exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        s.placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hwnd, &s.placement);
        GetWindowRect(hwnd, &s.rect);
        s.valid = true;
        return s;
    };

    p1State_ = captureState(p1Window_);
    p2State_ = captureState(p2Window_);

    HMONITOR monitor = MonitorFromWindow(p1Window_, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{sizeof(mi)};
    if (!GetMonitorInfoW(monitor, &mi)) {
        error = L"Could not read monitor work area.";
        p1State_ = {};
        p2State_ = {};
        return false;
    }

    RECT work = mi.rcWork;
    RECT first = work;
    RECT second = work;

    if (layout == SplitLayout::SideBySide) {
        LONG mid = work.left + (work.right - work.left) / 2;
        first.right = mid;
        second.left = mid;
    } else {
        LONG mid = work.top + (work.bottom - work.top) / 2;
        first.bottom = mid;
        second.top = mid;
    }

    ShowWindow(p1Window_, SW_RESTORE);
    ShowWindow(p2Window_, SW_RESTORE);
    makeBorderless(p1Window_, first);
    makeBorderless(p2Window_, second);

    active_ = true;
    p1Events_ = 0;
    p2Events_ = 0;
    return true;
}

void SplitScreenManager::stop() {
    if (!active_) return;
    restoreWindow(p1State_);
    restoreWindow(p2State_);
    active_ = false;
    p1Window_ = nullptr;
    p2Window_ = nullptr;
    p1State_ = {};
    p2State_ = {};
}

HWND SplitScreenManager::targetForDevice(HANDLE device, DWORD type) const {
    if (!device) return nullptr;
    if (type == RIM_TYPEKEYBOARD) {
        if (device == p1Keyboard_) return p1Window_;
        if (device == p2Keyboard_) return p2Window_;
    } else if (type == RIM_TYPEMOUSE) {
        if (device == p1Mouse_) return p1Window_;
        if (device == p2Mouse_) return p2Window_;
    }
    return nullptr;
}

void SplitScreenManager::routeKeyboard(HWND target, const RAWKEYBOARD& keyboard) {
    if (!target || !experimentalRouting_) return;

    UINT msg = (keyboard.Flags & RI_KEY_BREAK) ? WM_KEYUP : WM_KEYDOWN;
    UINT vk = keyboard.VKey;
    UINT scan = keyboard.MakeCode;

    LPARAM lp = 1 | (static_cast<LPARAM>(scan) << 16);
    if (keyboard.Flags & RI_KEY_E0) lp |= (1LL << 24);
    if (msg == WM_KEYUP) lp |= (1LL << 30) | (1LL << 31);

    PostMessageW(target, msg, vk, lp);
}

void SplitScreenManager::routeMouse(HWND target, const RAWMOUSE& mouse) {
    if (!target || !experimentalRouting_) return;

    // Window messages can route buttons to a background client. Roblox mouse-look
    // is raw-input/focus sensitive, so relative motion is deliberately not forged here.
    POINT screen{};
    GetCursorPos(&screen);
    POINT client = screen;
    ScreenToClient(target, &client);
    LPARAM pos = MAKELPARAM(static_cast<short>(client.x), static_cast<short>(client.y));

    USHORT flags = mouse.usButtonFlags;
    if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) PostMessageW(target, WM_LBUTTONDOWN, MK_LBUTTON, pos);
    if (flags & RI_MOUSE_LEFT_BUTTON_UP) PostMessageW(target, WM_LBUTTONUP, 0, pos);
    if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) PostMessageW(target, WM_RBUTTONDOWN, MK_RBUTTON, pos);
    if (flags & RI_MOUSE_RIGHT_BUTTON_UP) PostMessageW(target, WM_RBUTTONUP, 0, pos);
    if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) PostMessageW(target, WM_MBUTTONDOWN, MK_MBUTTON, pos);
    if (flags & RI_MOUSE_MIDDLE_BUTTON_UP) PostMessageW(target, WM_MBUTTONUP, 0, pos);

    if (flags & RI_MOUSE_WHEEL) {
        SHORT delta = static_cast<SHORT>(mouse.usButtonData);
        PostMessageW(target, WM_MOUSEWHEEL, MAKEWPARAM(0, delta), MAKELPARAM(screen.x, screen.y));
    }
}

void SplitScreenManager::handleRawInput(LPARAM lParam) {
    UINT size = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || !size) {
        return;
    }

    std::vector<BYTE> buffer(size);
    if (GetRawInputData(
        reinterpret_cast<HRAWINPUT>(lParam),
        RID_INPUT,
        buffer.data(),
        &size,
        sizeof(RAWINPUTHEADER)
    ) != size) {
        return;
    }

    const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(buffer.data());
    if (!raw->header.hDevice) return;

    HWND target = targetForDevice(raw->header.hDevice, raw->header.dwType);
    if (!target) return;

    if (target == p1Window_) ++p1Events_;
    if (target == p2Window_) ++p2Events_;

    if (!active_) return;

    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        routeKeyboard(target, raw->data.keyboard);
    } else if (raw->header.dwType == RIM_TYPEMOUSE) {
        routeMouse(target, raw->data.mouse);
    }
}

std::wstring SplitScreenManager::inputStatus() const {
    return L"Raw input: P1 " + std::to_wstring(p1Events_) + L" events  ·  P2 " + std::to_wstring(p2Events_) + L" events";
}

} // namespace splitbox
