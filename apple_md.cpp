#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include "apple_md.h"
#include "cunicode.h"
#include "fsplugin.h"

#pragma comment(lib, "ws2_32.lib")

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

typedef void* CFDataRef;
typedef void* CFDictionaryRef;
typedef void* CFArrayRef;
typedef void* CFBooleanRef;
typedef unsigned long CFTypeID;
typedef unsigned long CFOptionFlags;

typedef CFStringRef (*t_CFStringCreateWithCString)(CFAllocatorRef, const char*, CFStringEncoding);
typedef unsigned char (*t_CFStringGetCString)(CFStringRef, char*, CFIndex, CFStringEncoding);
typedef void (*t_CFRelease)(CFTypeRef);
typedef CFTypeID (*t_CFGetTypeID)(CFTypeRef);
typedef CFTypeID (*t_CFStringGetTypeID)(void);
typedef CFTypeID (*t_CFArrayGetTypeID)(void);
typedef CFTypeID (*t_CFDictionaryGetTypeID)(void);
typedef CFTypeID (*t_CFBooleanGetTypeID)(void);
typedef CFIndex (*t_CFArrayGetCount)(CFArrayRef);
typedef CFTypeRef (*t_CFArrayGetValueAtIndex)(CFArrayRef, CFIndex);
typedef CFTypeRef (*t_CFDictionaryGetValue)(CFDictionaryRef, CFTypeRef);
typedef unsigned char (*t_CFBooleanGetValue)(CFBooleanRef);
typedef CFDataRef (*t_CFDataCreate)(CFAllocatorRef, const unsigned char*, CFIndex);
typedef const unsigned char* (*t_CFDataGetBytePtr)(CFDataRef);
typedef CFIndex (*t_CFDataGetLength)(CFDataRef);
typedef CFTypeRef (*t_CFPropertyListCreateWithData)(CFAllocatorRef, CFDataRef, CFOptionFlags, unsigned long*, void**);
typedef CFDataRef (*t_CFPropertyListCreateData)(CFAllocatorRef, CFTypeRef, unsigned long, unsigned long, void**);
typedef void* CFMutableDictionaryRef;
typedef CFMutableDictionaryRef (*t_CFDictionaryCreateMutable)(CFAllocatorRef, CFIndex, const void*, const void*);
typedef void (*t_CFDictionarySetValue)(CFMutableDictionaryRef, const void*, const void*);
typedef CFTypeID (*t_CFNumberGetTypeID)(void);
typedef unsigned char (*t_CFNumberGetValue)(void*, int, void*);
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
typedef mach_error_t (*t_AMDeviceSecureStartService)(am_device, CFStringRef, void*, void**);
typedef mach_error_t (*t_AMDeviceStartHouseArrestService)(am_device, CFStringRef, void*, int*, unsigned int*);
typedef int (*t_AMDServiceConnectionGetSocket)(void*);
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
typedef afc_error_t (*t_AFCFileRefWrite)(afc_connection, afc_file_ref, const void*, size_t*);
typedef afc_error_t (*t_AFCFileRefClose)(afc_connection, afc_file_ref);
typedef afc_error_t (*t_AFCRemovePath)(afc_connection, const char*);
typedef afc_error_t (*t_AFCDirectoryCreate)(afc_connection, const char*);
typedef afc_error_t (*t_AFCDeviceInfoOpen)(afc_connection, afc_dictionary*);

static t_CFStringCreateWithCString pCFStringCreateWithCString;
static t_CFStringGetCString pCFStringGetCString;
static t_CFRelease pCFRelease;
static t_CFGetTypeID pCFGetTypeID;
static t_CFStringGetTypeID pCFStringGetTypeID;
static t_CFArrayGetTypeID pCFArrayGetTypeID;
static t_CFDictionaryGetTypeID pCFDictionaryGetTypeID;
static t_CFBooleanGetTypeID pCFBooleanGetTypeID;
static t_CFArrayGetCount pCFArrayGetCount;
static t_CFArrayGetValueAtIndex pCFArrayGetValueAtIndex;
static t_CFDictionaryGetValue pCFDictionaryGetValue;
static t_CFBooleanGetValue pCFBooleanGetValue;
static t_CFDataCreate pCFDataCreate;
static t_CFDataGetBytePtr pCFDataGetBytePtr;
static t_CFDataGetLength pCFDataGetLength;
static t_CFPropertyListCreateWithData pCFPropertyListCreateWithData;
static t_CFPropertyListCreateData pCFPropertyListCreateData;
static t_CFDictionaryCreateMutable pCFDictionaryCreateMutable;
static t_CFDictionarySetValue pCFDictionarySetValue;
static t_CFNumberGetTypeID pCFNumberGetTypeID;
static t_CFNumberGetValue pCFNumberGetValue;
static const void* g_cfKeyCb;
static const void* g_cfValCb;
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
static t_AMDeviceSecureStartService pAMDeviceSecureStartService;
static t_AMDeviceStartHouseArrestService pAMDeviceStartHouseArrestService;
static t_AMDServiceConnectionGetSocket pAMDServiceConnectionGetSocket;
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
static t_AFCFileRefWrite pAFCFileRefWrite;
static t_AFCFileRefClose pAFCFileRefClose;
static t_AFCRemovePath pAFCRemovePath;
static t_AFCDirectoryCreate pAFCDirectoryCreate;
static t_AFCDeviceInfoOpen pAFCDeviceInfoOpen;

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
#define APPLE_MAX_APPS 256
#define APPLE_PHOTOS L"Photos"
#define APPLE_APPS L"Applications"
#define APPLE_PANICS L"Panic Logs"
#define AFK_ROOT 1
#define AFK_APPS 2
#define AFK_AFC 3
enum {
	AR_ROOT=0,
	AR_PHOTOS,
	AR_PHOTOS_REL,
	AR_APPS,
	AR_APP,
	AR_APP_REL,
	AR_PANICS,
	AR_PANICS_REL
};
struct AppleApp {
	WCHAR name[128];
	char bundle[160];
	BOOL sharing;
	BOOL inplace;
};
struct ApplePhone {
	am_device dev;
	afc_connection afc;
	int sock;
	BOOL session;
	afc_connection appAfc;
	int appSock;
	char appBundle[160];
	char appRoot[40];
	afc_connection panicAfc;
	int panicSock;
	WCHAR name[128];
	WCHAR udid[80];
	WCHAR ios[40];
	WCHAR build[40];
	WCHAR product[40];
	int napps;
	AppleApp apps[APPLE_MAX_APPS];
};
static ApplePhone g_phones[APPLE_MAX];
static int g_nphones=0;
static BOOL g_wsa=FALSE;

