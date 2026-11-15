// ==WindhawkMod==
// @id refined-quick-settings-and-sound
// @name Refined Quick Settings and Sound
// @description More options for Quick Settings and direct access to sound settings
// @description:ru-RU Дополнительные параметры панели «Быстрые настройки» и открытие раздела «Звук» одним нажатием
// @description:es-ES Más opciones para la configuración rápida y acceso directo a los ajustes de sonido
// @description:de-DE Mehr Optionen für die Schnelleinstellungen und direkter Zugriff auf die Soundeinstellungen
// @description:pt-BR Mais opções para as configurações rápidas e acesso direto aos ajustes de som
// @description:zh-CN 快速设置面板的更多选项，快速打开声音设置
// @version 0.6.4
// @author HeyOkay, CTravkin
// @github https://github.com/HeyOkay
// @homepage https://github.com/HeyOkay/Refined-Quick-Settings-and-Sound
// @include explorer.exe
// @include ShellHost.exe
// @architecture x86-64
// @compilerOptions -DWIN32_LEAN_AND_MEAN -lcomctl32 -lole32 -loleaut32 -lruntimeobject -liphlpapi -lshlwapi -lshell32 -lpowrprof -luuid
// @license GPL-3.0-only
// ==/WindhawkMod==
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

- **Display brightness** defaults to **Auto**. It keeps the Windows slider when available or adds one for a monitor with a usable DDC/CI range. **Disable** hides the brightness row.
- The DDC/CI brightness row has a shortcut to Display settings and a Night light button. The latter uses the native quick action when available and opens Night light settings otherwise.
- Choose up to five **Additional shortcuts** to the right of **Settings**, including Notifications, Graphics, Advanced network settings, Power and Windows Update. Empty slots and duplicate shortcuts are skipped. The Display shortcut is also skipped when the brightness row already has a Display button.
- Set **Rows of tiles** to 1–4. Pages follow the number of available tiles. The override requires the ControlCenter build from Windows 26300.9974.

**Sound**

- The "Sound output" page is now simply "Sound".
- Change the output and input devices and the spatial sound provider from
  dropdown lists.
- Below them is a reworked Volume mixer for the apps. It starts collapsed the first time Sound opens in each ShellHost process and remembers its state on subsequent openings.
- Volume sliders show the new value when dragged or scrolled. The default wheel step is 2 percentage points.
- Press an app's icon to mute and restore it, or its volume number to set it to 100.
- **Hide from the volume mixer** takes app names or executable names, one per row. Hidden apps keep playing audio.

**Taskbar**

- **Quick access to Sound** opens Sound with a click on the taskbar volume icon.
- **Middle-click to mute** toggles sound from the volume icon and preserves the volume level. Off by default.
- **More sound settings** in the volume icon menu opens the classic Sound Control Panel.
- Device Manager is available in the network and battery icon menus.
- **Alternative icons** includes the animated volume and wired network icons from the original mod. Each location has its own switch. Switching one off uses the Windows icon there.

## Platforms

- Windows 11 24H2 or later (Quick Settings hosted by ShellHost), x86-64.
  The original mod was developed on build 26300, primarily at 125% scale.
- Other Windows builds and display scales need validation. The mod depends on
  internal shell XAML elements and symbols which can change with Windows updates.
- DDC/CI brightness requires a compatible monitor with DDC/CI enabled.
- Added labels are available in English, Russian, Spanish, German, Brazilian Portuguese and Simplified Chinese.

## Usage

Open Quick Settings with Win+A and Sound with Win+Ctrl+V. When **Quick access to Sound** is enabled, the taskbar volume icon opens Sound as well. Adjust the mod's options on its **Settings** tab in Windhawk.

Disable other mods or styles that change the same controls. If the panels do not update after enabling the mod, close and reopen them. If needed, restart ShellHost and check the Windhawk log.

## Updating from the Windhawk catalog version 0.1.154

