#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <conio.h>

namespace fs = std::filesystem;

static const char* kVersion = "2.2.0";
static const char* kHiddifyInstallerUrl =
    "https://github.com/hiddify/hiddify-app/releases/download/v4.1.1/Hiddify-Windows-Setup-x64.exe";
static const char* kSafeSettingsUrl =
    "https://github.com/ameblablabla/hiddify-safe-settings-tool/releases/download/v2.2.0/hiddify-app-settings.zip";
static const char* kZapretBundleUrl =
    "https://github.com/ameblablabla/hiddify-safe-settings-tool/releases/download/v2.2.0/vovavpn-zapret.zip";
static const wchar_t* kZapretDefaultDir = L"C:\\zapret\\vovavpn-zapret";

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

enum class Lang {
    Ru,
    En
};

static Lang gLang = Lang::Ru;

std::string tr(const char* ru, const char* en) {
    return gLang == Lang::Ru ? std::string(ru) : std::string(en);
}

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
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write file: " + path.string());
    out.write(data.data(), (std::streamsize)data.size());
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

std::wstring quoteCmd(const fs::path& path) {
    std::wstring s = path.wstring();
    std::wstring out = L"\"";
    for (wchar_t c : s) {
        if (c == L'"') out += L"\\\"";
        else out += c;
    }
    out += L"\"";
    return out;
}

int runProcess(const std::wstring& command, bool hidden = false) {
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    if (hidden) si.dwFlags = STARTF_USESHOWWINDOW;
    if (hidden) si.wShowWindow = SW_HIDE;

    std::vector<wchar_t> cmd(command.begin(), command.end());
    cmd.push_back(L'\0');
    DWORD flags = hidden ? CREATE_NO_WINDOW : 0;
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, flags, nullptr, nullptr, &si, &pi)) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

int runPowerShell(const std::wstring& command, bool hidden = true) {
    return runProcess(L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command " + command, hidden);
}

bool launchAndWaitElevated(const fs::path& file, const std::wstring& parameters = L"") {
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = file.c_str();
    sei.lpParameters = parameters.empty() ? nullptr : parameters.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) return false;
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
    }
    return true;
}

void enableVirtualTerminal() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
    }
}

void clearScreen() {
    std::cout << "\x1b[2J\x1b[H";
}

void color(const char* ansi) {
    std::cout << ansi;
}

void resetColor() {
    std::cout << "\x1b[0m";
}

void printHeader(const std::string& subtitle = "") {
    color("\x1b[38;2;228;188;58m");
    std::cout << "VovaVPN Hiddify Setup Tool v" << kVersion << "\n";
    resetColor();
    std::cout << "------------------------------------------------------------\n";
    if (!subtitle.empty()) {
        std::cout << subtitle << "\n\n";
    }
}

void waitKey() {
    std::cout << "\n" << tr("Нажмите любую клавишу...", "Press any key...") << "\n";
    std::cout.flush();
    _getch();
}

int selectMenu(const std::string& title, const std::vector<std::string>& items, int selected = 0) {
    if (items.empty()) return -1;
    selected = std::clamp(selected, 0, (int)items.size() - 1);

    while (true) {
        clearScreen();
        printHeader(title);
        std::cout << tr("Используйте стрелки вверх/вниз и Enter. Esc - назад.\n\n",
                         "Use Up/Down arrows and Enter. Esc - back.\n\n");
        for (int i = 0; i < (int)items.size(); ++i) {
            if (i == selected) {
                color("\x1b[30;48;2;228;188;58m");
                std::cout << "  > " << items[i] << "  ";
                resetColor();
                std::cout << "\n";
            } else {
                std::cout << "    " << items[i] << "\n";
            }
        }

        std::cout.flush();
        int ch = _getch();
        if (ch == 27) return -1;
        if (ch == 13) return selected;
        if (ch >= '1' && ch <= '9') {
            int index = ch - '1';
            if (index >= 0 && index < (int)items.size()) return index;
        }
        if (ch == 0 || ch == 224) {
            int ext = _getch();
            if (ext == 72) selected = (selected - 1 + (int)items.size()) % (int)items.size();
            if (ext == 80) selected = (selected + 1) % (int)items.size();
        }
    }
}

