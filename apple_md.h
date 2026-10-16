#pragma once
#include <windows.h>
#include "wpdplug_int.h"

void AppleMdInit(void);
void AppleMdShutdown(void);
void AppleMdResetSessions(void);
/* Plugin-root listing only. At most once a minute. */
void AppleMdResetSessionsCached(void);
int AppleMdCount(void);
BOOL AppleMdGetName(int index, WCHAR* name, int cch);
BOOL AppleMdIsDeviceName(LPCWSTR name);
BOOL AppleMdFillInfo(LPCWSTR deviceName, PluginDeviceInfo* info);
BOOL AppleMdFindFirst(LPCWSTR deviceName, LPCWSTR relPath, WIN32_FIND_DATAW* fd, HANDLE* out);
BOOL AppleMdFindNext(HANDLE h, WIN32_FIND_DATAW* fd);
void AppleMdFindClose(HANDLE h);
int AppleMdGetFile(LPCWSTR deviceName, LPCWSTR relPath, LPCWSTR localPath,
	ULONGLONG totalHint, FILETIME* mtime);
/* 1 when localJpeg is a cached thumbnail for an iPhone Photos file. */
int AppleMdLoadPhotoThumb(LPCWSTR deviceName, LPCWSTR relPath, WCHAR* localJpeg, int cch);
int AppleMdPutFile(LPCWSTR deviceName, LPCWSTR relPath, LPCWSTR localPath, BOOL overwrite);
BOOL AppleMdDelete(LPCWSTR deviceName, LPCWSTR relPath);
BOOL AppleMdMkDir(LPCWSTR deviceName, LPCWSTR relPath);
