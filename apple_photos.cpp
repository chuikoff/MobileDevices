#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "apple_photos.h"

/* PhotoData/Photos.sqlite (Core Data) over com.apple.afc.
   ZDATECREATED is seconds since 2001-01-01 UTC. ZKINDSUBTYPE is Apple's
   internal enum (1 panorama, 2 live, 10 screenshot), not PHAssetMediaSubtype.
   ZDIRECTORY is "100APPLE" on older libraries and "DCIM/100APPLE" on current
   ones. Album rows are ZGENERICALBUM ZKIND 2 (album) and 1505 (shared).
   Membership is Z_<n>ASSETS with columns Z_<digits>ALBUMS and Z_<digits>ASSETS.
   Thumbnails are PhotoData/Thumbnails/V2/<asset path>/5003.JPG (else 5005.JPG).
   The -wal is copied beside the db; SQLite rebuilds the missing -shm on open.
   If the library cannot be read, DCIM is flattened into All Photos and
   dot-folders are skipped. Synthetic YYYY-MM date folders are not listed.
   A failed library is not downloaded again until Photos.sqlite changes. */

enum {
	AL_USER=0,
	AL_ALL=1,
	AL_FAV=2,
	AL_SCREEN=3,
	AL_SELFIE=4,
	AL_LIVE=5,
	AL_PANO=6,
	AL_PORT=7,
	AL_VIDEO=8,
	AL_SLOMO=9,
	AL_BURST=10,
	AL_HIDDEN=11,
	AL_TRASH=12,
	AL_MONTH=13
};

enum {
	PF_VIDEO=1u,
	PF_FAV=2u,
	PF_HIDDEN=4u,
	PF_TRASH=8u,
	PF_SCREEN=16u,
	PF_SELFIE=32u,
	PF_LIVE=64u,
	PF_PANO=128u,
	PF_SLOMO=256u,
	PF_BURST=512u,
	PF_PORT=1024u,
	PF_SKIP=2048u
};

#define SQL_OK 0
#define SQL_ROW 100
#define SQL_OPEN_READWRITE 0x00000002
#define SQL_OPEN_CREATE 0x00000004
#define SQL_NULL 5
#define PHOTO_ASSET_MAX 250000
#define PHOTO_ALBUM_MAX 4096

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

typedef int (*t_sql_open)(const char*, sqlite3**, int, const char*);
typedef int (*t_sql_close)(sqlite3*);
typedef int (*t_sql_exec)(sqlite3*, const char*, int(*)(void*,int,char**,char**), void*, char**);
typedef void (*t_sql_free)(void*);
typedef int (*t_sql_prepare)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
typedef int (*t_sql_step)(sqlite3_stmt*);
typedef int (*t_sql_finalize)(sqlite3_stmt*);
typedef int (*t_sql_colcount)(sqlite3_stmt*);
typedef int (*t_sql_coltype)(sqlite3_stmt*, int);
typedef const char* (*t_sql_colname)(sqlite3_stmt*, int);
typedef const unsigned char* (*t_sql_text)(sqlite3_stmt*, int);
typedef int (*t_sql_int)(sqlite3_stmt*, int);
typedef long long (*t_sql_int64)(sqlite3_stmt*, int);
typedef double (*t_sql_double)(sqlite3_stmt*, int);
typedef const char* (*t_sql_errmsg)(sqlite3*);

static t_sql_open pSqlOpen;
static t_sql_close pSqlClose;
static t_sql_exec pSqlExec;
static t_sql_free pSqlFree;
static t_sql_prepare pSqlPrepare;
static t_sql_step pSqlStep;
static t_sql_finalize pSqlFinalize;
static t_sql_colcount pSqlColCount;
static t_sql_coltype pSqlColType;
static t_sql_colname pSqlColName;
static t_sql_text pSqlText;
static t_sql_int pSqlInt;
static t_sql_int64 pSqlInt64;
static t_sql_double pSqlDouble;
static t_sql_errmsg pSqlErrmsg;
static HMODULE g_sqlite=NULL;
static int g_sqliteReady=0;
static int g_sqliteFailed=0;

struct PhotoAsset {
	unsigned flags;
	ULONGLONG size;
	FILETIME when;
	char* path;
	WCHAR* display;
};

/* Open-addressed set of AFC paths. Slots point at strings owned elsewhere.
   strcmp, not case-fold: a tombstone is an exact remote path. */
struct PathSet {
	char** key;
	unsigned char* used;
	int cap;
	int n;
	int failed;
};

/* Open-addressed set of display names. Built while an album is filled
   and dropped afterwards. Equal names share a sort key, so the hash
   matches WcsIcmp. */
struct NameSet {
	WCHAR** key;
	unsigned char* used;
	int cap;
	int n;
	int failed;
};

struct PhotoAlbum {
	int smart;
	WCHAR* name;
	int* ids;
	WCHAR** names;
	int n;
	int cap;
	FILETIME newest;
	NameSet nameset;
};

struct PhotoLib {
	WCHAR key[80];
	int russian;
	int fromSqlite;
	int sqlFailed;
	ULONGLONG sqlSize, sqlMtime, walSize, walMtime;
	DWORD builtTick;
	ULONGLONG failSize, failMtime, failWalSize, failWalMtime;
	ULONGLONG dcimSig;
	DWORD sqlFailTick;
	int nassets, assetCap;
	PhotoAsset* assets;
	int nalbums, albumCap;
	PhotoAlbum* albums;
	int ntomb, tombCap;
	char** tombs;
	PathSet tombset;
	char** thumbMiss;
	int nThumbMiss, thumbMissCap;
};

static AppleAfcIo g_io;
static CRITICAL_SECTION g_cs;
static volatile LONG g_csState=0;
static CRITICAL_SECTION g_ioCs;
static volatile LONG g_ioCsState=0;
static HANDLE g_buildEvent=NULL;
static volatile LONG g_buildBusy=0;
static WCHAR g_buildKey[80];
static PhotoLib* g_libs[8];
static int g_nlibs=0;

static void Log(const char* line)
{
	if (g_io.log && line)
		g_io.log(line);
}

static void PhLock(void)
{
	for (;;) {
		LONG s=g_csState;
		if (s==2) {
			EnterCriticalSection(&g_cs);
			return;
		}
		if (s==0 && InterlockedCompareExchange(&g_csState, 1, 0)==0) {
			InitializeCriticalSection(&g_cs);
			InterlockedExchange(&g_csState, 2);
			EnterCriticalSection(&g_cs);
			return;
		}
		Sleep(0);
	}
}

static void PhUnlock(void)
{
	if (g_csState==2)
		LeaveCriticalSection(&g_cs);
}

static void IoLock(void)
{
	for (;;) {
		LONG s=g_ioCsState;
		if (s==2) {
			EnterCriticalSection(&g_ioCs);
			return;
		}
		if (s==0 && InterlockedCompareExchange(&g_ioCsState, 1, 0)==0) {
			InitializeCriticalSection(&g_ioCs);
			InterlockedExchange(&g_ioCsState, 2);
			EnterCriticalSection(&g_ioCs);
			return;
		}
		Sleep(0);
	}
}

static void IoUnlock(void)
{
	if (g_ioCsState==2)
		LeaveCriticalSection(&g_ioCs);
}

static void WcsCopy(WCHAR* dst, int cch, const WCHAR* src);

/* Called with PhLock held. Manual-reset event starts nonsignaled;
   waiters block only while g_buildBusy is set. */
static void BuildArm(const WCHAR* udid)
{
	if (!g_buildEvent)
		g_buildEvent=CreateEventW(NULL, TRUE, FALSE, NULL);
	g_buildBusy=1;
	WcsCopy(g_buildKey, 80, udid ? udid : L"");
	if (g_buildEvent)
		ResetEvent(g_buildEvent);
}

static void BuildSignal(void)
{
	g_buildBusy=0;
	g_buildKey[0]=0;
	if (g_buildEvent)
		SetEvent(g_buildEvent);
}

static void BuildWait(void)
{
	HANDLE ev=g_buildEvent;
	PhUnlock();
	if (ev)
		WaitForSingleObject(ev, INFINITE);
	else
		Sleep(30);
}

static void WcsCopy(WCHAR* dst, int cch, const WCHAR* src)
{
	if (!dst || cch<=0)
		return;
	if (!src)
		src=L"";
	int i=0;
	for (; i<cch-1 && src[i]; i++)
		dst[i]=src[i];
	dst[i]=0;
}

static int WcsIcmp(const WCHAR* a, const WCHAR* b)
{
	if (!a) a=L"";
	if (!b) b=L"";
	int r=CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, a, -1, b, -1);
	if (r==0)
		return _wcsicmp(a, b);
	return r-2;
}

static char* StrDup(const char* s)
{
	if (!s)
		s="";
	size_t n=strlen(s);
	char* d=(char*)malloc(n+1);
	if (!d)
		return NULL;
	memcpy(d, s, n+1);
	return d;
}

static WCHAR* WcsDup(const WCHAR* s)
{
	if (!s)
		s=L"";
	size_t n=wcslen(s);
	WCHAR* d=(WCHAR*)malloc((n+1)*sizeof(WCHAR));
	if (!d)
		return NULL;
	memcpy(d, s, (n+1)*sizeof(WCHAR));
	return d;
}

static void Utf8ToWide(const char* u, WCHAR* w, int cch)
{
	if (!w || cch<=0)
		return;
	w[0]=0;
	if (!u || !u[0])
		return;
	if (MultiByteToWideChar(CP_UTF8, 0, u, -1, w, cch)>0)
		return;
	w[cch-1]=0;
}

static BOOL WideToUtf8(const WCHAR* w, char* u, int cch)
{
	if (!u || cch<=0)
		return FALSE;
	u[0]=0;
	if (!w)
		w=L"";
	return WideCharToMultiByte(CP_UTF8, 0, w, -1, u, cch, NULL, NULL)>0;
}

static void FtNone(FILETIME* ft)
{
	ft->dwLowDateTime=FT_NO_TIME_LOW;
	ft->dwHighDateTime=FT_NO_TIME_HIGH;
}

static BOOL FtValid(const FILETIME* ft)
{
	return ft && ft->dwHighDateTime!=FT_NO_TIME_HIGH;
}

static BOOL FtFromUnix(unsigned long long sec, FILETIME* ft)
{
	if (!ft)
		return FALSE;
	if (sec<315532800ULL || sec>20000000000ULL) {
		FtNone(ft);
		return FALSE;
	}
	unsigned long long ticks=(sec+11644473600ULL)*10000000ULL;
	ft->dwLowDateTime=(DWORD)ticks;
	ft->dwHighDateTime=(DWORD)(ticks>>32);
	return TRUE;
}

BOOL AppleFileTimeFromAfcRaw(unsigned long long raw, FILETIME* ft)
{
	unsigned long long sec;
	if (raw>=10000000000000000ULL)
		sec=raw/1000000000ULL;
	else if (raw>=10000000000000ULL)
		sec=raw/1000000ULL;
	else if (raw>=10000000000ULL)
		sec=raw/1000ULL;
	else
		sec=raw;
	return FtFromUnix(sec, ft);
}

static BOOL FtFromCocoa(double cocoa, FILETIME* ft)
{
	if (!ft)
		return FALSE;
	if (cocoa==0.0) {
		FtNone(ft);
		return FALSE;
	}
	double unixSec=cocoa+978307200.0;
	if (unixSec<315532800.0 || unixSec>20000000000.0) {
		FtNone(ft);
		return FALSE;
	}
	return FtFromUnix((unsigned long long)(unixSec+0.5), ft);
}

static const WCHAR* SmartName(int id, int russian)
{
	switch (id) {
	case AL_ALL: return russian ? L"Все фото" : L"All Photos";
	case AL_FAV: return russian ? L"Избранное" : L"Favorites";
	case AL_SCREEN: return russian ? L"Снимки экрана" : L"Screenshots";
	case AL_SELFIE: return russian ? L"Селфи" : L"Selfies";
	case AL_LIVE: return russian ? L"Фото Live" : L"Live Photos";
	case AL_PANO: return russian ? L"Панорамы" : L"Panoramas";
	case AL_PORT: return russian ? L"Портрет" : L"Portrait";
	case AL_VIDEO: return russian ? L"Видео" : L"Videos";
	case AL_SLOMO: return russian ? L"Замедленно" : L"Slo-mo";
	case AL_BURST: return russian ? L"Серии" : L"Bursts";
	case AL_HIDDEN: return russian ? L"Скрытые" : L"Hidden";
	case AL_TRASH: return russian ? L"Недавно удалённые" : L"Recently Deleted";
	default: return NULL;
	}
}

static void ApplyLang(PhotoLib* lib, int russian)
{
	lib->russian=russian ? 1 : 0;
	for (int i=0;i<lib->nalbums;i++) {
		const WCHAR* n=SmartName(lib->albums[i].smart, lib->russian);
		if (!n)
			continue;
		free(lib->albums[i].name);
		lib->albums[i].name=WcsDup(n);
	}
}

static void FreeAlbum(PhotoAlbum* al)
{
	if (!al)
		return;
	free(al->name);
	free(al->ids);
	if (al->names) {
		for (int i=0;i<al->n;i++)
			free(al->names[i]);
		free(al->names);
	}
	free(al->nameset.key);
	free(al->nameset.used);
	memset(al, 0, sizeof(*al));
}

static void DropNameSets(PhotoLib* lib)
{
	if (!lib)
		return;
	for (int i=0;i<lib->nalbums;i++) {
		free(lib->albums[i].nameset.key);
		free(lib->albums[i].nameset.used);
		memset(&lib->albums[i].nameset, 0, sizeof(lib->albums[i].nameset));
	}
}

