// wpdplug.cpp : Definiert den Einstiegspunkt f�r die DLL-Anwendung.
//

#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include <stdio.h>
#include <wchar.h>
#include <Dbt.h>
#include <GdiPlus.h>
#include "wpdplug.h"
#include "cunicode.h"
#include "fsplugin.h"
#include "utils.h"
#include "connectionsettings.h"
#include "wpdplug_int.h"
#include "apple_md.h"

#define DefPluginTitle PLUGIN_DISPLAY_NAME

CRITICAL_SECTION g_cs;
volatile LONG g_abort=0;
volatile LONG g_contentStop=0;
IPortableDevice* g_cancelDevice=NULL;

WCHAR DefaultIniNameW[MAX_PATH];
HINSTANCE hInst;
tProgressProc ProgressProc;
tLogProc LogProc;
tRequestProc RequestProc;
tProgressProcW ProgressProcW;
tLogProcW LogProcW;
tRequestProcW RequestProcW;
int PluginNumber;
BOOL connected=false;
BOOL DeviceEventReceived=false;   // if true, we need to re-scan all devices
volatile LONG g_cacheDirty=0;

void MarkObjectCacheDirty(void)
{
	InterlockedExchange(&g_cacheDirty, 1);
}

#define NUM_OBJECTS_TO_REQUEST 128

BOOL LoadAllDevices();
void FreeAllDevices();
void FreeDeviceList();
void LogWpdError(LPCWSTR context, HRESULT hr);

ULONG_PTR GdiPlusToken=0;

BOOL GdiPlusInitialize()
{
	if (GdiPlusToken)
		return true;

	Gdiplus::GdiplusStartupInput Gsi;
	memset(&Gsi,0,sizeof(Gsi));
	Gsi.GdiplusVersion=1;
	if (Gdiplus::GdiplusStartup(&GdiPlusToken,&Gsi,NULL)!=0)
        GdiPlusToken=0;
	return (GdiPlusToken!=0);
}

void ShutdownGdiPlus()   // do NOT call this from DLL_PROCESS_DETACH!
{
	if (GdiPlusToken) {
		Gdiplus::GdiplusShutdown(GdiPlusToken);
		GdiPlusToken=0;
	}
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		hInst=(HINSTANCE)hModule;
		InitializeCriticalSection(&g_cs);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		DeleteCriticalSection(&g_cs);
		break;
	}
    return TRUE;
}

// maxlen is the destination buffer size including the terminating 0
char* strlcpy(char* p,const char* p2,int maxlen)
{
	if (!p || maxlen<=0)
		return p;
	if (!p2) {
		p[0]=0;
		return p;
	}
	strncpy_s(p,maxlen,p2,_TRUNCATE);
	return p;
}

char* strlcat(char* p,const char* p2,int maxlen)
{
	if (!p || !p2 || maxlen<=0)
		return p;
	strncat_s(p,maxlen,p2,_TRUNCATE);
	return p;
}

typedef struct {
	WCHAR Path[wdirtypemax];
	WCHAR LastFoundName[wdirtypemax];
	IEnumPortableDeviceObjectIDs* pEnumObjectIDs;
	IPortableDeviceKeyCollection* pPropertiesToRead;
	IPortableDeviceProperties* pProperties;
	DWORD pPnpDeviceLastRead;
	LPWSTR szObjectIDArray[NUM_OBJECTS_TO_REQUEST];
	IPortableDeviceValues* szObjectValues[NUM_OBJECTS_TO_REQUEST];
	DWORD szObjectIDsFetched;
	DWORD szObjectIDLastRead;
	int LocalTime;
	DWORD listedCount;
	int listKind;
	HANDLE appleFind;
} tLastFindStuct,*pLastFindStuct;

BOOL initialized=FALSE;
BOOL firstinitialized=FALSE;
BOOL weInitializedCOM=FALSE;
IPortableDeviceManager* pDevMgr=NULL;
PWSTR* StoredPnpDeviceIDs=NULL;   // keep them globally!
PWSTR* StoredPnPFriendlyNames=NULL;
IPortableDevice** StoredDevices=NULL;
LPWSTR* StoredEventCookies=NULL;
DWORD StoredNumIds=0;

PWSTR FindPnpIdByPath(LPCWSTR path)
{
	if (!path)
		return NULL;
	WCHAR name[MAX_PATH];
	const WCHAR* p=path;
	if (p[0]=='\\')
		p++;
	wcslcpy(name,p,MAX_PATH-1);
	WCHAR* slash=wcschr(name,'\\');
	if (slash)
		slash[0]=0;
	for (DWORD i=0;i<StoredNumIds;i++) {
		if (StoredPnPFriendlyNames[i] && wcscmp(StoredPnPFriendlyNames[i],name)==0)
			return StoredPnpDeviceIDs[i];
	}
	return NULL;
}

HANDLE hDevNotify=NULL;
HWND hWndNotify=NULL;

void LockPlugin(void)
{
	EnterCriticalSection(&g_cs);
}

void UnlockPlugin(void)
{
	LeaveCriticalSection(&g_cs);
}

BOOL EnsureComApartment(void)
{
	HRESULT hr=CoInitialize(NULL);
	return (hr==S_OK || hr==S_FALSE);
}

void SetCancelDevice(IPortableDevice* pDevice)
{
	g_cancelDevice=pDevice;
}

IPortableDevice* FindStoredDeviceByPath(LPCWSTR path)
{
	if (!path || path[0]==0)
		return NULL;
	WCHAR name[MAX_PATH];
	const WCHAR* p=path;
	if (p[0]=='\\')
		p++;
	wcslcpy(name,p,MAX_PATH-1);
	WCHAR* slash=wcschr(name,'\\');
	if (slash)
		slash[0]=0;
	for (DWORD i=0;i<StoredNumIds;i++) {
		if (StoredPnPFriendlyNames[i] && wcscmp(StoredPnPFriendlyNames[i],name)==0)
			return StoredDevices[i];
	}
	return NULL;
}

void RequestAbort(void)
{
	InterlockedExchange(&g_abort,1);
	if (g_cancelDevice)
		g_cancelDevice->Cancel();
}

void ResetAbort(void)
{
	InterlockedExchange(&g_abort,0);
}

BOOL IsAbortRequested(void)
{
	return InterlockedCompareExchange(&g_abort,0,0)!=0;
}

void SetContentStop(BOOL stop)
{
	InterlockedExchange(&g_contentStop, stop ? 1 : 0);
}

BOOL IsContentStop(void)
{
	return InterlockedCompareExchange(&g_contentStop,0,0)!=0;
}

int ProgressCheck(WCHAR* src, WCHAR* dst, int percent)
{
	LeaveCriticalSection(&g_cs);
	int err=ProgressProcT(PluginNumber,src,dst,percent);
	EnterCriticalSection(&g_cs);
	if (err)
		RequestAbort();
	return err;
}

static BOOL IsWpdFolderContentType(REFGUID contentType)
{
	return IsEqualGUID(contentType, WPD_CONTENT_TYPE_FOLDER)
		|| IsEqualGUID(contentType, WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT)
		|| IsEqualGUID(contentType, WPD_CONTENT_TYPE_AUDIO_ALBUM)
		|| IsEqualGUID(contentType, WPD_CONTENT_TYPE_IMAGE_ALBUM)
		|| IsEqualGUID(contentType, WPD_CONTENT_TYPE_VIDEO_ALBUM)
		|| IsEqualGUID(contentType, WPD_CONTENT_TYPE_MIXED_CONTENT_ALBUM);
}

static BOOL IsWpdFolderObject(IPortableDeviceValues* pObjectProperties)
{
	if (!pObjectProperties)
		return TRUE;  // same fallback as before: treat unknown as a folder
	GUID contentType=GUID_NULL;
	HRESULT hr=pObjectProperties->GetGuidValue(WPD_OBJECT_CONTENT_TYPE,&contentType);
	if (FAILED(hr))
		return TRUE;
	return IsWpdFolderContentType(contentType);
}

void LogWpdError(LPCWSTR context, HRESULT hr)
{
	WCHAR buf[512];
	buf[0]=0;
	if (hr==HRESULT_FROM_WIN32(ERROR_BUSY) || hr==HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)) {
		swprintf_s(buf, L"%s failed (0x%08X): device is in use by Explorer, Phone Link or another program. Close that access and retry.",
			context ? context : L"WPD", hr);
	} else if (hr==HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) || hr==E_ACCESSDENIED) {
		swprintf_s(buf, L"%s failed (0x%08X): access denied. The device may be locked, in charging-only mode, or MTP may be disabled.",
			context ? context : L"WPD", hr);
	} else if (hr==HRESULT_FROM_WIN32(ERROR_NOT_READY) || hr==HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
		swprintf_s(buf, L"%s failed (0x%08X): device not ready or not found. Unlock the phone and set USB to File transfer (MTP).",
			context ? context : L"WPD", hr);
	} else {
		swprintf_s(buf, L"%s failed (0x%08X).", context ? context : L"WPD", hr);
	}
	LogProcT(PluginNumber, MSGTYPE_IMPORTANTERROR, buf);
}

static HRESULT CreateWpdClientInfo(IPortableDeviceValues** ppClientInfo, DWORD desiredAccess)
{
	if (!ppClientInfo)
		return E_POINTER;
	*ppClientInfo=NULL;
	IPortableDeviceValues* info=NULL;
	HRESULT hr=CoCreateInstance(CLSID_PortableDeviceValues, NULL, CLSCTX_INPROC_SERVER,
		IID_IPortableDeviceValues, (VOID**)&info);
	if (FAILED(hr))
		return hr;
	info->SetStringValue(WPD_CLIENT_NAME, PLUGIN_WPD_CLIENT_NAME);
	info->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION, PLUGIN_VERSION_MAJOR);
	info->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION, PLUGIN_VERSION_MINOR);
	info->SetUnsignedIntegerValue(WPD_CLIENT_REVISION, PLUGIN_VERSION_REV);
	info->SetUnsignedIntegerValue(WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE, SECURITY_IMPERSONATION);
	info->SetUnsignedIntegerValue(WPD_CLIENT_DESIRED_ACCESS, desiredAccess);
	info->SetUnsignedIntegerValue(WPD_CLIENT_SHARE_MODE, FILE_SHARE_READ | FILE_SHARE_WRITE);
	*ppClientInfo=info;
	return S_OK;
}

static HRESULT OpenWpdDevice(LPCWSTR deviceId, IPortableDevice** ppDevice)
{
	if (!ppDevice)
		return E_POINTER;
	*ppDevice=NULL;
	if (!deviceId)
		return E_INVALIDARG;

	IPortableDeviceValues* client=NULL;
	HRESULT hr=CreateWpdClientInfo(&client, GENERIC_READ | GENERIC_WRITE);
	if (FAILED(hr))
		return hr;

	IPortableDevice* dev=NULL;
	hr=CoCreateInstance(CLSID_PortableDevice, NULL, CLSCTX_INPROC_SERVER, IID_IPortableDevice, (VOID**)&dev);
	if (SUCCEEDED(hr))
		hr=dev->Open(deviceId, client);

	if (FAILED(hr)) {
		LogWpdError(L"Open(read-write)", hr);
		if (dev) {
			dev->Release();
			dev=NULL;
		}
		client->SetUnsignedIntegerValue(WPD_CLIENT_DESIRED_ACCESS, GENERIC_READ);
		HRESULT hr2=CoCreateInstance(CLSID_PortableDevice, NULL, CLSCTX_INPROC_SERVER, IID_IPortableDevice, (VOID**)&dev);
		if (SUCCEEDED(hr2)) {
			hr2=dev->Open(deviceId, client);
			if (SUCCEEDED(hr2))
				hr=hr2;
			else {
				LogWpdError(L"Open(read-only)", hr2);
				dev->Release();
				dev=NULL;
				hr=hr2;
			}
		} else
			hr=hr2;
	}
	client->Release();
	*ppDevice=dev;
	return hr;
}

static void SanitizeFriendlyName(LPWSTR name)
{
	if (!name)
		return;
	for (WCHAR* p=name; *p; p++) {
		WCHAR ch=*p;
		if (ch==L'\\' || ch==L'/' || ch==L'*' || ch==L'?' ||
			ch==L'<' || ch==L'>' || ch==L':' || ch==L'"' || ch==L'|')
			*p=L'_';
	}
	wcutlastbackslash(name);
}

static void MakeFriendlyNameUnique(DWORD index)
{
	if (!StoredPnPFriendlyNames || !StoredPnPFriendlyNames[index])
		return;
	WCHAR base[256];
	wcslcpy(base, StoredPnPFriendlyNames[index], 256);
	int n=2;
	for (;;) {
		BOOL clash=FALSE;
		for (DWORD k=0; k<index; k++) {
			if (StoredPnPFriendlyNames[k] && wcscmp(StoredPnPFriendlyNames[index], StoredPnPFriendlyNames[k])==0) {
				clash=TRUE;
				break;
			}
		}
		if (!clash)
			break;
		WCHAR tmp[280];
		swprintf_s(tmp, L"%s (%d)", base, n++);
		CoTaskMemFree(StoredPnPFriendlyNames[index]);
		StoredPnPFriendlyNames[index]=wstrnew(tmp);
		if (!StoredPnPFriendlyNames[index])
			break;
	}
}

BOOL InitFunctionsIfNeeded(BOOL trueconnect)
{
	HRESULT hr;
	BOOL result=false;

	if (!firstinitialized) {
		HRESULT cohr=CoInitialize(NULL);
		firstinitialized=true;
		if (cohr==S_OK)
			weInitializedCOM=TRUE;
	}
	if (!initialized) {
		initialized=true;
		hr = CoCreateInstance(CLSID_PortableDeviceManager, NULL, CLSCTX_INPROC_SERVER,
			IID_IPortableDeviceManager, (VOID**) &pDevMgr);
		if SUCCEEDED(hr) {
			result=true;
		} else {
			LogWpdError(L"CoCreateInstance(PortableDeviceManager)", hr);
			MessageBoxA(GetActiveWindow(),
				"Windows Portable Devices (WPD) is not available.\nThis plugin requires Windows Vista or later with WPD support.",
				PLUGIN_DISPLAY_NAME, MB_ICONSTOP);
			result=false;
		}
	} else
		result=true;
	AppleMdInit();
	if (result && pDevMgr && StoredNumIds==0)
		LoadAllDevices();
	return (result && StoredNumIds>0) || AppleMdCount()>0 || result;
}

