# MTP Devices

Total Commander file system plugin (WFX) for phones, media players and cameras over **MTP / Windows Portable Devices**.

This is a modified version of Christian Ghisler’s *Windows Media Audio 2* (`wpdplug`) plugin. It is **not** the original MediaAudio2.

- Shows up in Network Neighborhood as **MTP Devices**
- Files: `mtpdevices.wfx` (32-bit) and `mtpdevices.wfx64` (64-bit)
- License: [BSD-3-Clause](LICENSE.txt)

## Install

1. Download the zip from [Releases](../../releases).
2. Open the archive in Total Commander (or double-click it from TC).
3. Confirm plugin installation.

Requires Windows Vista or later with WPD support.

## Build

Open `wpdplug.sln` in Visual Studio 2022. Release | Win32 → `.wfx`, Release | x64 → `.wfx64`.

## Credits

Original plugin © 2011–2018 Christian Ghisler, Ghisler Software GmbH.  
Modifications © 2026 MTP Devices contributors.