bool confirm(const std::string& question, bool defaultYes = false) {
    std::vector<std::string> items;
    if (defaultYes) {
        items = {tr("Да", "Yes"), tr("Нет", "No")};
    } else {
        items = {tr("Нет", "No"), tr("Да", "Yes")};
    }
    int choice = selectMenu(question, items, 0);
    if (choice < 0) return false;
    return defaultYes ? choice == 0 : choice == 1;
}

bool isAdmin() {
    BOOL isMember = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = nullptr;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                 &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isMember);
        FreeSid(adminGroup);
    }
    return isMember == TRUE;
}

fs::path appDataRoaming() {
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) throw std::runtime_error("APPDATA is not set");
    return fs::path(appdata);
}

fs::path appDataLocal() {
    const char* local = std::getenv("LOCALAPPDATA");
    if (!local) throw std::runtime_error("LOCALAPPDATA is not set");
    return fs::path(local);
}

fs::path programFiles() {
    const char* pf = std::getenv("ProgramFiles");
    if (!pf) return {};
    return fs::path(pf);
}

fs::path programFilesX86() {
    const char* pf = std::getenv("ProgramFiles(x86)");
    if (!pf) return {};
    return fs::path(pf);
}

std::vector<fs::path> hiddifyPreferencePaths() {
    std::vector<fs::path> paths;
    paths.push_back(appDataRoaming() / "Hiddify" / "hiddify" / "shared_preferences.json");
    paths.push_back(appDataLocal() / "Packages" / "Hiddify.HiddifyNext_pvn3df8hp03bc" /
                    "LocalCache" / "Roaming" / "Hiddify" / "hiddify" / "shared_preferences.json");
    return paths;
}

std::vector<fs::path> hiddifyExeCandidates() {
    std::vector<fs::path> p;
    fs::path local = appDataLocal();
    fs::path pf = programFiles();
    fs::path pfx86 = programFilesX86();

    p.push_back(local / "Programs" / "Hiddify" / "Hiddify.exe");
    p.push_back(local / "Programs" / "Hiddify" / "HiddifyNext.exe");
    p.push_back(local / "Hiddify" / "Hiddify.exe");
    p.push_back(local / "Hiddify" / "HiddifyNext.exe");
    if (!pf.empty()) {
        p.push_back(pf / "Hiddify" / "Hiddify.exe");
        p.push_back(pf / "Hiddify" / "HiddifyNext.exe");
        p.push_back(pf / "hiddify" / "Hiddify.exe");
        p.push_back(pf / "hiddify" / "HiddifyNext.exe");
    }
    if (!pfx86.empty()) {
        p.push_back(pfx86 / "Hiddify" / "Hiddify.exe");
        p.push_back(pfx86 / "Hiddify" / "HiddifyNext.exe");
    }
    return p;
}

std::optional<fs::path> findHiddifyExe() {
    for (const auto& p : hiddifyExeCandidates()) {
        if (fs::exists(p)) return p;
    }
    return std::nullopt;
}

void downloadFile(const std::string& url, const fs::path& out) {
    fs::create_directories(out.parent_path());
    std::wstring ps =
        L"$ProgressPreference='SilentlyContinue'; "
        L"[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; "
        L"Invoke-WebRequest -UseBasicParsing -Uri " + quotePsString(widen(url)) +
        L" -OutFile " + quotePs(out) + L"; "
        L"if (!(Test-Path " + quotePs(out) + L")) { exit 2 }";
    int code = runPowerShell(ps, true);
    if (code != 0 || !fs::exists(out)) {
        throw std::runtime_error("Download failed: " + url);
    }
}

