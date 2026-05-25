import socket
import struct
import time

def read_registers():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1)
        sock.connect(('192.168.55.188', 502))
        # 请求帧
        request = struct.pack('>HHHBBHH', 1, 0, 6, 1, 3, 0, 2)
        sock.send(request)
        response = sock.recv(1024)
        sock.close()
        if len(response) >= 9:
            byte_count = response[8]
            regs = []
            for i in range(byte_count // 2):
                reg = (response[9 + i*2] << 8) | response[9 + i*2 + 1]
                regs.append(reg)
            return regs
    except:
        pass
    return None

def stress_test(interval_ms, total_requests):
    print(f"压力测试: 间隔 {interval_ms} ms, 共 {total_requests} 次")
    success = 0
    for i in range(total_requests):
        start = time.time()
        data = read_registers()
        elapsed = (time.time() - start) * 1000
        if data:
            success += 1
            print(f"[{i+1}] 成功: 温度={data[0]/10}℃, 湿度={data[1]/10}%, 耗时={elapsed:.1f}ms")
        else:
            print(f"[{i+1}] 失败")
        time.sleep(interval_ms / 1000.0)
    print(f"\n结果: 成功率 {success}/{total_requests} = {success/total_requests*100:.1f}%")

if __name__ == '__main__':
    # 先测试 200ms 间隔，共 20 次
    stress_test(200, 20)