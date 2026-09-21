#include "openmouse_bridge.h"

#include <shellapi.h>
#include <shlobj.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace splitbox {

namespace {
constexpr wchar_t kEngineWindowClass[] = L"openMouse.Engine";
}

std::filesystem::path OpenMouseBridge::moduleDirectory() {
    std::vector<wchar_t> path(32768);
    DWORD len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!len || len >= path.size()) return std::filesystem::current_path();
    return std::filesystem::path(std::wstring(path.data(), len)).parent_path();
}

std::filesystem::path OpenMouseBridge::executablePath() const {
    return moduleDirectory() / L"openmouse.exe";
}

std::filesystem::path OpenMouseBridge::configPath() const {
    PWSTR roaming = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming))) {
        result = std::filesystem::path(roaming) / L"openMouse" / L"openmouse.ini";
        CoTaskMemFree(roaming);
    } else {
        result = moduleDirectory() / L"openmouse.ini";
    }
    return result;
}

bool OpenMouseBridge::available() const {
    std::error_code ec;
    return std::filesystem::is_regular_file(executablePath(), ec);
}

bool OpenMouseBridge::running() const {
    return FindWindowW(kEngineWindowClass, nullptr) != nullptr;
}

bool OpenMouseBridge::launch(
    const std::wstring& args,
    bool wait,
    bool newConsole,
    DWORD* exitCode,
    std::wstring& error
) const {
    const auto exe = executablePath();
    if (!available()) {
        error = L"openmouse.exe was not found next to SplitBox.exe.";
        return false;
    }

    std::wstring command = L"\"" + exe.wstring() + L"\"";
    if (!args.empty()) command += L" " + args;

    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    DWORD flags = newConsole ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;
    std::wstring cwd = exe.parent_path().wstring();

    if (!CreateProcessW(
        exe.c_str(),
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        flags,
        nullptr,
        cwd.c_str(),
        &si,
        &pi
    )) {
        error = L"Could not start openMouse. Windows error " + std::to_wstring(GetLastError()) + L".";
        return false;
    }

    CloseHandle(pi.hThread);

    if (wait) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        if (exitCode) *exitCode = code;
    }

    CloseHandle(pi.hProcess);
    return true;
}

bool OpenMouseBridge::start(std::wstring& error) {
    if (running()) return true;

    DWORD unused = 0;
    if (!launch(L"", false, false, &unused, error)) return false;

    for (int i = 0; i < 30; ++i) {
        Sleep(100);
        if (running()) return true;
    }

    error = L"openMouse started but its engine window did not appear. Run Probe from SplitBox and inspect %APPDATA%\\openMouse\\openmouse.log.";
    return false;
}

bool OpenMouseBridge::stop(std::wstring& error) {
    HWND engine = FindWindowW(kEngineWindowClass, nullptr);
    if (!engine) return true;

    if (!PostMessageW(engine, WM_CLOSE, 0, 0)) {
        error = L"Could not ask openMouse to stop. Windows error " + std::to_wstring(GetLastError()) + L".";
        return false;
    }

    for (int i = 0; i < 30; ++i) {
        Sleep(100);
        if (!running()) return true;
    }

    error = L"openMouse did not stop in time. Use Ctrl+Alt+Shift+Q, its built-in panic shortcut.";
    return false;
}

bool OpenMouseBridge::identify(std::wstring& error) {
    if (running()) {
        std::wstring stopError;
        if (!stop(stopError)) {
            error = L"Stop openMouse before identification: " + stopError;
            return false;
        }
    }

    DWORD code = 0;
    if (!launch(L"--identify", false, true, &code, error)) return false;
    return true;
}

bool OpenMouseBridge::probe(std::wstring& error) {
    if (running()) {
        error = L"Stop openMouse before running the probe.";
        return false;
    }
    DWORD code = 0;
    return launch(L"--probe", false, true, &code, error);
}

bool OpenMouseBridge::probeEcho(std::wstring& error) {
    if (running()) {
        error = L"Stop openMouse before running the echo probe.";
        return false;
    }
    DWORD code = 0;
    return launch(L"--probe-echo", false, true, &code, error);
}

bool OpenMouseBridge::openConfigFolder(std::wstring& error) {
    auto folder = configPath().parent_path();
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);

    auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL)
    );
    if (result <= 32) {
        error = L"Could not open the openMouse config folder.";
        return false;
    }
    return true;
}

bool OpenMouseBridge::readTextFile(const std::filesystem::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool OpenMouseBridge::writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
}

bool OpenMouseBridge::setKeyboardSteering(bool enabled, std::wstring& error) {
    const auto path = configPath();
    std::string text;

    if (!readTextFile(path, text)) {
        text =
            "; openMouse configuration generated by SplitBox\\n"
            "[general]\\n"
            "seats=2\\n"
            "capture_keyboards=" + std::string(enabled ? "1\\n" : "0\\n") +
            "mark_active_seat=1\\n\\n"
            "[seat0]\\n"
            "color=FFFFFF\\n"
            "sensitivity=1.00\\n\\n"
            "[seat1]\\n"
            "color=000000\\n"
            "sensitivity=1.00\\n\\n"
            "[mouse]\\n\\n"
            "[keyboard]\\n";

        if (!writeTextFile(path, text)) {
            error = L"Could not create " + path.wstring();
            return false;
        }
        return true;
    }

    const std::string key = "capture_keyboards=";
    size_t pos = text.find(key);
    if (pos != std::string::npos) {
        size_t end = text.find_first_of("\r\n", pos);
        text.replace(pos, (end == std::string::npos ? text.size() : end) - pos,
                     key + std::string(enabled ? "1" : "0"));
    } else {
        size_t general = text.find("[general]");
        if (general != std::string::npos) {
            size_t insertAt = text.find('\n', general);
            if (insertAt == std::string::npos) insertAt = text.size();
            else ++insertAt;
            text.insert(insertAt, key + std::string(enabled ? "1\n" : "0\n"));
        } else {
            text = "[general]\nseats=2\n" + key + std::string(enabled ? "1\n" : "0\n") +
                   "mark_active_seat=1\n\n" + text;
        }
    }

    if (!writeTextFile(path, text)) {
        error = L"Could not update " + path.wstring();
        return false;
    }
    return true;
}

bool OpenMouseBridge::keyboardSteeringEnabled() const {
    std::string text;
    if (!readTextFile(configPath(), text)) return false;

    const std::string key = "capture_keyboards=";
    size_t pos = text.find(key);
    if (pos == std::string::npos) return false;
    pos += key.size();
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) ++pos;
    return pos < text.size() && text[pos] == '1';
}

} // namespace splitbox