static void FreeLib(PhotoLib* lib)
{
	if (!lib)
		return;
	if (lib->assets) {
		for (int i=0;i<lib->nassets;i++) {
			free(lib->assets[i].path);
			free(lib->assets[i].display);
		}
		free(lib->assets);
	}
	if (lib->albums) {
		for (int i=0;i<lib->nalbums;i++)
			FreeAlbum(&lib->albums[i]);
		free(lib->albums);
	}
	if (lib->tombs) {
		for (int i=0;i<lib->ntomb;i++)
			free(lib->tombs[i]);
		free(lib->tombs);
	}
	free(lib->tombset.key);
	free(lib->tombset.used);
	if (lib->thumbMiss) {
		for (int i=0;i<lib->nThumbMiss;i++)
			free(lib->thumbMiss[i]);
		free(lib->thumbMiss);
	}
	free(lib);
}

static BOOL SqlIdent(const char* s)
{
	if (!s || !s[0] || strlen(s)>64)
		return FALSE;
	for (const char* p=s; *p; p++) {
		unsigned char c=(unsigned char)*p;
		if (!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))
			return FALSE;
	}
	return TRUE;
}

static BOOL SqlAdd(char* sql, int cch, const char* piece)
{
	if (!sql || cch<=0 || !piece)
		return FALSE;
	size_t n=strlen(sql);
	size_t m=strlen(piece);
	if (n+m+1>(size_t)cch)
		return FALSE;
	memcpy(sql+n, piece, m+1);
	return TRUE;
}

static BOOL LoadSqlite(void)
{
	if (g_sqliteReady)
		return TRUE;
	if (g_sqliteFailed)
		return FALSE;
	HMODULE h=LoadLibraryW(L"winsqlite3.dll");
	if (!h) {
		Log("photos winsqlite3 missing");
		g_sqliteFailed=1;
		return FALSE;
	}
	pSqlOpen=(t_sql_open)GetProcAddress(h, "sqlite3_open_v2");
	pSqlClose=(t_sql_close)GetProcAddress(h, "sqlite3_close");
	pSqlExec=(t_sql_exec)GetProcAddress(h, "sqlite3_exec");
	pSqlFree=(t_sql_free)GetProcAddress(h, "sqlite3_free");
	pSqlPrepare=(t_sql_prepare)GetProcAddress(h, "sqlite3_prepare_v2");
	pSqlStep=(t_sql_step)GetProcAddress(h, "sqlite3_step");
	pSqlFinalize=(t_sql_finalize)GetProcAddress(h, "sqlite3_finalize");
	pSqlColCount=(t_sql_colcount)GetProcAddress(h, "sqlite3_column_count");
	pSqlColType=(t_sql_coltype)GetProcAddress(h, "sqlite3_column_type");
	pSqlColName=(t_sql_colname)GetProcAddress(h, "sqlite3_column_name");
	pSqlText=(t_sql_text)GetProcAddress(h, "sqlite3_column_text");
	pSqlInt=(t_sql_int)GetProcAddress(h, "sqlite3_column_int");
	pSqlInt64=(t_sql_int64)GetProcAddress(h, "sqlite3_column_int64");
	pSqlDouble=(t_sql_double)GetProcAddress(h, "sqlite3_column_double");
	pSqlErrmsg=(t_sql_errmsg)GetProcAddress(h, "sqlite3_errmsg");
	if (!pSqlOpen || !pSqlClose || !pSqlExec || !pSqlPrepare || !pSqlStep || !pSqlFinalize ||
		!pSqlColCount || !pSqlColType || !pSqlColName || !pSqlText || !pSqlInt ||
		!pSqlInt64 || !pSqlDouble || !pSqlErrmsg) {
		Log("photos winsqlite3 exports missing");
		g_sqliteFailed=1;
		return FALSE;
	}
	g_sqlite=h;
	g_sqliteReady=1;
	return TRUE;
}

struct PkMap {
	int* pk;
	int* index;
	unsigned char* used;
	int cap;
};

static BOOL PkInit(PkMap* m, int cap)
{
	memset(m, 0, sizeof(*m));
	int c=16;
	while (c<cap)
		c*=2;
	m->pk=(int*)calloc((size_t)c, sizeof(int));
	m->index=(int*)calloc((size_t)c, sizeof(int));
	m->used=(unsigned char*)calloc((size_t)c, 1);
	m->cap=c;
	if (!m->pk || !m->index || !m->used) {
		free(m->pk);
		free(m->index);
		free(m->used);
		memset(m, 0, sizeof(*m));
		return FALSE;
	}
	return TRUE;
}

static void PkFree(PkMap* m)
{
	if (!m)
		return;
	free(m->pk);
	free(m->index);
	free(m->used);
	memset(m, 0, sizeof(*m));
}

static int PkGet(PkMap* m, int pk)
{
	if (!m || !m->cap)
		return -1;
	unsigned mask=(unsigned)m->cap-1u;
	unsigned h=(unsigned)pk*2654435761u;
	for (int i=0;i<m->cap;i++) {
		unsigned s=(h+(unsigned)i)&mask;
		if (!m->used[s])
			return -1;
		if (m->pk[s]==pk)
			return m->index[s];
	}
	return -1;
}

static BOOL PkPut(PkMap* m, int pk, int index)
{
	if (!m || !m->cap)
		return FALSE;
	if (PkGet(m, pk)>=0)
		return TRUE;
	unsigned mask=(unsigned)m->cap-1u;
	unsigned h=(unsigned)pk*2654435761u;
	for (int i=0;i<m->cap;i++) {
		unsigned s=(h+(unsigned)i)&mask;
		if (!m->used[s]) {
			m->used[s]=1;
			m->pk[s]=pk;
			m->index[s]=index;
			return TRUE;
		}
	}
	return FALSE;
}

static int ColIndex(sqlite3_stmt* st, const char* alias)
{
	if (!st || !alias || !pSqlColCount || !pSqlColName)
		return -1;
	int n=pSqlColCount(st);
	for (int i=0;i<n;i++) {
		const char* nm=pSqlColName(st, i);
		if (nm && _stricmp(nm, alias)==0)
			return i;
	}
	return -1;
}

static int IntAt(sqlite3_stmt* st, int idx)
{
	if (idx<0 || !pSqlColType || pSqlColType(st, idx)==SQL_NULL)
		return 0;
	return pSqlInt(st, idx);
}

static double DoubleAt(sqlite3_stmt* st, int idx)
{
	if (idx<0 || !pSqlColType || pSqlColType(st, idx)==SQL_NULL)
		return 0;
	return pSqlDouble(st, idx);
}

static unsigned long long U64At(sqlite3_stmt* st, int idx)
{
	if (idx<0 || !pSqlColType || pSqlColType(st, idx)==SQL_NULL)
		return 0;
	long long v=pSqlInt64(st, idx);
	if (v<0)
		return 0;
	return (unsigned long long)v;
}

static const char* TextAt(sqlite3_stmt* st, int idx)
{
	if (idx<0 || !pSqlColType || pSqlColType(st, idx)==SQL_NULL)
		return NULL;
	return (const char*)pSqlText(st, idx);
}

static void SqlLog(sqlite3* db, const char* what)
{
	char line[200];
	const char* e=(db && pSqlErrmsg) ? pSqlErrmsg(db) : "";
	_snprintf_s(line, sizeof(line), _TRUNCATE, "photos sql %s %s", what ? what : "", e ? e : "");
	Log(line);
}

static BOOL HasTable(sqlite3* db, const char* name)
{
	if (!SqlIdent(name))
		return FALSE;
	char q[160];
	_snprintf_s(q, sizeof(q), _TRUNCATE,
		"SELECT 1 FROM sqlite_master WHERE type='table' AND name='%s'", name);
	sqlite3_stmt* st=NULL;
	if (pSqlPrepare(db, q, -1, &st, NULL)!=SQL_OK || !st)
		return FALSE;
	BOOL yes=pSqlStep(st)==SQL_ROW;
	pSqlFinalize(st);
	return yes;
}

static BOOL HasCol(sqlite3* db, const char* table, const char* col)
{
	if (!SqlIdent(table) || !SqlIdent(col))
		return FALSE;
	char q[128];
	_snprintf_s(q, sizeof(q), _TRUNCATE, "PRAGMA table_info(%s)", table);
	sqlite3_stmt* st=NULL;
	if (pSqlPrepare(db, q, -1, &st, NULL)!=SQL_OK || !st)
		return FALSE;
	BOOL yes=FALSE;
	while (pSqlStep(st)==SQL_ROW) {
		const char* n=(const char*)pSqlText(st, 1);
		if (n && _stricmp(n, col)==0) {
			yes=TRUE;
			break;
		}
	}
	pSqlFinalize(st);
	return yes;
}

static BOOL IsIndexedCol(const char* name, const char* suf)
{
	if (!name || name[0]!='Z' || name[1]!='_')
		return FALSE;
	const char* p=name+2;
	if (*p<'0' || *p>'9')
		return FALSE;
	while (*p>='0' && *p<='9')
		p++;
	return strcmp(p, suf)==0;
}

static void FindJoinCols(sqlite3* db, const char* table, char* albumCol, int albumCch, char* assetCol, int assetCch)
{
	albumCol[0]=0;
	assetCol[0]=0;
	char q[128];
	_snprintf_s(q, sizeof(q), _TRUNCATE, "PRAGMA table_info(%s)", table);
	sqlite3_stmt* st=NULL;
	if (pSqlPrepare(db, q, -1, &st, NULL)!=SQL_OK || !st)
		return;
	while (pSqlStep(st)==SQL_ROW) {
		const char* n=(const char*)pSqlText(st, 1);
		if (!n || !SqlIdent(n))
			continue;
		if (!albumCol[0] && IsIndexedCol(n, "ALBUMS"))
			strncpy_s(albumCol, (size_t)albumCch, n, _TRUNCATE);
		if (!assetCol[0] && IsIndexedCol(n, "ASSETS"))
			strncpy_s(assetCol, (size_t)assetCch, n, _TRUNCATE);
	}
	pSqlFinalize(st);
}

static void SanitizeName(WCHAR* s, int cch)
{
	if (!s || cch<=0)
		return;
	for (WCHAR* p=s; *p; p++) {
		if (*p<32 || wcschr(L"\\/:*?\"<>|", *p))
			*p=L'_';
	}
	int n=(int)wcslen(s);
	while (n>0 && (s[n-1]==L'.' || s[n-1]==L' ')) {
		s[n-1]=0;
		n--;
	}
	if (!s[0] || !wcscmp(s, L".."))
		WcsCopy(s, cch, L"photo");
}

static BOOL AlbumNameTaken(PhotoLib* lib, const WCHAR* name)
{
	for (int i=0;i<lib->nalbums;i++) {
		if (lib->albums[i].name && WcsIcmp(lib->albums[i].name, name)==0)
			return TRUE;
	}
	return FALSE;
}

static void UniqueAlbumName(PhotoLib* lib, WCHAR* name, int cch)
{
	if (!AlbumNameTaken(lib, name))
		return;
	WCHAR base[240];
	WcsCopy(base, 200, name);
	for (int n=2; n<10000; n++) {
		swprintf_s(name, (size_t)cch, L"%s (%d)", base, n);
		if (!AlbumNameTaken(lib, name))
			return;
	}
}

static BOOL IsSystemAlbumTitle(const WCHAR* name)
{
	static const WCHAR* skip[]={
		L"Camera Roll", L"Recents", L"All Photos", L"Recently Added",
		L"Recently Deleted", L"Hidden", L"Favorites", L"Selfies",
		L"Screenshots", L"Live Photos", L"Panoramas", L"Portrait",
		L"Videos", L"Slo-mo", L"Bursts", L"Time-lapse", L"Photo Live",
		L"Все фото", L"Недавние", L"Фотоплёнка", L"Камера",
		L"Недавно удалённые", L"Скрытые", L"Избранное", L"Селфи",
		L"Снимки экрана", L"Панорамы", L"Портрет", L"Видео",
		L"Замедленно", L"Серии", L"Таймлапс", L"Фото Live",
		NULL
	};
	for (int i=0; skip[i]; i++) {
		if (WcsIcmp(name, skip[i])==0)
			return TRUE;
	}
	return FALSE;
}

static PhotoAlbum* AddShell(PhotoLib* lib, int smart, const WCHAR* name)
{
	if (!name || !name[0] || lib->nalbums>=PHOTO_ALBUM_MAX)
		return NULL;
	if (lib->nalbums>=lib->albumCap) {
		int ncap=lib->albumCap ? lib->albumCap*2 : 16;
		PhotoAlbum* p=(PhotoAlbum*)realloc(lib->albums, (size_t)ncap*sizeof(PhotoAlbum));
		if (!p)
			return NULL;
		lib->albums=p;
		lib->albumCap=ncap;
	}
	PhotoAlbum* al=&lib->albums[lib->nalbums];
	memset(al, 0, sizeof(*al));
	al->smart=smart;
	al->name=WcsDup(name);
	FtNone(&al->newest);
	if (!al->name)
		return NULL;
	lib->nalbums++;
	return al;
}

static unsigned NameHash(const WCHAR* s)
{
	BYTE key[4096];
	unsigned h=2166136261u;
	if (!s)
		s=L"";
	int n=LCMapStringW(LOCALE_USER_DEFAULT, LCMAP_SORTKEY | NORM_IGNORECASE,
		s, -1, (LPWSTR)key, (int)sizeof(key));
	if (n<=0) {
		for (; *s; s++) {
			h^=(unsigned)*s;
			h*=16777619u;
		}
		return h;
	}
	for (int i=0;i<n;i++) {
		h^=key[i];
		h*=16777619u;
	}
	return h;
}

static BOOL NameSetRehash(NameSet* s, int cap)
{
	WCHAR** key=(WCHAR**)calloc((size_t)cap, sizeof(WCHAR*));
	unsigned char* used=(unsigned char*)calloc((size_t)cap, 1);
	if (!key || !used || cap<=0 || (cap&(cap-1))!=0) {
		free(key);
		free(used);
		return FALSE;
	}
	unsigned mask=(unsigned)cap-1u;
	int placed=0;
	for (int i=0; s->used && i<s->cap; i++) {
		if (!s->used[i] || !s->key[i])
			continue;
		unsigned h=NameHash(s->key[i]);
		int found=0;
		for (int j=0;j<cap;j++) {
			unsigned slot=(h+(unsigned)j)&mask;
			if (!used[slot]) {
				used[slot]=1;
				key[slot]=s->key[i];
				placed++;
				found=1;
				break;
			}
		}
		if (!found) {
			free(key);
			free(used);
			return FALSE;
		}
	}
	if (placed!=s->n) {
		free(key);
		free(used);
		return FALSE;
	}
	free(s->key);
	free(s->used);
	s->key=key;
	s->used=used;
	s->cap=cap;
	return TRUE;
}

