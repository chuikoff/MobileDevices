#pragma once
#include <windows.h>
#include "wpdplug_int.h"

void AppleMdInit(void);
void AppleMdShutdown(void);
int AppleMdCount(void);
BOOL AppleMdGetName(int index, WCHAR* name, int cch);
BOOL AppleMdIsDeviceName(LPCWSTR name);
BOOL AppleMdFillInfo(LPCWSTR deviceName, PluginDeviceInfo* info);
BOOL AppleMdFindFirst(LPCWSTR deviceName, LPCWSTR relPath, WIN32_FIND_DATAW* fd, HANDLE* out);
BOOL AppleMdFindNext(HANDLE h, WIN32_FIND_DATAW* fd);
void AppleMdFindClose(HANDLE h);
int AppleMdGetFile(LPCWSTR deviceName, LPCWSTR relPath, LPCWSTR localPath,
	ULONGLONG totalHint, FILETIME* mtime);
int AppleMdPutFile(LPCWSTR deviceName, LPCWSTR relPath, LPCWSTR localPath, BOOL overwrite);
BOOL AppleMdDelete(LPCWSTR deviceName, LPCWSTR relPath);
BOOL AppleMdMkDir(LPCWSTR deviceName, LPCWSTR relPath);
