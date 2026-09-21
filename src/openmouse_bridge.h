#pragma once
#include <windows.h>
#include <filesystem>
#include <string>

namespace splitbox {

class OpenMouseBridge {
public:
    bool available() const;
    bool running() const;

    bool start(std::wstring& error);
    bool stop(std::wstring& error);
    bool identify(std::wstring& error);
    bool probe(std::wstring& error);
    bool probeEcho(std::wstring& error);
    bool openConfigFolder(std::wstring& error);

    bool setKeyboardSteering(bool enabled, std::wstring& error);
    bool keyboardSteeringEnabled() const;

    std::filesystem::path executablePath() const;
    std::filesystem::path configPath() const;

private:
    bool launch(const std::wstring& args, bool wait, bool newConsole, DWORD* exitCode, std::wstring& error) const;
    static std::filesystem::path moduleDirectory();
    static bool readTextFile(const std::filesystem::path& path, std::string& out);
    static bool writeTextFile(const std::filesystem::path& path, const std::string& content);
};

} // namespace splitbox
