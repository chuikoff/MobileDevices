#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include "apple_md.h"
#include "cunicode.h"
#include "fsplugin.h"

#ifndef kCFStringEncodingUTF8
#define kCFStringEncodingUTF8 0x08000100
#endif
#define ADNCI_MSG_CONNECTED 1
#define ADNCI_MSG_DISCONNECTED 2

typedef void* CFTypeRef;
typedef void* CFStringRef;
typedef void* CFAllocatorRef;
typedef unsigned long CFIndex;
typedef unsigned int CFStringEncoding;
typedef int mach_error_t;
typedef unsigned int afc_error_t;
typedef unsigned long long afc_file_ref;
typedef void* am_device;
typedef void* afc_connection;
typedef void* afc_directory;
typedef void* afc_dictionary;

struct am_device_notification_callback_info {
	am_device dev;
	unsigned int msg;
};

typedef void (*am_device_notification_callback)(am_device_notification_callback_info*, void*);

typedef CFStringRef (*t_CFStringCreateWithCString)(CFAllocatorRef, const char*, CFStringEncoding);
typedef unsigned char (*t_CFStringGetCString)(CFStringRef, char*, CFIndex, CFStringEncoding);
typedef void (*t_CFRelease)(CFTypeRef);
typedef void* CFRunLoopRef;
typedef CFRunLoopRef (*t_CFRunLoopGetCurrent)(void);
typedef void (*t_CFRunLoopRun)(void);
typedef void (*t_CFRunLoopStop)(CFRunLoopRef);
typedef mach_error_t (*t_AMDeviceNotificationSubscribe)(am_device_notification_callback, unsigned, unsigned, void*, void**);
typedef mach_error_t (*t_AMDeviceNotificationUnsubscribe)(void*);
typedef mach_error_t (*t_AMDeviceConnect)(am_device);
typedef mach_error_t (*t_AMDeviceDisconnect)(am_device);
typedef int (*t_AMDeviceIsPaired)(am_device);
typedef mach_error_t (*t_AMDeviceValidatePairing)(am_device);
typedef mach_error_t (*t_AMDeviceStartSession)(am_device);
typedef mach_error_t (*t_AMDeviceStopSession)(am_device);
typedef CFStringRef (*t_AMDeviceCopyValue)(am_device, CFStringRef, CFStringRef);
typedef mach_error_t (*t_AMDeviceStartService)(am_device, CFStringRef, int*, void*);
typedef afc_error_t (*t_AFCConnectionOpen)(int, unsigned, afc_connection*);
typedef afc_error_t (*t_AFCConnectionClose)(afc_connection);
typedef afc_error_t (*t_AFCDirectoryOpen)(afc_connection, const char*, afc_directory*);
typedef afc_error_t (*t_AFCDirectoryRead)(afc_connection, afc_directory, char**);
typedef afc_error_t (*t_AFCDirectoryClose)(afc_connection, afc_directory);
typedef afc_error_t (*t_AFCFileInfoOpen)(afc_connection, const char*, afc_dictionary*);
typedef afc_error_t (*t_AFCKeyValueRead)(afc_dictionary, char**, char**);
typedef afc_error_t (*t_AFCKeyValueClose)(afc_dictionary);
typedef afc_error_t (*t_AFCFileRefOpen)(afc_connection, const char*, unsigned long long, afc_file_ref*);
typedef afc_error_t (*t_AFCFileRefRead)(afc_connection, afc_file_ref, void*, size_t*);
typedef afc_error_t (*t_AFCFileRefClose)(afc_connection, afc_file_ref);

static t_CFStringCreateWithCString pCFStringCreateWithCString;
static t_CFStringGetCString pCFStringGetCString;
static t_CFRelease pCFRelease;
static t_CFRunLoopGetCurrent pCFRunLoopGetCurrent;
static t_CFRunLoopRun pCFRunLoopRun;
static t_CFRunLoopStop pCFRunLoopStop;
static t_AMDeviceNotificationSubscribe pAMDeviceNotificationSubscribe;
static t_AMDeviceNotificationUnsubscribe pAMDeviceNotificationUnsubscribe;
static t_AMDeviceConnect pAMDeviceConnect;
static t_AMDeviceDisconnect pAMDeviceDisconnect;
static t_AMDeviceIsPaired pAMDeviceIsPaired;
static t_AMDeviceValidatePairing pAMDeviceValidatePairing;
static t_AMDeviceStartSession pAMDeviceStartSession;
static t_AMDeviceStopSession pAMDeviceStopSession;
static t_AMDeviceCopyValue pAMDeviceCopyValue;
static t_AMDeviceStartService pAMDeviceStartService;
static t_AFCConnectionOpen pAFCConnectionOpen;
static t_AFCConnectionClose pAFCConnectionClose;
static t_AFCDirectoryOpen pAFCDirectoryOpen;
static t_AFCDirectoryRead pAFCDirectoryRead;
static t_AFCDirectoryClose pAFCDirectoryClose;
static t_AFCFileInfoOpen pAFCFileInfoOpen;
static t_AFCKeyValueRead pAFCKeyValueRead;
static t_AFCKeyValueClose pAFCKeyValueClose;
static t_AFCFileRefOpen pAFCFileRefOpen;
static t_AFCFileRefRead pAFCFileRefRead;
static t_AFCFileRefClose pAFCFileRefClose;

