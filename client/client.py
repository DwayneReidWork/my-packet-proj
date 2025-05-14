import cmd2
import socket
import struct
from .netcomm import send_all, recv_expected
from .packet import Multiply, Ping, String, Upload


class Client():
    def __init__(self,client_socket: socket.socket, address:tuple[str,int]):   
        super().__init__()
        self.client_socket = client_socket
        self.address = address


class CmdController(cmd2.Cmd):
    
    def __init__(self):
        super().__init__()
        self.sock = None
        self.debug = True

    def do_connect(self, args):
        connection = ("127.0.0.1", 1234)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect(connection)
    
    def do_ping(self, args):
        self.sock.send(Ping().pack())

        packet_type, expected_len = self._check_packet_type(self.sock)
        
        if(packet_type == String):
            #CURRENTLY THE RESPONSE IS NOT PACKED JUST SENT AS A STRING!!!!!
            response = self.sock.recv(expected_len).decode()
            print(f'Response: {response}')
        


    def _check_packet_type(self, args):
        header_format = '!BI'
        header_size = struct.calcsize(header_format)
        packet_header =  recv_expected(self.sock, header_size)
        if len(packet_header) != header_size:
            raise ValueError("Incomplete header received")
        packet_type = struct.unpack(header_format, packet_header)[0]
        expected_len = struct.unpack(header_format, packet_header)[1]
        print(f"Check Packet Type Found: {packet_type}")
        return (packet_type, expected_len)