// From: http://blogs.msdn.com/b/wpdblog/archive/2007/03/17/how-to-receive-wpd-device-arrival-events.aspx
#define WPD_DEVINTERFACE L"{6ac27878-a6fa-4155-ba85-f98f491d4f33}"
#define szWindowClass "PnpNotificationListener"
GUID guidDevInterface=GUID_NULL;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{  
	switch (uMsg) {
		case WM_CREATE:
		{
			DEV_BROADCAST_DEVICEINTERFACE db = {0};
			db.dbcc_size = sizeof(db);
			db.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
			CLSIDFromString(WPD_DEVINTERFACE, &guidDevInterface);
			db.dbcc_classguid  = guidDevInterface;
			hDevNotify = RegisterDeviceNotificationW(hWnd, &db, DEVICE_NOTIFY_WINDOW_HANDLE);
			return 1;
		}
		case WM_DEVICECHANGE:
			switch(wParam)
			{
				case DBT_DEVICEARRIVAL:
				case DBT_DEVICEREMOVECOMPLETE:
				{
					PDEV_BROADCAST_HDR pdbh = (PDEV_BROADCAST_HDR)lParam;
					if (pdbh != NULL && pdbh->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
					{
						PDEV_BROADCAST_DEVICEINTERFACE pdbi = (PDEV_BROADCAST_DEVICEINTERFACE)lParam;
						if (IsEqualGUID(pdbi->dbcc_classguid, guidDevInterface)) 
						{
							DeviceEventReceived=true;
						}
					}
				 }
                 break;         
			} // switch (wParam)
			return 0;
		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	} // switch (uMsg)
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASS wc;
	memset(&wc,0,sizeof(wc));
	wc.style			= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	= (WNDPROC)WndProc;
	wc.hInstance		= hInstance;
	wc.lpszClassName	= szWindowClass;
	return RegisterClass(&wc);
}

BOOL RegisterForEventNotifications(void)
{
	if (!hWndNotify) {
		HINSTANCE hInstance=GetModuleHandle(NULL);
		MyRegisterClass(hInstance);
		hWndNotify=CreateWindow(szWindowClass, szWindowClass, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
	}
	return hWndNotify!=NULL;
}

void UnregisterNotification(void) {
	if (hDevNotify) {
		UnregisterDeviceNotification(hDevNotify);
		hDevNotify=NULL;
	}
	if (hWndNotify) {
		DestroyWindow(hWndNotify);
		hWndNotify=NULL;
	}
}

#define CACHESIZE 32
LPWSTR CachedDirs[CACHESIZE]={0};
DWORD CachedDirLens[CACHESIZE]={0};
LPWSTR CachedObjectIds[CACHESIZE]={0};
DWORD CachedLastUsedTimes[CACHESIZE]={0};

void ClearCache() {
	for (int i=0;i<CACHESIZE;i++) {
		if (CachedDirs[i]!=NULL)
			CoTaskMemFree(CachedDirs[i]);
		if (CachedObjectIds[i]!=NULL)
			CoTaskMemFree(CachedObjectIds[i]);
	}
	memset(&CachedDirs,0,sizeof(CachedDirs));
	memset(&CachedDirLens,0,sizeof(CachedDirLens));
	memset(&CachedObjectIds,0,sizeof(CachedObjectIds));
	memset(&CachedLastUsedTimes,0,sizeof(CachedLastUsedTimes));
}

void RemoveNameFromCache(LPWSTR pdir) {
	WCHAR fulldir[1024];
	wcslcpy(fulldir,pdir,1022);
	wcslcatbackslash(fulldir,1023);
	DWORD len=(DWORD)wcslen(fulldir);
	if (len<=1)
		return;
	// look whether the dir or any subdir in it is already in the cache
	for (int i=0;i<CACHESIZE;i++) {
		if (CachedDirLens[i]>=len && wcsncmp(fulldir,CachedDirs[i],len)==0) {
			CoTaskMemFree(CachedDirs[i]);
			CachedDirs[i]=NULL;
			CoTaskMemFree(CachedObjectIds[i]);
			CachedObjectIds[i]=NULL;
			CachedDirLens[i]=0;
			CachedLastUsedTimes[i]=0;
		}
	}

}

void RemoveFullPathFromCache(LPWSTR pdir) {
	if (!pdir || wcslen(pdir)<2)
		return;
	if (pdir[0]=='\\')
		RemoveNameFromCache(pdir+1);
	else
		RemoveNameFromCache(pdir);
}

void AddNameToCache(LPWSTR deviceName,LPWSTR pdir,LPWSTR pObjectId,DWORD time) {
	WCHAR fulldir[1024];
	if (!deviceName || !deviceName[0])
		return;
	wcslcpy(fulldir,deviceName,1022);
	wcslcatbackslash(fulldir,1023);
	if (pdir && pdir[0])
		wcslcat(fulldir,pdir,1022);
	wcslcatbackslash(fulldir,1023);
	DWORD len=(DWORD)wcslen(fulldir);
	if (len<=1)
		return;
	// first, look whether the dir is already in the cache
	for (int i=0;i<CACHESIZE;i++) {
		if (CachedDirLens[i]==len && wcscmp(fulldir,CachedDirs[i])==0)
			return;
	}
	// second, look for a free space
	int lastmatch=-1;
	for (int i=0;i<CACHESIZE;i++) {
		if (CachedDirs[i]==0) {
			lastmatch=i;
			break;
		}
	}
	// third, look for the one with the oldest timestamp
	if (lastmatch<0) {
		DWORD oldesttime=0xFFFFFFFF;
		for (int i=0;i<CACHESIZE;i++) {
			if (CachedLastUsedTimes[i]<oldesttime) {
				oldesttime=CachedLastUsedTimes[i];
				lastmatch=i;
			}
		}
		if (lastmatch>=0) {
			CoTaskMemFree(CachedDirs[lastmatch]);
			CoTaskMemFree(CachedObjectIds[lastmatch]);
		}
	}
	CachedDirs[lastmatch]=wstrnew(fulldir);
	CachedDirLens[lastmatch]=len;
	CachedObjectIds[lastmatch]=wstrnew(pObjectId);
	CachedLastUsedTimes[lastmatch]=time;
}

BOOL FindNameInCache(LPWSTR deviceName,LPWSTR pdir,LPWSTR* returnedObjectId,LPWSTR* pdirsubstart) {
	WCHAR fulldir[1024];
	if (!deviceName || !deviceName[0])
		return false;
	wcslcpy(fulldir,deviceName,1022);
	wcslcatbackslash(fulldir,1023);
	DWORD devicePartLen=(DWORD)wcslen(fulldir);
	if (pdir && pdir[0])
		wcslcat(fulldir,pdir,1022);
	wcslcatbackslash(fulldir,1023);
	DWORD maxmatchlen=0;
	int lastmatch=-1;
	// find longest match!
	for (int i=0;i<CACHESIZE;i++) {
		if (CachedDirs[i]) {
			if (CachedDirLens[i]>maxmatchlen) {
				if (wcsncmp(CachedDirs[i],fulldir,CachedDirLens[i])==0) {
					lastmatch=i;
					maxmatchlen=CachedDirLens[i];
				}
			}
		}
	}
	if (lastmatch>=0) {
		DWORD matchedSubLen=(maxmatchlen>devicePartLen) ? (maxmatchlen-devicePartLen) : 0;
		if (pdir && wcslen(pdir)>matchedSubLen)
			*pdirsubstart=pdir+matchedSubLen;
		else if (pdir)
			*pdirsubstart=pdir+wcslen(pdir);  // full match
		else
			*pdirsubstart=pdir;
		*returnedObjectId=CachedObjectIds[lastmatch];
		return true;
	}
	return false;
}

static void PickObjectDisplayName(IPortableDeviceValues* v, WCHAR* out, DWORD cch);

HRESULT GetFolderIDFromPathName(LPWSTR pPath,IEnumPortableDeviceObjectIDs** pEnumObjectIDsRetVal,
	IPortableDeviceProperties** pPropertiesRetVal,IPortableDeviceContent** pDeviceContent,LPWSTR* pStorageIDRetVal)
{
	if (StoredPnPFriendlyNames==NULL || StoredPnpDeviceIDs==NULL)
		return E_FAIL;
	// wsSearch has form \Devicename\path1\path2
	// First, get Device

	IPortableDevice* pDevice=NULL;
	IPortableDeviceContent* pContent=NULL;
	IEnumPortableDeviceObjectIDs* pEnumObjectIDs=NULL;
	IPortableDeviceProperties* pProperties=NULL;
	IPortableDeviceKeyCollection* pPropertiesToRead=NULL;
	WCHAR *pname,*pnext,*pdirstart,*pdirsubstart;
	HRESULT hr;
	WCHAR wcSearch[1024];

	wcslcpy(wcSearch,pPath+1,1023);
	pdirstart=wcschr(wcSearch,'\\');
	if (pdirstart) {
		pdirstart[0]=0;
		pdirstart++;
	} else {
		pdirstart=wcSearch+wcslen(wcSearch);  // "\Device" with no trailing slash
	}
	if (wcSearch[0]==0)
		return E_FAIL;
	// Find the right device, p points to it!

	WCHAR* DeviceID=NULL;
	DWORD DeviceIndex=0;
	for (DeviceIndex=0;DeviceIndex<StoredNumIds;DeviceIndex++) {
		if (wcscmp((const wchar_t *)StoredPnPFriendlyNames[DeviceIndex],wcSearch)==0) {
			DeviceID=StoredPnpDeviceIDs[DeviceIndex];
			pDevice=StoredDevices[DeviceIndex];
			break;
		}
	}
	if (DeviceID==NULL) {
		LogProc(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,"Device not found!");
		return E_FAIL;
	}
	if (pDevice==NULL) {
		hr = OpenWpdDevice(DeviceID, &pDevice);
		if (SUCCEEDED(hr))
			StoredDevices[DeviceIndex]=pDevice;
		else
			return hr;
	} else
		hr=S_OK;
	if (SUCCEEDED(hr)) {
		hr = pDevice->Content(&pContent);
		if (FAILED(hr)) {
			LogProc(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,"No content on Device!");
			return hr;
		} 
	}
	PWSTR returnedObjectId=NULL;
	pdirsubstart=pdirstart;
	if (!FindNameInCache(wcSearch,pdirstart,&returnedObjectId,&pdirsubstart))
		returnedObjectId=WPD_DEVICE_OBJECT_ID;

	if (pdirsubstart[0]==NULL) { // device root
		if (pEnumObjectIDsRetVal) {
			hr = pContent->EnumObjects(0,               // Flags are unused
                     returnedObjectId,     // Starting from the passed in object
                     NULL,            // Filter is unused
                     &pEnumObjectIDs);
			*pEnumObjectIDsRetVal=pEnumObjectIDs;
		} else 
			hr=S_OK;
		if (pPropertiesRetVal)
			pContent->Properties(pPropertiesRetVal);
		if (pDeviceContent)
			*pDeviceContent=pContent;
		else
			pContent->Release();
		if (pStorageIDRetVal)
			*pStorageIDRetVal=wstrnew(returnedObjectId);
		return hr;
	}

	// ShowSupportedFormats(pIDevice);
	if (SUCCEEDED(hr))
		hr = pContent->Properties(&pProperties);
	if (SUCCEEDED(hr))
		hr = CoCreateInstance(CLSID_PortableDeviceKeyCollection,
                      NULL,
                      CLSCTX_INPROC_SERVER,
					  IID_IPortableDeviceKeyCollection,
                      (VOID**) &pPropertiesToRead);
	if (SUCCEEDED(hr)) {
		pPropertiesToRead->Add(WPD_OBJECT_NAME);
		pPropertiesToRead->Add(WPD_OBJECT_ORIGINAL_FILE_NAME);
		pPropertiesToRead->Add(WPD_STORAGE_DESCRIPTION);
		pPropertiesToRead->Add(WPD_FUNCTIONAL_OBJECT_CATEGORY);
	}
	LPWSTR LastParentName=wstrnew(WPD_DEVICE_OBJECT_ID);
	if (SUCCEEDED(hr))
		hr = pContent->EnumObjects(0,               // Flags are unused
                                  returnedObjectId,     // Starting from the passed in object
                                  NULL,            // Filter is unused
                                  &pEnumObjectIDs);
	if (FAILED(hr)) {
		if (pPropertiesToRead)
			pPropertiesToRead->Release();
		if (pProperties)
			pProperties->Release();
		if (pEnumObjectIDs)
			pEnumObjectIDs->Release();
		if (pContent)
			pContent->Release();
		return hr;
	}
	pname=pdirsubstart;
	pnext=wcschr(pname,'\\');
	if (pnext)
		pnext[0]=0;

	DWORD cFetched=0;
	DWORD time=GetTickCount();
	while (SUCCEEDED(hr)) {
		PWSTR szObjectIDArray[NUM_OBJECTS_TO_REQUEST];
		hr = pEnumObjectIDs->Next(NUM_OBJECTS_TO_REQUEST,   // Number of objects to request on each NEXT call
			                          szObjectIDArray,          // Array of PWSTR array which will be populated on each NEXT call
				                      &cFetched);               // Number of objects written to the PWSTR array
		DWORD i=0;
		if (cFetched==0)
			hr=E_FAIL;
		while (SUCCEEDED(hr) && i<cFetched) {
			IPortableDeviceValues* pObjectProperties=NULL;
			HRESULT hr2=pProperties->GetValues(szObjectIDArray[i],
                                pPropertiesToRead,   // The properties we want to read
                                &pObjectProperties); // Driver supplied property values for the specified object
			if (SUCCEEDED(hr2) && pObjectProperties) {
				WCHAR showname[MAX_PATH];
				PickObjectDisplayName(pObjectProperties, showname, MAX_PATH);
				pObjectProperties->Release();
				pObjectProperties=NULL;

				if (wcscmp(showname,pname)==0) {  //found!!
					AddNameToCache(wcSearch,pdirstart,szObjectIDArray[i],time);
					pEnumObjectIDs->Release();
					pEnumObjectIDs=NULL;

					CoTaskMemFree(LastParentName);
					LastParentName=wstrnew(szObjectIDArray[i]);
					for (i=0;i<cFetched;i++)
						CoTaskMemFree(szObjectIDArray[i]);
					cFetched=0;

					pname=pnext;
					if (pname) {
						pname[0]='\\';
						pname++;
						pnext=wcschr(pname,'\\');
						if (pnext)
							pnext[0]=0;
						if (pname[0]==0)
							pname=NULL;
					}
					if (pname || pEnumObjectIDsRetVal) {
						hr = pContent->EnumObjects(0,               // Flags are unused
									  LastParentName,               // Subitem
									  NULL,                         // Filter is unused
									  &pEnumObjectIDs);
						if (!SUCCEEDED(hr))    // a file?
							pEnumObjectIDs=NULL;
					}
					if (!pname) {   //we are done!
						if (pEnumObjectIDsRetVal)
							*pEnumObjectIDsRetVal=pEnumObjectIDs;
						if (pPropertiesRetVal)
							*pPropertiesRetVal=pProperties;
						else if (pProperties)
							pProperties->Release();
						if (pDeviceContent)
							*pDeviceContent=pContent;
						else
							pContent->Release();
						if (pStorageIDRetVal)
							*pStorageIDRetVal=LastParentName;
						else
							CoTaskMemFree(LastParentName);
						pPropertiesToRead->Release();
						return S_OK;
					}
				}
			}
			i++;
		}
		for (i=0;i<cFetched;i++)
			CoTaskMemFree(szObjectIDArray[i]);
	}
	if (pPropertiesToRead)
		pPropertiesToRead->Release();
	if (pProperties)
		pProperties->Release();
	if (pEnumObjectIDs)
		pEnumObjectIDs->Release();
	if (pContent)
		pContent->Release();
	if (LastParentName)
		CoTaskMemFree(LastParentName);
	return E_FAIL;
}

static BOOL NameContainsI(LPCWSTR hay, LPCWSTR needle)
{
	if (!hay || !needle || !needle[0])
		return FALSE;
	size_t n=wcslen(needle);
	for (LPCWSTR p=hay; *p; p++) {
		if (_wcsnicmp(p, needle, n)==0)
			return TRUE;
	}
	return FALSE;
}

static BOOL IsGenericWpdName(LPCWSTR name)
{
	if (!name || !name[0])
		return TRUE;
	WCHAR buf[256];
	wcslcpy(buf, name, 256);
	WCHAR* s=buf;
	while (*s==L' ' || *s==L'\t')
		s++;
	WCHAR* e=s+wcslen(s);
	while (e>s && (e[-1]==L' ' || e[-1]==L'\t'))
		*--e=0;
	if (!s[0])
		return TRUE;
	if (_wcsicmp(s, L"USB")==0 || _wcsicmp(s, L"MTP")==0 ||
		_wcsicmp(s, L"Android")==0 || _wcsicmp(s, L"Generic")==0 ||
		_wcsicmp(s, L"Unknown")==0 || _wcsicmp(s, L"Device")==0)
		return TRUE;
	if (_wcsnicmp(s, L"USB", 3)==0) {
		LPCWSTR p=s+3;
		while (*p==L' ' || *p==L'-' || *p==L'_' || *p==L'#')
			p++;
		if (!p[0])
			return TRUE;
		BOOL digits=TRUE;
		for (LPCWSTR q=p; *q; q++) {
			if (*q<L'0' || *q>L'9') {
				digits=FALSE;
				break;
			}
		}
		if (digits)
			return TRUE;
	}
	if (_wcsnicmp(s, L"Device", 6)==0) {
		LPCWSTR p=s+6;
		while (*p==L' ' || *p==L'-' || *p==L'_' || *p==L'#')
			p++;
		if (!p[0])
			return TRUE;
		BOOL digits=TRUE;
		for (LPCWSTR q=p; *q; q++) {
			if (*q<L'0' || *q>L'9') {
				digits=FALSE;
				break;
			}
		}
		if (digits)
			return TRUE;
	}
	static const WCHAR* gen[]={
		L"MTP Device", L"MTP USB", L"USB MTP",
		L"Portable Device", L"Portable Devices",
		L"Android Phone", L"Android Device", L"Android Composite",
		L"USB Composite", L"Composite Device",
		L"Unknown Device", L"Generic Device",
		L"ADB Interface", L"ADB Composite",
		L"MTP-устройств", L"MTP устройств", L"устройство MTP", L"устройств MTP",
		L"стандартное MTP", L"USB-устройств", L"USB устройств",
		L"портативное устройств"
	};
	for (int i=0;i<(int)(sizeof(gen)/sizeof(gen[0]));i++) {
		if (NameContainsI(s, gen[i]))
			return TRUE;
	}
	if (NameContainsI(s, L"MTP") &&
		(NameContainsI(s, L"USB") || NameContainsI(s, L"Device") ||
		 NameContainsI(s, L"устройств") || NameContainsI(s, L"стандарт")))
		return TRUE;
	if (NameContainsI(s, L"USB") && NameContainsI(s, L"устройств"))
		return TRUE;
	return FALSE;
}

static BOOL QueryWpdMgrString(IPortableDeviceManager* mgr, LPCWSTR pnpId, int kind, WCHAR* out, DWORD cch)
{
	if (out && cch)
		out[0]=0;
	if (!mgr || !pnpId || !out || cch<2)
		return FALSE;
	WCHAR tmp[256];
	tmp[0]=0;
	DWORD n=0;
	HRESULT hr=E_FAIL;
	if (kind==0)
		hr=mgr->GetDeviceFriendlyName((LPWSTR)pnpId, NULL, &n);
	else if (kind==1)
		hr=mgr->GetDeviceDescription((LPWSTR)pnpId, NULL, &n);
	else
		hr=mgr->GetDeviceManufacturer((LPWSTR)pnpId, NULL, &n);
	if (SUCCEEDED(hr) && n>1 && n<=256) {
		if (kind==0)
			hr=mgr->GetDeviceFriendlyName((LPWSTR)pnpId, tmp, &n);
		else if (kind==1)
			hr=mgr->GetDeviceDescription((LPWSTR)pnpId, tmp, &n);
		else
			hr=mgr->GetDeviceManufacturer((LPWSTR)pnpId, tmp, &n);
	} else {
		n=256;
		if (kind==0)
			hr=mgr->GetDeviceFriendlyName((LPWSTR)pnpId, tmp, &n);
		else if (kind==1)
			hr=mgr->GetDeviceDescription((LPWSTR)pnpId, tmp, &n);
		else
			hr=mgr->GetDeviceManufacturer((LPWSTR)pnpId, tmp, &n);
	}
	if (FAILED(hr) || tmp[0]==0)
		return FALSE;
	SanitizeFriendlyName(tmp);
	WCHAR* s=tmp;
	while (*s==L' ' || *s==L'\t')
		s++;
	WCHAR* e=s+wcslen(s);
	while (e>s && (e[-1]==L' ' || e[-1]==L'\t'))
		*--e=0;
	if (!s[0])
		return FALSE;
	wcslcpy(out, s, cch);
	return TRUE;
}

static void ComposeWpdDisplayName(WCHAR* out, DWORD cch, LPCWSTR model, LPCWSTR mfr, LPCWSTR friendly)
{
	if (!out || cch<2)
		return;
	out[0]=0;
	WCHAR pick[256]=L"";
	if (!IsGenericWpdName(model))
		wcslcpy(pick, model, 256);
	else if (!IsGenericWpdName(friendly))
		wcslcpy(pick, friendly, 256);
	if (pick[0]) {
		if (!IsGenericWpdName(mfr) && mfr && wcslen(mfr)>=2 && !NameContainsI(pick, mfr))
			swprintf_s(out, cch, L"%s %s", mfr, pick);
		else
			wcslcpy(out, pick, cch);
		return;
	}
	if (!IsGenericWpdName(mfr))
		wcslcpy(out, mfr, cch);
}

static BOOL CopyWpdPropString(IPortableDeviceValues* v, REFPROPERTYKEY key, WCHAR* dest, DWORD cch)
{
	if (!dest || !cch)
		return FALSE;
	dest[0]=0;
	if (!v)
		return FALSE;
	LPWSTR s=NULL;
	if (SUCCEEDED(v->GetStringValue(key, &s)) && s && s[0]) {
		wcslcpy(dest, s, cch);
		SanitizeFriendlyName(dest);
	}
	if (s)
		CoTaskMemFree(s);
	return dest[0]!=0;
}

// Same properties as Alt+Enter / quote info (WPD_DEVICE_MODEL), not PnP USB1.
static BOOL QueryOpenedDeviceIdentity(IPortableDevice* dev, WCHAR* model, DWORD mcch, WCHAR* mfr, DWORD frcch, WCHAR* friendly, DWORD fcch)
{
	if (model && mcch)
		model[0]=0;
	if (mfr && frcch)
		mfr[0]=0;
	if (friendly && fcch)
		friendly[0]=0;
	if (!dev)
		return FALSE;
	IPortableDeviceContent* content=NULL;
	if (FAILED(dev->Content(&content)) || !content)
		return FALSE;
	IPortableDeviceProperties* props=NULL;
	HRESULT hr=content->Properties(&props);
	content->Release();
	if (FAILED(hr) || !props)
		return FALSE;
	IPortableDeviceKeyCollection* keys=NULL;
	hr=CoCreateInstance(CLSID_PortableDeviceKeyCollection, NULL, CLSCTX_INPROC_SERVER,
		IID_IPortableDeviceKeyCollection, (void**)&keys);
	if (FAILED(hr) || !keys) {
		props->Release();
		return FALSE;
	}
	keys->Add(WPD_DEVICE_MODEL);
	keys->Add(WPD_DEVICE_MANUFACTURER);
	keys->Add(WPD_DEVICE_FRIENDLY_NAME);
	IPortableDeviceValues* v=NULL;
	hr=props->GetValues(WPD_DEVICE_OBJECT_ID, keys, &v);
	keys->Release();
	props->Release();
	if (FAILED(hr) || !v)
		return FALSE;
	if (model && mcch)
		CopyWpdPropString(v, WPD_DEVICE_MODEL, model, mcch);
	if (mfr && frcch)
		CopyWpdPropString(v, WPD_DEVICE_MANUFACTURER, mfr, frcch);
	if (friendly && fcch)
		CopyWpdPropString(v, WPD_DEVICE_FRIENDLY_NAME, friendly, fcch);
	v->Release();
	return (model && model[0]) || (mfr && mfr[0]) || (friendly && friendly[0]);
}

static void PickObjectDisplayName(IPortableDeviceValues* v, WCHAR* out, DWORD cch)
{
	if (!out || cch<2)
		return;
	out[0]=0;
	if (!v) {
		wcslcpy(out, L"_", cch);
		return;
	}
	WCHAR orig[MAX_PATH]=L"", name[MAX_PATH]=L"", stor[MAX_PATH]=L"";
	CopyWpdPropString(v, WPD_OBJECT_ORIGINAL_FILE_NAME, orig, MAX_PATH);
	CopyWpdPropString(v, WPD_OBJECT_NAME, name, MAX_PATH);
	CopyWpdPropString(v, WPD_STORAGE_DESCRIPTION, stor, MAX_PATH);
	GUID cat=GUID_NULL;
	v->GetGuidValue(WPD_FUNCTIONAL_OBJECT_CATEGORY, &cat);
	BOOL isStor=IsEqualGUID(cat, WPD_FUNCTIONAL_CATEGORY_STORAGE);
	LPCWSTR pick=NULL;
	if (stor[0] && (isStor || IsGenericWpdName(orig) || IsGenericWpdName(name) || !orig[0]))
		pick=stor;
	else if (orig[0] && !IsGenericWpdName(orig))
		pick=orig;
	else if (name[0] && !IsGenericWpdName(name))
		pick=name;
	else if (stor[0])
		pick=stor;
	else if (orig[0])
		pick=orig;
	else if (name[0])
		pick=name;
	if (pick && pick[0] && wcscmp(pick, L"\\")!=0)
		wcslcpy(out, pick, cch);
	else
		wcslcpy(out, L"_", cch);
}

static void PickWpdDisplayName(IPortableDeviceManager* mgr, LPCWSTR pnpId, DWORD index, WCHAR* out, DWORD cch)
{
	if (!out || cch<2)
		return;
	out[0]=0;
	WCHAR friendly[256]=L"", desc[256]=L"", mfr[256]=L"";
	QueryWpdMgrString(mgr, pnpId, 0, friendly, 256);
	QueryWpdMgrString(mgr, pnpId, 1, desc, 256);
	QueryWpdMgrString(mgr, pnpId, 2, mfr, 256);
	ComposeWpdDisplayName(out, cch, desc, mfr, friendly);
	if (out[0] && !IsGenericWpdName(out))
		return;
	if (pnpId && NameContainsI(pnpId, L"usb"))
		swprintf_s(out, cch, L"USB%d", index+1);
	else
		swprintf_s(out, cch, L"Device%d", index+1);
}

static void RefineNamesFromWpdModel(void)
{
	for (DWORD i=0;i<StoredNumIds;i++) {
		IPortableDevice* dev=StoredDevices[i];
		if (!dev) {
			if (FAILED(OpenWpdDevice(StoredPnpDeviceIDs[i], &dev)) || !dev)
				continue;
			StoredDevices[i]=dev;
		}
		WCHAR model[256]=L"", mfr[256]=L"", friendly[256]=L"", showname[256]=L"";
		if (!QueryOpenedDeviceIdentity(dev, model, 256, mfr, 256, friendly, 256))
			continue;
		ComposeWpdDisplayName(showname, 256, model, mfr, friendly);
		if (!showname[0] || IsGenericWpdName(showname))
			continue;
		SanitizeFriendlyName(showname);
		if (StoredPnPFriendlyNames[i] && wcscmp(StoredPnPFriendlyNames[i], showname)==0)
			continue;
		CoTaskMemFree(StoredPnPFriendlyNames[i]);
		StoredPnPFriendlyNames[i]=wstrnew(showname);
		if (!StoredPnPFriendlyNames[i]) {
			StoredPnPFriendlyNames[i]=(PWSTR)CoTaskMemRealloc(NULL,16*sizeof(WCHAR));
			if (StoredPnPFriendlyNames[i])
				swprintf_s(StoredPnPFriendlyNames[i],16,L"Device%d",i+1);
		}
	}
	for (DWORD i=0;i<StoredNumIds;i++)
		MakeFriendlyNameUnique(i);
}

BOOL LoadAllDevices()
{
	RegisterForEventNotifications();
	StoredNumIds=0;
	HRESULT hr = pDevMgr->GetDevices(NULL,&StoredNumIds);
	if (FAILED(hr)) {
		LogWpdError(L"GetDevices", hr);
		return false;
	}
	if (StoredNumIds) {
		StoredPnpDeviceIDs = (PWSTR*)malloc(StoredNumIds*sizeof(LPWSTR));
		StoredPnPFriendlyNames = (PWSTR*)malloc(StoredNumIds*sizeof(LPWSTR));
		int sz=StoredNumIds*sizeof(IPortableDevice*);
		StoredDevices = (IPortableDevice**)malloc(sz);
		StoredEventCookies = (LPWSTR*)malloc(StoredNumIds*sizeof(LPWSTR));
		if (!StoredPnpDeviceIDs || !StoredPnPFriendlyNames || !StoredDevices || !StoredEventCookies) {
			FreeDeviceList();
			return false;
		}
		memset(StoredPnpDeviceIDs,0,StoredNumIds*sizeof(LPWSTR));
		memset(StoredPnPFriendlyNames,0,StoredNumIds*sizeof(LPWSTR));
		memset(StoredDevices,0,sz);
		memset(StoredEventCookies,0,StoredNumIds*sizeof(LPWSTR));
		hr = pDevMgr->GetDevices(StoredPnpDeviceIDs, &StoredNumIds);
		if (FAILED(hr)) {
			LogWpdError(L"GetDevices(list)", hr);
			FreeDeviceList();
			return false;
		}
		LogProc(PluginNumber,MSGTYPE_CONNECT,"CONNECT \\");
		connected=true;

		for (DWORD i=0;i<StoredNumIds;i++) {
			WCHAR showname[256];
			PickWpdDisplayName(pDevMgr, StoredPnpDeviceIDs[i], i, showname, 256);
			SanitizeFriendlyName(showname);
			StoredPnPFriendlyNames[i]=wstrnew(showname);
			if (!StoredPnPFriendlyNames[i]) {
				StoredPnPFriendlyNames[i]=(PWSTR)CoTaskMemRealloc(NULL,16*sizeof(WCHAR));
				if (StoredPnPFriendlyNames[i])
					swprintf_s(StoredPnPFriendlyNames[i],16,L"Device%d",i+1);
			}
			MakeFriendlyNameUnique(i);
		}
		DWORD keep=0;
		for (DWORD i=0;i<StoredNumIds;i++) {
			if (ShouldHideWpdDevice(pDevMgr, StoredPnpDeviceIDs[i], StoredPnPFriendlyNames[i])) {
				CoTaskMemFree(StoredPnpDeviceIDs[i]);
				CoTaskMemFree(StoredPnPFriendlyNames[i]);
				StoredPnpDeviceIDs[i]=NULL;
				StoredPnPFriendlyNames[i]=NULL;
				continue;
			}
			if (keep!=i) {
				StoredPnpDeviceIDs[keep]=StoredPnpDeviceIDs[i];
				StoredPnPFriendlyNames[keep]=StoredPnPFriendlyNames[i];
				StoredDevices[keep]=StoredDevices[i];
				StoredEventCookies[keep]=StoredEventCookies[i];
				StoredPnpDeviceIDs[i]=NULL;
				StoredPnPFriendlyNames[i]=NULL;
				StoredDevices[i]=NULL;
				StoredEventCookies[i]=NULL;
			}
			keep++;
		}
		StoredNumIds=keep;
		RefineNamesFromWpdModel();
	}
	return true;
}

/*
void logValue(PWSTR name,PWSTR text,HRESULT hr,PROPVARIANT* pr) {
	WCHAR buf[256];
	__try {
		if (!SUCCEEDED(hr)) {
			wsprintfW(buf,L"%s %s: Error=%d",name,text,hr);
		} else if (pr->vt==VT_ERROR) {
			wsprintfW(buf,L"%s %s: vt=VT_ERROR",name,text);
		} else if (pr->vt==VT_DATE) {
			SYSTEMTIME systime;
			hr=VariantTimeToSystemTime(pr->date,&systime);
			if (SUCCEEDED(hr))
				wsprintfW(buf,L"%s %s: VT_DATE=%d-%02d-%02d",name,text,systime.wYear,systime.wMonth,systime.wDay);
			else
				wsprintfW(buf,L"%s %s: VT_DATE invalid time: %d",name,text,hr);
		} else if (pr->vt==VT_FILETIME) {
			SYSTEMTIME systime;
			if (FileTimeToSystemTime(&pr->filetime,&systime))
				wsprintfW(buf,L"%s %s: VT_FILETIME=%d-%02d-%02d",name,text,systime.wYear,systime.wMonth,systime.wDay);
			else
				wsprintfW(buf,L"%s %s: VT_FILETIME invalid value",name,text);
		} else {
			wsprintfW(buf,L"%s %s: vt=%d, hr=%d",name,text,pr->vt,hr);
		}
		LogProcW(PluginNumber,MSGTYPE_DETAILS,buf);
	} __except (true) {
		LogProcW(PluginNumber,MSGTYPE_DETAILS,L"Exception in logValue");
	}
}

WCHAR lastbuf[2048]={0};
*/

#define COUNTOF(x) sizeof(x)/sizeof(x[0])

/*
DEFINE_GUID( WPD_PROPERTIES_MTP_VENDOR_EXTENDED_OBJECT_PROPS , 0x4d545058, 0x4fce, 0x4578, 0x95, 0xc8, 0x86, 0x98, 0xa9, 0xbc, 0xf, 0x49 );

WCHAR retbuf[128];

WCHAR* getNameFromPropertyKey(PROPERTYKEY* propkey,BOOL* unknownField) {
	WCHAR* p=NULL;
	__try {
		*unknownField=false;
		if (IsEqualGUID(propkey->fmtid,WPD_OBJECT_PROPERTIES_V1)) {
			switch(propkey->pid) {
				case 2:p=L"ID ";break;
				case 3:p=L"PARENT_ID ";break;
				case 4:p=L"NAME ";break;
				case 5:p=L"PERSISTENT_UNIQUE_ID ";break;
				case 6:p=L"FORMAT ";break;
				case 7:p=L"CONTENT_TYPE ";break;
				case 9:p=L"ISHIDDEN ";break;
				case 10:p=L"ISSYSTEM ";break;
				case 11:p=L"SIZE ";break;
				case 12:p=L"ORIGINAL_FILE_NAME ";break;
				case 13:p=L"NON_CONSUMABLE ";break;
				case 14:p=L"REFERENCES ";break;
				case 15:p=L"KEYWORDS ";break;
				case 16:p=L"SYNC_ID ";break;
				case 17:p=L"IS_DRM_PROTECTED ";break;
				case 18:p=L"DATE_CREATED ";*unknownField=true;break;
				case 19:p=L"DATE_MODIFIED ";*unknownField=true;break;
				case 20:p=L"DATE_AUTHORED ";*unknownField=true;break;
				case 21:p=L"BACK_REFERENCES ";break;
				case 23:p=L"CONTAINER_FUNCTIONAL_OBJECT_ID ";break;
				case 24:p=L"GENERATE_THUMBNAIL_FROM_RESOURCE ";break;
				case 25:p=L"HINT_LOCATION_DISPLAY_NAME ";break;
				case 26:p=L"CAN_DELETE ";break;
			}
		} else if (IsEqualGUID(propkey->fmtid,WPD_CONTACT_OBJECT_PROPERTIES_V1)) {
			switch(propkey->pid) {
				case 2:p=L"DISPLAY_NAME ";break;
			}
		} else if (IsEqualGUID(propkey->fmtid,WPD_FUNCTIONAL_OBJECT_PROPERTIES_V1)) {
			switch(propkey->pid) {
				case 2:p=L"FUNCTIONAL_OBJECT_CATEGORY ";break;
			}
		} else if (IsEqualGUID(propkey->fmtid,WPD_STORAGE_OBJECT_PROPERTIES_V1)) {
			switch(propkey->pid) {
				case 2:p=L"STORAGE_TYPE ";break;
				case 3:p=L"STORAGE_FILE_SYSTEM_TYPE ";break;
				case 4:p=L"STORAGE_CAPACITY ";break;
				case 5:p=L"STORAGE_FREE_SPACE_IN_BYTES ";break;
				case 6:p=L"STORAGE_FREE_SPACE_IN_OBJECTS ";break;
				case 7:p=L"STORAGE_DESCRIPTION ";break;
				case 8:p=L"STORAGE_SERIAL_NUMBER ";break;
				case 9:p=L"STORAGE_MAX_OBJECT_SIZE ";break;
				case 10:p=L"STORAGE_CAPACITY_IN_OBJECTS ";break;
				case 11:p=L"STORAGE_ACCESS_CAPABILITY ";break;
			}
		} else if (IsEqualGUID(propkey->fmtid,WPD_PROPERTIES_MTP_VENDOR_EXTENDED_OBJECT_PROPS)) {
			*unknownField=true;
			wsprintfW(retbuf,L"Vendor key %d ",propkey->pid);
			p=retbuf;
		}
		if (p==NULL) {
			*unknownField=true;
			retbuf[0]=0;
			StringFromGUID2(propkey->fmtid,retbuf,COUNTOF(retbuf)-1);
			if (retbuf[0]) {
				size_t L=wcslen(retbuf);
				_itow_s(propkey->pid,retbuf+L+1,COUNTOF(retbuf)-L-1,10);
				retbuf[L]=':';
				wcslcat(retbuf,L" ",COUNTOF(retbuf)-1);
			}
		}
		return p;
	} __except (true) {
		return L"Exception in getNameFromPropertyKey!";
	}
}
*/

static void ReleaseBatchValues(pLastFindStuct lf)
{
	if (!lf)
		return;
	for (DWORD i=0;i<NUM_OBJECTS_TO_REQUEST;i++) {
		if (lf->szObjectValues[i]) {
			lf->szObjectValues[i]->Release();
			lf->szObjectValues[i]=NULL;
		}
	}
}

class CBulkCb : public IPortableDevicePropertiesBulkCallback
{
	LONG m_ref;
	HANDLE m_done;
	pLastFindStuct m_lf;
	HRESULT m_hr;
public:
	CBulkCb(pLastFindStuct lf) : m_ref(1), m_lf(lf), m_hr(S_OK)
	{
		m_done=CreateEvent(NULL,TRUE,FALSE,NULL);
	}
	~CBulkCb()
	{
		if (m_done)
			CloseHandle(m_done);
	}
	HRESULT WaitDone(DWORD ms)
	{
		if (!m_done)
			return E_FAIL;
		if (WaitForSingleObject(m_done,ms)!=WAIT_OBJECT_0)
			return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
		return m_hr;
	}
	STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
	{
		if (!ppv) return E_POINTER;
		if (IsEqualIID(riid,IID_IUnknown) || IsEqualIID(riid,IID_IPortableDevicePropertiesBulkCallback)) {
			*ppv=static_cast<IPortableDevicePropertiesBulkCallback*>(this);
			AddRef();
			return S_OK;
		}
		*ppv=NULL;
		return E_NOINTERFACE;
	}
	STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_ref); }
	STDMETHODIMP_(ULONG) Release()
	{
		LONG r=InterlockedDecrement(&m_ref);
		if (r==0)
			delete this;
		return r;
	}
	STDMETHODIMP OnStart(REFGUID) { return S_OK; }
	STDMETHODIMP OnProgress(REFGUID, IPortableDeviceValuesCollection* col)
	{
		if (!col || !m_lf)
			return S_OK;
		DWORD n=0;
		col->GetCount(&n);
		for (DWORD i=0;i<n;i++) {
			IPortableDeviceValues* v=NULL;
			if (FAILED(col->GetAt(i,&v)) || !v)
				continue;
			LPWSTR id=NULL;
			v->GetStringValue(WPD_OBJECT_ID,&id);
			BOOL kept=FALSE;
			if (id) {
				for (DWORD k=0;k<m_lf->szObjectIDsFetched;k++) {
					if (m_lf->szObjectIDArray[k] && wcscmp(m_lf->szObjectIDArray[k],id)==0) {
						if (m_lf->szObjectValues[k])
							m_lf->szObjectValues[k]->Release();
						m_lf->szObjectValues[k]=v;
						kept=TRUE;
						break;
					}
				}
				CoTaskMemFree(id);
			}
			if (!kept)
				v->Release();
		}
		return S_OK;
	}
	STDMETHODIMP OnEnd(REFGUID, HRESULT hrStatus)
	{
		m_hr=hrStatus;
		if (m_done)
			SetEvent(m_done);
		return S_OK;
	}
};