struct AppleFind {
	int magic;
	int phone;
	int kind;
	int index;
	afc_directory dir;
	afc_connection conn;
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

static CFTypeRef CopyRaw(am_device dev, const char* domain, const char* key)
{
	if (!pAMDeviceCopyValue || !key)
		return NULL;
	CFStringRef d=domain ? CfStr(domain) : NULL;
	CFStringRef k=CfStr(key);
	if (!k) {
		if (d) pCFRelease(d);
		return NULL;
	}
	CFTypeRef v=pAMDeviceCopyValue(dev, d, k);
	pCFRelease(k);
	if (d) pCFRelease(d);
	return v;
}

static BOOL CfAsI64(CFTypeRef v, long long* out)
{
	if (!v || !out || !pCFGetTypeID)
		return FALSE;
	CFTypeID t=pCFGetTypeID(v);
	if (pCFNumberGetTypeID && pCFNumberGetValue && t==pCFNumberGetTypeID()) {
		long long n=0;
		int i=0;
		double d=0;
		if (pCFNumberGetValue(v, 11, &n)) { *out=n; return TRUE; }
		if (pCFNumberGetValue(v, 4, &n)) { *out=n; return TRUE; }
		if (pCFNumberGetValue(v, 9, &i)) { *out=i; return TRUE; }
		if (pCFNumberGetValue(v, 6, &d)) { *out=(long long)(d>0 && d<=1.01 ? d*100.0+0.5 : d); return TRUE; }
		return FALSE;
	}
	if (pCFStringGetTypeID && t==pCFStringGetTypeID()) {
		WCHAR w[64];
		CfToWide((CFStringRef)v, w, 64);
		if (!w[0])
			return FALSE;
		*out=_wtoi64(w);
		return TRUE;
	}
	return FALSE;
}

static void CopyValue(am_device dev, const char* key, WCHAR* out, int cch)
{
	out[0]=0;
	CFTypeRef v=CopyRaw(dev, NULL, key);
	if (!v)
		return;
	if (pCFGetTypeID && pCFStringGetTypeID && pCFGetTypeID(v)==pCFStringGetTypeID())
		CfToWide((CFStringRef)v, out, cch);
	else {
		long long n=0;
		if (CfAsI64(v, &n))
			swprintf_s(out, cch, L"%lld", n);
	}
	pCFRelease(v);
}

static int CopyValueInt(am_device dev, const char* domain, const char* key)
{
	CFTypeRef v=CopyRaw(dev, domain, key);
	if (!v)
		return -1;
	long long n=0;
	BOOL ok=CfAsI64(v, &n);
	pCFRelease(v);
	if (!ok)
		return -1;
	if (n<0)
		return -1;
	if (n<=100)
		return (int)n;
	if (n<=1000)
		return (int)n;
	return (int)n;
}

static BOOL CopyValueU64(am_device dev, const char* domain, const char* key, ULONGLONG* out)
{
	CFTypeRef v=CopyRaw(dev, domain, key);
	if (!v)
		return FALSE;
	long long n=0;
	BOOL ok=CfAsI64(v, &n) && n>=0;
	pCFRelease(v);
	if (!ok)
		return FALSE;
	*out=(ULONGLONG)n;
	return TRUE;
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
	if (dirPath && dirPath[0] && strcmp(dirPath, "/") && strcmp(dirPath, "."))
		sprintf_s(full, "%s/%s", dirPath, name);
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

static void CloseAppAfc(ApplePhone* p)
{
	if (!p)
		return;
	if (p->appAfc && pAFCConnectionClose) {
		__try { pAFCConnectionClose(p->appAfc); } __except(EXCEPTION_EXECUTE_HANDLER) {}
		p->appAfc=NULL;
	}
	p->appSock=0;
	p->appBundle[0]=0;
	p->appRoot[0]=0;
}

static void ClosePanicAfc(ApplePhone* p)
{
	if (!p)
		return;
	if (p->panicAfc && pAFCConnectionClose) {
		__try { pAFCConnectionClose(p->panicAfc); } __except(EXCEPTION_EXECUTE_HANDLER) {}
		p->panicAfc=NULL;
	}
	p->panicSock=0;
}

static BOOL EnsureLockdown(ApplePhone* p)
{
	if (!p || !p->dev)
		return FALSE;
	if (p->session)
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
	return TRUE;
}

static BOOL StartNamedService(ApplePhone* p, const char* name, int* sock)
{
	*sock=0;
	if (!EnsureLockdown(p))
		return FALSE;
	CFStringRef svc=CfStr(name);
	if (!svc)
		return FALSE;
	int s=0;
	if (pAMDeviceStartService && pAMDeviceStartService(p->dev, svc, &s, NULL)==0 && s) {
		pCFRelease(svc);
		*sock=s;
		return TRUE;
	}
	if (pAMDeviceSecureStartService && pAMDServiceConnectionGetSocket) {
		void* conn=NULL;
		if (pAMDeviceSecureStartService(p->dev, svc, NULL, &conn)==0 && conn) {
			s=pAMDServiceConnectionGetSocket(conn);
			if (s) {
				pCFRelease(svc);
				*sock=s;
				return TRUE;
			}
		}
	}
	pCFRelease(svc);
	return FALSE;
}

static BOOL EnsureSession(ApplePhone* p)
{
	if (!p || !p->dev)
		return FALSE;
	if (p->session && p->afc)
		return TRUE;
	if (!EnsureLockdown(p))
		return FALSE;
	if (p->afc)
		return TRUE;
	int sock=0;
	if (!StartNamedService(p, "com.apple.afc", &sock))
		return FALSE;
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
	CloseAppAfc(p);
	ClosePanicAfc(p);
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
	p->napps=0;
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
	pAMDeviceSecureStartService=(t_AMDeviceSecureStartService)GetProcAddress(g_md, "AMDeviceSecureStartService");
	pAMDeviceStartHouseArrestService=(t_AMDeviceStartHouseArrestService)GetProcAddress(g_md, "AMDeviceStartHouseArrestService");
	pAMDServiceConnectionGetSocket=(t_AMDServiceConnectionGetSocket)GetProcAddress(g_md, "AMDServiceConnectionGetSocket");
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
	pAFCFileRefWrite=(t_AFCFileRefWrite)GetProcAddress(g_md, "AFCFileRefWrite");
	pAFCFileRefClose=(t_AFCFileRefClose)GetProcAddress(g_md, "AFCFileRefClose");
	pAFCRemovePath=(t_AFCRemovePath)GetProcAddress(g_md, "AFCRemovePath");
	pAFCDirectoryCreate=(t_AFCDirectoryCreate)GetProcAddress(g_md, "AFCDirectoryCreate");
	pAFCDeviceInfoOpen=(t_AFCDeviceInfoOpen)GetProcAddress(g_md, "AFCDeviceInfoOpen");
	pCFGetTypeID=(t_CFGetTypeID)GetProcAddress(g_cf, "CFGetTypeID");
	pCFStringGetTypeID=(t_CFStringGetTypeID)GetProcAddress(g_cf, "CFStringGetTypeID");
	pCFArrayGetTypeID=(t_CFArrayGetTypeID)GetProcAddress(g_cf, "CFArrayGetTypeID");
	pCFDictionaryGetTypeID=(t_CFDictionaryGetTypeID)GetProcAddress(g_cf, "CFDictionaryGetTypeID");
	pCFBooleanGetTypeID=(t_CFBooleanGetTypeID)GetProcAddress(g_cf, "CFBooleanGetTypeID");
	pCFArrayGetCount=(t_CFArrayGetCount)GetProcAddress(g_cf, "CFArrayGetCount");
	pCFArrayGetValueAtIndex=(t_CFArrayGetValueAtIndex)GetProcAddress(g_cf, "CFArrayGetValueAtIndex");
	pCFDictionaryGetValue=(t_CFDictionaryGetValue)GetProcAddress(g_cf, "CFDictionaryGetValue");
	pCFBooleanGetValue=(t_CFBooleanGetValue)GetProcAddress(g_cf, "CFBooleanGetValue");
	pCFDataCreate=(t_CFDataCreate)GetProcAddress(g_cf, "CFDataCreate");
	pCFDataGetBytePtr=(t_CFDataGetBytePtr)GetProcAddress(g_cf, "CFDataGetBytePtr");
	pCFDataGetLength=(t_CFDataGetLength)GetProcAddress(g_cf, "CFDataGetLength");
	pCFPropertyListCreateWithData=(t_CFPropertyListCreateWithData)GetProcAddress(g_cf, "CFPropertyListCreateWithData");
	pCFPropertyListCreateData=(t_CFPropertyListCreateData)GetProcAddress(g_cf, "CFPropertyListCreateData");
	pCFDictionaryCreateMutable=(t_CFDictionaryCreateMutable)GetProcAddress(g_cf, "CFDictionaryCreateMutable");
	pCFDictionarySetValue=(t_CFDictionarySetValue)GetProcAddress(g_cf, "CFDictionarySetValue");
	pCFNumberGetTypeID=(t_CFNumberGetTypeID)GetProcAddress(g_cf, "CFNumberGetTypeID");
	pCFNumberGetValue=(t_CFNumberGetValue)GetProcAddress(g_cf, "CFNumberGetValue");
	g_cfKeyCb=GetProcAddress(g_cf, "kCFTypeDictionaryKeyCallBacks");
	g_cfValCb=GetProcAddress(g_cf, "kCFTypeDictionaryValueCallBacks");
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
	if (!g_wsa) {
		WSADATA wd;
		if (WSAStartup(MAKEWORD(2, 2), &wd)==0)
			g_wsa=TRUE;
	}

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

	EnsureLockdown(p);
	int bat=CopyValueInt(p->dev, NULL, "BatteryCurrentCapacity");
	if (bat<0 || bat>100)
		bat=CopyValueInt(p->dev, "com.apple.mobile.battery", "BatteryCurrentCapacity");
	if (bat>=0 && bat<=100)
		info->battery=bat;

	ULONGLONG cap=0, freeb=0;
	if (EnsureSession(p) && p->afc && pAFCDeviceInfoOpen && pAFCKeyValueRead) {
		afc_dictionary dict=NULL;
		if (pAFCDeviceInfoOpen(p->afc, &dict)==0 && dict) {
			for (;;) {
				char *k=NULL, *v=NULL;
				if (pAFCKeyValueRead(dict, &k, &v)!=0 || !k)
					break;
				if (!v)
					continue;
				if (!strcmp(k, "FSTotalBytes"))
					cap=_strtoui64(v, NULL, 10);
				else if (!strcmp(k, "FSFreeBytes"))
					freeb=_strtoui64(v, NULL, 10);
			}
			if (pAFCKeyValueClose)
				pAFCKeyValueClose(dict);
		}
	}
	if (!cap && !freeb) {
		CopyValueU64(p->dev, "com.apple.disk_usage", "TotalDiskCapacity", &cap);
		if (!CopyValueU64(p->dev, "com.apple.disk_usage", "TotalDataAvailable", &freeb))
			CopyValueU64(p->dev, "com.apple.disk_usage", "AmountDataAvailable", &freeb);
	}
	if (cap || freeb) {
		int n=info->nstor;
		wcslcpy(info->stor[n].name, L"iPhone", 80);
		info->stor[n].capacityBytes=cap;
		info->stor[n].freeBytes=freeb;
		info->nstor++;
	}
	AppleUnlock();
	return TRUE;
}

static void ToUtf8Slash(LPCWSTR w, char* u, int cch)
{
	WideToUtf8(w ? w : L"", u, cch);
	for (char* q=u; *q; q++) {
		if (*q=='\\')
			*q='/';
	}
}

static void StripSlash(WCHAR* s)
{
	size_t n=wcslen(s);
	while (n>0 && s[n-1]=='\\') {
		s[n-1]=0;
		n--;
	}
}

static int ParseAppleRel(LPCWSTR rel, WCHAR* appOut, int appcch, char* afcOut, int afccch)
{
	if (appOut && appcch>0)
		appOut[0]=0;
	if (afcOut && afccch>0)
		afcOut[0]=0;
	WCHAR buf[wdirtypemax];
	wcslcpy(buf, rel ? rel : L"", wdirtypemax);
	StripSlash(buf);
	WCHAR* p=buf;
	if (p[0]=='\\')
		p++;
	if (!p[0])
		return AR_ROOT;

	WCHAR* slash=wcschr(p, '\\');
	WCHAR first[128];
	if (slash) {
		*slash=0;
		wcslcpy(first, p, 128);
	} else
		wcslcpy(first, p, 128);

	if (_wcsicmp(first, APPLE_PHOTOS)==0) {
		if (!slash || !slash[1])
			return AR_PHOTOS;
		ToUtf8Slash(slash+1, afcOut, afccch);
		char tmp[1024];
		strcpy_s(tmp, afcOut);
		sprintf_s(afcOut, afccch, "DCIM/%s", tmp);
		return AR_PHOTOS_REL;
	}
	if (_wcsicmp(first, L"DCIM")==0) {
		if (!slash || !slash[1]) {
			strcpy_s(afcOut, afccch, "DCIM");
			return AR_PHOTOS;
		}
		ToUtf8Slash(rel, afcOut, afccch);
		return AR_PHOTOS_REL;
	}
	if (_wcsicmp(first, APPLE_APPS)==0) {
		if (!slash || !slash[1])
			return AR_APPS;
		WCHAR* s2=wcschr(slash+1, '\\');
		if (!s2) {
			if (appOut)
				wcslcpy(appOut, slash+1, appcch);
			if (afcOut)
				afcOut[0]=0;
			return AR_APP;
		}
		*s2=0;
		if (appOut)
			wcslcpy(appOut, slash+1, appcch);
		ToUtf8Slash(s2+1, afcOut, afccch);
		return AR_APP_REL;
	}
	if (_wcsicmp(first, APPLE_PANICS)==0) {
		if (!slash || !slash[1])
			return AR_PANICS;
		ToUtf8Slash(slash+1, afcOut, afccch);
		return AR_PANICS_REL;
	}
	ToUtf8Slash(p, afcOut, afccch);
	char tmp[1024];
	strcpy_s(tmp, afcOut);
	sprintf_s(afcOut, afccch, "DCIM/%s", tmp);
	return AR_PHOTOS_REL;
}

static void FillDirFd(LPCWSTR name, WIN32_FIND_DATAW* fd)
{
	memset(fd, 0, sizeof(*fd));
	wcslcpy(fd->cFileName, name, MAX_PATH);
	fd->dwFileAttributes=FILE_ATTRIBUTE_DIRECTORY;
	fd->ftLastWriteTime.dwHighDateTime=0xFFFFFFFF;
	fd->ftLastWriteTime.dwLowDateTime=0xFFFFFFFE;
}

static void SanitizeName(WCHAR* s)
{
	for (; *s; s++) {
		if (*s=='\\' || *s=='/' || *s==':' || *s=='*' || *s=='?' ||
			*s=='"' || *s=='<' || *s=='>' || *s=='|')
			*s='_';
	}
}

static BOOL SockSendAll(int sock, const void* data, int n)
{
	const char* p=(const char*)data;
	while (n>0) {
		int s=send(sock, p, n, 0);
		if (s<=0)
			return FALSE;
		p+=s;
		n-=s;
	}
	return TRUE;
}

static BOOL SockRecvAll(int sock, void* data, int n)
{
	char* p=(char*)data;
	DWORD t=8000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&t, sizeof(t));
	while (n>0) {
		int r=recv(sock, p, n, 0);
		if (r<=0)
			return FALSE;
		p+=r;
		n-=r;
	}
	return TRUE;
}

static BOOL PlistSendXml(int sock, const char* xml)
{
	int n=(int)strlen(xml);
	unsigned int be=htonl((unsigned int)n);
	if (!SockSendAll(sock, &be, 4))
		return FALSE;
	return SockSendAll(sock, xml, n);
}

static CFTypeRef PlistRecv(int sock)
{
	unsigned int be=0;
	if (!SockRecvAll(sock, &be, 4))
		return NULL;
	unsigned int n=ntohl(be);
	if (n==0 || n>8*1024*1024)
		return NULL;
	char* buf=(char*)malloc(n+1);
	if (!buf)
		return NULL;
	if (!SockRecvAll(sock, buf, (int)n)) {
		free(buf);
		return NULL;
	}
	buf[n]=0;
	CFTypeRef pl=NULL;
	if (pCFDataCreate && pCFPropertyListCreateWithData) {
		CFDataRef d=pCFDataCreate(NULL, (const unsigned char*)buf, (CFIndex)n);
		if (d) {
			pl=pCFPropertyListCreateWithData(NULL, d, 0, NULL, NULL);
			pCFRelease(d);
		}
	}
	free(buf);
	return pl;
}

static CFTypeRef DictGet(CFTypeRef d, const char* key)
{
	if (!d || !pCFDictionaryGetValue || !pCFGetTypeID || !pCFDictionaryGetTypeID)
		return NULL;
	if (pCFGetTypeID(d)!=pCFDictionaryGetTypeID())
		return NULL;
	CFStringRef k=CfStr(key);
	if (!k)
		return NULL;
	CFTypeRef v=pCFDictionaryGetValue((CFDictionaryRef)d, k);
	pCFRelease(k);
	return v;
}

static BOOL DictStr(CFTypeRef d, const char* key, WCHAR* out, int cch)
{
	out[0]=0;
	CFTypeRef v=DictGet(d, key);
	if (!v || !pCFGetTypeID || !pCFStringGetTypeID)
		return FALSE;
	if (pCFGetTypeID(v)!=pCFStringGetTypeID())
		return FALSE;
	CfToWide((CFStringRef)v, out, cch);
	return out[0]!=0;
}

static BOOL DictTruthy(CFTypeRef d, const char* key)
{
	CFTypeRef v=DictGet(d, key);
	if (!v || !pCFGetTypeID)
		return FALSE;
	CFTypeID t=pCFGetTypeID(v);
	if (pCFBooleanGetTypeID && t==pCFBooleanGetTypeID())
		return pCFBooleanGetValue && pCFBooleanGetValue((CFBooleanRef)v)!=0;
	if (pCFNumberGetTypeID && pCFNumberGetValue && t==pCFNumberGetTypeID()) {
		int n=0;
		if (pCFNumberGetValue(v, 9, &n))
			return n!=0;
		if (pCFNumberGetValue(v, 3, &n))
			return n!=0;
		return FALSE;
	}
	if (pCFStringGetTypeID && t==pCFStringGetTypeID()) {
		WCHAR s[16];
		CfToWide((CFStringRef)v, s, 16);
		return s[0]==L'1' || !_wcsicmp(s, L"true") || !_wcsicmp(s, L"YES");
	}
	return TRUE;
}

static BOOL PlistSendBytes(int sock, const void* bytes, int n)
{
	unsigned int be=htonl((unsigned int)n);
	if (!SockSendAll(sock, &be, 4))
		return FALSE;
	return SockSendAll(sock, bytes, n);
}

static BOOL PlistSendCF(int sock, CFTypeRef plist, BOOL binary)
{
	if (!plist || !pCFPropertyListCreateData || !pCFDataGetBytePtr || !pCFDataGetLength)
		return FALSE;
	CFDataRef data=pCFPropertyListCreateData(NULL, plist, binary ? 200 : 100, 0, NULL);
	if (!data)
		return FALSE;
	int n=(int)pCFDataGetLength(data);
	const unsigned char* b=pCFDataGetBytePtr(data);
	BOOL ok=n>0 && b && PlistSendBytes(sock, b, n);
	pCFRelease(data);
	return ok;
}

static CFMutableDictionaryRef DictNew(void)
{
	if (!pCFDictionaryCreateMutable || !g_cfKeyCb || !g_cfValCb)
		return NULL;
	return pCFDictionaryCreateMutable(NULL, 0, g_cfKeyCb, g_cfValCb);
}

static void DictSetCStr(CFMutableDictionaryRef d, const char* k, const char* v)
{
	if (!d || !pCFDictionarySetValue)
		return;
	CFStringRef ck=CfStr(k);
	CFStringRef cv=CfStr(v);
	if (ck && cv)
		pCFDictionarySetValue(d, ck, cv);
	if (ck) pCFRelease(ck);
	if (cv) pCFRelease(cv);
}

static void AddAppFromDict(ApplePhone* p, CFTypeRef app)
{
	if (!p || !app || p->napps>=APPLE_MAX_APPS)
		return;
	WCHAR id[160], name[128];
	if (!DictStr(app, "CFBundleIdentifier", id, 160) || !id[0])
		return;
	if (!DictStr(app, "CFBundleDisplayName", name, 128) || !name[0])
		DictStr(app, "CFBundleName", name, 128);
	if (!name[0])
		wcslcpy(name, id, 128);
	SanitizeName(name);
	char bundle[160];
	WideToUtf8(id, bundle, 160);
	for (int i=0;i<p->napps;i++) {
		if (_stricmp(p->apps[i].bundle, bundle)==0)
			return;
	}
	WCHAR unique[128];
	wcslcpy(unique, name, 128);
	int extra=2;
	for (;;) {
		BOOL clash=FALSE;
		for (int i=0;i<p->napps;i++) {
			if (_wcsicmp(p->apps[i].name, unique)==0) {
				clash=TRUE;
				break;
			}
		}
		if (!clash)
			break;
		swprintf_s(unique, L"%s (%d)", name, extra++);
	}
	AppleApp* a=&p->apps[p->napps++];
	memset(a, 0, sizeof(*a));
	wcslcpy(a->name, unique, 128);
	strcpy_s(a->bundle, bundle);
	a->sharing=DictTruthy(app, "UIFileSharingEnabled");
	a->inplace=DictTruthy(app, "LSSupportsOpeningDocumentsInPlace");
}

static int CmpApps(const void* a, const void* b)
{
	const AppleApp* x=(const AppleApp*)a;
	const AppleApp* y=(const AppleApp*)b;
	return _wcsicmp(x->name, y->name);
}

static BOOL HouseArrest(ApplePhone* p, const char* bundle, const char* command, int* sock);
static BOOL AppDocumentsOpen(ApplePhone* p, const char* bundle, afc_connection* outConn, int* outSock);

static BOOL RefreshApps(ApplePhone* p)
{
	p->napps=0;
	if (!pCFPropertyListCreateWithData || !pCFArrayGetCount)
		return FALSE;
	int sock=0;
	if (!StartNamedService(p, "com.apple.mobile.installation_proxy", &sock))
		return FALSE;
	const char* xml=
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
		"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
		"<plist version=\"1.0\"><dict>"
		"<key>Command</key><string>Browse</string>"
		"<key>ClientOptions</key><dict>"
		"<key>ApplicationType</key><string>User</string>"
		"<key>ReturnAttributes</key><array>"
		"<string>CFBundleIdentifier</string>"
		"<string>CFBundleDisplayName</string>"
		"<string>CFBundleName</string>"
		"<string>UIFileSharingEnabled</string>"
		"<string>LSSupportsOpeningDocumentsInPlace</string>"
		"</array></dict></dict></plist>";
	BOOL ok=PlistSendXml(sock, xml);
	if (ok) {
		for (int n=0;n<64;n++) {
			CFTypeRef pl=PlistRecv(sock);
			if (!pl)
				break;
			WCHAR status[64];
			DictStr(pl, "Status", status, 64);
			CFTypeRef list=DictGet(pl, "CurrentList");
			if (list && pCFGetTypeID && pCFArrayGetTypeID &&
				pCFGetTypeID(list)==pCFArrayGetTypeID()) {
				CFIndex cnt=pCFArrayGetCount((CFArrayRef)list);
				for (CFIndex i=0;i<cnt;i++)
					AddAppFromDict(p, pCFArrayGetValueAtIndex((CFArrayRef)list, i));
			}
			BOOL done=_wcsicmp(status, L"Complete")==0;
			pCFRelease(pl);
			if (done)
				break;
		}
	}
	closesocket(sock);

	AppleApp raw[APPLE_MAX_APPS];
	int nraw=p->napps;
	if (nraw>APPLE_MAX_APPS)
		nraw=APPLE_MAX_APPS;
	memcpy(raw, p->apps, nraw*sizeof(AppleApp));
	p->napps=0;
	int flagged=0;
	for (int i=0;i<nraw;i++) {
		if (raw[i].sharing || raw[i].inplace)
			flagged++;
	}
	if (flagged>0) {
		for (int i=0;i<nraw;i++) {
			if (raw[i].sharing || raw[i].inplace)
				p->apps[p->napps++]=raw[i];
		}
	} else {
		for (int i=0;i<nraw && p->napps<APPLE_MAX_APPS;i++) {
			afc_connection c=NULL;
			int s=0;
			if (AppDocumentsOpen(p, raw[i].bundle, &c, &s)) {
				if (c && pAFCConnectionClose)
					pAFCConnectionClose(c);
				p->apps[p->napps++]=raw[i];
			}
		}
	}
	if (p->napps>1)
		qsort(p->apps, p->napps, sizeof(p->apps[0]), CmpApps);
	return p->napps>0;
}

static AppleApp* FindAppByName(ApplePhone* p, LPCWSTR name)
{
	if (!p || !name)
		return NULL;
	for (int i=0;i<p->napps;i++) {
		if (_wcsicmp(p->apps[i].name, name)==0)
			return &p->apps[i];
	}
	return NULL;
}

static BOOL HouseArrestSend(int sock, const char* command, const char* bundle)
{
	CFMutableDictionaryRef d=DictNew();
	if (d) {
		DictSetCStr(d, "Command", command);
		DictSetCStr(d, "Identifier", bundle);
		BOOL ok=PlistSendCF(sock, d, TRUE);
		if (!ok)
			ok=PlistSendCF(sock, d, FALSE);
		pCFRelease(d);
		if (ok)
			return TRUE;
	}
	char xml[1024];
	sprintf_s(xml,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
		"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
		"<plist version=\"1.0\"><dict>"
		"<key>Command</key><string>%s</string>"
		"<key>Identifier</key><string>%s</string>"
		"</dict></plist>", command, bundle);
	return PlistSendXml(sock, xml);
}

static BOOL HouseArrest(ApplePhone* p, const char* bundle, const char* command, int* sock)
{
	*sock=0;
	if (!bundle || !bundle[0])
		return FALSE;
	if (!StartNamedService(p, "com.apple.mobile.house_arrest", sock))
		return FALSE;
	if (!HouseArrestSend(*sock, command, bundle)) {
		*sock=0;
		return FALSE;
	}
	CFTypeRef pl=PlistRecv(*sock);
	BOOL ok=FALSE;
	if (pl) {
		WCHAR err[80], status[80];
		DictStr(pl, "Error", err, 80);
		DictStr(pl, "Status", status, 80);
		ok=(err[0]==0) && (_wcsicmp(status, L"Complete")==0 || _wcsicmp(status, L"Success")==0);
		pCFRelease(pl);
	}
	if (!ok)
		*sock=0;
	return ok;
}

static BOOL AfcHasDir(afc_connection conn, const char* name)
{
	if (!conn || !pAFCFileInfoOpen)
		return FALSE;
	afc_dictionary dict=NULL;
	if (pAFCFileInfoOpen(conn, name, &dict)!=0 || !dict)
		return FALSE;
	if (pAFCKeyValueClose)
		pAFCKeyValueClose(dict);
	return TRUE;
}

static BOOL SafeAfcDirOpen(afc_connection conn, const char* path, afc_directory* dir)
{
	*dir=NULL;
	if (!conn || !pAFCDirectoryOpen || !path || !path[0])
		return FALSE;
	BOOL ok=FALSE;
	__try {
		if (pAFCDirectoryOpen(conn, path, dir)==0 && *dir)
			ok=TRUE;
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		*dir=NULL;
		ok=FALSE;
	}
	return ok;
}

static BOOL OpenAfcDir(afc_connection conn, const char* path, afc_directory* dir)
{
	*dir=NULL;
	if (!conn)
		return FALSE;
	if (path && path[0] && strcmp(path, "/")!=0 && strcmp(path, ".")!=0)
		return SafeAfcDirOpen(conn, path, dir);
	static const char* tries[]={ ".", "/", "Panics", "Documents" };
	for (int i=0;i<(int)(sizeof(tries)/sizeof(tries[0]));i++) {
		if (SafeAfcDirOpen(conn, tries[i], dir))
			return TRUE;
	}
	return FALSE;
}

static void DetectAppRoot(ApplePhone* p)
{
	p->appRoot[0]=0;
	if (!p->appAfc)
		return;
	BOOL docs=AfcHasDir(p->appAfc, "Documents") || AfcHasDir(p->appAfc, "/Documents");
	BOOL lib=AfcHasDir(p->appAfc, "Library") || AfcHasDir(p->appAfc, "tmp");
	if (docs && lib)
		strcpy_s(p->appRoot, "Documents");
}

static BOOL AppDocumentsOpen(ApplePhone* p, const char* bundle, afc_connection* outConn, int* outSock)
{
	*outConn=NULL;
	*outSock=0;
	if (!p || !bundle || !bundle[0] || !EnsureLockdown(p) || !pAFCConnectionOpen)
		return FALSE;

	int sock=0;
	if (!HouseArrest(p, bundle, "VendDocuments", &sock) || sock==0)
		return FALSE;

	afc_connection conn=NULL;
	BOOL ok=FALSE;
	__try {
		if (pAFCConnectionOpen(sock, 0, &conn)==0 && conn)
			ok=TRUE;
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		conn=NULL;
		ok=FALSE;
	}
	if (!ok)
		return FALSE;
	*outConn=conn;
	*outSock=sock;
	return TRUE;
}

static BOOL EnsureAppAfc(ApplePhone* p, LPCWSTR appName)
{
	if (!p)
		return FALSE;
	if (p->napps==0)
		RefreshApps(p);
	AppleApp* a=FindAppByName(p, appName);
	if (!a)
		return FALSE;
	if (p->appAfc && _stricmp(p->appBundle, a->bundle)==0)
		return TRUE;
	CloseAppAfc(p);
	afc_connection conn=NULL;
	int sock=0;
	if (!AppDocumentsOpen(p, a->bundle, &conn, &sock))
		return FALSE;
	p->appAfc=conn;
	p->appSock=sock;
	strcpy_s(p->appBundle, a->bundle);
	DetectAppRoot(p);
	return TRUE;
}

struct PanicOpenJob {
	ApplePhone* p;
	HANDLE done;
	volatile LONG cancel;
	BOOL ok;
};

static DWORD WINAPI PanicOpenThread(LPVOID arg)
{
	PanicOpenJob* j=(PanicOpenJob*)arg;
	int dummy=0;
	StartNamedService(j->p, "com.apple.crashreportmover", &dummy);
	int sock=0;
	afc_connection conn=NULL;
	BOOL ok=FALSE;
	if (!j->cancel && StartNamedService(j->p, "com.apple.crashreportcopymobile", &sock) && sock) {
		__try {
			if (!j->cancel && pAFCConnectionOpen(sock, 0, &conn)==0 && conn)
				ok=TRUE;
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			ok=FALSE;
			conn=NULL;
		}
	}
	if (j->cancel) {
		if (conn && pAFCConnectionClose) {
			__try { pAFCConnectionClose(conn); } __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		if (j->done)
			SetEvent(j->done);
		return 0;
	}
	if (ok) {
		j->p->panicAfc=conn;
		j->p->panicSock=sock;
	}
	j->ok=ok;
	SetEvent(j->done);
	return 0;
}

static BOOL EnsurePanicAfc(ApplePhone* p)
{
	if (!p)
		return FALSE;
	if (p->panicAfc)
		return TRUE;
	if (!EnsureLockdown(p) || !pAFCConnectionOpen)
		return FALSE;
	PanicOpenJob* j=(PanicOpenJob*)calloc(1, sizeof(PanicOpenJob));
	if (!j)
		return FALSE;
	j->p=p;
	j->done=CreateEventW(NULL, TRUE, FALSE, NULL);
	HANDLE th=CreateThread(NULL, 0, PanicOpenThread, j, 0, NULL);
	if (!th) {
		if (j->done) CloseHandle(j->done);
		free(j);
		return FALSE;
	}
	DWORD w=WaitForSingleObject(j->done, 5000);
	if (w!=WAIT_OBJECT_0) {
		InterlockedExchange(&j->cancel, 1);
		CloseHandle(th);
		return p->panicAfc!=NULL;
	}
	WaitForSingleObject(th, 1000);
	BOOL ok=j->ok || p->panicAfc!=NULL;
	CloseHandle(th);
	CloseHandle(j->done);
	free(j);
	return ok;
}

static void JoinAfc(const char* prefix, const char* rel, char* out, int cch)
{
	if (!rel || !rel[0] || !strcmp(rel, "/") || !strcmp(rel, ".")) {
		if (prefix && prefix[0])
			strcpy_s(out, cch, prefix);
		else
			out[0]=0;
		return;
	}
	if (prefix && prefix[0])
		sprintf_s(out, cch, "%s/%s", prefix, rel);
	else
		strcpy_s(out, cch, rel);
}

static BOOL ResolveAfc(ApplePhone* p, LPCWSTR rel, afc_connection* conn, char* path, int pathcch, BOOL forWrite)
{
	WCHAR app[128];
	char relAfc[1024];
	int kind=ParseAppleRel(rel, app, 128, relAfc, 1024);
	if (kind==AR_ROOT || kind==AR_APPS)
		return FALSE;
	if (kind==AR_PHOTOS || kind==AR_PHOTOS_REL) {
		if (!EnsureSession(p) || !p->afc)
			return FALSE;
		*conn=p->afc;
		if (kind==AR_PHOTOS)
			strcpy_s(path, pathcch, "DCIM");
		else
			strcpy_s(path, pathcch, relAfc);
		return TRUE;
	}
	if (kind==AR_APP || kind==AR_APP_REL) {
		if (forWrite && kind==AR_APP)
			return FALSE;
		if (!EnsureAppAfc(p, app) || !p->appAfc)
			return FALSE;
		*conn=p->appAfc;
		JoinAfc(p->appRoot, relAfc, path, pathcch);
		return TRUE;
	}
	if (kind==AR_PANICS || kind==AR_PANICS_REL) {
		if (forWrite && kind==AR_PANICS)
			return FALSE;
		if (!EnsurePanicAfc(p) || !p->panicAfc)
			return FALSE;
		*conn=p->panicAfc;
		if (kind==AR_PANICS)
			strcpy_s(path, pathcch, ".");
		else
			strcpy_s(path, pathcch, relAfc);
		return TRUE;
	}
	return FALSE;
}

static AppleFind* NewFind(int phone, int kind)
{
	AppleFind* f=(AppleFind*)calloc(1, sizeof(AppleFind));
	f->magic=APPLE_FIND_MAGIC;
	f->phone=phone;
	f->kind=kind;
	return f;
}

BOOL AppleMdFindFirst(LPCWSTR deviceName, LPCWSTR relPath, WIN32_FIND_DATAW* fd, HANDLE* out)
{
	*out=INVALID_HANDLE_VALUE;
	AppleLock();
	ApplePhone* p=FindPhoneByName(deviceName);
	if (!p) {
		AppleUnlock();
		return FALSE;
	}
	WCHAR app[128];
	char afcPath[1024];
	int kind=ParseAppleRel(relPath, app, 128, afcPath, 1024);
	AppleFind* f=NULL;

	if (kind==AR_ROOT) {
		f=NewFind((int)(p-g_phones), AFK_ROOT);
		f->index=1;
		FillDirFd(APPLE_PHOTOS, fd);
		*out=(HANDLE)f;
		AppleUnlock();
		return TRUE;
	}
	if (kind==AR_APPS) {
		RefreshApps(p);
		if (p->napps==0) {
			AppleUnlock();
			return FALSE;
		}
		f=NewFind((int)(p-g_phones), AFK_APPS);
		f->index=1;
		FillDirFd(p->apps[0].name, fd);
		*out=(HANDLE)f;
		AppleUnlock();
		return TRUE;
	}

	afc_connection conn=NULL;
	if (!ResolveAfc(p, relPath, &conn, afcPath, 1024, FALSE) || !conn) {
		AppleUnlock();
		return FALSE;
	}
	afc_directory dir=NULL;
	if (!OpenAfcDir(conn, afcPath, &dir) || !dir) {
		AppleUnlock();
		return FALSE;
	}
	f=NewFind((int)(p-g_phones), AFK_AFC);
	f->dir=dir;
	f->conn=conn;
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
	if (f->phone<0 || f->phone>=g_nphones) {
		AppleUnlock();
		return FALSE;
	}
	ApplePhone* p=&g_phones[f->phone];
	if (f->kind==AFK_ROOT) {
		if (f->index==1) {
			FillDirFd(APPLE_APPS, fd);
			f->index=2;
			AppleUnlock();
			return TRUE;
		}
		if (f->index==2) {
			FillDirFd(APPLE_PANICS, fd);
			f->index=3;
			AppleUnlock();
			return TRUE;
		}
		AppleUnlock();
		return FALSE;
	}
	if (f->kind==AFK_APPS) {
		if (f->index>=p->napps) {
			AppleUnlock();
			return FALSE;
		}
		FillDirFd(p->apps[f->index].name, fd);
		f->index++;
		AppleUnlock();
		return TRUE;
	}
	if (!f->conn || !f->dir) {
		AppleUnlock();
		return FALSE;
	}
	for (;;) {
		char* name=NULL;
		BOOL got=FALSE;
		__try {
			got=(pAFCDirectoryRead(f->conn, f->dir, &name)==0 && name && name[0]);
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			got=FALSE;
			name=NULL;
		}
		if (!got) {
			AppleUnlock();
			return FALSE;
		}
		if (!strcmp(name, ".") || !strcmp(name, ".."))
			continue;
		FillFindFromAfc(f->conn, f->afcPath, name, fd);
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
	if (f->dir && f->conn && pAFCDirectoryClose) {
		__try {
			pAFCDirectoryClose(f->conn, f->dir);
		} __except(EXCEPTION_EXECUTE_HANDLER) {
		}
	}
	f->magic=0;
	free(f);
	AppleUnlock();
}

int AppleMdGetFile(LPCWSTR deviceName, LPCWSTR relPath, LPCWSTR localPath, ULONGLONG, FILETIME* mtime)
{
	AppleLock();
	ApplePhone* p=FindPhoneByName(deviceName);
	afc_connection conn=NULL;
	char afcPath[1024];
	if (!p || !ResolveAfc(p, relPath, &conn, afcPath, 1024, FALSE) || !conn) {
		AppleUnlock();
		return FS_FILE_READERROR;
	}
	afc_file_ref ref=0;
	if (pAFCFileRefOpen(conn, afcPath, 1, &ref)!=0) {
		AppleUnlock();
		return FS_FILE_NOTFOUND;
	}
	HANDLE out=CreateFileT((WCHAR*)localPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
	if (out==INVALID_HANDLE_VALUE) {
		pAFCFileRefClose(conn, ref);
		AppleUnlock();
		return FS_FILE_WRITEERROR;
	}
	char buf[64*1024];
	int result=FS_FILE_OK;
	for (;;) {
		size_t n=sizeof(buf);
		if (pAFCFileRefRead(conn, ref, buf, &n)!=0) {
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
	pAFCFileRefClose(conn, ref);
	if (mtime && result==FS_FILE_OK &&
		!(mtime->dwHighDateTime==0xFFFFFFFF))
		SetFileTime(out, NULL, NULL, mtime);
	CloseHandle(out);
	AppleUnlock();
	if (result!=FS_FILE_OK)
		DeleteFileT((WCHAR*)localPath);
	return result;
}

static BOOL AfcDeleteTree(afc_connection conn, const char* path)
{
	if (!conn || !path || !path[0] || !pAFCRemovePath)
		return FALSE;
	if (PathLooksLikeDir(conn, path) && pAFCDirectoryOpen && pAFCDirectoryRead) {
		afc_directory dir=NULL;
		if (pAFCDirectoryOpen(conn, path, &dir)==0 && dir) {
			for (;;) {
				char* name=NULL;
				if (pAFCDirectoryRead(conn, dir, &name)!=0 || !name || !name[0])
					break;
				if (!strcmp(name, ".") || !strcmp(name, ".."))
					continue;
				char child[1024];
				if (path[0] && strcmp(path, "/")!=0)
					sprintf_s(child, "%s/%s", path, name);
				else
					sprintf_s(child, "/%s", name);
				AfcDeleteTree(conn, child);
			}
			if (pAFCDirectoryClose)
				pAFCDirectoryClose(conn, dir);
		}
	}
	return pAFCRemovePath(conn, path)==0;
}

BOOL AppleMdDelete(LPCWSTR deviceName, LPCWSTR relPath)
{
	WCHAR app[128];
	char afcPath[1024];
	int kind=ParseAppleRel(relPath, app, 128, afcPath, 1024);
	if (kind==AR_ROOT || kind==AR_PHOTOS || kind==AR_APPS || kind==AR_APP || kind==AR_PANICS)
		return FALSE;
	AppleLock();
	ApplePhone* p=FindPhoneByName(deviceName);
	afc_connection conn=NULL;
	if (!p || !ResolveAfc(p, relPath, &conn, afcPath, 1024, TRUE) || !conn) {
		AppleUnlock();
		return FALSE;
	}
	BOOL ok=AfcDeleteTree(conn, afcPath);
	AppleUnlock();
	return ok;
}

BOOL AppleMdMkDir(LPCWSTR deviceName, LPCWSTR relPath)
{
	WCHAR app[128];
	char afcPath[1024];
	int kind=ParseAppleRel(relPath, app, 128, afcPath, 1024);
	if (kind==AR_ROOT || kind==AR_PHOTOS || kind==AR_APPS || kind==AR_APP ||
		kind==AR_PANICS || kind==AR_PANICS_REL)
		return FALSE;
	if (!pAFCDirectoryCreate)
		return FALSE;
	AppleLock();
	ApplePhone* p=FindPhoneByName(deviceName);
	afc_connection conn=NULL;
	if (!p || !ResolveAfc(p, relPath, &conn, afcPath, 1024, TRUE) || !conn) {
		AppleUnlock();
		return FALSE;
	}
	BOOL ok=pAFCDirectoryCreate(conn, afcPath)==0;
	AppleUnlock();
	return ok;
}

int AppleMdPutFile(LPCWSTR deviceName, LPCWSTR relPath, LPCWSTR localPath, BOOL overwrite)
{
	WCHAR app[128];
	char afcPath[1024];
	int kind=ParseAppleRel(relPath, app, 128, afcPath, 1024);
	if (kind==AR_ROOT || kind==AR_PHOTOS || kind==AR_APPS || kind==AR_APP ||
		kind==AR_PANICS || kind==AR_PANICS_REL)
		return FS_FILE_NOTSUPPORTED;
	if (!pAFCFileRefWrite)
		return FS_FILE_NOTSUPPORTED;
	AppleLock();
	ApplePhone* p=FindPhoneByName(deviceName);
	afc_connection conn=NULL;
	if (!p || !ResolveAfc(p, relPath, &conn, afcPath, 1024, TRUE) || !conn) {
		AppleUnlock();
		return FS_FILE_WRITEERROR;
	}
	if (!overwrite && pAFCFileInfoOpen) {
		afc_dictionary dict=NULL;
		if (pAFCFileInfoOpen(conn, afcPath, &dict)==0 && dict) {
			if (pAFCKeyValueClose)
				pAFCKeyValueClose(dict);
			AppleUnlock();
			return FS_FILE_EXISTS;
		}
	}
	HANDLE in=CreateFileT((WCHAR*)localPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (in==INVALID_HANDLE_VALUE) {
		AppleUnlock();
		return FS_FILE_READERROR;
	}
	afc_file_ref ref=0;
	if (pAFCFileRefOpen(conn, afcPath, 4, &ref)!=0) {
		CloseHandle(in);
		AppleUnlock();
		return FS_FILE_WRITEERROR;
	}
	char buf[64*1024];
	int result=FS_FILE_OK;
	for (;;) {
		DWORD rd=0;
		if (!ReadFile(in, buf, sizeof(buf), &rd, NULL)) {
			result=FS_FILE_READERROR;
			break;
		}
		if (rd==0)
			break;
		size_t n=rd;
		if (pAFCFileRefWrite(conn, ref, buf, &n)!=0) {
			result=FS_FILE_WRITEERROR;
			break;
		}
	}
	pAFCFileRefClose(conn, ref);
	CloseHandle(in);
	AppleUnlock();
	return result;
}
