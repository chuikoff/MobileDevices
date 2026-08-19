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

#define PLUGIN_VERSION_MAJOR 2
#define PLUGIN_VERSION_MINOR 0
#define PLUGIN_VERSION_REV   1
#define PLUGIN_VERSION_STR   "2.0.1"
#define PLUGIN_VERSION_COMMA 2,0,0,1

#define PLUGIN_DISPLAY_NAME     "MTP Devices"
#define PLUGIN_DISPLAY_NAME_W   L"MTP Devices"
#define PLUGIN_FILE_STEM        "mtpdevices"
#define PLUGIN_INI_SECTION      L"MTPDevices"
#define PLUGIN_INI_SECTION_LEGACY L"MediaAudio2"
#define PLUGIN_WPD_CLIENT_NAME  L"Total Commander MTP Devices"

