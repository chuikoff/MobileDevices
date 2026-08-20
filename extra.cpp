#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include <GdiPlus.h>
#include "wpdplug.h"
#include "cunicode.h"
#include "fsplugin.h"
#include "wpdplug_int.h"
#include "resource.h"
#include "apple_md.h"

enum {
	FIELD_TYPE=0,
	FIELD_TITLE,
	FIELD_ARTIST,
	FIELD_ALBUM,
	FIELD_DURATION,
	FIELD_BITRATE,
	FIELD_WIDTH,
	FIELD_HEIGHT,
	FIELD_FREE,
	FIELD_CAPACITY,
	FIELD_SERIAL,
	FIELD_BATTERY,
	FIELD_COUNT
};

static const char* kFieldNames[FIELD_COUNT]={
	"Type","Title","Artist","Album","Duration","Bitrate","Width","Height",
	"Free space","Capacity","Serial","Battery"
};

int __stdcall FsGetBackgroundFlags(void)
{
	return BG_DOWNLOAD | BG_UPLOAD;
}

void __stdcall FsStatusInfo(char* RemoteDir,int InfoStartEnd,int InfoOperation)
{
	WCHAR dirW[wdirtypemax];
	FsStatusInfoW(awfilenamecopy(dirW,RemoteDir),InfoStartEnd,InfoOperation);
}

void __stdcall FsStatusInfoW(WCHAR* RemoteDir,int InfoStartEnd,int InfoOperation)
{
	UNREFERENCED_PARAMETER(RemoteDir);
	EnsureComApartment();
	if (InfoStartEnd==FS_STATUS_START) {
		ResetAbort();
		SetContentStop(FALSE);
	}
	LockPlugin();
	SetCancelDevice(NULL);
	if (InfoStartEnd==FS_STATUS_END && InfoOperation==FS_STATUS_OP_LIST)
		EnsureWpdEventsAdvised();
	UnlockPlugin();
}

static LPCWSTR ContentTypeName(REFGUID g)
{
	if (IsEqualGUID(g,WPD_CONTENT_TYPE_FOLDER) ||
		IsEqualGUID(g,WPD_CONTENT_TYPE_AUDIO_ALBUM) ||
		IsEqualGUID(g,WPD_CONTENT_TYPE_IMAGE_ALBUM) ||
		IsEqualGUID(g,WPD_CONTENT_TYPE_VIDEO_ALBUM) ||
		IsEqualGUID(g,WPD_CONTENT_TYPE_MIXED_CONTENT_ALBUM))
		return L"Folder";
	if (IsEqualGUID(g,WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT))
		return L"Storage";
	if (IsEqualGUID(g,WPD_CONTENT_TYPE_AUDIO))
		return L"Audio";
	if (IsEqualGUID(g,WPD_CONTENT_TYPE_VIDEO))
		return L"Video";
	if (IsEqualGUID(g,WPD_CONTENT_TYPE_IMAGE))
		return L"Image";
	if (IsEqualGUID(g,WPD_CONTENT_TYPE_DOCUMENT))
		return L"Document";
	if (IsEqualGUID(g,WPD_CONTENT_TYPE_PLAYLIST))
		return L"Playlist";
	return L"File";
}