void installHiddifyIfNeeded() {
    clearScreen();
    printHeader(tr("Проверка Hiddify", "Checking Hiddify"));

    auto existing = findHiddifyExe();
    if (existing) {
        std::cout << tr("Hiddify найден:\n", "Hiddify found:\n") << existing->string() << "\n";
        waitKey();
        return;
    }

    std::cout << tr("Hiddify не найден. Можно скачать официальный установщик и запустить его сейчас.\n",
                     "Hiddify was not found. The official installer can be downloaded and started now.\n");
    if (!confirm(tr("Скачать и запустить установщик Hiddify?", "Download and run the Hiddify installer?"), true)) {
        return;
    }

    fs::path temp = makeTempDir("hiddify-installer");
    fs::path installer = temp / "Hiddify-Windows-Setup-x64.exe";
    std::cout << tr("Скачиваю установщик...\n", "Downloading installer...\n");
    downloadFile(kHiddifyInstallerUrl, installer);

    std::cout << tr("Запускаю установщик. Пройдите установку и вернитесь в это окно.\n",
                     "Starting the installer. Finish setup, then return to this window.\n");
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = installer.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) {
        fs::remove_all(temp);
        throw std::runtime_error("Failed to start Hiddify installer");
    }
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
    }
    fs::remove_all(temp);

    if (findHiddifyExe()) {
        std::cout << tr("Hiddify установлен.\n", "Hiddify is installed.\n");
    } else {
        std::cout << tr("Я не смог автоматически найти Hiddify после установки. Если он открылся, это нормально.\n",
                         "Hiddify was not found automatically after setup. If it opened, that is okay.\n");
    }
    waitKey();
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

void copyPreferencesWithBackup(const fs::path& prefs, const std::string& safeData) {
    fs::create_directories(prefs.parent_path());
    if (fs::exists(prefs)) {
        fs::path backup = prefs;
        backup += ".bak-" + timestamp();
        fs::copy_file(prefs, backup, fs::copy_options::overwrite_existing);
    }
    writeFile(prefs, safeData);
}

void importSettingsArchive(const fs::path& archive) {
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

        auto prefs = hiddifyPreferencePaths();
        copyPreferencesWithBackup(prefs.front(), safeData);
        for (size_t i = 1; i < prefs.size(); ++i) {
            if (fs::exists(prefs[i].parent_path())) {
                copyPreferencesWithBackup(prefs[i], safeData);
            }
        }
    } catch (...) {
        fs::remove_all(temp);
        throw;
    }
    fs::remove_all(temp);
}

void exportSettings() {
    fs::path prefs;
    for (const auto& p : hiddifyPreferencePaths()) {
        if (fs::exists(p)) {
            prefs = p;
            break;
        }
    }
    if (prefs.empty()) throw std::runtime_error("Hiddify shared_preferences.json not found");

    fs::path destination = saveZipDialog();
    if (destination.empty()) return;

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
        std::cout << tr("Экспортировано:\n", "Exported:\n") << destination.string() << "\n";
    } catch (...) {
        fs::remove_all(temp);
        throw;
    }
    fs::remove_all(temp);
}

void importSettingsFromFile() {
    fs::path archive = openZipDialog();
    if (archive.empty()) return;
    importSettingsArchive(archive);
    std::cout << tr("Безопасные настройки Hiddify импортированы.\n",
                     "Safe Hiddify settings were imported.\n");
}

void importBundledSettings() {
    clearScreen();
    printHeader(tr("Импорт настроек Hiddify", "Import Hiddify settings"));
    fs::path temp = makeTempDir("vovavpn-settings");
    fs::path archive = temp / "hiddify-app-settings.zip";
    try {
        std::cout << tr("Скачиваю настройки VovaVPN...\n", "Downloading VovaVPN settings...\n");
        downloadFile(kSafeSettingsUrl, archive);
        importSettingsArchive(archive);
        std::cout << tr("Настройки импортированы. Если Hiddify открыт, перезапустите его.\n",
                         "Settings were imported. Restart Hiddify if it is open.\n");
    } catch (...) {
        fs::remove_all(temp);
        throw;
    }
    fs::remove_all(temp);
    waitKey();
}

struct ProcessInfo {
    DWORD pid;
    std::wstring name;
};

