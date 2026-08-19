// stdafx.h : Includedatei f�r Standardsystem-Includedateien
// oder h�ufig verwendete projektspezifische Includedateien,
// die nur in unregelm��igen Abst�nden ge�ndert werden.
//

#pragma once

// �ndern Sie folgende Definitionen f�r Plattformen, die �lter als die unten angegebenen sind.
// In MSDN finden Sie die neuesten Informationen �ber die entsprechenden Werte f�r die unterschiedlichen Plattformen.
#ifndef WINVER				// Windows 10 / 11 APIs (plugin v1.4)
#define WINVER 0x0A00
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef _WIN32_WINDOWS		// Lassen Sie die Verwendung spezifischer Features von Windows 98 oder sp�ter zu.
#define _WIN32_WINDOWS 0x0410 // �ndern Sie dies in den geeigneten Wert f�r Windows Me oder h�her.
#endif

#ifndef _WIN32_IE			// Lassen Sie die Verwendung spezifischer Features von IE 6.0 oder sp�ter zu.
#define _WIN32_IE 0x0600	// �ndern Sie dies in den geeigneten Wert f�r andere Versionen von IE.
#endif

#define WIN32_LEAN_AND_MEAN		// Selten verwendete Teile der Windows-Header nicht einbinden.
// Windows-Headerdateien:
#include <windows.h>



// TODO: Hier auf zus�tzliche Header, die das Programm erfordert, verweisen.

