#pragma once

#include <windows.h>
#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include "cunicode.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p)=NULL; } } while (0)
#endif

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

#define DEVICE_INFO_MAX_STOR 8
typedef struct {
	WCHAR os[40];
	WCHAR manufacturer[128];
	WCHAR model[128];
	WCHAR firmware[128];
	WCHAR protocol[80];
	int battery;
	int nstor;
	struct {
		WCHAR name[80];
		ULONGLONG freeBytes;
		ULONGLONG capacityBytes;
	} stor[DEVICE_INFO_MAX_STOR];
} PluginDeviceInfo;

BOOL QueryDeviceInfo(LPCWSTR remoteName, PluginDeviceInfo* info);
void FormatDeviceInfo(int lang, const PluginDeviceInfo* info, WCHAR* out, int outcch);
extern volatile LONG g_cacheDirty;
