#!/usr/bin/env python
import hashlib
import logging
import os
import sys

import requests
from requests_toolbelt.multipart.encoder import MultipartEncoderMonitor, MultipartEncoder
from clint.textui.progress import Bar as ProgressBar


def create_callback(size):
    bar = ProgressBar(expected_size=size, filled_char='=')

    def callback(monitor):
        bar.show(monitor.bytes_read)

    return callback


class Updater:
    def __init__(self, filename, ip):
        self.filename = filename
        self.file = open(filename, 'rb')
        self.ip = ip
        self.offset = 0
        self.file_size = os.path.getsize(self.filename)

    def my_callback(self, monitor):
        # Your callback function
        pass

    def do_update(self):
        file_md5 = hashlib.md5(self.file.read()).hexdigest()
        self.file.seek(0)

        url = 'http://%s/fw?md5=%s&size=%s' % (self.ip, file_md5, self.file_size)

        encoder = MultipartEncoder(
            {'file': ('firmware.bin', self.file, 'application/octet-stream')}
        )
        monitor = MultipartEncoderMonitor(encoder, create_callback(self.file_size))
        r = requests.post(url, data=monitor, headers={'Content-Type': encoder.content_type})
        if r.status_code == requests.codes.ok:
            print "OK!"
        else:
            print r.status_code


def main(args):
    logging.basicConfig()
    if len(args) != 3:
        print "USAGE: update.py [FIRMWARE] [IP/HOSTNAME]"
        return
    u = Updater(args[1], args[2]).do_update()


if __name__ == '__main__':
    main(sys.argv)
