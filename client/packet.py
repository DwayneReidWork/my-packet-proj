from enum import IntEnum, auto
import struct

class PacketType(IntEnum):
    PING = 1
    STRING = auto()
    MUL = auto()
    UPLOAD = auto()


class Packet:
    def __init__(self, p_type: PacketType):
        self._p_type = p_type

    @property
    def packet_type(self):
        return self._p_type

    def pack(self) -> bytes:
        # Format: Network(Big-endian + Single Byte)
        return struct.pack('!B', self.packet_type)

    def packet_size(self) -> int:
        return 0 # for no header size inclusion

class Ping(Packet):
    def __init__(self):
        super().__init__(PacketType.PING)

class Multiply(Packet):
    def __init__(self, num: int):
        super().__init__(PacketType.MUL)
        self.num = num

    def pack(self):
        # Format: Network(Big-endian + Single Byte:signed in (pos/neg))
        return super().pack() + struct.pack('!i', self.num)
    
    def packet_size(self):
        return super().packet_size() + struct.calcsize('!i')
    
class Upload(Packet):
    def __init__(self, data: str):
        super().__init__(PacketType.UPLOAD)
        self.data = data

    def pack(self) -> bytes:
        return super().pack() + struct.pack('!I', len(self.data.encode())) + self.data.encode('utf-8')
    
    def packet_size(self) -> int:
        return super().packet_size() + struct.calcsize('!I', len(self.data)) + len(self.data)