static BOOL g_bulkDisabled=FALSE;

static void PrefetchBatchValues(pLastFindStuct lf)
{
	ReleaseBatchValues(lf);
	// Small folders (device root, a few storages): lazy GetValues in PopulateFindDataW.
	// Bulk on many Android MTP stacks never calls OnEnd → used to block 20s per directory.
	if (g_bulkDisabled || !lf || !lf->pProperties || !lf->pPropertiesToRead || lf->szObjectIDsFetched<24)
		return;
	IPortableDevicePropertiesBulk* bulk=NULL;
	if (FAILED(lf->pProperties->QueryInterface(IID_IPortableDevicePropertiesBulk,(void**)&bulk)) || !bulk)
		return;
	IPortableDevicePropVariantCollection* ids=NULL;
	if (FAILED(CoCreateInstance(CLSID_PortableDevicePropVariantCollection,NULL,CLSCTX_INPROC_SERVER,
		IID_IPortableDevicePropVariantCollection,(void**)&ids))) {
		bulk->Release();
		return;
	}
	for (DWORD i=0;i<lf->szObjectIDsFetched;i++) {
		PROPVARIANT pv;
		PropVariantInit(&pv);
		pv.vt=VT_LPWSTR;
		pv.pwszVal=lf->szObjectIDArray[i];
		ids->Add(&pv);
		pv.vt=VT_EMPTY;
		pv.pwszVal=NULL;
	}
	CBulkCb* cb=new CBulkCb(lf);
	GUID ctx=GUID_NULL;
	HRESULT hr=bulk->QueueGetValuesByObjectList(ids,lf->pPropertiesToRead,cb,&ctx);
	if (SUCCEEDED(hr))
		hr=bulk->Start(ctx);
	if (SUCCEEDED(hr)) {
		HRESULT waitHr=cb->WaitDone(800);
		if (FAILED(waitHr)) {
			g_bulkDisabled=TRUE;
			bulk->Cancel(ctx);
			ReleaseBatchValues(lf);
		}
	} else
		g_bulkDisabled=TRUE;
	cb->Release();
	ids->Release();
	bulk->Release();
}

