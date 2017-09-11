#!/usr/bin/env python
import hashlib, json, websocket, sys, os, thread, time, logging
from optparse import OptionParser, OptionGroup

CHUNK_SIZE = 32 * 1024

def update_progress(progress):
    progress = int(progress)
    print '\r[{0}] {1}%'.format('#'*(progress/10), progress)

class Updater:
    def __init__(self, filename, ip):
        self.filename = filename
        self.file = open(filename, 'rb')
        self.ip = ip
        self.offset = 0
        self.file_size = os.path.getsize(self.filename)
        self.ws = websocket.WebSocketApp("ws://%s/" % self.ip,
                                         on_message=self.on_message,
                                         on_error=self.on_error,
                                         on_open=self.do_update)
        thread.start_new_thread(self.ws.run_forever, ())


    def on_message(self, ws, message):
        msg = json.loads(message)
        if msg["op"] == "firmwareUpdateChunkAck":
            update_progress(float(self.offset) / self.file_size * 100)
            self.__send_next_chunk()

    def on_error(self, ws, error):
        print(error)

    def do_update(self, ws):
        file_md5 = hashlib.md5(self.file.read()).hexdigest()
        self.file.seek(0)

        self.ws.send(json.dumps({"op": "firmwareUpdateStart", "fileSize": self.file_size, "md5": file_md5}))
        self.__send_next_chunk()

    def __send_next_chunk(self):
        chunk = self.file.read(CHUNK_SIZE)
        if chunk:
            self.offset += len(chunk)
            self.ws.send(chunk, websocket.ABNF.OPCODE_BINARY)
        else:
            self.ws.send(json.dumps({"op": "firmwareUpdateComplete"}))
            print "Complete!"
            self.ws.close()
            self.ws.keep_running = False


def main(args):
    logging.basicConfig()
    if len(args) != 3:
        print "USAGE: update_firmware.py [FIRMWARE] [IP/HOSTNAME]"
        return
    u = Updater(args[1], args[2])
    while u.ws.keep_running:
        time.sleep(0.1)


if __name__ == '__main__':
    main(sys.argv)