static void NameSetDisable(NameSet* s)
{
	if (!s)
		return;
	free(s->key);
	free(s->used);
	memset(s, 0, sizeof(*s));
	s->failed=1;
}

static BOOL NameSetHas(const NameSet* s, const WCHAR* name)
{
	if (!s || !s->cap || !name)
		return FALSE;
	unsigned mask=(unsigned)s->cap-1u;
	unsigned h=NameHash(name);
	for (int i=0;i<s->cap;i++) {
		unsigned slot=(h+(unsigned)i)&mask;
		if (!s->used[slot])
			return FALSE;
		if (s->key[slot] && WcsIcmp(s->key[slot], name)==0)
			return TRUE;
	}
	return FALSE;
}

static BOOL NameSetPut(NameSet* s, WCHAR* name)
{
	if (!s || !name || s->failed)
		return FALSE;
	if (s->cap<16 || s->n*2>=s->cap) {
		int ncap=s->cap ? s->cap*2 : 16;
		if (!NameSetRehash(s, ncap))
			return FALSE;
	}
	unsigned mask=(unsigned)s->cap-1u;
	unsigned h=NameHash(name);
	for (int i=0;i<s->cap;i++) {
		unsigned slot=(h+(unsigned)i)&mask;
		if (!s->used[slot]) {
			s->used[slot]=1;
			s->key[slot]=name;
			s->n++;
			return TRUE;
		}
		if (s->key[slot] && WcsIcmp(s->key[slot], name)==0)
			return TRUE;
	}
	return FALSE;
}

static BOOL NameTakenLinear(const PhotoAlbum* al, const WCHAR* name)
{
	for (int i=0;i<al->n;i++) {
		if (al->names[i] && WcsIcmp(al->names[i], name)==0)
			return TRUE;
	}
	return FALSE;
}

static BOOL NameTaken(const PhotoAlbum* al, const WCHAR* name)
{
	if (!al || !name)
		return FALSE;
	if (al->nameset.failed || !al->nameset.cap)
		return NameTakenLinear(al, name);
	return NameSetHas(&al->nameset, name);
}

static BOOL IdTaken(const PhotoAlbum* al, int assetIndex)
{
	/* Smart albums receive each asset index once, in order. A join row
	   can name the same asset twice, so only user albums need the scan. */
	if (!al || al->smart!=AL_USER)
		return FALSE;
	for (int i=0;i<al->n;i++) {
		if (al->ids[i]==assetIndex)
			return TRUE;
	}
	return FALSE;
}

static void UniqueFileName(PhotoAlbum* al, WCHAR* name, int cch)
{
	if (!NameTaken(al, name))
		return;
	WCHAR stem[240], ext[64];
	WcsCopy(stem, 200, name);
	ext[0]=0;
	WCHAR* dot=wcsrchr(stem, L'.');
	if (dot && dot!=stem) {
		WcsCopy(ext, 64, dot);
		*dot=0;
	}
	for (int n=2; n<10000; n++) {
		swprintf_s(name, (size_t)cch, L"%s (%d)%s", stem, n, ext);
		if (!NameTaken(al, name))
			return;
	}
}

static BOOL AddToAlbum(PhotoAlbum* al, int assetIndex, const WCHAR* display, const FILETIME* when)
{
	if (!al || assetIndex<0)
		return FALSE;
	if (IdTaken(al, assetIndex))
		return TRUE;
	if (al->n>=al->cap) {
		int ncap=al->cap ? al->cap*2 : 8;
		int* ids=(int*)realloc(al->ids, (size_t)ncap*sizeof(int));
		if (!ids)
			return FALSE;
		al->ids=ids;
		WCHAR** names=(WCHAR**)realloc(al->names, (size_t)ncap*sizeof(WCHAR*));
		if (!names)
			return FALSE;
		al->names=names;
		al->cap=ncap;
	}
	WCHAR tmp[260];
	WcsCopy(tmp, 240, display ? display : L"photo");
	SanitizeName(tmp, 240);
	UniqueFileName(al, tmp, 260);
	WCHAR* dup=WcsDup(tmp);
	if (!dup)
		return FALSE;
	al->ids[al->n]=assetIndex;
	al->names[al->n]=dup;
	al->n++;
	if (!al->nameset.failed && !NameSetPut(&al->nameset, dup))
		NameSetDisable(&al->nameset);
	if (when && FtValid(when) && (!FtValid(&al->newest) || CompareFileTime(when, &al->newest)>0))
		al->newest=*when;
	return TRUE;
}

static int AddAsset(PhotoLib* lib, unsigned flags, ULONGLONG size, const FILETIME* when,
	const char* path, const WCHAR* display)
{
	if (!path || !path[0] || lib->nassets>=PHOTO_ASSET_MAX)
		return -1;
	if (lib->nassets>=lib->assetCap) {
		int ncap=lib->assetCap ? lib->assetCap*2 : 64;
		PhotoAsset* p=(PhotoAsset*)realloc(lib->assets, (size_t)ncap*sizeof(PhotoAsset));
		if (!p)
			return -1;
		lib->assets=p;
		lib->assetCap=ncap;
	}
	PhotoAsset* a=&lib->assets[lib->nassets];
	memset(a, 0, sizeof(*a));
	a->flags=flags;
	a->size=size;
	if (when && FtValid(when))
		a->when=*when;
	else
		FtNone(&a->when);
	a->path=StrDup(path);
	a->display=WcsDup(display ? display : L"photo");
	if (!a->path || !a->display) {
		free(a->path);
		free(a->display);
		a->path=NULL;
		a->display=NULL;
		return -1;
	}
	return lib->nassets++;
}

static BOOL InRoll(unsigned fl)
{
	return (fl & (PF_TRASH|PF_HIDDEN|PF_SKIP))==0;
}

static BOOL IsDotName(const char* name)
{
	return !name || !name[0] || name[0]=='.' || strcmp(name, ".")==0 || strcmp(name, "..")==0;
}

static BOOL IsJunkFile(const char* name)
{
	if (IsDotName(name))
		return TRUE;
	const char* dot=strrchr(name, '.');
	if (!dot || !dot[1])
		return TRUE;
	const char* e=dot+1;
	return _stricmp(e, "aae")==0 || _stricmp(e, "thm")==0 || _stricmp(e, "ithmb")==0 ||
		_stricmp(e, "plist")==0 || _stricmp(e, "xml")==0 || _stricmp(e, "db")==0 ||
		_stricmp(e, "sqlite")==0 || _stricmp(e, "tmp")==0;
}

static void MakeDisplay(const char* original, const char* filename, WCHAR* out, int cch)
{
	const char* pick=filename ? filename : "";
	if (original && original[0] && strchr(original, '.'))
		pick=original;
	Utf8ToWide(pick, out, cch);
	SanitizeName(out, cch);
}

static BOOL DirComponentsOk(const char* dir)
{
	if (!dir || !dir[0] || dir[0]=='/' || dir[0]=='\\')
		return FALSE;
	const char* p=dir;
	while (*p) {
		const char* sl=strchr(p, '/');
		size_t n=sl ? (size_t)(sl-p) : strlen(p);
		if (n==0 || n>200)
			return FALSE;
		if (p[0]=='.' || memchr(p, '\\', n))
			return FALSE;
		if (!sl)
			break;
		p=sl+1;
	}
	return TRUE;
}

static BOOL MakeDcimPath(char* out, int cch, const char* dir, const char* file)
{
	if (!out || cch<16 || !dir || !file)
		return FALSE;
	if (!file[0] || IsDotName(file) || strchr(file, '/') || strchr(file, '\\'))
		return FALSE;
	if (!DirComponentsOk(dir))
		return FALSE;
	/* Current Photos.sqlite stores "DCIM/100APPLE" or a PhotoData path.
	   Older libraries store only "100APPLE". */
	if (strchr(dir, '/'))
		return _snprintf_s(out, (size_t)cch, _TRUNCATE, "%s/%s", dir, file)>0;
	return _snprintf_s(out, (size_t)cch, _TRUNCATE, "DCIM/%s/%s", dir, file)>0;
}

static void RemoveEmpty(PhotoLib* lib)
{
	int w=0;
	for (int i=0;i<lib->nalbums;i++) {
		if (lib->albums[i].n<=0) {
			FreeAlbum(&lib->albums[i]);
			continue;
		}
		if (w!=i) {
			lib->albums[w]=lib->albums[i];
			memset(&lib->albums[i], 0, sizeof(lib->albums[i]));
		}
		w++;
	}
	lib->nalbums=w;
}

static int CmpAlbumAsc(const void* a, const void* b)
{
	const PhotoAlbum* x=(const PhotoAlbum*)a;
	const PhotoAlbum* y=(const PhotoAlbum*)b;
	int r=CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
		x->name ? x->name : L"", -1, y->name ? y->name : L"", -1);
	if (r==0)
		return 0;
	return r-2;
}

static void SortUserAlbums(PhotoLib* lib)
{
	int first=-1;
	for (int i=0;i<lib->nalbums;i++) {
		if (lib->albums[i].smart!=AL_USER)
			continue;
		first=i;
		break;
	}
	if (first<0 || first>=lib->nalbums)
		return;
	int n=lib->nalbums-first;
	if (n<=1)
		return;
	qsort(lib->albums+first, (size_t)n, sizeof(PhotoAlbum), CmpAlbumAsc);
}

static void FillSmart(PhotoLib* lib, int russian)
{
	struct { int id; } order[]={
		{AL_ALL},{AL_FAV},{AL_SCREEN},{AL_SELFIE},{AL_LIVE},{AL_PANO},
		{AL_PORT},{AL_VIDEO},{AL_SLOMO},{AL_BURST},{AL_HIDDEN},{AL_TRASH}
	};
	PhotoAlbum* slot[13];
	memset(slot, 0, sizeof(slot));
	for (int i=0;i<(int)(sizeof(order)/sizeof(order[0]));i++) {
		const WCHAR* n=SmartName(order[i].id, russian);
		slot[order[i].id]=AddShell(lib, order[i].id, n);
	}
	for (int i=0;i<lib->nassets;i++) {
		unsigned fl=lib->assets[i].flags;
		const WCHAR* disp=lib->assets[i].display;
		const FILETIME* when=&lib->assets[i].when;
		BOOL roll=InRoll(fl);
		if (roll && slot[AL_ALL])
			AddToAlbum(slot[AL_ALL], i, disp, when);
		if (roll && (fl&PF_FAV) && slot[AL_FAV])
			AddToAlbum(slot[AL_FAV], i, disp, when);
		if (roll && (fl&PF_SCREEN) && slot[AL_SCREEN])
			AddToAlbum(slot[AL_SCREEN], i, disp, when);
		if (roll && (fl&PF_SELFIE) && slot[AL_SELFIE])
			AddToAlbum(slot[AL_SELFIE], i, disp, when);
		if (roll && (fl&PF_LIVE) && slot[AL_LIVE])
			AddToAlbum(slot[AL_LIVE], i, disp, when);
		if (roll && (fl&PF_PANO) && slot[AL_PANO])
			AddToAlbum(slot[AL_PANO], i, disp, when);
		if (roll && (fl&PF_PORT) && slot[AL_PORT])
			AddToAlbum(slot[AL_PORT], i, disp, when);
		if (roll && (fl&PF_VIDEO) && !(fl&PF_LIVE) && slot[AL_VIDEO])
			AddToAlbum(slot[AL_VIDEO], i, disp, when);
		if (roll && (fl&PF_SLOMO) && slot[AL_SLOMO])
			AddToAlbum(slot[AL_SLOMO], i, disp, when);
		if (roll && (fl&PF_BURST) && slot[AL_BURST])
			AddToAlbum(slot[AL_BURST], i, disp, when);
		if ((fl&PF_HIDDEN) && !(fl&PF_TRASH) && slot[AL_HIDDEN])
			AddToAlbum(slot[AL_HIDDEN], i, disp, when);
		if ((fl&PF_TRASH) && slot[AL_TRASH])
			AddToAlbum(slot[AL_TRASH], i, disp, when);
	}
}

