from enum import IntEnum, auto
import struct


class PacketType(IntEnum):
    PING = 1
    CONNECT = auto()
    STRING = auto()
    MUL = auto()
    UPLOAD = auto()
    LIST_FILES = auto()
    RUN_CMD = auto()


class Packet:
    """
    A single unified Packet class that can represent any type of message.
    Each packet carries:
        - type (PacketType)
        - data (bytes, optional)
    """

    HEADER_FORMAT = "!BI"  # packet_type (1 byte), payload_size (4 bytes)
    ## SEMI Relevant PSEUDOCODE (wouldn't be in the class)
    # SAMPLE_FORMAT = "III"

    # type, len = recv_expected(self.sock, struct.calcsize(HEADER_FORMAT))
    # parse_bytes(type, len)

    # def parse_bytes(bytes, byte_len):
    #     rest_of_data = recv_expected(self.sock, byte_len)
    #     #formatting
    #     var1, var2, var3 = struct.unpack(SAMPLE_FORMAT, rest_of_data)

    #     # that header parsing
    #     # switch off of type for tlv
    #     # function to handle 


    def __init__(self, p_type: PacketType, payload=None):
        self.p_type = p_type
        self.payload = b""

        # Allow flexible input for payload
        if payload is None:
            self.payload = b""
        elif isinstance(payload, int):
            self.payload = struct.pack("!i", payload)
        elif isinstance(payload, str):
            self.payload = payload.encode("utf-8")
        elif isinstance(payload, bytes):
            self.payload = payload
        else:
            raise TypeError(f"Unsupported payload type: {type(payload)}")

    @property
    def packet_size(self) -> int:
        """Return size of payload only."""
        return len(self.payload)

    def pack(self) -> bytes:
        """Serialize the packet into bytes."""
        header = struct.pack(self.HEADER_FORMAT, self.p_type, self.packet_size)
        return header + self.payload

    @classmethod
    def unpack(cls, data: bytes):
        """Deserialize a packet from bytes."""
        header_size = struct.calcsize(cls.HEADER_FORMAT)
        p_type, payload_len = struct.unpack(cls.HEADER_FORMAT, data[:header_size])
        payload = data[header_size:header_size + payload_len]
        return cls(PacketType(p_type), payload)

    def __repr__(self):
        return f"<Packet type={self.p_type.name} size={self.packet_size} payload={self.payload!r}>"
