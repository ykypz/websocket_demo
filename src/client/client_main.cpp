#include "device_client.h"

#include <iostream>
#include <string>
#include <limits>
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

    // 创建设备客户端
    DeviceClient client;
    
    // 连接到服务器
    if (!client.connect("ws://localhost:9002")) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    std::cout << "Connected to server." << std::endl;
    
    // 等待连接建立
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    int choice = 0;
    bool running = true;
    
    while (running) {
        std::cout << "\n=== 设备控制菜单 ===" << std::endl;
        std::cout << "1. 发送阻塞测量命令" << std::endl;
        std::cout << "2. 发送非阻塞测量命令" << std::endl;
        std::cout << "3. 设置取流模式" << std::endl;
        std::cout << "4. 获取当前取流模式" << std::endl;
        std::cout << "5. 获取设备状态" << std::endl;
        std::cout << "6. 开始取流" << std::endl;
        std::cout << "7. 停止取流" << std::endl;
        std::cout << "8. 停止测量" << std::endl;
        std::cout << "9. 获取面形数据" << std::endl;
        std::cout << "0. 退出" << std::endl;
        std::cout << "请选择操作 (0-9): ";
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        CommandResult result;
        
        switch (choice) {
            case 1: {
                // 发送阻塞测量命令
                std::cout << "发送阻塞测量命令..." << std::endl;
                result = client.executeMeasurement(true);
                break;
            }
            case 2: {
                // 发送非阻塞测量命令
                std::cout << "发送非阻塞测量命令..." << std::endl;
                result = client.executeMeasurement(false);
                
                // 显示初始结果（通常是pending状态）
                if (result.timeout) {
                    std::cout << "命令执行超时" << std::endl;
                } else if (!result.completed) {
                    std::cout << "命令执行失败: " << result.errorMessage << std::endl;
                } else {
                    std::cout << "命令初始响应: " << result.data.dump(2) << std::endl;
                    std::cout << "命令已进入异步执行状态，后续结果将通过回调处理" << std::endl;
                }
                
                // 等待一段时间以观察后台处理
                std::cout << "等待5秒以观察后台处理..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
                
                // 直接跳过后面的结果显示
                continue;
            }
            case 3: {
                // 设置取流模式
                std::string mode;
                std::cout << "请输入取流模式 (continuous/trigger/snapshot): ";
                std::getline(std::cin, mode);
                
                std::cout << "设置取流模式为: " << mode << std::endl;
                result = client.setAlignViewMode(mode);
                break;
            }
            case 4: {
                // 获取当前取流模式
                std::cout << "获取当前取流模式..." << std::endl;
                result = client.getAlignViewMode();
                break;
            }
            case 5: {
                // 获取设备状态
                std::cout << "获取设备状态..." << std::endl;
                result = client.getMeasureStatus();
                break;
            }
            case 6: {
                // 开始取流
                std::cout << "开始取流..." << std::endl;
                result = client.startStream();
                break;
            }
            case 7: {
                // 停止取流
                std::cout << "停止取流..." << std::endl;
                result = client.stopStream();
                break;
            }
            case 8: {
                // 停止测量
                std::cout << "停止测量..." << std::endl;
                result = client.stopMeasure();
                break;
            }
            case 9: {
                // 获取面形数据
                std::cout << "获取面形数据..." << std::endl;
                result = client.getSurfaceData();
                break;
            }
            case 0:
                running = false;
                continue;
            default:
                std::cout << "无效选择，请重试" << std::endl;
                continue;
        }
        
        // 显示命令结果
        if (result.timeout) {
            std::cout << "命令执行超时" << std::endl;
        } else if (!result.completed) {
            std::cout << "命令执行失败: " << result.errorMessage << std::endl;
        } else {
            std::cout << "命令执行成功" << std::endl;
            std::cout << "结果数据: " << result.data.dump(2) << std::endl;
        }
    }
    
    // 关闭连接
    client.close();
    
    return 0;
}