static BOOL ReadAssets(sqlite3* db, PhotoLib* lib, PkMap* pks, const char* table)
{
	BOOL join=HasTable(db, "ZADDITIONALASSETATTRIBUTES") &&
		HasCol(db, table, "ZADDITIONALATTRIBUTES");
	BOOL joinAsset=!join && HasTable(db, "ZADDITIONALASSETATTRIBUTES") &&
		HasCol(db, "ZADDITIONALASSETATTRIBUTES", "ZASSET");
	char sql[2048];
	sql[0]=0;
	if (!SqlAdd(sql, (int)sizeof(sql), "SELECT "))
		return FALSE;
	int nsel=0;
	const char* exprs[24];
	int nc=0;
	if (HasCol(db, table, "Z_PK")) exprs[nc++]="a.Z_PK AS pk";
	if (HasCol(db, table, "ZFILENAME")) exprs[nc++]="a.ZFILENAME AS filename";
	if (HasCol(db, table, "ZDIRECTORY")) exprs[nc++]="a.ZDIRECTORY AS directory";
	if (HasCol(db, table, "ZDATECREATED")) exprs[nc++]="a.ZDATECREATED AS created";
	if (HasCol(db, table, "ZADDEDDATE")) exprs[nc++]="a.ZADDEDDATE AS added";
	if (HasCol(db, table, "ZKIND")) exprs[nc++]="a.ZKIND AS kind";
	if (HasCol(db, table, "ZKINDSUBTYPE")) exprs[nc++]="a.ZKINDSUBTYPE AS sub";
	if (HasCol(db, table, "ZFAVORITE")) exprs[nc++]="a.ZFAVORITE AS fav";
	if (HasCol(db, table, "ZHIDDEN")) exprs[nc++]="a.ZHIDDEN AS hidden";
	if (HasCol(db, table, "ZTRASHEDSTATE")) exprs[nc++]="a.ZTRASHEDSTATE AS trash";
	if (HasCol(db, table, "ZVISIBILITYSTATE")) exprs[nc++]="a.ZVISIBILITYSTATE AS vis";
	if (HasCol(db, table, "ZHIGHFRAMERATESTATE")) exprs[nc++]="a.ZHIGHFRAMERATESTATE AS hfr";
	if (HasCol(db, table, "ZDEPTHTYPE")) exprs[nc++]="a.ZDEPTHTYPE AS depth";
	if (HasCol(db, table, "ZPLAYBACKSTYLE")) exprs[nc++]="a.ZPLAYBACKSTYLE AS playback";
	if (HasCol(db, table, "ZAVALANCHEPICKTYPE")) exprs[nc++]="a.ZAVALANCHEPICKTYPE AS burst";
	if (HasCol(db, table, "ZISDETECTEDSCREENSHOT")) exprs[nc++]="a.ZISDETECTEDSCREENSHOT AS shot";
	if (join || joinAsset) {
		const char* aa="ZADDITIONALASSETATTRIBUTES";
		if (HasCol(db, aa, "ZORIGINALFILENAME")) exprs[nc++]="aa.ZORIGINALFILENAME AS orig";
		if (HasCol(db, aa, "ZORIGINALFILESIZE")) exprs[nc++]="aa.ZORIGINALFILESIZE AS sz";
		if (HasCol(db, aa, "ZCAMERACAPTUREDEVICE")) exprs[nc++]="aa.ZCAMERACAPTUREDEVICE AS camera";
	} else if (HasCol(db, table, "ZCAMERACAPTUREDEVICE"))
		exprs[nc++]="a.ZCAMERACAPTUREDEVICE AS camera";
	for (int i=0;i<nc;i++) {
		if (nsel && !SqlAdd(sql, (int)sizeof(sql), ", "))
			return FALSE;
		if (!SqlAdd(sql, (int)sizeof(sql), exprs[i]))
			return FALSE;
		nsel++;
	}
	if (nsel==0)
		return FALSE;
	if (!SqlAdd(sql, (int)sizeof(sql), " FROM ") || !SqlAdd(sql, (int)sizeof(sql), table) ||
		!SqlAdd(sql, (int)sizeof(sql), " a"))
		return FALSE;
	if (join) {
		if (!SqlAdd(sql, (int)sizeof(sql),
			" LEFT JOIN ZADDITIONALASSETATTRIBUTES aa ON aa.Z_PK = a.ZADDITIONALATTRIBUTES"))
			return FALSE;
	} else if (joinAsset) {
		if (!SqlAdd(sql, (int)sizeof(sql),
			" LEFT JOIN ZADDITIONALASSETATTRIBUTES aa ON aa.ZASSET = a.Z_PK"))
			return FALSE;
	}
	sqlite3_stmt* st=NULL;
	if (pSqlPrepare(db, sql, -1, &st, NULL)!=SQL_OK || !st) {
		SqlLog(db, "assets");
		return FALSE;
	}
	int ixPk=ColIndex(st, "pk");
	int ixFile=ColIndex(st, "filename");
	int ixDir=ColIndex(st, "directory");
	int ixCreated=ColIndex(st, "created");
	int ixAdded=ColIndex(st, "added");
	int ixKind=ColIndex(st, "kind");
	int ixSub=ColIndex(st, "sub");
	int ixFav=ColIndex(st, "fav");
	int ixHidden=ColIndex(st, "hidden");
	int ixTrash=ColIndex(st, "trash");
	int ixVis=ColIndex(st, "vis");
	int ixHfr=ColIndex(st, "hfr");
	int ixDepth=ColIndex(st, "depth");
	int ixPlay=ColIndex(st, "playback");
	int ixBurst=ColIndex(st, "burst");
	int ixShot=ColIndex(st, "shot");
	int ixOrig=ColIndex(st, "orig");
	int ixSz=ColIndex(st, "sz");
	int ixCam=ColIndex(st, "camera");
	if (ixFile<0 || ixDir<0) {
		pSqlFinalize(st);
		Log("photos sqlite has no filename");
		return FALSE;
	}
	int rows=0;
	while (pSqlStep(st)==SQL_ROW) {
		if (++rows>300000)
			break;
		int pk=ixPk>=0 ? IntAt(st, ixPk) : rows;
		if (ixPk>=0 && PkGet(pks, pk)>=0)
			continue;
		const char* file=TextAt(st, ixFile);
		const char* dir=TextAt(st, ixDir);
		if (!file || !dir || IsJunkFile(file))
			continue;
		char path[512];
		if (!MakeDcimPath(path, (int)sizeof(path), dir, file))
			continue;
		int kind=IntAt(st, ixKind);
		int sub=IntAt(st, ixSub);
		int play=IntAt(st, ixPlay);
		int cam=IntAt(st, ixCam);
		unsigned fl=0;
		if (kind==1)
			fl|=PF_VIDEO;
		if (sub==1)
			fl|=PF_PANO;
		if (sub==2 || play==2 || play==3)
			fl|=PF_LIVE;
		if (sub==10 || IntAt(st, ixShot)==1)
			fl|=PF_SCREEN;
		if (IntAt(st, ixDepth)>0)
			fl|=PF_PORT;
		if (IntAt(st, ixFav))
			fl|=PF_FAV;
		if (IntAt(st, ixHidden))
			fl|=PF_HIDDEN;
		if (IntAt(st, ixTrash))
			fl|=PF_TRASH;
		if (IntAt(st, ixVis)==1)
			fl|=PF_SKIP;
		if (IntAt(st, ixHfr)==1)
			fl|=PF_SLOMO;
		if (IntAt(st, ixBurst)>0)
			fl|=PF_BURST;
		if (cam==1 && !(fl&PF_SCREEN) && !(fl&PF_VIDEO))
			fl|=PF_SELFIE;
		FILETIME when;
		if (!FtFromCocoa(DoubleAt(st, ixCreated), &when))
			FtFromCocoa(DoubleAt(st, ixAdded), &when);
		WCHAR disp[260];
		MakeDisplay(TextAt(st, ixOrig), file, disp, 260);
		int idx=AddAsset(lib, fl, U64At(st, ixSz), &when, path, disp);
		if (idx<0)
			break;
		if (ixPk>=0)
			PkPut(pks, pk, idx);
	}
	pSqlFinalize(st);
	return TRUE;
}

static BOOL ReadUserAlbums(sqlite3* db, PhotoLib* lib, PkMap* albums, PkMap* assets)
{
	if (!HasTable(db, "ZGENERICALBUM"))
		return TRUE;
	BOOL hasKind=HasCol(db, "ZGENERICALBUM", "ZKIND");
	BOOL hasTrash=HasCol(db, "ZGENERICALBUM", "ZTRASHEDSTATE");
	BOOL hasTitle=HasCol(db, "ZGENERICALBUM", "ZTITLE");
	BOOL hasPk=HasCol(db, "ZGENERICALBUM", "Z_PK");
	if (!hasTitle || !hasPk)
		return TRUE;
	char sql[256];
	sql[0]=0;
	SqlAdd(sql, (int)sizeof(sql), "SELECT Z_PK AS pk, ZTITLE AS title");
	if (hasKind)
		SqlAdd(sql, (int)sizeof(sql), ", ZKIND AS kind");
	if (hasTrash)
		SqlAdd(sql, (int)sizeof(sql), ", ZTRASHEDSTATE AS trash");
	SqlAdd(sql, (int)sizeof(sql), " FROM ZGENERICALBUM");
	sqlite3_stmt* st=NULL;
	if (pSqlPrepare(db, sql, -1, &st, NULL)!=SQL_OK || !st) {
		SqlLog(db, "albums");
		return FALSE;
	}
	int ixPk=ColIndex(st, "pk");
	int ixTitle=ColIndex(st, "title");
	int ixKind=ColIndex(st, "kind");
	int ixTrash=ColIndex(st, "trash");
	while (pSqlStep(st)==SQL_ROW) {
		if (ixTrash>=0 && IntAt(st, ixTrash)!=0)
			continue;
		if (ixKind>=0) {
			int kind=IntAt(st, ixKind);
			if (kind!=2 && kind!=1505)
				continue;
		}
		const char* title=TextAt(st, ixTitle);
		if (!title || !title[0])
			continue;
		WCHAR name[240];
		Utf8ToWide(title, name, 240);
		SanitizeName(name, 240);
		if (!name[0] || IsSystemAlbumTitle(name))
			continue;
		UniqueAlbumName(lib, name, 240);
		int before=lib->nalbums;
		if (!AddShell(lib, AL_USER, name))
			continue;
		PkPut(albums, IntAt(st, ixPk), before);
	}
	pSqlFinalize(st);

	sqlite3_stmt* tabs=NULL;
	if (pSqlPrepare(db,
		"SELECT name FROM sqlite_master WHERE type='table' AND name GLOB 'Z_*ASSETS'",
		-1, &tabs, NULL)!=SQL_OK || !tabs)
		return TRUE;
	while (pSqlStep(tabs)==SQL_ROW) {
		const char* tname=(const char*)pSqlText(tabs, 0);
		if (!tname || !SqlIdent(tname))
			continue;
		char albumCol[64], assetCol[64];
		FindJoinCols(db, tname, albumCol, 64, assetCol, 64);
		if (!albumCol[0] || !assetCol[0])
			continue;
		char q[192];
		_snprintf_s(q, sizeof(q), _TRUNCATE, "SELECT %s, %s FROM %s", albumCol, assetCol, tname);
		sqlite3_stmt* ms=NULL;
		if (pSqlPrepare(db, q, -1, &ms, NULL)!=SQL_OK || !ms)
			continue;
		int rows=0;
		while (pSqlStep(ms)==SQL_ROW) {
			if (++rows>2000000)
				break;
			int albumPk=pSqlInt(ms, 0);
			int assetPk=pSqlInt(ms, 1);
			int ai=PkGet(albums, albumPk);
			int xi=PkGet(assets, assetPk);
			if (ai<0 || xi<0 || ai>=lib->nalbums)
				continue;
			AddToAlbum(&lib->albums[ai], xi, lib->assets[xi].display, &lib->assets[xi].when);
		}
		pSqlFinalize(ms);
	}
	pSqlFinalize(tabs);
	return TRUE;
}

static PhotoLib* BuildLibFromDb(const WCHAR* dbPath, int russian)
{
	if (!LoadSqlite())
		return NULL;
	char u8[MAX_PATH*3];
	if (!WideToUtf8(dbPath, u8, (int)sizeof(u8)))
		return NULL;
	sqlite3* db=NULL;
	if (pSqlOpen(u8, &db, SQL_OPEN_READWRITE, NULL)!=SQL_OK || !db) {
		SqlLog(db, "open");
		if (db)
			pSqlClose(db);
		return NULL;
	}
	char* err=NULL;
	if (pSqlExec)
		pSqlExec(db, "PRAGMA wal_checkpoint(TRUNCATE);", NULL, NULL, &err);
	if (err && pSqlFree)
		pSqlFree(err);
	const char* table=NULL;
	if (HasTable(db, "ZASSET"))
		table="ZASSET";
	else if (HasTable(db, "ZGENERICASSET"))
		table="ZGENERICASSET";
	if (!table) {
		Log("photos sqlite has no ZASSET");
		pSqlClose(db);
		return NULL;
	}
	PhotoLib* lib=(PhotoLib*)calloc(1, sizeof(PhotoLib));
	PkMap assets, albums;
	memset(&assets, 0, sizeof(assets));
	memset(&albums, 0, sizeof(albums));
	if (!lib || !PkInit(&assets, 262144) || !PkInit(&albums, 8192)) {
		PkFree(&assets);
		PkFree(&albums);
		FreeLib(lib);
		pSqlClose(db);
		return NULL;
	}
	BOOL ok=ReadAssets(db, lib, &assets, table);
	if (ok)
		FillSmart(lib, russian);
	if (ok)
		ok=ReadUserAlbums(db, lib, &albums, &assets);
	PkFree(&assets);
	PkFree(&albums);
	pSqlClose(db);
	if (!ok) {
		FreeLib(lib);
		return NULL;
	}
	RemoveEmpty(lib);
	SortUserAlbums(lib);
	DropNameSets(lib);
	char line[80];
	_snprintf_s(line, sizeof(line), _TRUNCATE, "photos sqlite assets=%d albums=%d",
		lib->nassets, lib->nalbums);
	Log(line);
	return lib;
}

static void FreeNames(char** names, int n)
{
	if (!names)
		return;
	for (int i=0;i<n;i++)
		free(names[i]);
	free(names);
}

static PhotoAlbum* FindSmart(PhotoLib* lib, int smart)
{
	for (int i=0;i<lib->nalbums;i++) {
		if (lib->albums[i].smart==smart)
			return &lib->albums[i];
	}
	return NULL;
}

static void AddDcimFile(PhotoLib* lib, const char* path, const char* file,
	ULONGLONG size, const FILETIME* when)
{
	if (IsJunkFile(file))
		return;
	WCHAR disp[260];
	Utf8ToWide(file, disp, 260);
	SanitizeName(disp, 260);
	int idx=AddAsset(lib, 0, size, when, path, disp);
	if (idx<0)
		return;
	PhotoAlbum* all=FindSmart(lib, AL_ALL);
	if (all)
		AddToAlbum(all, idx, lib->assets[idx].display, &lib->assets[idx].when);
}

