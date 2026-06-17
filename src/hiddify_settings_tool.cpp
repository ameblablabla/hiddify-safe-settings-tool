#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static const std::vector<std::string> kSensitiveKeyParts = {
    "profile", "profiles", "subscription", "config", "configs",
    "vless", "vmess", "trojan", "ss://", "hysteria", "tuic",
    "uuid", "password", "token", "secret", "private", "wireguard-config",
    "account-id", "access-token", "sub-link", "sub_url", "sub-url"
};

static const std::vector<std::string> kForbiddenContent = {
    "vless://", "vmess://", "trojan://", "ss://",
    "\"outbounds\"", "\"inbounds\"", "\"uuid\"", "\"public_key\"",
    "\"private_key\"", "\"short_id\"", "\"server_port\"",
    "xtls-rprx-vision", "reality", "wireguard"
};

std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), size);
    return out;
}

std::string narrow(const std::wstring& s) {
    if (s.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), size, nullptr, nullptr);
    return out;
}

std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool containsAny(const std::string& text, const std::vector<std::string>& needles) {
    std::string low = lowerCopy(text);
    for (const auto& needle : needles) {
        if (low.find(lowerCopy(needle)) != std::string::npos) return true;
    }
    return false;
}

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read file: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeFile(const fs::path& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write file: " + path.string());
    out.write(data.data(), (std::streamsize)data.size());
}

fs::path hiddifyDir() {
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) throw std::runtime_error("APPDATA is not set");
    return fs::path(appdata) / "Hiddify" / "hiddify";
}

fs::path sharedPrefsPath() {
    return hiddifyDir() / "shared_preferences.json";
}

std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
    return buf;
}

fs::path makeTempDir(const std::string& prefix) {
    fs::path base = fs::temp_directory_path() / (prefix + "-" + timestamp() + "-" + std::to_string(GetCurrentProcessId()));
    fs::create_directories(base);
    return base;
}

std::wstring quotePs(const fs::path& path) {
    std::wstring s = path.wstring();
    std::wstring escaped;
    escaped.reserve(s.size() + 8);
    for (wchar_t c : s) {
        if (c == L'\'') escaped += L"''";
        else escaped += c;
    }
    return L"'" + escaped + L"'";
}

std::wstring quotePsString(const std::wstring& input) {
    std::wstring escaped;
    escaped.reserve(input.size() + 8);
    for (wchar_t c : input) {
        if (c == L'\'') escaped += L"''";
        else escaped += c;
    }
    return L"'" + escaped + L"'";
}