static HMODULE g_cf, g_md;
static void* g_notify;
static CFRunLoopRef g_runLoop;
static HANDLE g_loopThread=NULL;
static HANDLE g_loopReady=NULL;
static volatile LONG g_loopStop=0;
static CRITICAL_SECTION g_appleCs;
static BOOL g_appleCsInit=FALSE;
static BOOL g_loaded=FALSE;

#define APPLE_MAX 8
struct ApplePhone {
	am_device dev;
	afc_connection afc;
	int sock;
	BOOL session;
	WCHAR name[128];
	WCHAR udid[80];
	WCHAR ios[40];
	WCHAR build[40];
	WCHAR product[40];
};
static ApplePhone g_phones[APPLE_MAX];
static int g_nphones=0;

struct AppleFind {
	int magic;
	int phone;
	afc_directory dir;
	char afcPath[1024];
};
#define APPLE_FIND_MAGIC 0x41464C44

static void AppleLock() { if (g_appleCsInit) EnterCriticalSection(&g_appleCs); }
static void AppleUnlock() { if (g_appleCsInit) LeaveCriticalSection(&g_appleCs); }

static CFStringRef CfStr(const char* utf8)
{
	if (!pCFStringCreateWithCString)
		return NULL;
	return pCFStringCreateWithCString(NULL, utf8, kCFStringEncodingUTF8);
}

static void CfToWide(CFStringRef s, WCHAR* out, int cch)
{
	out[0]=0;
	if (!s || !pCFStringGetCString)
		return;
	char utf8[512];
	if (pCFStringGetCString(s, utf8, sizeof(utf8), kCFStringEncodingUTF8))
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, cch);
}

static void CopyValue(am_device dev, const char* key, WCHAR* out, int cch)
{
	out[0]=0;
	if (!pAMDeviceCopyValue)
		return;
	CFStringRef k=CfStr(key);
	if (!k)
		return;
	CFStringRef v=pAMDeviceCopyValue(dev, NULL, k);
	pCFRelease(k);
	if (v) {
		CfToWide(v, out, cch);
		pCFRelease(v);
	}
}

static void WideToUtf8(LPCWSTR w, char* u, int u8cch)
{
	WideCharToMultiByte(CP_UTF8, 0, w, -1, u, u8cch, NULL, NULL);
}

static BOOL PathLooksLikeDir(afc_connection conn, const char* path)
{
	if (!pAFCFileInfoOpen || !pAFCKeyValueRead)
		return FALSE;
	afc_dictionary dict=NULL;
	if (pAFCFileInfoOpen(conn, path, &dict)!=0 || !dict)
		return FALSE;
	BOOL dir=FALSE;
	for (;;) {
		char *k=NULL, *v=NULL;
		if (pAFCKeyValueRead(dict, &k, &v)!=0 || !k)
			break;
		if (v && (!strcmp(k, "st_ifmt") || !strcmp(k, "st_nlink"))) {
			if (v && strstr(v, "DIR"))
				dir=TRUE;
		}
		if (v && !strcmp(k, "st_ifmt") && strstr(v, "S_IFDIR"))
			dir=TRUE;
	}
	if (pAFCKeyValueClose)
		pAFCKeyValueClose(dict);
	return dir;
}

