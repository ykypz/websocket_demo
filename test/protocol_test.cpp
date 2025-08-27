#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <vector>

using json = nlohmann::json;

// 定义模拟测试的辅助函数
bool validate_json_schema(const json& j, const std::string& type) {
    if (type == "measureRequest") {
        return j.contains("type") && j["type"] == "measureRequest" &&
               j.contains("requestId") && j.contains("params");
    } else if (type == "measureStatus") {
        return j.contains("type") && j["type"] == "measureStatus" &&
               j.contains("requestId") && j.contains("status");
    }
    return false;
}

// 模拟测试函数
void run_protocol_test() {
    std::cout << "===== WebSocket 测量协议测试 =====" << std::endl;
    
    // 模拟生成请求ID
    std::string requestId = "20240425123045123"; // 使用时间戳格式
    
    // 1. 模拟客户端发送测量请求
    json request = {
        {"type", "measureRequest"},
        {"requestId", requestId},
        {"params", {
            {"mode", "standard"},
            {"precision", "high"}
        }}
    };
    
    std::cout << "1. 客户端发送测量请求:" << std::endl;
    std::cout << request.dump(2) << std::endl << std::endl;
    
    // 验证请求格式
    if (!validate_json_schema(request, "measureRequest")) {
        std::cerr << "错误：客户端请求格式不符合协议要求" << std::endl;
        return;
    }
    
    // 2. 模拟服务端发送"正在测量"状态
    json measuring_response = {
        {"type", "measureStatus"},
        {"requestId", requestId},
        {"status", "measuring"}
    };
    
    std::cout << "2. 服务端发送'正在测量'状态:" << std::endl;
    std::cout << measuring_response.dump(2) << std::endl << std::endl;
    
    // 验证响应格式
    if (!validate_json_schema(measuring_response, "measureStatus")) {
        std::cerr << "错误：服务端'正在测量'状态格式不符合协议要求" << std::endl;
        return;
    }
    
    // 3. 模拟服务端发送"测量完成"状态
    json done_response = {
        {"type", "measureStatus"},
        {"requestId", requestId},
        {"status", "done"},
        {"data", {
            {"value", 42.5},
            {"unit", "mm"},
            {"timestamp", 1692345678901}
        }}
    };
    
    std::cout << "3. 服务端发送'测量完成'状态:" << std::endl;
    std::cout << done_response.dump(2) << std::endl << std::endl;
    
    // 验证响应格式
    if (!validate_json_schema(done_response, "measureStatus")) {
        std::cerr << "错误：服务端'测量完成'状态格式不符合协议要求" << std::endl;
        return;
    }
    
    // 4. 模拟客户端处理结果
    std::cout << "4. 客户端处理测量结果:" << std::endl;
    if (done_response["status"] == "done" && done_response.contains("data")) {
        std::cout << "测量成功，结果：" << done_response["data"]["value"] << " " 
                  << done_response["data"]["unit"] << std::endl;
    } else {
        std::cout << "测量未完成或出错" << std::endl;
    }
    
    std::cout << std::endl << "协议测试完成，所有消息格式符合规范" << std::endl;
}

int main() {
    // 运行协议测试
    run_protocol_test();
    
    std::cout << std::endl << "按Enter键退出..." << std::endl;
    std::cin.get();
    return 0;
}
