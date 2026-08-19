//
// Helper functions to extract metadata from WMF SDK
// From Microsoft sample "wmdmapp"
// 
#include <windows.h>
#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include <stdlib.h>
#include <shellapi.h>
#include <wchar.h>
#include <wmsdk.h>
#include "fsplugin.h"

extern tLogProc LogProc;
extern int PluginNumber;

#ifndef SAFE_ARRAY_DELETE
#define SAFE_ARRAY_DELETE(x) \
   if(x != NULL)             \
   {                         \
      delete[] x;            \
      x = NULL;              \
   }
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }
#endif

class CCallback :public IWMReaderCallback
{
public:
	CCallback() {};
	~CCallback() {};
    //
    //Methods of IWMReaderCallback
    //
    HRESULT STDMETHODCALLTYPE OnSample( /* [in] */ DWORD dwOutputNum,
        /* [in] */ QWORD cnsSampleTime,
        /* [in] */ QWORD cnsSampleDuration,
        /* [in] */ DWORD dwFlags,
        /* [in] */ INSSBuffer __RPC_FAR *pSample,
		/* [in] */ void __RPC_FAR *pvContext) {return S_OK;};
    HRESULT STDMETHODCALLTYPE OnStatus( /* [in] */ WMT_STATUS Status,
        /* [in] */ HRESULT hr,
        /* [in] */ WMT_ATTR_DATATYPE dwType,
        /* [in] */ BYTE __RPC_FAR *pValue,
		/* [in] */ void __RPC_FAR *pvContext) {return S_OK;};
	ULONG STDMETHODCALLTYPE AddRef( void ) { return 1; }
    ULONG STDMETHODCALLTYPE Release( void ) { return 1; }
	HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid,
        void __RPC_FAR *__RPC_FAR *ppvObject) {
        if( riid == IID_IWMReaderCallback )
        {
            *ppvObject = ( IWMReaderCallback* )this;
        }
        else if( riid == IID_IWMStatusCallback )
        {
            *ppvObject = ( IWMStatusCallback* )this;
        }
        else
        {
            return E_NOINTERFACE;
        }
        return S_OK;
    }
};

HRESULT GetPropertyFromWMFSDK(IWMHeaderInfo *pHeaderInfo, 
					    LPCWSTR pwszName,
					    BYTE **ppBuf,
					    WORD *pBufSize,
						WMT_ATTR_DATATYPE *proptype)
{
	HRESULT hr         = S_OK ;
	WORD    nstreamNum = 0 ;
	WORD    cbLength   = 0 ;
	BYTE* pValue       = NULL;
	WMT_ATTR_DATATYPE type ;

	hr = pHeaderInfo->GetAttributeByName( &nstreamNum,
					      pwszName,
					      &type,
					      NULL,
					      &cbLength );

	if ( FAILED( hr ) && hr != ASF_E_NOTFOUND )
	{
		*pBufSize = 0;
		return hr;
	}

	if ( cbLength == 0)
	{
	    *pBufSize = 0;
		return hr;
	}

	if (proptype)
		*proptype=type;

	if(WMT_TYPE_STRING == type || WMT_TYPE_BINARY == type)
	{
		*ppBuf = (BYTE *)new BYTE[cbLength];

		if ( NULL == *ppBuf )
	   	{
			return E_OUTOFMEMORY;
		}
	} else if (cbLength>*pBufSize)   // don't copy too much data if the preallocated buffer is smaller!
		cbLength=*pBufSize;

	nstreamNum = 0;
	hr = pHeaderInfo->GetAttributeByName( &nstreamNum,
					      pwszName,
					      &type,
					      *ppBuf,
					      &cbLength );

	if (FAILED(hr) && (WMT_TYPE_STRING == type || WMT_TYPE_BINARY == type))
	{
		*pBufSize = 0;
	    delete [] *ppBuf;
	}
	else
	{
	    *pBufSize = cbLength;
	}

	return hr;

}

#define	fFalse		0

