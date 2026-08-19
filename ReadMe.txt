MTP Devices plugin v1.4
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

Existing MediaAudio2 timezone settings (Alt+Enter on the device) are still
read; new settings are stored under [MTPDevices].

History
=======
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
