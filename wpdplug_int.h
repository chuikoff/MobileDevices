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
void UninitComApartment(void);

class ComApartmentGuard {
	BOOL m_ok;
public:
	ComApartmentGuard();
	~ComApartmentGuard();
	BOOL ok() const { return m_ok; }
private:
	ComApartmentGuard(const ComApartmentGuard&);
	ComApartmentGuard& operator=(const ComApartmentGuard&);
};
void RequestAbort(void);
void ResetAbort(void);

// One GetFile/PutFile operation. AddRef's the WPD device so RequestAbort can
// call IPortableDevice::Cancel() from another thread without a dangling pointer.
class TransferScope {
	mutable volatile LONG m_abort;
	IPortableDevice* m_device;
	TransferScope* m_next;
	static TransferScope* s_head;
	static CRITICAL_SECTION s_cs;
public:
	explicit TransferScope(IPortableDevice* device);
	~TransferScope();
	BOOL aborted() const;
	static void Init(void);
	static void Uninit(void);
	static void AbortAll(void);
private:
	TransferScope(const TransferScope&);
	TransferScope& operator=(const TransferScope&);
};

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
	WCHAR serial[64];
	WCHAR imei[64];
	WCHAR imei2[64];
	int battery;
	int batteryHealth;   /* -1 = hide; maximum capacity % */
	int batteryCycles;   /* -1 = hide */
	ULONGLONG designCapacity; /* mAh raw, 0 = unset */
	ULONGLONG maxCapacity;    /* mAh raw, 0 = unset */
	int nstor;
	struct {
		WCHAR name[80];
		ULONGLONG freeBytes;
		ULONGLONG capacityBytes;
	} stor[DEVICE_INFO_MAX_STOR];
	/* Apple disk_usage detail (0 = unset / hide) */
	ULONGLONG totalDisk;
	ULONGLONG dataCapacity;
	ULONGLONG dataAvailable;
	ULONGLONG systemCapacity;
	ULONGLONG systemAvailable;
	ULONGLONG photoUsage;
	ULONGLONG appUsage;
} PluginDeviceInfo;

BOOL QueryDeviceInfo(LPCWSTR remoteName, PluginDeviceInfo* info);
void FormatDeviceInfo(int lang, const PluginDeviceInfo* info, WCHAR* out, int outcch);
extern volatile LONG g_cacheDirty;
