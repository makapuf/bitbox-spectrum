NAME=bitboZx

GAME_BINARY_FILES=title.scr rom/zx48.rom rom/aticatac.z80 rom/tintin.z80 rom/knightlore.z80 rom/lunar.z80 rom/manicm.z80 
GAME_C_FILES = main.c zx_graph.c zx_emu.c zx_couples.c z80_lib/Z80.c zx_filetyp_z80.c lib/events/events.c icon.c


DEFINES = VGA_MODE=320 

include $(BITBOX)/kernel/bitbox.mk

zx_couples.c: gen_zx_couples.py
	python gen_zx_couples.py > $@

title.scr: title.png
	python img2scr.py title.png 

icon.c: icon.png
	$(BITBOX)/2nd_boot/mk_ico.py $^ > $@
clean:: 
	rm icon.c