std::vector<ProcessInfo> findAmneziaProcesses() {
    std::vector<ProcessInfo> result;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return result;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring name = pe.szExeFile;
            std::string low = lowerCopy(narrow(name));
            if (low.find("amnezia") != std::string::npos || low.find("amneziawg") != std::string::npos) {
                result.push_back({pe.th32ProcessID, name});
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return result;
}

void terminateProcess(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (h) {
        TerminateProcess(h, 0);
        CloseHandle(h);
    }
}

void handleAmneziaConflict() {
    auto processes = findAmneziaProcesses();
    if (processes.empty()) {
        std::cout << tr("Процессы Amnezia/AmneziaWG не найдены.\n",
                         "No Amnezia/AmneziaWG processes were found.\n");
        return;
    }

    clearScreen();
    printHeader(tr("Конфликт Amnezia", "Amnezia conflict"));
    std::cout << tr(
        "На компьютере найдены процессы Amnezia/AmneziaWG. Они часто конфликтуют с Hiddify.\n"
        "Программа может закрыть запущенные процессы. Удалять Amnezia автоматически я не буду.\n\n",
        "Amnezia/AmneziaWG processes were found. They often conflict with Hiddify.\n"
        "This tool can close running processes. It will not uninstall Amnezia automatically.\n\n");

    for (const auto& p : processes) {
        std::cout << "  " << narrow(p.name) << " (PID " << p.pid << ")\n";
    }

    std::vector<std::string> actions = {
        tr("Закрыть процессы Amnezia", "Close Amnezia processes"),
        tr("Открыть окно удаления программ", "Open Apps uninstall settings"),
        tr("Оставить как есть", "Leave as is")
    };
    int choice = selectMenu(tr("Что сделать с Amnezia?", "What should be done with Amnezia?"), actions, 0);
    if (choice == 0) {
        for (const auto& p : processes) terminateProcess(p.pid);
        std::cout << tr("Процессы закрыты.\n", "Processes closed.\n");
        waitKey();
    } else if (choice == 1) {
        ShellExecuteW(nullptr, L"open", L"ms-settings:appsfeatures", nullptr, nullptr, SW_SHOWNORMAL);
    }
}

bool validZapretDir(const fs::path& dir) {
    return fs::exists(dir / "service.bat") && fs::exists(dir / "bin" / "winws.exe");
}

bool validVovaZapretDir(const fs::path& dir) {
    return validZapretDir(dir) && fs::exists(dir / "000-vovavpn.bat");
}

std::optional<fs::path> findZapretDir() {
    std::vector<fs::path> candidates = {
        fs::path(kZapretDefaultDir),
        fs::path(L"C:\\zapret\\zapret-discord-youtube-1.9.9c"),
        fs::path(L"C:\\zapret\\zapret-discord-youtube")
    };

    for (const auto& c : candidates) {
        if (validVovaZapretDir(c)) return c;
    }

    fs::path root = L"C:\\zapret";
    if (fs::exists(root)) {
        for (const auto& entry : fs::directory_iterator(root)) {
            if (entry.is_directory() && validVovaZapretDir(entry.path())) return entry.path();
        }
    }
    return std::nullopt;
}

fs::path ensureZapretDownloaded() {
    if (auto found = findZapretDir()) return *found;

    fs::path target = kZapretDefaultDir;
    std::wstring ps =
        L"$ErrorActionPreference='Stop'; "
        L"$ProgressPreference='SilentlyContinue'; "
        L"[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; "
        L"$target=" + quotePs(target) + L"; "
        L"$tmp=Join-Path $env:TEMP ('vovavpn-zapret-' + [guid]::NewGuid().ToString()); "
        L"$zip=Join-Path $tmp 'zapret.zip'; "
        L"New-Item -ItemType Directory -Path $tmp -Force | Out-Null; "
        L"$headers=@{'User-Agent'='VovaVPN-Setup'}; "
        L"Invoke-WebRequest -UseBasicParsing -Headers $headers -Uri " + quotePsString(widen(kZapretBundleUrl)) + L" -OutFile $zip; "
        L"$extract=Join-Path $tmp 'extract'; "
        L"Expand-Archive -LiteralPath $zip -DestinationPath $extract -Force; "
        L"$service=Get-ChildItem -LiteralPath $extract -Recurse -Filter service.bat | Select-Object -First 1; "
        L"if (-not $service) { throw 'service.bat was not found in archive' }; "
        L"$src=Split-Path -Parent $service.FullName; "
        L"New-Item -ItemType Directory -Path $target -Force | Out-Null; "
        L"Copy-Item -LiteralPath (Join-Path $src '*') -Destination $target -Recurse -Force; "
        L"Remove-Item -LiteralPath $tmp -Recurse -Force; ";

    std::cout << tr("Скачиваю VovaVPN zapret...\n", "Downloading VovaVPN zapret...\n");
    int code = runPowerShell(ps, false);
    if (code != 0 || !validVovaZapretDir(target)) {
        throw std::runtime_error("Failed to download or unpack VovaVPN zapret");
    }
    return target;
}

std::vector<fs::path> listZapretStrategies(const fs::path& dir) {
    std::vector<fs::path> result;
    if (!fs::exists(dir)) return result;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        fs::path p = entry.path();
        std::string name = lowerCopy(p.filename().string());
        if (p.extension() == ".bat" && name.rfind("service", 0) != 0 && name.rfind("000-vovavpn", 0) != 0) {
            result.push_back(p);
        }
    }
    std::sort(result.begin(), result.end(), [](const fs::path& a, const fs::path& b) {
        return lowerCopy(a.filename().string()) < lowerCopy(b.filename().string());
    });
    return result;
}