static HRESULT GetValuesForPath(WCHAR* path, IPortableDeviceValues** ppValues)
{
	*ppValues=NULL;
	if (!path || path[0]!='\\')
		return E_INVALIDARG;
	EnsureComApartment();
	if (!InitFunctionsIfNeeded(TRUE))
		return E_FAIL;
	LockPlugin();
	IPortableDeviceProperties* props=NULL;
	IPortableDeviceContent* content=NULL;
	LPWSTR id=NULL;
	WCHAR pathCopy[wdirtypemax];
	wcslcpy(pathCopy,path,wdirtypemax-1);
	HRESULT hr=GetFolderIDFromPathName(pathCopy,NULL,&props,&content,&id);
	if (FAILED(hr) || !props || !id) {
		UnlockPlugin();
		if (props) props->Release();
		if (content) content->Release();
		if (id) CoTaskMemFree(id);
		return FAILED(hr)?hr:E_FAIL;
	}
	IPortableDeviceKeyCollection* keys=NULL;
	hr=CoCreateInstance(CLSID_PortableDeviceKeyCollection,NULL,CLSCTX_INPROC_SERVER,
		IID_IPortableDeviceKeyCollection,(VOID**)&keys);
	if (FAILED(hr) || !keys)
		goto done;
	keys->Add(WPD_OBJECT_CONTENT_TYPE);
	keys->Add(WPD_MEDIA_TITLE);
	keys->Add(WPD_MEDIA_ARTIST);
	keys->Add(WPD_MUSIC_ALBUM);
	keys->Add(WPD_MEDIA_DURATION);
	keys->Add(WPD_MEDIA_TOTAL_BITRATE);
	keys->Add(WPD_AUDIO_BITRATE);
	keys->Add(WPD_MEDIA_WIDTH);
	keys->Add(WPD_MEDIA_HEIGHT);
	keys->Add(WPD_STORAGE_FREE_SPACE_IN_BYTES);
	keys->Add(WPD_STORAGE_CAPACITY);
	keys->Add(WPD_DEVICE_SERIAL_NUMBER);
	keys->Add(WPD_DEVICE_POWER_LEVEL);
	hr=props->GetValues(id,keys,ppValues);
	if (FAILED(hr) && ppValues && *ppValues) {
		(*ppValues)->Release();
		*ppValues=NULL;
	}
done:
	if (keys) keys->Release();
	if (props) props->Release();
	if (content) content->Release();
	if (id) CoTaskMemFree(id);
	UnlockPlugin();
	return hr;
}

int __stdcall FsContentGetSupportedField(int FieldIndex,char* FieldName,char* Units,int maxlen)
{
	if (FieldIndex<0 || FieldIndex>=FIELD_COUNT)
		return ft_nomorefields;
	if (FieldName && maxlen>0)
		lstrcpynA(FieldName,kFieldNames[FieldIndex],maxlen);
	if (Units && maxlen>0)
		Units[0]=0;
	switch (FieldIndex) {
	case FIELD_DURATION:
		return ft_stringw;
	case FIELD_BITRATE:
	case FIELD_WIDTH:
	case FIELD_HEIGHT:
	case FIELD_BATTERY:
		if (Units && maxlen>0) {
			if (FieldIndex==FIELD_BITRATE)
				lstrcpynA(Units,"kbit/s",maxlen);
			else if (FieldIndex==FIELD_BATTERY)
				lstrcpynA(Units,"%",maxlen);
		}
		return ft_numeric_32;
	case FIELD_FREE:
	case FIELD_CAPACITY:
		if (Units && maxlen>0)
			lstrcpynA(Units,"bytes|kbytes|Mbytes|Gbytes",maxlen);
		return ft_numeric_64;
	default:
		return ft_stringw;
	}
}

int __stdcall FsContentGetValue(char* FileName,int FieldIndex,int UnitIndex,void* FieldValue,int maxlen,int flags)
{
	WCHAR nameW[wdirtypemax];
	int r=FsContentGetValueW(awfilenamecopy(nameW,FileName),FieldIndex,UnitIndex,FieldValue,maxlen,flags);
	if (r==ft_stringw) {
		WCHAR tmp[1024];
		wcslcpy(tmp,(WCHAR*)FieldValue,1024);
		walcopy((char*)FieldValue,tmp,maxlen>0?maxlen-1:0);
		return ft_string;
	}
	return r;
}

