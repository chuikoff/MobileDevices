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
typedef CFIndex (*t_CFDictionaryGetCount)(CFDictionaryRef);
typedef void (*t_CFDictionaryGetKeysAndValues)(CFDictionaryRef, const void**, const void**);
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
typedef mach_error_t (*t_AMDeviceStartHouseArrestService)(am_device, CFStringRef, void*, int*, void*);
typedef mach_error_t (*t_AMDeviceCreateHouseArrestService)(am_device, CFStringRef, void*, afc_connection*);
typedef int (*t_AMDServiceConnectionGetSocket)(void*);
typedef int (*t_AMDServiceConnectionSend)(void*, const void*, size_t);
typedef int (*t_AMDServiceConnectionReceive)(void*, void*, size_t);
typedef void* (*t_AMDServiceConnectionGetSecureIOContext)(void*);
typedef void (*t_AMDServiceConnectionInvalidate)(void*);
typedef int (*t_AFCConnectionSetSecureContext)(afc_connection, void*);
typedef afc_error_t (*t_AFCConnectionOpen)(void*, unsigned, afc_connection*);
typedef afc_error_t (*t_AFCConnectionClose)(afc_connection);
typedef afc_error_t (*t_AFCDirectoryOpen)(afc_connection, const char*, afc_directory*);
typedef afc_error_t (*t_AFCDirectoryRead)(afc_connection, afc_directory, char**);
typedef afc_error_t (*t_AFCDirectoryClose)(afc_connection, afc_directory);
typedef afc_error_t (*t_AFCFileInfoOpen)(afc_connection, const char*, afc_dictionary*);
typedef afc_error_t (*t_AFCKeyValueRead)(afc_dictionary, char**, char**);
typedef afc_error_t (*t_AFCKeyValueClose)(afc_dictionary);
typedef afc_error_t (*t_AFCFileRefOpen)(afc_connection, const char*, unsigned long long, afc_file_ref*);
typedef afc_error_t (*t_AFCFileRefRead)(afc_connection, afc_file_ref, void*, size_t*);
typedef afc_error_t (*t_AFCFileRefWrite)(afc_connection, afc_file_ref, const void*, size_t);
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
static t_CFDictionaryGetCount pCFDictionaryGetCount;
static t_CFDictionaryGetKeysAndValues pCFDictionaryGetKeysAndValues;
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
static t_AMDeviceCreateHouseArrestService pAMDeviceCreateHouseArrestService;
static t_AMDServiceConnectionGetSocket pAMDServiceConnectionGetSocket;
static t_AMDServiceConnectionSend pAMDServiceConnectionSend;
static t_AMDServiceConnectionReceive pAMDServiceConnectionReceive;
static t_AMDServiceConnectionGetSecureIOContext pAMDServiceConnectionGetSecureIOContext;
static t_AMDServiceConnectionInvalidate pAMDServiceConnectionInvalidate;
static t_AFCConnectionSetSecureContext pAFCConnectionSetSecureContext;
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
#define APPLE_MAX_APPS 1024
#define APPLE_PHOTOS L"Photos"
#define APPLE_APPS L"Applications"
#define APPLE_PANICS L"Panic Logs"
#define AFK_ROOT 1
#define AFK_APPS 2
#define AFK_AFC 3
#define AFK_PANIC_CACHE 4
#define APPLE_PANIC_MAX 256
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
	BOOL shareKnown;
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
	BOOL appDocsOnly;
	LONG appXfer;
	void* appSvc;
	char appMiss[160];
	DWORD appMissTick;
	afc_connection panicAfc;
	int panicSock;
	int nPanicEnt;
	BOOL panicListed;
	WIN32_FIND_DATAW panicEnt[APPLE_PANIC_MAX];
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
	WIN32_FIND_DATAW* ents;
	int nent;
};
#define APPLE_FIND_MAGIC 0x41464C44

static void AppleLock() { if (g_appleCsInit) EnterCriticalSection(&g_appleCs); }
static void AppleUnlock() { if (g_appleCsInit) LeaveCriticalSection(&g_appleCs); }
static BOOL AppleTryLock() { return g_appleCsInit && TryEnterCriticalSection(&g_appleCs); }

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
	if (!u || u8cch<=0)
		return;
	u[0]=0;
	if (!w)
		w=L"";
	if (WideCharToMultiByte(CP_UTF8, 0, w, -1, u, u8cch, NULL, NULL)>0)
		return;
	/* Buffer too small (or other error): convert the longest prefix that fits. */
	int wlen=(int)wcslen(w);
	int lo=0, hi=wlen, fit=0;
	while (lo<=hi) {
		int mid=lo+((hi-lo)/2);
		int need=WideCharToMultiByte(CP_UTF8, 0, w, mid, NULL, 0, NULL, NULL);
		if (need>0 && need<=u8cch-1) {
			fit=mid;
			lo=mid+1;
		} else
			hi=mid-1;
	}
	if (fit>0) {
		int n=WideCharToMultiByte(CP_UTF8, 0, w, fit, u, u8cch-1, NULL, NULL);
		if (n>0) {
			u[n]=0;
			return;
		}
	}
	u[0]=0;
}

/* AFCKeyValueRead returns aliases inside dict. AFCKeyValueClose frees them.
   Do not free(k)/free(v) — Apple's MobileDevice.dll will crash. */
static void AfcDictClose(afc_dictionary dict)
{
	if (dict && pAFCKeyValueClose)
		pAFCKeyValueClose(dict);
}

static BOOL AfcDictNext(afc_dictionary dict, char** k, char** v)
{
	if (k) *k=NULL;
	if (v) *v=NULL;
	if (!dict || !pAFCKeyValueRead || !k || !v)
		return FALSE;
	return pAFCKeyValueRead(dict, k, v)==0 && *k!=NULL;
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
		if (!AfcDictNext(dict, &k, &v))
			break;
		if (v && (!strcmp(k, "st_ifmt") || !strcmp(k, "st_nlink"))) {
			if (v && strstr(v, "DIR"))
				dir=TRUE;
		}
		if (v && !strcmp(k, "st_ifmt") && strstr(v, "S_IFDIR"))
			dir=TRUE;
	}
	AfcDictClose(dict);
	return dir;
}

/* No sprintf_s/strcpy_s: those abort via invalid_parameter_handler on overflow. */
static BOOL AfcCopyStr(char* dst, size_t dstcch, const char* src)
{
	if (!dst || dstcch==0)
		return FALSE;
	if (!src) {
		dst[0]=0;
		return TRUE;
	}
	size_t n=strlen(src);
	if (n>=dstcch) {
		dst[0]=0;
		return FALSE;
	}
	memcpy(dst, src, n+1);
	return TRUE;
}

static BOOL AfcPathHasDotDot(const char* s)
{
	if (!s || !s[0])
		return FALSE;
	const char* p=s;
	while (*p) {
		while (*p=='/')
			p++;
		if (!*p)
			break;
		const char* seg=p;
		while (*p && *p!='/')
			p++;
		size_t n=(size_t)(p-seg);
		if (n==2 && seg[0]=='.' && seg[1]=='.')
			return TRUE;
	}
	return FALSE;
}

static BOOL AfcJoinSlash(char* dst, size_t dstcch, const char* left, const char* right)
{
	if (!dst || dstcch==0)
		return FALSE;
	if (!left) left="";
	if (!right) right="";
	if (AfcPathHasDotDot(left) || AfcPathHasDotDot(right)) {
		dst[0]=0;
		return FALSE;
	}
	size_t nl=strlen(left), nr=strlen(right);
	if (nl+1+nr>=dstcch) {
		dst[0]=0;
		return FALSE;
	}
	memcpy(dst, left, nl);
	dst[nl]='/';
	memcpy(dst+nl+1, right, nr+1);
	return TRUE;
}

static BOOL AfcPrepend(char* dst, size_t dstcch, const char* prefix)
{
	if (!dst || dstcch==0 || !prefix)
		return FALSE;
	size_t pre=strlen(prefix);
	size_t n=strlen(dst);
	if (pre+n>=dstcch) {
		dst[0]=0;
		return FALSE;
	}
	memmove(dst+pre, dst, n+1);
	memcpy(dst, prefix, pre);
	return TRUE;
}

static int AfcJoinChild(char* dst, size_t dstcch, const char* dir, const char* name, BOOL slashRoot)
{
	if (!dst || dstcch==0 || !name || !name[0])
		return -1;
	if (!strcmp(name, "..") || !strcmp(name, ".") || strchr(name, '/') || strchr(name, '\\'))
		return -1;
	BOOL ok;
	if (dir && dir[0] && strcmp(dir, "/")!=0 && strcmp(dir, ".")!=0)
		ok=AfcJoinSlash(dst, dstcch, dir, name);
	else if (slashRoot)
		ok=AfcJoinSlash(dst, dstcch, "", name);
	else
		ok=AfcCopyStr(dst, dstcch, name);
	return ok ? (int)strlen(dst) : -1;
}