static void FillFindFromAfc(afc_connection conn, const char* dirPath, const char* name, WIN32_FIND_DATAW* fd)
{
	memset(fd, 0, sizeof(*fd));
	MultiByteToWideChar(CP_UTF8, 0, name, -1, fd->cFileName, MAX_PATH);
	fd->dwFileAttributes=FILE_ATTRIBUTE_NORMAL;
	fd->ftLastWriteTime.dwHighDateTime=0xFFFFFFFF;
	fd->ftLastWriteTime.dwLowDateTime=0xFFFFFFFE;
	char full[1024];
	if (dirPath[0] && strcmp(dirPath, "/"))
		sprintf_s(full, "%s/%s", dirPath, name);
	else if (dirPath[0]=='/' && dirPath[1]==0)
		sprintf_s(full, "/%s", name);
	else
		sprintf_s(full, "%s", name);
	if (!pAFCFileInfoOpen)
		return;
	afc_dictionary dict=NULL;
	if (pAFCFileInfoOpen(conn, full, &dict)!=0 || !dict)
		return;
	for (;;) {
		char *k=NULL, *v=NULL;
		if (pAFCKeyValueRead(dict, &k, &v)!=0 || !k)
			break;
		if (!v)
			continue;
		if (!strcmp(k, "st_ifmt") && (strstr(v, "DIR") || strstr(v, "S_IFDIR")))
			fd->dwFileAttributes=FILE_ATTRIBUTE_DIRECTORY;
		if (!strcmp(k, "st_size")) {
			ULONGLONG sz=_strtoui64(v, NULL, 10);
			fd->nFileSizeHigh=(DWORD)(sz>>32);
			fd->nFileSizeLow=(DWORD)sz;
		}
		if (!strcmp(k, "st_mtime")) {
			ULONGLONG unix=_strtoui64(v, NULL, 10);
			if (unix>100000) {
				ULONGLONG ft=(unix+11644473600ULL)*10000000ULL;
				fd->ftLastWriteTime.dwLowDateTime=(DWORD)ft;
				fd->ftLastWriteTime.dwHighDateTime=(DWORD)(ft>>32);
			}
		}
	}
	if (pAFCKeyValueClose)
		pAFCKeyValueClose(dict);
	if (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		fd->nFileSizeHigh=0;
		fd->nFileSizeLow=0;
	}
}

static BOOL EnsureSession(ApplePhone* p)
{
	if (!p || !p->dev)
		return FALSE;
	if (p->session && p->afc)
		return TRUE;
	if (pAMDeviceConnect(p->dev)!=0)
		return FALSE;
	if (!pAMDeviceIsPaired(p->dev)) {
		pAMDeviceDisconnect(p->dev);
		return FALSE;
	}
	if (pAMDeviceValidatePairing(p->dev)!=0) {
		pAMDeviceDisconnect(p->dev);
		return FALSE;
	}
	if (pAMDeviceStartSession(p->dev)!=0) {
		pAMDeviceDisconnect(p->dev);
		return FALSE;
	}
	p->session=TRUE;
	CFStringRef svc=CfStr("com.apple.afc");
	int sock=0;
	if (!svc || pAMDeviceStartService(p->dev, svc, &sock, NULL)!=0) {
		if (svc) pCFRelease(svc);
		return FALSE;
	}
	pCFRelease(svc);
	p->sock=sock;
	afc_connection conn=NULL;
	if (pAFCConnectionOpen(sock, 0, &conn)!=0 || !conn)
		return FALSE;
	p->afc=conn;
	return TRUE;
}

static void ClosePhone(ApplePhone* p)
{
	if (!p)
		return;
	if (p->afc && pAFCConnectionClose) {
		pAFCConnectionClose(p->afc);
		p->afc=NULL;
	}
	if (p->session && pAMDeviceStopSession) {
		pAMDeviceStopSession(p->dev);
		p->session=FALSE;
	}
	if (p->dev && pAMDeviceDisconnect)
		pAMDeviceDisconnect(p->dev);
}

static ApplePhone* FindPhoneByName(LPCWSTR name)
{
	for (int i=0;i<g_nphones;i++) {
		if (_wcsicmp(g_phones[i].name, name)==0)
			return &g_phones[i];
	}
	return NULL;
}

