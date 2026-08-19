#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <shlwapi.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include "wpdplug.h"
#include "cunicode.h"
#include "fsplugin.h"
#include "wpdplug_int.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

static LONG g_evRef=1;

class CWpdEvents : public IPortableDeviceEventCallback
{
public:
	STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
	{
		if (!ppv) return E_POINTER;
		if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IPortableDeviceEventCallback)) {
			*ppv=static_cast<IPortableDeviceEventCallback*>(this);
			AddRef();
			return S_OK;
		}
		*ppv=NULL;
		return E_NOINTERFACE;
	}
	STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&g_evRef); }
	STDMETHODIMP_(ULONG) Release() { return InterlockedDecrement(&g_evRef); }
	STDMETHODIMP OnEvent(IPortableDeviceValues* pEvent)
	{
		if (!pEvent)
			return S_OK;
		GUID id=GUID_NULL;
		if (FAILED(pEvent->GetGuidValue(WPD_EVENT_PARAMETER_EVENT_ID, &id)))
			return S_OK;
		if (IsEqualGUID(id, WPD_EVENT_DEVICE_REMOVED) ||
			IsEqualGUID(id, WPD_EVENT_DEVICE_RESET))
			DeviceEventReceived=true;
		else if (IsEqualGUID(id, WPD_EVENT_OBJECT_ADDED) ||
			IsEqualGUID(id, WPD_EVENT_OBJECT_REMOVED) ||
			IsEqualGUID(id, WPD_EVENT_OBJECT_UPDATED) ||
			IsEqualGUID(id, WPD_EVENT_OBJECT_TRANSFER_REQUESTED))
			MarkObjectCacheDirty();
		return S_OK;
	}
};

static CWpdEvents g_events;

HRESULT AdviseWpdDevice(IPortableDevice* dev, LPWSTR* cookie)
{
	if (cookie)
		*cookie=NULL;
	if (!dev)
		return E_POINTER;
	return dev->Advise(0, &g_events, NULL, cookie);
}

void UnadviseWpdDevice(IPortableDevice* dev, LPWSTR cookie)
{
	if (dev && cookie)
		dev->Unadvise(cookie);
	if (cookie)
		CoTaskMemFree(cookie);
}

BOOL ShouldHideWpdDevice(IPortableDeviceManager* mgr, LPCWSTR pnpId, LPCWSTR friendly)
{
	static const WCHAR* junk[]={
		L"Pass Through", L"PassThrough", L"Media Library",
		L"WIA", L"Scanner", L"Printer", L"Fax", L"SIDESHOW",
		L"SideShow", L"Microsoft XPS"
	};
	WCHAR desc[256]=L"";
	DWORD dlen=256;
	if (mgr && pnpId)
		mgr->GetDeviceDescription((LPWSTR)pnpId, desc, &dlen);
	for (int i=0;i<(int)(sizeof(junk)/sizeof(junk[0]));i++) {
		if (friendly && StrStrIW(friendly, junk[i]))
			return TRUE;
		if (desc[0] && StrStrIW(desc, junk[i]))
			return TRUE;
		if (pnpId && StrStrIW(pnpId, junk[i]))
			return TRUE;
	}
	return FALSE;
}

static BOOL PnpIdToInstanceId(LPCWSTR pnp, WCHAR* inst, DWORD instcch)
{
	if (!pnp || !inst || instcch<8)
		return FALSE;
	const WCHAR* p=pnp;
	if (p[0]=='\\' && p[1]=='\\' && p[2]=='?' && p[3]=='\\')
		p+=4;
	wcslcpy(inst, p, instcch);
	for (WCHAR* q=inst; *q; q++) {
		if (*q=='#')
			*q='\\';
	}
	WCHAR* guid=wcsrchr(inst, '{');
	if (guid && guid>inst && guid[-1]=='\\')
		guid[-1]=0;
	return TRUE;
}

BOOL EjectWpdDevice(LPCWSTR pnpId)
{
	if (!pnpId)
		return FALSE;
	WCHAR inst[512];
	if (!PnpIdToInstanceId(pnpId, inst, 512))
		return FALSE;
	DEVINST devInst=0;
	if (CM_Locate_DevNodeW(&devInst, inst, CM_LOCATE_DEVNODE_NORMAL)!=CR_SUCCESS)
		return FALSE;
	PPNP_VETO_TYPE veto=NULL;
	CONFIGRET cr=CM_Request_Device_EjectW(devInst, veto, NULL, 0, 0);
	return cr==CR_SUCCESS;
}

