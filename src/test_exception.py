import socket
import struct

def send_modbus_request(ip, port, unit_id, start_addr, quantity):
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
        print("收到响应（十六进制）:", response.hex())
        return response
    except Exception as e:
        print("错误:", e)
        return None
    finally:
        sock.close()

if __name__ == '__main__':
    # 测试非法地址（地址10，超出范围）
    resp = send_modbus_request('192.168.55.188', 502, 1, 10, 2)
    if resp and len(resp) >= 9:
        # 检查是否是异常响应（功能码最高位为1）
        func_code = resp[7]
        if func_code & 0x80:
            exception_code = resp[8]
            print(f"异常响应：功能码 {func_code:#x}，异常码 {exception_code:#x}")
            if exception_code == 0x02:
                print("✅ 正确的异常码：非法数据地址 (0x02)")
            else:
                print(f"❌ 异常码不正确，预期 0x02，实际 {exception_code:#x}")
        else:
            print("收到正常响应，解析寄存器值...")
    else:
        print("未收到有效响应")