static void OnAppleNotify(am_device_notification_callback_info* info, void*)
{
	if (!info || !info->dev)
		return;
	AppleLock();
	if (info->msg==ADNCI_MSG_CONNECTED) {
		if (g_nphones>=APPLE_MAX) {
			AppleUnlock();
			return;
		}
		for (int i=0;i<g_nphones;i++) {
			if (g_phones[i].dev==info->dev) {
				AppleUnlock();
				return;
			}
		}
		if (pAMDeviceConnect(info->dev)!=0) {
			AppleUnlock();
			return;
		}
		ApplePhone* p=&g_phones[g_nphones];
		memset(p, 0, sizeof(*p));
		p->dev=info->dev;
		CopyValue(info->dev, "DeviceName", p->name, 128);
		CopyValue(info->dev, "UniqueDeviceID", p->udid, 80);
		CopyValue(info->dev, "ProductVersion", p->ios, 40);
		CopyValue(info->dev, "BuildVersion", p->build, 40);
		CopyValue(info->dev, "ProductType", p->product, 40);
		WCHAR dclass[40];
		CopyValue(info->dev, "DeviceClass", dclass, 40);
		BOOL isPhone=FALSE;
		if (!p->product[0] && !dclass[0])
			isPhone=TRUE;
		else if (wcsstr(p->product, L"iPhone") || wcsstr(p->product, L"iPad") || wcsstr(p->product, L"iPod") ||
			wcsstr(dclass, L"iPhone") || wcsstr(dclass, L"iPad") || wcsstr(dclass, L"iPod"))
			isPhone=TRUE;
		if (!isPhone) {
			pAMDeviceDisconnect(info->dev);
			memset(p, 0, sizeof(*p));
			AppleUnlock();
			return;
		}
		if (!p->name[0]) {
			if (dclass[0])
				wcslcpy(p->name, dclass, 128);
			else if (p->product[0])
				wcslcpy(p->name, p->product, 128);
			else
				wcslcpy(p->name, L"iPhone", 128);
		}
		for (int i=0;i<g_nphones;i++) {
			if (!_wcsicmp(g_phones[i].name, p->name)) {
				WCHAR tmp[140];
				swprintf_s(tmp, L"%s (%d)", p->name, i+2);
				wcslcpy(p->name, tmp, 128);
				break;
			}
		}
		pAMDeviceDisconnect(info->dev);
		g_nphones++;
	} else if (info->msg==ADNCI_MSG_DISCONNECTED) {
		for (int i=0;i<g_nphones;i++) {
			if (g_phones[i].dev==info->dev) {
				ClosePhone(&g_phones[i]);
				g_phones[i]=g_phones[g_nphones-1];
				memset(&g_phones[g_nphones-1], 0, sizeof(g_phones[0]));
				g_nphones--;
				break;
			}
		}
	}
	AppleUnlock();
}

