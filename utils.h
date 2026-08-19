#pragma once
#include <PortableDeviceApi.h>

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
