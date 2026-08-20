# MTP Devices

Плагин файловой системы **Total Commander (WFX)** для **Android** и **iPhone**.  
В сетевом окружении TC: **MTP Devices**. Файлы: `mtpdevices.wfx` (32-bit) и `mtpdevices.wfx64` (64-bit).

Каталоги читаются **намного быстрее**, чем через стандартный MTP Windows / Проводник: пакетное чтение свойств, без повторного подключения на каждый переход. Копирование идёт в фоне (F5/F6), без типичных зависаний MTP.

На **iPhone** плагин умеет работать с файлами напрямую (Apple Devices из Microsoft Store, не MTP): фото, документы приложений с File Sharing, копирование и удаление — как в 3uTools / iMazing.

Это изменённая версия плагина Christian Ghisler *Windows Media Audio 2* (`wpdplug`). Это **не** оригинальный MediaAudio2. Лицензия: [BSD-3-Clause](LICENSE.txt).

---

## Функции (20)

1. Чтение каталогов **намного быстрее стандартного MTP** (пакетные свойства WPD, без паузы 20 секунд).
2. Копирование файлов **быстрее обычного MTP** в Проводнике: фон, отмена, сессия не рвётся на каждый каталог.
3. Полный доступ к **Android** по MTP: просмотр, копирование, перенос, удаление, переименование, папки.
4. Управление файлами на **iPhone / iPad** через Apple Devices (не MTP).
5. **Фото на iPhone** (DCIM / AFC).
6. **Applications** на iPhone — только приложения с доступом File Sharing / Файлы.
7. Копирование и удаление файлов документов этих приложений.
8. Папка **Panic Logs** на iPhone (crash-логи).
9. Карточка устройства: ОС, модель, производитель, батарея, протокол, свободно / ёмкость.
10. Android в списке по **реальной модели** (например realme C21-Y), не USB1 и не «стандартное MTP-устройство».
11. Накопитель показывается как **внутренний общий накопитель**, а не «USB-устройство MTP».
12. В корне только телефоны: без принтеров, сканеров, WIA, заглушек Apple MTP.
13. Разные значки Android и iPhone (в т.ч. эскизы).
14. Миниатюры файлов в режиме эскизов Total Commander.
15. Доп. колонки: тип, название, исполнитель, альбом, длительность, битрейт, свободно, ёмкость, серийный номер, батарея.
16. Фоновые F5/F6 и отмена передачи (Esc).
17. Команды: `quote refresh`, `quote eject`, `quote info`, `quote reconnect`.
18. Язык настроек: русский и английский; время файла — локальное или UTC (Alt+Enter).
19. Сборка **32-bit и 64-bit**; iPhone — только 64-bit TC + Apple Devices из Microsoft Store.
20. Безопасное извлечение устройства; сессия MTP сохраняется при возврате в корень плагина.

---

## Features (20)

1. **Directory listing much faster than standard MTP** (bulk WPD properties, no 20-second stalls).
2. **File copy faster than typical Explorer MTP**: background transfers, cancel, session kept across folders.
3. Full **Android** MTP access: browse, copy, move, delete, rename, create folders.
4. **Manage files on iPhone / iPad** via Apple Devices (not MTP).
5. **iPhone Photos** (DCIM / AFC).
6. **Applications** — only apps with iOS File Sharing / Files access.
7. Copy and delete that app’s document files.
8. **Panic Logs** folder on iPhone.
9. Device info: OS, model, manufacturer, battery, protocol, free / capacity.
10. Android listed by **real model** (e.g. realme C21-Y), not USB1 or “Standard MTP Device”.
11. Storage shown as **internal shared storage**, not “USB MTP Device”.
12. Plugin root shows phones only (no printers, scanners, WIA, Apple MTP stubs).
13. Separate Android and iPhone icons (including thumbnail view).
14. File thumbnails in Total Commander thumbnail mode.
15. Custom columns: type, title, artist, album, duration, bitrate, free, capacity, serial, battery.
16. Background F5/F6 copy and cancel (Esc).
17. Commands: `quote refresh`, `quote eject`, `quote info`, `quote reconnect`.
18. Settings in **Russian and English**; file times local or UTC (Alt+Enter).
19. **32-bit and 64-bit** builds; iPhone needs 64-bit TC and Apple Devices (Microsoft Store).
20. Safe eject; MTP session is kept when you return to the plugin root.

---

## Install

1. Download the zip from [Releases](../../releases).
2. Open it in Total Commander (or double-click from TC).
3. Confirm plugin installation.

Windows Vista or later with WPD. iPhone: Apple Devices (Microsoft Store) and 64-bit Total Commander.

## Build

Open `wpdplug.sln` in Visual Studio 2022. Release | Win32 → `.wfx`, Release | x64 → `.wfx64`.

## Credits

Original plugin © 2011–2018 Christian Ghisler, Ghisler Software GmbH.  
Modifications © 2026 MTP Devices contributors.
