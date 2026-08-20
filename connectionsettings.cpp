#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <shellapi.h>
#include "cunicode.h"
#include "resource.h"
#include "wpdplug.h"
#include "wpdplug_int.h"

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

extern WCHAR DefaultIniNameW[MAX_PATH];
WCHAR SettingsName[MAX_PATH];
WCHAR LastSettingsName[MAX_PATH]={0};
int LastLocalTime=0;

enum { UI_LANG_EN=0, UI_LANG_RU=1 };

static PluginDeviceInfo g_dlgInfo;
static BOOL g_dlgInfoOk=FALSE;
static HICON g_aboutIcon=NULL;

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
	WCHAR KeyName[MAX_PATH];
	wcslcpy(LastSettingsName,SettingsName2,MAX_PATH-1);
	wcslcpy(KeyName,SettingsName2,MAX_PATH-1);
	wcslcat(KeyName,L"_LOCALTIME",MAX_PATH-1);
	LastLocalTime=ReadLocalTimeIni(KeyName);
	return LastLocalTime;
}

static BOOL CALLBACK DeviceInfoDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		{
			int lang=GetPluginUiLanguage();
			SetWindowTextW(hDlg, SettingsName[0] ? SettingsName : PLUGIN_DISPLAY_NAME_W);
			WCHAR text[2048];
			if (g_dlgInfoOk)
				FormatDeviceInfo(lang, &g_dlgInfo, text, 2048);
			else
				wcslcpy(text, lang==UI_LANG_RU
					? L"Не удалось прочитать сведения об устройстве."
					: L"Could not read device information.", 2048);
			SetDlgItemTextW(hDlg, IDC_DEVICE_INFO, text);
			SetDlgItemTextW(hDlg, IDOK, lang==UI_LANG_RU ? L"ОК" : L"OK");
			SetWindowTheme(hDlg, L"Explorer", NULL);
		}
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam)==IDOK || LOWORD(wParam)==IDCANCEL) {
			EndDialog(hDlg, IDOK);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static void OpenGitHub(HWND parent)
{
	ShellExecuteW(parent, L"open", PLUGIN_GITHUB_URL_W, NULL, NULL, SW_SHOWNORMAL);
}

static BOOL CALLBACK PluginAboutDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			int ru=(GetPluginUiLanguage()==UI_LANG_RU);
			g_aboutIcon=(HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR);
			if (g_aboutIcon) {
				SendDlgItemMessageW(hDlg, IDC_PLUGIN_ICON, STM_SETICON, (WPARAM)g_aboutIcon, 0);
				SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)g_aboutIcon);
			}
			SetWindowTextW(hDlg, PLUGIN_DISPLAY_NAME_W);
			SetDlgItemTextW(hDlg, IDC_ABOUT_TITLE, PLUGIN_DISPLAY_NAME_W);
			WCHAR ver[80];
			swprintf_s(ver, ru ? L"Версия %hs" : L"Version %hs", PLUGIN_VERSION_STR);
			SetDlgItemTextW(hDlg, IDC_ABOUT_VER, ver);
			SetDlgItemTextW(hDlg, IDC_ABOUT_DESC, ru
				? L"Плагин Total Commander для Android (MTP) и iPhone.\nКаталоги читаются быстрее стандартного MTP."
				: L"Total Commander plugin for Android (MTP) and iPhone.\nDirectory listing is faster than standard MTP.");
			WCHAR link[320];
			swprintf_s(link, L"<a href=\"%s\">%s</a>", PLUGIN_GITHUB_URL_W, PLUGIN_GITHUB_URL_W);
			SetDlgItemTextW(hDlg, IDC_GITHUB_LINK, link);
			SetDlgItemTextW(hDlg, IDOK, ru ? L"ОК" : L"OK");
			SetWindowTheme(hDlg, L"Explorer", NULL);
		}
		return TRUE;
	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->idFrom==IDC_GITHUB_LINK &&
			(((LPNMHDR)lParam)->code==NM_CLICK || ((LPNMHDR)lParam)->code==NM_RETURN)) {
			PNMLINK pNMLink=(PNMLINK)lParam;
			if (pNMLink->item.szUrl[0])
				ShellExecuteW(hDlg, L"open", pNMLink->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
			else
				OpenGitHub(hDlg);
			return TRUE;
		}
		break;
	case WM_COMMAND:
		if (LOWORD(wParam)==IDOK || LOWORD(wParam)==IDCANCEL) {
			EndDialog(hDlg, IDOK);
			return TRUE;
		}
		break;
	case WM_DESTROY:
		if (g_aboutIcon) {
			DestroyIcon(g_aboutIcon);
			g_aboutIcon=NULL;
		}
		break;
	}
	return FALSE;
}

BOOL ShowDevicePropertiesDialog(HINSTANCE hInstDlg, HWND parent, WCHAR* RemoteName)
{
	if (RemoteName && RemoteName[0]=='\\')
		RemoteName++;
	if (RemoteName)
		wcslcpy(SettingsName, RemoteName, MAX_PATH-1);
	else
		SettingsName[0]=0;
	wcutlastbackslash(SettingsName);
	g_dlgInfoOk=QueryDeviceInfo(SettingsName, &g_dlgInfo);
	return DialogBoxW(hInstDlg, MAKEINTRESOURCEW(IDD_DEVICEINFO), parent, (DLGPROC)DeviceInfoDlg)==IDOK;
}

BOOL ShowPluginAboutDialog(HINSTANCE hInstDlg, HWND parent)
{
	INITCOMMONCONTROLSEX icc={sizeof(icc), ICC_LINK_CLASS};
	InitCommonControlsEx(&icc);
	return DialogBoxW(hInstDlg, MAKEINTRESOURCEW(IDD_PLUGINABOUT), parent, (DLGPROC)PluginAboutDlg)==IDOK;
}

BOOL ChangeConnectionSettingsW(HINSTANCE hInstDlg, HWND parent, WCHAR* RemoteName)
{
	return ShowDevicePropertiesDialog(hInstDlg, parent, RemoteName);
}
