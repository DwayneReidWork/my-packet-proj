import cmd2
import socket
import struct
from .netcomm import send_all, recv_expected
from .packet import PacketType, Multiply, Ping, String, Upload


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
        if self.sock is None:
            raise Exception('Not Connected.')
        send_all(self.sock, Ping().pack())

        packet_type, expected_len = self._check_packet_type(self.sock)
        
        if(packet_type == PacketType.STRING):
            response = self.sock.recv(expected_len).decode()
            print(f'Response: {response}')

    mul_parser = cmd2.Cmd2ArgumentParser()
    mul_parser.add_argument('num', type=int, action='store', help='The number to be multiplied')
    @cmd2.with_argparser(mul_parser)
    def do_mul(self, args):
        #Send
        if self.sock is None:
            raise Exception('Not Connected.')
        mul_packet = Multiply(args.num).pack()
        send_all(self.sock, mul_packet)
        
        #Response
        packet_type, expected_len = self._check_packet_type(self.sock)

        if(packet_type != PacketType.MUL):
            print(f"Unexpected packet type: {packet_type}")
            return
        if(None == expected_len):
            print('Invalid Length.')
            return
        expected_size = struct.calcsize('!I') 
        response_data = recv_expected(self.sock, expected_size)
            
        result = struct.unpack('!I', response_data)[0]
        print(f"Result of multiplication is {result}.") 
    

    def _check_packet_type(self, args):
        header_format = '<BI'
        header_size = struct.calcsize(header_format)
        packet_header =  recv_expected(self.sock, header_size)
        if len(packet_header) != header_size:
            raise ValueError("Incomplete header received")
        
        raw_type, expected_len = struct.unpack(header_format, packet_header)
        try:
            packet_type = PacketType(raw_type)
        except ValueError:
            raise ValueError(f"Unknown packet type: {raw_type}")

        return packet_type, expected_len