import socket
import struct

def read_holding_registers(ip, port, unit_id, start_addr, quantity):
    transaction_id = 1
    protocol_id = 0
    length = 6
    mbap = struct.pack('>HHHBB', transaction_id, protocol_id, length, unit_id, 3)
    pdu = struct.pack('>HH', start_addr, quantity)
    request = mbap + pdu
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(2)
    try:
        sock.connect((ip, port))
        sock.send(request)
        response = sock.recv(1024)
    finally:
        sock.close()
    if len(response) < 9:
        return None
    byte_count = response[8]
    registers = []
    for i in range(byte_count // 2):
        reg = (response[9 + i*2] << 8) | response[9 + i*2 + 1]
        registers.append(reg)
    return registers

if __name__ == '__main__':
    data = read_holding_registers('192.168.55.188', 502, 1, 0, 2)
    if data:
        print(f"温度: {data[0]/10} ℃")
        print(f"湿度: {data[1]/10} %")
    else:
        print("读取失败")