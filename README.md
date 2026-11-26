# F-91W BLE sensor watch

A Casio F-91W modded into a BLE watch with heart rate (MAXM86161 PPG flex in the case back) and
step counting (LIS2DW12), on a custom STM32WB55 mainboard or the Sensor Watch Pro.

| Folder | Contents |
|---|---|
| `firmware/` | STM32WB55 firmware (STM32CubeWB v1.24.0, Makefile). See `firmware/README.md`. |
| `companion/` | Web Bluetooth page, `f91w.py` CLI, and the iOS Apple Health app (`companion/ios/`, not yet compiled). |
| `hardware/paper-test/` | 1:1 printable fit test for the PPG flex (revs A to C). |
| `archive/` | Hardware snapshots from 2026-10-09 (mainboard, flex revs A/B, stencils, CNC back). Superseded: the current flex is rev Q, which lives in the project files. |

Moved here from `oliverboersma/claudetest` (branches `claude/f91w-firmware-e3ohv1`,
`claude/project-thread-ljxiyl`, `claude/project-thread-tixzj6`, `claude/stoic-cori-1qhoci`).