void EnsureWpdEventsAdvised(void)
{
	for (DWORD i=0;i<StoredNumIds;i++) {
		if (StoredDevices[i] && StoredEventCookies && StoredEventCookies[i]==NULL)
			AdviseWpdDevice(StoredDevices[i], &StoredEventCookies[i]);
	}
}

void PopulateFindDataW(PWSTR szObject,pLastFindStuct lf,WIN32_FIND_DATAW *FindData,int LocalTime)
{
	__try {
		memset(FindData,0,sizeof(WIN32_FIND_DATAW));
		wcslcpy(FindData->cFileName,szObject,MAX_PATH-2);
		FindData->dwFileAttributes=FILE_ATTRIBUTE_DIRECTORY;
		FindData->ftLastWriteTime.dwHighDateTime=0xFFFFFFFF;
		FindData->ftLastWriteTime.dwLowDateTime=0xFFFFFFFE;
		FindData->nFileSizeHigh=0;
		FindData->nFileSizeLow=0;

/*		IPortableDeviceKeyCollection *pkeys=NULL;
		__int64 getvalueflag=0;
		DWORD count=0;
		BOOL fieldTypesChanged=FALSE;

		if (SUCCEEDED(lf->pProperties->GetSupportedProperties(szObject,&pkeys))) {
			if (pkeys!=NULL && SUCCEEDED(pkeys->GetCount(&count))) {
				WCHAR buf[2048];
				wcslcpy(buf,L"Supported:",COUNTOF(buf)-1);
				for (DWORD i=0;i<count;i++) {
					PROPERTYKEY propkey;
					if (SUCCEEDED(pkeys->GetAt(i,&propkey))) {
						BOOL unknownField=FALSE;
						WCHAR* p=getNameFromPropertyKey(&propkey,&unknownField);
						if (p!=NULL) {
							if (unknownField)
								getvalueflag|=(1LL<<i);
							wcslcat(buf,p,COUNTOF(buf)-1);
						}
					}
				}
				if (wcscmp(buf,lastbuf)) {
					fieldTypesChanged=true;
					LogProcW(PluginNumber,MSGTYPE_DETAILS,buf);
					wcslcpy(lastbuf,buf,COUNTOF(lastbuf)-1);
				}
			}
		} else
			LogProcW(PluginNumber,MSGTYPE_DETAILS,L"GetSupportedProperties failed.");
*/
		if (lf->pPropertiesToRead && lf->pProperties) {
			IPortableDeviceValues* pObjectProperties=NULL;
			BOOL ownProps=TRUE;
			HRESULT hr2=S_OK;
			DWORD idx=lf->szObjectIDLastRead;
			if (idx<lf->szObjectIDsFetched && lf->szObjectValues[idx] &&
				lf->szObjectIDArray[idx] && szObject && wcscmp(lf->szObjectIDArray[idx],szObject)==0) {
				pObjectProperties=lf->szObjectValues[idx];
				ownProps=FALSE;
				hr2=S_OK;
			} else {
				hr2=lf->pProperties->GetValues(szObject,
							lf->pPropertiesToRead,
							&pObjectProperties);
			}
			if (SUCCEEDED(hr2) && pObjectProperties) {
				PickObjectDisplayName(pObjectProperties, FindData->cFileName, MAX_PATH-2);

				if (!IsWpdFolderObject(pObjectProperties)) {
					FindData->dwFileAttributes=0;
					ULONGLONG sz=0;
					hr2=pObjectProperties->GetUnsignedLargeIntegerValue(WPD_OBJECT_SIZE,&sz);
					if (SUCCEEDED(hr2)) {
						FindData->nFileSizeHigh=(DWORD)(sz>>32);
						FindData->nFileSizeLow=(DWORD)(sz);
					}
				}
				BOOL hidden=FALSE;
				if (SUCCEEDED(pObjectProperties->GetBoolValue(WPD_OBJECT_ISHIDDEN,&hidden)) && hidden)
					FindData->dwFileAttributes|=FILE_ATTRIBUTE_HIDDEN;
				BOOL systemFile=FALSE;
				if (SUCCEEDED(pObjectProperties->GetBoolValue(WPD_OBJECT_ISSYSTEM,&systemFile)) && systemFile)
					FindData->dwFileAttributes|=FILE_ATTRIBUTE_SYSTEM;

				PROPVARIANT pr;
				/*if (fieldTypesChanged) {
					WCHAR buf[2048];
					buf[0]=0;
					wcslcat(buf,L"Field types:",COUNTOF(buf)-10);
					for (DWORD i=0;i<count;i++) {
						if (getvalueflag & (1LL<<i)) {
							PROPERTYKEY propkey;
							if (SUCCEEDED(pkeys->GetAt(i,&propkey))) {
								BOOL unknownField=FALSE;
								WCHAR* p=getNameFromPropertyKey(&propkey,&unknownField);
								pr.vt=VT_ERROR;
								hr2=pObjectProperties->GetValue(propkey,&pr);
								wcslcat(buf,p,COUNTOF(buf)-10);
								if (SUCCEEDED(hr2)) {
									wcslcat(buf,L"=",COUNTOF(buf)-10);
									_itow_s(pr.vt,buf+wcslen(buf),COUNTOF(buf)-wcslen(buf)-1,10);
									wcslcat(buf,L" ",COUNTOF(buf)-10);
								}
							}
						}
					}
					LogProcW(PluginNumber,MSGTYPE_DETAILS,buf);
				}
				hr2=pObjectProperties->GetValue(WPD_OBJECT_DATE_MODIFIED,&pr);
				logValue(FindData->cFileName,L"MODIFIED",hr2,&pr);
				hr2=pObjectProperties->GetValue(WPD_OBJECT_DATE_CREATED,&pr);
				logValue(FindData->cFileName,L"CREATED",hr2,&pr);
				*/
				hr2=pObjectProperties->GetValue(WPD_OBJECT_DATE_MODIFIED,&pr);
				if (!SUCCEEDED(hr2) || pr.vt==VT_ERROR || pr.date==29221)
					hr2=pObjectProperties->GetValue(WPD_OBJECT_DATE_CREATED,&pr);
				if (SUCCEEDED(hr2)) {
					SYSTEMTIME systime,systime2;
					if (pr.vt==VT_DATE) {
						VariantTimeToSystemTime(pr.date,&systime);
						if (!(systime.wYear==1980 && systime.wDay==1 && systime.wMonth==1 && systime.wMinute==0 && systime.wSecond==0) &&
							!(systime.wYear==1979 && systime.wDay==31 && systime.wMonth==12 && systime.wMinute==0 && systime.wSecond==0)) {
							if (LocalTime==2) {
								TzSpecificLocalTimeToSystemTime(NULL,&systime,&systime2);
								SystemTimeToFileTime(&systime2,&FindData->ftLastWriteTime);
							} else if (LocalTime==1) {
								FILETIME ftime;
								if (!SystemTimeToFileTime(&systime,&ftime)) {
									ftime.dwHighDateTime=0;
									ftime.dwLowDateTime=0;
								}
								LocalFileTimeToFileTime(&ftime,&FindData->ftLastWriteTime);
							} else {
								SystemTimeToFileTime(&systime,&FindData->ftLastWriteTime);
							}
						}
					} else if (pr.vt==VT_FILETIME) {
						FindData->ftLastWriteTime=pr.filetime;
					}
				}
				if (ownProps)
					pObjectProperties->Release();
			}
		}
	} __except (true) {
		LogProcW(PluginNumber,MSGTYPE_DETAILS,L"Exception in PopulateFindDataW");
	}
}

HANDLE __stdcall FsFindFirst(char* Path,WIN32_FIND_DATA *FindData)
{
	WCHAR PathW[wdirtypemax];
	WIN32_FIND_DATAW FindDataW;
	memset(&FindDataW,0,sizeof(FindDataW));
	HANDLE h=FsFindFirstW(awfilenamecopy(PathW,Path),&FindDataW);
	if (h!=INVALID_HANDLE_VALUE)
		copyfinddatawa(FindData,&FindDataW);
	return h;
}

HANDLE __stdcall FsFindFirstW(WCHAR* Path,WIN32_FIND_DATAW *FindData)
{
	pLastFindStuct lf;
	WCHAR wcSearch[wdirtypemax];

	EnsureComApartment();
	memset(FindData,0,sizeof(WIN32_FIND_DATAW));
	wcslcpy(wcSearch,Path,wdirtypemax-1);           // incl. Backslash!
	wcslcatbackslash(wcSearch,wdirtypemax-1);
	
	if (Path[0]=='\\') {
		LockPlugin();
		if (DeviceEventReceived) {
			DeviceEventReceived=false;
			InterlockedExchange(&g_cacheDirty,0);
			FreeAllDevices();
			ClearCache();
		} else if (InterlockedCompareExchange(&g_cacheDirty,0,1)) {
			ClearCache();
		}

		if (!InitFunctionsIfNeeded(TRUE)) {
			UnlockPlugin();
			SetLastError(ERROR_FILE_NOT_FOUND);
			return INVALID_HANDLE_VALUE;
		}

		if (Path[1]==0) {  // Enum just the devices
			AppleMdResetSessions();
			DWORD total=StoredNumIds+(DWORD)AppleMdCount();
			if (total==0) {
				UnlockPlugin();
				SetLastError(ERROR_NO_MORE_FILES);
				return INVALID_HANDLE_VALUE;
			}
			memset(FindData,0,sizeof(*FindData));
			FindData->dwFileAttributes=FILE_ATTRIBUTE_DIRECTORY;
			FindData->ftLastWriteTime.dwHighDateTime=0xFFFFFFFF;
			FindData->ftLastWriteTime.dwLowDateTime=0xFFFFFFFE;
			if (StoredNumIds>0)
				wcslcpy(FindData->cFileName,StoredPnPFriendlyNames[0],MAX_PATH-2);
			else
				AppleMdGetName(0, FindData->cFileName, MAX_PATH-2);
			lf=(pLastFindStuct)malloc(sizeof(tLastFindStuct));
			memset(lf,0,sizeof(tLastFindStuct));
			wcslcpy(lf->Path,wcSearch,wdirtypemax-1);
			lf->pPnpDeviceLastRead=0;
			lf->listKind=0;
			UnlockPlugin();
			return (HANDLE)lf;
		} else {
			WCHAR devName[MAX_PATH];
			const WCHAR* pth=Path[0]=='\\' ? Path+1 : Path;
			wcslcpy(devName, pth, MAX_PATH-1);
			WCHAR* sl=wcschr(devName, '\\');
			WCHAR rel[wdirtypemax]=L"";
			if (sl) {
				sl[0]=0;
				wcslcpy(rel, sl+1, wdirtypemax-1);
				wcutlastbackslash(rel);
			}
			if (AppleMdIsDeviceName(devName)) {
				HANDLE ah=INVALID_HANDLE_VALUE;
				UnlockPlugin();
				if (!AppleMdFindFirst(devName, rel, FindData, &ah)) {
					SetLastError(ERROR_NO_MORE_FILES);
					return INVALID_HANDLE_VALUE;
				}
				lf=(pLastFindStuct)malloc(sizeof(tLastFindStuct));
				memset(lf,0,sizeof(tLastFindStuct));
				wcslcpy(lf->Path, wcSearch, wdirtypemax-1);
				lf->listKind=2;
				lf->appleFind=ah;
				return (HANDLE)lf;
			}
			IEnumPortableDeviceObjectIDs* pEnumObjectIDs;
			IPortableDeviceProperties* pProperties;
			HRESULT hr = GetFolderIDFromPathName(wcSearch,&pEnumObjectIDs,&pProperties,NULL,NULL);
			if (!SUCCEEDED(hr)) {
				UnlockPlugin();
				SetLastError(ERROR_FILE_NOT_FOUND);
				return INVALID_HANDLE_VALUE;
			}
			lf=(pLastFindStuct)malloc(sizeof(tLastFindStuct));
			memset(lf,0,sizeof(tLastFindStuct));
			wcslcpy(lf->Path,wcSearch,wdirtypemax-1);
			lf->listKind=1;
			lf->szObjectIDsFetched=0;
			lf->szObjectIDLastRead=0;
			hr = pEnumObjectIDs->Next(NUM_OBJECTS_TO_REQUEST,lf->szObjectIDArray,&lf->szObjectIDsFetched);
			if (!SUCCEEDED(hr) || lf->szObjectIDsFetched==0) {
				free(lf);
				pEnumObjectIDs->Release();
				pProperties->Release();
				UnlockPlugin();
				SetLastError(ERROR_NO_MORE_FILES);
				LogProc(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,"Directory is empty!");
				return INVALID_HANDLE_VALUE;
			}

			WCHAR buf1[wdirtypemax];
			wcslcpy(buf1,L"GET DIR ",wdirtypemax-1);
			wcslcat(buf1,Path,wdirtypemax-1);
			LogProcT(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,buf1);

			lf->pEnumObjectIDs=pEnumObjectIDs;
			lf->pProperties=pProperties;

			hr = CoCreateInstance(CLSID_PortableDeviceKeyCollection,NULL,
                      CLSCTX_INPROC_SERVER,
					  IID_IPortableDeviceKeyCollection,
                      (VOID**) &lf->pPropertiesToRead);
			if (SUCCEEDED(hr)) {
				IPortableDeviceValues* pObjectProperties=NULL;
				lf->pPropertiesToRead->Add(WPD_OBJECT_NAME);
				lf->pPropertiesToRead->Add(WPD_OBJECT_ORIGINAL_FILE_NAME);
				lf->pPropertiesToRead->Add(WPD_STORAGE_DESCRIPTION);
				lf->pPropertiesToRead->Add(WPD_FUNCTIONAL_OBJECT_CATEGORY);
				lf->pPropertiesToRead->Add(WPD_OBJECT_SIZE);
				lf->pPropertiesToRead->Add(WPD_OBJECT_DATE_MODIFIED);
				lf->pPropertiesToRead->Add(WPD_OBJECT_DATE_CREATED);  // for devices like some cameras which don't have the modified date
				lf->pPropertiesToRead->Add(WPD_OBJECT_CONTENT_TYPE);
				lf->pPropertiesToRead->Add(WPD_OBJECT_ISHIDDEN);
				lf->pPropertiesToRead->Add(WPD_OBJECT_ISSYSTEM);
			} else
				lf->pPropertiesToRead=NULL;

			lf->LocalTime=UseLocalTime(Path);
			PrefetchBatchValues(lf);
			PopulateFindDataW(lf->szObjectIDArray[0],lf,FindData,lf->LocalTime);
			lf->listedCount=1;
			UnlockPlugin();
			ProgressProcT(PluginNumber,lf->Path,FindData->cFileName,0);
			return (HANDLE)lf;
		}
	}
	SetLastError(ERROR_PATH_NOT_FOUND);
	return INVALID_HANDLE_VALUE;
}