static BOOL DirExistsW(LPCWSTR p)
{
	DWORD a=GetFileAttributesW(p);
	return (a!=INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL FileExistsW(LPCWSTR p)
{
	DWORD a=GetFileAttributesW(p);
	return a!=INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static void EnsureDirW(LPCWSTR path)
{
	WCHAR tmp[MAX_PATH];
	wcslcpy(tmp, path, MAX_PATH);
	for (WCHAR* p=tmp+3; *p; p++) {
		if (*p=='\\') {
			*p=0;
			CreateDirectoryW(tmp, NULL);
			*p='\\';
		}
	}
	CreateDirectoryW(tmp, NULL);
}

static BOOL GetProcessDir(LPCWSTR exeName, WCHAR* dir, int cch)
{
	HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap==INVALID_HANDLE_VALUE)
		return FALSE;
	PROCESSENTRY32W pe;
	memset(&pe, 0, sizeof(pe));
	pe.dwSize=sizeof(pe);
	BOOL ok=FALSE;
	if (Process32FirstW(snap, &pe)) {
		do {
			if (_wcsicmp(pe.szExeFile, exeName)!=0)
				continue;
			HANDLE hp=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
			if (!hp)
				continue;
			WCHAR path[MAX_PATH];
			DWORD n=MAX_PATH;
			if (QueryFullProcessImageNameW(hp, 0, path, &n)) {
				WCHAR* slash=wcsrchr(path, '\\');
				if (slash) {
					*slash=0;
					wcslcpy(dir, path, cch);
					ok=TRUE;
				}
			}
			CloseHandle(hp);
			if (ok)
				break;
		} while (Process32NextW(snap, &pe));
	}
	CloseHandle(snap);
	return ok;
}

static BOOL ApplePkgLooksValid(LPCWSTR pkg)
{
#ifdef _WIN64
	WCHAR a[MAX_PATH], b[MAX_PATH];
	swprintf_s(a, L"%s\\MobileDevice.dll", pkg);
	swprintf_s(b, L"%s\\CoreFoundation.dll", pkg);
	return FileExistsW(a) && FileExistsW(b);
#else
	WCHAR a[MAX_PATH], b[MAX_PATH];
	swprintf_s(a, L"%s\\AMDS32\\MobileDevice.dll", pkg);
	swprintf_s(b, L"%s\\VFS\\ProgramFilesCommonX86\\Apple\\Apple Application Support\\CoreFoundation.dll", pkg);
	return FileExistsW(a) && FileExistsW(b);
#endif
}

static BOOL FindApplePackage(WCHAR* pkg, int cch)
{
	if (GetProcessDir(L"AppleMobileDeviceProcess.exe", pkg, cch) && ApplePkgLooksValid(pkg))
		return TRUE;
	WCHAR pf[MAX_PATH], apps[MAX_PATH];
	if (!GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH))
		return FALSE;
	swprintf_s(apps, L"%s\\WindowsApps", pf);
	WCHAR spec[MAX_PATH];
	swprintf_s(spec, L"%s\\AppleInc.AppleDevices_*", apps);
	WIN32_FIND_DATAW fd;
	HANDLE h=FindFirstFileW(spec, &fd);
	if (h==INVALID_HANDLE_VALUE)
		return FALSE;
	BOOL ok=FALSE;
	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			continue;
		if (wcsstr(fd.cFileName, L"_neutral_"))
			continue;
		swprintf_s(pkg, cch, L"%s\\%s", apps, fd.cFileName);
		if (ApplePkgLooksValid(pkg)) {
			ok=TRUE;
			break;
		}
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	return ok;
}

static BOOL CopyOneDll(LPCWSTR srcDir, LPCWSTR name, LPCWSTR dstDir)
{
	WCHAR src[MAX_PATH], dst[MAX_PATH];
	swprintf_s(src, L"%s\\%s", srcDir, name);
	swprintf_s(dst, L"%s\\%s", dstDir, name);
	if (!FileExistsW(src))
		return FALSE;
	WIN32_FILE_ATTRIBUTE_DATA sa, da;
	if (GetFileAttributesExW(dst, GetFileExInfoStandard, &da) &&
		GetFileAttributesExW(src, GetFileExInfoStandard, &sa) &&
		sa.nFileSizeLow==da.nFileSizeLow && sa.nFileSizeHigh==da.nFileSizeHigh &&
		CompareFileTime(&sa.ftLastWriteTime, &da.ftLastWriteTime)==0)
		return TRUE;
	return CopyFileW(src, dst, FALSE)!=0;
}

static BOOL StageAppleDlls(LPCWSTR pkg, WCHAR* dest, int destcch)
{
	WCHAR local[MAX_PATH];
	if (!GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH) || !local[0])
		return FALSE;
#ifdef _WIN64
	swprintf_s(dest, destcch, L"%s\\MTPDevices\\amds64", local);
#else
	swprintf_s(dest, destcch, L"%s\\MTPDevices\\amds32", local);
#endif
	EnsureDirW(dest);
	static const WCHAR* names[]={
		L"ASL.dll", L"CFNetwork.dll", L"CoreFoundation.dll", L"MobileDevice.dll",
		L"objc.dll", L"pthreadVC2.dll", L"libdispatch.dll", L"zlib1.dll", L"zlib.dll",
		L"icudt62.dll", L"libicuin.dll", L"libicuuc.dll", L"icui18n.dll", L"icuuc.dll",
		L"SQLite3.dll", L"ssl-46.dll", L"crypto-44.dll", L"libxml2.dll", L"libxslt.dll",
		L"libtidy.dll", L"Foundation.dll", L"AirTrafficHost.dll", L"dnssd.dll",
		L"mDNSResponderDLL.dll", L"DeviceLink.dll", L"SyncServices.dll"
	};
#ifdef _WIN64
	for (int i=0;i<(int)(sizeof(names)/sizeof(names[0]));i++)
		CopyOneDll(pkg, names[i], dest);
#else
	WCHAR aas[MAX_PATH], amds32[MAX_PATH];
	swprintf_s(aas, L"%s\\VFS\\ProgramFilesCommonX86\\Apple\\Apple Application Support", pkg);
	swprintf_s(amds32, L"%s\\AMDS32", pkg);
	for (int i=0;i<(int)(sizeof(names)/sizeof(names[0]));i++) {
		if (!CopyOneDll(aas, names[i], dest))
			CopyOneDll(amds32, names[i], dest);
	}
#endif
	WCHAR md[MAX_PATH];
	swprintf_s(md, L"%s\\MobileDevice.dll", dest);
	WCHAR cf[MAX_PATH];
	swprintf_s(cf, L"%s\\CoreFoundation.dll", dest);
	return FileExistsW(md) && FileExistsW(cf);
}

static DWORD WINAPI AppleLoopThread(LPVOID)
{
	mach_error_t err=-1;
	if (pCFRunLoopGetCurrent)
		g_runLoop=pCFRunLoopGetCurrent();
	if (pAMDeviceNotificationSubscribe)
		err=pAMDeviceNotificationSubscribe(OnAppleNotify, 0, 0, NULL, &g_notify);
	if (g_loopReady)
		SetEvent(g_loopReady);
	if (err==0 && pCFRunLoopRun && InterlockedCompareExchange(&g_loopStop, 0, 0)==0)
		pCFRunLoopRun();
	return 0;
}

