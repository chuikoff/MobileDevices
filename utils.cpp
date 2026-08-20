#include <windows.h>
#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shlobj.h>
#include "utils.h"
#include "cunicode.h"

#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "shlwapi.lib")

void FreeAlbumArt(AlbumArtBlob* p)
{
	if (!p)
		return;
	if (p->data)
		free(p->data);
	free(p);
}

static HRESULT ReadStreamToArt(IStream* stream, AlbumArtBlob** ppArt)
{
	if (!stream || !ppArt)
		return E_POINTER;
	*ppArt=NULL;
	const size_t kMaxArt=20u*1024u*1024u;
	STATSTG st={0};
	HRESULT hr=stream->Stat(&st, STATFLAG_NONAME);
	size_t cap=0;
	if (SUCCEEDED(hr) && st.cbSize.HighPart==0 && st.cbSize.LowPart>0 && st.cbSize.LowPart<=kMaxArt)
		cap=st.cbSize.LowPart;
	BYTE* data=NULL;
	size_t total=0;
	LARGE_INTEGER z={0};
	stream->Seek(z, STREAM_SEEK_SET, NULL);
	if (cap) {
		data=(BYTE*)malloc(cap);
		if (!data)
			return E_OUTOFMEMORY;
		while (total<cap) {
			ULONG n=0;
			ULONG want=(ULONG)(cap-total);
			hr=stream->Read(data+total, want, &n);
			if (FAILED(hr) || n==0)
				break;
			if (n>want)
				n=want;
			total+=n;
		}
	} else {
		BYTE buf[8192];
		for (;;) {
			ULONG n=0;
			hr=stream->Read(buf, sizeof(buf), &n);
			if (FAILED(hr) || n==0)
				break;
			if ((size_t)n>kMaxArt || total>kMaxArt-(size_t)n) {
				free(data);
				return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
			}
			BYTE* nd=(BYTE*)realloc(data, total+(size_t)n);
			if (!nd) {
				free(data);
				return E_OUTOFMEMORY;
			}
			data=nd;
			memcpy(data+total, buf, n);
			total+=(size_t)n;
		}
	}
	if (!data || total<8) {
		free(data);
		return E_FAIL;
	}
	AlbumArtBlob* art=(AlbumArtBlob*)calloc(1, sizeof(AlbumArtBlob));
	if (!art) {
		free(data);
		return E_OUTOFMEMORY;
	}
	art->data=data;
	art->len=(DWORD)total;
	if (data[0]==0xFF && data[1]==0xD8)
		wcscpy_s(art->mime, L"image/jpeg");
	else if (data[0]==0x89 && data[1]=='P')
		wcscpy_s(art->mime, L"image/png");
	else if (data[0]=='G' && data[1]=='I')
		wcscpy_s(art->mime, L"image/gif");
	else if (data[0]=='B' && data[1]=='M')
		wcscpy_s(art->mime, L"image/bmp");
	else
		wcscpy_s(art->mime, L"image/jpeg");
	*ppArt=art;
	return S_OK;
}

static BOOL SetStringIfPresent(IPropertyStore* store, REFPROPERTYKEY src, IPortableDeviceValues* dst, REFPROPERTYKEY wpd)
{
	PROPVARIANT pv;
	PropVariantInit(&pv);
	BOOL ok=FALSE;
	if (SUCCEEDED(store->GetValue(src, &pv))) {
		if (pv.vt==VT_LPWSTR && pv.pwszVal && pv.pwszVal[0]) {
			dst->SetStringValue(wpd, pv.pwszVal);
			ok=TRUE;
		} else if (pv.vt==VT_BSTR && pv.bstrVal && pv.bstrVal[0]) {
			dst->SetStringValue(wpd, pv.bstrVal);
			ok=TRUE;
		} else {
			WCHAR buf[256];
			if (SUCCEEDED(PropVariantToString(pv, buf, ARRAYSIZE(buf))) && buf[0]) {
				dst->SetStringValue(wpd, buf);
				ok=TRUE;
			}
		}
	}
	PropVariantClear(&pv);
	return ok;
}