int runPowerShell(const std::wstring& command) {
    std::wstring full = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command " + command;
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    std::vector<wchar_t> cmd(full.begin(), full.end());
    cmd.push_back(L'\0');
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

std::string sanitizeSharedPreferences(const std::string& raw) {
    std::regex pairRe(R"REGEX("([^"\\]*(?:\\.[^"\\]*)*)"\s*:\s*("(?:[^"\\]|\\.)*"|true|false|null|-?\d+(?:\.\d+)?))REGEX");
    std::sregex_iterator it(raw.begin(), raw.end(), pairRe);
    std::sregex_iterator end;

    std::vector<std::pair<std::string, std::string>> kept;
    for (; it != end; ++it) {
        std::string key = (*it)[1].str();
        std::string value = (*it)[2].str();
        std::string combined = key + ":" + value;
        if (containsAny(key, kSensitiveKeyParts) || containsAny(combined, kForbiddenContent)) continue;
        kept.push_back({key, value});
    }

    if (kept.empty()) throw std::runtime_error("No safe settings found in shared_preferences.json");

    std::ostringstream out;
    out << "{\n";
    for (size_t i = 0; i < kept.size(); ++i) {
        out << "  \"" << kept[i].first << "\": " << kept[i].second;
        if (i + 1 < kept.size()) out << ",";
        out << "\n";
    }
    out << "}\n";
    return out.str();
}

bool validateSafePreferences(const std::string& data, std::string& error) {
    if (data.find('{') == std::string::npos || data.find('}') == std::string::npos) {
        error = "settings file is not JSON-like";
        return false;
    }
    if (containsAny(data, kForbiddenContent) || containsAny(data, kSensitiveKeyParts)) {
        error = "archive contains sensitive profile/config data";
        return false;
    }
    if (data.find("\"flutter.") == std::string::npos) {
        error = "archive does not look like Hiddify app settings";
        return false;
    }
    return true;
}

fs::path saveZipDialog() {
    wchar_t fileName[MAX_PATH] = L"hiddify-app-settings.zip";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetConsoleWindow();
    ofn.lpstrFilter = L"ZIP archive (*.zip)\0*.zip\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"zip";
    if (!GetSaveFileNameW(&ofn)) return {};
    return fs::path(fileName);
}

fs::path openZipDialog() {
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetConsoleWindow();
    ofn.lpstrFilter = L"ZIP archive (*.zip)\0*.zip\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return {};
    return fs::path(fileName);
}

void exportSettings() {
    fs::path prefs = sharedPrefsPath();
    if (!fs::exists(prefs)) throw std::runtime_error("Hiddify shared_preferences.json not found");

    fs::path destination = saveZipDialog();
    if (destination.empty()) {
        std::cout << "Export cancelled.\n";
        return;
    }

    fs::path temp = makeTempDir("hiddify-settings-export");
    try {
        std::string raw = readFile(prefs);
        std::string safe = sanitizeSharedPreferences(raw);
        writeFile(temp / "shared_preferences.safe.json", safe);
        writeFile(temp / "manifest.json",
                  "{\n"
                  "  \"hiddify_safe_settings_export\": true,\n"
                  "  \"version\": 1,\n"
                  "  \"contains_profiles\": false,\n"
                  "  \"contains_connection_configs\": false\n"
                  "}\n");

        if (fs::exists(destination)) fs::remove(destination);
        std::wstring zipSource = (temp / "*").wstring();
        std::wstring ps = L"Compress-Archive -Path " + quotePsString(zipSource) +
                          L" -DestinationPath " + quotePs(destination) + L" -Force";
        int code = runPowerShell(ps);
        if (code != 0 || !fs::exists(destination)) {
            throw std::runtime_error("Compress-Archive failed");
        }
        std::cout << "Exported safe Hiddify app settings to:\n" << destination.string() << "\n";
    } catch (...) {
        fs::remove_all(temp);
        throw;
    }
    fs::remove_all(temp);
}

void importSettings() {
    fs::path archive = openZipDialog();
    if (archive.empty()) {
        std::cout << "Import cancelled.\n";
        return;
    }

    fs::path temp = makeTempDir("hiddify-settings-import");
    try {
        std::wstring ps = L"Expand-Archive -LiteralPath " + quotePs(archive) +
                          L" -DestinationPath " + quotePs(temp) + L" -Force";
        int code = runPowerShell(ps);
        if (code != 0) throw std::runtime_error("Expand-Archive failed");

        fs::path manifest = temp / "manifest.json";
        fs::path safePrefs = temp / "shared_preferences.safe.json";
        if (!fs::exists(manifest) || !fs::exists(safePrefs)) {
            throw std::runtime_error("Invalid archive: manifest/settings file is missing");
        }
        std::string manifestData = readFile(manifest);
        if (manifestData.find("\"hiddify_safe_settings_export\"") == std::string::npos ||
            manifestData.find("true") == std::string::npos) {
            throw std::runtime_error("Invalid archive: not a safe Hiddify settings export");
        }

        std::string safeData = readFile(safePrefs);
        std::string validationError;
        if (!validateSafePreferences(safeData, validationError)) {
            throw std::runtime_error("Invalid archive: " + validationError);
        }

        fs::path prefs = sharedPrefsPath();
        fs::create_directories(prefs.parent_path());
        if (fs::exists(prefs)) {
            fs::copy_file(prefs, prefs.string() + ".bak-" + timestamp(), fs::copy_options::overwrite_existing);
        }
        writeFile(prefs, safeData);
        std::cout << "Imported safe Hiddify app settings.\nRestart Hiddify to apply them.\n";
    } catch (...) {
        fs::remove_all(temp);
        throw;
    }
    fs::remove_all(temp);
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "Hiddify Safe App Settings Import/Export\n";
    std::cout << "Profiles and connection configs are intentionally excluded.\n\n";

    while (true) {
        std::cout << "1. Export settings\n";
        std::cout << "2. Import settings\n";
        std::cout << "0. Exit\n";
        std::cout << "Select: ";

        std::string choice;
        std::getline(std::cin, choice);

        try {
            if (choice == "1") exportSettings();
            else if (choice == "2") importSettings();
            else if (choice == "0") break;
            else std::cout << "Unknown option.\n";
        } catch (const std::exception& e) {
            std::cerr << "ERROR: " << e.what() << "\n";
        }
        std::cout << "\n";
    }
    return 0;
}