void ShowDeviceInfoBox(HWND parent, LPCWSTR remoteName)
{
	EnsureComApartment();
	InitFunctionsIfNeeded(TRUE);
	WCHAR msg[1024];
	msg[0]=0;
	LPCWSTR pnp=FindPnpIdByPath(remoteName);
	IPortableDevice* dev=FindStoredDeviceByPath(remoteName);
	swprintf_s(msg, L"%s\r\n", PLUGIN_DISPLAY_NAME_W);
	if (pnp) {
		wcslcat(msg, L"PnP: ", 1024);
		wcslcat(msg, pnp, 1024);
		wcslcat(msg, L"\r\n", 1024);
	}
	if (!dev && pnp) {
		LockPlugin();
		WCHAR path[wdirtypemax];
		wcslcpy(path, remoteName, wdirtypemax-1);
		IPortableDeviceContent* c=NULL;
		LPWSTR id=NULL;
		GetFolderIDFromPathName(path, NULL, NULL, &c, &id);
		if (c) c->Release();
		if (id) CoTaskMemFree(id);
		dev=FindStoredDeviceByPath(remoteName);
		UnlockPlugin();
	}
	if (dev) {
		IPortableDeviceContent* content=NULL;
		if (SUCCEEDED(dev->Content(&content))) {
			IPortableDeviceProperties* props=NULL;
			if (SUCCEEDED(content->Properties(&props))) {
				IPortableDeviceKeyCollection* keys=NULL;
				if (SUCCEEDED(CoCreateInstance(CLSID_PortableDeviceKeyCollection, NULL, CLSCTX_INPROC_SERVER,
					IID_IPortableDeviceKeyCollection, (void**)&keys))) {
					keys->Add(WPD_DEVICE_FRIENDLY_NAME);
					keys->Add(WPD_DEVICE_SERIAL_NUMBER);
					keys->Add(WPD_DEVICE_FIRMWARE_VERSION);
					keys->Add(WPD_DEVICE_PROTOCOL);
					keys->Add(WPD_DEVICE_POWER_LEVEL);
					keys->Add(WPD_DEVICE_MANUFACTURER);
					keys->Add(WPD_DEVICE_MODEL);
					IPortableDeviceValues* v=NULL;
					if (SUCCEEDED(props->GetValues(WPD_DEVICE_OBJECT_ID, keys, &v)) && v) {
						LPWSTR s=NULL;
						if (SUCCEEDED(v->GetStringValue(WPD_DEVICE_MANUFACTURER, &s)) && s) {
							wcslcat(msg, L"Manufacturer: ", 1024); wcslcat(msg, s, 1024); wcslcat(msg, L"\r\n", 1024);
							CoTaskMemFree(s); s=NULL;
						}
						if (SUCCEEDED(v->GetStringValue(WPD_DEVICE_MODEL, &s)) && s) {
							wcslcat(msg, L"Model: ", 1024); wcslcat(msg, s, 1024); wcslcat(msg, L"\r\n", 1024);
							CoTaskMemFree(s); s=NULL;
						}
						if (SUCCEEDED(v->GetStringValue(WPD_DEVICE_SERIAL_NUMBER, &s)) && s) {
							wcslcat(msg, L"Serial: ", 1024); wcslcat(msg, s, 1024); wcslcat(msg, L"\r\n", 1024);
							CoTaskMemFree(s); s=NULL;
						}
						if (SUCCEEDED(v->GetStringValue(WPD_DEVICE_FIRMWARE_VERSION, &s)) && s) {
							wcslcat(msg, L"Firmware: ", 1024); wcslcat(msg, s, 1024); wcslcat(msg, L"\r\n", 1024);
							CoTaskMemFree(s); s=NULL;
						}
						if (SUCCEEDED(v->GetStringValue(WPD_DEVICE_PROTOCOL, &s)) && s) {
							wcslcat(msg, L"Protocol: ", 1024); wcslcat(msg, s, 1024); wcslcat(msg, L"\r\n", 1024);
							CoTaskMemFree(s); s=NULL;
						}
						ULONG bat=0;
						if (SUCCEEDED(v->GetUnsignedIntegerValue(WPD_DEVICE_POWER_LEVEL, &bat))) {
							WCHAR b[64];
							swprintf_s(b, L"Battery: %u%%\r\n", bat);
							wcslcat(msg, b, 1024);
						}
						v->Release();
					}
					keys->Release();
				}
				props->Release();
			}
			content->Release();
		}
	}
	MessageBoxW(parent, msg, PLUGIN_DISPLAY_NAME_W, MB_OK | MB_ICONINFORMATION);
}