BOOL __stdcall FsFindNext(HANDLE Hdl,WIN32_FIND_DATA *FindData)
{
	WIN32_FIND_DATAW FindDataW;
	memset(&FindDataW,0,sizeof(FindDataW));
	if (!FsFindNextW(Hdl,&FindDataW))
		return false;
	copyfinddatawa(FindData,&FindDataW);
	return true;
}

BOOL __stdcall FsFindNextW(HANDLE Hdl,WIN32_FIND_DATAW *FindData)
{
	HRESULT hr;
	pLastFindStuct lf;

	if (Hdl==(HANDLE)1)
		return false;
	if (IsAbortRequested())
		return false;

	lf=(pLastFindStuct)Hdl;
	if (lf->listKind==2) {
		return AppleMdFindNext(lf->appleFind, FindData);
	}
	LockPlugin();
	if (lf->listKind==0) {
		DWORD total=StoredNumIds+(DWORD)AppleMdCount();
		if (lf->pPnpDeviceLastRead+1<total) {
			lf->pPnpDeviceLastRead++;
			DWORD idx=lf->pPnpDeviceLastRead;
			memset(FindData,0,sizeof(*FindData));
			FindData->dwFileAttributes=FILE_ATTRIBUTE_DIRECTORY;
			FindData->ftLastWriteTime.dwHighDateTime=0xFFFFFFFF;
			FindData->ftLastWriteTime.dwLowDateTime=0xFFFFFFFE;
			if (idx<StoredNumIds)
				wcslcpy(FindData->cFileName,StoredPnPFriendlyNames[idx],MAX_PATH-2);
			else
				AppleMdGetName((int)(idx-StoredNumIds), FindData->cFileName, MAX_PATH-2);
			UnlockPlugin();
			return true;
		}
	} else if (lf->pEnumObjectIDs) {
		if (lf->szObjectIDLastRead+1<lf->szObjectIDsFetched) {
			lf->szObjectIDLastRead++;
			hr=S_OK;
		} else {
			for (DWORD i=0;i<lf->szObjectIDsFetched;i++) {
				CoTaskMemFree(lf->szObjectIDArray[i]);
				lf->szObjectIDArray[i]=NULL;
			}
			lf->szObjectIDsFetched=0;
			lf->szObjectIDLastRead=0;
			ReleaseBatchValues(lf);
			hr = lf->pEnumObjectIDs->Next(NUM_OBJECTS_TO_REQUEST,lf->szObjectIDArray,&lf->szObjectIDsFetched);
			if (!SUCCEEDED(hr) || lf->szObjectIDsFetched==0) {
				SetLastError(ERROR_NO_MORE_FILES);
				for (DWORD i=0;i<lf->szObjectIDsFetched;i++) {
					CoTaskMemFree(lf->szObjectIDArray[i]);
					lf->szObjectIDArray[i]=NULL;
				}
				lf->szObjectIDsFetched=0;
				lf->pEnumObjectIDs->Release();
				lf->pEnumObjectIDs=NULL;
				lf->pProperties->Release();
				lf->pProperties=NULL;
				UnlockPlugin();
				return false;
			}
			PrefetchBatchValues(lf);
		}
		PopulateFindDataW(lf->szObjectIDArray[lf->szObjectIDLastRead],lf,FindData,lf->LocalTime);
		lf->listedCount++;
		DWORD n=lf->listedCount;
		UnlockPlugin();
		if ((n%16)==0) {
			if (ProgressProcT(PluginNumber,lf->Path,FindData->cFileName,(int)(n%99)))
				return false;
		}
		return true;
	}
	UnlockPlugin();
	return false;
}

int __stdcall FsFindClose(HANDLE Hdl)
{
	if (Hdl==(HANDLE)1)
		return 0;
	pLastFindStuct lf;
	lf=(pLastFindStuct)Hdl;
	if (lf->listKind==2 && lf->appleFind) {
		AppleMdFindClose(lf->appleFind);
		lf->appleFind=NULL;
	}
	if (lf->pEnumObjectIDs) {
		lf->pEnumObjectIDs->Release();
		lf->pEnumObjectIDs=NULL;
	}
	if (lf->pProperties) {
		lf->pProperties->Release();
		lf->pProperties=NULL;
	}
	if (lf->pPropertiesToRead) {
		lf->pPropertiesToRead->Release();
		lf->pPropertiesToRead=NULL;
	}
	for (DWORD i=0;i<lf->szObjectIDsFetched;i++) {
		if (lf->szObjectIDArray[i])
			CoTaskMemFree(lf->szObjectIDArray[i]);
		lf->szObjectIDArray[i]=NULL;
	}
	ReleaseBatchValues(lf);
	lf->szObjectIDsFetched=0;
	free(lf);
	return 0;
}

int __stdcall FsInit(int PluginNr,tProgressProc pProgressProc,tLogProc pLogProc,tRequestProc pRequestProc)
{
	PluginNumber=PluginNr;
	ProgressProc=pProgressProc;
    LogProc=pLogProc;
    RequestProc=pRequestProc;
	return 0;
}

int __stdcall FsInitW(int PluginNr,tProgressProcW pProgressProcW,tLogProcW pLogProcW,tRequestProcW pRequestProcW)
{
	PluginNumber=PluginNr;
	ProgressProcW=pProgressProcW;
    LogProcW=pLogProcW;
    RequestProcW=pRequestProcW;
	return 0;
}

void FreeDeviceList()
{
	for (DWORD i=0;i<StoredNumIds;i++) {
		if (StoredDevices && StoredDevices[i] && StoredEventCookies)
			UnadviseWpdDevice(StoredDevices[i], StoredEventCookies[i]);
		if (StoredPnpDeviceIDs)
			CoTaskMemFree(StoredPnpDeviceIDs[i]);
		if (StoredPnPFriendlyNames)
			CoTaskMemFree(StoredPnPFriendlyNames[i]);
		if (StoredDevices && StoredDevices[i])
			StoredDevices[i]->Release();
	}
	if (StoredPnpDeviceIDs)
		free(StoredPnpDeviceIDs);
	if (StoredPnPFriendlyNames)
		free(StoredPnPFriendlyNames);
	if (StoredDevices)
		free(StoredDevices);
	if (StoredEventCookies)
		free(StoredEventCookies);
	StoredPnpDeviceIDs=NULL;
	StoredPnPFriendlyNames=NULL;
	StoredDevices=NULL;
	StoredEventCookies=NULL;
	StoredNumIds=0;
	connected=false;
}

void FreeAllDevices()
{
	FreeDeviceList();
	if (pDevMgr)
		pDevMgr->Release();    // we must release this too, otherwise new devices are not seen!
	pDevMgr=NULL;
	initialized=false;
}

HRESULT DisConnectIfNeeded()
{
	if (initialized || pDevMgr || StoredNumIds) {
		FreeAllDevices();
		UnregisterNotification();
		return S_OK;
	} else
		return E_FAIL;
}

BOOL __stdcall FsDisconnect(char* DisconnectRoot)
{
	AppleMdResetSessions();
	DisConnectIfNeeded();
	char buf1[MAX_PATH];
	strlcpy(buf1,"DISCONNECT ",MAX_PATH);
	strlcat(buf1,DisconnectRoot,MAX_PATH);
	LogProc(PluginNumber,MSGTYPE_DISCONNECT,buf1);
	return TRUE;
}

BOOL __stdcall FsMkDir(char* Path)
{
	WCHAR PathW[wdirtypemax];
	return FsMkDirW(awfilenamecopy(PathW,Path));
}

#define ArraySize 128

// Returns 0 if not exists, 1 for files, 2 for folders
int NameExistsInEnum(IEnumPortableDeviceObjectIDs* pEnumObjectIDs,PWSTR pSearchName,IPortableDeviceProperties *pProperties,
	LPWSTR *pReturnedObjectId)
{
	int match=0;
	IPortableDeviceKeyCollection* pPropertiesToRead=NULL;
	HRESULT hr = CoCreateInstance(CLSID_PortableDeviceKeyCollection,
                      NULL,
                      CLSCTX_INPROC_SERVER,
					  IID_IPortableDeviceKeyCollection,
                      (VOID**) &pPropertiesToRead);
	if (SUCCEEDED(hr)) {
		pPropertiesToRead->Add(WPD_OBJECT_NAME);
		pPropertiesToRead->Add(WPD_OBJECT_ORIGINAL_FILE_NAME);
		pPropertiesToRead->Add(WPD_STORAGE_DESCRIPTION);
		pPropertiesToRead->Add(WPD_FUNCTIONAL_OBJECT_CATEGORY);
		pPropertiesToRead->Add(WPD_OBJECT_CONTENT_TYPE);
	} else
		return 0;

	DWORD  cFetched;
	do {
		cFetched = 0;
		PWSTR  szObjectIDArray[ArraySize] = {0};
		hr = pEnumObjectIDs->Next(ArraySize,szObjectIDArray,&cFetched);
		if (SUCCEEDED(hr)) {
			for (DWORD i=0;i<cFetched;i++) {
				IPortableDeviceValues* pObjectProperties=NULL;
				HRESULT hr2=pProperties->GetValues(szObjectIDArray[i],
						pPropertiesToRead,   // The properties we want to read
						&pObjectProperties); // Driver supplied property values for the specified object
				if (SUCCEEDED(hr2) && pObjectProperties) {
					WCHAR showname[MAX_PATH];
					PickObjectDisplayName(pObjectProperties, showname, MAX_PATH);
					if (showname[0] && wcscmp(showname,pSearchName)==0)
						match=IsWpdFolderObject(pObjectProperties) ? 2 : 1;
					pObjectProperties->Release();
					if (match) {
						if (pReturnedObjectId) {
							*pReturnedObjectId=szObjectIDArray[i];
							szObjectIDArray[i]=NULL;  // do not delete it!
						}
						break;
					}
				}
			}
			for (DWORD i=0;i<cFetched;i++)
				if (szObjectIDArray[i])
					CoTaskMemFree(szObjectIDArray[i]);
			if (match)
				break;
		} else
			break;
	} while (cFetched==ArraySize);
	pPropertiesToRead->Release();
	return match;
}

BOOL __stdcall FsMkDirW(WCHAR* Path)
{
	if (Path[0]!='\\')
		return false;
	BOOL result=false;

	InitFunctionsIfNeeded(TRUE);
	{
		WCHAR dev[MAX_PATH];
		const WCHAR* rp=Path[0]=='\\' ? Path+1 : Path;
		wcslcpy(dev, rp, MAX_PATH);
		WCHAR* sl=wcschr(dev, '\\');
		WCHAR rel[wdirtypemax]=L"";
		if (sl) {
			sl[0]=0;
			wcslcpy(rel, sl+1, wdirtypemax);
			wcutlastbackslash(rel);
		}
		if (AppleMdIsDeviceName(dev))
			return AppleMdMkDir(dev, rel);
	}

	WCHAR wcSearch[wdirtypemax],*p;
	wcslcpy(wcSearch,Path,wdirtypemax-1);
	p=wcsrchr(wcSearch,'\\');
	if (p) {
		p[0]=0;
		p++;
		IEnumPortableDeviceObjectIDs* pEnumObjectIDs=NULL;
		IPortableDeviceProperties* pProperties=NULL;
		IPortableDeviceContent* pDeviceContent=NULL;
		LPWSTR pStorageID=NULL;
		HRESULT hr = GetFolderIDFromPathName(wcSearch,&pEnumObjectIDs,&pProperties,&pDeviceContent,&pStorageID);
		if (SUCCEEDED(hr)) {
			// Make sure that there isn't already a file with that name!
			int i=NameExistsInEnum(pEnumObjectIDs,p,pProperties,NULL);
			if (i!=0) {
				if (i==2)  // a folder?
					result=true;
				else
					result=false;
			} else {
				IPortableDeviceValues* pValues;
				hr = CoCreateInstance(CLSID_PortableDeviceValues,NULL,
					CLSCTX_INPROC_SERVER,IID_IPortableDeviceValues,(VOID**) &pValues);
				if (SUCCEEDED(hr)) {
					pValues->SetGuidValue(WPD_OBJECT_CONTENT_TYPE,WPD_CONTENT_TYPE_FOLDER);
					pValues->SetStringValue(WPD_OBJECT_PARENT_ID,pStorageID);
					pValues->SetStringValue(WPD_OBJECT_ORIGINAL_FILE_NAME,p);
					pValues->SetStringValue(WPD_OBJECT_NAME,p);

					LPWSTR NewObject=NULL;
					hr = pDeviceContent->CreateObjectWithPropertiesOnly( pValues,&NewObject);
					if SUCCEEDED(hr)
					{
						result=true;
						WCHAR buf1[wdirtypemax];
						wcslcpy(buf1,L"MKDIR ",wdirtypemax-1);
						wcslcat(buf1,Path,wdirtypemax-1);
						LogProcT(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,buf1);
					}
					if (NewObject)
						CoTaskMemFree(NewObject);
					pValues->Release();
				}
			}
			if (pStorageID)
				CoTaskMemFree(pStorageID);
			if (pDeviceContent)
				pDeviceContent->Release();
			if (pEnumObjectIDs)
				pEnumObjectIDs->Release();
			if (pProperties)
				pProperties->Release();
		}
	}
	return result;
}

BOOL __stdcall FsDeleteFile(char* RemoteName)
{
	WCHAR RemoteNameW[wdirtypemax];
	return FsDeleteFileW(awfilenamecopy(RemoteNameW,RemoteName));
}

