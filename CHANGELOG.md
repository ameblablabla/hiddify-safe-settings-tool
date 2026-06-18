# Changelog

## 2.9.2 - Zapret Elevated Console Encoding

- Fixed broken Cyrillic text in the elevated zapret install popup console.
- Console output now always goes through Unicode `WriteConsoleW` instead of raw UTF-8 bytes.
- Elevated zapret child process reuses the selected language and sets a proper UTF-8 console title.

## 2.9.1 - GitHub Download Hang Hotfix

- Reverted bundled settings and zapret URLs back to the stable `v2.8.0` release assets.
- Fixed hangs on `Скачиваю настройки VovaVPN...` by skipping `URLDownloadToFileW` for GitHub release downloads and using timed `curl` first.
- Kept only the silent persistent Hiddify run-as-admin registry setup from `2.9.0`.

## 2.9.0 - Persistent Hiddify Run As Administrator

- The setup tool now writes Windows AppCompat `RUNASADMIN` flags for Hiddify executables.
- Hiddify keeps requesting administrator rights from any shortcut or launch path, even after the setup tool is closed.
- Covers `Hiddify.exe`, `HiddifyNext.exe`, and `HiddifyCli.exe` across common install locations and uninstall registry entries.
- Updated bundled settings and zapret release URLs to `v2.9.0`.

## 2.8.0 - Zapret Installer Diagnostics

- Reworked zapret service installation to generate and run `vovavpn-install-service.ps1` instead of passing one large hidden PowerShell command.
- The installer now creates `vovavpn-install-service.log` before starting PowerShell, so service-install failures no longer disappear without diagnostics.
- Added richer service logs, including `sc.exe` output and recent Service Control Manager events.
- Updated bundled settings and zapret release URLs to `v2.8.0`.

## 2.7.0 - Zapret Empty Filter Hotfix

- Fixed zapret service startup failure when `GameFilterTCP` / `GameFilterUDP` were empty.
- The setup tool now removes empty game-filter sections before creating the Windows service, preventing `winws.exe` from exiting with service error `1067`.
- Added validation so a broken strategy is caught before service creation.
- Updated bundled settings and zapret release URLs to `v2.7.0`.

## 2.6.0 - Silent Zapret Service Install

- Removed the interactive `service.bat` manager from the VovaVPN install flow.
- The setup tool now parses `000-vovavpn.bat` and creates the `zapret` Windows service directly with `sc.exe`.
- Added `vovavpn-install-service.log` in the zapret folder for service install diagnostics.
- Updated bundled settings and zapret release URLs to `v2.6.0`.

## 2.5.0 - Zapret Unpack and Unicode Console Hotfix

- Fixed zapret unpacking: the archive copy step now expands wildcard paths correctly and validates that `service.bat` and `000-vovavpn.bat` reached `C:\zapret\vovavpn-zapret`.
- Replaced normal console text output with Windows Unicode console output so Russian text does not turn into `????` on older Windows console settings.
- Updated bundled settings and zapret release URLs to `v2.5.0`.

## 2.4.0 - Windows Downloader and Encoding Hotfix

- Fixed Russian console text turning into question marks on some Windows systems by forcing UTF-8 input/output during build.
- Replaced the visible first-step `curl` download with Windows URL downloader first.
- Added BITS, WebClient, and hidden `curl` fallback download paths.
- Added a browser fallback for Hiddify: if automatic download is blocked, the tool opens the official installer URL and then tries to find the downloaded installer in `Downloads`.
- Updated bundled settings and zapret release URLs to `v2.4.0`.

## 2.3.0 - GitHub Download Hotfix

- Fixed Hiddify installer downloads from GitHub release assets.
- Added `curl.exe` download path with redirects, retries, User-Agent, and visible errors.
- Kept PowerShell download as a fallback instead of the only method.
- Reused the same robust downloader for the VovaVPN zapret archive.
- Added downloaded file size validation before running installers or extracting archives.

## 2.2.0 - Mini App Owns Key Copy

- Removed VPN config copy/import from the default setup wizard.
- Removed the VPN config copy option from the main menu.
- Updated release URLs to `v2.2.0`.
- The Mini App is now responsible for copying the VLESS key.

## 2.1.0 - Bundled VovaVPN Zapret

- Added VovaVPN zapret bundle download support.
- The setup tool now requires `000-vovavpn.bat` and downloads the VovaVPN zapret package when it is missing.
- The preferred zapret strategy is now `000-vovavpn.bat`.
- Updated release URLs to `v2.1.0`.

## 2.0.1 - Console Render Hotfix

- Fixed a console rendering issue where the first menu could appear as a blank black window.
- Added explicit output flushing before keyboard/input waits.
- Updated bundled settings download URL for the `v2.0.1` release.

## 2.0.0 - VovaVPN Setup Wizard

- Added Russian/English interactive menu with arrow-key navigation.
- Added full VovaVPN setup flow.
- Added Hiddify detection and official installer download.
- Added VovaVPN safe Hiddify settings download/import.
- Added VPN config clipboard flow for Hiddify `Import from Clipboard`.
- Added Amnezia/AmneziaWG conflict detection.
- Added explicit process-closing action for Amnezia conflicts.
- Added Flowseal zapret download/reuse and service installation flow.
- Added optional Minecraft TCP `25565` rule injection for zapret strategy files.
- Preserved safe Hiddify settings import/export.

## 1.0.0 - Safe Settings Import/Export

- Added Windows-only safe Hiddify app settings export.
- Added safe Hiddify app settings import.
- Blocked VPN profiles, subscriptions, UUIDs, private keys, and other sensitive config data from safe settings archives.
