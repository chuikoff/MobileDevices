#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include "resource.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")

static BOOL WriteResFile(HINSTANCE inst, int id, LPCWSTR path)
{
	HRSRC rs=FindResourceW(inst, MAKEINTRESOURCEW(id), RT_RCDATA);
	if (!rs)
		return FALSE;
	DWORD sz=SizeofResource(inst, rs);
	HGLOBAL g=LoadResource(inst, rs);
	const void* p=LockResource(g);
	if (!p || !sz)
		return FALSE;
	HANDLE f=CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f==INVALID_HANDLE_VALUE)
		return FALSE;
	DWORD w=0;
	BOOL ok=WriteFile(f, p, sz, &w, NULL) && w==sz;
	CloseHandle(f);
	return ok;
}

static void Join(WCHAR* dst, DWORD cch, LPCWSTR a, LPCWSTR b)
{
	wcsncpy_s(dst, cch, a, _TRUNCATE);
	size_t n=wcslen(dst);
	if (n && dst[n-1]!=L'\\' && dst[n-1]!=L'/')
		wcsncat_s(dst, cch, L"\\", _TRUNCATE);
	wcsncat_s(dst, cch, b, _TRUNCATE);
}

static BOOL FindTotalCommander(WCHAR* dir, DWORD dircch, WCHAR* ini, DWORD inicch)
{
	dir[0]=0;
	ini[0]=0;
	HKEY k=NULL;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Ghisler\\Total Commander", 0, KEY_READ, &k)==ERROR_SUCCESS) {
		DWORD sz=dircch*sizeof(WCHAR);
		RegQueryValueExW(k, L"InstallDir", NULL, NULL, (LPBYTE)dir, &sz);
		sz=inicch*sizeof(WCHAR);
		RegQueryValueExW(k, L"IniFileName", NULL, NULL, (LPBYTE)ini, &sz);
		RegCloseKey(k);
	}
	if (!dir[0]) {
		static const WCHAR* tryPaths[]={
			L"C:\\totalcmd",
			L"C:\\Program Files\\totalcmd",
			L"C:\\Program Files\\Total Commander",
			L"C:\\Program Files (x86)\\totalcmd",
			L"C:\\Program Files (x86)\\Total Commander"
		};
		for (int i=0;i<(int)(sizeof(tryPaths)/sizeof(tryPaths[0]));i++) {
			WCHAR exe[MAX_PATH];
			Join(exe, MAX_PATH, tryPaths[i], L"TOTALCMD64.EXE");
			if (GetFileAttributesW(exe)!=INVALID_FILE_ATTRIBUTES) {
				wcsncpy_s(dir, dircch, tryPaths[i], _TRUNCATE);
				break;
			}
			Join(exe, MAX_PATH, tryPaths[i], L"TOTALCMD.EXE");
			if (GetFileAttributesW(exe)!=INVALID_FILE_ATTRIBUTES) {
				wcsncpy_s(dir, dircch, tryPaths[i], _TRUNCATE);
				break;
			}
		}
	}
	if (dir[0] && (dir[0]==L'%' || !wcschr(dir, L':'))) {
		WCHAR exp[MAX_PATH];
		if (ExpandEnvironmentStringsW(dir, exp, MAX_PATH))
			wcsncpy_s(dir, dircch, exp, _TRUNCATE);
	}
	if (!ini[0] && dir[0])
		Join(ini, inicch, dir, L"wincmd.ini");
	if (ini[0] && ini[0]==L'%') {
		WCHAR exp[MAX_PATH];
		if (ExpandEnvironmentStringsW(ini, exp, MAX_PATH))
			wcsncpy_s(ini, inicch, exp, _TRUNCATE);
	}
	return dir[0]!=0;
}

static void RegisterPlugin(LPCWSTR ini, LPCWSTR section, LPCWSTR name, LPCWSTR file)
{
	if (!ini || !ini[0])
		return;
	WritePrivateProfileStringW(section, name, file, ini);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int)
{
	WCHAR tc[MAX_PATH], ini[MAX_PATH], dest[MAX_PATH], path[MAX_PATH];
	if (!FindTotalCommander(tc, MAX_PATH, ini, MAX_PATH)) {
		MessageBoxW(NULL,
			L"Total Commander не найден.\nУкажите каталог установки TC в реестре\nHKCU\\Software\\Ghisler\\Total Commander\\InstallDir",
			L"Mobile Devices", MB_ICONERROR);
		return 1;
	}
	Join(dest, MAX_PATH, tc, L"plugins\\wfx\\MobileDevices");
	if (!CreateDirectoryW(dest, NULL)) {
		DWORD err=GetLastError();
		if (err!=ERROR_ALREADY_EXISTS) {
			MessageBoxW(NULL, L"Не удалось создать каталог плагина.", L"Mobile Devices", MB_ICONERROR);
			return 1;
		}
	}
	Join(path, MAX_PATH, dest, L"mobiledevices.wfx");
	if (!WriteResFile(inst, IDR_WFX, path)) {
		MessageBoxW(NULL, L"Не удалось записать mobiledevices.wfx", L"Mobile Devices", MB_ICONERROR);
		return 1;
	}
	Join(path, MAX_PATH, dest, L"mobiledevices.wfx64");
	if (!WriteResFile(inst, IDR_WFX64, path)) {
		MessageBoxW(NULL, L"Не удалось записать mobiledevices.wfx64", L"Mobile Devices", MB_ICONERROR);
		return 1;
	}
	Join(path, MAX_PATH, dest, L"LICENSE.txt");
	WriteResFile(inst, IDR_LIC, path);
	Join(path, MAX_PATH, dest, L"ReadMe.txt");
	WriteResFile(inst, IDR_README, path);

	WCHAR wfx[MAX_PATH], wfx64[MAX_PATH];
	Join(wfx, MAX_PATH, dest, L"mobiledevices.wfx");
	Join(wfx64, MAX_PATH, dest, L"mobiledevices.wfx64");
	RegisterPlugin(ini, L"FileSystemPlugins", L"Mobile Devices", wfx);
	RegisterPlugin(ini, L"FileSystemPlugins64", L"Mobile Devices", wfx64);

	WCHAR msg[1024];
	swprintf_s(msg,
		L"Mobile Devices 0.9.20 установлен.\n\n%s\n\nПерезапустите Total Commander.",
		dest);
	MessageBoxW(NULL, msg, L"Mobile Devices", MB_ICONINFORMATION);
	return 0;
}
