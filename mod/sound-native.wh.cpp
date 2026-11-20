// ==WindhawkMod==
// @id refined-quick-settings-and-sound
// @name Refined Quick Settings and Sound
// @description More options for Quick Settings and direct access to sound settings
// @description:ru-RU Дополнительные параметры панели «Быстрые настройки» и открытие раздела «Звук» одним нажатием
// @description:es-ES Más opciones para la configuración rápida y acceso directo a los ajustes de sonido
// @description:de-DE Mehr Optionen für die Schnelleinstellungen und direkter Zugriff auf die Soundeinstellungen
// @description:pt-BR Mais opções para as configurações rápidas e acesso direto aos ajustes de som
// @description:zh-CN 快速设置面板的更多选项，快速打开声音设置
// @version 0.9.11
// @author HeyOkay, CTravkin
// @github https://github.com/HeyOkay
// @homepage https://github.com/HeyOkay/Refined-Quick-Settings-and-Sound
// @include explorer.exe
// @include ShellHost.exe
// @architecture x86-64
// @compilerOptions -DWIN32_LEAN_AND_MEAN -lcomctl32 -lole32 -loleaut32 -lruntimeobject -liphlpapi -lshlwapi -lshell32 -lpowrprof -luuid
// @license GPL-3.0-only
// ==/WindhawkMod==
// Build: 194
// Modified by Constantine Travkin on 2026-11-14.
// Based on HeyOkay's v0.1.154 (source blob 7229ac098ad0e8732905b831259d2aa99984bb31).
// Upstream: https://github.com/HeyOkay/Refined-Quick-Settings-and-Sound
// ==WindhawkModReadme==
/*
# Refined Quick Settings and Sound

![Quick Settings and Sound preview](https://raw.githubusercontent.com/HeyOkay/Refined-Quick-Settings-and-Sound/main/docs/assets/preview-lossless.webp)

This "vanilla+" Windhawk mod reworks the Windows 11 "Quick Settings" and
"Volume mixer" panels and makes them fit for daily use. It follows the
Windows 11 design language with thin sliders, even row spacing and a smooth
frame transition to the Sound page.

Everything runs inside the shell's own panels (ShellHost). The system Acrylic,
fonts, themes, lists, bindings and animations are kept, and the original layout
is restored when the mod is unloaded.

## Features

**Quick Settings**

- Choose up to five **Additional shortcuts** to the right of **Settings**, including Notifications, Graphics, Advanced network settings, Power and Windows Update. Empty slots and duplicate shortcuts are skipped. The Display shortcut is also skipped when the brightness row already has a Display button.
- **Display brightness** uses the Windows slider in the default **Auto** mode and falls back to DDC/CI on compatible monitors with DDC/CI enabled. **Disable** hides the brightness row.
- The mod places Night light and Display settings beside the brightness slider. If the native slider cannot be moved, Windows keeps its original row. Night light uses the native quick action when available and opens Night light settings otherwise.
- On devices with a battery, the battery button moves into the **Settings** row.

- **Rows of tiles** sets 1–4 rows per page with Windows pagination. If the mod cannot recognize the tile grid layout, Windows uses its default row count.
- **Tile shape** rounds the outer edges of Quick Settings tiles in **Pills** mode.

**Sound**

- The "Sound output" page is now simply "Sound".
- Change the output and input devices and the spatial sound provider from
  dropdown lists.
- Below them is a reworked Volume mixer for the apps. It starts collapsed the first time Sound opens in each ShellHost process and remembers its state on subsequent openings.
- **Microphone volume** defaults to **Off**. **Auto** adds a volume slider above system volume when the selected microphone has an adjustable level. The button on the left mutes and unmutes the microphone without changing its level.
- **Microphone settings shortcut** shows the volume percentage by default. **Device settings** replaces it with a button for the selected microphone's Windows settings.
- **Sound settings shortcut** puts the Windows Sound settings button beside system volume by default. **None (Percentage)** shows the volume percentage. **Device settings** opens the selected output device's settings. Both modes move the Sound settings button to the header.
- Turn off **Spatial sound** to hide the mode selector.
- **Sound field shape** rounds the closed output, input and spatial sound selectors in **Pills** mode.
- Volume sliders show the new value when dragged or scrolled. The default wheel step is 2 percentage points.
- Press a volume number to set that slider to 100. The app icon mutes or restores the app.
- **Hide from the volume mixer** takes app names or executable names, one per row. Hidden apps keep playing audio.

**Taskbar**

- **Quick access to Sound** opens Sound with a click on the taskbar volume icon.
- **Middle-click to mute** toggles sound from the volume icon and preserves the volume level. Off by default.
- **More sound settings** in the volume icon menu opens the classic Sound Control Panel.
- Device Manager is available in the network and battery icon menus.
- **Alternative icons** includes the animated volume and wired network icons from the original mod. Each location has its own switch. Switching one off uses the Windows icon there.

The interface is available in English, Russian, Spanish, German, Brazilian Portuguese and Simplified Chinese.

## Platforms

Requires Windows 11 24H2 or later (x64).

## Usage

Open Quick Settings with Win+A and Sound with Win+Ctrl+V. When **Quick access to Sound** is enabled, the taskbar volume icon opens Sound as well. Adjust the mod's options on its **Settings** tab in Windhawk.

Disable other mods or styles that change the same controls. If the panels do not update after enabling the mod, close and reopen them. If needed, restart ShellHost and check the Windhawk log.

## Licenses

[GPL-3.0-only](https://github.com/HeyOkay/Refined-Quick-Settings-and-Sound/blob/main/LICENSE). The UI-thread dispatch technique follows
[m417z's Notification Center Styler](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-notification-center-styler.wh.cpp)
(GPL-3.0). ControlCenter.dll discovery uses the COM produce-interface pattern
documented by [Per-monitor brightness](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/per-monitor-brightness.wh.cpp).
The hidden-applications parser is adapted from the local
Sound Output Direct and Taller mod (GPL-3.0-only).
The sliders-horizontal and ethernet-port icons are from
[Lucide](https://lucide.dev/) (ISC); the license notice is included below
in this source file.

## Links

- [Refined Quick Settings and Sound on GitHub](https://github.com/HeyOkay/Refined-Quick-Settings-and-Sound)
*/
// ==/WindhawkModReadme==
// ==WindhawkModSettings==
/*
- taskbar:
  - openSoundFromVolumeIcon: true
    $name: "Quick access to Sound"
    $name:ru-RU: "Быстрый доступ к настройкам звука"
    $name:es-ES: "Acceso rápido al sonido"
    $name:de-DE: "Schnellzugriff auf Sound"
    $name:pt-BR: "Acesso rápido ao som"
    $name:zh-CN: "快速访问声音设置"
    $description: "Opens the Sound panel from the taskbar volume icon"
    $description:ru-RU: "Панель «Звук» открывается по нажатию на значок громкости"
    $description:es-ES: "Abre el panel Sonido desde el icono de volumen de la barra de tareas"
    $description:de-DE: "Öffnet das Soundfenster über das Lautstärkesymbol in der Taskleiste"
    $description:pt-BR: "Abre o painel Som pelo ícone de volume da barra de tarefas"
    $description:zh-CN: "单击任务栏上的音量图标可直接打开“声音”面板"
  - middleClickMute: false
    $name: "Middle-click to mute"
    $name:ru-RU: "Выключение звука средним кликом"
    $name:es-ES: "Silenciar con el botón central"
    $name:de-DE: "Ton per Mittelklick stummschalten"
    $name:pt-BR: "Silenciar com o botão do meio"
    $name:zh-CN: "单击鼠标中键静音"
    $description: "Middle-click the taskbar volume icon to mute or unmute"
    $description:ru-RU: "Клик средней кнопкой мыши по значку громкости на панели задач выключает или включает звук"
    $description:es-ES: "El botón central sobre el icono de volumen de la barra de tareas silencia o reactiva el sonido"
    $description:de-DE: "Ein Mittelklick auf das Lautstärkesymbol in der Taskleiste schaltet den Ton aus oder wieder ein"
    $description:pt-BR: "O botão do meio no ícone de volume da barra de tarefas ativa ou desativa o som"
    $description:zh-CN: "在任务栏的音量图标上单击鼠标中键可静音或取消静音"
  $name: Taskbar
  $name:ru-RU: Панель задач
  $name:es-ES: Barra de tareas
  $name:de-DE: Taskleiste
  $name:pt-BR: Barra de tarefas
  $name:zh-CN: 任务栏
- sound:
  - volumeWheelStep: 2
    $name: Volume step
    $name:ru-RU: Шаг громкости
    $name:es-ES: Paso del volumen
    $name:de-DE: Lautstärkeschritt
    $name:pt-BR: Passo do volume
    $name:zh-CN: 音量步长
    $description: Volume change per mouse wheel notch, from 1 to 100 percentage points
    $description:ru-RU: Изменение громкости за деление прокрутки колеса, от 1 до 100 процентных пунктов
    $description:es-ES: Cambio de volumen por cada paso de la rueda del ratón, de 1 a 100 puntos porcentuales
    $description:de-DE: Lautstärkeänderung pro Mausradraste, von 1 bis 100 Prozentpunkten
    $description:pt-BR: Variação do volume por passo da roda do mouse, de 1 a 100 pontos percentuais
    $description:zh-CN: 鼠标滚轮每滚动一格时的音量变化，范围为 1 到 100 个百分点
  - microphoneVolumeMode: disabled
    $name: Microphone volume
    $name:ru-RU: Громкость микрофона
    $name:es-ES: Volumen del micrófono
    $name:de-DE: Mikrofonlautstärke
    $name:pt-BR: Volume do microfone
    $name:zh-CN: 麦克风音量
    $description: Shows a slider for the selected microphone above the system volume
    $description:ru-RU: Показывает ползунок громкости выбранного микрофона над общей громкостью
    $description:es-ES: Muestra un control para el micrófono seleccionado encima del volumen del sistema
    $description:de-DE: Zeigt über der Systemlautstärke einen Regler für das ausgewählte Mikrofon an
    $description:pt-BR: Mostra um controle para o microfone selecionado acima do volume do sistema
    $description:zh-CN: 在系统音量上方显示所选麦克风的音量滑块
    $options:
    - auto: Auto
    - disabled: Off
    $options:ru-RU:
    - auto: Авто
    - disabled: Выключено
    $options:es-ES:
    - auto: Automático
    - disabled: Desactivado
    $options:de-DE:
    - auto: Automatisch
    - disabled: Aus
    $options:pt-BR:
    - auto: Automático
    - disabled: Desativado
    $options:zh-CN:
    - auto: 自动
    - disabled: 关闭
  - microphoneVolumeRight: percentage
    $name: Microphone settings shortcut
    $name:ru-RU: Переход к настройкам микрофона
    $name:es-ES: Acceso a la configuración del micrófono
    $name:de-DE: Verknüpfung zu den Mikrofoneinstellungen
    $name:pt-BR: Atalho para as configurações do microfone
    $name:zh-CN: 麦克风设置快捷方式
    $description: Shows a button for the selected input device’s settings in place of the volume percentage
    $description:ru-RU: Отображение кнопки для перехода к настройкам выбранного устройства ввода вместо процента громкости
    $description:es-ES: Muestra un botón para abrir la configuración del dispositivo de entrada seleccionado en lugar del porcentaje de volumen
    $description:de-DE: Zeigt statt der Lautstärke in Prozent eine Schaltfläche für die Einstellungen des ausgewählten Eingabegeräts an
    $description:pt-BR: Exibe um botão para abrir as configurações do dispositivo de entrada selecionado no lugar da porcentagem do volume
    $description:zh-CN: 显示所选输入设备的设置快捷按钮，替代音量百分比
    $options:
    - percentage: None (Percentage)
    - deviceSettings: Device settings
    $options:ru-RU:
    - percentage: Нет (Проценты)
    - deviceSettings: Параметры устройства
    $options:es-ES:
    - percentage: Ninguno (Porcentaje)
    - deviceSettings: Configuración del dispositivo
    $options:de-DE:
    - percentage: Keine (Prozent)
    - deviceSettings: Geräteeinstellungen
    $options:pt-BR:
    - percentage: Nenhum (Porcentagem)
    - deviceSettings: Configurações do dispositivo
    $options:zh-CN:
    - percentage: 无（百分比）
    - deviceSettings: 设备设置
  - systemVolumeRight: soundSettings
    $name: Sound settings shortcut
    $name:ru-RU: Переход к настройкам звука
    $name:es-ES: Acceso a la configuración de sonido
    $name:de-DE: Verknüpfung zu den Soundeinstellungen
    $name:pt-BR: Atalho para as configurações de som
    $name:zh-CN: 声音设置快捷方式
    $description: Shows a shortcut to Sound settings, a shortcut to the selected output device’s settings, or the system volume percentage
    $description:ru-RU: Отображение перехода к настройкам звука, параметрам выбранного устройства вывода или процента общей громкости
    $description:es-ES: Muestra un acceso a la configuración de sonido, un acceso a la configuración del dispositivo de salida seleccionado o el porcentaje del volumen del sistema
    $description:de-DE: Zeigt eine Schaltfläche für die Soundeinstellungen, eine für die Einstellungen des ausgewählten Ausgabegeräts oder die Systemlautstärke in Prozent an
    $description:pt-BR: Exibe um atalho para as configurações de som, um atalho para as configurações do dispositivo de saída selecionado ou a porcentagem do volume do sistema
    $description:zh-CN: 显示声音设置快捷按钮、所选输出设备的设置快捷按钮或系统音量百分比
    $options:
    - soundSettings: Sound settings
    - percentage: None (Percentage)
    - deviceSettings: Device settings
    $options:ru-RU:
    - soundSettings: Параметры звука
    - percentage: Нет (Проценты)
    - deviceSettings: Параметры устройства
    $options:es-ES:
    - soundSettings: Configuración de sonido
    - percentage: Ninguno (Porcentaje)
    - deviceSettings: Configuración del dispositivo
    $options:de-DE:
    - soundSettings: Soundeinstellungen
    - percentage: Keine (Prozent)
    - deviceSettings: Geräteeinstellungen
    $options:pt-BR:
    - soundSettings: Configurações de som
    - percentage: Nenhum (Porcentagem)
    - deviceSettings: Configurações do dispositivo
    $options:zh-CN:
    - soundSettings: 声音设置
    - percentage: 无（百分比）
    - deviceSettings: 设备设置
  - spatialSoundMenu: enabled
    $name: Spatial sound
    $name:ru-RU: Пространственный звук
    $name:es-ES: Sonido espacial
    $name:de-DE: Raumklang
    $name:pt-BR: Som espacial
    $name:zh-CN: 空间音效
    $description: Menu for selecting a spatial audio mode
    $description:ru-RU: Меню выбора режима пространственного аудио
    $description:es-ES: Menú para elegir un modo de audio espacial
    $description:de-DE: Menü zur Auswahl des Raumklangmodus
    $description:pt-BR: Menu para escolher um modo de áudio espacial
    $description:zh-CN: 用于选择空间音频模式的菜单
    $options:
    - enabled: On
    - disabled: Off
    $options:ru-RU:
    - enabled: Включено
    - disabled: Выключено
    $options:es-ES:
    - enabled: Activado
    - disabled: Desactivado
    $options:de-DE:
    - enabled: Ein
    - disabled: Aus
    $options:pt-BR:
    - enabled: Ativado
    - disabled: Desativado
    $options:zh-CN:
    - enabled: 开启
    - disabled: 关闭
  - selectorShape: default
    $name: Sound field shape
    $name:ru-RU: Форма полей устройств
    $name:es-ES: Forma de los selectores
    $name:de-DE: Form der Soundfelder
    $name:pt-BR: Forma dos seletores
    $name:zh-CN: 声音选框形状
    $description: Pills mode rounds the closed selectors for output, input and spatial sound on the Sound page
    $description:ru-RU: В режиме «Капсулы» скругляются закрытые поля выбора вывода, ввода и пространственного звука
    $description:es-ES: En el modo Cápsulas se redondean los selectores cerrados de salida, entrada y sonido espacial en el panel Sonido
    $description:de-DE: Die Kapselform rundet die geschlossenen Auswahlfelder für Ausgabe, Eingabe und Raumklang auf der Soundseite ab
    $description:pt-BR: No modo Cápsulas, os seletores fechados de saída, entrada e som espacial ficam arredondados no painel Som
    $description:zh-CN: 选择胶囊形后，声音面板中的输出、输入和空间音效选择框在关闭时呈胶囊形
    $options:
    - default: Default
    - pills: Pills
    $options:ru-RU:
    - default: По умолчанию
    - pills: Капсулы
    $options:es-ES:
    - default: Predeterminada
    - pills: Cápsulas
    $options:de-DE:
    - default: Standard
    - pills: Kapselform
    $options:pt-BR:
    - default: Padrão
    - pills: Cápsulas
    $options:zh-CN:
    - default: 默认
    - pills: 胶囊形
  - hiddenApplications: [""]
    $name: "Hide from the volume mixer"
    $name:ru-RU: "Скрыть в микшере громкости"
    $name:es-ES: "Ocultar en el mezclador de volumen"
    $name:de-DE: "Im Lautstärkemixer ausblenden"
    $name:pt-BR: "Ocultar no mixer de volume"
    $name:zh-CN: "在音量合成器中隐藏"
    $description: "One app per row, by name or executable, for example iCloud or iCloudHome.exe"
    $description:ru-RU: "По одному названию или имени файла на строку, например iCloud или iCloudHome.exe"
    $description:es-ES: "Una aplicación por fila, por nombre o archivo ejecutable, por ejemplo, iCloud o iCloudHome.exe"
    $description:de-DE: "Eine App pro Zeile anhand ihres Namens oder ihrer ausführbaren Datei, etwa iCloud oder iCloudHome.exe"
    $description:pt-BR: "Um aplicativo por linha, pelo nome ou executável, como iCloud ou iCloudHome.exe"
    $description:zh-CN: "每行填写一个应用名称或可执行文件名，例如 iCloud 或 iCloudHome.exe"
  $name: Sound
  $name:ru-RU: Звук
  $name:es-ES: Sonido
  $name:de-DE: Sound
  $name:pt-BR: Som
  $name:zh-CN: 声音
- quickSettings:
  - brightnessMode: auto
    $name: Display brightness
    $name:ru-RU: Яркость дисплея
    $name:es-ES: Brillo de la pantalla
    $name:de-DE: Anzeigehelligkeit
    $name:pt-BR: Brilho da tela
    $name:zh-CN: 显示亮度
    $description: In Auto mode, the slider appears when brightness can be adjusted
    $description:ru-RU: В режиме «Авто» ползунок виден, если яркость можно регулировать
    $description:es-ES: En modo automático, el control se muestra si el brillo se puede ajustar
    $description:de-DE: Im Modus Automatisch erscheint der Regler, wenn die Helligkeit einstellbar ist
    $description:pt-BR: No modo automático, o controle aparece quando é possível ajustar o brilho
    $description:zh-CN: 自动模式下，可调节亮度时显示滑块
    $options:
    - auto: Auto
    - disabled: Disable
    $options:ru-RU:
    - auto: Авто
    - disabled: Отключить
    $options:es-ES:
    - auto: Automático
    - disabled: Desactivar
    $options:de-DE:
    - auto: Automatisch
    - disabled: Deaktivieren
    $options:pt-BR:
    - auto: Automático
    - disabled: Desativar
    $options:zh-CN:
    - auto: 自动
    - disabled: 禁用
  - tileRows: "2"
    $name: Rows of tiles
    $name:ru-RU: Строки плиток
    $name:es-ES: Filas de mosaicos
    $name:de-DE: Kachelzeilen in den Schnelleinstellungen
    $name:pt-BR: Linhas de blocos nas configurações rápidas
    $name:zh-CN: 快速设置磁贴行数
    $description: How many rows fit on each Quick Settings page
    $description:ru-RU: Сколько строк помещается на одной странице быстрых настроек
    $description:es-ES: Número de filas de mosaicos por página de Configuración rápida
    $description:de-DE: Anzahl der Kachelzeilen pro Seite der Schnelleinstellungen
    $description:pt-BR: Número de linhas por página das Configurações rápidas
    $description:zh-CN: 每页快速设置显示的磁贴行数
    $options:
    - "1": "1"
    - "2": "2"
    - "3": "3"
    - "4": "4"
  - tileShape: default
    $name: Tile shape
    $name:ru-RU: Форма плиток
    $name:es-ES: Forma de los mosaicos
    $name:de-DE: Kachelform
    $name:pt-BR: Forma dos blocos
    $name:zh-CN: 磁贴形状
    $description: In Pills mode, Quick Settings tiles have rounded outer edges
    $description:ru-RU: В режиме «Капсулы» скругляются внешние края плиток быстрых настроек
    $description:es-ES: En el modo Cápsulas se redondean los bordes exteriores de los mosaicos de Configuración rápida
    $description:de-DE: Die Kapselform rundet die Außenkanten der Schnelleinstellungskacheln ab
    $description:pt-BR: No modo Cápsulas, as bordas externas dos blocos das Configurações rápidas ficam arredondadas
    $description:zh-CN: 选择胶囊形后，快速设置磁贴的外侧边缘会变圆
    $options:
    - default: Default
    - pills: Pills
    $options:ru-RU:
    - default: По умолчанию
    - pills: Капсулы
    $options:es-ES:
    - default: Predeterminada
    - pills: Cápsulas
    $options:de-DE:
    - default: Standard
    - pills: Kapselform
    $options:pt-BR:
    - default: Padrão
    - pills: Cápsulas
    $options:zh-CN:
    - default: 默认
    - pills: 胶囊形
  - additionalShortcuts:
    - shortcut1: display
      $name: "Shortcut 1"
      $name:ru-RU: "Переход 1"
      $name:es-ES: "Acceso directo 1"
      $name:de-DE: "Verknüpfung 1"
      $name:pt-BR: "Atalho 1"
      $name:zh-CN: "快捷方式 1"
      $options:
      - none: "None"
      - display: "Display settings"
      - graphics: "Graphics"
      - notifications: "Notifications"
      - power: "Power"
      - bluetooth: "Bluetooth & devices"
      - network: "Network & internet"
      - networkSettings: "Advanced network settings"
      - apps: "Installed apps"
      - update: "Windows Update"
      $options:ru-RU:
      - none: "Нет"
      - display: "Параметры дисплея"
      - graphics: "Графика"
      - notifications: "Уведомления"
      - power: "Питание"
      - bluetooth: "Bluetooth и устройства"
      - network: "Сеть и Интернет"
      - networkSettings: "Сетевые параметры"
      - apps: "Установленные приложения"
      - update: "Центр обновления"
      $options:es-ES:
      - none: "Ninguno"
      - display: "Configuración de pantalla"
      - graphics: "Gráficos"
      - notifications: "Notificaciones"
      - power: "Energía"
      - bluetooth: "Bluetooth y dispositivos"
      - network: "Red e Internet"
      - networkSettings: "Configuración de red avanzada"
      - apps: "Aplicaciones instaladas"
      - update: "Windows Update"
      $options:de-DE:
      - none: "Keine"
      - display: "Anzeigeeinstellungen"
      - graphics: "Grafik"
      - notifications: "Benachrichtigungen"
      - power: "Strom"
      - bluetooth: "Bluetooth und Geräte"
      - network: "Netzwerk und Internet"
      - networkSettings: "Erweiterte Netzwerkeinstellungen"
      - apps: "Installierte Apps"
      - update: "Windows Update"
      $options:pt-BR:
      - none: "Nenhum"
      - display: "Configurações de exibição"
      - graphics: "Elementos gráficos"
      - notifications: "Notificações"
      - power: "Energia"
      - bluetooth: "Bluetooth e dispositivos"
      - network: "Rede e Internet"
      - networkSettings: "Configurações avançadas de rede"
      - apps: "Aplicativos instalados"
      - update: "Windows Update"
      $options:zh-CN:
      - none: "无"
      - display: "显示设置"
      - graphics: "图形"
      - notifications: "通知"
      - power: "电源"
      - bluetooth: "蓝牙和设备"
      - network: "网络和 Internet"
      - networkSettings: "高级网络设置"
      - apps: "已安装的应用"
      - update: "Windows 更新"
    - shortcut2: none
      $name: "Shortcut 2"
      $name:ru-RU: "Переход 2"
      $name:es-ES: "Acceso directo 2"
      $name:de-DE: "Verknüpfung 2"
      $name:pt-BR: "Atalho 2"
      $name:zh-CN: "快捷方式 2"
      $options:
      - none: "None"
      - display: "Display settings"
      - graphics: "Graphics"
      - notifications: "Notifications"
      - power: "Power"
      - bluetooth: "Bluetooth & devices"
      - network: "Network & internet"
      - networkSettings: "Advanced network settings"
      - apps: "Installed apps"
      - update: "Windows Update"
      $options:ru-RU:
      - none: "Нет"
      - display: "Параметры дисплея"
      - graphics: "Графика"
      - notifications: "Уведомления"
      - power: "Питание"
      - bluetooth: "Bluetooth и устройства"
      - network: "Сеть и Интернет"
      - networkSettings: "Сетевые параметры"
      - apps: "Установленные приложения"
      - update: "Центр обновления"
      $options:es-ES:
      - none: "Ninguno"
      - display: "Configuración de pantalla"
      - graphics: "Gráficos"
      - notifications: "Notificaciones"
      - power: "Energía"
      - bluetooth: "Bluetooth y dispositivos"
      - network: "Red e Internet"
      - networkSettings: "Configuración de red avanzada"
      - apps: "Aplicaciones instaladas"
      - update: "Windows Update"
      $options:de-DE:
      - none: "Keine"
      - display: "Anzeigeeinstellungen"
      - graphics: "Grafik"
      - notifications: "Benachrichtigungen"
      - power: "Strom"
      - bluetooth: "Bluetooth und Geräte"
      - network: "Netzwerk und Internet"
      - networkSettings: "Erweiterte Netzwerkeinstellungen"
      - apps: "Installierte Apps"
      - update: "Windows Update"
      $options:pt-BR:
      - none: "Nenhum"
      - display: "Configurações de exibição"
      - graphics: "Elementos gráficos"
      - notifications: "Notificações"
      - power: "Energia"
      - bluetooth: "Bluetooth e dispositivos"
      - network: "Rede e Internet"
      - networkSettings: "Configurações avançadas de rede"
      - apps: "Aplicativos instalados"
      - update: "Windows Update"
      $options:zh-CN:
      - none: "无"
      - display: "显示设置"
      - graphics: "图形"
      - notifications: "通知"
      - power: "电源"
      - bluetooth: "蓝牙和设备"
      - network: "网络和 Internet"
      - networkSettings: "高级网络设置"
      - apps: "已安装的应用"
      - update: "Windows 更新"
    - shortcut3: none
      $name: "Shortcut 3"
      $name:ru-RU: "Переход 3"
      $name:es-ES: "Acceso directo 3"
      $name:de-DE: "Verknüpfung 3"
      $name:pt-BR: "Atalho 3"
      $name:zh-CN: "快捷方式 3"
      $options:
      - none: "None"
      - display: "Display settings"
      - graphics: "Graphics"
      - notifications: "Notifications"
      - power: "Power"
      - bluetooth: "Bluetooth & devices"
      - network: "Network & internet"
      - networkSettings: "Advanced network settings"
      - apps: "Installed apps"
      - update: "Windows Update"
      $options:ru-RU:
      - none: "Нет"
      - display: "Параметры дисплея"
      - graphics: "Графика"
      - notifications: "Уведомления"
      - power: "Питание"
      - bluetooth: "Bluetooth и устройства"
      - network: "Сеть и Интернет"
      - networkSettings: "Сетевые параметры"
      - apps: "Установленные приложения"
      - update: "Центр обновления"
      $options:es-ES:
      - none: "Ninguno"
      - display: "Configuración de pantalla"
      - graphics: "Gráficos"
      - notifications: "Notificaciones"
      - power: "Energía"
      - bluetooth: "Bluetooth y dispositivos"
      - network: "Red e Internet"
      - networkSettings: "Configuración de red avanzada"
      - apps: "Aplicaciones instaladas"
      - update: "Windows Update"
      $options:de-DE:
      - none: "Keine"
      - display: "Anzeigeeinstellungen"
      - graphics: "Grafik"
      - notifications: "Benachrichtigungen"
      - power: "Strom"
      - bluetooth: "Bluetooth und Geräte"
      - network: "Netzwerk und Internet"
      - networkSettings: "Erweiterte Netzwerkeinstellungen"
      - apps: "Installierte Apps"
      - update: "Windows Update"
      $options:pt-BR:
      - none: "Nenhum"
      - display: "Configurações de exibição"
      - graphics: "Elementos gráficos"
      - notifications: "Notificações"
      - power: "Energia"
      - bluetooth: "Bluetooth e dispositivos"
      - network: "Rede e Internet"
      - networkSettings: "Configurações avançadas de rede"
      - apps: "Aplicativos instalados"
      - update: "Windows Update"
      $options:zh-CN:
      - none: "无"
      - display: "显示设置"
      - graphics: "图形"
      - notifications: "通知"
      - power: "电源"
      - bluetooth: "蓝牙和设备"
      - network: "网络和 Internet"
      - networkSettings: "高级网络设置"
      - apps: "已安装的应用"
      - update: "Windows 更新"
    - shortcut4: none
      $name: "Shortcut 4"
      $name:ru-RU: "Переход 4"
      $name:es-ES: "Acceso directo 4"
      $name:de-DE: "Verknüpfung 4"
      $name:pt-BR: "Atalho 4"
      $name:zh-CN: "快捷方式 4"
      $options:
      - none: "None"
      - display: "Display settings"
      - graphics: "Graphics"
      - notifications: "Notifications"
      - power: "Power"
      - bluetooth: "Bluetooth & devices"
      - network: "Network & internet"
      - networkSettings: "Advanced network settings"
      - apps: "Installed apps"
      - update: "Windows Update"
      $options:ru-RU:
      - none: "Нет"
      - display: "Параметры дисплея"
      - graphics: "Графика"
      - notifications: "Уведомления"
      - power: "Питание"
      - bluetooth: "Bluetooth и устройства"
      - network: "Сеть и Интернет"
      - networkSettings: "Сетевые параметры"
      - apps: "Установленные приложения"
      - update: "Центр обновления"
      $options:es-ES:
      - none: "Ninguno"
      - display: "Configuración de pantalla"
      - graphics: "Gráficos"
      - notifications: "Notificaciones"
      - power: "Energía"
      - bluetooth: "Bluetooth y dispositivos"
      - network: "Red e Internet"
      - networkSettings: "Configuración de red avanzada"
      - apps: "Aplicaciones instaladas"
      - update: "Windows Update"
      $options:de-DE:
      - none: "Keine"
      - display: "Anzeigeeinstellungen"
      - graphics: "Grafik"
      - notifications: "Benachrichtigungen"
      - power: "Strom"
      - bluetooth: "Bluetooth und Geräte"
      - network: "Netzwerk und Internet"
      - networkSettings: "Erweiterte Netzwerkeinstellungen"
      - apps: "Installierte Apps"
      - update: "Windows Update"
      $options:pt-BR:
      - none: "Nenhum"
      - display: "Configurações de exibição"
      - graphics: "Elementos gráficos"
      - notifications: "Notificações"
      - power: "Energia"
      - bluetooth: "Bluetooth e dispositivos"
      - network: "Rede e Internet"
      - networkSettings: "Configurações avançadas de rede"
      - apps: "Aplicativos instalados"
      - update: "Windows Update"
      $options:zh-CN:
      - none: "无"
      - display: "显示设置"
      - graphics: "图形"
      - notifications: "通知"
      - power: "电源"
      - bluetooth: "蓝牙和设备"
      - network: "网络和 Internet"
      - networkSettings: "高级网络设置"
      - apps: "已安装的应用"
      - update: "Windows 更新"
    - shortcut5: none
      $name: "Shortcut 5"
      $name:ru-RU: "Переход 5"
      $name:es-ES: "Acceso directo 5"
      $name:de-DE: "Verknüpfung 5"
      $name:pt-BR: "Atalho 5"
      $name:zh-CN: "快捷方式 5"
      $options:
      - none: "None"
      - display: "Display settings"
      - graphics: "Graphics"
      - notifications: "Notifications"
      - power: "Power"
      - bluetooth: "Bluetooth & devices"
      - network: "Network & internet"
      - networkSettings: "Advanced network settings"
      - apps: "Installed apps"
      - update: "Windows Update"
      $options:ru-RU:
      - none: "Нет"
      - display: "Параметры дисплея"
      - graphics: "Графика"
      - notifications: "Уведомления"
      - power: "Питание"
      - bluetooth: "Bluetooth и устройства"
      - network: "Сеть и Интернет"
      - networkSettings: "Сетевые параметры"
      - apps: "Установленные приложения"
      - update: "Центр обновления"
      $options:es-ES:
      - none: "Ninguno"
      - display: "Configuración de pantalla"
      - graphics: "Gráficos"
      - notifications: "Notificaciones"
      - power: "Energía"
      - bluetooth: "Bluetooth y dispositivos"
      - network: "Red e Internet"
      - networkSettings: "Configuración de red avanzada"
      - apps: "Aplicaciones instaladas"
      - update: "Windows Update"
      $options:de-DE:
      - none: "Keine"
      - display: "Anzeigeeinstellungen"
      - graphics: "Grafik"
      - notifications: "Benachrichtigungen"
      - power: "Strom"
      - bluetooth: "Bluetooth und Geräte"
      - network: "Netzwerk und Internet"
      - networkSettings: "Erweiterte Netzwerkeinstellungen"
      - apps: "Installierte Apps"
      - update: "Windows Update"
      $options:pt-BR:
      - none: "Nenhum"
      - display: "Configurações de exibição"
      - graphics: "Elementos gráficos"
      - notifications: "Notificações"
      - power: "Energia"
      - bluetooth: "Bluetooth e dispositivos"
      - network: "Rede e Internet"
      - networkSettings: "Configurações avançadas de rede"
      - apps: "Aplicativos instalados"
      - update: "Windows Update"
      $options:zh-CN:
      - none: "无"
      - display: "显示设置"
      - graphics: "图形"
      - notifications: "通知"
      - power: "电源"
      - bluetooth: "蓝牙和设备"
      - network: "网络和 Internet"
      - networkSettings: "高级网络设置"
      - apps: "已安装的应用"
      - update: "Windows 更新"
    $name: "Additional shortcuts"
    $name:ru-RU: "Дополнительные переходы"
    $name:es-ES: "Accesos directos adicionales"
    $name:de-DE: "Weitere Verknüpfungen"
    $name:pt-BR: "Atalhos adicionais"
    $name:zh-CN: "其他快捷方式"
    $description: Shortcuts to the right of Settings, in the selected order, skipping empty slots and duplicates
    $description:ru-RU: Порядок кнопок справа от «Настроек», пустые позиции и повторы пропускаются
    $description:es-ES: Accesos a la derecha de Configuración, en el orden elegido y sin huecos ni duplicados
    $description:de-DE: Verknüpfungen rechts neben Einstellungen in der gewählten Reihenfolge, ohne Lücken oder Duplikate
    $description:pt-BR: Atalhos à direita de Configurações, na ordem escolhida, sem lacunas nem duplicatas
    $description:zh-CN: 快捷方式按所选顺序显示在“设置”右侧，跳过空位及重复项
  $name: Quick Settings
  $name:ru-RU: Быстрые настройки
  $name:es-ES: Configuración rápida
  $name:de-DE: Schnelleinstellungen
  $name:pt-BR: Configurações rápidas
  $name:zh-CN: 快速设置
- alternativeIcons:
  - mixer:
    - quick: true
      $name: "Quick Settings"
      $name:ru-RU: "Быстрые настройки"
      $name:es-ES: "Configuración rápida"
      $name:de-DE: "Schnelleinstellungen"
      $name:pt-BR: "Configurações rápidas"
      $name:zh-CN: "快速设置"
    - sound: true
      $name: "Sound panel"
      $name:ru-RU: "Панель «Звук»"
      $name:es-ES: "Panel Sonido"
      $name:de-DE: "Soundbereich"
      $name:pt-BR: "Painel Som"
      $name:zh-CN: "“声音”面板"
    $name: "Volume mixer"
    $name:ru-RU: "Микшер громкости"
    $name:es-ES: "Mezclador de volumen"
    $name:de-DE: "Lautstärkemixer"
    $name:pt-BR: "Mixer de volume"
    $name:zh-CN: "音量合成器"
  - volume:
    - taskbar: true
      $name: "Taskbar"
      $name:ru-RU: "Панель задач"
      $name:es-ES: "Barra de tareas"
      $name:de-DE: "Taskleiste"
      $name:pt-BR: "Barra de tarefas"
      $name:zh-CN: "任务栏"
    - quick: true
      $name: "Quick Settings slider"
      $name:ru-RU: "Ползунок в быстрых настройках"
      $name:es-ES: "Control de Configuración rápida"
      $name:de-DE: "Regler in den Schnelleinstellungen"
      $name:pt-BR: "Controle das Configurações rápidas"
      $name:zh-CN: "快速设置音量滑块"
    - sound: true
      $name: "Sound panel slider"
      $name:ru-RU: "Ползунок на панели «Звук»"
      $name:es-ES: "Control del panel Sonido"
      $name:de-DE: "Regler im Soundbereich"
      $name:pt-BR: "Controle do painel Som"
      $name:zh-CN: "“声音”面板的音量滑块"
    - apps: true
      $name: "Muted apps in the mixer"
      $name:ru-RU: "Приложения без звука в микшере"
      $name:es-ES: "Aplicaciones silenciadas en el mezclador"
      $name:de-DE: "Stummgeschaltete Apps im Mixer"
      $name:pt-BR: "Aplicativos silenciados no mixer"
      $name:zh-CN: "合成器中已静音的应用"
    $name: "Speaker"
    $name:ru-RU: "Динамик"
    $name:es-ES: "Altavoz"
    $name:de-DE: "Lautsprecher"
    $name:pt-BR: "Alto-falante"
    $name:zh-CN: "扬声器"
  - ethernet:
    - taskbar: true
      $name: "Taskbar"
      $name:ru-RU: "Панель задач"
      $name:es-ES: "Barra de tareas"
      $name:de-DE: "Taskleiste"
      $name:pt-BR: "Barra de tarefas"
      $name:zh-CN: "任务栏"
    $name: "Ethernet"
    $name:ru-RU: "Ethernet"
    $name:es-ES: "Ethernet"
    $name:de-DE: "Ethernet"
    $name:pt-BR: "Ethernet"
    $name:zh-CN: "以太网"
  - gear:
    - sound: true
      $name: "Sound panel"
      $name:ru-RU: "Панель «Звук»"
      $name:es-ES: "Panel Sonido"
      $name:de-DE: "Soundbereich"
      $name:pt-BR: "Painel Som"
      $name:zh-CN: "“声音”面板"
    $name: "Settings gear"
    $name:ru-RU: "Шестеренка настроек"
    $name:es-ES: "Engranaje de configuración"
    $name:de-DE: "Einstellungszahnrad"
    $name:pt-BR: "Engrenagem de configurações"
    $name:zh-CN: "设置齿轮"
  $name: "Alternative icons"
  $name:ru-RU: "Альтернативные иконки"
  $name:es-ES: "Iconos alternativos"
  $name:de-DE: "Alternative Symbole"
  $name:pt-BR: "Ícones alternativos"
  $name:zh-CN: "替代图标"
  $description: "Turn off a switch to use the Windows icon in that location"
  $description:ru-RU: "Выключенный переключатель возвращает иконку Windows"
  $description:es-ES: "Al desactivar un interruptor, se usa el icono de Windows en esa ubicación"
  $description:de-DE: "Ein ausgeschalteter Schalter verwendet dort das Windows-Symbol"
  $description:pt-BR: "Ao desativar uma opção, o ícone do Windows aparece nesse local"
  $description:zh-CN: "关闭开关后，相应位置将使用 Windows 图标"
*/
// ==/WindhawkModSettings==
#include "host.h"
#include "settings.h"
#include "tray.h"
namespace native {
inline bool explorer = false;
inline ModSettings ReadSettings() {
  auto readString = [](PCWSTR key) {
    auto value = WindhawkUtils::StringSetting::make(key);
    return std::wstring(value.get());
  };
  return ReadPanelSettings(Wh_GetIntSetting, readString);
}
} // namespace native
BOOL Wh_ModInit() {
  using namespace native;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&Wh_ModInit), &module);
  WCHAR path[32768]{};
  GetModuleFileNameW(nullptr, path, 32768);
  explorer = !_wcsicmp(PathFindFileNameW(path), L"explorer.exe");
  stopping = false;
  modSettings = ReadSettings();
  if (explorer) {
    if (!tray::Initialize(module)) {
      tray::Shutdown();
      return FALSE;
    }
    return TRUE;
  }
  return InitializeHost();
}
void Wh_ModAfterInit() {
  if (native::explorer) {
    native::tray::AttachExisting();
  } else
    native::AfterHostInit();
}
void Wh_ModUninit() {
  native::stopping = true;
  if (native::explorer) {
    native::tray::Shutdown();
  } else
    native::StopHost();
}
BOOL Wh_ModSettingsChanged(BOOL *reload) {
  using namespace native;
  *reload = FALSE;
  auto next = ReadSettings();
  {
    std::lock_guard lock(settingsMutex);
    if (next == modSettings)
      return TRUE;
    modSettings = std::move(next);
  }
  if (explorer)
    tray::RefreshSettings();
  else
    RefreshHostSettings();
  return TRUE;
}
