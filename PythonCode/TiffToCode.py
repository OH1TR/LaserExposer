from PIL import Image


class TiffToCode(object):
    negative = False

    def __init__(self):
        self.image = None

    def load_image(self, file_name):
        im = Image.open(file_name)
        self.image = im.convert('RGB')

    @property
    def height(self):
        width, height = self.image.size
        return height

    @property
    def width(self):
        width, height = self.image.size
        return width

    def is_pixel_on(self, x, y):
        r, g, b = self.image.getpixel((x, y))
        return ((r > 100) & self.negative) | ((r < 100) and not self.negative)

    def is_empty_line(self, line_number):
        for x in range(0, self.width - 1):
            if self.is_pixel_on(x, line_number):
                return False
        return True

    def get_line_as_hex(self, line_number):
        retval = ""
        data = 0
        c = 0
        for x in range(0, self.width - 1):
            data = data >> 1
            if self.is_pixel_on(x, line_number):
                data = data | 128
            c = c + 1
            if c == 8:
                retval = retval + "%0.2X" % data
                c = 0
                data = 0
        if c != 0:
            data = data >> (8 - c)
            retval = retval + "%0.2X" % data

        return retval
