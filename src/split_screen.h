#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace splitbox {

struct RobloxWindow {
    HWND hwnd{};
    DWORD pid{};
    std::wstring title;
    std::wstring label;
};

struct InputDevice {
    HANDLE handle{};
    DWORD type{};
    std::wstring path;
    std::wstring label;
};

enum class SplitLayout {
    SideBySide = 0,
    TopBottom = 1
};

class SplitScreenManager {
public:
    void refreshWindows();
    void refreshDevices();

    const std::vector<RobloxWindow>& windows() const { return windows_; }
    const std::vector<InputDevice>& keyboards() const { return keyboards_; }
    const std::vector<InputDevice>& mice() const { return mice_; }

    bool registerRawInput(HWND sink);
    void setAssignments(HANDLE p1Keyboard, HANDLE p1Mouse, HANDLE p2Keyboard, HANDLE p2Mouse);
    void setExperimentalRouting(bool enabled) { experimentalRouting_ = enabled; }
    bool experimentalRouting() const { return experimentalRouting_; }

    bool start(size_t p1WindowIndex, size_t p2WindowIndex, SplitLayout layout, std::wstring& error);
    void stop();
    bool active() const { return active_; }

    void handleRawInput(LPARAM lParam);
    std::wstring inputStatus() const;

private:
    struct WindowState {
        HWND hwnd{};
        LONG_PTR style{};
        LONG_PTR exStyle{};
        WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
        RECT rect{};
        bool valid{};
    };

    static bool isRobloxProcess(DWORD pid);
    static std::wstring windowTitle(HWND hwnd);
    static std::wstring deviceName(HANDLE handle);
    static std::wstring friendlyDeviceLabel(HANDLE handle, DWORD type, size_t ordinal);
    static void makeBorderless(HWND hwnd, const RECT& target);
    static void restoreWindow(const WindowState& state);

    HWND targetForDevice(HANDLE device, DWORD type) const;
    void routeKeyboard(HWND target, const RAWKEYBOARD& keyboard);
    void routeMouse(HWND target, const RAWMOUSE& mouse);

    std::vector<RobloxWindow> windows_;
    std::vector<InputDevice> keyboards_;
    std::vector<InputDevice> mice_;

    WindowState p1State_{};
    WindowState p2State_{};
    HWND p1Window_{};
    HWND p2Window_{};

    HANDLE p1Keyboard_{};
    HANDLE p1Mouse_{};
    HANDLE p2Keyboard_{};
    HANDLE p2Mouse_{};

    bool active_{};
    bool experimentalRouting_{};
    ULONGLONG p1Events_{};
    ULONGLONG p2Events_{};
};

} // namespace splitbox
