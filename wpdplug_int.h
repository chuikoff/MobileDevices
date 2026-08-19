#pragma once

#include <windows.h>
#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include "cunicode.h"

BOOL InitFunctionsIfNeeded(BOOL trueconnect);
HRESULT GetFolderIDFromPathName(LPWSTR pPath, IEnumPortableDeviceObjectIDs** pEnumObjectIDsRetVal,
	IPortableDeviceProperties** pPropertiesRetVal, IPortableDeviceContent** pDeviceContent, LPWSTR* pStorageIDRetVal);

void LockPlugin(void);
void UnlockPlugin(void);
BOOL EnsureComApartment(void);
void RequestAbort(void);
void ResetAbort(void);
void SetCancelDevice(IPortableDevice* pDevice);
BOOL GdiPlusInitialize(void);
IPortableDevice* FindStoredDeviceByPath(LPCWSTR path);
int ProgressCheck(WCHAR* src, WCHAR* dst, int percent);
BOOL IsAbortRequested(void);
void SetContentStop(BOOL stop);
BOOL IsContentStop(void);

extern HINSTANCE hInst;
extern int PluginNumber;
extern BOOL DeviceEventReceived;

void MarkObjectCacheDirty(void);
void ClearCache(void);
HRESULT AdviseWpdDevice(IPortableDevice* dev, LPWSTR* cookie);
void UnadviseWpdDevice(IPortableDevice* dev, LPWSTR cookie);
void EnsureWpdEventsAdvised(void);
BOOL ShouldHideWpdDevice(IPortableDeviceManager* mgr, LPCWSTR pnpId, LPCWSTR friendly);
BOOL EjectWpdDevice(LPCWSTR pnpId);
void ShowDeviceInfoBox(HWND parent, LPCWSTR remoteName);
PWSTR FindPnpIdByPath(LPCWSTR path);
extern volatile LONG g_cacheDirty;