#define	CORg(hResult)\
	do\
		{\
		hr = (hResult);\
        if (FAILED(hr))\
            {\
            goto Error;\
            }\
		}\
	while (fFalse)

HRESULT CopyStringProperty(IWMHeaderInfo* pHeaderInfo,IPortableDeviceValues *pValues,
						  LPCWSTR g_wszWM,REFPROPERTYKEY g_wszWPD)
{
	WCHAR *Name;
	WORD size=0;
	Name = NULL;
	HRESULT hr2 = GetPropertyFromWMFSDK( pHeaderInfo, g_wszWM,(BYTE **)&Name, &size,NULL);
	if(SUCCEEDED(hr2) )
	{
		pValues->SetStringValue(g_wszWPD,Name);
	    SAFE_ARRAY_DELETE(Name);
	}
	return hr2;
}

VARENUM ConvertDataTypeName(WMT_ATTR_DATATYPE proptype)
{
	switch (proptype) {
		case WMT_TYPE_DWORD:return VT_UI4; break;
		case WMT_TYPE_STRING:return  VT_LPWSTR; break;
		case WMT_TYPE_BINARY:return  VT_ARRAY; break;
		case WMT_TYPE_BOOL:return VT_BOOL; break;
		case WMT_TYPE_QWORD:return VT_UI8; break;
		case WMT_TYPE_WORD:return VT_UI2; break;
		case WMT_TYPE_GUID:return VT_CLSID; break;
	}
	return VT_UI4;
}

HRESULT CopyProperty(IWMHeaderInfo* pHeaderInfo,IPortableDeviceValues* pValues,
						  LPCWSTR g_wszWM,REFPROPERTYKEY g_wszWPD,int vt_type)
{
	WCHAR *Name;
	ULARGE_INTEGER dwData;
	DOUBLE oledate;
	WCHAR defbuf[4];   //8 bytes default buffer
	WORD size=8;
	WMT_ATTR_DATATYPE proptype;
	Name = defbuf;
	HRESULT hr2 = GetPropertyFromWMFSDK( pHeaderInfo, g_wszWM,(BYTE **)&Name, &size, &proptype);
	if(SUCCEEDED(hr2) )
	{
		if ((vt_type==VT_UI4 || vt_type==VT_UI8 || vt_type==VT_BOOL) && proptype==WMT_TYPE_STRING) {
			// detect strings in the form "11/12" !
			WCHAR* p=Name;
			while (p[0]>='0' && p[0]<='9')
				p++;
			if (p[0])
				p[0]=0;
			dwData.QuadPart=_wtoi(Name);     // convert it to a DWORD!
			SAFE_ARRAY_DELETE(Name);
			Name=(WCHAR*)&dwData;
			size=4;
			proptype=WMT_TYPE_DWORD;
		} else if (g_wszWM==g_wszWMYear && proptype==WMT_TYPE_STRING) {
			SYSTEMTIME systime={0};
			WCHAR* p=Name;
			while (p[0]>='0' && p[0]<='9')
				p++;
			if (p[0])
				p[0]=0;
			systime.wYear=_wtoi(Name);     // convert it to a DWORD!
			systime.wMonth=1;
			systime.wDay=1;
			SystemTimeToVariantTime(&systime,&oledate);
		}
		PROPVARIANT pv = {0};
		PropVariantInit(&pv);
		pv.vt = vt_type;
		BOOL found=true;
		switch (vt_type) {
		case VT_UI8:pv.uhVal=*(ULARGE_INTEGER*)Name; break;
		case VT_UI4:pv.ulVal=*(ULONG*)Name; break;
		case VT_BOOL:pv.ulVal=*(ULONG*)Name; break;
		case VT_DATE:pv.date=oledate; break;
		default:
			found=false;
		}
		if (found) {
			if (g_wszWM==g_wszWMDuration)
				pv.uhVal.QuadPart/=10000;   // it's in 100 nanoseconds, but should be in milliseconds
			pValues->SetValue(g_wszWPD, &pv);
		} else
			hr2=E_FAIL;
		if (proptype==WMT_TYPE_STRING || proptype==WMT_TYPE_BINARY)
			SAFE_ARRAY_DELETE(Name);
	}
	return hr2;
}

