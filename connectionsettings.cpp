#include <windows.h>
#include <uxtheme.h>
#include "cunicode.h"

#pragma comment(lib, "uxtheme.lib")
#include "resource.h"
#include "wpdplug.h"
#include "wpdplug_int.h"

extern WCHAR DefaultIniNameW[MAX_PATH];
WCHAR SettingsName[MAX_PATH];
WCHAR LastSettingsName[MAX_PATH]={0};
int LastLocalTime=0;

enum { UI_LANG_EN=0, UI_LANG_RU=1 };

struct DlgStrings {
	const WCHAR* caption;
	const WCHAR* langLabel;
	const WCHAR* infoGroup;
	const WCHAR* timeGroup;
	const WCHAR* localNew;
	const WCHAR* localOld;
	const WCHAR* utc;
	const WCHAR* hint1;
	const WCHAR* ok;
	const WCHAR* cancel;
};

static const DlgStrings kStrEn={
	L"Device Settings",
	L"Language:",
	L"Android / device",
	L"File time sent by the device",
	L"Local time, new conversion (Android)",
	L"Local time, old conversion (some mp3 players)",
	L"UTC (some mp3 players)",
	L"Hint: upload a file and check whether the timestamp is correct. On most devices it will be the current time.",
	L"OK",
	L"Cancel"
};

static const DlgStrings kStrRu={
	L"Настройки устройства",
	L"Язык:",
	L"Android / устройство",
	L"Дата и время, которые присылает устройство",
	L"Местное время, новый способ (Android)",
	L"Местное время, старый способ (некоторые mp3-плееры)",
	L"Всемирное время UTC (некоторые mp3-плееры)",
	L"Подсказка: загрузите файл и проверьте, верна ли дата. На большинстве устройств это текущее время.",
	L"ОК",
	L"Отмена"
};

static PluginDeviceInfo g_dlgInfo;
static BOOL g_dlgInfoOk=FALSE;

static int ReadLocalTimeIni(WCHAR* keyName)
{
	int value=GetPrivateProfileIntW(PLUGIN_INI_SECTION,keyName,-1,DefaultIniNameW);
	if (value>=0)
		return value;
	value=GetPrivateProfileIntW(PLUGIN_INI_SECTION_LEGACY,keyName,-1,DefaultIniNameW);
	if (value>=0)
		return value;
	return GetPrivateProfileIntW(PLUGIN_INI_SECTION_LEGACY2,keyName,2,DefaultIniNameW);
}

int GetPluginUiLanguage(void)
{
	WCHAR buf[16]=L"";
	GetPrivateProfileStringW(PLUGIN_INI_SECTION,L"Language",L"",buf,16,DefaultIniNameW);
	if (!buf[0])
		GetPrivateProfileStringW(PLUGIN_INI_SECTION_LEGACY,L"Language",L"",buf,16,DefaultIniNameW);
	if (buf[0]==L'r' || buf[0]==L'R' || buf[0]==L'1')
		return UI_LANG_RU;
	if (buf[0]==L'e' || buf[0]==L'E' || buf[0]==L'0')
		return UI_LANG_EN;
	if (PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_RUSSIAN)
		return UI_LANG_RU;
	return UI_LANG_EN;
}

static void SavePluginUiLanguage(int lang)
{
	WritePrivateProfileStringW(PLUGIN_INI_SECTION,L"Language",
		lang==UI_LANG_RU ? L"ru" : L"en", DefaultIniNameW);
}

int UseLocalTime(WCHAR* path)
{
	WCHAR SettingsName2[MAX_PATH];
	if (path[0]=='\\')
		path++;
	wcslcpy(SettingsName2,path,MAX_PATH-1);
	WCHAR* p=wcschr(SettingsName2,'\\');
	if (p)
		p[0]=0;
	if (wcscmp(LastSettingsName,SettingsName2)==0)
		return LastLocalTime;
	else {
		WCHAR KeyName[MAX_PATH];
		wcslcpy(LastSettingsName,SettingsName2,MAX_PATH-1);
		wcslcpy(KeyName,SettingsName2,MAX_PATH-1);
		wcslcat(KeyName,L"_LOCALTIME",MAX_PATH-1);
		LastLocalTime=ReadLocalTimeIni(KeyName);
		return LastLocalTime;
	}
}

