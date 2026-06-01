from pymodbus.client import ModbusTcpClient

client = ModbusTcpClient('192.168.55.188', port=502)
if client.connect():
    print("连接成功，正在读取寄存器...")
    # 直接使用位置参数：地址、数量、单元ID
    result = client.read_holding_registers(0, 2, 1)
    if not result.isError():
        print(f"寄存器值: {result.registers}")
        print(f"温度: {result.registers[0]/10} ℃")
        print(f"湿度: {result.registers[1]/10} %")
    else:
        print(f"Modbus错误: {result}")
    client.close()
else:
    print("连接失败")