int __stdcall FsContentGetValueW(WCHAR* FileName,int FieldIndex,int UnitIndex,void* FieldValue,int maxlen,int flags)
{
	UNREFERENCED_PARAMETER(UnitIndex);
	if (FieldIndex<0 || FieldIndex>=FIELD_COUNT)
		return ft_nosuchfield;
	if (flags & CONTENT_DELAYIFSLOW)
		return ft_delayed;
	if (IsContentStop())
		return ft_fieldempty;

	if (FieldIndex==FIELD_FREE || FieldIndex==FIELD_CAPACITY || FieldIndex==FIELD_BATTERY) {
		WCHAR dev[MAX_PATH];
		const WCHAR* rp=FileName[0]=='\\' ? FileName+1 : FileName;
		wcslcpy(dev, rp, MAX_PATH);
		WCHAR* sl=wcschr(dev, '\\');
		if (sl) sl[0]=0;
		if (AppleMdIsDeviceName(dev)) {
			PluginDeviceInfo inf;
			if (!AppleMdFillInfo(dev, &inf))
				return ft_fieldempty;
			if (FieldIndex==FIELD_BATTERY) {
				if (inf.battery<0)
					return ft_fieldempty;
				*(int*)FieldValue=inf.battery;
				return ft_numeric_32;
			}
			if (inf.nstor<=0)
				return ft_fieldempty;
			ULONGLONG v=(FieldIndex==FIELD_FREE) ? inf.stor[0].freeBytes : inf.stor[0].capacityBytes;
			*(INT64*)FieldValue=(INT64)v;
			return ft_numeric_64;
		}
	}

	IPortableDeviceValues* values=NULL;
	HRESULT hr=GetValuesForPath(FileName,&values);
	if (FAILED(hr) || !values)
		return ft_fileerror;

	int result=ft_fieldempty;
	switch (FieldIndex) {
	case FIELD_TYPE: {
		GUID g=GUID_NULL;
		if (SUCCEEDED(values->GetGuidValue(WPD_OBJECT_CONTENT_TYPE,&g))) {
			wcslcpy((WCHAR*)FieldValue,ContentTypeName(g),maxlen>0?maxlen:MAX_PATH);
			result=ft_stringw;
		}
		break;
	}
	case FIELD_TITLE:
	case FIELD_ARTIST:
	case FIELD_ALBUM:
	case FIELD_SERIAL: {
		REFPROPERTYKEY key=(FieldIndex==FIELD_TITLE)?WPD_MEDIA_TITLE:
			(FieldIndex==FIELD_ARTIST)?WPD_MEDIA_ARTIST:
			(FieldIndex==FIELD_ALBUM)?WPD_MUSIC_ALBUM:WPD_DEVICE_SERIAL_NUMBER;
		LPWSTR s=NULL;
		if (SUCCEEDED(values->GetStringValue(key,&s)) && s && s[0]) {
			wcslcpy((WCHAR*)FieldValue,s,maxlen>0?maxlen:MAX_PATH);
			result=ft_stringw;
		}
		if (s) CoTaskMemFree(s);
		break;
	}
	case FIELD_DURATION: {
		ULONGLONG ms=0;
		if (SUCCEEDED(values->GetUnsignedLargeIntegerValue(WPD_MEDIA_DURATION,&ms)) && ms>0) {
			unsigned sec=(unsigned)(ms/1000);
			unsigned h=sec/3600;
			unsigned m=(sec%3600)/60;
			unsigned s=sec%60;
			WCHAR buf[32];
			if (h)
				swprintf_s(buf,L"%u:%02u:%02u",h,m,s);
			else
				swprintf_s(buf,L"%u:%02u",m,s);
			wcslcpy((WCHAR*)FieldValue,buf,maxlen>0?maxlen:MAX_PATH);
			result=ft_stringw;
		}
		break;
	}
	case FIELD_BITRATE: {
		ULONG br=0;
		if (FAILED(values->GetUnsignedIntegerValue(WPD_MEDIA_TOTAL_BITRATE,&br)))
			values->GetUnsignedIntegerValue(WPD_AUDIO_BITRATE,&br);
		if (br>0) {
			*(int*)FieldValue=(int)(br/1000);
			result=ft_numeric_32;
		}
		break;
	}
	case FIELD_WIDTH:
	case FIELD_HEIGHT: {
		ULONG v=0;
		REFPROPERTYKEY key=(FieldIndex==FIELD_WIDTH)?WPD_MEDIA_WIDTH:WPD_MEDIA_HEIGHT;
		if (SUCCEEDED(values->GetUnsignedIntegerValue(key,&v)) && v>0) {
			*(int*)FieldValue=(int)v;
			result=ft_numeric_32;
		}
		break;
	}
	case FIELD_FREE:
	case FIELD_CAPACITY: {
		ULONGLONG v=0;
		REFPROPERTYKEY key=(FieldIndex==FIELD_FREE)?WPD_STORAGE_FREE_SPACE_IN_BYTES:WPD_STORAGE_CAPACITY;
		if (SUCCEEDED(values->GetUnsignedLargeIntegerValue(key,&v))) {
			*(INT64*)FieldValue=(INT64)v;
			result=ft_numeric_64;
		}
		break;
	}
	case FIELD_BATTERY: {
		ULONG v=0;
		if (SUCCEEDED(values->GetUnsignedIntegerValue(WPD_DEVICE_POWER_LEVEL,&v))) {
			*(int*)FieldValue=(int)v;
			result=ft_numeric_32;
		}
		break;
	}
	}
	values->Release();
	return result;
}