HRESULT GetMetaDataFromWMFSDK(LPWSTR pwszFileName, 
			      IPortableDeviceValues* pValues,
				  CONST GUID* pContentGUID,
				  WM_PICTURE** ppPreviewImage)
{
	WCHAR* unknown=L"unknown";
	WORD   size = 0;
	HRESULT hr = S_OK;
	HRESULT hr2 = S_OK;
	IWMMetadataEditor* pEditor      = NULL;
	IWMHeaderInfo*     pHeaderInfo  = NULL; 

	if (ppPreviewImage)
		*ppPreviewImage=NULL;

	CORg(WMCreateEditor( &pEditor ));
	CORg(pEditor->Open( pwszFileName ) );
	CORg(pEditor->QueryInterface( IID_IWMHeaderInfo, ( void ** ) &pHeaderInfo ) );

	// Return S_OK if just the title is there!
	hr = CopyStringProperty(pHeaderInfo,pValues,g_wszWMTitle,WPD_MEDIA_TITLE);
	if (SUCCEEDED(hr)) {
		LPWSTR pTitle=NULL;
		pValues->GetStringValue(WPD_MEDIA_TITLE,&pTitle);
		if (pTitle)
			pValues->SetStringValue(WPD_OBJECT_NAME,pTitle);
	}
	hr2 = CopyStringProperty(pHeaderInfo,pValues,g_wszWMAuthor,WPD_MEDIA_ARTIST);
	if(!SUCCEEDED(hr2)) {
		pValues->SetStringValue(WPD_MEDIA_ARTIST,L"unknown");
	}

	if(!SUCCEEDED(CopyStringProperty(pHeaderInfo,pValues,g_wszWMAlbumTitle,WPD_MUSIC_ALBUM)))
	{
		WCHAR buf[16];     // use the system time for the Album title
		SYSTEMTIME systime;
		GetLocalTime(&systime);
		swprintf(buf,15,L"%04u%02u%02u",systime.wYear,systime.wMonth,systime.wDay);
		pValues->SetStringValue(WPD_MUSIC_ALBUM,buf);
	}
	
	CopyStringProperty(pHeaderInfo,pValues,g_wszWMGenre,WPD_MEDIA_GENRE);
	CopyProperty(pHeaderInfo,pValues,g_wszWMYear,WPD_MEDIA_RELEASE_DATE,VT_DATE);  // convert year to OLE date 1.1.year 00:00:00
	CopyStringProperty(pHeaderInfo,pValues,g_wszWMAlbumArtist,WPD_MEDIA_ALBUM_ARTIST);
	
	// Duration: Quad word!
	CopyProperty(pHeaderInfo,pValues,g_wszWMDuration,WPD_MEDIA_DURATION,VT_UI8);
	// Protected flag (DWORD -> VT_BOOL)
	CopyProperty(pHeaderInfo,pValues,g_wszWMProtected,WPD_OBJECT_IS_DRM_PROTECTED,VT_BOOL);
	// Bitrate (DWORD)
	CopyProperty(pHeaderInfo,pValues,g_wszWMBitrate,WPD_MEDIA_TOTAL_BITRATE,VT_UI4);
	if (*pContentGUID==WPD_CONTENT_TYPE_AUDIO) {
		CopyProperty(pHeaderInfo,pValues,g_wszWMBitrate,WPD_AUDIO_BITRATE,VT_UI4);
		// Track (DWORD)
		if (!SUCCEEDED(CopyProperty(pHeaderInfo,pValues,g_wszWMTrack,WPD_MUSIC_TRACK,VT_UI4)))
			CopyProperty(pHeaderInfo,pValues,g_wszWMTrackNumber,WPD_MUSIC_TRACK,VT_UI4);
	}
	/*{
		char buf[200];
		WCHAR Data[4];
		WORD size=0;
		WCHAR *pData = Data;
		HRESULT hr2 = GetPropertyFromWMFSDK( pHeaderInfo, g_wszWMTrack, (BYTE **)&pData, &size,NULL);
		if(SUCCEEDED(hr2)) {
			wsprintf(buf,"WMTrack: %S",pData);
			LogProc(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,buf);
		}
		hr2 = GetPropertyFromWMFSDK( pHeaderInfo, g_wszWMTrackNumber, (BYTE **)&pData, &size,NULL);
		if(SUCCEEDED(hr2)) {
			wsprintf(buf,"WMTrackNumber: %S",pData);
			LogProc(PluginNumber,MSGTYPE_OPERATIONCOMPLETE,buf);
		}
	}*/

	// Rating (type DWORD)
	CopyProperty(pHeaderInfo,pValues,g_wszWMRating,WPD_MEDIA_USER_EFFECTIVE_RATING,VT_UI4);
	// Video width (type DWORD)
	CopyProperty(pHeaderInfo,pValues,g_wszWMVideoWidth,WPD_MEDIA_WIDTH,VT_UI4);
	// Video height (type DWORD )
	CopyProperty(pHeaderInfo,pValues,g_wszWMVideoHeight,WPD_MEDIA_HEIGHT,VT_UI4);
	// Codec (type DWORD)
	CopyProperty(pHeaderInfo,pValues,g_wszWMCodec,WPD_AUDIO_FORMAT_CODE,VT_UI4);
	CopyProperty(pHeaderInfo,pValues,g_wszWMCodec,WPD_VIDEO_FOURCC_CODE,VT_UI4);

	// Composer (type string)
	CopyStringProperty(pHeaderInfo,pValues,g_wszWMComposer,WPD_MEDIA_COMPOSER);
	// Copyright (type string)
	CopyStringProperty(pHeaderInfo,pValues,g_wszWMCopyright,WPD_MEDIA_COPYRIGHT);
	// Description (type string)
	CopyStringProperty(pHeaderInfo,pValues,g_wszWMDescription,WPD_MEDIA_DESCRIPTION);
	// Parental rating (type STRING)
	CopyProperty(pHeaderInfo,pValues,g_wszWMParentalRating,WPD_MEDIA_PARENTAL_RATING,VT_UI4);
	// Radio station name (type STRING)
	//CopyStringProperty(pHeaderInfo,pValues,g_wszWMRadioStationName,g_wszWMDMMediaStationName);
	// TV episode title (type STRING)
	CopyStringProperty(pHeaderInfo,pValues,g_wszWMSubTitle,WPD_MEDIA_SUB_TITLE);
	// Track mood (type STRING)
	CopyStringProperty(pHeaderInfo,pValues,g_wszWMMood,WPD_MUSIC_MOOD);
	// Cover (type BINARY)
	//g_wszWMPicture ID3v2.2: PIC, ID3v2.3/v2.4: APIC
	//See: http://msdn2.microsoft.com/en-us/library/aa386866(VS.85).aspx
	//CopyProperty(pHeaderInfo,pValues,g_wszWMPicture,g_wszWMDMAlbumCoverData);

	if (ppPreviewImage && *pContentGUID==WPD_CONTENT_TYPE_AUDIO) {
		WMT_ATTR_DATATYPE proptype;
		WORD iPreviewImageLen;
		HRESULT hr2 = GetPropertyFromWMFSDK( pHeaderInfo, g_wszWMPicture,(BYTE **)ppPreviewImage, &iPreviewImageLen, &proptype);

		if (!SUCCEEDED(hr2))
			*ppPreviewImage=NULL;
	}
	pEditor->Close();

Error:
	SAFE_RELEASE( pHeaderInfo ) ;
	SAFE_RELEASE( pEditor ) ;

	return hr;
}


