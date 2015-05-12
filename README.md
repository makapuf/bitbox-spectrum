BitboZx : a Bitbox Spectrum emulator !
--------------------------------------

Origin
======

Inspiration from UB spectrum emulator, heavily modified for bitbox.
ZX80 file Loading from UB routines.

Keyboard
========
(using QWERTY keyboard conventions)

Left Shift 	caps shift
Left Ctrl 	symbol shift
Arrows/RShift	Kempston Joystick
Alt+Del		Reset
Alt+AKLMT	Load Games ...
Alt+PgUp/Dn	Turbo On/Off 

Modifications
=============

The following modifications have been performed : 

- STM32f429 -> stm32f405 (no DMA2D)
- No LCD screen -> bitbox VGA out with no internal RAM
- using bitbox USB (keyboard & joystick) & uSD drivers (future)
- Integration to Bitbox conventions, build process
- Externalization of roms as z80 files


TODO
====

- gamepad
- sound
- SD file game loading with menu
- basic programs load/save ?
- save screenshot to (scr) file
- turbo on / off
- several Keymaps 

Notes
======

first screen display, then go to basic.
