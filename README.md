# VovaVPN Hiddify Setup Tool

Windows 10/11 setup helper for VovaVPN users.

The tool helps a user install Hiddify, import safe Hiddify app settings, handle common Amnezia conflicts, and optionally install VovaVPN zapret for Discord/YouTube.

## Download

Open the latest GitHub release and download:

```text
hiddify_settings_tool.exe
```

Run it, choose the language, then use the arrow keys and Enter.

## What It Does

- Checks whether Hiddify is installed.
- Downloads and starts the official Hiddify Windows installer if Hiddify is missing.
- Downloads and imports the VovaVPN safe Hiddify settings archive.
- Detects running Amnezia / AmneziaWG processes that can conflict with Hiddify.
- Offers to close those processes after explicit confirmation.
- Downloads the VovaVPN zapret bundle when it is not already installed.
- Optionally installs VovaVPN zapret as a Windows service.
- Optionally adds Minecraft TCP port `25565` handling to the zapret strategy.
- Keeps the original safe settings import/export feature.

## Important Safety Notes

The tool does not silently uninstall Amnezia or any other VPN app.

If Amnezia is detected, the user can:

- close currently running Amnezia processes;
- open Windows Apps settings and uninstall it manually;
- leave it as-is.

Zapret service installation requires administrator rights because it installs a Windows service and uses WinDivert.

## Hiddify Config Flow

The setup tool does not copy or import a VPN key.

The Mini App owns the key flow:

1. user presses `Copy key` in the Mini App;
2. Mini App copies the VLESS key to the clipboard;
3. user opens Hiddify and selects `Add Config` -> `Import from Clipboard`.

This is safer and easier to recover from.

## Zapret Flow

If zapret is enabled, the tool:

1. reuses an existing valid VovaVPN zapret installation when `000-vovavpn.bat` is present;
2. otherwise downloads `vovavpn-zapret.zip` from this repository release;
3. extracts it to `C:\zapret\vovavpn-zapret`;
4. uses `000-vovavpn.bat` as the preferred strategy;
5. optionally injects the Minecraft `25565` rule;
6. installs the strategy through the bundled `service.bat`.

The preferred strategy is:

```text
000-vovavpn.bat
```

If it is unavailable, the tool falls back to other bundled general strategies.

## Build

Install MinGW-w64 or WinLibs, then run:

```powershell
.\build.ps1
```

The executable will be written to:

```text
dist\hiddify_settings_tool.exe
```

Quick checks:

```powershell
.\dist\hiddify_settings_tool.exe --version
.\dist\hiddify_settings_tool.exe --help
```

## Command Line

Interactive mode:

```powershell
.\hiddify_settings_tool.exe
```

Install zapret directly:

```powershell
.\hiddify_settings_tool.exe --install-zapret
```

Install zapret with the Minecraft TCP `25565` rule:

```powershell
.\hiddify_settings_tool.exe --install-zapret --minecraft
```
