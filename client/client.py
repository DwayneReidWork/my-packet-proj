import cmd2
import socket
import struct
# from .netcomm import send_all, recv_expected
# from .packet import PacketType, Multiply, Ping, Upload, List_Files, Run_Cmd
from netcomm import send_all, recv_expected
from packet import PacketType, Multiply, Ping, Upload, List_Files, Run_Cmd

MAX_LIST_SIZE = 128
MAX_FNAME_SIZE = 256


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

        # Send ping
        send_all(self.sock, Ping().pack())

        # Read response header
        packet_type, expected_len = self._check_packet_type(self.sock)

        if packet_type == PacketType.PING:
            if expected_len != 0:
                print(f"Warning: expected 0-length ping response, got {expected_len}")
                _ = recv_expected(self.sock, expected_len)  # Drain extra if sent
            print('Ping Successful!')
        else:
            print(f"Unexpected packet type: {packet_type}")


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
        response_data = recv_expected(self.sock, expected_len)
            
        result = struct.unpack('!i', response_data)[0]
        print(f"Result of multiplication is {result}.") 
    
    upload_parser = cmd2.Cmd2ArgumentParser()
    upload_parser.add_argument('fileName', type=str, action='store', help='The File Whose contents to upload.')
    @cmd2.with_argparser(upload_parser)
    def do_upload(self, args):
        #Read file & send data
        if self.sock is None:
            raise Exception('Not Connected.')
        try:
            with open (args.fileName, 'r', encoding='utf-8') as file:
                contents = file.read()             

            filename = args.fileName
            full_contents = f"{filename}|{contents}"
            print(f"Full Contents {full_contents}")

            # Create the upload packet and send it
            upload_packet = Upload(full_contents).pack()
            send_all(self.sock, upload_packet)
            print("Content Sent")
                
        except FileNotFoundError as e:
            print(e)
            return

        #receive response
        packet_type, expected_len = self._check_packet_type(self.sock)
        
        if(packet_type != PacketType.UPLOAD):
            print(f"Unexpected packet type: {packet_type}")
            return
        if(None == expected_len):
            print(f'Invalid Length. {expected_len}')
            return
        
        response_data = recv_expected(self.sock, expected_len)

        if response_data:
            print(f'Response: {response_data.decode()}')
        else:
            print("Error: Failed to receive valid response data.")

    def do_list_files(self, args):
        #send
        if self.sock is None:
            raise Exception('Not Connected.')
        send_all(self.sock, List_Files().pack())

        #receive
        packet_type, expected_len = self._check_packet_type(self.sock)
        
        if(packet_type != PacketType.LIST_FILES):
            print(f"Unexpected packet type: {packet_type}")
            return
        if(None == expected_len):
            print(f'Invalid Length. {expected_len}')
            return
        
        '''Receive 1 byte for number of file names
    For filename_i in range(that length):
    receive 1 byte to know the file name length
    receive that number of bytes for a file name'''
        response_data = recv_expected(self.sock, expected_len)

        # hex_values_of_resp_data = [hex(byte) for byte in response_data]
        # print(' '.join(hex_values_of_resp_data))

        if response_data:
            filenames = parse_file_list(response_data)
            print("Files:")
            for f in filenames:
                print(f" - {f}")
            #print(f'Found Files:\n{response_data}')
        else:
            print("Error: Failed to receive valid response data.")

    run_cmd_parser = cmd2.Cmd2ArgumentParser()
    run_cmd_parser.add_argument('cmd', type=str, action='store', help='The command to run on the other side.')
    @cmd2.with_argparser(run_cmd_parser)
    def do_run_cmd(self, args):
        if self.sock is None:
            raise Exception('Not Connected.')
        try:
            cmd_packet = Run_Cmd(args.cmd).pack()
            send_all(self.sock, cmd_packet)
            print("Command Send.")

        except Exception as e:
            print(e)
            return

    def _check_packet_type(self, args):
        header_format = '!BI'
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

def parse_file_list(data: bytes):
    count = data[0]
    pos = 1
    filenames = []
    for _ in range(count):
        name_len = data[pos]
        pos += 1
        name = data[pos:pos+name_len]
        pos += name_len
        filenames.append(name)
    return filenames