import sys
from TiffToCode import TiffToCode
import math
import numpy as np
import serial
import time

class Exposer(object):
    port = None

    def run(self):

        xdbi = 400
        ydbi = 1693

        file = sys.argv[1]

        print("Opening port")
        self.port = serial.Serial('/dev/ttyUSB0', 9600)

        time.sleep(3);
        self.send_command("L1")

        yincstep= math.floor(ydbi/xdbi)
        yincpx = yincstep / ydbi * xdbi

        tc = TiffToCode()
        print("Loading file "+file)
        tc.load_image(file)

        lines = int(round(tc.height / yincpx))
        for y in np.arange(tc.height - 1 ,0, -yincpx):            
            start = time.time()
            if not tc.is_empty_line(round(y)):
                str = tc.get_line_as_hex(round(y))
                self.send_command("P"+("%0.4X" % int(len(str)/2)) + str)

            self.send_command("MY+"+ ("%0.4X" % int(yincstep*16)))
            lines = lines - 1
            totalsec = (time.time()-start) * lines
            print("%d min %d sec left" % ( totalsec / 60, totalsec % 60))
        self.port.close()

    def send_command(self,cmd):
        print("Sending:"+cmd)
        self.port.write(cmd.encode('ascii'))
        self.port.flush()
        resp = self.port.readline()
        #resp = "OK"
        retval=resp.startswith(b'O')
        if not retval:
            print("Error!!!")
        return


if __name__ == '__main__':
    Exposer().run()
