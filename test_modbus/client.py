import time
from pymodbus.client import ModbusTcpClient

def run_client():
    # 连接到服务端（这里连本机，IP是127.0.0.1，端口502）
    client = ModbusTcpClient('127.0.0.1', port=502)
    
    if client.connect():
        print("✅ 连接服务端成功！")
        
        try:
            while True:
                # 读取保持寄存器 (功能码 3)
                # 地址 0，读取 2 个寄存器
                response = client.read_holding_registers(address=0, count=2, unit=1)
                
                if not response.isError():
                    # 解析数据
                    temp_raw = response.registers[0]
                    humi_raw = response.registers[1]
                    
                    # 换算成实际值 (根据你的设定：除以10)
                    temperature = temp_raw / 10.0
                    humidity = humi_raw / 10.0
                    
                    print(f"🌡️  温度: {temperature} ℃")
                    print(f"💧 湿度: {humidity} %")
                    print("-" * 30)
                else:
                    print("❌ 读取数据出错:", response)
                
                time.sleep(2) # 休息2秒
                
        except KeyboardInterrupt:
            print("🛑 客户端手动停止")
        finally:
            client.close()
            print("🔌 连接已关闭")
    else:
        print("❌ 连接失败！请确认服务端(server.py)是否已经启动。")

if __name__ == "__main__":
    run_client()