BOOL __stdcall FsDeleteFileW(WCHAR* RemoteName)
{
	if (RemoteName[0]!='\\')
		return false;

	InitFunctionsIfNeeded(TRUE);
	{
		WCHAR dev[MAX_PATH];
		WCHAR wcTmp[wdirtypemax];
		wcslcpy(wcTmp, RemoteName, wdirtypemax-1);
		int tl=(int)wcslen(wcTmp)-1;
		if (tl>=0 && wcTmp[tl]=='\\')
			wcTmp[tl]=0;
		const WCHAR* rp=wcTmp[0]=='\\' ? wcTmp+1 : wcTmp;
		wcslcpy(dev, rp, MAX_PATH);
		WCHAR* sl=wcschr(dev, '\\');
		WCHAR rel[wdirtypemax]=L"";
		if (sl) {
			sl[0]=0;
			wcslcpy(rel, sl+1, wdirtypemax);
		}
		if (AppleMdIsDeviceName(dev))
			return AppleMdDelete(dev, rel);
	}

	WCHAR wcSearch[wdirtypemax];
	wcslcpy(wcSearch,RemoteName,wdirtypemax-1);
	int l=(int)wcslen(wcSearch)-1;
	if (wcSearch[l]=='\\')
		wcSearch[l]=0;
	BOOL result=false;
	LPWSTR p=wcsrchr(wcSearch,'\\');
	if (p) {
		p[0]=0;
		p++;
		IEnumPortableDeviceObjectIDs* pEnumObjectIDs=NULL;
		IPortableDeviceProperties* pProperties=NULL;
		IPortableDeviceContent* pDeviceContent=NULL;
		LPWSTR pStorageID=NULL;
		LPWSTR pItemStorageID=NULL;
		HRESULT hr = GetFolderIDFromPathName(wcSearch,&pEnumObjectIDs,&pProperties,&pDeviceContent,&pStorageID);
		if (SUCCEEDED(hr)) {
			// Find the file/folder with this name!
			int i=NameExistsInEnum(pEnumObjectIDs,p,pProperties,&pItemStorageID);
			if (i!=0 && pItemStorageID) {
				IPortableDevicePropVariantCollection* pCollection;
				hr = CoCreateInstance(CLSID_PortableDevicePropVariantCollection,NULL,
					CLSCTX_INPROC_SERVER,IID_IPortableDevicePropVariantCollection,(VOID**) &pCollection);
				if (SUCCEEDED(hr)) {
					PROPVARIANT pv = {0};
					PropVariantInit(&pv);
					pv.vt      = VT_LPWSTR;
					int len=(int)wcslen(pItemStorageID)+1;
					pv.pwszVal=(LPWSTR)CoTaskMemRealloc(NULL,len*sizeof(WCHAR));
					wcscpy_s((LPWSTR)pv.pwszVal,len,pItemStorageID);
					pCollection->Add(&pv);
					hr = pDeviceContent->Delete(PORTABLE_DEVICE_DELETE_NO_RECURSION,pCollection,NULL);
					if (SUCCEEDED(hr)) {
						if (hr!=S_FALSE) {
							RemoveFullPathFromCache(RemoteName);
							result=true;
						}
					}
					PropVariantClear(&pv);
					pCollection->Release();
				}
			}
		}
		if (pItemStorageID)
			CoTaskMemFree(pItemStorageID);
		if (pStorageID)
			CoTaskMemFree(pStorageID);
		if (pDeviceContent)
			pDeviceContent->Release();
		if (pEnumObjectIDs)
			pEnumObjectIDs->Release();
		if (pProperties)
			pProperties->Release();
	}
	return result;
}

BOOL __stdcall FsRemoveDir(char* RemoteName)
{
	WCHAR RemoteNameW[wdirtypemax];
	return FsRemoveDirW(awfilenamecopy(RemoteNameW,RemoteName));
}

BOOL __stdcall FsRemoveDirW(WCHAR* RemoteName)
{
	if (RemoteName[0]!='\\')
		return false;
	return FsDeleteFileW(RemoteName);
}

int __stdcall FsRenMovFile(char* OldName,char* NewName,BOOL Move,BOOL OverWrite,RemoteInfoStruct* ri)
{
	WCHAR NewNameW[wdirtypemax],OldNameW[wdirtypemax];
	return FsRenMovFileW(awfilenamecopy(OldNameW,OldName),awfilenamecopy(NewNameW,NewName),Move,OverWrite,ri);
}