static void FillDeviceInfoBox(HWND hDlg, int lang)
{
	WCHAR text[2048];
	if (g_dlgInfoOk)
		FormatDeviceInfo(lang, &g_dlgInfo, text, 2048);
	else
		wcslcpy(text, lang==UI_LANG_RU
			? L"Не удалось прочитать сведения (откройте устройство и повторите)."
			: L"Could not read device info (open the device and retry).", 2048);
	SetDlgItemTextW(hDlg, IDC_DEVICE_INFO, text);
}

static void ApplyDlgLanguage(HWND hDlg, int lang)
{
	const DlgStrings* s=(lang==UI_LANG_RU) ? &kStrRu : &kStrEn;
	SetWindowTextW(hDlg, s->caption);
	SetDlgItemTextW(hDlg, IDC_LANG_LABEL, s->langLabel);
	SetDlgItemTextW(hDlg, IDC_INFO_GROUP, s->infoGroup);
	SetDlgItemTextW(hDlg, IDC_TIME_GROUP, s->timeGroup);
	SetDlgItemTextW(hDlg, IDC_LOCALTIME_NEW, s->localNew);
	SetDlgItemTextW(hDlg, IDC_LOCALTIME_OLD, s->localOld);
	SetDlgItemTextW(hDlg, IDC_UNIVERSALTIME, s->utc);
	SetDlgItemTextW(hDlg, IDC_HINT1, s->hint1);
	SetDlgItemTextW(hDlg, IDOK, s->ok);
	SetDlgItemTextW(hDlg, IDCANCEL, s->cancel);
	FillDeviceInfoBox(hDlg, lang);
}

BOOL CALLBACK PropDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	WCHAR KeyName[MAX_PATH];
	WCHAR* local=NULL;
	wcslcpy(KeyName,SettingsName,MAX_PATH-1);
	wcslcat(KeyName,L"_LOCALTIME",MAX_PATH-1);
	switch (message)
	{
		case WM_INITDIALOG:
			SendDlgItemMessageW(hDlg, IDC_LANGUAGE, CB_ADDSTRING, 0, (LPARAM)L"English");
			SendDlgItemMessageW(hDlg, IDC_LANGUAGE, CB_ADDSTRING, 0, (LPARAM)L"Русский");
			g_dlgInfoOk=QueryDeviceInfo(SettingsName, &g_dlgInfo);
			{
				int lang=GetPluginUiLanguage();
				SendDlgItemMessageW(hDlg, IDC_LANGUAGE, CB_SETCURSEL, lang, 0);
				ApplyDlgLanguage(hDlg, lang);
			}
			switch (ReadLocalTimeIni(KeyName))
			{
			case 1:
				CheckDlgButton(hDlg,IDC_LOCALTIME_OLD,BST_CHECKED);
				break;
			case 2:
				CheckDlgButton(hDlg,IDC_LOCALTIME_NEW,BST_CHECKED);
				break;
			default:
				CheckDlgButton(hDlg,IDC_UNIVERSALTIME,BST_CHECKED);
				break;
			}
			SetWindowTheme(hDlg, L"Explorer", NULL);
			return TRUE;
		case WM_COMMAND:
			if (LOWORD(wParam)==IDC_LANGUAGE && HIWORD(wParam)==CBN_SELCHANGE) {
				int lang=(int)SendDlgItemMessageW(hDlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
				if (lang<0)
					lang=UI_LANG_EN;
				ApplyDlgLanguage(hDlg, lang);
				return TRUE;
			}
			switch (LOWORD(wParam)) {
			case IDOK:
				if (IsDlgButtonChecked(hDlg,IDC_LOCALTIME_OLD))
					local=L"1";
				else if (IsDlgButtonChecked(hDlg,IDC_LOCALTIME_NEW))
					local=L"2";
				else
					local=L"0";
				WritePrivateProfileStringW(PLUGIN_INI_SECTION,KeyName,local,
					DefaultIniNameW);
				{
					int lang=(int)SendDlgItemMessageW(hDlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
					SavePluginUiLanguage(lang==UI_LANG_RU ? UI_LANG_RU : UI_LANG_EN);
				}
				LastSettingsName[0]=0;   // reset value
			case IDCANCEL:
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			break;
	}
	return FALSE;
}

BOOL ChangeConnectionSettingsW(HINSTANCE hInst,HWND parent,WCHAR* RemoteName)
{
	if (RemoteName[0]=='\\')
		RemoteName++;
	wcslcpy(SettingsName,RemoteName,MAX_PATH-1);
	wcutlastbackslash(SettingsName);
	return DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_CONNECTIONSETTINGS), parent, (DLGPROC)PropDlg)==IDOK;
}
