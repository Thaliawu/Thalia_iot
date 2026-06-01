from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusServerContext

def run_server():
    # 1. 定义寄存器数据
    # 注意：这里 0 是起始地址，[256, 600] 是数据
    store = ModbusSequentialDataBlock(0, [256, 600])

    # 2. 创建服务器上下文
    # 【重点修正】这里直接传 store，single=True 表示单设备
    context = ModbusServerContext(store, single=True)

    print("========================================")
    print("🚀 ESP32 模拟器（服务端）已启动！")
    print("💡 正在监听本机 502 端口，等待连接...")
    print("🛑 按 Ctrl+C 停止")
    print("========================================")

    # 3. 启动服务器
    StartTcpServer(context, address=("0.0.0.0", 502))

if __name__ == "__main__":
    run_server()