int __stdcall FsRenMovFileW(WCHAR* OldName,WCHAR* NewName,BOOL Move,BOOL OverWrite,RemoteInfoStruct* ri)
{
	if (OldName[0]!='\\'  || NewName[0]!='\\')
		return FS_FILE_NOTFOUND;

	int err=ProgressProcT(PluginNumber,OldName,NewName,0);
	if (err)
		return FS_FILE_USERABORT;

	if (!InitFunctionsIfNeeded(TRUE))
		return FS_FILE_READERROR;

	WCHAR buf1[wdirtypemax];
	if (Move) 
		wcscpy_s(buf1,6,L"MOVE ");
	else
		wcscpy_s(buf1,6,L"COPY ");

	WCHAR WOldName[wdirtypemax],*p,*p2;
	WCHAR WNewName[wdirtypemax];
	wcslcpy(WOldName,OldName,wdirtypemax-1);
	wcslcpy(WNewName,NewName,wdirtypemax-1);
	p=wcsrchr(WOldName,'\\');
	p2=wcsrchr(WNewName,'\\');
	int result=FS_FILE_NOTFOUND;
	if (p && p2) {
		IEnumPortableDeviceObjectIDs* pEnumObjectIDs=NULL;
		IEnumPortableDeviceObjectIDs* pEnumObjectIDs2=NULL;
		IPortableDeviceProperties* pProperties=NULL;
		IPortableDeviceProperties* pProperties2=NULL;
		IPortableDeviceContent* pDeviceContent=NULL;
		IPortableDeviceContent* pDeviceContent2=NULL;
		LPWSTR pStorageID=NULL;
		LPWSTR pStorageID2=NULL;
		LPWSTR pItemStorageID=NULL;

		p[0]=0;
		p++;
		p2[0]=0;
		p2++;
		// the interface supports rename in place, and copy/move WITHOUT renaming. Determine which to do first!
		BOOL samedir=wcscmp(WOldName,WNewName)==0;
		BOOL samename=wcscmp(p,p2)==0;
		if (samedir && samename)
			result=FS_FILE_OK;
		else if (!samedir && !samename)
			result=FS_FILE_NOTSUPPORTED;
		else if (!samename) {    // rename in same dir!
			result=FS_FILE_NOTSUPPORTED;
			HRESULT hr = GetFolderIDFromPathName(WOldName,&pEnumObjectIDs,&pProperties,&pDeviceContent,&pStorageID);
			if (SUCCEEDED(hr)) {
				int i=NameExistsInEnum(pEnumObjectIDs,p,pProperties,&pItemStorageID);
				LPWSTR pDestStorageID=NULL;
				int i2=0;
				if (pEnumObjectIDs)
					pEnumObjectIDs->Reset();
				i2=NameExistsInEnum(pEnumObjectIDs,p2,pProperties,&pDestStorageID);
				if (i2 && pDestStorageID && pItemStorageID && wcscmp(pDestStorageID,pItemStorageID)==0) {
					// same object (case-only rename on a case-insensitive store)
					i2=0;
					CoTaskMemFree(pDestStorageID);
					pDestStorageID=NULL;
				}
				if (i2) {
					if (i2==2)
						result=FS_FILE_WRITEERROR;  // cannot overwrite folder with file!
					else if (OverWrite) {
						if (!FsDeleteFileW(NewName)) {
							result=FS_FILE_WRITEERROR;
							i=0;
						}
					} else {
						i=0;
						result=FS_FILE_EXISTS;
					}
				}
				if (pDestStorageID)
					CoTaskMemFree(pDestStorageID);
				if (i!=0 && pItemStorageID) {
					IPortableDeviceValues* pObjectProperties=NULL;
					IPortableDeviceValues* pResultProperties=NULL;
					hr = CoCreateInstance(CLSID_PortableDeviceValues,
                              NULL,
                              CLSCTX_INPROC_SERVER,
							  IID_IPortableDeviceValues,
                              (VOID**)&pObjectProperties);
					if (SUCCEEDED(hr)) {
						hr = pObjectProperties->SetStringValue(WPD_OBJECT_ORIGINAL_FILE_NAME, p2);
						hr = pObjectProperties->SetStringValue(WPD_OBJECT_NAME, p2);
					}
					if (SUCCEEDED(hr)) {
						hr=pProperties->SetValues(pItemStorageID,pObjectProperties,&pResultProperties);
						if (FAILED(hr)) {
							hr = pObjectProperties->Clear();
							if (SUCCEEDED(hr))
								hr = pObjectProperties->SetStringValue(WPD_OBJECT_NAME, p2);
							if (SUCCEEDED(hr))
								hr=pProperties->SetValues(pItemStorageID,pObjectProperties,NULL);
						}
					}
					if (SUCCEEDED(hr)) {
						if (hr!=S_FALSE) {
							RemoveFullPathFromCache(OldName);
							RemoveFullPathFromCache(NewName);
							result=FS_FILE_OK;
						}
					} else if (hr==HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
						result=FS_FILE_NOTSUPPORTED;
					else
						result=FS_FILE_WRITEERROR;
					if (pObjectProperties)
						pObjectProperties->Release();
					if (pResultProperties)
						pResultProperties->Release();
				}
			}
		} else {  // !samedir
			HRESULT hr = GetFolderIDFromPathName(WOldName,&pEnumObjectIDs,&pProperties,&pDeviceContent,&pStorageID);
			if (SUCCEEDED(hr)) {
				hr = GetFolderIDFromPathName(WNewName,&pEnumObjectIDs2,&pProperties2,&pDeviceContent2,&pStorageID2);
				if (FAILED(hr))
					result=FS_FILE_WRITEERROR;
			} else
				result=FS_FILE_READERROR;
			if (SUCCEEDED(hr)) {
				// Find the file/folder with this name!
				int i=NameExistsInEnum(pEnumObjectIDs,p,pProperties,&pItemStorageID);
				int i2=NameExistsInEnum(pEnumObjectIDs2,p,pProperties2,NULL);
				if (i2) {
					if (i2==2)
						result=FS_FILE_WRITEERROR;  // cannot overwrite folder with file!
					else if (OverWrite) {
						if (!FsDeleteFileW(NewName)) {
							result=FS_FILE_WRITEERROR;
							i=0;
						}
					} else {
						i=0;
						result=FS_FILE_EXISTS;
					}
				}
				if (i!=0 && pItemStorageID) {
					IPortableDevicePropVariantCollection* pCollection=NULL;
					hr = CoCreateInstance(CLSID_PortableDevicePropVariantCollection,NULL,
						CLSCTX_INPROC_SERVER,IID_IPortableDevicePropVariantCollection,(VOID**) &pCollection);
					if (SUCCEEDED(hr)) {
						PROPVARIANT pv = {0};
						PropVariantInit(&pv);
						pv.vt      = VT_LPWSTR;
						pv.pwszVal=wstrnew(pItemStorageID);
						pCollection->Add(&pv);
						result=FS_FILE_WRITEERROR;
						if (Move)
							hr = pDeviceContent->Move(pCollection,pStorageID2,NULL);
						else
							hr = pDeviceContent->Copy(pCollection,pStorageID2,NULL);
						if (SUCCEEDED(hr)) {
							if (hr!=S_FALSE) {
								RemoveFullPathFromCache(OldName);
								RemoveFullPathFromCache(NewName);
								result=FS_FILE_OK;
							}
						} else if (hr==HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
							result=FS_FILE_NOTSUPPORTED;
						PropVariantClear(&pv);
					}
					if (pCollection)
						pCollection->Release();
				}
			}
		}
		if (pItemStorageID)
			CoTaskMemFree(pItemStorageID);
		if (pStorageID)
			CoTaskMemFree(pStorageID);
		if (pStorageID2)
			CoTaskMemFree(pStorageID2);
		if (pDeviceContent)
			pDeviceContent->Release();
		if (pDeviceContent2)
			pDeviceContent2->Release();
		if (pEnumObjectIDs)
			pEnumObjectIDs->Release();
		if (pEnumObjectIDs2)
			pEnumObjectIDs2->Release();
		if (pProperties)
			pProperties->Release();
		if (pProperties2)
			pProperties2->Release();
	}
	if (result==FS_FILE_OK) {
		result=ProgressProcT(PluginNumber,OldName,NewName,100);
		if (result)
			return FS_FILE_USERABORT;
	}
	if (result==FS_FILE_OK) {
		wcslcat(buf1,OldName,wdirtypemax-1);
		wcslcat(buf1,L"->",wdirtypemax-1);
		wcslcat(buf1,NewName,wdirtypemax-1);
		LogProcT(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,buf1);
	}
	return result;
}

int __stdcall FsGetFile(char* RemoteName,char* LocalName,int CopyFlags,RemoteInfoStruct* ri)
{
	WCHAR RemoteNameW[wdirtypemax],LocalNameW[wdirtypemax],OldLocalNameW[wdirtypemax];
	awfilenamecopy(RemoteNameW,RemoteName);
	awfilenamecopy(LocalNameW,LocalName);
	wcslcpy(OldLocalNameW,LocalNameW,wdirtypemax-1);
	int ret=FsGetFileW(RemoteNameW,LocalNameW,CopyFlags,ri);
	// A conversion may cause a name change
	if (ret==0 && wcscmp(LocalNameW,OldLocalNameW)!=0) {
		wafilenamecopy(LocalName,LocalNameW);
	}
	return ret;
}

int __stdcall FsGetFileW(WCHAR* RemoteName,WCHAR* LocalName,int CopyFlags,RemoteInfoStruct* ri)
{
    int err;
	BOOL OverWrite,Resume,Move;

	OverWrite=CopyFlags & FS_COPYFLAGS_OVERWRITE;
	Resume=CopyFlags & FS_COPYFLAGS_RESUME;
	Move=CopyFlags & FS_COPYFLAGS_MOVE;
	
	if (RemoteName[0]!='\\')
		return FS_FILE_NOTFOUND;
	if (Resume)
		return FS_FILE_NOTSUPPORTED;

	if (!OverWrite) {
		WIN32_FIND_DATAW fd;
		HANDLE hd;
		hd=FindFirstFileT(LocalName,&fd);
		if (hd!=INVALID_HANDLE_VALUE) {
			FindClose(hd);
			return FS_FILE_EXISTS;
		}
	}

	err=ProgressProcT(PluginNumber,RemoteName,LocalName,0);
	if (err)
		return FS_FILE_USERABORT;

	if (!InitFunctionsIfNeeded(TRUE))
		return FS_FILE_READERROR;

	EnsureComApartment();
	WCHAR WLocalName[wdirtypemax];
	WCHAR WRemoteName[wdirtypemax];
	wcslcpy(WLocalName,LocalName,wdirtypemax-1);
	wcslcpy(WRemoteName,RemoteName,wdirtypemax-1);
	{
		WCHAR dev[MAX_PATH];
		const WCHAR* rp=WRemoteName[0]=='\\' ? WRemoteName+1 : WRemoteName;
		wcslcpy(dev, rp, MAX_PATH-1);
		WCHAR* sl=wcschr(dev, '\\');
		WCHAR rel[wdirtypemax]=L"";
		if (sl) {
			sl[0]=0;
			wcslcpy(rel, sl+1, wdirtypemax-1);
		}
		if (AppleMdIsDeviceName(dev)) {
			FILETIME* mt=NULL;
			if (ri && !(ri->LastWriteTime.dwHighDateTime==0xFFFFFFFF))
				mt=&ri->LastWriteTime;
			ULONGLONG sz=0;
			if (ri)
				sz=((ULONGLONG)ri->SizeHigh<<32)+ri->SizeLow;
			return AppleMdGetFile(dev, rel, WLocalName, sz, mt);
		}
	}
	LPWSTR pItemStorageID=NULL;
	IPortableDeviceContent* pDeviceContent=NULL;
	LockPlugin();
	InterlockedExchange(&g_abort,0);
	HRESULT hr = GetFolderIDFromPathName(WRemoteName,NULL,NULL,&pDeviceContent,&pItemStorageID);
	SetCancelDevice(FindStoredDeviceByPath(WRemoteName));
	ULONGLONG totalsize=0;
	ULONGLONG totalcopied=0;
	if (ri) {
		totalsize=ri->SizeHigh;
		totalsize=(totalsize<<32) + ri->SizeLow;
	}
	int result=FS_FILE_READERROR;
	if (SUCCEEDED(hr)) {
		IPortableDeviceResources *pResources=NULL;
		IStream *pStream=NULL;
		hr=pDeviceContent->Transfer(&pResources);
		DWORD OptimalBufferSize=32768;
		if (SUCCEEDED(hr))
			hr= pResources->GetStream(pItemStorageID,
				WPD_RESOURCE_DEFAULT,
				STGM_READ,
				&OptimalBufferSize,
				&pStream);
		if SUCCEEDED(hr) {
			if (OptimalBufferSize<1024)
				OptimalBufferSize=1024;
			char* buf=(char*)malloc(OptimalBufferSize);
			if (!buf) {
				result=FS_FILE_READERROR;
			} else {
				DWORD BytesRead,BytesWritten;
				HANDLE f=CreateFileT(LocalName,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,0,NULL);
				if (f!=INVALID_HANDLE_VALUE) {
					DWORD lasttime=GetTickCount();
					DWORD thistime;
					while (1) {
						hr=pStream->Read(buf,OptimalBufferSize,&BytesRead);
						if (SUCCEEDED(hr) && BytesRead>0) {
							if (!WriteFile(f,buf,BytesRead,&BytesWritten,NULL)) {
								result=FS_FILE_WRITEERROR;
								break;
							}
							totalcopied+=BytesWritten;
							thistime=GetTickCount();
							if (totalsize && (thistime-lasttime)>100) {
								int percent=(int)((totalcopied*100)/totalsize);
								lasttime=thistime;
								err=ProgressCheck(NULL,NULL,percent);
								if (err) {
									result=FS_FILE_USERABORT;
									break;
								}
							}
						} else {
							if (BytesRead==0)
								result=FS_FILE_OK;
							break;
						}
					}
					if (ri && result==FS_FILE_OK)  
						if (!(ri->LastWriteTime.dwHighDateTime==0xFFFFFFFF && ri->LastWriteTime.dwLowDateTime==0xFFFFFFFF) &&
							!(ri->LastWriteTime.dwHighDateTime==0xFFFFFFFF && ri->LastWriteTime.dwLowDateTime==0xFFFFFFFE) &&
							!(ri->LastWriteTime.dwHighDateTime>=27846442 && ri->LastWriteTime.dwHighDateTime<=27846660) && //1.1.1980, different time zones
							!(ri->LastWriteTime.dwHighDateTime==0 && ri->LastWriteTime.dwLowDateTime==0))  // only if valid!
								SetFileTime(f,NULL,NULL,&ri->LastWriteTime);
					CloseHandle(f);
					if (result!=FS_FILE_OK)
						DeleteFileT(LocalName);
				} else
					result=FS_FILE_WRITEERROR;
				free(buf);
			}
		}
		if (pStream)
			pStream->Release();
		if (pResources)
			pResources->Release();
	}
	if (pItemStorageID)
		CoTaskMemFree(pItemStorageID);
	if (pDeviceContent)
		pDeviceContent->Release();
	SetCancelDevice(NULL);
	UnlockPlugin();
	return result;
}

int __stdcall FsPutFile(char* LocalName,char* RemoteName,int CopyFlags)
{
	WCHAR RemoteNameW[wdirtypemax],LocalNameW[wdirtypemax],OldRemoteNameW[wdirtypemax];
	awfilenamecopy(LocalNameW,LocalName);
	awfilenamecopy(RemoteNameW,RemoteName);
	wcslcpy(OldRemoteNameW,RemoteNameW,wdirtypemax-1);
	int ret=FsPutFileW(LocalNameW,RemoteNameW,CopyFlags);
	// A conversion may cause a name change
	if (ret==0 && wcscmp(RemoteNameW,OldRemoteNameW)!=0) {
		wafilenamecopy(RemoteName,RemoteNameW);
	}
	return ret;
}

typedef struct _ExtensionMap
{
    LPCWSTR wszExtension;
    const GUID* formatCode;
	const GUID* contentCode;
} ExtensionMap;

static const ExtensionMap rgExtensionMap[] =
{
    { L"aiff",  &WPD_OBJECT_FORMAT_AIFF, &WPD_CONTENT_TYPE_AUDIO },
    { L"wav",   &WPD_OBJECT_FORMAT_WAVE, &WPD_CONTENT_TYPE_AUDIO  },
    { L"mp2",   &WPD_OBJECT_FORMAT_MP2, &WPD_CONTENT_TYPE_AUDIO  },
    { L"mp3",   &WPD_OBJECT_FORMAT_MP3, &WPD_CONTENT_TYPE_AUDIO  },
	{ L"wma",   &WPD_OBJECT_FORMAT_WMA, &WPD_CONTENT_TYPE_AUDIO },
	{ L"au",   &WPD_OBJECT_FORMAT_AUDIBLE, &WPD_CONTENT_TYPE_AUDIO  },
	{ L"aac",   &WPD_OBJECT_FORMAT_AAC, &WPD_CONTENT_TYPE_AUDIO  },
    { L"m4a",   &WPD_OBJECT_FORMAT_AAC, &WPD_CONTENT_TYPE_AUDIO },
	{ L"ogg",   &WPD_OBJECT_FORMAT_OGG, &WPD_CONTENT_TYPE_AUDIO},
	{ L"flac",   &WPD_OBJECT_FORMAT_FLAC, &WPD_CONTENT_TYPE_AUDIO},
	{ L"opus",   &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_AUDIO},
	{ L"mp4",   &WPD_OBJECT_FORMAT_MP4, &WPD_CONTENT_TYPE_VIDEO },
	{ L"m4v",   &WPD_OBJECT_FORMAT_MP4, &WPD_CONTENT_TYPE_VIDEO },
	{ L"mov",   &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_VIDEO },
	{ L"mkv",   &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_VIDEO },
	{ L"webm",  &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_VIDEO },
    { L"avi",   &WPD_OBJECT_FORMAT_AVI, &WPD_CONTENT_TYPE_VIDEO },
    { L"mpeg",  &WPD_OBJECT_FORMAT_MPEG, &WPD_CONTENT_TYPE_VIDEO },
    { L"mpg",   &WPD_OBJECT_FORMAT_MPEG, &WPD_CONTENT_TYPE_VIDEO },
	{ L"3gp",   &WPD_OBJECT_FORMAT_3GP, &WPD_CONTENT_TYPE_VIDEO },
    { L"asf",   &WPD_OBJECT_FORMAT_ASF, &WPD_CONTENT_TYPE_VIDEO },
    { L"wmv",   &WPD_OBJECT_FORMAT_WMV, &WPD_CONTENT_TYPE_VIDEO },  
    { L"dvr-ms",&WPD_OBJECT_FORMAT_ASF, &WPD_CONTENT_TYPE_VIDEO },
    { L"jpg",   &WPD_OBJECT_FORMAT_JFIF, &WPD_CONTENT_TYPE_IMAGE },
    { L"jpe",   &WPD_OBJECT_FORMAT_JFIF, &WPD_CONTENT_TYPE_IMAGE },
    { L"jpeg",  &WPD_OBJECT_FORMAT_JFIF, &WPD_CONTENT_TYPE_IMAGE },
    { L"pcd",   &WPD_OBJECT_FORMAT_PCD, &WPD_CONTENT_TYPE_IMAGE },
    { L"bmp",  &WPD_OBJECT_FORMAT_BMP, &WPD_CONTENT_TYPE_IMAGE },
	{ L"pict",  &WPD_OBJECT_FORMAT_PICT, &WPD_CONTENT_TYPE_IMAGE },
	{ L"gif",  	&WPD_OBJECT_FORMAT_GIF, &WPD_CONTENT_TYPE_IMAGE },
	{ L"png",  	&WPD_OBJECT_FORMAT_PNG, &WPD_CONTENT_TYPE_IMAGE },
    { L"tif",  	&WPD_OBJECT_FORMAT_TIFF, &WPD_CONTENT_TYPE_IMAGE },
	{ L"tiff",  	&WPD_OBJECT_FORMAT_TIFF, &WPD_CONTENT_TYPE_IMAGE },
	{ L"webp",  &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_IMAGE },
	{ L"heic",  &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_IMAGE },
	{ L"heif",  &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_IMAGE },
	{ L"avif",  &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_IMAGE },
	{ L"dng",   &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_IMAGE },
	{ L"cr2",   &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_IMAGE },
	{ L"nef",   &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_IMAGE },
	{ L"arw",   &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_IMAGE },
	{ L"rw2",   &WPD_OBJECT_FORMAT_UNSPECIFIED, &WPD_CONTENT_TYPE_IMAGE },
	{ L"mpl",   &WPD_OBJECT_FORMAT_MPLPLAYLIST, &WPD_CONTENT_TYPE_PLAYLIST},
	{ L"asx",   &WPD_OBJECT_FORMAT_ASXPLAYLIST, &WPD_CONTENT_TYPE_PLAYLIST},
	{ L"pls",   &WPD_OBJECT_FORMAT_PLSPLAYLIST, &WPD_CONTENT_TYPE_PLAYLIST},
	{ L"m3u",   &WPD_OBJECT_FORMAT_M3UPLAYLIST, &WPD_CONTENT_TYPE_PLAYLIST},
	{ L"doc",   &WPD_OBJECT_FORMAT_MICROSOFT_WORD, &WPD_CONTENT_TYPE_DOCUMENT},
	{ L"docx",   &WPD_OBJECT_FORMAT_MICROSOFT_WORD, &WPD_CONTENT_TYPE_DOCUMENT},
	{ L"xls",   &WPD_OBJECT_FORMAT_MICROSOFT_EXCEL, &WPD_CONTENT_TYPE_DOCUMENT},
	{ L"xlm",   &WPD_OBJECT_FORMAT_MICROSOFT_EXCEL, &WPD_CONTENT_TYPE_DOCUMENT},
	{ L"xlsx",   &WPD_OBJECT_FORMAT_MICROSOFT_EXCEL, &WPD_CONTENT_TYPE_DOCUMENT},
	{ L"ppt",   &WPD_OBJECT_FORMAT_MICROSOFT_POWERPOINT, &WPD_CONTENT_TYPE_DOCUMENT},
	{ L"pptx",   &WPD_OBJECT_FORMAT_MICROSOFT_POWERPOINT, &WPD_CONTENT_TYPE_DOCUMENT},
	{ L"txt",   &WPD_OBJECT_FORMAT_UNSPECIFIED,&WPD_CONTENT_TYPE_DOCUMENT},
	{ L"rtf",   &WPD_OBJECT_FORMAT_UNSPECIFIED,&WPD_CONTENT_TYPE_DOCUMENT},
	{ L"pdf",   &WPD_OBJECT_FORMAT_UNSPECIFIED,&WPD_CONTENT_TYPE_DOCUMENT},
	{ L"epub",  &WPD_OBJECT_FORMAT_UNSPECIFIED,&WPD_CONTENT_TYPE_DOCUMENT},
	{ L"apk",   &WPD_OBJECT_FORMAT_UNSPECIFIED,&WPD_CONTENT_TYPE_GENERIC_FILE}
};

void GetFormatCodeFromFile(WCHAR* pszFile,const GUID** pFormat,const GUID** pContent)
{
    CONST GUID* dwFormatCode = &WPD_OBJECT_FORMAT_UNSPECIFIED;
	CONST GUID* dwContentCode = &WPD_CONTENT_TYPE_GENERIC_FILE;
	WCHAR* p=wcsrchr(pszFile,'\\');
	if (!p)
		p=pszFile;
    WCHAR* pszExtension = wcsrchr(p,'.');
    if (pszExtension)
    {
        if (L'.' == pszExtension[0])
        {
            pszExtension++;
        }
        for (int i = 0; i < ARRAYSIZE(rgExtensionMap); i++)
        {
            //the length of comparison is bounded by strings represented by rgExtensionMap[i].wszExtension             
            if (0 == _wcsicmp(pszExtension, rgExtensionMap[i].wszExtension))
            {
                dwFormatCode = rgExtensionMap[i].formatCode;
				dwContentCode = rgExtensionMap[i].contentCode;
                break;
            }
        }
    }
    *pFormat=dwFormatCode;
	*pContent=dwContentCode;
}

int __stdcall FsPutFileW(WCHAR* LocalName,WCHAR* RemoteName,int CopyFlags)
{
	WCHAR* unknown=L"unknown";
	int err;
	BOOL OverWrite,Resume,Move;
	int retval=FS_FILE_WRITEERROR;

	OverWrite=CopyFlags & FS_COPYFLAGS_OVERWRITE;
	Resume=CopyFlags & FS_COPYFLAGS_RESUME;
	Move=CopyFlags & FS_COPYFLAGS_MOVE;
	
	if (RemoteName[0]!='\\')
		return FS_FILE_NOTFOUND;

	err=ProgressProcT(PluginNumber,LocalName,RemoteName,0);
	if (err)
		return FS_FILE_USERABORT;
	
	if (!InitFunctionsIfNeeded(TRUE))
		return FS_FILE_READERROR;

	EnsureComApartment();
	WCHAR WLocalName[wdirtypemax],*p;
	WCHAR WRemoteName[wdirtypemax];
	wcslcpy(WLocalName,LocalName,wdirtypemax);
	wcslcpy(WRemoteName,RemoteName,wdirtypemax);
	{
		WCHAR dev[MAX_PATH];
		const WCHAR* rp=WRemoteName[0]=='\\' ? WRemoteName+1 : WRemoteName;
		wcslcpy(dev, rp, MAX_PATH);
		WCHAR* sl=wcschr(dev, '\\');
		WCHAR rel[wdirtypemax]=L"";
		if (sl) {
			sl[0]=0;
			wcslcpy(rel, sl+1, wdirtypemax);
		}
		if (AppleMdIsDeviceName(dev))
			return AppleMdPutFile(dev, rel, WLocalName, OverWrite);
	}
	LockPlugin();
	InterlockedExchange(&g_abort,0);
	SetCancelDevice(FindStoredDeviceByPath(RemoteName));
	p=wcsrchr(WRemoteName,'\\');
	int result=FS_FILE_READERROR;
	ULONGLONG totalsize=0;
	ULONGLONG totalcopied=0;
	if (p) {
		p[0]=0;
		p++;
		IEnumPortableDeviceObjectIDs* pEnumObjectIDs=NULL;
		IPortableDeviceProperties* pProperties=NULL;
		IPortableDeviceContent* pDeviceContent=NULL;
		LPWSTR pStorageID=NULL;
		LPWSTR pItemStorageID=NULL;
		HRESULT hr = GetFolderIDFromPathName(WRemoteName,&pEnumObjectIDs,&pProperties,&pDeviceContent,&pStorageID);
		if (SUCCEEDED(hr)) {
			// Make sure that there isn't already a file with that name!
			int i=NameExistsInEnum(pEnumObjectIDs,p,pProperties,&pItemStorageID);
			if (i==2)  // a folder?
				result=FS_FILE_WRITEERROR;
			else if (i==1 && !OverWrite)
					result=FS_FILE_EXISTS;
			else {
				if (i==1) {    // file exists -> delete it!
					IPortableDevicePropVariantCollection* pCollection;
					hr = CoCreateInstance(CLSID_PortableDevicePropVariantCollection,NULL,
						CLSCTX_INPROC_SERVER,IID_IPortableDevicePropVariantCollection,(VOID**) &pCollection);
					if (SUCCEEDED(hr)) {
						PROPVARIANT pv = {0};
						PropVariantInit(&pv);
						pv.vt      = VT_LPWSTR;
						pv.pwszVal=wstrnew(pItemStorageID);
						pCollection->Add(&pv);
						hr = pDeviceContent->Delete(PORTABLE_DEVICE_DELETE_NO_RECURSION,pCollection,NULL);
						pCollection->Release();
						if (hr == S_FALSE)
							hr=E_FAIL;
					}
				} else
					hr=S_OK;
				if (SUCCEEDED(hr)) {
					IPortableDeviceValues* pValues;
					hr = CoCreateInstance(CLSID_PortableDeviceValues,NULL,
						CLSCTX_INPROC_SERVER,IID_IPortableDeviceValues,(VOID**) &pValues);
					if (SUCCEEDED(hr)) {
						HANDLE f=CreateFileT(LocalName,GENERIC_READ,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
						if (f!=INVALID_HANDLE_VALUE) {
							DWORD SizeHigh;
							totalsize=GetFileSize(f,&SizeHigh);
							totalsize=totalsize | (((ULONGLONG)SizeHigh)<<32);
							IStream* pStream=NULL;
							CONST GUID* pFormat=NULL;
							CONST GUID* pContent=NULL;
							AlbumArtBlob* pPreviewImage=NULL;
							GetFormatCodeFromFile(RemoteName,&pFormat,&pContent);
							if (pFormat)
								pValues->SetGuidValue(WPD_OBJECT_FORMAT,*pFormat);
							if (pContent)
								pValues->SetGuidValue(WPD_OBJECT_CONTENT_TYPE,*pContent);
							pValues->SetStringValue(WPD_OBJECT_PARENT_ID,pStorageID);
							pValues->SetStringValue(WPD_OBJECT_ORIGINAL_FILE_NAME,p);
							// pValues->SetStringValue(WPD_OBJECT_NAME,p);   // set it to title instead for music!
							// set size
							PROPVARIANT pv1 = {0};
							PropVariantInit(&pv1);
							pv1.vt = VT_UI8;
							pv1.uhVal.QuadPart=totalsize;
							pValues->SetValue(WPD_OBJECT_SIZE,&pv1);
							
							// set date
							FILETIME filetime;
							SYSTEMTIME systime;
							PROPVARIANT pv = {0};
							PropVariantInit(&pv);
							pv.vt = VT_DATE;
							GetFileTime(f,NULL,NULL,&filetime);
							int LocalTime=UseLocalTime(RemoteName);
							if (LocalTime==2) {
								SYSTEMTIME systime1;
								FileTimeToSystemTime(&filetime,&systime1);
								TzSpecificLocalTimeToSystemTime(NULL,&systime1,&systime);
							} else if (LocalTime==1) {
								FILETIME ftime;
								FileTimeToLocalFileTime(&filetime,&ftime);
								FileTimeToSystemTime(&ftime,&systime);
							} else {
								FileTimeToSystemTime(&filetime,&systime);
							}

							SystemTimeToVariantTime(&systime,&pv.date);
							pValues->SetValue(WPD_OBJECT_DATE_MODIFIED,&pv);
							if (!OverWrite)
								pValues->SetValue(WPD_OBJECT_DATE_CREATED,&pv);
							pValues->SetValue(WPD_MEDIA_LAST_ACCESSED_TIME,&pv);
							pValues->SetValue(WPD_OBJECT_DATE_AUTHORED,&pv);
							
							hr = GetFileMetadata(WLocalName, pValues,pContent,&pPreviewImage);
							if (!SUCCEEDED(hr)) {
								pValues->SetStringValue(WPD_OBJECT_NAME,p);
								if (pFormat!=&WPD_OBJECT_FORMAT_UNSPECIFIED) {
									WCHAR *p1,*p2,*p3;
									WCHAR *pTitle=NULL;
									WCHAR *pAuthor=NULL;
									p1=wcsrchr(p,'\\');
									if (p1) p1++; else p1=p;
									p3=wcsrchr(p1,'.');   // remove extension
									if (p3)
										p3[0]=0;
									p2=wcsstr(p,L" - ");
									if (p2) {
										p2[0]=0;
										pTitle=wstrnew(p2+3);
										pAuthor=wstrnew(p1);
										p2[0]=' ';
									} else {   // just the title
										pTitle=wstrnew(p1);
										pAuthor=wstrnew(L"unknown");
									}
									pValues->SetStringValue(WPD_MEDIA_TITLE,pTitle);
									pValues->SetStringValue(WPD_MEDIA_ARTIST,pAuthor);
									CoTaskMemFree(pTitle);
									CoTaskMemFree(pAuthor);
									// use current date as album title!
									WCHAR buf[16];
									SYSTEMTIME systime;
									GetLocalTime(&systime);
									swprintf(buf,15,L"%04u%02u%02u",systime.wYear,systime.wMonth,systime.wDay);
									pValues->SetStringValue(WPD_MUSIC_ALBUM,buf);
									if (p3)
										p3[0]='.';  // restore extension
								}
							}

							// Now copy the actual data
							DWORD OptimalBufferSize=32768;
							hr = pDeviceContent->CreateObjectWithPropertiesAndData(pValues,&pStream,&OptimalBufferSize,NULL);
							if SUCCEEDED(hr) {
								RemoveFullPathFromCache(RemoteName);
								if (OptimalBufferSize<1024)
									OptimalBufferSize=1024;
								char* buf=(char*)malloc(OptimalBufferSize);
								if (!buf) {
									result=FS_FILE_READERROR;
								} else {
									DWORD BytesRead,BytesWritten;
									DWORD lasttime=GetTickCount();
									DWORD thistime;
									while (1) {
										if (ReadFile(f,buf,OptimalBufferSize,&BytesRead,NULL) && BytesRead>0) {
											hr=pStream->Write(buf,BytesRead,&BytesWritten);
											if (!SUCCEEDED(hr) || BytesWritten==0) {
												result=FS_FILE_WRITEERROR;
												break;
											}
											totalcopied+=BytesWritten;
											thistime=GetTickCount();
											if (totalsize && (thistime-lasttime)>100) {
												int percent=(int)((totalcopied*100)/totalsize);
												lasttime=thistime;
												err=ProgressCheck(NULL,NULL,percent);
												if (err) {
													result=FS_FILE_USERABORT;
													break;
												}
											}
										} else {
											if (BytesRead==0)
												result=FS_FILE_OK;
											break;
										}
									}
									free(buf);
								}
								if (result==FS_FILE_OK)
									pStream->Commit(STGC_DEFAULT);
								IPortableDeviceDataStream* pResultingStream=NULL;
								IPortableDeviceValues* spResInfo=NULL;
								LPWSTR NewId=NULL;
								DWORD cnt=0;
								UINT PreviewImageWidth=0;
								UINT PreviewImageHeight=0;
								IPortableDeviceKeyCollection* pKeys=NULL;
								IPortableDeviceResources* pResources=NULL;
								if (result!=FS_FILE_OK || !pPreviewImage || !GdiPlusInitialize())
									pStream->Release();
								else {
									HRESULT hr2=S_OK;
									CONST GUID* imgtype=NULL;
									if (_wcsicmp(pPreviewImage->mime,L"image/jpeg")==0) {
										imgtype=&WPD_OBJECT_FORMAT_JFIF;										
									} else if (_wcsicmp(pPreviewImage->mime,L"image/png")==0) {
										imgtype=&WPD_OBJECT_FORMAT_PNG;
									} else if (_wcsicmp(pPreviewImage->mime,L"image/gif")==0) {
										imgtype=&WPD_OBJECT_FORMAT_GIF;
									} else if (_wcsicmp(pPreviewImage->mime,L"image/bmp")==0) {
										imgtype=&WPD_OBJECT_FORMAT_BMP;
									} else
										hr2=E_FAIL;

									int err=0;
									if (SUCCEEDED(hr2))
										HRESULT hr2=pStream->QueryInterface(IID_IPortableDeviceDataStream,(void**)&pResultingStream);
									else
										err=1;
									pStream->Release();
									if (SUCCEEDED(hr2)) {
										hr2=pResultingStream->GetObjectID(&NewId);
										pResultingStream->Release();
										pResultingStream=NULL;
									} else if (err==0)
										err=2;
									if (SUCCEEDED(hr2)) {
										hr2=pDeviceContent->Transfer(&pResources);
									} else if (err==0)
										err=3;
									if (SUCCEEDED(hr2)) {
										hr2=pResources->GetSupportedResources(NewId,&pKeys);
									} else if (err==0)
										err=4;
									if (SUCCEEDED(hr2)) {
										hr2=pKeys->GetCount(&cnt);
									} else if (err==0)
										err=5;
									BOOL preview=false;
									char supportedformats[128];
									supportedformats[0]=0;
									if (SUCCEEDED(hr2) && cnt>0) {
										for (DWORD dw=0;dw<cnt;dw++) {
											PROPERTYKEY pKey;
											hr2=pKeys->GetAt(dw,&pKey);
											if (SUCCEEDED(hr2)) {
												if (IsEqualPropertyKey(WPD_RESOURCE_ALBUM_ART,pKey)) {
													preview=true;
													break;
												} else if (IsEqualPropertyKey(WPD_RESOURCE_DEFAULT,pKey)) {
													strcat_s(supportedformats,128,"data ");
												} else if (IsEqualPropertyKey(WPD_RESOURCE_THUMBNAIL,pKey)) {
													strcat_s(supportedformats,128,"thumb ");
												} else if (IsEqualPropertyKey(WPD_RESOURCE_ICON,pKey)) {
													strcat_s(supportedformats,128,"icon ");
												} else if (IsEqualPropertyKey(WPD_RESOURCE_BRANDING_ART,pKey)) {
													strcat_s(supportedformats,128,"logo ");
												} else if (IsEqualPropertyKey(WPD_RESOURCE_GENERIC,pKey)) {
													strcat_s(supportedformats,128,"generic ");
												}
											}
										}
									}											

									if (preview) {
										HANDLE DataHandle=GlobalAlloc(GMEM_MOVEABLE,pPreviewImage->len);
										if (DataHandle) {
											IStream* fDataStream;
											char* p=(char*)GlobalLock(DataHandle);
											memcpy(p,pPreviewImage->data,pPreviewImage->len);
											GlobalUnlock(DataHandle);
											if (SUCCEEDED(CreateStreamOnHGlobal(DataHandle,true,&fDataStream))) {  // now fStream = data
												Gdiplus::GpImage* GdiplusImage;
												int err=Gdiplus::DllExports::GdipLoadImageFromStream(fDataStream,&GdiplusImage);
												if (err==0) {
													if (Gdiplus::DllExports::GdipGetImageWidth(GdiplusImage,&PreviewImageWidth)!=0)
														PreviewImageWidth=0;
													if (Gdiplus::DllExports::GdipGetImageHeight(GdiplusImage,&PreviewImageHeight)!=0)
														PreviewImageHeight=0;
													 Gdiplus::DllExports::GdipDisposeImage(GdiplusImage);
												}
											}
											fDataStream->Release();
											GlobalFree(DataHandle);
										}
									} else if (err==0)
										err=6;

									if (PreviewImageWidth>0 && PreviewImageHeight>0)
										hr2=CoCreateInstance(CLSID_PortableDeviceValues, NULL, CLSCTX_INPROC_SERVER, IID_IPortableDeviceValues, (VOID**)&spResInfo);
									else {
										hr2=E_FAIL;
										if (err==0)
											err=7;
									}
									if (SUCCEEDED(hr2))
										hr2 = spResInfo->SetStringValue(WPD_OBJECT_ID, NewId);
									 else if (err==0)
										err=8;
									if (SUCCEEDED(hr2))
										hr2 = spResInfo->SetKeyValue(WPD_RESOURCE_ATTRIBUTE_RESOURCE_KEY,WPD_RESOURCE_ALBUM_ART);
									else if (err==0)
										err=9;
									if (SUCCEEDED(hr2))
										hr2 = spResInfo->SetSignedLargeIntegerValue(WPD_RESOURCE_ATTRIBUTE_TOTAL_SIZE,pPreviewImage->len);
									else if (err==0)
										err=10;
									if (SUCCEEDED(hr2))
										hr2 = spResInfo->SetGuidValue(WPD_RESOURCE_ATTRIBUTE_FORMAT,*imgtype);
									else if (err==0)
										err=11;
									if (SUCCEEDED(hr2))
										hr2 = spResInfo->SetSignedLargeIntegerValue(WPD_MEDIA_WIDTH,PreviewImageWidth);
									else if (err==0)
										err=12;
									if (SUCCEEDED(hr2))
										hr2 = spResInfo->SetSignedLargeIntegerValue(WPD_MEDIA_HEIGHT,PreviewImageHeight);
									else if (err==0)
										err=13;
									if (SUCCEEDED(hr2))
										hr2=pResources->CreateResource(spResInfo,&pStream,&OptimalBufferSize,NULL);
									else if (err==0)
										err=14;
									if (SUCCEEDED(hr2)) {
										DWORD BytesWritten;
										hr2=pStream->Write(pPreviewImage->data,pPreviewImage->len,&BytesWritten);
										if (!SUCCEEDED(hr2))
											err=16;
										HRESULT hr3=pStream->Commit(STGC_DEFAULT);
										if (!SUCCEEDED(hr2) && err==0) {
											hr2=hr3;
											err=17;
										}
										pStream->Release();
										pStream=NULL;
									} else if (err==0)
										err=15;
									if (err) {
										char errbuf[256];
										if (err==1)
											LogProc(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,"Unknown album art, skipped.");
										else if (err==6) {
											strcpy_s(errbuf,sizeof(errbuf),"Album art skipped (not supported by this device). Supported streams: ");
											strcat_s(errbuf,sizeof(errbuf),supportedformats);
											LogProc(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,errbuf);
										} else if (err==7)
											LogProc(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,"GDI+: Could not load image to determine the size.");
										else {
											sprintf_s(errbuf,sizeof(errbuf),"Error copying album image in function %d: %x",err,hr2);
											LogProc(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,errbuf);
										}
									}
								}
								if (spResInfo)
									spResInfo->Release();
								if (pKeys)
									pKeys->Release();
								if (pResources)
									pResources->Release();

/* Doesn't work, returns access denied
								// set date again after copying!
								IEnumPortableDeviceObjectIDs* pEnumObjectIDs2=NULL;
								IPortableDeviceProperties* pProperties2=NULL;
								LPWSTR pItemStorageID2=NULL;
								HRESULT hr = GetFolderIDFromPathName(WRemoteName,&pEnumObjectIDs2,&pProperties2,NULL,NULL);
								if (SUCCEEDED(hr)) {
									// Find the file/folder with this name!
									int i=NameExistsInEnum(pEnumObjectIDs2,p,pProperties2,&pItemStorageID2);
									if (i>=0 && pItemStorageID2) {
										PROPVARIANT pv = {0};
										PropVariantInit(&pv);
										pv.vt = VT_DATE;
										SystemTimeToVariantTime(&systime,&pv.date);
										pValues->SetValue(WPD_OBJECT_DATE_MODIFIED,&pv);
										pValues->SetValue(WPD_OBJECT_DATE_CREATED,&pv);
										IPortableDeviceValues* pValues2=NULL;
										hr=pProperties2->SetValues(pItemStorageID2,pValues,&pValues2);
										HRESULT res2=0;
										if (!SUCCEEDED(hr))
											MessageBeep(0);
										else {
											pValues2->GetErrorValue(WPD_OBJECT_DATE_MODIFIED,&res2);
											pValues2->GetErrorValue(WPD_OBJECT_DATE_CREATED,&res2);
										}
										PropVariantClear(&pv);
										CoTaskMemFree(pItemStorageID2);
									}
									if (pEnumObjectIDs2)
										pEnumObjectIDs2->Release();
									if (pProperties2)
										pProperties2->Release();
								}
*/
							} else
								result=FS_FILE_WRITEERROR;
							CloseHandle(f);
							PropVariantClear(&pv);
							if (pPreviewImage)
								FreeAlbumArt(pPreviewImage);
							pPreviewImage=NULL;
						}
						pValues->Release();
					}
				}
			}
			if (pItemStorageID)
				CoTaskMemFree(pItemStorageID);
			if (pStorageID)
				CoTaskMemFree(pStorageID);
			if (pDeviceContent)
				pDeviceContent->Release();
			if (pEnumObjectIDs)
				pEnumObjectIDs->Release();
			if (pProperties)
				pProperties->Release();
		}
	}
	SetCancelDevice(NULL);
	UnlockPlugin();
	return result;
}

int __stdcall FsExecuteFile(HWND MainWin,char* RemoteName,char* Verb)
{
	WCHAR RemoteNameW[wdirtypemax],VerbW[wdirtypemax];
	return FsExecuteFileW(MainWin,awfilenamecopy(RemoteNameW,RemoteName),awfilenamecopy(VerbW,Verb));
}

int __stdcall FsExecuteFileW(HWND MainWin,WCHAR* RemoteName,WCHAR* Verb)
{
	char RemoteNameA[wdirtypemax];
	if (RemoteName[0]!='\\')
		return FS_EXEC_ERROR;
	
	if (_wcsicmp(Verb,L"open")==0) {
		return FS_EXEC_YOURSELF;
	} else if (_wcsicmp(Verb,L"properties")==0) {
		if (RemoteName[1]==0) {
			ShowPluginAboutDialog(hInst, MainWin);
			return FS_EXEC_OK;
		}
		WCHAR* p=wcschr(RemoteName+1,'\\');
		if (p==NULL || p[1]==0) {
			ShowDevicePropertiesDialog(hInst, MainWin, RemoteName);
			return FS_EXEC_OK;
		}
		return FS_EXEC_YOURSELF;
	} else if (_wcsnicmp(Verb,L"quote ",6)==0) {
		WCHAR* cmd=Verb+6;
		while (*cmd==' ')
			cmd++;
		if (_wcsicmp(cmd,L"refresh")==0 || _wcsicmp(cmd,L"reconnect")==0) {
			AppleMdResetSessions();
			DeviceEventReceived=true;
			InterlockedExchange(&g_cacheDirty,0);
			LogProcT(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,L"REFRESH");
			return FS_EXEC_OK;
		}
		if (_wcsicmp(cmd,L"eject")==0) {
			LockPlugin();
			PWSTR pnp=FindPnpIdByPath(RemoteName);
			IPortableDevice* dev=FindStoredDeviceByPath(RemoteName);
			WCHAR pnpCopy[512]=L"";
			if (pnp)
				wcslcpy(pnpCopy,pnp,512);
			if (dev)
				dev->Close();
			UnlockPlugin();
			BOOL ok=EjectWpdDevice(pnpCopy[0]?pnpCopy:NULL);
			DeviceEventReceived=true;
			if (!ok)
				LogProcT(PluginNumber,MSGTYPE_IMPORTANTERROR,L"Eject failed. Close Explorer/Phone Link and retry.");
			else
				LogProcT(PluginNumber,MSGTYPE_DISCONNECT,L"EJECT");
			return ok?FS_EXEC_OK:FS_EXEC_ERROR;
		}
		if (_wcsicmp(cmd,L"info")==0) {
			ShowDeviceInfoBox(MainWin, RemoteName);
			return FS_EXEC_OK;
		}
		WCHAR help[]=L"Supported: quote refresh | quote eject | quote info | quote reconnect";
		MessageBoxW(MainWin,help,PLUGIN_DISPLAY_NAME_W,MB_ICONINFORMATION);
		return FS_EXEC_OK;
	} else if (_wcsnicmp(Verb,L"chmod ",6)==0) {
		MessageBox(MainWin,wafilenamecopy(RemoteNameA,Verb),"Chmod verb not supported!",MB_ICONEXCLAMATION);
		return FS_EXEC_ERROR;
	} else
		return FS_EXEC_ERROR;
}

void __stdcall FsGetDefRootName(char* DefRootName,int maxlen)
{
	strlcpy(DefRootName,DefPluginTitle,maxlen);
}

void __stdcall FsContentPluginUnloading(void)
{
	ShutdownGdiPlus();
	AppleMdShutdown();
	DisConnectIfNeeded();
	if (weInitializedCOM) {
		CoUninitialize();
		weInitializedCOM=FALSE;
	}
	firstinitialized=FALSE;
}

void __stdcall FsSetDefaultParams(FsDefaultParamStruct* dps)
{
	awlcopy(DefaultIniNameW,dps->DefaultIniName,sizeof(DefaultIniNameW)/2-1);
}