static BOOL BindAppleProcs()
{
	pCFStringCreateWithCString=(t_CFStringCreateWithCString)GetProcAddress(g_cf, "CFStringCreateWithCString");
	pCFStringGetCString=(t_CFStringGetCString)GetProcAddress(g_cf, "CFStringGetCString");
	pCFRelease=(t_CFRelease)GetProcAddress(g_cf, "CFRelease");
	pCFRunLoopGetCurrent=(t_CFRunLoopGetCurrent)GetProcAddress(g_cf, "CFRunLoopGetCurrent");
	pCFRunLoopRun=(t_CFRunLoopRun)GetProcAddress(g_cf, "CFRunLoopRun");
	pCFRunLoopStop=(t_CFRunLoopStop)GetProcAddress(g_cf, "CFRunLoopStop");
	pAMDeviceNotificationSubscribe=(t_AMDeviceNotificationSubscribe)GetProcAddress(g_md, "AMDeviceNotificationSubscribe");
	pAMDeviceNotificationUnsubscribe=(t_AMDeviceNotificationUnsubscribe)GetProcAddress(g_md, "AMDeviceNotificationUnsubscribe");
	pAMDeviceConnect=(t_AMDeviceConnect)GetProcAddress(g_md, "AMDeviceConnect");
	pAMDeviceDisconnect=(t_AMDeviceDisconnect)GetProcAddress(g_md, "AMDeviceDisconnect");
	pAMDeviceIsPaired=(t_AMDeviceIsPaired)GetProcAddress(g_md, "AMDeviceIsPaired");
	pAMDeviceValidatePairing=(t_AMDeviceValidatePairing)GetProcAddress(g_md, "AMDeviceValidatePairing");
	pAMDeviceStartSession=(t_AMDeviceStartSession)GetProcAddress(g_md, "AMDeviceStartSession");
	pAMDeviceStopSession=(t_AMDeviceStopSession)GetProcAddress(g_md, "AMDeviceStopSession");
	pAMDeviceCopyValue=(t_AMDeviceCopyValue)GetProcAddress(g_md, "AMDeviceCopyValue");
	pAMDeviceStartService=(t_AMDeviceStartService)GetProcAddress(g_md, "AMDeviceStartService");
	pAFCConnectionOpen=(t_AFCConnectionOpen)GetProcAddress(g_md, "AFCConnectionOpen");
	pAFCConnectionClose=(t_AFCConnectionClose)GetProcAddress(g_md, "AFCConnectionClose");
	pAFCDirectoryOpen=(t_AFCDirectoryOpen)GetProcAddress(g_md, "AFCDirectoryOpen");
	pAFCDirectoryRead=(t_AFCDirectoryRead)GetProcAddress(g_md, "AFCDirectoryRead");
	pAFCDirectoryClose=(t_AFCDirectoryClose)GetProcAddress(g_md, "AFCDirectoryClose");
	pAFCFileInfoOpen=(t_AFCFileInfoOpen)GetProcAddress(g_md, "AFCFileInfoOpen");
	pAFCKeyValueRead=(t_AFCKeyValueRead)GetProcAddress(g_md, "AFCKeyValueRead");
	pAFCKeyValueClose=(t_AFCKeyValueClose)GetProcAddress(g_md, "AFCKeyValueClose");
	pAFCFileRefOpen=(t_AFCFileRefOpen)GetProcAddress(g_md, "AFCFileRefOpen");
	pAFCFileRefRead=(t_AFCFileRefRead)GetProcAddress(g_md, "AFCFileRefRead");
	pAFCFileRefClose=(t_AFCFileRefClose)GetProcAddress(g_md, "AFCFileRefClose");
	return pAMDeviceNotificationSubscribe && pAMDeviceConnect && pAMDeviceCopyValue &&
		pAFCConnectionOpen && pCFStringCreateWithCString && pCFRunLoopRun;
}