static void WalkDcimDir(void* conn, PhotoLib* lib, const char* dirPath)
{
	if (!g_io.list)
		return;
	char** names=NULL;
	int n=g_io.list(conn, dirPath, &names);
	if (n<0)
		return;
	for (int i=0;i<n;i++) {
		if (IsJunkFile(names[i]))
			continue;
		char child[512];
		if (_snprintf_s(child, sizeof(child), _TRUNCATE, "%s/%s", dirPath, names[i])<0)
			continue;
		ULONGLONG sz=0, mt=0, bt=0;
		int kind=g_io.stat ? g_io.stat(conn, child, &sz, &mt, &bt) : 1;
		FILETIME when;
		if (!AppleFileTimeFromAfcRaw(bt, &when))
			AppleFileTimeFromAfcRaw(mt, &when);
		AddDcimFile(lib, child, names[i], sz, &when);
	}
	FreeNames(names, n);
}

static PhotoLib* BuildFromDcim(void* conn, int russian)
{
	if (!g_io.list || !conn)
		return NULL;
	char** names=NULL;
	int n=g_io.list(conn, "DCIM", &names);
	if (n<0)
		return NULL;
	PhotoLib* lib=(PhotoLib*)calloc(1, sizeof(PhotoLib));
	if (!lib) {
		FreeNames(names, n);
		return NULL;
	}
	AddShell(lib, AL_ALL, SmartName(AL_ALL, russian));
	for (int i=0;i<n;i++) {
		if (IsDotName(names[i]))
			continue;
		char path[512];
		if (_snprintf_s(path, sizeof(path), _TRUNCATE, "DCIM/%s", names[i])<0)
			continue;
		ULONGLONG sz=0, mt=0, bt=0;
		int kind=g_io.stat ? g_io.stat(conn, path, &sz, &mt, &bt) : 2;
		if (kind==1) {
			FILETIME when;
			if (!AppleFileTimeFromAfcRaw(bt, &when))
				AppleFileTimeFromAfcRaw(mt, &when);
			AddDcimFile(lib, path, names[i], sz, &when);
		} else {
			WalkDcimDir(conn, lib, path);
		}
	}
	FreeNames(names, n);
	RemoveEmpty(lib);
	SortUserAlbums(lib);
	DropNameSets(lib);
	char line[80];
	_snprintf_s(line, sizeof(line), _TRUNCATE, "photos dcim assets=%d albums=%d",
		lib->nassets, lib->nalbums);
	Log(line);
	return lib;
}

static ULONGLONG DcimSig(void* conn)
{
	if (!g_io.list)
		return 0;
	char** names=NULL;
	int n=g_io.list(conn, "DCIM", &names);
	if (n<0)
		return 0;
	ULONGLONG sig=1469598103934665603ULL;
	for (int i=0;i<n;i++) {
		ULONGLONG sz=0, mt=0, bt=0;
		char path[300];
		_snprintf_s(path, sizeof(path), _TRUNCATE, "DCIM/%s", names[i] ? names[i] : "");
		if (g_io.stat)
			g_io.stat(conn, path, &sz, &mt, &bt);
		sig^=mt+bt+sz+(unsigned long long)(i+1);
		sig*=1099511628211ULL;
		const char* p=names[i] ? names[i] : "";
		for (; *p; p++) {
			sig^=(unsigned char)*p;
			sig*=1099511628211ULL;
		}
	}
	FreeNames(names, n);
	return sig ? sig : 1;
}

struct DbSig {
	int present;
	ULONGLONG size, mtime, walSize, walMtime;
};

static void StatDb(void* conn, DbSig* s)
{
	memset(s, 0, sizeof(*s));
	if (!g_io.stat || !conn)
		return;
	ULONGLONG sz=0, mt=0, bt=0;
	int k=g_io.stat(conn, "PhotoData/Photos.sqlite", &sz, &mt, &bt);
	if (k==1 && sz>0) {
		s->present=1;
		s->size=sz;
		s->mtime=mt ? mt : bt;
	}
	sz=0; mt=0; bt=0;
	k=g_io.stat(conn, "PhotoData/Photos.sqlite-wal", &sz, &mt, &bt);
	if (k==1) {
		s->walSize=sz;
		s->walMtime=mt ? mt : bt;
	}
}

static BOOL SameSql(PhotoLib* lib, const DbSig* s)
{
	/* The phone rewrites the -wal and touches mtime while we read it.
	   Matching those made every folder and every delete rebuild the
	   library and forget which files were just removed. Size is stable
	   until the library itself grows. */
	return lib && lib->fromSqlite && s && s->present &&
		lib->sqlSize==s->size && lib->sqlSize>0;
}

static BOOL SameFailed(PhotoLib* lib, const DbSig* s)
{
	return lib && lib->sqlFailed && s && s->present &&
		lib->failSize==s->size && lib->failMtime==s->mtime &&
		lib->failWalSize==s->walSize && lib->failWalMtime==s->walMtime;
}

