#include <windows.h>
#include <uxtheme.h>
#include "cunicode.h"

#pragma comment(lib, "uxtheme.lib")
#include "resource.h"
#include "wpdplug.h"
extern WCHAR DefaultIniNameW[MAX_PATH];
WCHAR SettingsName[MAX_PATH];
WCHAR LastSettingsName[MAX_PATH]={0};
int LastLocalTime=0;

static int ReadLocalTimeIni(WCHAR* keyName)
{
	int value=GetPrivateProfileIntW(PLUGIN_INI_SECTION,keyName,-1,DefaultIniNameW);
	if (value>=0)
		return value;
	return GetPrivateProfileIntW(PLUGIN_INI_SECTION_LEGACY,keyName,2,DefaultIniNameW);
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

BOOL CALLBACK PropDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	WCHAR KeyName[MAX_PATH];
	WCHAR* local=NULL;
	wcslcpy(KeyName,SettingsName,MAX_PATH-1);
	wcslcat(KeyName,L"_LOCALTIME",MAX_PATH-1);
	switch (message)
	{
		case WM_INITDIALOG:
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
	return DialogBox(hInst, (LPCTSTR)IDD_CONNECTIONSETTINGS, parent, (DLGPROC)PropDlg)==IDOK;
}