static BOOL LoadAppleDlls()
{
	if (g_loaded)
		return g_md!=NULL;
	g_loaded=TRUE;
	if (!g_appleCsInit) {
		InitializeCriticalSection(&g_appleCs);
		g_appleCsInit=TRUE;
	}

	WCHAR pkg[MAX_PATH], dest[MAX_PATH];
	pkg[0]=0; dest[0]=0;
	if (FindApplePackage(pkg, MAX_PATH))
		StageAppleDlls(pkg, dest, MAX_PATH);

	HMODULE k32=GetModuleHandleW(L"kernel32.dll");
	typedef PVOID (WINAPI *t_AddDllDirectory)(PCWSTR);
	t_AddDllDirectory pAdd=(t_AddDllDirectory)GetProcAddress(k32, "AddDllDirectory");
	if (pAdd && dest[0])
		pAdd(dest);

	WCHAR path[MAX_PATH];
	if (dest[0]) {
		swprintf_s(path, L"%s\\CoreFoundation.dll", dest);
		g_cf=LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
		swprintf_s(path, L"%s\\MobileDevice.dll", dest);
		g_md=LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	}
	if (!g_md) {
		WCHAR pf[MAX_PATH], mds[MAX_PATH];
		GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH);
		swprintf_s(mds, L"%s\\Common Files\\Apple\\Mobile Device Support\\iTunesMobileDevice.dll", pf);
		if (FileExistsW(mds))
			g_md=LoadLibraryExW(mds, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	}
	if (!g_cf || !g_md)
		return FALSE;
	if (!BindAppleProcs())
		return FALSE;

	g_loopReady=CreateEventW(NULL, TRUE, FALSE, NULL);
	g_loopThread=CreateThread(NULL, 0, AppleLoopThread, NULL, 0, NULL);
	if (!g_loopThread)
		return FALSE;
	WaitForSingleObject(g_loopReady, 2000);
	for (int i=0;i<20 && g_nphones==0;i++)
		Sleep(50);
	return TRUE;
}

void AppleMdInit(void)
{
	LoadAppleDlls();
}

void AppleMdShutdown(void)
{
	InterlockedExchange(&g_loopStop, 1);
	if (g_runLoop && pCFRunLoopStop)
		pCFRunLoopStop(g_runLoop);
	if (g_notify && pAMDeviceNotificationUnsubscribe) {
		pAMDeviceNotificationUnsubscribe(g_notify);
		g_notify=NULL;
	}
	if (g_loopThread) {
		WaitForSingleObject(g_loopThread, 2000);
		CloseHandle(g_loopThread);
		g_loopThread=NULL;
	}
	if (g_loopReady) {
		CloseHandle(g_loopReady);
		g_loopReady=NULL;
	}
	AppleLock();
	for (int i=0;i<g_nphones;i++)
		ClosePhone(&g_phones[i]);
	g_nphones=0;
	AppleUnlock();
}

int AppleMdCount(void)
{
	AppleLock();
	int n=g_nphones;
	AppleUnlock();
	return n;
}

BOOL AppleMdGetName(int index, WCHAR* name, int cch)
{
	if (!name)
		return FALSE;
	AppleLock();
	BOOL ok=index>=0 && index<g_nphones;
	if (ok)
		wcslcpy(name, g_phones[index].name, cch);
	AppleUnlock();
	return ok;
}

BOOL AppleMdIsDeviceName(LPCWSTR name)
{
	if (!name)
		return FALSE;
	AppleLock();
	BOOL r=FindPhoneByName(name)!=NULL;
	AppleUnlock();
	return r;
}

BOOL AppleMdFillInfo(LPCWSTR deviceName, PluginDeviceInfo* info)
{
	if (!info)
		return FALSE;
	memset(info, 0, sizeof(*info));
	info->battery=-1;
	AppleLock();
	ApplePhone* p=FindPhoneByName(deviceName);
	if (!p) {
		AppleUnlock();
		return FALSE;
	}
	wcslcpy(info->manufacturer, L"Apple", 128);
	wcslcpy(info->model, p->product[0] ? p->product : p->name, 128);
	if (p->ios[0] && p->build[0])
		swprintf_s(info->firmware, L"%s (%s)", p->ios, p->build);
	else if (p->ios[0])
		wcslcpy(info->firmware, p->ios, 128);
	wcslcpy(info->protocol, L"Apple Mobile Device (AFC)", 80);
	wcslcpy(info->os, L"iOS", 40);
	if (p->ios[0])
		swprintf_s(info->os, L"iOS %s", p->ios);
	AppleUnlock();
	return TRUE;
}

static void RelToAfc(LPCWSTR rel, char* out, int cch)
{
	char utf8[1024];
	WideToUtf8(rel ? rel : L"", utf8, 1024);
	if (!utf8[0]) {
		strcpy_s(out, cch, "DCIM");
		return;
	}
	for (char* q=utf8; *q; q++) {
		if (*q=='\\')
			*q='/';
	}
	if (_strnicmp(utf8, "DCIM", 4)!=0)
		sprintf_s(out, cch, "DCIM/%s", utf8);
	else
		strcpy_s(out, cch, utf8);
}

