# Hiddify Safe Settings Tool

Windows-only console utility for importing and exporting **safe Hiddify app settings**.

It intentionally excludes connection profiles and sensitive data such as:

- VLESS / VMess / Trojan / Shadowsocks links
- UUIDs, passwords, tokens, secrets, private keys
- WARP account tokens and WireGuard configs
- `current-config.json`
- Hiddify `configs/` profile files

## Compatibility

This tool is intended for Windows 10/11. It reads and writes:

```text
%APPDATA%\Hiddify\hiddify\shared_preferences.json
```

It does not manage VPN connections and does not export VPN profiles.

## Usage

Run:

```text
hiddify_settings_tool.exe
```

Menu:

```text
1. Export settings
2. Import settings
0. Exit
```

Export creates a `.zip` archive containing:

- `manifest.json`
- `shared_preferences.safe.json`

Import accepts only archives created by this tool and rejects archives containing profile/config-like sensitive data.

## Build

Install MinGW-w64 or WinLibs, then run:

```powershell
.\build.ps1
```

The compiled file will be written to:

```text
dist\hiddify_settings_tool.exe
```

## Security Note

This tool is designed for sharing UI/app preferences, not VPN access.
If you want to share VPN access, create a separate VPN client/profile and share only its QR code.
