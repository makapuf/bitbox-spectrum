"ZX spectrum SCR writer from image"
# input : filename 


import sys
from glob import glob
from PIL import Image
from array import array


sys.argv += glob('*.gif')


# in order of ZX index, multiply by 190 or 255
ZXcols=[(0,0,0),(0,0,1),(1,0,0),(1,0,1),(0,1,0),(0,1,1),(1,1,0),(1,1,1)] 


for imgname in sys.argv[1:] : 
    img_pixels = array('B',[0]*6144)
    img_attrs = array('B')

    # no transp
    img=Image.open(imgname).convert('RGB') 
    assert img.size == (256,192), 'Bad image size, must be 256x192'

    for ty in range(img.size[1]//8) : 
        for tx in range(img.size[0]//8) : 
            tile=img.crop((tx*8,ty*8,(tx+1)*8,(ty+1)*8))
            tiledata = tuple(tile.getdata())
            l = list(set(tiledata))
            c1,c2 = l if len(l)==2 else l*2

            bright = all(x in (0,255) for x in c1+c2)
            i1 = ZXcols.index(tuple(x!=0 for x in c1))
            i2 = ZXcols.index(tuple(x!=0 for x in c2))
            attr = bright<<6 | i1<<3 | i2
            img_attrs.append(attr)


            pixels = [int(p==c2) for p in tiledata]
            # convert bits to a list of bytes - stupid oneliner
            pixels_str = [int("".join(map(str,pixels[i:i+8])),2) for i in range(0,len(pixels),8)]
            for dy in range(8) : 
                y=ty*8+dy
                newy = ((ty<<3) & 0b11000000) | ( dy & 0b00000111) << 3 | (ty & 0b111)
                img_pixels[32*newy+tx] = pixels_str[dy]


            # same brightness ? 
    print imgname, len(img_pixels) + len(img_attrs)
    with open(imgname.rsplit('.',1)[0]+'.scr','wb') as of : 
        img_pixels.tofile(of)
        img_attrs.tofile(of)

# output scr file 
# FIXME output z80 file ?
