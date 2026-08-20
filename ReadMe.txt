MTP Devices plugin v0.9
=======================
Modified from Windows Media Audio 2 (wpdplug) by Christian Ghisler.

Copyright (C) 2011-2018 Christian Ghisler, Ghisler Software GmbH
Copyright (C) 2026 MTP Devices contributors

This is free software under the 3-clause BSD license. See LICENSE.txt.
This is an altered version and must not be presented as the original
MediaAudio2 plugin.

The plugin appears in Network Neighborhood as "MTP Devices".
Files: mtpdevices.wfx (32-bit) and mtpdevices.wfx64 (64-bit).

Directory listing is much faster than standard Windows MTP.
Copy runs in the background (F5/F6). iPhone files are managed via
Apple Devices (Microsoft Store), not MTP.

Requirements: Windows Vista or later with WPD.
iPhone: Apple Devices (Microsoft Store) and 64-bit Total Commander.

Installation: open this archive in Total Commander.

--------------------------------------------------------------------
Функции (20)
--------------------------------------------------------------------
1. Чтение каталогов намного быстрее стандартного MTP (пакетные
   свойства WPD, без паузы ~20 секунд).
2. Копирование быстрее обычного MTP в Проводнике: фон, отмена,
   сессия не рвётся на каждый каталог.
3. Полный доступ к Android по MTP: просмотр, копирование, перенос,
   удаление, переименование, папки.
4. Управление файлами на iPhone / iPad через Apple Devices (не MTP).
5. Фото на iPhone (DCIM / AFC).
6. Applications — только приложения с File Sharing / Файлы.
7. Копирование и удаление файлов документов этих приложений.
8. Папка Panic Logs на iPhone (crash-логи).
9. Карточка устройства: ОС, модель, производитель, батарея,
   протокол, свободно / ёмкость.
10. Android в списке по реальной модели, не USB1 и не
    «стандартное MTP-устройство».
11. Накопитель как «внутренний общий накопитель», не
    «USB-устройство MTP».
12. В корне только телефоны (без принтеров, сканеров, WIA,
    заглушек Apple MTP).
13. Разные значки Android и iPhone (в т.ч. эскизы).
14. Миниатюры файлов в режиме эскизов Total Commander.
15. Колонки: тип, название, исполнитель, альбом, длительность,
    битрейт, свободно, ёмкость, серийный номер, батарея.
16. Фоновые F5/F6 и отмена передачи (Esc).
17. quote refresh | quote eject | quote info | quote reconnect
18. Язык настроек: русский и английский; время — локальное или UTC
    (Alt+Enter).
19. 32-bit и 64-bit; iPhone — только 64-bit TC + Apple Devices.
20. Безопасное извлечение; сессия MTP сохраняется при возврате
    в корень плагина.

--------------------------------------------------------------------
Features (20)
--------------------------------------------------------------------
1. Directory listing much faster than standard MTP (bulk WPD
   properties, no ~20 second stalls).
2. File copy faster than typical Explorer MTP: background,
   cancel, session kept across folders.
3. Full Android MTP access: browse, copy, move, delete, rename,
   create folders.
4. Manage files on iPhone / iPad via Apple Devices (not MTP).
5. iPhone Photos (DCIM / AFC).
6. Applications — only apps with iOS File Sharing / Files access.
7. Copy and delete that app's document files.
8. Panic Logs folder on iPhone.
9. Device info: OS, model, manufacturer, battery, protocol,
   free / capacity.
10. Android listed by real model, not USB1 or "Standard MTP Device".
11. Storage shown as internal shared storage, not "USB MTP Device".
12. Plugin root shows phones only (no printers, scanners, WIA,
    Apple MTP stubs).
13. Separate Android and iPhone icons (including thumbnail view).
14. File thumbnails in Total Commander thumbnail mode.
15. Custom columns: type, title, artist, album, duration, bitrate,
    free, capacity, serial, battery.
16. Background F5/F6 copy and cancel (Esc).
17. quote refresh | quote eject | quote info | quote reconnect
18. Settings in Russian and English; file times local or UTC
    (Alt+Enter).
19. 32-bit and 64-bit builds; iPhone needs 64-bit TC and Apple
    Devices (Microsoft Store).
20. Safe eject; MTP session is kept when returning to the plugin
    root.

Timezone settings (Alt+Enter on the device) are stored under
[MTPDevices].