static void FillFindFromAfc(afc_connection conn, const char* dirPath, const char* name, WIN32_FIND_DATAW* fd)
{
	memset(fd, 0, sizeof(*fd));
	MultiByteToWideChar(CP_UTF8, 0, name, -1, fd->cFileName, MAX_PATH);
	fd->dwFileAttributes=FILE_ATTRIBUTE_NORMAL;
	fd->ftLastWriteTime.dwHighDateTime=0xFFFFFFFF;
	fd->ftLastWriteTime.dwLowDateTime=0xFFFFFFFE;
	char full[1024];
	if (AfcJoinChild(full, countof(full), dirPath, name, FALSE)<0)
		return;
	if (!pAFCFileInfoOpen || !pAFCKeyValueRead)
		return;
	afc_dictionary dict=NULL;
	int infoErr=-1;
	__try {
		infoErr=pAFCFileInfoOpen(conn, full, &dict);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		infoErr=-1;
		dict=NULL;
	}
	if (infoErr!=0 || !dict)
		return;
	for (;;) {
		char *k=NULL, *v=NULL;
		if (!AfcDictNext(dict, &k, &v))
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
	AfcDictClose(dict);
	if (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		fd->nFileSizeHigh=0;
		fd->nFileSizeLow=0;
	}
}

static void CloseAppAfc(ApplePhone* p)
{
	if (!p)
		return;
	if (p->appXfer)
		return;
	if (p->appAfc && pAFCConnectionClose) {
		__try { pAFCConnectionClose(p->appAfc); } __except(EXCEPTION_EXECUTE_HANDLER) {}
		p->appAfc=NULL;
	}
	p->appSock=0;
	p->appBundle[0]=0;
	p->appRoot[0]=0;
	p->appDocsOnly=FALSE;
	if (p->appSvc && pAMDServiceConnectionInvalidate) {
		__try { pAMDServiceConnectionInvalidate(p->appSvc); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
	p->appSvc=NULL;
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
	p->nPanicEnt=0;
	p->panicListed=FALSE;
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

static void CloseAppleSock(int* sock)
{
	if (!sock || *sock==0)
		return;
	closesocket(*sock);
	*sock=0;
}

static BOOL AfcOpenAny(void* handle, afc_connection* out)
{
	*out=NULL;
	if (!handle || !pAFCConnectionOpen)
		return FALSE;
	BOOL ok=FALSE;
	__try {
		if (pAFCConnectionOpen(handle, 0, out)==0 && *out)
			ok=TRUE;
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		*out=NULL;
		ok=FALSE;
	}
	return ok;
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
	afc_connection conn=NULL;
	if (!AfcOpenAny((void*)(intptr_t)(unsigned)sock, &conn) || !conn) {
		CloseAppleSock(&sock);
		return FALSE;
	}
	p->sock=sock;
	p->afc=conn;
	return TRUE;
}

static void DropPhoneSession(ApplePhone* p)
{
	if (!p)
		return;
	CloseAppAfc(p);
	ClosePanicAfc(p);
	if (p->afc && pAFCConnectionClose) {
		__try { pAFCConnectionClose(p->afc); } __except(EXCEPTION_EXECUTE_HANDLER) {}
		p->afc=NULL;
	}
	if (p->session && pAMDeviceStopSession) {
		__try { pAMDeviceStopSession(p->dev); } __except(EXCEPTION_EXECUTE_HANDLER) {}
		p->session=FALSE;
	}
	if (p->dev && pAMDeviceDisconnect) {
		__try { pAMDeviceDisconnect(p->dev); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
}

static void ClosePhone(ApplePhone* p)
{
	if (!p)
		return;
	DropPhoneSession(p);
	p->napps=0;
}

void AppleMdResetSessions(void)
{
	AppleLock();
	for (int i=0;i<g_nphones;i++)
		DropPhoneSession(&g_phones[i]);
	AppleUnlock();
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
	/* Apple CFRunLoop thread. info->dev lifetime is Apple's and may already be invalid. */
	if (!info || !info->dev)
		return;
	am_device dev=info->dev;
	unsigned int msg=info->msg;
	/* Must not block this thread: StartService waits on the run loop. */
	if (!AppleTryLock())
		return;
	if (InterlockedCompareExchange(&g_loopStop, 0, 0)!=0) {
		AppleUnlock();
		return;
	}
	int slot=-1;
	/* Do not add C++ objects with destructors in this __try: SEH will skip them. */
	__try {
		if (msg==ADNCI_MSG_CONNECTED && g_nphones<APPLE_MAX && pAMDeviceConnect) {
			BOOL known=FALSE;
			for (int i=0;i<g_nphones;i++) {
				if (g_phones[i].dev==dev) {
					known=TRUE;
					break;
				}
			}
			if (!known && pAMDeviceConnect(dev)==0) {
				slot=g_nphones;
				ApplePhone* p=&g_phones[slot];
				memset(p, 0, sizeof(*p));
				p->dev=dev;
				CopyValue(dev, "DeviceName", p->name, 128);
				CopyValue(dev, "UniqueDeviceID", p->udid, 80);
				CopyValue(dev, "ProductVersion", p->ios, 40);
				CopyValue(dev, "BuildVersion", p->build, 40);
				CopyValue(dev, "ProductType", p->product, 40);
				WCHAR dclass[40];
				CopyValue(dev, "DeviceClass", dclass, 40);
				BOOL isPhone=FALSE;
				if (!p->product[0] && !dclass[0])
					isPhone=TRUE;
				else if (wcsstr(p->product, L"iPhone") || wcsstr(p->product, L"iPad") || wcsstr(p->product, L"iPod") ||
					wcsstr(dclass, L"iPhone") || wcsstr(dclass, L"iPad") || wcsstr(dclass, L"iPod"))
					isPhone=TRUE;
				if (!isPhone) {
					if (pAMDeviceDisconnect)
						pAMDeviceDisconnect(dev);
					memset(p, 0, sizeof(*p));
					slot=-1;
				} else {
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
							swprintf_s(tmp, countof(tmp), L"%s (%d)", p->name, i+2);
							wcslcpy(p->name, tmp, 128);
							break;
						}
					}
					if (pAMDeviceDisconnect)
						pAMDeviceDisconnect(dev);
					g_nphones++;
					slot=-1;
				}
			}
		} else if (msg==ADNCI_MSG_DISCONNECTED) {
			for (int i=0;i<g_nphones;i++) {
				if (g_phones[i].dev==dev) {
					ClosePhone(&g_phones[i]);
					g_phones[i]=g_phones[g_nphones-1];
					memset(&g_phones[g_nphones-1], 0, sizeof(g_phones[0]));
					g_nphones--;
					break;
				}
			}
		}
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		if (slot>=0 && slot<APPLE_MAX)
			memset(&g_phones[slot], 0, sizeof(g_phones[0]));
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
	swprintf_s(a, countof(a), L"%s\\MobileDevice.dll", pkg);
	swprintf_s(b, countof(b), L"%s\\CoreFoundation.dll", pkg);
	return FileExistsW(a) && FileExistsW(b);
#else
	WCHAR a[MAX_PATH], b[MAX_PATH];
	swprintf_s(a, countof(a), L"%s\\AMDS32\\MobileDevice.dll", pkg);
	swprintf_s(b, countof(b), L"%s\\VFS\\ProgramFilesCommonX86\\Apple\\Apple Application Support\\CoreFoundation.dll", pkg);
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
	swprintf_s(apps, countof(apps), L"%s\\WindowsApps", pf);
	WCHAR spec[MAX_PATH];
	swprintf_s(spec, countof(spec), L"%s\\AppleInc.AppleDevices_*", apps);
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
	swprintf_s(src, countof(src), L"%s\\%s", srcDir, name);
	swprintf_s(dst, countof(dst), L"%s\\%s", dstDir, name);
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
	swprintf_s(dest, destcch, L"%s\\MobileDevices\\amds64", local);
#else
	swprintf_s(dest, destcch, L"%s\\MobileDevices\\amds32", local);
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
	swprintf_s(aas, countof(aas), L"%s\\VFS\\ProgramFilesCommonX86\\Apple\\Apple Application Support", pkg);
	swprintf_s(amds32, countof(amds32), L"%s\\AMDS32", pkg);
	for (int i=0;i<(int)(sizeof(names)/sizeof(names[0]));i++) {
		if (!CopyOneDll(aas, names[i], dest))
			CopyOneDll(amds32, names[i], dest);
	}
#endif
	WCHAR md[MAX_PATH];
	swprintf_s(md, countof(md), L"%s\\MobileDevice.dll", dest);
	WCHAR cf[MAX_PATH];
	swprintf_s(cf, countof(cf), L"%s\\CoreFoundation.dll", dest);
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
	pAMDeviceCreateHouseArrestService=(t_AMDeviceCreateHouseArrestService)GetProcAddress(g_md, "AMDeviceCreateHouseArrestService");
	pAMDServiceConnectionGetSocket=(t_AMDServiceConnectionGetSocket)GetProcAddress(g_md, "AMDServiceConnectionGetSocket");
	pAMDServiceConnectionSend=(t_AMDServiceConnectionSend)GetProcAddress(g_md, "AMDServiceConnectionSend");
	pAMDServiceConnectionReceive=(t_AMDServiceConnectionReceive)GetProcAddress(g_md, "AMDServiceConnectionReceive");
	pAMDServiceConnectionGetSecureIOContext=(t_AMDServiceConnectionGetSecureIOContext)GetProcAddress(g_md, "AMDServiceConnectionGetSecureIOContext");
	pAMDServiceConnectionInvalidate=(t_AMDServiceConnectionInvalidate)GetProcAddress(g_md, "AMDServiceConnectionInvalidate");
	pAFCConnectionSetSecureContext=(t_AFCConnectionSetSecureContext)GetProcAddress(g_md, "AFCConnectionSetSecureContext");
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
	pCFDictionaryGetCount=(t_CFDictionaryGetCount)GetProcAddress(g_cf, "CFDictionaryGetCount");
	pCFDictionaryGetKeysAndValues=(t_CFDictionaryGetKeysAndValues)GetProcAddress(g_cf, "CFDictionaryGetKeysAndValues");
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
		swprintf_s(path, countof(path), L"%s\\CoreFoundation.dll", dest);
		g_cf=LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
		swprintf_s(path, countof(path), L"%s\\MobileDevice.dll", dest);
		g_md=LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	}
	if (!g_md) {
		WCHAR pf[MAX_PATH], mds[MAX_PATH];
		GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH);
		swprintf_s(mds, countof(mds), L"%s\\Common Files\\Apple\\Mobile Device Support\\iTunesMobileDevice.dll", pf);
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
	if (g_appleCsInit) {
		DeleteCriticalSection(&g_appleCs);
		g_appleCsInit=FALSE;
	}
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
	info->batteryHealth=-1;
	info->batteryCycles=-1;
	AppleLock();
	ApplePhone* p=FindPhoneByName(deviceName);
	if (!p) {
		AppleUnlock();
		return FALSE;
	}
	wcslcpy(info->manufacturer, L"Apple", 128);
	if (p->ios[0] && p->build[0])
		swprintf_s(info->firmware, countof(info->firmware), L"%s (%s)", p->ios, p->build);
	else if (p->ios[0])
		wcslcpy(info->firmware, p->ios, 128);
	wcslcpy(info->protocol, L"Apple Mobile Device (AFC)", 80);
	wcslcpy(info->os, L"iOS", 40);
	if (p->ios[0])
		swprintf_s(info->os, countof(info->os), L"iOS %s", p->ios);

	EnsureLockdown(p);
	/* Prefer human MarketingName when lockdown provides it; else ProductType. */
	CopyValue(p->dev, "MarketingName", info->model, 128);
	if (!info->model[0])
		wcslcpy(info->model, p->product[0] ? p->product : p->name, 128);

	CopyValue(p->dev, "SerialNumber", info->serial, 64);
	CopyValue(p->dev, "InternationalMobileEquipmentIdentity", info->imei, 64);
	CopyValue(p->dev, "InternationalMobileEquipmentIdentity2", info->imei2, 64);

	static const char* batDom="com.apple.mobile.battery";
	int bat=CopyValueInt(p->dev, NULL, "BatteryCurrentCapacity");
	if (bat<0 || bat>100)
		bat=CopyValueInt(p->dev, batDom, "BatteryCurrentCapacity");
	if (bat>=0 && bat<=100)
		info->battery=bat;

	/* CycleCount — omit if missing. Tried also BatteryCycleCount (not used if absent). */
	int cycles=CopyValueInt(p->dev, batDom, "CycleCount");
	if (cycles<0)
		cycles=CopyValueInt(p->dev, batDom, "BatteryCycleCount");
	if (cycles>=0)
		info->batteryCycles=cycles;

	/*
	 * Maximum capacity % from DesignCapacity vs AppleRawMaxCapacity /
	 * NominalChargeCapacity / MaxCapacity. Direct keys tried (omit if absent):
	 * MaximumCapacityPercent, BatteryHealth, GasGaugeBatteryHealth.
	 * Do not claim health if design/max unavailable.
	 */
	int directHealth=CopyValueInt(p->dev, batDom, "MaximumCapacityPercent");
	if (directHealth<0 || directHealth>100)
		directHealth=CopyValueInt(p->dev, batDom, "BatteryHealth");
	if (directHealth<0 || directHealth>100)
		directHealth=CopyValueInt(p->dev, batDom, "GasGaugeBatteryHealth");
	if (directHealth>=0 && directHealth<=100)
		info->batteryHealth=directHealth;
	else {
		ULONGLONG design=0, maxc=0;
		CopyValueU64(p->dev, batDom, "DesignCapacity", &design);
		if (!CopyValueU64(p->dev, batDom, "AppleRawMaxCapacity", &maxc))
			if (!CopyValueU64(p->dev, batDom, "NominalChargeCapacity", &maxc))
				CopyValueU64(p->dev, batDom, "MaxCapacity", &maxc);
		info->designCapacity=design;
		info->maxCapacity=maxc;
		if (design>0 && maxc>0) {
			double pct=100.0*(double)maxc/(double)design;
			int h=(int)(pct+0.5);
			if (h<0) h=0;
			if (h>100) h=100;
			info->batteryHealth=h;
		}
	}

	ULONGLONG cap=0, freeb=0;
	if (EnsureSession(p) && p->afc && pAFCDeviceInfoOpen && pAFCKeyValueRead) {
		afc_dictionary dict=NULL;
		if (pAFCDeviceInfoOpen(p->afc, &dict)==0 && dict) {
			for (;;) {
				char *k=NULL, *v=NULL;
				if (!AfcDictNext(dict, &k, &v))
					break;
				if (!v)
					continue;
				if (!strcmp(k, "FSTotalBytes"))
					cap=_strtoui64(v, NULL, 10);
				else if (!strcmp(k, "FSFreeBytes"))
					freeb=_strtoui64(v, NULL, 10);
			}
			AfcDictClose(dict);
		}
	}

	/* disk_usage (+ .factory fallback). Photo/app keys tried if present. */
	static const char* diskDom="com.apple.disk_usage";
	static const char* diskFactory="com.apple.disk_usage.factory";
	ULONGLONG totalDisk=0, dataCap=0, dataAvail=0, sysCap=0, sysAvail=0;
	ULONGLONG photo=0, apps=0;
	CopyValueU64(p->dev, diskDom, "TotalDiskCapacity", &totalDisk);
	if (!totalDisk)
		CopyValueU64(p->dev, diskFactory, "TotalDiskCapacity", &totalDisk);
	CopyValueU64(p->dev, diskDom, "TotalDataCapacity", &dataCap);
	if (!dataCap)
		CopyValueU64(p->dev, diskFactory, "TotalDataCapacity", &dataCap);
	if (!CopyValueU64(p->dev, diskDom, "TotalDataAvailable", &dataAvail))
		if (!CopyValueU64(p->dev, diskDom, "AmountDataAvailable", &dataAvail))
			if (!CopyValueU64(p->dev, diskFactory, "TotalDataAvailable", &dataAvail))
				CopyValueU64(p->dev, diskFactory, "AmountDataAvailable", &dataAvail);
	CopyValueU64(p->dev, diskDom, "TotalSystemCapacity", &sysCap);
	if (!sysCap)
		CopyValueU64(p->dev, diskFactory, "TotalSystemCapacity", &sysCap);
	CopyValueU64(p->dev, diskDom, "TotalSystemAvailable", &sysAvail);
	if (!sysAvail)
		CopyValueU64(p->dev, diskFactory, "TotalSystemAvailable", &sysAvail);
	/* Optional usage keys — skip silently when lockdown omits them. */
	if (!CopyValueU64(p->dev, diskDom, "PhotoDataUsage", &photo))
		if (!CopyValueU64(p->dev, diskDom, "PhotoUsage", &photo))
			CopyValueU64(p->dev, diskFactory, "PhotoDataUsage", &photo);
	if (!CopyValueU64(p->dev, diskDom, "MobileApplicationUsage", &apps))
		if (!CopyValueU64(p->dev, diskDom, "AppUsage", &apps))
			CopyValueU64(p->dev, diskFactory, "MobileApplicationUsage", &apps);

	info->totalDisk=totalDisk;
	info->dataCapacity=dataCap;
	info->dataAvailable=dataAvail;
	info->systemCapacity=sysCap;
	info->systemAvailable=sysAvail;
	info->photoUsage=photo;
	info->appUsage=apps;

	if (!cap && !freeb) {
		if (totalDisk)
			cap=totalDisk;
		else if (dataCap)
			cap=dataCap;
		if (dataAvail)
			freeb=dataAvail;
		else {
			CopyValueU64(p->dev, diskDom, "TotalDiskCapacity", &cap);
			if (!CopyValueU64(p->dev, diskDom, "TotalDataAvailable", &freeb))
				CopyValueU64(p->dev, diskDom, "AmountDataAvailable", &freeb);
		}
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
	if (!u || cch<=0)
		return;
	WideToUtf8(w ? w : L"", u, cch);
	u[cch-1]=0;
	for (int i=0; i<cch-1 && u[i]; i++) {
		if (u[i]=='\\')
			u[i]='/';
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
		if (afcOut && afccch>0)
			AfcPrepend(afcOut, (size_t)afccch, "DCIM/");
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
	if (afcOut && afccch>0)
		AfcPrepend(afcOut, (size_t)afccch, "DCIM/");
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

static DWORD g_plistRecvMs=8000;

static BOOL SockRecvAll(int sock, void* data, int n)
{
	char* p=(char*)data;
	DWORD t=g_plistRecvMs ? g_plistRecvMs : 8000;
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

static BOOL DictHasKey(CFTypeRef d, const char* key)
{
	return DictGet(d, key)!=NULL;
}

static void NoteShareFlag(CFTypeRef d, const char* key, BOOL* known, BOOL* on)
{
	if (!d || !known || !on || !DictHasKey(d, key))
		return;
	*known=TRUE;
	if (DictTruthy(d, key))
		*on=TRUE;
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
		swprintf_s(unique, countof(unique), L"%s (%d)", name, extra++);
	}
	AppleApp* a=&p->apps[p->napps++];
	memset(a, 0, sizeof(*a));
	wcslcpy(a->name, unique, 128);
	strcpy_s(a->bundle, bundle);
	BOOL known=FALSE, share=FALSE, inplace=FALSE;
	NoteShareFlag(app, "UIFileSharingEnabled", &known, &share);
	NoteShareFlag(app, "UISupportsDocumentBrowser", &known, &share);
	NoteShareFlag(app, "LSSupportsOpeningDocumentsInPlace", &known, &inplace);
	CFTypeRef info=DictGet(app, "Info");
	if (info) {
		NoteShareFlag(info, "UIFileSharingEnabled", &known, &share);
		NoteShareFlag(info, "UISupportsDocumentBrowser", &known, &share);
		NoteShareFlag(info, "LSSupportsOpeningDocumentsInPlace", &known, &inplace);
	}
	a->sharing=share;
	a->inplace=inplace;
	a->shareKnown=known;
}

static int CmpApps(const void* a, const void* b)
{
	const AppleApp* x=(const AppleApp*)a;
	const AppleApp* y=(const AppleApp*)b;
	return _wcsicmp(x->name, y->name);
}

static BOOL HouseArrest(ApplePhone* p, const char* bundle, const char* command, int* sock, void** service);
static BOOL AppDocumentsOpen(ApplePhone* p, const char* bundle, afc_connection* outConn, int* outSock, BOOL* docsOnly, char* rootOut, int rootCch, BOOL skipDocuments);

static void MdLog(const char* line)
{
	static int cleared=0;
	char path[MAX_PATH];
	DWORD n=GetTempPathA(MAX_PATH, path);
	if (!n || n>=MAX_PATH-20)
		return;
	strcat_s(path, "mobiledevices.log");
	FILE* f=NULL;
	if (fopen_s(&f, path, cleared ? "a" : "w")!=0 || !f)
		return;
	cleared=1;
	fputs(line, f);
	fputc('\n', f);
	fclose(f);
}

static void AddLookupDict(ApplePhone* p, CFTypeRef dict)
{
	if (!p || !dict || !pCFGetTypeID || !pCFDictionaryGetTypeID || !pCFDictionaryGetCount || !pCFDictionaryGetKeysAndValues)
		return;
	if (pCFGetTypeID(dict)!=pCFDictionaryGetTypeID())
		return;
	CFIndex cnt=pCFDictionaryGetCount((CFDictionaryRef)dict);
	if (cnt<=0 || cnt>8192)
		return;
	const void** vals=(const void**)calloc((size_t)cnt, sizeof(void*));
	if (!vals)
		return;
	pCFDictionaryGetKeysAndValues((CFDictionaryRef)dict, NULL, vals);
	for (CFIndex i=0;i<cnt;i++)
		AddAppFromDict(p, (CFTypeRef)vals[i]);
	free(vals);
}

static void ReadInstallProxy(ApplePhone* p, int sock, const char* xml, const char* listKey)
{
	if (!PlistSendXml(sock, xml))
		return;
	for (int n=0;n<4096;n++) {
		CFTypeRef pl=PlistRecv(sock);
		if (!pl)
			break;
		WCHAR status[64];
		DictStr(pl, "Status", status, 64);
		CFTypeRef list=DictGet(pl, listKey);
		if (list && pCFGetTypeID && pCFArrayGetTypeID &&
			pCFGetTypeID(list)==pCFArrayGetTypeID()) {
			CFIndex cnt=pCFArrayGetCount((CFArrayRef)list);
			for (CFIndex i=0;i<cnt;i++)
				AddAppFromDict(p, pCFArrayGetValueAtIndex((CFArrayRef)list, i));
		} else if (list) {
			AddLookupDict(p, list);
		}
		BOOL done=_wcsicmp(status, L"Complete")==0;
		pCFRelease(pl);
		if (done)
			break;
	}
}

static BOOL RefreshApps(ApplePhone* p)
{
	p->napps=0;
	if (!pCFPropertyListCreateWithData || !pCFArrayGetCount)
		return FALSE;
	int sock=0;
	if (!StartNamedService(p, "com.apple.mobile.installation_proxy", &sock))
		return FALSE;
	const char* browse=
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
		"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
		"<plist version=\"1.0\"><dict>"
		"<key>Command</key><string>Browse</string>"
		"<key>ClientOptions</key><dict>"
		"<key>ApplicationType</key><string>Any</string>"
		"</dict></dict></plist>";
	ReadInstallProxy(p, sock, browse, "CurrentList");
	closesocket(sock);
	sock=0;
	if (StartNamedService(p, "com.apple.mobile.installation_proxy", &sock)) {
		const char* lookup=
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
			"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
			"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
			"<plist version=\"1.0\"><dict>"
			"<key>Command</key><string>Lookup</string>"
			"<key>ClientOptions</key><dict>"
			"<key>ApplicationType</key><string>Any</string>"
			"</dict></dict></plist>";
		ReadInstallProxy(p, sock, lookup, "LookupResult");
		closesocket(sock);
	}

	AppleApp* raw=(AppleApp*)calloc(APPLE_MAX_APPS, sizeof(AppleApp));
	if (raw) {
		int nraw=p->napps;
		if (nraw>APPLE_MAX_APPS)
			nraw=APPLE_MAX_APPS;
		memcpy(raw, p->apps, nraw*sizeof(AppleApp));
		p->napps=0;
		/* Keep every non-Apple app. File Sharing flags are often missing,
		   so dropping on that flag hides folders that do contain files. */
		for (int i=0;i<nraw && p->napps<APPLE_MAX_APPS;i++) {
			if (_strnicmp(raw[i].bundle, "com.apple.", 10)==0)
				continue;
			p->apps[p->napps++]=raw[i];
		}
		free(raw);
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
	/* XML first: the same framing already works for installation_proxy on this phone.
	   A binary plist that is only half-accepted leaves the socket out of AFC mode. */
	char xml[1024];
	if (_snprintf_s(xml, countof(xml), _TRUNCATE,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
		"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
		"<plist version=\"1.0\"><dict>"
		"<key>Command</key><string>%s</string>"
		"<key>Identifier</key><string>%s</string>"
		"</dict></plist>", command, bundle)>=0 && PlistSendXml(sock, xml))
		return TRUE;
	CFMutableDictionaryRef d=DictNew();
	if (!d)
		return FALSE;
	DictSetCStr(d, "Command", command);
	DictSetCStr(d, "Identifier", bundle);
	BOOL ok=PlistSendCF(sock, d, FALSE);
	if (!ok)
		ok=PlistSendCF(sock, d, TRUE);
	pCFRelease(d);
	return ok;
}

static BOOL StartHouseArrestSock(ApplePhone* p, int* sock, void** service)
{
	*sock=0;
	if (service)
		*service=NULL;
	if (!EnsureLockdown(p))
		return FALSE;
	CFStringRef svc=CfStr("com.apple.mobile.house_arrest");
	if (!svc)
		return FALSE;
	int s=0;
	/* Same path as installation_proxy. The SecureStart socket does not answer
	   a raw VendDocuments plist (no-reply, 8s each try). */
	if (pAMDeviceStartService && pAMDeviceStartService(p->dev, svc, &s, NULL)==0 && s) {
		pCFRelease(svc);
		*sock=s;
		return TRUE;
	}
	pCFRelease(svc);
	MdLog("house startservice fail");
	return FALSE;
}

static BOOL HouseArrest(ApplePhone* p, const char* bundle, const char* command, int* sock, void** service)
{
	*sock=0;
	if (service)
		*service=NULL;
	if (!bundle || !bundle[0])
		return FALSE;
	if (!StartHouseArrestSock(p, sock, service))
		return FALSE;
	if (!HouseArrestSend(*sock, command, bundle)) {
		CloseAppleSock(sock);
		return FALSE;
	}
	g_plistRecvMs=1500;
	CFTypeRef pl=PlistRecv(*sock);
	g_plistRecvMs=8000;
	BOOL ok=FALSE;
	char note[160];
	note[0]=0;
	if (pl) {
		WCHAR err[80], status[80];
		DictStr(pl, "Error", err, 80);
		DictStr(pl, "Status", status, 80);
		ok=(err[0]==0) && (
			_wcsicmp(status, L"Complete")==0 ||
			_wcsicmp(status, L"Success")==0 ||
			_wcsicmp(status, L"Ready")==0);
		_snprintf_s(note, countof(note), _TRUNCATE, "house %s %s err=%ls status=%ls",
			command, bundle, err, status);
		pCFRelease(pl);
	} else {
		_snprintf_s(note, countof(note), _TRUNCATE, "house %s %s no-reply", command, bundle);
	}
	MdLog(note);
	if (!ok)
		CloseAppleSock(sock);
	return ok;
}

static int SafeAfcFileOpen(afc_connection conn, const char* path, unsigned long long mode, afc_file_ref* ref)
{
	*ref=0;
	if (!pAFCFileRefOpen || !conn || !path || !path[0])
		return -1;
	int err=-1;
	__try {
		err=pAFCFileRefOpen(conn, path, mode, ref);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		*ref=0;
		err=-1;
	}
	if (err!=0)
		*ref=0;
	return err;
}

static int SafeAfcFileWrite(afc_connection conn, afc_file_ref ref, const void* buf, size_t n)
{
	if (!pAFCFileRefWrite || !conn || !buf || n==0)
		return -1;
	int err=-1;
	__try {
		err=pAFCFileRefWrite(conn, ref, buf, n);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		err=-1;
	}
	return err;
}

static void SafeAfcFileClose(afc_connection conn, afc_file_ref ref)
{
	if (!pAFCFileRefClose || !conn || !ref)
		return;
	__try {
		pAFCFileRefClose(conn, ref);
	} __except(EXCEPTION_EXECUTE_HANDLER) {}
}

static BOOL AfcHasDir(afc_connection conn, const char* name)
{
	if (!conn || !pAFCFileInfoOpen || !name || !name[0])
		return FALSE;
	afc_dictionary dict=NULL;
	int err=-1;
	__try {
		err=pAFCFileInfoOpen(conn, name, &dict);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		err=-1;
		dict=NULL;
	}
	if (err!=0 || !dict)
		return FALSE;
	AfcDictClose(dict);
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
	if (!conn || !path || !path[0])
		return FALSE;
	/* House arrest on current MobileDevice lists "/" and rejects ".". */
	if (strcmp(path, ".")==0 || strcmp(path, "/")==0)
		return SafeAfcDirOpen(conn, "/", dir) || SafeAfcDirOpen(conn, ".", dir);
	if (SafeAfcDirOpen(conn, path, dir))
		return TRUE;
	if (path[0]=='/')
		return SafeAfcDirOpen(conn, path+1, dir);
	if (path[0]!='/') {
		char slash[1024];
		if (AfcJoinSlash(slash, countof(slash), "", path) && SafeAfcDirOpen(conn, slash, dir))
			return TRUE;
	}
	return FALSE;
}

static void DetectAppRoot(ApplePhone* p)
{
	p->appRoot[0]=0;
	if (!p->appAfc || p->appDocsOnly)
		return;
	if (PathLooksLikeDir(p->appAfc, "Documents"))
		strcpy_s(p->appRoot, "Documents");
}

static BOOL CreateHouseArrestConn(ApplePhone* p, const char* bundle, afc_connection* out)
{
	*out=NULL;
	if (!pAMDeviceCreateHouseArrestService || !p || !p->dev || !bundle || !bundle[0])
		return FALSE;
	CFStringRef bid=CfStr(bundle);
	if (!bid)
		return FALSE;
	afc_connection conn=NULL;
	mach_error_t err=-1;
	__try {
		err=pAMDeviceCreateHouseArrestService(p->dev, bid, NULL, &conn);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		err=-1;
		conn=NULL;
	}
	pCFRelease(bid);
	if (err==0 && conn) {
		*out=conn;
		return TRUE;
	}
	if (conn && pAFCConnectionClose) {
		__try { pAFCConnectionClose(conn); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
	return FALSE;
}

static BOOL StartHouseArrestSockAfc(ApplePhone* p, const char* bundle, afc_connection* outConn, int* outSock)
{
	*outConn=NULL;
	*outSock=0;
	if (!pAMDeviceStartHouseArrestService || !p || !p->dev || !bundle || !bundle[0])
		return FALSE;
	CFStringRef bid=CfStr(bundle);
	if (!bid)
		return FALSE;
	int sock=0;
	mach_error_t err=-1;
	__try {
		err=pAMDeviceStartHouseArrestService(p->dev, bid, NULL, &sock, NULL);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		err=-1;
		sock=0;
	}
	pCFRelease(bid);
	if (err!=0 || sock==0)
		return FALSE;
	afc_connection conn=NULL;
	if (!AfcOpenAny((void*)(intptr_t)sock, &conn) || !conn) {
		CloseAppleSock(&sock);
		return FALSE;
	}
	*outConn=conn;
	*outSock=sock;
	return TRUE;
}

static BOOL AfcProbe(afc_connection conn)
{
	if (!conn)
		return FALSE;
	return AfcHasDir(conn, ".") || AfcHasDir(conn, "Documents");
}

static BOOL TakeProbedAfc(afc_connection conn, afc_connection* out)
{
	if (!conn)
		return FALSE;
	if (AfcProbe(conn)) {
		*out=conn;
		return TRUE;
	}
	if (pAFCConnectionClose) {
		__try { pAFCConnectionClose(conn); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
	return FALSE;
}

static void AfcConnDiscard(afc_connection conn, int* sock)
{
	if (conn && pAFCConnectionClose) {
		__try { pAFCConnectionClose(conn); } __except(EXCEPTION_EXECUTE_HANDLER) {}
		/* AFCConnectionClose already owns the socket. closesocket here
		   closes whatever handle Windows reused next. */
		if (sock)
			*sock=0;
		return;
	}
	if (sock)
		CloseAppleSock(sock);
}

static int ReadAfcNames(afc_connection conn, const char* path, char*** outNames)
{
	if (outNames)
		*outNames=NULL;
	afc_directory dir=NULL;
	if (!conn || !path || !path[0] || !pAFCDirectoryRead)
		return -1;
	if (!OpenAfcDir(conn, path, &dir) || !dir)
		return -1;
	int cap=32;
	int n=0;
	char** names=(char**)calloc((size_t)cap, sizeof(char*));
	if (!names) {
		if (pAFCDirectoryClose) {
			__try { pAFCDirectoryClose(conn, dir); } __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		return -1;
	}
	for (;;) {
		char* name=NULL;
		BOOL got=FALSE;
		__try {
			got=(pAFCDirectoryRead(conn, dir, &name)==0 && name && name[0]);
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			got=FALSE;
			name=NULL;
		}
		if (!got)
			break;
		if (!strcmp(name, ".") || !strcmp(name, ".."))
			continue;
		if (n>=16384)
			break;
		if (n>=cap) {
			int ncap=cap*2;
			if (ncap>16384)
				ncap=16384;
			void* grow=realloc(names, (size_t)ncap*sizeof(char*));
			if (!grow)
				break;
			names=(char**)grow;
			cap=ncap;
		}
		names[n]=_strdup(name);
		if (!names[n])
			break;
		n++;
	}
	if (pAFCDirectoryClose) {
		__try { pAFCDirectoryClose(conn, dir); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
	if (outNames)
		*outNames=names;
	else {
		for (int i=0;i<n;i++)
			free(names[i]);
		free(names);
	}
	return n;
}

static void FreeAfcNames(char** names, int n)
{
	if (!names)
		return;
	for (int i=0;i<n;i++)
		free(names[i]);
	free(names);
}

static BOOL OpenAfcFromHouse(void* service, int sock, afc_connection* out)
{
	*out=NULL;
	if (service && AfcOpenAny(service, out) && *out) {
		int n=ReadAfcNames(*out, ".", NULL);
		if (n>=0)
			return TRUE;
		AfcConnDiscard(*out, NULL);
		*out=NULL;
	}
	if (sock && AfcOpenAny((void*)(intptr_t)sock, out) && *out)
		return TRUE;
	return FALSE;
}

struct AfcBest {
	afc_connection conn;
	int sock;
	int score;
	BOOL docsOnly;
	char root[40];
	char how[32];
};

static void ConsiderAfc(AfcBest* best, afc_connection conn, int sock, const char* how, const char* bundle)
{
	int nRoot=ReadAfcNames(conn, ".", NULL);
	int nDocs=ReadAfcNames(conn, "Documents", NULL);
	int score=nRoot;
	BOOL docsOnly=TRUE;
	const char* root="";
	if (nDocs>score) {
		score=nDocs;
		docsOnly=FALSE;
		root="Documents";
	}
	char line[192];
	_snprintf_s(line, countof(line), _TRUNCATE, "afc %s %s root=%d docs=%d",
		how ? how : "?", bundle ? bundle : "?", nRoot, nDocs);
	MdLog(line);
	if (score<0 || (best->how[0] && score<=best->score)) {
		AfcConnDiscard(conn, &sock);
		return;
	}
	if (best->conn)
		AfcConnDiscard(best->conn, &best->sock);
	best->conn=conn;
	best->sock=sock;
	best->score=score;
	best->docsOnly=docsOnly;
	AfcCopyStr(best->root, countof(best->root), root);
	AfcCopyStr(best->how, countof(best->how), how ? how : "");
}

static BOOL SvcSendAll(void* svc, const void* data, int n)
{
	const char* p=(const char*)data;
	while (n>0) {
		int r=-1;
		__try { r=pAMDServiceConnectionSend(svc, p, (size_t)n); }
		__except(EXCEPTION_EXECUTE_HANDLER) { r=-1; }
		if (r<=0)
			return FALSE;
		if (r>n)
			return FALSE;
		p+=r;
		n-=r;
	}
	return TRUE;
}

static BOOL SvcRecvAll(void* svc, void* data, int n)
{
	char* p=(char*)data;
	while (n>0) {
		int r=-1;
		__try { r=pAMDServiceConnectionReceive(svc, p, (size_t)n); }
		__except(EXCEPTION_EXECUTE_HANDLER) { r=-1; }
		if (r<=0)
			return FALSE;
		if (r>n)
			return FALSE;
		p+=r;
		n-=r;
	}
	return TRUE;
}

static void SvcInvalidate(void* svc)
{
	if (svc && pAMDServiceConnectionInvalidate) {
		__try { pAMDServiceConnectionInvalidate(svc); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
}

static CFTypeRef SvcRecvPlist(void* svc)
{
	unsigned int be=0;
	if (!SvcRecvAll(svc, &be, 4))
		return NULL;
	unsigned int n=ntohl(be);
	if (n==0 || n>1024*1024)
		return NULL;
	char* buf=(char*)malloc((size_t)n+1);
	if (!buf)
		return NULL;
	if (!SvcRecvAll(svc, buf, (int)n)) {
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

static BOOL SvcVendCommand(void* svc, const char* command, const char* bundle)
{
	char xml[1024];
	if (_snprintf_s(xml, countof(xml), _TRUNCATE,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
		"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
		"<plist version=\"1.0\"><dict>"
		"<key>Command</key><string>%s</string>"
		"<key>Identifier</key><string>%s</string>"
		"</dict></plist>", command, bundle)<0)
		return FALSE;
	unsigned int be=htonl((unsigned int)strlen(xml));
	if (!SvcSendAll(svc, &be, 4) || !SvcSendAll(svc, xml, (int)strlen(xml))) {
		MdLog("secure send fail");
		return FALSE;
	}
	CFTypeRef pl=SvcRecvPlist(svc);
	WCHAR err[80], status[80];
	err[0]=0;
	status[0]=0;
	if (pl) {
		DictStr(pl, "Error", err, 80);
		DictStr(pl, "Status", status, 80);
		pCFRelease(pl);
	}
	char note[220];
	_snprintf_s(note, countof(note), _TRUNCATE, "secure %s %s err=%ls status=%ls",
		command, bundle, err, status);
	MdLog(note);
	return err[0]==0 && (
		_wcsicmp(status, L"Complete")==0 ||
		_wcsicmp(status, L"Success")==0 ||
		_wcsicmp(status, L"Ready")==0);
}

static BOOL SecureVendAfc(ApplePhone* p, const char* bundle, const char* command, afc_connection* outAfc, void** outSvc)
{
	*outAfc=NULL;
	*outSvc=NULL;
	if (!p || !pAMDeviceSecureStartService || !pAMDServiceConnectionSend || !pAMDServiceConnectionReceive || !pAFCConnectionOpen)
		return FALSE;
	CFStringRef name=CfStr("com.apple.mobile.house_arrest");
	if (!name)
		return FALSE;
	void* svc=NULL;
	mach_error_t err=-1;
	__try { err=pAMDeviceSecureStartService(p->dev, name, NULL, &svc); }
	__except(EXCEPTION_EXECUTE_HANDLER) { err=-1; svc=NULL; }
	pCFRelease(name);
	char line[160];
	_snprintf_s(line, countof(line), _TRUNCATE, "secure start %s err=%d", command, (int)err);
	MdLog(line);
	if (err!=0 || !svc)
		return FALSE;
	if (!SvcVendCommand(svc, command, bundle)) {
		SvcInvalidate(svc);
		return FALSE;
	}
	afc_connection afc=NULL;
	int openErr=-1;
	__try { openErr=pAFCConnectionOpen(svc, 0, &afc); }
	__except(EXCEPTION_EXECUTE_HANDLER) { openErr=-1; afc=NULL; }
	if (openErr!=0 || !afc) {
		int s=pAMDServiceConnectionGetSocket ? pAMDServiceConnectionGetSocket(svc) : 0;
		afc=NULL;
		if (s) {
			__try { openErr=pAFCConnectionOpen((void*)(intptr_t)s, 0, &afc); }
			__except(EXCEPTION_EXECUTE_HANDLER) { openErr=-1; afc=NULL; }
		}
		_snprintf_s(line, countof(line), _TRUNCATE, "afc open sock=%d err=%d", s, openErr);
	} else {
		_snprintf_s(line, countof(line), _TRUNCATE, "afc open svc err=%d", openErr);
	}
	MdLog(line);
	if (!afc) {
		SvcInvalidate(svc);
		return FALSE;
	}
	void* ssl=NULL;
	if (pAMDServiceConnectionGetSecureIOContext) {
		__try { ssl=pAMDServiceConnectionGetSecureIOContext(svc); }
		__except(EXCEPTION_EXECUTE_HANDLER) { ssl=NULL; }
	}
	if (ssl && pAFCConnectionSetSecureContext) {
		__try { pAFCConnectionSetSecureContext(afc, ssl); }
		__except(EXCEPTION_EXECUTE_HANDLER) {}
	}
	*outAfc=afc;
	*outSvc=svc;
	return TRUE;
}

static BOOL UseSecureAfc(ApplePhone* p, const char* bundle, const char* command, afc_connection* outConn, int* outSock, BOOL* docsOnly, char* rootOut, int rootCch)
{
	afc_connection conn=NULL;
	void* svc=NULL;
	if (!SecureVendAfc(p, bundle, command, &conn, &svc))
		return FALSE;
	AfcBest best;
	memset(&best, 0, sizeof(best));
	best.score=-1;
	ConsiderAfc(&best, conn, 0, command, bundle);
	if (!best.conn || best.score<=0) {
		if (best.conn)
			AfcConnDiscard(best.conn, &best.sock);
		SvcInvalidate(svc);
		return FALSE;
	}
	p->appSvc=svc;
	*outConn=best.conn;
	*outSock=0;
	if (docsOnly)
		*docsOnly=best.docsOnly;
	if (rootOut && rootCch>0)
		AfcCopyStr(rootOut, (size_t)rootCch, best.root);
	char line[192];
	_snprintf_s(line, countof(line), _TRUNCATE, "afc use %s %s score=%d root=%s docsOnly=%d",
		command, bundle, best.score, best.root, best.docsOnly ? 1 : 0);
	MdLog(line);
	return TRUE;
}

static BOOL VendHouseArrestAfc(ApplePhone* p, const char* bundle, const char* command, afc_connection* outConn, int* outSock)
{
	*outConn=NULL;
	*outSock=0;
	int sock=0;
	void* service=NULL;
	if (!HouseArrest(p, bundle, command, &sock, &service) || !sock)
		return FALSE;
	afc_connection conn=NULL;
	if (!OpenAfcFromHouse(service, sock, &conn) || !conn) {
		CloseAppleSock(&sock);
		return FALSE;
	}
	*outConn=conn;
	*outSock=sock;
	return TRUE;
}

static BOOL AppDocumentsOpen(ApplePhone* p, const char* bundle, afc_connection* outConn, int* outSock, BOOL* docsOnly, char* rootOut, int rootCch, BOOL skipDocuments)
{
	*outConn=NULL;
	*outSock=0;
	if (docsOnly)
		*docsOnly=FALSE;
	if (rootOut && rootCch>0)
		rootOut[0]=0;
	if (!p || !bundle || !bundle[0] || !EnsureLockdown(p))
		return FALSE;
	UNREFERENCED_PARAMETER(skipDocuments);

	/* The house_arrest socket from SecureStart is TLS. Raw send/recv never
	   gets a reply; AMDServiceConnectionSend does. */
	if (UseSecureAfc(p, bundle, "VendDocuments", outConn, outSock, docsOnly, rootOut, rootCch))
		return TRUE;
	if (UseSecureAfc(p, bundle, "VendContainer", outConn, outSock, docsOnly, rootOut, rootCch))
		return TRUE;

	AfcBest best;
	memset(&best, 0, sizeof(best));
	best.score=-1;
	char fallback[32];
	char fallbackRoot[40];
	fallback[0]=0;
	fallbackRoot[0]=0;
	int fallbackScore=-1;
	BOOL fallbackDocs=FALSE;

	/* Create/Start speak AFC directly. Raw VendDocuments on this phone
	   gets no reply and was stalling every folder for many seconds. */
	afc_connection conn=NULL;
	if (CreateHouseArrestConn(p, bundle, &conn) && conn) {
		ConsiderAfc(&best, conn, 0, "Create", bundle);
	} else {
		MdLog("afc Create open fail");
	}
	if (!(best.score>0 && best.conn)) {
		if (best.score>=0 && best.how[0]) {
			AfcCopyStr(fallback, countof(fallback), best.how);
			AfcCopyStr(fallbackRoot, countof(fallbackRoot), best.root);
			fallbackScore=best.score;
			fallbackDocs=best.docsOnly;
		}
		if (best.conn) {
			AfcConnDiscard(best.conn, &best.sock);
			best.conn=NULL;
		}
		int sock=0;
		conn=NULL;
		if (StartHouseArrestSockAfc(p, bundle, &conn, &sock) && conn)
			ConsiderAfc(&best, conn, sock, "Start", bundle);
		else
			MdLog("afc Start open fail");
		if (!(best.score>0 && best.conn)) {
			if (best.score>fallbackScore && best.how[0]) {
				AfcCopyStr(fallback, countof(fallback), best.how);
				AfcCopyStr(fallbackRoot, countof(fallbackRoot), best.root);
				fallbackScore=best.score;
				fallbackDocs=best.docsOnly;
			}
			if (best.conn) {
				AfcConnDiscard(best.conn, &best.sock);
				best.conn=NULL;
			}
			if (fallbackScore<0) {
				sock=0;
				conn=NULL;
				if (VendHouseArrestAfc(p, bundle, "VendDocuments", &conn, &sock))
					ConsiderAfc(&best, conn, sock, "VendDocuments", bundle);
			} else {
				conn=NULL;
				int sock2=0;
				BOOL reopened=FALSE;
				if (!strcmp(fallback, "Create"))
					reopened=CreateHouseArrestConn(p, bundle, &conn);
				else if (!strcmp(fallback, "Start"))
					reopened=StartHouseArrestSockAfc(p, bundle, &conn, &sock2);
				if (reopened && conn) {
					best.conn=conn;
					best.sock=sock2;
					best.score=fallbackScore;
					best.docsOnly=fallbackDocs;
					AfcCopyStr(best.root, countof(best.root), fallbackRoot);
					AfcCopyStr(best.how, countof(best.how), fallback);
				}
			}
		}
	}
	if (!best.conn) {
		MdLog("afc none");
		return FALSE;
	}
	char line[192];
	_snprintf_s(line, countof(line), _TRUNCATE, "afc use %s %s score=%d root=%s docsOnly=%d",
		best.how, bundle, best.score, best.root, best.docsOnly ? 1 : 0);
	MdLog(line);
	*outConn=best.conn;
	*outSock=best.sock;
	if (docsOnly)
		*docsOnly=best.docsOnly;
	if (rootOut && rootCch>0)
		AfcCopyStr(rootOut, (size_t)rootCch, best.root);
	return TRUE;
}

static BOOL EnsureAppAfc(ApplePhone* p, LPCWSTR appName, BOOL preferDocuments, BOOL skipDocuments)
{
	if (!p)
		return FALSE;
	if (p->napps==0)
		RefreshApps(p);
	AppleApp* a=FindAppByName(p, appName);
	if (!a)
		return FALSE;
	if (p->appMiss[0] && _stricmp(p->appMiss, a->bundle)==0 &&
		GetTickCount()-p->appMissTick<2000)
		return FALSE;
	if (p->appAfc && _stricmp(p->appBundle, a->bundle)==0)
		return TRUE;
	if (p->appXfer)
		return p->appAfc!=NULL;
	CloseAppAfc(p);
	UNREFERENCED_PARAMETER(preferDocuments);
	UNREFERENCED_PARAMETER(skipDocuments);
	afc_connection conn=NULL;
	int sock=0;
	BOOL docsOnly=FALSE;
	char root[40];
	root[0]=0;
	if (!AppDocumentsOpen(p, a->bundle, &conn, &sock, &docsOnly, root, (int)sizeof(root), skipDocuments)) {
		AfcCopyStr(p->appMiss, countof(p->appMiss), a->bundle);
		p->appMissTick=GetTickCount();
		return FALSE;
	}
	p->appMiss[0]=0;
	p->appAfc=conn;
	p->appSock=sock;
	p->appDocsOnly=docsOnly;
	strcpy_s(p->appBundle, a->bundle);
	AfcCopyStr(p->appRoot, countof(p->appRoot), root);
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
	afc_connection conn=NULL;
	BOOL ok=FALSE;
	if (!j->cancel && pAMDeviceSecureStartService) {
		CFStringRef name=CfStr("com.apple.crashreportcopymobile");
		void* svc=NULL;
		if (name && pAMDeviceSecureStartService(j->p->dev, name, NULL, &svc)==0 && svc) {
			if (AfcOpenAny(svc, &conn))
				ok=TRUE;
			else if (pAMDServiceConnectionGetSocket) {
				int s=pAMDServiceConnectionGetSocket(svc);
				if (s)
					ok=AfcOpenAny((void*)(intptr_t)(unsigned)s, &conn);
			}
		}
		if (name)
			pCFRelease(name);
	}
	if (!ok && !j->cancel) {
		int sock=0;
		if (StartNamedService(j->p, "com.apple.crashreportcopymobile", &sock) && sock)
			ok=AfcOpenAny((void*)(intptr_t)(unsigned)sock, &conn);
		if (!ok)
			CloseAppleSock(&sock);
	}
	if (j->cancel) {
		if (conn && pAFCConnectionClose) {
			__try { pAFCConnectionClose(conn); } __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		if (j->done)
			SetEvent(j->done);
		return 0;
	}
	int nent=0;
	WIN32_FIND_DATAW tmp[APPLE_PANIC_MAX];
	memset(tmp, 0, sizeof(tmp));
	if (ok && conn) {
		afc_directory dir=NULL;
		if (OpenAfcDir(conn, ".", &dir) || OpenAfcDir(conn, "/", &dir) || OpenAfcDir(conn, "Panics", &dir)) {
			for (;;) {
				char* name=NULL;
				BOOL got=FALSE;
				/* Do not add C++ objects with destructors in this __try: SEH will skip them. */
				__try {
					got=(pAFCDirectoryRead(conn, dir, &name)==0 && name && name[0]);
				} __except(EXCEPTION_EXECUTE_HANDLER) {
					got=FALSE;
					name=NULL;
				}
				if (!got)
					break;
				if (!strcmp(name, ".") || !strcmp(name, ".."))
					continue;
				if (nent>=APPLE_PANIC_MAX)
					break;
				FillFindFromAfc(conn, ".", name, &tmp[nent]);
				nent++;
			}
			if (dir && pAFCDirectoryClose) {
				__try { pAFCDirectoryClose(conn, dir); } __except(EXCEPTION_EXECUTE_HANDLER) {}
			}
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
	AppleLock();
	j->p->panicAfc=ok ? conn : NULL;
	j->p->panicSock=0;
	j->p->nPanicEnt=nent;
	j->p->panicListed=TRUE;
	if (nent>0)
		memcpy(j->p->panicEnt, tmp, nent*sizeof(tmp[0]));
	AppleUnlock();
	j->ok=ok && nent>=0;
	SetEvent(j->done);
	return 0;
}

static BOOL EnsurePanicAfc(ApplePhone* p)
{
	if (!p)
		return FALSE;
	if (p->panicListed && p->nPanicEnt>0)
		return TRUE;
	if (p->panicListed && p->nPanicEnt==0)
		return FALSE;
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
		/* Do not set panicListed on timeout — allow retry. */
		DWORD w2=WaitForSingleObject(j->done, 2000);
		if (w2==WAIT_OBJECT_0) {
			WaitForSingleObject(th, 1000);
			CloseHandle(th);
			if (j->done) CloseHandle(j->done);
			free(j);
		} else {
			/* Thread still running: leak job intentionally to avoid UAF. */
			CloseHandle(th);
		}
		return FALSE;
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
	if (!out || cch<=0)
		return;
	if (rel && AfcPathHasDotDot(rel)) {
		out[0]=0;
		return;
	}
	if (!rel || !rel[0] || !strcmp(rel, "/") || !strcmp(rel, ".")) {
		if (prefix && prefix[0])
			AfcCopyStr(out, (size_t)cch, prefix);
		else
			out[0]=0;
		return;
	}
	if (prefix && prefix[0]) {
		if (prefix[0]=='/' && prefix[1]==0)
			AfcJoinSlash(out, (size_t)cch, "", rel);
		else
			AfcJoinSlash(out, (size_t)cch, prefix, rel);
	} else
		AfcCopyStr(out, (size_t)cch, rel);
}

static void AppAfcPath(const char* root, const char* rel, char* out, int cch)
{
	if (!out || cch<=0)
		return;
	out[0]=0;
	if (rel && AfcPathHasDotDot(rel))
		return;
	while (rel && *rel=='/')
		rel++;
	if (!rel || !rel[0] || !strcmp(rel, ".")) {
		if (root && root[0])
			AfcCopyStr(out, (size_t)cch, root);
		else
			AfcCopyStr(out, (size_t)cch, ".");
		return;
	}
	if (root && root[0] && _stricmp(rel, "Documents")!=0 && _strnicmp(rel, "Documents/", 10)!=0)
		AfcJoinSlash(out, (size_t)cch, root, rel);
	else
		AfcCopyStr(out, (size_t)cch, rel);
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
		if (!EnsureAppAfc(p, app, FALSE, FALSE) || !p->appAfc)
			return FALSE;
		*conn=p->appAfc;
		AppAfcPath(p->appRoot, relAfc, path, pathcch);
		return path[0]!=0;
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

static int SnapshotAfcDir(afc_connection conn, const char* path, WIN32_FIND_DATAW** out)
{
	*out=NULL;
	if (!conn || !path || !path[0])
		return -1;
	char** names=NULL;
	int n=ReadAfcNames(conn, path, &names);
	char opened[1024];
	AfcCopyStr(opened, sizeof(opened), (strcmp(path, "/")==0 || strcmp(path, ".")==0) ? "." : path);
	if (n<=0 && strcmp(path, "Documents")==0) {
		FreeAfcNames(names, n>0 ? n : 0);
		names=NULL;
		n=ReadAfcNames(conn, ".", &names);
		AfcCopyStr(opened, sizeof(opened), ".");
	}
	if (n<=0) {
		FreeAfcNames(names, n>0 ? n : 0);
		return n;
	}
	WIN32_FIND_DATAW* ents=(WIN32_FIND_DATAW*)calloc((size_t)n, sizeof(WIN32_FIND_DATAW));
	if (!ents) {
		FreeAfcNames(names, n);
		return -1;
	}
	/* Directory is closed. Stat afterwards so FileInfo cannot cut the listing short. */
	for (int i=0;i<n;i++)
		FillFindFromAfc(conn, opened, names[i], &ents[i]);
	FreeAfcNames(names, n);
	*out=ents;
	return n;
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

	int phoneIdx=(int)(p-g_phones);
	if (kind==AR_ROOT) {
		f=NewFind(phoneIdx, AFK_ROOT);
		f->index=1;
		FillDirFd(APPLE_PHOTOS, fd);
		*out=(HANDLE)f;
		AppleUnlock();
		return TRUE;
	}

	/* Never call Apple StartService / AFCDirectoryOpen while holding AppleLock:
	   CFRunLoop callbacks need the same lock. */
	AppleUnlock();

	if (kind==AR_APPS) {
		RefreshApps(&g_phones[phoneIdx]);
		AppleLock();
		p=&g_phones[phoneIdx];
		if (p->napps==0) {
			AppleUnlock();
			return FALSE;
		}
		f=NewFind(phoneIdx, AFK_APPS);
		f->index=1;
		FillDirFd(p->apps[0].name, fd);
		*out=(HANDLE)f;
		AppleUnlock();
		return TRUE;
	}
	if (kind==AR_PANICS) {
		EnsurePanicAfc(&g_phones[phoneIdx]);
		AppleLock();
		p=&g_phones[phoneIdx];
		if (p->nPanicEnt<=0) {
			AppleUnlock();
			return FALSE;
		}
		f=NewFind(phoneIdx, AFK_PANIC_CACHE);
		f->index=1;
		*fd=p->panicEnt[0];
		*out=(HANDLE)f;
		AppleUnlock();
		return TRUE;
	}

	afc_connection conn=NULL;
	if (!ResolveAfc(&g_phones[phoneIdx], relPath, &conn, afcPath, 1024, FALSE) || !conn)
		return FALSE;
	/* Read every name and close the directory before returning. Total Commander
	   keeps the find handle while F5 runs; an open AFC directory blocks the write. */
	WIN32_FIND_DATAW* ents=NULL;
	int nent=SnapshotAfcDir(conn, afcPath, &ents);
	if (nent<=0) {
		free(ents);
		return FALSE;
	}
	AppleLock();
	f=NewFind(phoneIdx, AFK_AFC);
	if (!f) {
		free(ents);
		AppleUnlock();
		return FALSE;
	}
	f->ents=ents;
	f->nent=nent;
	f->index=1;
	strcpy_s(f->afcPath, afcPath);
	*fd=ents[0];
	*out=(HANDLE)f;
	AppleUnlock();
	return TRUE;
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
	if (f->kind==AFK_PANIC_CACHE) {
		if (f->index>=p->nPanicEnt) {
			AppleUnlock();
			return FALSE;
		}
		*fd=p->panicEnt[f->index];
		f->index++;
		AppleUnlock();
		return TRUE;
	}
	if (f->ents) {
		if (f->index>=f->nent) {
			AppleUnlock();
			return FALSE;
		}
		*fd=f->ents[f->index];
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
	if (f->ents)
		free(f->ents);
	f->ents=NULL;
	f->magic=0;
	free(f);
	AppleUnlock();
}

int AppleMdGetFile(LPCWSTR deviceName, LPCWSTR relPath, LPCWSTR localPath, ULONGLONG totalHint, FILETIME* mtime)
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
	AppleUnlock(); /* unlock around I/O chunks */

	WCHAR remoteDisp[wdirtypemax];
	wcslcpy(remoteDisp, L"\\", wdirtypemax-1);
	wcslcat(remoteDisp, deviceName ? deviceName : L"", wdirtypemax-1);
	if (relPath && relPath[0]) {
		wcslcat(remoteDisp, L"\\", wdirtypemax-1);
		wcslcat(remoteDisp, relPath, wdirtypemax-1);
	}

	char buf[64*1024];
	int result=FS_FILE_OK;
	ULONGLONG totalcopied=0;
	DWORD lasttime=GetTickCount();
	for (;;) {
		size_t n=sizeof(buf);
		if (pAFCFileRefRead(conn, ref, buf, &n)!=0) {
			result=FS_FILE_READERROR;
			break;
		}
		if (n==0)
			break;
		if (n>sizeof(buf)) {
			result=FS_FILE_READERROR;
			break;
		}
		DWORD left=(DWORD)n;
		char* pbuf=buf;
		while (left>0) {
			DWORD wr=0;
			if (!WriteFile(out, pbuf, left, &wr, NULL) || wr==0) {
				result=FS_FILE_WRITEERROR;
				break;
			}
			totalcopied+=wr;
			pbuf+=wr;
			left-=wr;
		}
		if (result!=FS_FILE_OK)
			break;
		DWORD thistime=GetTickCount();
		if ((thistime-lasttime)>100) {
			lasttime=thistime;
			int percent=0;
			if (totalHint)
				percent=(int)((totalcopied*100)/totalHint);
			if (ProgressCheck(remoteDisp, (WCHAR*)localPath, percent)) {
				result=FS_FILE_USERABORT;
				break;
			}
		}
	}
	if (result==FS_FILE_OK && totalHint!=0 && totalcopied!=totalHint)
		result=FS_FILE_READERROR;

	AppleLock();
	pAFCFileRefClose(conn, ref);
	AppleUnlock();
	if (mtime && result==FS_FILE_OK &&
		!(mtime->dwHighDateTime==0xFFFFFFFF))
		SetFileTime(out, NULL, NULL, mtime);
	CloseHandle(out);
	if (result!=FS_FILE_OK)
		DeleteFileT((WCHAR*)localPath);
	return result;
}

#define AFC_DELETE_MAX_DEPTH 100

static BOOL AfcDeleteTreeDepth(afc_connection conn, const char* path, int depth)
{
	if (depth>AFC_DELETE_MAX_DEPTH)
		return FALSE;
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
				if (AfcJoinChild(child, countof(child), path, name, TRUE)<0)
					continue;
				AfcDeleteTreeDepth(conn, child, depth+1);
			}
			if (pAFCDirectoryClose)
				pAFCDirectoryClose(conn, dir);
		}
	}
	return pAFCRemovePath(conn, path)==0;
}

static BOOL AfcDeleteTree(afc_connection conn, const char* path)
{
	return AfcDeleteTreeDepth(conn, path, 0);
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

static const char* AfcBaseName(const char* path)
{
	if (!path || !path[0])
		return "";
	const char* slash=strrchr(path, '/');
	if (slash && slash[1])
		return slash+1;
	return path;
}

static BOOL AfcOpenWrite(afc_connection conn, const char* path, afc_file_ref* ref)
{
	*ref=0;
	if (!conn || !path || !path[0] || !strcmp(path, ".") || !strcmp(path, "/"))
		return FALSE;
	static const unsigned long long modes[]={3ull, 4ull, 2ull};
	for (int i=0;i<(int)(sizeof(modes)/sizeof(modes[0]));i++) {
		if (SafeAfcFileOpen(conn, path, modes[i], ref)==0 && *ref)
			return TRUE;
		*ref=0;
	}
	return FALSE;
}

static BOOL AfcPathExists(afc_connection conn, const char* path)
{
	if (!conn || !path || !path[0] || !pAFCFileInfoOpen)
		return FALSE;
	afc_dictionary dict=NULL;
	int err=-1;
	__try {
		err=pAFCFileInfoOpen(conn, path, &dict);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		err=-1;
		dict=NULL;
	}
	if (err!=0 || !dict)
		return FALSE;
	AfcDictClose(dict);
	return TRUE;
}

static BOOL AfcOpenWriteAny(afc_connection conn, const char* hinted, BOOL overwrite, afc_file_ref* ref, char* used, int usedcch)
{
	*ref=0;
	if (used && usedcch>0)
		used[0]=0;
	if (!conn)
		return FALSE;
	const char* name=AfcBaseName(hinted);
	if (!name[0] || strchr(name, '/') || !strcmp(name, ".") || !strcmp(name, ".."))
		return FALSE;
	while (hinted && *hinted=='/')
		hinted++;
	char pDocs[512];
	char pSlash[512];
	const char* cand[4];
	int n=0;
	if (hinted && hinted[0])
		cand[n++]=hinted;
	if (hinted && hinted[0] && AfcJoinSlash(pSlash, countof(pSlash), "", hinted))
		cand[n++]=pSlash;
	if (hinted && !strchr(hinted, '/') &&
		PathLooksLikeDir(conn, "Documents") &&
		AfcJoinSlash(pDocs, countof(pDocs), "Documents", name) &&
		strcmp(hinted, pDocs)!=0)
		cand[n++]=pDocs;
	for (int pass=0; pass<2; pass++) {
		for (int i=0;i<n;i++) {
			if (pass==1 && overwrite && AfcPathExists(conn, cand[i]) && pAFCRemovePath) {
				__try { pAFCRemovePath(conn, cand[i]); }
				__except(EXCEPTION_EXECUTE_HANDLER) {}
			}
			if (AfcOpenWrite(conn, cand[i], ref)) {
				if (used)
					AfcCopyStr(used, (size_t)usedcch, cand[i]);
				return TRUE;
			}
		}
		if (!overwrite)
			break;
	}
	return FALSE;
}

static int AppleMdPutFileWork(LPCWSTR deviceName, LPCWSTR relPath, LPCWSTR localPath, BOOL overwrite)
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
	AppleUnlock();
	/* CreateHouseArrest / StartService must run without AppleLock:
	   the device run loop takes the same lock. */
	afc_connection conn=NULL;
	if (!p || !ResolveAfc(p, relPath, &conn, afcPath, 1024, TRUE) || !conn) {
		MdLog("put no session");
		return FS_FILE_WRITEERROR;
	}
	AppleLock();
	p=FindPhoneByName(deviceName);
	if (!p) {
		AppleUnlock();
		return FS_FILE_WRITEERROR;
	}
	if (!overwrite && pAFCFileInfoOpen) {
		afc_dictionary dict=NULL;
		int infoErr=-1;
		__try { infoErr=pAFCFileInfoOpen(conn, afcPath, &dict); }
		__except(EXCEPTION_EXECUTE_HANDLER) { infoErr=-1; dict=NULL; }
		if (infoErr==0 && dict) {
			AfcDictClose(dict);
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
	char usedPath[1024];
	usedPath[0]=0;
	if (!AfcOpenWriteAny(conn, afcPath, overwrite, &ref, usedPath, 1024)) {
		char line[300];
		_snprintf_s(line, countof(line), _TRUNCATE, "put fail %s", afcPath);
		MdLog(line);
		CloseHandle(in);
		AppleUnlock();
		return FS_FILE_WRITEERROR;
	}
	if (usedPath[0])
		strcpy_s(afcPath, usedPath);
	p->appXfer++;

	WCHAR remoteDisp[wdirtypemax];
	wcslcpy(remoteDisp, L"\\", wdirtypemax-1);
	wcslcat(remoteDisp, deviceName ? deviceName : L"", wdirtypemax-1);
	if (relPath && relPath[0]) {
		wcslcat(remoteDisp, L"\\", wdirtypemax-1);
		wcslcat(remoteDisp, relPath, wdirtypemax-1);
	}

	DWORD sizeHigh=0;
	DWORD sizeLow=GetFileSize(in, &sizeHigh);
	ULONGLONG totalsize=((ULONGLONG)sizeHigh<<32) | (ULONGLONG)sizeLow;
	ULONGLONG totalcopied=0;
	DWORD lasttime=GetTickCount();
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
		if (rd>sizeof(buf)) {
			result=FS_FILE_READERROR;
			break;
		}
		int werr=SafeAfcFileWrite(conn, ref, buf, (size_t)rd);
		if (werr!=0) {
			char line[80];
			_snprintf_s(line, countof(line), _TRUNCATE, "put write err=%d n=%lu", werr, (unsigned long)rd);
			MdLog(line);
			result=FS_FILE_WRITEERROR;
			break;
		}
		totalcopied+=rd;
		DWORD thistime=GetTickCount();
		if ((thistime-lasttime)>100) {
			lasttime=thistime;
			int percent=0;
			if (totalsize)
				percent=(int)((totalcopied*100)/totalsize);
			if (ProgressCheck((WCHAR*)localPath, remoteDisp, percent)) {
				result=FS_FILE_USERABORT;
				break;
			}
		}
	}
	if (result==FS_FILE_OK && totalsize!=0 && totalcopied!=totalsize)
		result=FS_FILE_WRITEERROR;

	SafeAfcFileClose(conn, ref);
	if (result!=FS_FILE_OK && totalcopied==0 && afcPath[0] && pAFCRemovePath) {
		__try { pAFCRemovePath(conn, afcPath); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
	p->appXfer--;
	AppleUnlock();
	CloseHandle(in);
	return result;
}

int AppleMdPutFile(LPCWSTR deviceName, LPCWSTR relPath, LPCWSTR localPath, BOOL overwrite)
{
	int r=FS_FILE_WRITEERROR;
	__try {
		r=AppleMdPutFileWork(deviceName, relPath, localPath, overwrite);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		r=FS_FILE_WRITEERROR;
	}
	return r;
}
