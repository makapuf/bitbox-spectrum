#!/usr/bin/env python

"""scrview.py: ZX Spectrum SCREEN$ writer"""
" from : https://gist.github.com/alexanderk23/f459c76847d9412548f7"

import sys
from PIL import Image
from array import array

class ZXScreen:
    WIDTH = 256
    HEIGHT = 192

    def __init__(self):
        self.bitmap = array('B')
        self.attributes = array('B')

    def load(self, filename):
        with open(filename, 'rb') as f:
            self.bitmap.fromfile(f, 6144)
            self.attributes.fromfile(f, 768)

    def get_pixel_address(self, x, y):
        y76 = y & 0b11000000 # third of screen
        y53 = y & 0b00111000
        y20 = y & 0b00000111
        address = (y76 << 5) + (y20 << 8) + (y53 << 2) + (x >> 3)
        return address

    def get_attribute_address(self, x, y):
        y73 = y & 0b11111000
        address = (y73 << 2) + (x >> 3)
        return address

    def get_byte(self, x, y):
        return self.bitmap[ self.get_pixel_address(x,y) ]

    def get_attribute(self, x, y):
        return self.attributes[ self.get_attribute_address(x,y) ]

    def convert(self):
        img = Image.new('RGB', (ZXScreen.WIDTH, ZXScreen.HEIGHT), 'white')
        pixels = img.load()
        for y in xrange(ZXScreen.HEIGHT):
            for col in xrange(ZXScreen.WIDTH >> 3):
                x = col << 3
                byte = self.get_byte(x, y)
                attr = self.get_attribute(x, y)
                ink = attr & 0b0111
                paper = (attr >> 3) & 0b0111
                bright = (attr >> 6) & 1
                val = 0xcd if not bright else 0xff
                for bit in xrange(8):
                    bit_is_set = (byte >> (7 - bit)) & 1
                    color = ink if bit_is_set else paper
                    rgb = tuple(val * (color >> i & 1) for i in (1,2,0))
                    pixels[x + bit, y] = rgb
        return img

if __name__ == '__main__':

    if len(sys.argv) < 2:
        print "Usage: %s filename.scr" % sys.argv[0]
        sys.exit(2)

    screen = ZXScreen()
    screen.load(sys.argv[1])
    img = screen.convert()
    img.save(sys.argv[1].rsplit('.',1)[0]+'_out.png')