fs::path chooseZapretStrategy(const fs::path& dir) {
    fs::path vova = dir / "000-vovavpn.bat";
    if (fs::exists(vova)) return vova;

    std::vector<std::string> preferred = {
        "general (ALT).bat",
        "general.bat",
        "general (FAKE TLS AUTO).bat",
        "general (SIMPLE FAKE).bat"
    };
    for (const auto& name : preferred) {
        fs::path p = dir / name;
        if (fs::exists(p)) return p;
    }
    auto all = listZapretStrategies(dir);
    if (!all.empty()) return all.front();
    throw std::runtime_error("No zapret strategy .bat files found");
}

std::string replaceFirstWfTcp(const std::string& content) {
    size_t pos = content.find("--wf-tcp=");
    if (pos == std::string::npos) return content;
    size_t valueStart = pos + 9;
    size_t valueEnd = content.find_first_of(" \t\r\n^", valueStart);
    if (valueEnd == std::string::npos) valueEnd = content.size();
    std::string value = content.substr(valueStart, valueEnd - valueStart);
    if (value.find("25565") != std::string::npos) return content;

    std::string out = content;
    out.insert(valueEnd, ",25565");
    return out;
}

std::string ensureMinecraftRule(std::string content) {
    content = replaceFirstWfTcp(content);
    if (content.find("--filter-tcp=25565") != std::string::npos) return content;

    const std::string rule =
        "--filter-tcp=25565 --ipset-exclude=\"%LISTS%ipset-exclude.txt\" "
        "--dpi-desync-any-protocol=1 --dpi-desync-cutoff=n5 "
        "--dpi-desync=multisplit --dpi-desync-split-seqovl=582 "
        "--dpi-desync-split-pos=1 "
        "--dpi-desync-split-seqovl-pattern=\"%BIN%tls_clienthello_4pda_to.bin\" --new ^\r\n";

    std::istringstream in(content);
    std::ostringstream out;
    std::string line;
    bool inserted = false;
    while (std::getline(in, line)) {
        out << line << "\n";
        if (!inserted && line.find("%BIN%winws.exe") != std::string::npos) {
            out << rule;
            inserted = true;
        }
    }
    if (!inserted) out << rule;
    return out.str();
}

fs::path prepareVovaStrategy(const fs::path& zapretDir, const fs::path& sourceStrategy, bool minecraft) {
    std::string content = readFile(sourceStrategy);
    if (minecraft) content = ensureMinecraftRule(content);

    fs::path target = zapretDir / "000-vovavpn.bat";
    writeFile(target, content);
    return target;
}

void installZapretService(bool minecraft) {
    if (!isAdmin()) {
        fs::path self;
        wchar_t buf[MAX_PATH]{};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        self = buf;
        std::wstring args = L"--install-zapret";
        if (minecraft) args += L" --minecraft";
        std::cout << tr("Для установки сервиса нужны права администратора. Сейчас появится UAC.\n",
                         "Administrator rights are required to install the service. UAC will appear now.\n");
        launchAndWaitElevated(self, args);
        return;
    }

    fs::path zapretDir = ensureZapretDownloaded();
    fs::path chosen = chooseZapretStrategy(zapretDir);
    prepareVovaStrategy(zapretDir, chosen, minecraft);

    fs::path installer = zapretDir / "vovavpn-install-service.cmd";
    std::string script =
        "@echo off\r\n"
        "cd /d \"%~dp0\"\r\n"
        "(echo 1&echo 1&echo.) | \"%~dp0service.bat\" admin\r\n";
    writeFile(installer, script);

    std::cout << tr("Устанавливаю zapret как сервис через стратегию VovaVPN...\n",
                     "Installing zapret as a service using the VovaVPN strategy...\n");
    int code = runProcess(L"cmd.exe /c " + quoteCmd(installer), false);
    if (code != 0) {
        throw std::runtime_error("Zapret service installer returned an error");
    }

    std::cout << tr("Zapret установлен как сервис.\n", "Zapret was installed as a service.\n");
    waitKey();
}