If **Hidden applications** was filled in before the update, move those entries before adding any new rows under **Hide from the volume mixer**.

1. Open **Advanced** → **Mod settings**. In the **Load** menu, choose **Load formatted**.
2. Replace the `hiddenApps` string with one indexed setting per entry. For example, `"hiddenApps": "iCloud; Example.exe"` becomes `"hiddenApplications[0]": "iCloud"` and `"hiddenApplications[1]": "Example.exe"`. If rows are already filled in, use the next free indices. Remove `hiddenApps` in the same edit and keep the other settings.
3. Click **Save** and check that the entries appear under **Hide from the volume mixer** on the **Settings** tab.

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
- quickSoundAccess: true
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
- trayMiddleClickMute: false
  $name: "Middle-click to mute"
  $name:ru-RU: "Выключение звука средним кликом"
  $name:es-ES: "Silenciar con el botón central"
  $name:de-DE: "Ton per Mittelklick stummschalten"
  $name:pt-BR: "Silenciar com o botão do meio"
  $name:zh-CN: "单击鼠标中键静音"
  $description: "Middle-click the taskbar volume icon to mute or unmute"
  $description:ru-RU: "Средний клик по значку громкости на панели задач выключает или включает звук"
  $description:es-ES: "El botón central sobre el icono de volumen de la barra de tareas silencia o reactiva el sonido"
  $description:de-DE: "Ein Mittelklick auf das Lautstärkesymbol in der Taskleiste schaltet den Ton aus oder wieder ein"
  $description:pt-BR: "O botão do meio no ícone de volume da barra de tarefas ativa ou desativa o som"
  $description:zh-CN: "在任务栏的音量图标上单击鼠标中键可静音或取消静音"
- volumeWheelStep: 2
  $name: Volume wheel step
  $name:ru-RU: Шаг громкости при прокрутке
  $name:es-ES: Paso de volumen con la rueda del ratón
  $name:de-DE: Lautstärkeschritt mit dem Mausrad
  $name:pt-BR: Passo do volume com a roda do mouse
  $name:zh-CN: 鼠标滚轮音量步长
  $description: Percentage points per notch over a volume slider, from 1 to 100
  $description:ru-RU: Изменение громкости за деление колеса, от 1 до 100 процентных пунктов
  $description:es-ES: Puntos porcentuales por paso de rueda sobre un control de volumen, de 1 a 100
  $description:de-DE: Prozentpunkte pro Mausradschritt über einem Lautstärkeregler, von 1 bis 100
  $description:pt-BR: Pontos percentuais por passo da roda sobre um controle de volume, de 1 a 100
  $description:zh-CN: 在音量滑块上滚动一格改变的百分点，范围为 1 到 100