HRESULT GetFileMetadata(LPWSTR pwszFileName,
	IPortableDeviceValues* pValues,
	CONST GUID* pContentGUID,
	AlbumArtBlob** ppAlbumArt)
{
	if (ppAlbumArt)
		*ppAlbumArt=NULL;
	if (!pwszFileName || !pValues)
		return E_POINTER;

	IPropertyStore* store=NULL;
	HRESULT hr=SHGetPropertyStoreFromParsingName(pwszFileName, NULL,
		GPS_BESTEFFORT, IID_IPropertyStore, (void**)&store);
	if (FAILED(hr))
		return hr;

	BOOL any=FALSE;
	if (SetStringIfPresent(store, PKEY_Title, pValues, WPD_MEDIA_TITLE)) {
		LPWSTR title=NULL;
		if (SUCCEEDED(pValues->GetStringValue(WPD_MEDIA_TITLE, &title)) && title) {
			pValues->SetStringValue(WPD_OBJECT_NAME, title);
			CoTaskMemFree(title);
			any=TRUE;
		}
	}
	SetStringIfPresent(store, PKEY_Music_Artist, pValues, WPD_MEDIA_ARTIST);
	SetStringIfPresent(store, PKEY_Music_AlbumArtist, pValues, WPD_MEDIA_ALBUM_ARTIST);
	SetStringIfPresent(store, PKEY_Music_AlbumTitle, pValues, WPD_MUSIC_ALBUM);
	SetStringIfPresent(store, PKEY_Music_Genre, pValues, WPD_MEDIA_GENRE);
	SetStringIfPresent(store, PKEY_Music_Composer, pValues, WPD_MEDIA_COMPOSER);
	SetStringIfPresent(store, PKEY_Copyright, pValues, WPD_MEDIA_COPYRIGHT);

	PROPVARIANT pv;
	PropVariantInit(&pv);
	if (SUCCEEDED(store->GetValue(PKEY_Media_Duration, &pv))) {
		ULONGLONG hns=0;
		if (SUCCEEDED(PropVariantToUInt64(pv, &hns)) && hns>0) {
			PROPVARIANT d;
			PropVariantInit(&d);
			d.vt=VT_UI8;
			d.uhVal.QuadPart=hns/10000; // 100ns -> ms
			pValues->SetValue(WPD_MEDIA_DURATION, &d);
			any=TRUE;
		}
	}
	PropVariantClear(&pv);

	PropVariantInit(&pv);
	if (SUCCEEDED(store->GetValue(PKEY_Audio_EncodingBitrate, &pv)) ||
		SUCCEEDED(store->GetValue(PKEY_Video_EncodingBitrate, &pv))) {
		ULONG br=0;
		if (SUCCEEDED(PropVariantToUInt32(pv, &br)) && br>0) {
			pValues->SetUnsignedIntegerValue(WPD_MEDIA_TOTAL_BITRATE, br);
			if (pContentGUID && IsEqualGUID(*pContentGUID, WPD_CONTENT_TYPE_AUDIO))
				pValues->SetUnsignedIntegerValue(WPD_AUDIO_BITRATE, br);
			any=TRUE;
		}
	}
	PropVariantClear(&pv);

	PropVariantInit(&pv);
	if (SUCCEEDED(store->GetValue(PKEY_Music_TrackNumber, &pv))) {
		ULONG tr=0;
		if (SUCCEEDED(PropVariantToUInt32(pv, &tr)))
			pValues->SetUnsignedIntegerValue(WPD_MUSIC_TRACK, tr);
	}
	PropVariantClear(&pv);

	PropVariantInit(&pv);
	if (SUCCEEDED(store->GetValue(PKEY_Image_HorizontalSize, &pv))) {
		ULONG w=0;
		if (SUCCEEDED(PropVariantToUInt32(pv, &w)) && w)
			pValues->SetUnsignedIntegerValue(WPD_MEDIA_WIDTH, w);
	}
	PropVariantClear(&pv);
	PropVariantInit(&pv);
	if (SUCCEEDED(store->GetValue(PKEY_Image_VerticalSize, &pv))) {
		ULONG h=0;
		if (SUCCEEDED(PropVariantToUInt32(pv, &h)) && h)
			pValues->SetUnsignedIntegerValue(WPD_MEDIA_HEIGHT, h);
	}
	PropVariantClear(&pv);

	PropVariantInit(&pv);
	if (SUCCEEDED(store->GetValue(PKEY_Photo_DateTaken, &pv)) && pv.vt==VT_FILETIME) {
		SYSTEMTIME st;
		if (FileTimeToSystemTime(&pv.filetime, &st)) {
			PROPVARIANT date;
			PropVariantInit(&date);
			date.vt=VT_DATE;
			if (SystemTimeToVariantTime(&st, &date.date))
				pValues->SetValue(WPD_OBJECT_DATE_AUTHORED, &date);
		}
	}
	PropVariantClear(&pv);

	if (ppAlbumArt) {
		PropVariantInit(&pv);
		if (SUCCEEDED(store->GetValue(PKEY_ThumbnailStream, &pv))) {
			IStream* stm=NULL;
			if (pv.vt==VT_STREAM && pv.pStream)
				stm=pv.pStream;
			if (stm)
				ReadStreamToArt(stm, ppAlbumArt);
		}
		PropVariantClear(&pv);
	}

	SAFE_RELEASE(store);
	return any ? S_OK : S_FALSE;
}
