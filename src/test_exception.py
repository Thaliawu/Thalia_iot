import socket
import struct

def send_modbus_request(ip, port, unit_id, function_code, start_addr, quantity):
    transaction_id = 1
    protocol_id = 0
    length = 6
    mbap = struct.pack('>HHHBB', transaction_id, protocol_id, length, unit_id, function_code)
    pdu = struct.pack('>HH', start_addr, quantity)
    request = mbap + pdu
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(2)
    try:
        sock.connect((ip, port))
        sock.send(request)
        response = sock.recv(1024)
        return response
    except Exception as e:
        print("错误:", e)
        return None
    finally:
        sock.close()

def parse_exception(response):
    if response and len(response) >= 9:
        func = response[7]
        if func & 0x80:
            exc_code = response[8]
            return exc_code
    return None

if __name__ == '__main__':
    ip = '192.168.55.188'   # 改成你 ESP32 的 IP
    port = 502
    unit = 1

    # 测试1：非法功能码（0x66）
    print("测试非法功能码 (0x66):")
    resp = send_modbus_request(ip, port, unit, 0x66, 0, 0)
    if resp:
        exc = parse_exception(resp)
        print(f"响应十六进制: {resp.hex()}, 异常码: {exc} (预期 0x01)")
    else:
        print("无响应")

    # 测试2：非法数据地址（地址10，数量1）
    print("\n测试非法数据地址 (地址10):")
    resp = send_modbus_request(ip, port, unit, 0x03, 10, 1)
    if resp:
        exc = parse_exception(resp)
        print(f"响应: {resp.hex()}, 异常码: {exc} (预期 0x02)")

    # 测试3：非法数据值（数量0）
    print("\n测试非法数据值 (数量0):")
    resp = send_modbus_request(ip, port, unit, 0x03, 0, 0)
    if resp:
        exc = parse_exception(resp)
        print(f"响应: {resp.hex()}, 异常码: {exc} (预期 0x03)")

    # 测试4：非法数据值（数量3）
    print("\n测试非法数据值 (数量3):")
    resp = send_modbus_request(ip, port, unit, 0x03, 0, 3)
    if resp:
        exc = parse_exception(resp)
        print(f"响应: {resp.hex()}, 异常码: {exc} (预期 0x03)")