static ULONGLONG LocalFileBytes(const WCHAR* path)
{
	WIN32_FILE_ATTRIBUTE_DATA a;
	if (!path || !GetFileAttributesExW(path, GetFileExInfoStandard, &a))
		return 0;
	if (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return 0;
	return ((ULONGLONG)a.nFileSizeHigh<<32) | a.nFileSizeLow;
}

static BOOL PhotoTempDir(const WCHAR* udid, WCHAR* out, int cch)
{
	WCHAR temp[MAX_PATH];
	DWORD n=GetTempPathW(MAX_PATH, temp);
	if (!n || n>=MAX_PATH)
		return FALSE;
	WCHAR root[MAX_PATH];
	if (swprintf_s(root, MAX_PATH, L"%sMobileDevices-photos", temp)<0)
		return FALSE;
	CreateDirectoryW(root, NULL);
	WCHAR safe[64];
	int j=0;
	if (udid) {
		for (int i=0; udid[i] && j<48; i++) {
			WCHAR c=udid[i];
			if ((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'-'||c==L'_')
				safe[j++]=c;
			else
				safe[j++]=L'_';
		}
	}
	safe[j]=0;
	if (!safe[0])
		WcsCopy(safe, 64, L"phone");
	if (swprintf_s(out, (size_t)cch, L"%s\\%s", root, safe)<0)
		return FALSE;
	CreateDirectoryW(out, NULL);
	return TRUE;
}

static BOOL PullMatching(void* conn, const char* remote, const WCHAR* local, ULONGLONG remoteSize)
{
	if (!remoteSize) {
		DeleteFileW(local);
		return TRUE;
	}
	if (LocalFileBytes(local)==remoteSize)
		return TRUE;
	if (!g_io.pull)
		return FALSE;
	WCHAR part[MAX_PATH];
	if (swprintf_s(part, MAX_PATH, L"%s.part", local)<0)
		return FALSE;
	DeleteFileW(part);
	if (g_io.pull(conn, remote, part)!=0) {
		DeleteFileW(part);
		return FALSE;
	}
	if (!LocalFileBytes(part)) {
		DeleteFileW(part);
		return FALSE;
	}
	DeleteFileW(local);
	if (!MoveFileW(part, local)) {
		DeleteFileW(part);
		return FALSE;
	}
	return TRUE;
}

static BOOL PullDb(void* conn, const WCHAR* udid, WCHAR* dbPath, int cch)
{
	if (!g_io.stat)
		return FALSE;
	WCHAR dir[MAX_PATH];
	if (!PhotoTempDir(udid, dir, MAX_PATH))
		return FALSE;
	WCHAR db[MAX_PATH], wal[MAX_PATH], shm[MAX_PATH];
	if (swprintf_s(db, MAX_PATH, L"%s\\Photos.sqlite", dir)<0)
		return FALSE;
	if (swprintf_s(wal, MAX_PATH, L"%s\\Photos.sqlite-wal", dir)<0)
		return FALSE;
	if (swprintf_s(shm, MAX_PATH, L"%s\\Photos.sqlite-shm", dir)<0)
		return FALSE;
	DeleteFileW(shm);
	ULONGLONG sz=0, mt=0, bt=0, walSz=0;
	if (g_io.stat(conn, "PhotoData/Photos.sqlite", &sz, &mt, &bt)!=1 || !sz)
		return FALSE;
	mt=0; bt=0;
	if (g_io.stat(conn, "PhotoData/Photos.sqlite-wal", &walSz, &mt, &bt)!=1)
		walSz=0;
	if (LocalFileBytes(db)==sz)
		Log("photos reuse PhotoData/Photos.sqlite");
	else
		Log("photos pull PhotoData/Photos.sqlite");
	if (!PullMatching(conn, "PhotoData/Photos.sqlite", db, sz)) {
		Log("photos pull fail");
		return FALSE;
	}
	if (walSz) {
		if (LocalFileBytes(wal)!=walSz)
			Log("photos pull PhotoData/Photos.sqlite-wal");
		if (!PullMatching(conn, "PhotoData/Photos.sqlite-wal", wal, walSz))
			DeleteFileW(wal);
	} else
		DeleteFileW(wal);
	WcsCopy(dbPath, cch, db);
	return LocalFileBytes(db)>0;
}

static PhotoLib* FindLib(const WCHAR* key)
{
	if (!key)
		return NULL;
	for (int i=0;i<g_nlibs;i++) {
		if (g_libs[i] && _wcsicmp(g_libs[i]->key, key)==0)
			return g_libs[i];
	}
	return NULL;
}

static void ReplaceLib(PhotoLib* neu)
{
	for (int i=0;i<g_nlibs;i++) {
		if (g_libs[i] && _wcsicmp(g_libs[i]->key, neu->key)==0) {
			FreeLib(g_libs[i]);
			g_libs[i]=neu;
			return;
		}
	}
	if (g_nlibs<8) {
		g_libs[g_nlibs++]=neu;
		return;
	}
	FreeLib(g_libs[0]);
	memmove(&g_libs[0], &g_libs[1], (size_t)(g_nlibs-1)*sizeof(g_libs[0]));
	g_libs[g_nlibs-1]=neu;
}

void ApplePhotosSetIo(const AppleAfcIo* io)
{
	if (io)
		g_io=*io;
	else
		memset(&g_io, 0, sizeof(g_io));
	PhLock();
	PhUnlock();
}

/* PhLock covers the in-memory library only. Pulling Photos.sqlite and
   parsing it run outside that lock: another listing, resolve, or log
   must not wait on the USB transfer. One build at a time; other callers
   wait on g_buildEvent. AFC itself is not thread-safe, so stat/pull/list
   take IoLock and never nest it with PhLock. The thread that starts the
   build still blocks until the library is ready. */
BOOL ApplePhotosEnsure(void* conn, const WCHAR* udid, int russian)
{
	if (!conn || !udid || !udid[0])
		return FALSE;
	russian=russian ? 1 : 0;
	for (;;) {
		PhLock();
		if (g_buildBusy) {
			BuildWait();
			continue;
		}
		PhUnlock();

		DbSig sig;
		IoLock();
		StatDb(conn, &sig);
		IoUnlock();

		PhLock();
		if (g_buildBusy) {
			PhUnlock();
			continue;
		}
		PhotoLib* cur=FindLib(udid);
		DWORD now=GetTickCount();
		if (cur && SameSql(cur, &sig)) {
			BOOL walMoved=cur->sqlMtime!=sig.mtime || cur->walSize!=sig.walSize || cur->walMtime!=sig.walMtime;
			DWORD age=now-cur->builtTick;
			if (!walMoved || age<180000) {
				if (cur->russian!=russian)
					ApplyLang(cur, russian);
				BOOL ok=cur->nalbums>0;
				PhUnlock();
				return ok;
			}
		}
		if (cur && SameFailed(cur, &sig)) {
			if (cur->russian!=russian)
				ApplyLang(cur, russian);
			BOOL ok=cur->nalbums>0;
			PhUnlock();
			return ok;
		}
		BOOL skipSql=FALSE;
		if (sig.present && cur && cur->sqlFailTick && (DWORD)(now-cur->sqlFailTick)<20000)
			skipSql=TRUE;
		BuildArm(udid);
		PhUnlock();

		PhotoLib* neu=NULL;
		if (sig.present && !skipSql && g_io.pull) {
			WCHAR dbPath[MAX_PATH];
			IoLock();
			BOOL pulled=PullDb(conn, udid, dbPath, MAX_PATH);
			IoUnlock();
			if (pulled) {
				neu=BuildLibFromDb(dbPath, russian);
				if (neu && neu->nalbums<=0) {
					FreeLib(neu);
					neu=NULL;
				}
				if (neu) {
					neu->fromSqlite=1;
					neu->sqlSize=sig.size;
					neu->sqlMtime=sig.mtime;
					neu->walSize=sig.walSize;
					neu->walMtime=sig.walMtime;
					now=GetTickCount();
					neu->builtTick=now ? now : 1;
				}
			}
			if (!neu)
				Log("photos sqlite unusable");
		}
		if (!neu) {
			IoLock();
			ULONGLONG ds=DcimSig(conn);
			IoUnlock();
			PhLock();
			cur=FindLib(udid);
			if (cur && !cur->fromSqlite && cur->dcimSig && cur->dcimSig==ds && cur->nalbums>0) {
				if (sig.present) {
					now=GetTickCount();
					cur->sqlFailTick=now ? now : 1;
				}
				if (cur->russian!=russian)
					ApplyLang(cur, russian);
				BuildSignal();
				PhUnlock();
				return TRUE;
			}
			PhUnlock();
			IoLock();
			neu=BuildFromDcim(conn, russian);
			IoUnlock();
			if (neu && neu->nalbums<=0) {
				FreeLib(neu);
				neu=NULL;
			}
			if (neu) {
				neu->fromSqlite=0;
				neu->dcimSig=ds ? ds : 1;
				if (sig.present) {
					neu->sqlFailed=1;
					neu->failSize=sig.size;
					neu->failMtime=sig.mtime;
					neu->failWalSize=sig.walSize;
					neu->failWalMtime=sig.walMtime;
					now=GetTickCount();
					neu->sqlFailTick=now ? now : 1;
				}
			}
		}
		PhLock();
		if (!neu) {
			cur=FindLib(udid);
			if (cur && sig.present) {
				now=GetTickCount();
				cur->sqlFailTick=now ? now : 1;
			}
			BuildSignal();
			PhUnlock();
			return FALSE;
		}
		WcsCopy(neu->key, 80, udid);
		neu->russian=russian;
		ReplaceLib(neu);
		BuildSignal();
		PhUnlock();
		return TRUE;
	}
}

static unsigned PathHash(const char* s)
{
	unsigned h=2166136261u;
	if (!s)
		return h;
	for (; *s; s++) {
		h^=(unsigned char)*s;
		h*=16777619u;
	}
	return h;
}

static void PathSetFree(PathSet* s)
{
	if (!s)
		return;
	free(s->key);
	free(s->used);
	memset(s, 0, sizeof(*s));
}

static BOOL PathSetRehash(PathSet* s, int cap)
{
	char** key=(char**)calloc((size_t)cap, sizeof(char*));
	unsigned char* used=(unsigned char*)calloc((size_t)cap, 1);
	if (!key || !used || cap<=0 || (cap&(cap-1))!=0) {
		free(key);
		free(used);
		return FALSE;
	}
	unsigned mask=(unsigned)cap-1u;
	int placed=0;
	for (int i=0; s->used && i<s->cap; i++) {
		if (!s->used[i] || !s->key[i])
			continue;
		unsigned h=PathHash(s->key[i]);
		int found=0;
		for (int j=0;j<cap;j++) {
			unsigned slot=(h+(unsigned)j)&mask;
			if (!used[slot]) {
				used[slot]=1;
				key[slot]=s->key[i];
				placed++;
				found=1;
				break;
			}
		}
		if (!found) {
			free(key);
			free(used);
			return FALSE;
		}
	}
	if (placed!=s->n) {
		free(key);
		free(used);
		return FALSE;
	}
	free(s->key);
	free(s->used);
	s->key=key;
	s->used=used;
	s->cap=cap;
	return TRUE;
}

static void PathSetDisable(PathSet* s)
{
	if (!s)
		return;
	PathSetFree(s);
	s->failed=1;
}

static BOOL PathSetHas(const PathSet* s, const char* path)
{
	if (!s || !s->cap || !path)
		return FALSE;
	unsigned mask=(unsigned)s->cap-1u;
	unsigned h=PathHash(path);
	for (int i=0;i<s->cap;i++) {
		unsigned slot=(h+(unsigned)i)&mask;
		if (!s->used[slot])
			return FALSE;
		if (s->key[slot] && strcmp(s->key[slot], path)==0)
			return TRUE;
	}
	return FALSE;
}

static BOOL PathSetPut(PathSet* s, char* path)
{
	if (!s || !path || s->failed)
		return FALSE;
	if (s->cap<16 || s->n*2>=s->cap) {
		int ncap=s->cap ? s->cap*2 : 16;
		if (!PathSetRehash(s, ncap))
			return FALSE;
	}
	unsigned mask=(unsigned)s->cap-1u;
	unsigned h=PathHash(path);
	for (int i=0;i<s->cap;i++) {
		unsigned slot=(h+(unsigned)i)&mask;
		if (!s->used[slot]) {
			s->used[slot]=1;
			s->key[slot]=path;
			s->n++;
			return TRUE;
		}
		if (s->key[slot] && strcmp(s->key[slot], path)==0)
			return TRUE;
	}
	return FALSE;
}

struct TombKeep {
	WCHAR key[80];
	char** paths;
	int n, cap;
	int loaded;
	PathSet set;
};

static TombKeep g_keep[4];
static int g_nkeep=0;

static BOOL TombPathOk(const char* path)
{
	return path && path[0] && !strchr(path, '\n') && !strchr(path, '\r') && strlen(path)<500;
}

static TombKeep* KeepFind(const WCHAR* key, int create)
{
	if (!key || !key[0])
		return NULL;
	for (int i=0;i<g_nkeep;i++) {
		if (_wcsicmp(g_keep[i].key, key)==0)
			return &g_keep[i];
	}
	if (!create || g_nkeep>=4)
		return NULL;
	TombKeep* k=&g_keep[g_nkeep++];
	memset(k, 0, sizeof(*k));
	WcsCopy(k->key, 80, key);
	return k;
}

static void KeepPath(const TombKeep* k, WCHAR* out, int cch)
{
	out[0]=0;
	WCHAR dir[MAX_PATH];
	if (!k || !PhotoTempDir(k->key, dir, MAX_PATH))
		return;
	swprintf_s(out, (size_t)cch, L"%s\\removed.txt", dir);
}

static BOOL KeepHas(TombKeep* k, const char* path)
{
	if (!k || !path)
		return FALSE;
	if (!k->set.failed && k->set.cap)
		return PathSetHas(&k->set, path);
	if (!k->set.failed)
		return FALSE;
	for (int i=0;i<k->n;i++) {
		if (k->paths[i] && strcmp(k->paths[i], path)==0)
			return TRUE;
	}
	return FALSE;
}

static void KeepAddMem(TombKeep* k, const char* path)
{
	if (!k || !TombPathOk(path) || KeepHas(k, path) || k->n>=4096)
		return;
	if (k->n>=k->cap) {
		int ncap=k->cap ? k->cap*2 : 8;
		char** p=(char**)realloc(k->paths, (size_t)ncap*sizeof(char*));
		if (!p)
			return;
		k->paths=p;
		k->cap=ncap;
	}
	char* d=StrDup(path);
	if (!d)
		return;
	k->paths[k->n++]=d;
	if (!k->set.failed && !PathSetPut(&k->set, d))
		PathSetDisable(&k->set);
}

static void KeepLoad(TombKeep* k)
{
	if (!k || k->loaded)
		return;
	k->loaded=1;
	WCHAR file[MAX_PATH];
	KeepPath(k, file, MAX_PATH);
	if (!file[0])
		return;
	FILE* f=NULL;
	if (_wfopen_s(&f, file, L"rb")!=0 || !f)
		return;
	char line[512];
	while (fgets(line, (int)sizeof(line), f)) {
		char* p=line;
		size_t n=strlen(p);
		while (n && (p[n-1]=='\n' || p[n-1]=='\r'))
			p[--n]=0;
		KeepAddMem(k, p);
	}
	fclose(f);
}

static void KeepRemember(const WCHAR* udid, const char* path)
{
	TombKeep* k=KeepFind(udid, 1);
	if (!k)
		return;
	KeepLoad(k);
	if (KeepHas(k, path))
		return;
	KeepAddMem(k, path);
	WCHAR file[MAX_PATH];
	KeepPath(k, file, MAX_PATH);
	if (!file[0])
		return;
	FILE* f=NULL;
	if (_wfopen_s(&f, file, L"ab")!=0 || !f)
		return;
	fwrite(path, 1, strlen(path), f);
	fputc('\n', f);
	fclose(f);
}

static void KeepFreeAll(void)
{
	for (int i=0;i<g_nkeep;i++) {
		for (int j=0;j<g_keep[i].n;j++)
			free(g_keep[i].paths[j]);
		free(g_keep[i].paths);
		PathSetFree(&g_keep[i].set);
		memset(&g_keep[i], 0, sizeof(g_keep[i]));
	}
	g_nkeep=0;
}

static BOOL TombLibHas(PhotoLib* lib, const char* path)
{
	if (!lib || !path)
		return FALSE;
	if (!lib->tombset.failed && lib->tombset.cap)
		return PathSetHas(&lib->tombset, path);
	if (!lib->tombset.failed)
		return FALSE;
	for (int i=0;i<lib->ntomb;i++) {
		if (lib->tombs[i] && strcmp(lib->tombs[i], path)==0)
			return TRUE;
	}
	return FALSE;
}

static BOOL TombLibAdd(PhotoLib* lib, const char* path)
{
	if (!lib || !path || lib->ntomb>=4096)
		return FALSE;
	if (lib->ntomb>=lib->tombCap) {
		int ncap=lib->tombCap ? lib->tombCap*2 : 8;
		char** p=(char**)realloc(lib->tombs, (size_t)ncap*sizeof(char*));
		if (!p)
			return FALSE;
		lib->tombs=p;
		lib->tombCap=ncap;
	}
	char* d=StrDup(path);
	if (!d)
		return FALSE;
	lib->tombs[lib->ntomb++]=d;
	if (!lib->tombset.failed && !PathSetPut(&lib->tombset, d))
		PathSetDisable(&lib->tombset);
	return TRUE;
}

static BOOL Tomb(PhotoLib* lib, const char* path)
{
	if (!path)
		return FALSE;
	if (lib) {
		if (TombLibHas(lib, path))
			return TRUE;
		TombKeep* k=KeepFind(lib->key, 0);
		if (!k && lib->key[0]) {
			k=KeepFind(lib->key, 1);
			KeepLoad(k);
		}
		if (KeepHas(k, path))
			return TRUE;
	}
	return FALSE;
}

static void SplitVirt(const char* virt, WCHAR* album, int albumCch, WCHAR* file, int fileCch)
{
	if (album && albumCch>0)
		album[0]=0;
	if (file && fileCch>0)
		file[0]=0;
	if (!virt)
		return;
	while (*virt=='/')
		virt++;
	const char* slash=strchr(virt, '/');
	if (!slash) {
		Utf8ToWide(virt, album, albumCch);
		return;
	}
	char tmp[512];
	size_t n=(size_t)(slash-virt);
	if (n>=sizeof(tmp))
		n=sizeof(tmp)-1;
	memcpy(tmp, virt, n);
	tmp[n]=0;
	Utf8ToWide(tmp, album, albumCch);
	Utf8ToWide(slash+1, file, fileCch);
}

static PhotoAlbum* FindAlbum(PhotoLib* lib, const WCHAR* name)
{
	if (!lib || !name || !name[0])
		return NULL;
	for (int i=0;i<lib->nalbums;i++) {
		if (lib->albums[i].name && WcsIcmp(lib->albums[i].name, name)==0)
			return &lib->albums[i];
	}
	return NULL;
}

static int CmpFile(const void* a, const void* b)
{
	const WIN32_FIND_DATAW* x=(const WIN32_FIND_DATAW*)a;
	const WIN32_FIND_DATAW* y=(const WIN32_FIND_DATAW*)b;
	BOOL vx=FtValid(&x->ftLastWriteTime);
	BOOL vy=FtValid(&y->ftLastWriteTime);
	if (vx!=vy)
		return vx ? -1 : 1;
	if (vx) {
		int c=CompareFileTime(&y->ftLastWriteTime, &x->ftLastWriteTime);
		if (c)
			return c;
	}
	int r=CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, x->cFileName, -1, y->cFileName, -1);
	if (r==0)
		return 0;
	return r-2;
}

static void FillDirFd(WIN32_FIND_DATAW* fd, const WCHAR* name, const FILETIME* when)
{
	memset(fd, 0, sizeof(*fd));
	WcsCopy(fd->cFileName, MAX_PATH, name);
	fd->dwFileAttributes=FILE_ATTRIBUTE_DIRECTORY;
	if (when && FtValid(when)) {
		fd->ftLastWriteTime=*when;
		fd->ftCreationTime=*when;
	} else
		FtNone(&fd->ftLastWriteTime);
}

static void FillFileFd(WIN32_FIND_DATAW* fd, const WCHAR* name, const PhotoAsset* a)
{
	memset(fd, 0, sizeof(*fd));
	WcsCopy(fd->cFileName, MAX_PATH, name);
	fd->dwFileAttributes=FILE_ATTRIBUTE_NORMAL;
	if (a) {
		fd->nFileSizeLow=(DWORD)a->size;
		fd->nFileSizeHigh=(DWORD)(a->size>>32);
		if (FtValid(&a->when)) {
			fd->ftCreationTime=a->when;
			fd->ftLastWriteTime=a->when;
		} else
			FtNone(&fd->ftLastWriteTime);
	} else
		FtNone(&fd->ftLastWriteTime);
}

static BOOL Emit(PhotoLib* lib, const char* virt, WIN32_FIND_DATAW** ents, int* nent)
{
	WCHAR album[260], file[260];
	SplitVirt(virt, album, 260, file, 260);
	if (file[0])
		return FALSE;
	if (!album[0]) {
		if (lib->nalbums<=0)
			return FALSE;
		WIN32_FIND_DATAW* out=(WIN32_FIND_DATAW*)calloc((size_t)lib->nalbums, sizeof(WIN32_FIND_DATAW));
		if (!out)
			return FALSE;
		for (int i=0;i<lib->nalbums;i++)
			FillDirFd(&out[i], lib->albums[i].name, &lib->albums[i].newest);
		*ents=out;
		*nent=lib->nalbums;
		return TRUE;
	}
	PhotoAlbum* al=FindAlbum(lib, album);
	if (!al || al->n<=0)
		return FALSE;
	WIN32_FIND_DATAW* out=(WIN32_FIND_DATAW*)calloc((size_t)al->n, sizeof(WIN32_FIND_DATAW));
	if (!out)
		return FALSE;
	int w=0;
	for (int i=0;i<al->n;i++) {
		int xi=al->ids[i];
		if (xi<0 || xi>=lib->nassets)
			continue;
		if (Tomb(lib, lib->assets[xi].path))
			continue;
		FillFileFd(&out[w], al->names[i], &lib->assets[xi]);
		w++;
	}
	if (w<=0) {
		free(out);
		return FALSE;
	}
	qsort(out, (size_t)w, sizeof(WIN32_FIND_DATAW), CmpFile);
	*ents=out;
	*nent=w;
	return TRUE;
}

BOOL ApplePhotosList(void* conn, const WCHAR* udid, const char* virtUtf8, int russian,
	WIN32_FIND_DATAW** ents, int* nent)
{
	if (!ents || !nent)
		return FALSE;
	*ents=NULL;
	*nent=0;
	if (!ApplePhotosEnsure(conn, udid, russian))
		return FALSE;
	PhLock();
	PhotoLib* lib=FindLib(udid);
	BOOL ok=lib && Emit(lib, virtUtf8, ents, nent);
	PhUnlock();
	return ok;
}

static BOOL ResolveLib(PhotoLib* lib, const char* virt, char* out, int cch)
{
	if (!lib || !out || cch<=0)
		return FALSE;
	out[0]=0;
	WCHAR album[260], file[260];
	SplitVirt(virt, album, 260, file, 260);
	if (!album[0] || !file[0] || wcschr(file, L'/') || wcschr(file, L'\\'))
		return FALSE;
	PhotoAlbum* al=FindAlbum(lib, album);
	if (!al)
		return FALSE;
	for (int i=0;i<al->n;i++) {
		if (!al->names[i] || WcsIcmp(al->names[i], file)!=0)
			continue;
		int xi=al->ids[i];
		if (xi<0 || xi>=lib->nassets || !lib->assets[xi].path)
			return FALSE;
		if (Tomb(lib, lib->assets[xi].path))
			return FALSE;
		return strncpy_s(out, (size_t)cch, lib->assets[xi].path, _TRUNCATE)==0;
	}
	return FALSE;
}

BOOL ApplePhotosResolve(const WCHAR* udid, const char* virtUtf8, char* afcPath, int cch)
{
	if (!afcPath || cch<=0)
		return FALSE;
	afcPath[0]=0;
	PhLock();
	for (;;) {
		PhotoLib* lib=FindLib(udid);
		if (lib || !g_buildBusy) {
			BOOL ok=ResolveLib(lib, virtUtf8, afcPath, cch);
			PhUnlock();
			return ok;
		}
		BuildWait();
		PhLock();
	}
}

static BOOL BuildThumbAfc(const char* asset, const char* leaf, char* out, int cch)
{
	if (!out || cch<32 || !asset || !asset[0] || !leaf)
		return FALSE;
	if (strchr(asset, '\\') || strstr(asset, ".."))
		return FALSE;
	return _snprintf_s(out, (size_t)cch, _TRUNCATE,
		"PhotoData/Thumbnails/V2/%s/%s", asset, leaf)>0;
}

static unsigned long long ThumbHash(const char* s)
{
	unsigned long long h=1469598103934665603ULL;
	if (!s)
		return h;
	for (; *s; s++) {
		h^=(unsigned char)*s;
		h*=1099511628211ULL;
	}
	return h;
}

static BOOL ThumbLocalPath(const WCHAR* udid, const char* afc, WCHAR* out, int cch)
{
	WCHAR dir[MAX_PATH];
	if (!ThumbHash(afc) || !PhotoTempDir(udid, dir, MAX_PATH))
		return FALSE;
	WCHAR sub[MAX_PATH];
	if (swprintf_s(sub, MAX_PATH, L"%s\\thumbs", dir)<0)
		return FALSE;
	CreateDirectoryW(sub, NULL);
	unsigned long long h=ThumbHash(afc);
	return swprintf_s(out, (size_t)cch, L"%s\\%016llx.jpg", sub, h)>0;
}

static BOOL ThumbMissed(PhotoLib* lib, const char* asset)
{
	if (!lib || !asset)
		return FALSE;
	for (int i=0;i<lib->nThumbMiss;i++) {
		if (lib->thumbMiss[i] && strcmp(lib->thumbMiss[i], asset)==0)
			return TRUE;
	}
	return FALSE;
}

static void ThumbRememberMiss(PhotoLib* lib, const char* asset)
{
	if (!lib || !asset || lib->nThumbMiss>=4096 || ThumbMissed(lib, asset))
		return;
	if (lib->nThumbMiss>=lib->thumbMissCap) {
		int ncap=lib->thumbMissCap ? lib->thumbMissCap*2 : 16;
		char** p=(char**)realloc(lib->thumbMiss, (size_t)ncap*sizeof(char*));
		if (!p)
			return;
		lib->thumbMiss=p;
		lib->thumbMissCap=ncap;
	}
	char* d=StrDup(asset);
	if (d)
		lib->thumbMiss[lib->nThumbMiss++]=d;
}

static BOOL PullThumbOne(void* conn, const char* remote, const WCHAR* local)
{
	if (!conn || !remote || !remote[0] || !local || !g_io.stat || !g_io.pull)
		return FALSE;
	if (LocalFileBytes(local)>0)
		return TRUE;
	ULONGLONG sz=0;
	int kind=g_io.stat(conn, remote, &sz, NULL, NULL);
	if (kind==0)
		return FALSE;
	if (kind!=1 || sz==0 || sz>3ULL*1024ULL*1024ULL)
		return FALSE;
	WCHAR part[MAX_PATH];
	if (swprintf_s(part, MAX_PATH, L"%s.part", local)<0)
		return FALSE;
	DeleteFileW(part);
	if (g_io.pull(conn, remote, part)!=0) {
		DeleteFileW(part);
		return FALSE;
	}
	if (!LocalFileBytes(part)) {
		DeleteFileW(part);
		return FALSE;
	}
	DeleteFileW(local);
	if (!MoveFileW(part, local)) {
		DeleteFileW(part);
		return FALSE;
	}
	return LocalFileBytes(local)>0;
}

BOOL ApplePhotosLoadThumb(void* conn, const WCHAR* udid, const char* virtUtf8, WCHAR* localJpeg, int cch)
{
	if (!conn || !udid || !virtUtf8 || !localJpeg || cch<=0)
		return FALSE;
	localJpeg[0]=0;
	PhLock();
	PhotoLib* lib=NULL;
	for (;;) {
		lib=FindLib(udid);
		if (lib || !g_buildBusy)
			break;
		BuildWait();
		PhLock();
	}
	char asset[512];
	if (!ResolveLib(lib, virtUtf8, asset, (int)sizeof(asset))) {
		PhUnlock();
		return FALSE;
	}
	if (ThumbMissed(lib, asset)) {
		PhUnlock();
		return FALSE;
	}
	const char* leaves[2]={"5003.JPG", "5005.JPG"};
	char remote[2][512];
	WCHAR local[2][MAX_PATH];
	int n=0;
	for (int i=0;i<2;i++) {
		if (!BuildThumbAfc(asset, leaves[i], remote[n], 512))
			continue;
		if (!ThumbLocalPath(udid, remote[n], local[n], MAX_PATH))
			continue;
		n++;
	}
	for (int i=0;i<n;i++) {
		if (LocalFileBytes(local[i])>0) {
			WcsCopy(localJpeg, cch, local[i]);
			PhUnlock();
			return localJpeg[0]!=0;
		}
	}
	PhUnlock();
	/* AFC is not thread-safe and must not run under PhLock: the library
	   build holds IoLock across the sqlite download. */
	IoLock();
	int missing=0;
	int got=-1;
	for (int i=0;i<n;i++) {
		ULONGLONG sz=0;
		int kind=g_io.stat ? g_io.stat(conn, remote[i], &sz, NULL, NULL) : -1;
		if (kind==0) {
			missing++;
			continue;
		}
		if (PullThumbOne(conn, remote[i], local[i])) {
			got=i;
			break;
		}
	}
	IoUnlock();
	if (got<0) {
		if (n>0 && missing==n) {
			PhLock();
			lib=FindLib(udid);
			if (lib)
				ThumbRememberMiss(lib, asset);
			PhUnlock();
		}
		return FALSE;
	}
	WcsCopy(localJpeg, cch, local[got]);
	static int logged=0;
	if (!logged) {
		logged=1;
		Log("photos thumb ok");
	}
	return localJpeg[0]!=0;
}

void ApplePhotosTombstone(const WCHAR* udid, const char* afcPath)
{
	if (!udid || !afcPath || !afcPath[0])
		return;
	PhLock();
	KeepRemember(udid, afcPath);
	PhotoLib* lib=FindLib(udid);
	if (lib && !Tomb(lib, afcPath))
		TombLibAdd(lib, afcPath);
	PhUnlock();
}

void ApplePhotosShutdown(void)
{
	if (g_csState!=2)
		return;
	for (;;) {
		EnterCriticalSection(&g_cs);
		if (!g_buildBusy)
			break;
		HANDLE ev=g_buildEvent;
		LeaveCriticalSection(&g_cs);
		if (ev)
			WaitForSingleObject(ev, INFINITE);
		else
			Sleep(30);
	}
	for (int i=0;i<g_nlibs;i++) {
		FreeLib(g_libs[i]);
		g_libs[i]=NULL;
	}
	g_nlibs=0;
	KeepFreeAll();
	HANDLE ev=g_buildEvent;
	g_buildEvent=NULL;
	LeaveCriticalSection(&g_cs);
	if (ev)
		CloseHandle(ev);
	DeleteCriticalSection(&g_cs);
	g_csState=0;
	if (g_ioCsState==2) {
		DeleteCriticalSection(&g_ioCs);
		g_ioCsState=0;
	}
}

#ifdef PHOTOS_SELFTEST
static int g_fail=0;
static void Expect(int cond, const char* msg)
{
	if (!cond) {
		printf("FAIL %s\n", msg);
		g_fail++;
	}
}

static PhotoAlbum* BySmart(PhotoLib* lib, int smart)
{
	for (int i=0;i<lib->nalbums;i++) {
		if (lib->albums[i].smart==smart)
			return &lib->albums[i];
	}
	return NULL;
}

static int AlbumHasPath(PhotoLib* lib, PhotoAlbum* al, const char* path)
{
	if (!al)
		return 0;
	for (int i=0;i<al->n;i++) {
		int xi=al->ids[i];
		if (xi>=0 && xi<lib->nassets && lib->assets[xi].path && strcmp(lib->assets[xi].path, path)==0)
			return 1;
	}
	return 0;
}

static int AlbumHasName(PhotoAlbum* al, const WCHAR* name)
{
	if (!al)
		return 0;
	for (int i=0;i<al->n;i++) {
		if (al->names[i] && WcsIcmp(al->names[i], name)==0)
			return 1;
	}
	return 0;
}

static int AlbumHasExact(PhotoAlbum* al, const WCHAR* name)
{
	if (!al || !name)
		return 0;
	for (int i=0;i<al->n;i++) {
		if (al->names[i] && wcscmp(al->names[i], name)==0)
			return 1;
	}
	return 0;
}

int main(int argc, char** argv)
{
	if (argc>=2) {
		WCHAR w[MAX_PATH];
		MultiByteToWideChar(CP_ACP, 0, argv[1], -1, w, MAX_PATH);
		PhotoLib* probe=BuildLibFromDb(w, 1);
		if (!probe) {
			printf("probe fail\n");
			return 1;
		}
		int months=0, users=0;
		FILE* out=NULL;
		WCHAR probeOut[MAX_PATH];
		probeOut[0]=0;
		if (GetTempPathW(MAX_PATH, probeOut))
			wcscat_s(probeOut, MAX_PATH, L"photos_probe.txt");
		if (probeOut[0])
			_wfopen_s(&out, probeOut, L"wb");
		if (out)
			wprintf(L"probe %s\n", probeOut);
		printf("assets %d albums %d\n", probe->nassets, probe->nalbums);
		for (int i=0;i<probe->nalbums;i++) {
			if (probe->albums[i].smart==AL_MONTH)
				months++;
			if (probe->albums[i].smart==AL_USER)
				users++;
			char name[512];
			WideCharToMultiByte(CP_UTF8, 0, probe->albums[i].name ? probe->albums[i].name : L"",
				-1, name, 512, NULL, NULL);
			printf("smart %d n %d %s\n", probe->albums[i].smart, probe->albums[i].n, name);
			if (out) {
				fwrite(name, 1, strlen(name), out);
				fprintf(out, "\t%d\t%d\n", probe->albums[i].smart, probe->albums[i].n);
			}
		}
		if (out)
			fclose(out);
		printf("months %d users %d\n", months, users);
		FreeLib(probe);
		return 0;
	}
	FILETIME a, b, c, d;
	Expect(AppleFileTimeFromAfcRaw(1705320000ULL, &a), "sec");
	Expect(AppleFileTimeFromAfcRaw(1705320000ULL*1000ULL, &b), "ms");
	Expect(AppleFileTimeFromAfcRaw(1705320000ULL*1000000ULL, &c), "us");
	Expect(AppleFileTimeFromAfcRaw(1705320000ULL*1000000000ULL, &d), "ns");
	Expect(a.dwLowDateTime==b.dwLowDateTime && a.dwHighDateTime==b.dwHighDateTime, "ms match");
	Expect(a.dwLowDateTime==c.dwLowDateTime && a.dwHighDateTime==c.dwHighDateTime, "us match");
	Expect(a.dwLowDateTime==d.dwLowDateTime && a.dwHighDateTime==d.dwHighDateTime, "ns match");
	SYSTEMTIME st;
	Expect(FileTimeToSystemTime(&a, &st), "ft");
	Expect(st.wYear==2024 && st.wMonth==1 && st.wDay==15 && st.wHour==12 && st.wMinute==0, "2024-01-15");

	Expect(LoadSqlite(), "sqlite");
	WCHAR dir[MAX_PATH];
	GetTempPathW(MAX_PATH, dir);
	WCHAR dbw[MAX_PATH];
	swprintf_s(dbw, MAX_PATH, L"%smd-photos-selftest.sqlite", dir);
	char dbu[MAX_PATH*3];
	WideToUtf8(dbw, dbu, (int)sizeof(dbu));
	DeleteFileW(dbw);
	sqlite3* db=NULL;
	Expect(pSqlOpen(dbu, &db, SQL_OPEN_READWRITE|SQL_OPEN_CREATE, NULL)==SQL_OK && db, "create");
	const char* schema=
		"CREATE TABLE ZASSET ("
		" Z_PK INTEGER PRIMARY KEY, ZFILENAME TEXT, ZDIRECTORY TEXT, ZDATECREATED REAL,"
		" ZKIND INTEGER, ZKINDSUBTYPE INTEGER, ZFAVORITE INTEGER, ZHIDDEN INTEGER,"
		" ZTRASHEDSTATE INTEGER, ZVISIBILITYSTATE INTEGER, ZHIGHFRAMERATESTATE INTEGER,"
		" ZDEPTHTYPE INTEGER, ZPLAYBACKSTYLE INTEGER, ZAVALANCHEPICKTYPE INTEGER,"
		" ZADDITIONALATTRIBUTES INTEGER);"
		"CREATE TABLE ZADDITIONALASSETATTRIBUTES ("
		" Z_PK INTEGER PRIMARY KEY, ZASSET INTEGER, ZORIGINALFILENAME TEXT,"
		" ZORIGINALFILESIZE INTEGER, ZCAMERACAPTUREDEVICE INTEGER);"
		"CREATE TABLE ZGENERICALBUM (Z_PK INTEGER PRIMARY KEY, ZTITLE TEXT, ZKIND INTEGER, ZTRASHEDSTATE INTEGER);"
		"CREATE TABLE Z_27ASSETS (Z_27ALBUMS INTEGER, Z_3ASSETS INTEGER, Z_FOK_27ALBUMS INTEGER);"
		"CREATE TABLE Z_4ASSETS (Z_FOK_4ALBUMS INTEGER, Z_FOK_3ASSETS INTEGER);";
	char* err=NULL;
	Expect(pSqlExec(db, schema, NULL, NULL, &err)==SQL_OK, err ? err : "schema");
	if (err) { pSqlFree(err); err=NULL; }
	const char* ins=
		"INSERT INTO ZADDITIONALASSETATTRIBUTES (Z_PK,ZORIGINALFILENAME,ZORIGINALFILESIZE,ZCAMERACAPTUREDEVICE) VALUES"
		"(1,'Vacation.HEIC',111,0),(2,NULL,0,1),(6,NULL,0,1),(10,'Same.JPG',10,0),(11,'Same.JPG',10,0);"
		"INSERT INTO ZASSET (Z_PK,ZFILENAME,ZDIRECTORY,ZDATECREATED,ZKIND,ZKINDSUBTYPE,ZFAVORITE,ZHIDDEN,ZTRASHEDSTATE,ZVISIBILITYSTATE,ZHIGHFRAMERATESTATE,ZDEPTHTYPE,ZPLAYBACKSTYLE,ZAVALANCHEPICKTYPE,ZADDITIONALATTRIBUTES) VALUES"
		"(1,'IMG_0001.HEIC','100APPLE',727012800,0,0,1,0,0,0,0,0,1,0,1),"
		"(2,'IMG_0002.PNG','100APPLE',727000000,0,10,0,0,0,0,0,0,1,0,2),"
		"(3,'IMG_0003.MOV','100APPLE',726000000,1,0,0,0,0,0,0,0,4,0,NULL),"
		"(4,'IMG_0004.HEIC','100APPLE',726000000,0,0,0,1,0,0,0,0,1,0,NULL),"
		"(5,'IMG_0005.HEIC','100APPLE',726000000,0,0,0,0,1,0,0,0,1,0,NULL),"
		"(6,'IMG_0006.HEIC','100APPLE',726000000,0,0,0,0,0,0,0,0,1,0,6),"
		"(7,'IMG_0007.HEIC','100APPLE',726000000,0,2,0,0,0,0,0,0,1,0,NULL),"
		"(8,'IMG_0008.HEIC','100APPLE',726000000,0,1,0,0,0,0,0,0,1,0,NULL),"
		"(9,'IMG_0009.HEIC','100APPLE',726000000,0,0,0,0,0,1,0,0,1,0,NULL),"
		"(10,'IMG_0010.JPG','100APPLE',726000000,0,0,0,0,0,0,0,0,1,0,10),"
		"(11,'IMG_0011.JPG','100APPLE',726000000,0,0,0,0,0,0,0,0,1,0,11),"
		"(12,'IMG_0012.MOV','100APPLE',726000000,1,0,0,0,0,0,1,0,4,0,NULL),"
		"(13,'IMG_0013.AAE','100APPLE',726000000,0,0,0,0,0,0,0,0,1,0,NULL),"
		"(14,'IMG_0014.HEIC','100APPLE',726000000,0,0,0,0,0,0,0,1,1,0,NULL),"
		"(15,'IMG_0099.HEIC','DCIM/108APPLE',727012800,0,0,0,0,0,0,0,0,1,0,NULL),"
		"(16,'IMG_0098.JPG','PhotoData/CPLAssets/group1',726000000,0,0,0,0,1,0,0,0,1,0,NULL);"
		"INSERT INTO ZGENERICALBUM (Z_PK,ZTITLE,ZKIND,ZTRASHEDSTATE) VALUES"
		"(1,NULL,2,0),(2,'Folder',4000,0),(3,'Shared',1505,0),(4,'Gone',2,1),(5,'Отпуск',2,0);"
		"INSERT INTO Z_27ASSETS (Z_27ALBUMS,Z_3ASSETS,Z_FOK_27ALBUMS) VALUES"
		"(5,1,0),(5,2,1),(5,10,2),(5,11,3),(5,999,4),(2,1,0);";
	Expect(pSqlExec(db, ins, NULL, NULL, &err)==SQL_OK, err ? err : "insert");
	if (err) { pSqlFree(err); err=NULL; }
	pSqlClose(db);

	PhotoLib* lib=BuildLibFromDb(dbw, 1);
	Expect(lib!=NULL, "build");
	if (!lib)
		return 1;
	Expect(lib->nalbums==12, "album count");
	Expect(lib->albums[0].name && wcscmp(lib->albums[0].name, L"Все фото")==0, "ru name");
	PhotoAlbum* all=BySmart(lib, AL_ALL);
	PhotoAlbum* fav=BySmart(lib, AL_FAV);
	PhotoAlbum* screen=BySmart(lib, AL_SCREEN);
	PhotoAlbum* selfie=BySmart(lib, AL_SELFIE);
	PhotoAlbum* live=BySmart(lib, AL_LIVE);
	PhotoAlbum* pano=BySmart(lib, AL_PANO);
	PhotoAlbum* port=BySmart(lib, AL_PORT);
	PhotoAlbum* video=BySmart(lib, AL_VIDEO);
	PhotoAlbum* slomo=BySmart(lib, AL_SLOMO);
	PhotoAlbum* hidden=BySmart(lib, AL_HIDDEN);
	PhotoAlbum* trash=BySmart(lib, AL_TRASH);
	PhotoAlbum* burst=BySmart(lib, AL_BURST);
	Expect(all && all->n==11, "all count");
	Expect(fav && fav->n==1, "fav");
	Expect(screen && screen->n==1, "screen");
	Expect(selfie && selfie->n==1, "selfie");
	Expect(live && live->n==1, "live");
	Expect(pano && pano->n==1, "pano");
	Expect(port && port->n==1, "port");
	Expect(video && video->n==2, "video");
	Expect(slomo && slomo->n==1, "slomo");
	Expect(hidden && hidden->n==1, "hidden");
	Expect(trash && trash->n==2, "trash");
	Expect(burst==NULL, "no empty burst");
	Expect(AlbumHasPath(lib, all, "DCIM/100APPLE/IMG_0001.HEIC"), "roll has 1");
	Expect(AlbumHasPath(lib, all, "DCIM/108APPLE/IMG_0099.HEIC"), "full dir");
	Expect(!AlbumHasPath(lib, all, "DCIM/DCIM/108APPLE/IMG_0099.HEIC"), "no doubled dcim");
	Expect(AlbumHasPath(lib, trash, "PhotoData/CPLAssets/group1/IMG_0098.JPG"), "cpl path");
	char thumb[256];
	Expect(BuildThumbAfc("DCIM/100APPLE/IMG_0001.HEIC", "5003.JPG", thumb, 256), "thumb build");
	Expect(strcmp(thumb, "PhotoData/Thumbnails/V2/DCIM/100APPLE/IMG_0001.HEIC/5003.JPG")==0, "thumb path");
	for (int i=0;i<lib->nalbums;i++)
		Expect(lib->albums[i].smart!=AL_MONTH, "no date folder");
	Expect(!AlbumHasPath(lib, all, "DCIM/100APPLE/IMG_0004.HEIC"), "hidden not in roll");
	Expect(!AlbumHasPath(lib, all, "DCIM/100APPLE/IMG_0005.HEIC"), "trash not in roll");
	Expect(!AlbumHasPath(lib, all, "DCIM/100APPLE/IMG_0009.HEIC"), "skip not in roll");
	Expect(!AlbumHasPath(lib, all, "DCIM/100APPLE/IMG_0013.AAE"), "no aae");
	Expect(AlbumHasPath(lib, screen, "DCIM/100APPLE/IMG_0002.PNG"), "screen file");
	Expect(!AlbumHasPath(lib, selfie, "DCIM/100APPLE/IMG_0002.PNG"), "screen not selfie");
	Expect(AlbumHasPath(lib, selfie, "DCIM/100APPLE/IMG_0006.HEIC"), "selfie file");
	Expect(AlbumHasPath(lib, slomo, "DCIM/100APPLE/IMG_0012.MOV"), "slomo file");
	Expect(AlbumHasPath(lib, video, "DCIM/100APPLE/IMG_0012.MOV"), "slomo also video");
	Expect(!AlbumHasPath(lib, video, "DCIM/100APPLE/IMG_0007.HEIC"), "live not video");
	int user=-1;
	for (int i=0;i<lib->nalbums;i++) {
		if (lib->albums[i].smart==AL_USER)
			user=i;
	}
	Expect(user>=0 && lib->albums[user].name && wcscmp(lib->albums[user].name, L"Отпуск")==0, "user album");
	if (user>=0) {
		Expect(lib->albums[user].n==4, "user count");
		Expect(AlbumHasName(&lib->albums[user], L"Vacation.HEIC"), "vacation");
		Expect(AlbumHasName(&lib->albums[user], L"Same.JPG"), "same");
		Expect(AlbumHasName(&lib->albums[user], L"Same (2).JPG"), "same2");
		char resolved[256];
		char virt[128];
		WideCharToMultiByte(CP_UTF8, 0, L"Отпуск/Vacation.HEIC", -1, virt, 128, NULL, NULL);
		Expect(ResolveLib(lib, virt, resolved, 256), "resolve");
		Expect(strcmp(resolved, "DCIM/100APPLE/IMG_0001.HEIC")==0, "resolve path");
	}
	for (int i=0;i<lib->nassets;i++) {
		if (lib->assets[i].path && strcmp(lib->assets[i].path, "DCIM/100APPLE/IMG_0001.HEIC")==0) {
			SYSTEMTIME pst;
			Expect(FileTimeToSystemTime(&lib->assets[i].when, &pst), "photo ft");
			Expect(pst.wYear==2024 && pst.wMonth==1 && pst.wDay==15 && pst.wHour==12, "photo date");
			Expect(lib->assets[i].size==111, "size");
		}
	}
	FreeLib(lib);
	lib=BuildLibFromDb(dbw, 0);
	Expect(lib && lib->albums[0].name && wcscmp(lib->albums[0].name, L"All Photos")==0, "en name");
	FreeLib(lib);
	{
		PhotoLib* stress=(PhotoLib*)calloc(1, sizeof(PhotoLib));
		PhotoAlbum* all=stress ? AddShell(stress, AL_ALL, L"All Photos") : NULL;
		PhotoAlbum* user=stress ? AddShell(stress, AL_USER, L"Dup") : NULL;
		Expect(all && user, "stress shells");
		for (int i=0; all && i<4000; i++) {
			WCHAR nm[32];
			swprintf_s(nm, 32, L"A_%d.JPG", i);
			if (!AddToAlbum(all, i, nm, NULL))
				Expect(0, "stress add");
		}
		Expect(all && all->n==4000, "stress count");
		Expect(all && AlbumHasExact(all, L"A_0.JPG") && AlbumHasExact(all, L"A_3999.JPG"), "stress names");
		Expect(all && AddToAlbum(all, 4000, L"A_0.JPG", NULL), "stress same name");
		Expect(all && AlbumHasExact(all, L"A_0 (2).JPG"), "stress suffix");
		Expect(all && AddToAlbum(all, 4001, L"a_0.jpg", NULL), "stress case");
		Expect(all && AlbumHasExact(all, L"a_0 (3).jpg"), "stress case suffix");
		Expect(user && AddToAlbum(user, 1, L"Same.JPG", NULL), "user add");
		Expect(user && AddToAlbum(user, 1, L"Other.JPG", NULL), "user dup id");
		Expect(user && user->n==1, "user id once");
		Expect(user && AddToAlbum(user, 2, L"Same.JPG", NULL), "user second");
		Expect(user && AlbumHasExact(user, L"Same (2).JPG"), "user suffix");
		FreeLib(stress);
	}
	{
		PhotoLib* lib=(PhotoLib*)calloc(1, sizeof(PhotoLib));
		Expect(lib!=NULL, "tomb lib");
		if (lib)
			WcsCopy(lib->key, 80, L"photos-selftest-tomb");
		TombKeep* k=KeepFind(L"photos-selftest-tomb", 1);
		if (k) {
			WCHAR file[MAX_PATH];
			KeepPath(k, file, MAX_PATH);
			if (file[0])
				DeleteFileW(file);
			k->loaded=0;
			KeepLoad(k);
		}
		for (int i=0; lib && i<200; i++) {
			char path[64];
			_snprintf_s(path, 64, _TRUNCATE, "DCIM/100APPLE/IMG_%04d.JPG", i);
			if (i<100)
				Expect(TombLibAdd(lib, path), "tomb lib add");
			else if (k)
				KeepAddMem(k, path);
		}
		Expect(lib && lib->tombset.n==100 && !lib->tombset.failed, "tomb lib hash");
		Expect(k && k->set.n==100 && !k->set.failed, "tomb file hash");
		Expect(lib && Tomb(lib, "DCIM/100APPLE/IMG_0001.JPG"), "tomb mem hit");
		Expect(lib && Tomb(lib, "DCIM/100APPLE/IMG_0150.JPG"), "tomb file hit");
		Expect(lib && !Tomb(lib, "DCIM/100APPLE/IMG_0999.JPG"), "tomb miss");
		FreeLib(lib);
		KeepFreeAll();
	}
	DeleteFileW(dbw);
	if (g_fail) {
		printf("%d failed\n", g_fail);
		return 1;
	}
	printf("ok\n");
	return 0;
}
#endif
