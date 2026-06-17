# VovaVPN Hiddify Setup Tool

Windows 10/11 setup helper for VovaVPN users.

The tool helps a user install Hiddify, import safe Hiddify app settings, copy a VPN config into the clipboard, handle common Amnezia conflicts, and optionally install Flowseal zapret for Discord/YouTube.

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
- Lets the user paste a VPN subscription/config link.
- Copies that link to the clipboard and opens Hiddify.
- Detects running Amnezia / AmneziaWG processes that can conflict with Hiddify.
- Offers to close those processes after explicit confirmation.
- Optionally installs Flowseal zapret as a Windows service.
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

The tool does not secretly edit private Hiddify VPN profile files.

Instead it:

1. asks the user for a VPN config/subscription link;
2. copies the link to the Windows clipboard;
3. opens Hiddify;
4. asks the user to select `Add Config` -> `Import from Clipboard`.

This is safer and easier to recover from.

## Zapret Flow

If zapret is enabled, the tool:

1. reuses an existing valid `C:\zapret\...` installation when found;
2. otherwise downloads the latest Flowseal `zapret-discord-youtube` release;
3. creates `000-vovavpn.bat` from a recommended strategy;
4. optionally injects the Minecraft `25565` rule;
5. installs the strategy through the bundled `service.bat`.

The preferred base strategy is:

```text
general (ALT).bat
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
