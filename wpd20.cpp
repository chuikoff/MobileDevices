#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <shlwapi.h>
#include <propvarutil.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include "wpdplug.h"
#include "cunicode.h"
#include "fsplugin.h"
#include "wpdplug_int.h"
#include "connectionsettings.h"
#include "apple_md.h"

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

static BOOL ContainsAny(LPCWSTR hay, const WCHAR** needles, int n)
{
	if (!hay)
		return FALSE;
	for (int i=0;i<n;i++) {
		if (StrStrIW(hay, needles[i]))
			return TRUE;
	}
	return FALSE;
}

BOOL ShouldHideWpdDevice(IPortableDeviceManager* mgr, LPCWSTR pnpId, LPCWSTR friendly)
{
	static const WCHAR* junk[]={
		L"Pass Through", L"PassThrough", L"Media Library",
		L"WIA", L"Scanner", L"Printer", L"Fax", L"SIDESHOW",
		L"SideShow", L"Microsoft XPS"
	};
	static const WCHAR* apple[]={
		L"Apple", L"iPhone", L"iPad", L"iPod", L"VID_05AC"
	};
	static const WCHAR* androidHint[]={
		L"Android", L"Google", L"Pixel", L"Samsung", L"Galaxy", L"Xiaomi", L"Redmi",
		L"POCO", L"HUAWEI", L"Honor", L"OnePlus", L"OPPO", L"vivo", L"Realme",
		L"Motorola", L"Sony", L"ASUS", L"ZTE", L"Nokia", L"Nothing", L"Fairphone",
		L"VID_18D1", L"VID_04E8", L"VID_22B8", L"VID_0BB4", L"VID_12D1", L"VID_2717",
		L"VID_2A70", L"VID_0FCE", L"VID_0E8D", L"VID_05C6", L"VID_0B05", L"VID_2A45",
		L"VID_22D9", L"VID_19D2"
	};
	WCHAR desc[256]=L"";
	DWORD dlen=256;
	if (mgr && pnpId)
		mgr->GetDeviceDescription((LPWSTR)pnpId, desc, &dlen);
	if (ContainsAny(friendly, junk, (int)(sizeof(junk)/sizeof(junk[0]))) ||
		ContainsAny(desc, junk, (int)(sizeof(junk)/sizeof(junk[0]))) ||
		ContainsAny(pnpId, junk, (int)(sizeof(junk)/sizeof(junk[0]))))
		return TRUE;
	if (ContainsAny(friendly, apple, 5) || ContainsAny(desc, apple, 5) || ContainsAny(pnpId, apple, 5))
		return TRUE;
	int nh=(int)(sizeof(androidHint)/sizeof(androidHint[0]));
	if (ContainsAny(friendly, androidHint, nh) || ContainsAny(desc, androidHint, nh) || ContainsAny(pnpId, androidHint, nh))
		return FALSE;
	return TRUE;
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

static void CopyWpdString(IPortableDeviceValues* v, REFPROPERTYKEY key, WCHAR* dest, int destcch)
{
	dest[0]=0;
	if (!v)
		return;
	LPWSTR s=NULL;
	if (SUCCEEDED(v->GetStringValue(key, &s)) && s && s[0])
		wcslcpy(dest, s, destcch);
	if (s)
		CoTaskMemFree(s);
}

static int ReadBatteryPercent(IPortableDeviceValues* v)
{
	if (!v)
		return -1;
	ULONG u=0;
	if (SUCCEEDED(v->GetUnsignedIntegerValue(WPD_DEVICE_POWER_LEVEL, &u)))
		return (int)u;
	PROPVARIANT pv;
	PropVariantInit(&pv);
	int bat=-1;
	if (SUCCEEDED(v->GetValue(WPD_DEVICE_POWER_LEVEL, &pv))) {
		ULONGLONG n=0;
		if (pv.vt==VT_UI4 || pv.vt==VT_I4)
			bat=(int)pv.ulVal;
		else if (pv.vt==VT_UI1)
			bat=(int)pv.bVal;
		else if (SUCCEEDED(PropVariantToUInt64(pv, &n)))
			bat=(int)n;
	}
	PropVariantClear(&pv);
	return bat;
}

static void FormatBytes(ULONGLONG n, int lang, WCHAR* buf, int cch)
{
	const double gb=1024.0*1024.0*1024.0;
	const double mb=1024.0*1024.0;
	if (n>=(ULONGLONG)(10*gb))
		swprintf_s(buf, cch, lang ? L"%.1f ГБ" : L"%.1f GB", n/gb);
	else if (n>=(ULONGLONG)gb)
		swprintf_s(buf, cch, lang ? L"%.2f ГБ" : L"%.2f GB", n/gb);
	else if (n>=(ULONGLONG)mb)
		swprintf_s(buf, cch, lang ? L"%.0f МБ" : L"%.0f MB", n/mb);
	else
		swprintf_s(buf, cch, lang ? L"%llu байт" : L"%llu bytes", n);
}

/* AOSP puts "1.0" in the MTP device-version field. That is not the Android release. */
static BOOL IsPlaceholderOsVersion(const WCHAR* s)
{
	if (!s || !s[0])
		return TRUE;
	return _wcsicmp(s, L"1")==0 || _wcsicmp(s, L"1.0")==0 || _wcsicmp(s, L"1.0.0")==0
		|| _wcsicmp(s, L"0")==0 || _wcsicmp(s, L"0.0")==0 || _wcsicmp(s, L"0.0.0")==0
		|| _wcsicmp(s, L"unknown")==0 || _wcsicmp(s, L"none")==0 || _wcsicmp(s, L"n/a")==0
		|| _wcsicmp(s, L"mtp")==0 || _wcsicmp(s, L"android")==0;
}

static void TrimTrailingDotZero(WCHAR* s)
{
	if (!s)
		return;
	for (;;) {
		int n=(int)wcslen(s);
		if (n>=2 && s[n-2]==L'.' && s[n-1]==L'0')
			s[n-2]=0;
		else
			break;
	}
}

/* 4..20, optional .minor and .patch. Rejects the AOSP placeholder 1.0. */
static BOOL LooksLikeAndroidRelease(const WCHAR* s)
{
	const WCHAR* p;
	int major=0;
	int dots=0;
	if (!s || !s[0])
		return FALSE;
	p=s;
	if (*p<L'0' || *p>L'9')
		return FALSE;
	while (*p>=L'0' && *p<=L'9') {
		major=major*10+(*p-L'0');
		if (major>20)
			return FALSE;
		p++;
	}
	if (major<4 || major>20)
		return FALSE;
	while (*p==L'.') {
		int part=0;
		int digits=0;
		p++;
		dots++;
		if (dots>2 || *p<L'0' || *p>L'9')
			return FALSE;
		while (*p>=L'0' && *p<=L'9') {
			part=part*10+(*p-L'0');
			digits++;
			if (digits>2 || part>99)
				return FALSE;
			p++;
		}
	}
	return *p==0;
}

static void NoteAndroidVersion(PluginDeviceInfo* info, const WCHAR* raw, BOOL bareNumber)
{
	WCHAR tmp[128];
	const WCHAR* ver=NULL;
	WCHAR rel[40];
	int n;
	if (!info || !raw || !raw[0])
		return;
	if (info->os[0] && _wcsicmp(info->os, L"Android")!=0)
		return;
	while (*raw==L' ' || *raw==L'\t')
		raw++;
	wcslcpy(tmp, raw, 128);
	n=(int)wcslen(tmp);
	while (n>0 && (tmp[n-1]==L' ' || tmp[n-1]==L'\t' || tmp[n-1]==L'\r' || tmp[n-1]==L'\n'))
		tmp[--n]=0;
	if (IsPlaceholderOsVersion(tmp))
		return;
	if (_wcsnicmp(tmp, L"android", 7)==0) {
		const WCHAR* p=tmp+7;
		if (*p==L' ' || *p==L'-' || *p==L':' || *p==L'/') {
			while (*p==L' ' || *p==L'-' || *p==L':' || *p==L'/')
				p++;
			ver=p;
		}
	} else if (bareNumber && LooksLikeAndroidRelease(tmp))
		ver=tmp;
	if (!ver || !ver[0])
		return;
	wcslcpy(rel, ver, 40);
	{
		WCHAR* sp=wcspbrk(rel, L" \t(");
		if (sp)
			*sp=0;
	}
	TrimTrailingDotZero(rel);
	if (!LooksLikeAndroidRelease(rel))
		return;
	swprintf_s(info->os, 40, L"Android %s", rel);
}

static void ScanAndroidVersionProps(IPortableDeviceProperties* props, PluginDeviceInfo* info)
{
	IPortableDeviceKeyCollection* all=NULL;
	IPortableDeviceValues* vals=NULL;
	DWORD count=0;
	DWORD i;
	int pass;
	if (!props || !info)
		return;
	if (info->os[0] && _wcsicmp(info->os, L"Android")!=0)
		return;
	if (FAILED(props->GetSupportedProperties(WPD_DEVICE_OBJECT_ID, &all)) || !all) {
		SAFE_RELEASE(all);
		return;
	}
	if (FAILED(props->GetValues(WPD_DEVICE_OBJECT_ID, all, &vals)) || !vals) {
		SAFE_RELEASE(all);
		return;
	}
	SAFE_RELEASE(all);
	if (FAILED(vals->GetCount(&count))) {
		SAFE_RELEASE(vals);
		return;
	}
	if (count>80)
		count=80;
	for (pass=0; pass<2; pass++) {
		for (i=0; i<count; i++) {
			PROPERTYKEY key;
			PROPVARIANT pv;
			PropVariantInit(&pv);
			if (SUCCEEDED(vals->GetAt(i, &key, &pv)) && pv.vt==VT_LPWSTR && pv.pwszVal)
				NoteAndroidVersion(info, pv.pwszVal, pass==1);
			PropVariantClear(&pv);
			if (info->os[0] && _wcsicmp(info->os, L"Android")!=0)
				break;
		}
		if (info->os[0] && _wcsicmp(info->os, L"Android")!=0)
			break;
	}
	SAFE_RELEASE(vals);
}

BOOL QueryDeviceInfo(LPCWSTR remoteName, PluginDeviceInfo* info)
{
	if (!info)
		return FALSE;
	memset(info, 0, sizeof(*info));
	info->battery=-1;
	info->batteryHealth=-1;
	info->batteryCycles=-1;
	if (!remoteName || remoteName[0]==0)
		return FALSE;

	{
		WCHAR dev[MAX_PATH];
		const WCHAR* p=remoteName;
		if (p[0]=='\\') p++;
		wcslcpy(dev, p, MAX_PATH);
		WCHAR* sl=wcschr(dev, '\\');
		if (sl) sl[0]=0;
		if (AppleMdIsDeviceName(dev))
			return AppleMdFillInfo(dev, info);
	}

	ComApartmentGuard comApt;
	if (!comApt.ok())
		return FALSE;
	if (!InitFunctionsIfNeeded(TRUE))
		return FALSE;

	WCHAR path[wdirtypemax];
	if (remoteName[0]=='\\')
		wcslcpy(path, remoteName, wdirtypemax-1);
	else {
		path[0]='\\';
		wcslcpy(path+1, remoteName, wdirtypemax-2);
	}
	WCHAR* slash=wcschr(path+1, '\\');
	if (slash)
		slash[1]=0;
	else
		wcslcatbackslash(path, wdirtypemax-1);

	LockPlugin();
	IPortableDeviceContent* content=NULL;
	IPortableDeviceProperties* props=NULL;
	LPWSTR rootId=NULL;
	HRESULT hr=GetFolderIDFromPathName(path, NULL, &props, &content, &rootId);
	if (FAILED(hr) || !content || !props) {
		UnlockPlugin();
		SAFE_RELEASE(content);
		SAFE_RELEASE(props);
		if (rootId) CoTaskMemFree(rootId);
		return FALSE;
	}

	IPortableDeviceKeyCollection* keys=NULL;
	if (SUCCEEDED(CoCreateInstance(CLSID_PortableDeviceKeyCollection, NULL, CLSCTX_INPROC_SERVER,
		IID_IPortableDeviceKeyCollection, (void**)&keys))) {
		keys->Add(WPD_DEVICE_FRIENDLY_NAME);
		keys->Add(WPD_DEVICE_MANUFACTURER);
		keys->Add(WPD_DEVICE_MODEL);
		keys->Add(WPD_DEVICE_PROTOCOL);
		keys->Add(WPD_DEVICE_POWER_LEVEL);
		keys->Add(WPD_DEVICE_FIRMWARE_VERSION);
		keys->Add(WPD_DEVICE_SERIAL_NUMBER);
		IPortableDeviceValues* v=NULL;
		if (SUCCEEDED(props->GetValues(WPD_DEVICE_OBJECT_ID, keys, &v)) && v) {
			WCHAR fw[128];
			fw[0]=0;
			CopyWpdString(v, WPD_DEVICE_MANUFACTURER, info->manufacturer, 128);
			CopyWpdString(v, WPD_DEVICE_MODEL, info->model, 128);
			if (!info->model[0])
				CopyWpdString(v, WPD_DEVICE_FRIENDLY_NAME, info->model, 128);
			CopyWpdString(v, WPD_DEVICE_FIRMWARE_VERSION, fw, 128);
			CopyWpdString(v, WPD_DEVICE_SERIAL_NUMBER, info->serial, 64);
			CopyWpdString(v, WPD_DEVICE_PROTOCOL, info->protocol, 80);
			wcslcpy(info->os, L"Android", 40);
			NoteAndroidVersion(info, fw, TRUE);
			if (fw[0] && !IsPlaceholderOsVersion(fw)) {
				BOOL onlyRelease=FALSE;
				if (_wcsnicmp(info->os, L"Android ", 8)==0 &&
					(_wcsicmp(fw, info->os+8)==0 || _wcsicmp(fw, info->os)==0))
					onlyRelease=TRUE;
				if (!onlyRelease)
					wcslcpy(info->firmware, fw, 128);
			}
			if (_wcsicmp(info->os, L"Android")==0)
				ScanAndroidVersionProps(props, info);
			info->battery=ReadBatteryPercent(v);
			SAFE_RELEASE(v);
		}
		SAFE_RELEASE(keys);
	}

	IEnumPortableDeviceObjectIDs* en=NULL;
	if (SUCCEEDED(content->EnumObjects(0, WPD_DEVICE_OBJECT_ID, NULL, &en)) && en) {
		PWSTR ids[32]={0};
		DWORD fetched=0;
		if (SUCCEEDED(en->Next(32, ids, &fetched))) {
			IPortableDeviceKeyCollection* sk=NULL;
			if (SUCCEEDED(CoCreateInstance(CLSID_PortableDeviceKeyCollection, NULL, CLSCTX_INPROC_SERVER,
				IID_IPortableDeviceKeyCollection, (void**)&sk))) {
				sk->Add(WPD_FUNCTIONAL_OBJECT_CATEGORY);
				sk->Add(WPD_OBJECT_NAME);
				sk->Add(WPD_OBJECT_ORIGINAL_FILE_NAME);
				sk->Add(WPD_STORAGE_DESCRIPTION);
				sk->Add(WPD_STORAGE_FREE_SPACE_IN_BYTES);
				sk->Add(WPD_STORAGE_CAPACITY);
				for (DWORD i=0;i<fetched;i++) {
					if (info->nstor<DEVICE_INFO_MAX_STOR) {
						IPortableDeviceValues* sv=NULL;
						if (SUCCEEDED(props->GetValues(ids[i], sk, &sv)) && sv) {
							GUID cat=GUID_NULL;
							sv->GetGuidValue(WPD_FUNCTIONAL_OBJECT_CATEGORY, &cat);
							ULONGLONG cap=0, freeb=0;
							sv->GetUnsignedLargeIntegerValue(WPD_STORAGE_CAPACITY, &cap);
							sv->GetUnsignedLargeIntegerValue(WPD_STORAGE_FREE_SPACE_IN_BYTES, &freeb);
							BOOL isStor=IsEqualGUID(cat, WPD_FUNCTIONAL_CATEGORY_STORAGE) || cap>0 || freeb>0;
							if (isStor) {
								int n=info->nstor;
								CopyWpdString(sv, WPD_STORAGE_DESCRIPTION, info->stor[n].name, 80);
								if (!info->stor[n].name[0])
									CopyWpdString(sv, WPD_OBJECT_ORIGINAL_FILE_NAME, info->stor[n].name, 80);
								if (!info->stor[n].name[0])
									CopyWpdString(sv, WPD_OBJECT_NAME, info->stor[n].name, 80);
								if (!info->stor[n].name[0])
									wcslcpy(info->stor[n].name, L"Storage", 80);
								info->stor[n].capacityBytes=cap;
								info->stor[n].freeBytes=freeb;
								info->nstor++;
							}
							SAFE_RELEASE(sv);
						}
					}
					CoTaskMemFree(ids[i]);
				}
				SAFE_RELEASE(sk);
			} else {
				for (DWORD i=0;i<fetched;i++)
					CoTaskMemFree(ids[i]);
			}
		}
		SAFE_RELEASE(en);
	}

	if (rootId)
		CoTaskMemFree(rootId);
	SAFE_RELEASE(props);
	SAFE_RELEASE(content);
	UnlockPlugin();
	return TRUE;
}

void FormatDeviceInfo(int lang, const PluginDeviceInfo* info, WCHAR* out, int outcch)
{
	if (!out || outcch<8)
		return;
	out[0]=0;
	if (!info)
		return;
	const int ru=(lang==1);
	WCHAR line[256];
	const WCHAR* na=ru ? L"н/д" : L"n/a";

	if (info->os[0]) {
		swprintf_s(line, ru ? L"ОС: %s\r\n" : L"OS: %s\r\n", info->os);
		wcslcat(out, line, outcch);
	}
	swprintf_s(line, ru ? L"Модель: %s\r\n" : L"Model: %s\r\n",
		info->model[0] ? info->model : na);
	wcslcat(out, line, outcch);
	if (info->manufacturer[0]) {
		swprintf_s(line, ru ? L"Производитель: %s\r\n" : L"Manufacturer: %s\r\n", info->manufacturer);
		wcslcat(out, line, outcch);
	}
	if (info->firmware[0]) {
		BOOL same=FALSE;
		const WCHAR* osVer=NULL;
		if (info->os[0] && _wcsicmp(info->os, info->firmware)==0)
			same=TRUE;
		if (_wcsnicmp(info->os, L"Android ", 8)==0)
			osVer=info->os+8;
		else if (_wcsnicmp(info->os, L"iOS ", 4)==0)
			osVer=info->os+4;
		if (osVer && _wcsicmp(osVer, info->firmware)==0)
			same=TRUE;
		if (!same) {
			swprintf_s(line, ru ? L"Прошивка: %s\r\n" : L"Firmware: %s\r\n", info->firmware);
			wcslcat(out, line, outcch);
		}
	}
	if (info->serial[0]) {
		swprintf_s(line, ru ? L"Серийный номер: %s\r\n" : L"Serial: %s\r\n", info->serial);
		wcslcat(out, line, outcch);
	}
	if (info->imei[0]) {
		swprintf_s(line, L"IMEI: %s\r\n", info->imei);
		wcslcat(out, line, outcch);
	}
	if (info->imei2[0]) {
		swprintf_s(line, L"IMEI2: %s\r\n", info->imei2);
		wcslcat(out, line, outcch);
	}
	if (info->battery>=0)
		swprintf_s(line, ru ? L"Батарея: %d%%\r\n" : L"Battery: %d%%\r\n", info->battery);
	else
		swprintf_s(line, ru ? L"Батарея: %s\r\n" : L"Battery: %s\r\n", na);
	wcslcat(out, line, outcch);
	if (info->batteryHealth>=0) {
		swprintf_s(line, ru ? L"Состояние батареи: %d%%\r\n" : L"Battery health: %d%%\r\n",
			info->batteryHealth);
		wcslcat(out, line, outcch);
	}
	if (info->batteryCycles>=0) {
		swprintf_s(line, ru ? L"Циклы зарядки: %d\r\n" : L"Charge cycles: %d\r\n",
			info->batteryCycles);
		wcslcat(out, line, outcch);
	}
	if (info->protocol[0]) {
		swprintf_s(line, ru ? L"Протокол: %s\r\n" : L"Protocol: %s\r\n", info->protocol);
		wcslcat(out, line, outcch);
	}
	if (info->nstor==0) {
		wcslcat(out, ru ? L"Память: н/д\r\n" : L"Storage: n/a\r\n", outcch);
	} else {
		for (int i=0;i<info->nstor;i++) {
			WCHAR fs[64], cs[64];
			if (info->stor[i].capacityBytes || info->stor[i].freeBytes) {
				FormatBytes(info->stor[i].freeBytes, ru, fs, 64);
				FormatBytes(info->stor[i].capacityBytes, ru, cs, 64);
				swprintf_s(line, ru ? L"%s: %s свободно / %s\r\n" : L"%s: %s free / %s\r\n",
					info->stor[i].name, fs, cs);
			} else
				swprintf_s(line, L"%s\r\n", info->stor[i].name);
			wcslcat(out, line, outcch);
		}
	}
	/* Apple disk_usage breakdown when detail fields are set. */
	if (info->systemCapacity || info->dataCapacity || info->totalDisk) {
		WCHAR usedS[64], usedD[64], freeS[64], purS[64];
		if (info->systemCapacity) {
			ULONGLONG used=info->systemCapacity;
			if (info->systemAvailable && info->systemAvailable<=info->systemCapacity)
				used=info->systemCapacity-info->systemAvailable;
			FormatBytes(used, ru, usedS, 64);
			swprintf_s(line, ru ? L"Система: %s\r\n" : L"System: %s\r\n", usedS);
			wcslcat(out, line, outcch);
		}
		if (info->dataCapacity) {
			ULONGLONG used=info->dataCapacity;
			if (info->dataAvailable && info->dataAvailable<=info->dataCapacity)
				used=info->dataCapacity-info->dataAvailable;
			FormatBytes(used, ru, usedD, 64);
			swprintf_s(line, ru ? L"Данные: %s\r\n" : L"Data: %s\r\n", usedD);
			wcslcat(out, line, outcch);
		}
		/* Hide a sub-megabyte gap; it is rounding, not a cache bucket. */
		if (info->purgeable>=1024ULL*1024ULL) {
			FormatBytes(info->purgeable, ru, purS, 64);
			swprintf_s(line, ru ? L"Очищаемое: %s\r\n" : L"Purgeable: %s\r\n", purS);
			wcslcat(out, line, outcch);
		}
		/* Same figure as the header. dataAvailable still includes purgeable. */
		ULONGLONG freeb=0;
		if (info->nstor>0)
			freeb=info->stor[0].freeBytes;
		else if (info->dataAvailable>info->purgeable)
			freeb=info->dataAvailable-info->purgeable;
		if (freeb || info->purgeable>=1024ULL*1024ULL) {
			FormatBytes(freeb, ru, freeS, 64);
			swprintf_s(line, ru ? L"Свободно: %s\r\n" : L"Free: %s\r\n", freeS);
			wcslcat(out, line, outcch);
		}
	}
	if (info->photoUsage) {
		WCHAR ps[64];
		FormatBytes(info->photoUsage, ru, ps, 64);
		swprintf_s(line, ru ? L"Фото: %s\r\n" : L"Photos: %s\r\n", ps);
		wcslcat(out, line, outcch);
	}
	if (info->appUsage) {
		WCHAR as[64];
		FormatBytes(info->appUsage, ru, as, 64);
		swprintf_s(line, ru ? L"Приложения: %s\r\n" : L"Apps: %s\r\n", as);
		wcslcat(out, line, outcch);
	}
}

void ShowDeviceInfoBox(HWND parent, LPCWSTR remoteName)
{
	WCHAR buf[wdirtypemax];
	wcslcpy(buf, remoteName ? remoteName : L"\\", wdirtypemax-1);
	ShowDevicePropertiesDialog(hInst, parent, buf);
}