void zapretWizard() {
    clearScreen();
    printHeader(tr("Zapret для Discord/YouTube", "Zapret for Discord/YouTube"));
    std::cout << tr(
        "Этот шаг ставит Flowseal zapret как Windows-сервис. Он может помочь Discord/YouTube.\n"
        "Если всё уже работает без него, можно пропустить.\n\n",
        "This step installs Flowseal zapret as a Windows service. It may help Discord/YouTube.\n"
        "If everything already works without it, you can skip it.\n\n");

    if (!confirm(tr("Установить zapret-сервис?", "Install zapret service?"), false)) return;
    bool minecraft = confirm(tr("Добавить обход Minecraft порта 25565?", "Add Minecraft port 25565 bypass?"), false);
    installZapretService(minecraft);
}

void fullSetupWizard() {
    installHiddifyIfNeeded();
    handleAmneziaConflict();
    importBundledSettings();
    zapretWizard();
}

void runInstallZapretFromArgs(bool minecraft) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    enableVirtualTerminal();
    try {
        installZapretService(minecraft);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        waitKey();
    }
}

void chooseLanguage() {
    int choice = selectMenu("Language / Язык", {"Русский", "English"}, 0);
    gLang = (choice == 1) ? Lang::En : Lang::Ru;
}

void mainMenu() {
    while (true) {
        std::vector<std::string> items = {
            tr("Полная настройка VovaVPN", "Full VovaVPN setup"),
            tr("Проверить / установить Hiddify", "Check / install Hiddify"),
            tr("Импортировать настройки Hiddify VovaVPN", "Import VovaVPN Hiddify settings"),
            tr("Проверить конфликт Amnezia", "Check Amnezia conflict"),
            tr("Установить zapret для Discord/YouTube", "Install zapret for Discord/YouTube"),
            tr("Экспорт безопасных настроек Hiddify", "Export safe Hiddify settings"),
            tr("Импорт безопасных настроек из файла", "Import safe settings from file"),
            tr("Выход", "Exit")
        };

        int choice = selectMenu(tr("Главное меню", "Main menu"), items, 0);
        if (choice < 0 || choice == 7) break;

        try {
            clearScreen();
            switch (choice) {
                case 0: fullSetupWizard(); break;
                case 1: installHiddifyIfNeeded(); break;
                case 2: importBundledSettings(); break;
                case 3: handleAmneziaConflict(); waitKey(); break;
                case 4: zapretWizard(); break;
                case 5: exportSettings(); waitKey(); break;
                case 6: importSettingsFromFile(); waitKey(); break;
                default: break;
            }
        } catch (const std::exception& e) {
            std::cerr << "\nERROR: " << e.what() << "\n";
            waitKey();
        }
    }
}

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    enableVirtualTerminal();
    std::ios::sync_with_stdio(false);
    std::cout.setf(std::ios::unitbuf);

    bool installZapretArg = false;
    bool minecraftArg = false;
    bool printHelp = false;
    bool printVersion = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--install-zapret") installZapretArg = true;
        if (arg == "--minecraft") minecraftArg = true;
        if (arg == "--en") gLang = Lang::En;
        if (arg == "--help" || arg == "-h" || arg == "/?") printHelp = true;
        if (arg == "--version" || arg == "-v") printVersion = true;
    }

    if (printVersion) {
        std::cout << "VovaVPN Hiddify Setup Tool " << kVersion << "\n";
        return 0;
    }

    if (printHelp) {
        std::cout << "VovaVPN Hiddify Setup Tool " << kVersion << "\n"
                  << "Usage:\n"
                  << "  hiddify_settings_tool.exe\n"
                  << "  hiddify_settings_tool.exe --version\n"
                  << "  hiddify_settings_tool.exe --install-zapret [--minecraft]\n";
        return 0;
    }

    if (installZapretArg) {
        runInstallZapretFromArgs(minecraftArg);
        return 0;
    }

    chooseLanguage();
    mainMenu();
    return 0;
}