void __stdcall FsContentStopGetValue(char* FileName)
{
	UNREFERENCED_PARAMETER(FileName);
	SetContentStop(TRUE);
}

void __stdcall FsContentStopGetValueW(WCHAR* FileName)
{
	UNREFERENCED_PARAMETER(FileName);
	SetContentStop(TRUE);
}

BOOL __stdcall FsContentGetDefaultView(char* ViewContents,char* ViewHeaders,char* ViewWidths,char* ViewOptions,int maxlen)
{
	UNREFERENCED_PARAMETER(ViewContents);
	UNREFERENCED_PARAMETER(ViewHeaders);
	UNREFERENCED_PARAMETER(ViewWidths);
	UNREFERENCED_PARAMETER(ViewOptions);
	UNREFERENCED_PARAMETER(maxlen);
	return FALSE;
}

BOOL __stdcall FsContentGetDefaultViewW(WCHAR* ViewContents,WCHAR* ViewHeaders,WCHAR* ViewWidths,WCHAR* ViewOptions,int maxlen)
{
	UNREFERENCED_PARAMETER(ViewContents);
	UNREFERENCED_PARAMETER(ViewHeaders);
	UNREFERENCED_PARAMETER(ViewWidths);
	UNREFERENCED_PARAMETER(ViewOptions);
	UNREFERENCED_PARAMETER(maxlen);
	return FALSE;
}

static HRESULT CopyToMemoryStream(IStream* src, IStream** dst)
{
	*dst=NULL;
	HGLOBAL hg=GlobalAlloc(GMEM_MOVEABLE,1);
	if (!hg)
		return E_OUTOFMEMORY;
	HRESULT hr=CreateStreamOnHGlobal(hg,TRUE,dst);
	if (FAILED(hr)) {
		GlobalFree(hg);
		return hr;
	}
	BYTE buf[8192];
	ULONG nread=0,nwritten=0;
	for (;;) {
		hr=src->Read(buf,sizeof(buf),&nread);
		if (FAILED(hr) || nread==0)
			break;
		hr=(*dst)->Write(buf,nread,&nwritten);
		if (FAILED(hr))
			break;
	}
	LARGE_INTEGER zero={0};
	(*dst)->Seek(zero,STREAM_SEEK_SET,NULL);
	return S_OK;
}

static HBITMAP ScaleBitmapKeepAspect(Gdiplus::Bitmap* src, int maxW, int maxH)
{
	if (!src)
		return NULL;
	UINT srcW=src->GetWidth();
	UINT srcH=src->GetHeight();
	if (srcW==0 || srcH==0)
		return NULL;
	int w,h;
	if ((int)srcW>=maxW || (int)srcH>=maxH) {
		int stretchy=MulDiv(maxW,(int)srcH,(int)srcW);
		if (stretchy<=maxH) {
			w=maxW;
			h=stretchy;
			if (h<1) h=1;
		} else {
			w=MulDiv(maxH,(int)srcW,(int)srcH);
			if (w<1) w=1;
			h=maxH;
		}
	} else {
		w=(int)srcW;
		h=(int)srcH;
	}
	Gdiplus::Bitmap thumb(w,h,PixelFormat32bppRGB);
	Gdiplus::Graphics g(&thumb);
	g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	g.Clear(Gdiplus::Color(255,255,255,255));
	g.DrawImage(src,0,0,w,h);
	HBITMAP hb=NULL;
	thumb.GetHBITMAP(Gdiplus::Color(255,255,255,255),&hb);
	return hb;
}

int __stdcall FsGetPreviewBitmap(char* RemoteName,int width,int height,HBITMAP* ReturnedBitmap)
{
	WCHAR nameW[wdirtypemax];
	return FsGetPreviewBitmapW(awfilenamecopy(nameW,RemoteName),width,height,ReturnedBitmap);
}

