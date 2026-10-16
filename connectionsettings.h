int UseLocalTime(WCHAR* SettingsName2);
int GetPluginUiLanguage(void);
/* [MobileDevices] LogLevel in mobiledevices.ini. 0 errors, 1 outcome, 2 probes. */
int PluginLogLevel(void);
BOOL ShowDevicePropertiesDialog(HINSTANCE hInst, HWND parent, WCHAR* RemoteName);
BOOL ShowPluginAboutDialog(HINSTANCE hInst, HWND parent);
BOOL ChangeConnectionSettingsW(HINSTANCE hInst, HWND parent, WCHAR* RemoteName);
