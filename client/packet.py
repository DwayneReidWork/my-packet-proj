from enum import IntEnum, auto
import struct

class PacketType(IntEnum):
    PING = 1
    STRING = auto()
    MUL = auto()
    UPLOAD = auto()
    LIST_FILES = auto()
    RUN_CMD = auto()


class Packet:
    def __init__(self, p_type: PacketType):
        self._p_type = p_type

    @property
    def packet_type(self):
        return self._p_type

    def packet_size(self) -> int:
        return 0 # for no header size inclusion
    
    def pack(self) -> bytes:
        # Format: Network(Big-endian + Single Byte)
        return struct.pack('!BI', self.packet_type, self.packet_size())


class Ping(Packet):
    def __init__(self):
        super().__init__(PacketType.PING)

    def pack(self) -> bytes:
        # Format: Network(Big-endian + Single Byte)
        return super().pack()
    

class Multiply(Packet):
    def __init__(self, num: int):
        super().__init__(PacketType.MUL)
        self.num = num

    def packet_size(self):
        return struct.calcsize('!i')
    
    def pack(self):
        # Format: Network(Big-endian + Single Byte:signed in (pos/neg))
        return super().pack() + struct.pack('!i', self.num)
    
    
class Upload(Packet):
    def __init__(self, data: str):
        self.data = data
        super().__init__(PacketType.UPLOAD)

    def pack(self) -> bytes:
        return super().pack() + self.data.encode('utf-8')
    
    def packet_size(self) -> int:
        data = self.data.encode()
        return len(data)
    
class List_Files(Packet):
    def __init__(self):
        super().__init__(PacketType.LIST_FILES)
    
    def pack(self) -> bytes:
        return super().pack()

class Run_Cmd(Packet):
    def __init__(self, cmd: str):
        self.cmd = cmd
        super().__init__(PacketType.RUN_CMD)

    def pack(self) -> bytes:
        return super().pack() + self.cmd.encode('utf-8')
    
    def packet_size(self) -> int:
        data = self.cmd.encode()
        return len(data)