- quickActionRows: "2"
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
- quickSettings:
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
*/
// ==/WindhawkModSettings==
#include "host.h"
#include "tray.h"
namespace native {
inline bool explorer = false;
inline void ReadHiddenApps(std::vector<std::wstring> &hidden) {
  hidden.clear();
  // Windhawk exposes indexed values but no array length. Allow empty rows
  // between names, including rows left blank in the settings editor.
  for (int i = 0, blanks = 0; blanks < 32; ++i) {
    auto name = WindhawkUtils::StringSetting::make(L"hiddenApplications[%d]", i);
    if (!*name.get()) {
      ++blanks;
      continue;
    }
    blanks = 0;
    sound::AddHiddenApp(hidden, name.get());
  }
  if (!hidden.empty()) {
    if (!Wh_GetIntValue(L"usesHiddenAppRows", 0))
      Wh_SetIntValue(L"usesHiddenAppRows", 1);
  } else if (!Wh_GetIntValue(L"usesHiddenAppRows", 0)) {
    auto legacy = WindhawkUtils::StringSetting::make(L"hiddenApps");
    hidden = sound::ParseHidden(legacy.get());
  }
}
inline ModSettings ReadSettings() {
  ModSettings settings;
  ReadHiddenApps(settings.hidden);
  settings.volumeWheelStep = sound::VolumeWheelStep(Wh_GetIntSetting(L"volumeWheelStep"));
  settings.trayMiddleClickMute = Wh_GetIntSetting(L"trayMiddleClickMute") != 0;
  auto brightness = WindhawkUtils::StringSetting::make(L"brightnessMode");
  settings.brightnessEnabled = std::wstring_view(brightness.get()) != L"disabled";
  auto rows = WindhawkUtils::StringSetting::make(L"quickActionRows");
  std::wstring_view rowCount(rows.get());
  if (rowCount.size() == 1 && rowCount.front() >= L'1' && rowCount.front() <= L'4')
    settings.quickActionRows = rowCount.front() - L'0';
  using S = sound::QuickShortcut;
  auto first = WindhawkUtils::StringSetting::make(L"quickSettings.shortcut1");
  const auto oldIcon = [&](PCWSTR key) { return !*first.get() || Wh_GetIntSetting(key) != 0; };
  const auto icon = [](PCWSTR key, bool fallback) {
    auto value = WindhawkUtils::StringSetting::make(key);
    return *value.get() ? std::wstring_view(value.get()) == L"1" : fallback;
  };
  auto mixer = WindhawkUtils::StringSetting::make(L"alternativeIcons.mixerPlaces");
  auto volume = WindhawkUtils::StringSetting::make(L"alternativeIcons.volumePlaces");
  auto ethernet = WindhawkUtils::StringSetting::make(L"alternativeIcons.ethernetPlaces");
  auto gear = WindhawkUtils::StringSetting::make(L"alternativeIcons.gearPlaces");
  unsigned mixerPlaces = sound::IconPlaces(mixer.get(), 3,
      oldIcon(L"alternativeIcons.quickMixer") | (oldIcon(L"alternativeIcons.soundMixer") << 1));
  unsigned volumePlaces = sound::IconPlaces(volume.get(), 15,
      oldIcon(L"alternativeIcons.trayVolume") | (oldIcon(L"alternativeIcons.quickVolume") << 1) |
      (oldIcon(L"alternativeIcons.soundVolume") << 2) | (oldIcon(L"alternativeIcons.appUnmute") << 3));
  settings.altQuickMixer = icon(L"alternativeIcons.mixer.quick", mixerPlaces & 1);
  settings.altSoundMixer = icon(L"alternativeIcons.mixer.sound", mixerPlaces & 2);
  settings.altTrayVolume = icon(L"alternativeIcons.volume.taskbar", volumePlaces & 1);
  settings.altQuickVolume = icon(L"alternativeIcons.volume.quick", volumePlaces & 2);
  settings.altSoundVolume = icon(L"alternativeIcons.volume.sound", volumePlaces & 4);
  settings.altAppUnmute = icon(L"alternativeIcons.volume.apps", volumePlaces & 8);
  settings.altTrayEthernet = icon(
      L"alternativeIcons.ethernet.taskbar",
      sound::IconPlaces(ethernet.get(), 1, oldIcon(L"alternativeIcons.trayEthernet")));
  settings.altSoundGear = icon(L"alternativeIcons.gear.sound", sound::IconPlaces(gear.get(), 1, 1));
  if (*first.get()) {
    for (int i = 0; i < int(settings.quickShortcuts.size()); ++i) {
      auto slot = WindhawkUtils::StringSetting::make(L"quickSettings.shortcut%d", i + 1);
      settings.quickShortcuts[i] = sound::ParseQuickShortcut(slot.get());
    }
    settings.quickSoundAccess = Wh_GetIntSetting(L"quickSoundAccess") != 0;
  } else {
    settings.quickShortcuts = {Wh_GetIntSetting(L"showDisplaySettings") ? S::Display : S::None};
  }
  return settings;
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
