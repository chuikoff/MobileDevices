#pragma once
#include <windows.h>

/* Same pair as apple_md.cpp next to ADNCI_MSG_*. Included here so the
   photos selftest does not pull in the rest of the plugin. */
#ifndef FT_NO_TIME_HIGH
#define FT_NO_TIME_HIGH 0xFFFFFFFF
#define FT_NO_TIME_LOW  0xFFFFFFFE
#endif

/* AFC callbacks supplied by apple_md.cpp. stat: 0 missing, 1 file, 2 directory, -1 error.
   pull: 0 success. list: malloc'd UTF-8 names, -1 on failure. */
struct AppleAfcIo {
	int (*pull)(void* conn, const char* remote, const wchar_t* local);
	int (*stat)(void* conn, const char* path, unsigned long long* size,
		unsigned long long* mtimeRaw, unsigned long long* birthRaw);
	int (*list)(void* conn, const char* path, char*** names);
	void (*log)(const char* line);
};

void ApplePhotosSetIo(const AppleAfcIo* io);
BOOL ApplePhotosEnsure(void* conn, const wchar_t* udid, int russian);
BOOL ApplePhotosList(void* conn, const wchar_t* udid, const char* virtUtf8, int russian,
	WIN32_FIND_DATAW** ents, int* nent);
BOOL ApplePhotosResolve(const wchar_t* udid, const char* virtUtf8, char* afcPath, int cch);
/* JPEG from PhotoData/Thumbnails/V2, cached under %TEMP%. FALSE if this photo has none. */
BOOL ApplePhotosLoadThumb(void* conn, const wchar_t* udid, const char* virtUtf8,
	wchar_t* localJpeg, int cch);
void ApplePhotosTombstone(const wchar_t* udid, const char* afcPath);
void ApplePhotosShutdown(void);

/* iOS AFC st_mtime / st_birthtime is nanoseconds. Older builds used seconds or ms. */
BOOL AppleFileTimeFromAfcRaw(unsigned long long raw, FILETIME* ft);
