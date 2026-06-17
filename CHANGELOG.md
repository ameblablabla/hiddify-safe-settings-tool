# Changelog

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
