#pragma once
#include <PortableDeviceApi.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p)=NULL; } } while (0)
#endif

typedef struct {
	WCHAR mime[40];
	BYTE* data;
	DWORD len;
} AlbumArtBlob;

HRESULT GetFileMetadata(LPWSTR pwszFileName,
	IPortableDeviceValues* pValues,
	CONST GUID* pContentGUID,
	AlbumArtBlob** ppAlbumArt);

void FreeAlbumArt(AlbumArtBlob* p);
