import re
import unittest
from unittest.mock import patch

from q80.config import Config
from q80.transport.telnet import Telnet, _Decoder


class FakeSocket:
    def __init__(self):
        self.chunks = [b"/ # "]

    def __enter__(self):
        return self

    def __exit__(self, *_):
        pass

    def settimeout(self, *_):
        pass

    def sendall(self, data):
        if data.startswith(b"\xff"):
            self.negotiation = data
            return
        token = re.search(rb"[0-9a-f]{32}", data).group().decode()
        reply = f"\r\n__Q80_{token}_BEGIN__\r\nok\r\n__Q80_{token}_END__:7\r\n".encode()
        self.chunks.extend((data, reply[:19], reply[19:]))

    def recv(self, _):
        return self.chunks.pop(0) if self.chunks else b""


class TelnetTest(unittest.TestCase):
    def test_result_and_exit_code(self):
        with patch("q80.transport.telnet.socket.create_connection", return_value=FakeSocket()):
            result = Telnet(Config()).run("false", check=False)
        self.assertEqual(result.output, "ok\n")
        self.assertEqual(result.code, 7)

    def test_split_telnet_negotiation(self):
        sock = FakeSocket()
        decoder = _Decoder(sock)
        self.assertEqual(decoder.feed(b"a\xff"), b"a")
        self.assertEqual(decoder.feed(b"\xfd\x01b"), b"b")
        self.assertEqual(sock.negotiation, b"\xff\xfc\x01")
