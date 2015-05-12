""" generates a precomputed palette for zx spectrum

We initialize a RAM (faster) palette of 128 (BPPPIII), 4 couples palettes
"""
from itertools import product # cartesian

print "// 128 * 4 couples for 2 pixels couples for each 128 attributes"
print "// placed in data segment, initialized from RAM ar startup"
print "#include <stdint.h>"
print "uint32_t zx_couples[128*4] = {"
LO = 25 ; HI = 31 # colors 
for B,Pg,Pr,Pb,Ig,Ir,Ib in product((0,1), repeat=7) : 
	fg = (Ir<<10 | Ig <<5 | Ib ) * (LO if B else HI)
	bg = (Pr<<10 | Pg <<5 | Pb ) * (LO if B else HI)

	print "    0x%08x,0x%08x,0x%08x,0x%08x, // Bri:%d bg:%d%d%d, fg:%d%d%d, "%  \
		(bg<<16|bg, fg<<16|bg, bg<<16|fg, fg<<16|fg,B,Pb,Pr,Pg,Ir,Ib,Ig)
print "};"