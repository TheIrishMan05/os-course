from stream import RWStream
from test import assert_eq


class Xv6:
    def __init__(self, stream: RWStream):
        self.stream = stream

    def run(self, program: str):
        self.stream.writeline(program)
        assert_eq(self.stream.readline().replace('$', '').strip(), program)
        assert_eq(self.stream.readline().replace('$', '').strip(), "")
