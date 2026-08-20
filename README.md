# MTP Devices

Плагин файловой системы **Total Commander (WFX)** для **Android** и **iPhone**.  
В сетевом окружении TC: **MTP Devices**. Файлы: `mtpdevices.wfx` (32-bit) и `mtpdevices.wfx64` (64-bit).

Каталоги читаются **намного быстрее**, чем через стандартный MTP Windows / Проводник: пакетное чтение свойств, без повторного подключения на каждый переход. Копирование идёт в фоне (F5/F6), без типичных зависаний MTP.

На **iPhone** плагин умеет работать с файлами напрямую: фото, документы приложений с File Sharing, копирование и удаление. Драйверы **не из iTunes** — нужен **Apple Devices из Microsoft Store**.

Протестировано на **Windows 11**.

Это изменённая версия плагина Christian Ghisler *Windows Media Audio 2* (`wpdplug`). Это **не** оригинальный MediaAudio2. Лицензия: [BSD-3-Clause](LICENSE.txt).

---

## Функции

1. Чтение каталогов **намного быстрее стандартного MTP** (пакетные свойства WPD, без паузы 20 секунд).
2. Копирование файлов **быстрее обычного MTP** в Проводнике: фон, отмена, сессия не рвётся на каждый каталог.
3. Полный доступ к **Android** по MTP: просмотр, копирование, перенос, удаление, переименование, папки.
4. Управление файлами на **iPhone / iPad**.
5. **Фото на iPhone** (DCIM / AFC).
6. **Applications** на iPhone — только приложения с доступом File Sharing / Файлы.
7. Копирование и удаление файлов документов этих приложений.
8. Папка **Panic Logs** на iPhone (crash-логи).
9. Карточка устройства: ОС, модель, производитель, батарея, протокол, свободно / ёмкость.
10. В корне только телефоны: без принтеров, сканеров, WIA, заглушек Apple MTP.
11. Миниатюры файлов в режиме эскизов Total Commander.
12. Фоновые F5/F6 и отмена передачи (Esc).
13. Протестировано на **Windows 11**.
14. Для iPhone **не нужны драйверы iTunes** — ставится **Apple Devices** из Microsoft Store.

---

## Features

1. **Directory listing much faster than standard MTP** (bulk WPD properties, no 20-second stalls).
2. **File copy faster than typical Explorer MTP**: background transfers, cancel, session kept across folders.
3. Full **Android** MTP access: browse, copy, move, delete, rename, create folders.
4. **Manage files on iPhone / iPad**.
5. **iPhone Photos** (DCIM / AFC).
6. **Applications** — only apps with iOS File Sharing / Files access.
7. Copy and delete that app’s document files.
8. **Panic Logs** folder on iPhone.
9. Device info: OS, model, manufacturer, battery, protocol, free / capacity.
10. Plugin root shows phones only (no printers, scanners, WIA, Apple MTP stubs).
11. File thumbnails in Total Commander thumbnail mode.
12. Background F5/F6 copy and cancel (Esc).
13. **Tested on Windows 11**.
14. iPhone does **not** need iTunes drivers — install **Apple Devices** from the Microsoft Store.

---

## Install

1. Download the zip from [Releases](../../releases).
2. Open it in Total Commander (or double-click from TC).
3. Confirm plugin installation.

Windows 11 (also Vista or later with WPD). iPhone: **Apple Devices from the Microsoft Store, not iTunes**, and 64-bit Total Commander.

## Build

Open `wpdplug.sln` in Visual Studio 2022. Release | Win32 → `.wfx`, Release | x64 → `.wfx64`.

## Credits

Original plugin © 2011–2018 Christian Ghisler, Ghisler Software GmbH.  
Modifications © 2026 MTP Devices contributors.