BOOL AppleMdFindFirst(LPCWSTR deviceName, LPCWSTR relPath, WIN32_FIND_DATAW* fd, HANDLE* out)
{
	*out=INVALID_HANDLE_VALUE;
	AppleLock();
	ApplePhone* p=FindPhoneByName(deviceName);
	if (!p || !EnsureSession(p) || !p->afc) {
		AppleUnlock();
		return FALSE;
	}
	char afcPath[1024];
	RelToAfc(relPath, afcPath, 1024);
	afc_directory dir=NULL;
	if (pAFCDirectoryOpen(p->afc, afcPath, &dir)!=0 || !dir) {
		AppleUnlock();
		return FALSE;
	}
	AppleFind* f=(AppleFind*)calloc(1, sizeof(AppleFind));
	f->magic=APPLE_FIND_MAGIC;
	f->phone=(int)(p-g_phones);
	f->dir=dir;
	strcpy_s(f->afcPath, afcPath);
	*out=(HANDLE)f;
	BOOL ok=AppleMdFindNext(*out, fd);
	if (!ok) {
		AppleMdFindClose(*out);
		*out=INVALID_HANDLE_VALUE;
	}
	AppleUnlock();
	return ok;
}

BOOL AppleMdFindNext(HANDLE h, WIN32_FIND_DATAW* fd)
{
	AppleFind* f=(AppleFind*)h;
	if (!f || f->magic!=APPLE_FIND_MAGIC)
		return FALSE;
	AppleLock();
	if (f->phone<0 || f->phone>=g_nphones || !g_phones[f->phone].afc) {
		AppleUnlock();
		return FALSE;
	}
	afc_connection conn=g_phones[f->phone].afc;
	for (;;) {
		char* name=NULL;
		if (pAFCDirectoryRead(conn, f->dir, &name)!=0 || !name || !name[0]) {
			AppleUnlock();
			return FALSE;
		}
		if (!strcmp(name, ".") || !strcmp(name, ".."))
			continue;
		FillFindFromAfc(conn, f->afcPath, name, fd);
		AppleUnlock();
		return TRUE;
	}
}

void AppleMdFindClose(HANDLE h)
{
	AppleFind* f=(AppleFind*)h;
	if (!f || f->magic!=APPLE_FIND_MAGIC)
		return;
	AppleLock();
	if (f->dir && f->phone>=0 && f->phone<g_nphones && g_phones[f->phone].afc && pAFCDirectoryClose)
		pAFCDirectoryClose(g_phones[f->phone].afc, f->dir);
	f->magic=0;
	free(f);
	AppleUnlock();
}

int AppleMdGetFile(LPCWSTR deviceName, LPCWSTR relPath, LPCWSTR localPath, ULONGLONG, FILETIME* mtime)
{
	AppleLock();
	ApplePhone* p=FindPhoneByName(deviceName);
	if (!p || !EnsureSession(p) || !p->afc) {
		AppleUnlock();
		return FS_FILE_READERROR;
	}
	char afcPath[1024];
	RelToAfc(relPath, afcPath, 1024);
	afc_file_ref ref=0;
	if (pAFCFileRefOpen(p->afc, afcPath, 1, &ref)!=0) {
		AppleUnlock();
		return FS_FILE_NOTFOUND;
	}
	HANDLE out=CreateFileT((WCHAR*)localPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
	if (out==INVALID_HANDLE_VALUE) {
		pAFCFileRefClose(p->afc, ref);
		AppleUnlock();
		return FS_FILE_WRITEERROR;
	}
	char buf[64*1024];
	int result=FS_FILE_OK;
	for (;;) {
		size_t n=sizeof(buf);
		if (pAFCFileRefRead(p->afc, ref, buf, &n)!=0) {
			result=FS_FILE_READERROR;
			break;
		}
		if (n==0)
			break;
		DWORD wr=0;
		if (!WriteFile(out, buf, (DWORD)n, &wr, NULL)) {
			result=FS_FILE_WRITEERROR;
			break;
		}
	}
	pAFCFileRefClose(p->afc, ref);
	if (mtime && result==FS_FILE_OK &&
		!(mtime->dwHighDateTime==0xFFFFFFFF))
		SetFileTime(out, NULL, NULL, mtime);
	CloseHandle(out);
	AppleUnlock();
	if (result!=FS_FILE_OK)
		DeleteFileT((WCHAR*)localPath);
	return result;
}