int __stdcall FsGetPreviewBitmapW(WCHAR* RemoteName,int width,int height,HBITMAP* ReturnedBitmap)
{
	if (!ReturnedBitmap || !RemoteName || RemoteName[0]!='\\')
		return FS_BITMAP_NONE;
	*ReturnedBitmap=NULL;
	EnsureComApartment();
	if (!InitFunctionsIfNeeded(TRUE))
		return FS_BITMAP_NONE;

	LockPlugin();
	IPortableDeviceProperties* props=NULL;
	IPortableDeviceContent* content=NULL;
	LPWSTR id=NULL;
	WCHAR pathCopy[wdirtypemax];
	wcslcpy(pathCopy,RemoteName,wdirtypemax-1);
	HRESULT hr=GetFolderIDFromPathName(pathCopy,NULL,&props,&content,&id);
	if (props) { props->Release(); props=NULL; }
	IStream* pStream=NULL;
	IStream* memStream=NULL;
	IPortableDeviceResources* resources=NULL;
	if (SUCCEEDED(hr) && content && id) {
		hr=content->Transfer(&resources);
		DWORD bufSize=65536;
		if (SUCCEEDED(hr)) {
			hr=resources->GetStream(id,WPD_RESOURCE_THUMBNAIL,STGM_READ,&bufSize,&pStream);
			if (FAILED(hr))
				hr=resources->GetStream(id,WPD_RESOURCE_ALBUM_ART,STGM_READ,&bufSize,&pStream);
			if (FAILED(hr))
				hr=resources->GetStream(id,WPD_RESOURCE_ICON,STGM_READ,&bufSize,&pStream);
		}
		if (SUCCEEDED(hr) && pStream)
			CopyToMemoryStream(pStream,&memStream);
	}
	if (pStream) pStream->Release();
	if (resources) resources->Release();
	if (content) content->Release();
	if (id) CoTaskMemFree(id);
	UnlockPlugin();

	if (!memStream)
		return FS_BITMAP_NONE;

	if (!GdiPlusInitialize()) {
		memStream->Release();
		return FS_BITMAP_NONE;
	}
	Gdiplus::Bitmap* bmp=Gdiplus::Bitmap::FromStream(memStream);
	memStream->Release();
	if (!bmp || bmp->GetLastStatus()!=Gdiplus::Ok) {
		delete bmp;
		return FS_BITMAP_NONE;
	}
	HBITMAP hb=ScaleBitmapKeepAspect(bmp,width,height);
	delete bmp;
	if (!hb)
		return FS_BITMAP_NONE;
	*ReturnedBitmap=hb;
	return FS_BITMAP_EXTRACTED | FS_BITMAP_CACHE;
}

int __stdcall FsExtractCustomIcon(char* RemoteName,int ExtractFlags,HICON* TheIcon)
{
	WCHAR nameW[wdirtypemax];
	return FsExtractCustomIconW(awfilenamecopy(nameW,RemoteName),ExtractFlags,TheIcon);
}

static int PluginIconPx(int ExtractFlags)
{
	if (ExtractFlags & FS_ICONFLAG_SMALL) {
		int w=GetSystemMetrics(SM_CXSMICON);
		return w>0 ? w : 16;
	}
	/* Thumbnail / large view: give TC a 256px image so it is not a stretched 32px icon. */
	return 256;
}

static int LoadPluginIcon(int id, int ExtractFlags, HICON* TheIcon)
{
	int wh=PluginIconPx(ExtractFlags);
	*TheIcon=(HICON)LoadImage(hInst,MAKEINTRESOURCE(id),IMAGE_ICON,wh,wh,LR_DEFAULTCOLOR);
	if (!*TheIcon)
		return FS_ICON_USEDEFAULT;
	return FS_ICON_EXTRACTED_DESTROY;
}

int __stdcall FsExtractCustomIconW(WCHAR* RemoteName,int ExtractFlags,HICON* TheIcon)
{
	if (!TheIcon || !RemoteName)
		return FS_ICON_USEDEFAULT;
	*TheIcon=NULL;

	BOOL isRoot=(RemoteName[0]==0) || (RemoteName[0]=='\\' && RemoteName[1]==0);
	if (isRoot)
		return LoadPluginIcon(IDI_ICON1, ExtractFlags, TheIcon);

	WCHAR dev[MAX_PATH];
	const WCHAR* p=RemoteName[0]=='\\' ? RemoteName+1 : RemoteName;
	wcslcpy(dev, p, MAX_PATH);
	WCHAR* sl=wcschr(dev, '\\');
	if (sl) {
		if (sl[1]!=0)
			return FS_ICON_USEDEFAULT;
		sl[0]=0;
	}
	if (!dev[0])
		return LoadPluginIcon(IDI_ICON1, ExtractFlags, TheIcon);

	int id=AppleMdIsDeviceName(dev) ? IDI_IPHONE : IDI_ANDROID;
	return LoadPluginIcon(id, ExtractFlags, TheIcon);
}
