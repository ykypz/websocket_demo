#include "device_server.h"
#include <iostream>
#include <locale>
#ifdef _WIN32
#include <windows.h>
#endif

int main() {
    // 设置控制台编码，以支持中文显示
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    std::locale::global(std::locale(""));
#endif
    
    DeviceServer server;
    
    std::cout << "===== 设备服务器 =====" << std::endl;
    std::cout << "支持的命令类型:" << std::endl;
    std::cout << "1. 测量命令 (executeMeasure)" << std::endl;
    std::cout << "2. 设置观察模式 (setAlignViewMode)" << std::endl;
    std::cout << "3. 获取观察模式 (getAlignViewMode)" << std::endl;
    std::cout << "4. 获取设备状态 (getMeasureStatus)" << std::endl;
    std::cout << "5. 开始取流 (startStream)" << std::endl;
    std::cout << "6. 停止取流 (stopStream)" << std::endl;
    std::cout << "7. 停止测量 (stopMeasure)" << std::endl;
    std::cout << "============================" << std::endl;
    
    server.run(9002);  // 在9002端口启动服务器
    return 0;
}
