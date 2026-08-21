// Folgender ifdef-Block ist die Standardmethode zum Erstellen von Makros, die das Exportieren 
// aus einer DLL vereinfachen. Alle Dateien in dieser DLL werden mit dem WPDPLUG_EXPORTS-Symbol
// kompiliert, das in der Befehlszeile definiert wurde. Das Symbol darf nicht f�r ein Projekt definiert werden,
// das diese DLL verwendet. Alle anderen Projekte, deren Quelldateien diese Datei beinhalten, erkennen 
// WPDPLUG_API-Funktionen als aus einer DLL importiert, w�hrend die DLL
// mit diesem Makro definierte Symbole als exportiert ansieht.
#ifdef WPDPLUG_EXPORTS
#define WPDPLUG_API __declspec(dllexport)
#else
#define WPDPLUG_API __declspec(dllimport)
#endif

#define PLUGIN_VERSION_MAJOR 0
#define PLUGIN_VERSION_MINOR 9
#define PLUGIN_VERSION_REV   24
#define PLUGIN_VERSION_STR   "0.9.24"
#define PLUGIN_VERSION_COMMA 0,9,24,0

#define PLUGIN_DISPLAY_NAME     "Mobile Devices"
#define PLUGIN_DISPLAY_NAME_W   L"Mobile Devices"
#define PLUGIN_FILE_STEM        "mobiledevices"
#define PLUGIN_INI_SECTION      L"MobileDevices"
#define PLUGIN_INI_SECTION_LEGACY L"MTPDevices"
#define PLUGIN_INI_SECTION_LEGACY2 L"MediaAudio2"
#define PLUGIN_WPD_CLIENT_NAME  L"Total Commander Mobile Devices"
#define PLUGIN_GITHUB_URL       "https://github.com/chuikoff/MobileDevices"
#define PLUGIN_GITHUB_URL_W     L"https://github.com/chuikoff/MobileDevices"

