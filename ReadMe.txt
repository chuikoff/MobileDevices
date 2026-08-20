MTP Devices plugin v2.2.5
=======================
Modified from Windows Media Audio 2 (wpdplug) by Christian Ghisler.

Copyright (C) 2011-2018 Christian Ghisler, Ghisler Software GmbH
Copyright (C) 2026 MTP Devices contributors

This is free software under the 3-clause BSD license. See LICENSE.txt.
This is an altered version and must not be presented as the original
MediaAudio2 plugin.

Description:
============
This plugin uses the Windows Portable Devices interface to access media
players, Android phones/tablets, cameras and other MTP devices.

Requirements:
=============
Windows Vista or later with Windows Portable Devices (WPD) support.

Installation:
=============
Just double click on the plugin archive to install it in Total Commander.
Files: mtpdevices.wfx (32-bit) and mtpdevices.wfx64 (64-bit).

Usage:
======
The plugin shows up in Network Neighborhood as "MTP Devices".

Command line (current plugin path):
  quote refresh     rescan devices
  quote eject       safely remove the current device
  quote info        model, battery, storage free/capacity (iOS version for iPhone)
  quote reconnect   same as refresh

Existing MediaAudio2 timezone settings (Alt+Enter on the device) are still
read; new settings are stored under [MTPDevices].

History
=======
20260820 Release v2.2.5
20260820 Added: iPhone Panic Logs folder (crashreportcopymobile)
20260820 Release v2.2.4
20260820 Fixed: opening an iPhone app folder could crash Total Commander
20260820 Release v2.2.3
20260820 Added: iPhone battery and free space in device info
20260820 Fixed: application documents via AMDeviceStartHouseArrestService
20260819 Release v2.2.2
20260819 Changed: iPhone icon is a pineapple; multi-size icons (16–256) for thumbnail view
20260819 Release v2.2.1
20260819 Changed: Applications lists only apps with Files/File Sharing access; show that app's Documents
20260819 Release v2.2.0
20260819 Added: iPhone Photos and Applications folders (app documents via house_arrest, like iMazing/3uTools)
20260819 Added: copy/delete app files on iPhone; separate Android and iPhone icons
20260819 Release v2.1.2
20260819 Changed: plugin icon restyled to Windows 11 Fluent (phone on rounded tile)
20260819 Release v2.1.1
20260819 Fixed: iPhone was not listed (Store Apple DLLs cannot be loaded in-place; copy + CFRunLoop)
20260819 Changed: Android device info no longer shows a software/firmware version
20260819 Release v2.1.0
20260819 Added: iPhone photos via Apple Mobile Device (Microsoft Store Apple Devices), not MTP
20260819 Changed: plugin root shows only Android (MTP) and iPhone/iPad (Apple driver)
20260819 Fixed: dummy MTP "1.0" no longer shown as Android firmware
20260819 Release v2.0.3
20260819 Added: Alt+Enter and quote info show model, firmware, battery and storage space (Android MTP)
20260819 Release v2.0.2
20260819 Added: Settings dialog language switch (English / Русский)
20260819 Changed: Default file list uses standard Total Commander columns (name, size, date)
20260819 Release v2.0.1
20260819 Fixed: Opening a device / device root could block ~20s (bulk properties wait + reconnect on every plugin-root listing)
20260819 Release v2.0
20260819 Added: Bulk property reads for faster folder listing
20260819 Added: quote refresh / eject / info / reconnect
20260819 Added: Filter out WPD pass-through, printers, scanners, Media Library Service
20260819 Added: Object-change events invalidate the path cache
20260819 Added: Metadata via Windows Property Store (no WMVCORE); HEIC/WebP/Opus/MKV/PDF and other types on upload
20260819 Added: DPI-aware settings dialog (Segoe UI, visual styles)
20260819 Release v1.5
20260819 Added: Background F5/F6 transfers (FsGetBackgroundFlags) with a lock around WPD calls
20260819 Added: Custom columns (Type, Title, Artist, Album, Duration, Bitrate, Width, Height, Free, Capacity, Serial, Battery)
20260819 Added: Thumbnails from WPD_RESOURCE_THUMBNAIL / album art / icon
20260819 Added: Device icon in the file list; hidden/system attributes from WPD
20260819 Added: FsStatusInfo + IPortableDevice::Cancel on abort; listing progress
20260819 Renamed plugin to "MTP Devices" (files mtpdevices.wfx / mtpdevices.wfx64)
20260819 Release v1.4
20260819 Fixed: Device names containing \, /, * or ? were not sanitized (path lookup broke)
20260819 Fixed: Duplicate device names now get a (2), (3), ... suffix
20260819 Fixed: Object-ID cache is now per-device (two phones with similar folders no longer mix IDs)
20260819 Fixed: Rename overwrite check used the old name and a consumed enumerator
20260819 Fixed: HRESULT comparison used assignment (hr = 0x80070490) instead of ==
20260819 Fixed: Folders/storage objects are detected via GUID content type (not a GUID string)
20260819 Fixed: Connection-settings radio buttons used MF_CHECKED and fell through WM_INITDIALOG
20260819 Fixed: ANSI FsGetFile/FsPutFile compared against an uninitialized name buffer
20260819 Fixed: strlcpy wrote one byte past the destination buffer
20260819 Fixed: Aborting a download left a truncated local file; aborting an upload still called Commit
20260819 Fixed: COM objects leaked after GetFile/PutFile; pPropertiesToRead leaked after FindClose
20260819 Fixed: MkDir/GetFolder at the device root crashed when the path had no trailing backslash
20260819 Added: Open the device as a proper WPD client (name, version, read-write, share mode)
20260819 Added: Fallback to read-only open, and readable log errors (device busy / MTP off / access denied)
20260819 Added: pluginst.inf and VERSIONINFO; Visual Studio 2022 project
20181230 Release v1.3
20181230 Added: German translation of connection settings dialog (Alt+Enter on connection name)
20181230 Fixed: Use DATE_CREATED instead of DATE_MODIFIED on devices which do not support DATE_MODIFIED
20131001 Release v1.2
20131001 Fixed: The plugin could delete the wrong files when there were more than 128 in a directory
20130701 Release v1.1
20130701 Fixed: Folders were recognized as files when the file system returned a size for them
20130701 Added: Let the user choose via Alt+Enter on the device name whether it sends time stamps as local time or UTC
20111230 Release v1.0
20111230 Release v0.7 beta
20111230 Fixed: The plugin didn't return the "friendly name" when more than 1 device was connected at the same time
20111229 Added: Copy cover art with audio files (untested, please report any errors shown in the log!)
20111229 Added: Set the OBJECT_NAME of uploaded audio files to the music title instead of the file name
20111219 Release v0.6 beta
20111219 Fixed: Metadata was not sent correctly
20111219 Fixed: Remove any name from the cache when it's renamed, deleted or uploaded, because the ID may change on some devices
20111219 Fixed: When uploading, we need to set the size of the uploaded file before sending the data
20111215 Initial Release v0.5 beta
