# Third-party notices

The mod is distributed under GPL-3.0-only. See [LICENSE](../LICENSE).

- **Windows 11 Notification Center Styler**, by **m417z**: the UI-thread dispatch approach and the historical XAML Diagnostics connection/lifecycle approach. Upstream identifies its code as GNU GPL v3.0. Source: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-notification-center-styler.wh.cpp . The old Diagnostics connection has been removed from the 0.1.149 candidate; UI-thread dispatch retains its provenance.
- **Sound Output Direct and Taller**, local mod, ID `sound-output-direct-tall`: only the hidden-applications parser in model.h was adapted. Its local source declares GPL-3.0-only and credits m417z for its styling engine. No public upstream URL was present in that source; none has been invented here.
- **Lucide Icons and Contributors**: `sliders-horizontal` and `ethernet-port`, ISC. Full notice: ../mod/LUCIDE-LICENSE.txt; package.py also embeds that notice in the distributed single-file source.

These notices describe the provenance recorded in the reviewed sources, not a line-by-line verification of all historical authorship.

The ControlCenter.dll COM produce-interface discovery pattern was consulted in **Per-monitor brightness** (bardelyne, GPL-3.0) and **Windows 11 Control Center Buttons** (Salyts, MIT; consulted as a reference). See https://github.com/ramensoftware/windhawk-mods/blob/main/mods/per-monitor-brightness.wh.cpp and https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-control-center-buttons.wh.cpp . The former XAML Diagnostics TAP and connection implementation has been removed from the 0.1.149 candidate.
