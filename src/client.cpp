#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_STL_

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <future>
#include <functional>
#include <any>
#include <limits>
#include <cstdio>
#include <locale>
#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// 使用asio作为底层网络库
typedef websocketpp::client<websocketpp::config::asio_client> client;

// 消息处理回调函数的类型
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

// 连接句柄类型
typedef websocketpp::connection_hdl connection_hdl;

// 命令类型定义
enum class CommandType {
    MEASURE,             // 测量命令
    SET_STREAM_MODE,     // 设置取流模式
    GET_STREAM_MODE,     // 获取取流模式
    DEVICE_STATUS,       // 获取设备状态
    CALIBRATE            // 校准命令
};

// 通用命令结果结构体
struct CommandResult {
    bool completed = false;
    bool timeout = false;
    json data;
    CommandType type;
    std::string errorMessage;
};

// 将CommandType转换为字符串
std::string commandTypeToString(CommandType type) {
    switch (type) {
        case CommandType::MEASURE: return "executeMeasure";
        case CommandType::SET_STREAM_MODE: return "setAlignViewMode";
        case CommandType::GET_STREAM_MODE: return "getAlignViewMode";
        case CommandType::DEVICE_STATUS: return "getDeviceStatus";
        case CommandType::CALIBRATE: return "calibrate";
        default: return "unknown";
    }
}

// 将字符串转换为CommandType
CommandType stringToCommandType(const std::string& typeStr) {
    if (typeStr == "executeMeasure") return CommandType::MEASURE;
    if (typeStr == "setAlignViewMode") return CommandType::SET_STREAM_MODE;
    if (typeStr == "getAlignViewMode") return CommandType::GET_STREAM_MODE;
    if (typeStr == "deviceStatus") return CommandType::DEVICE_STATUS;
    if (typeStr == "calibrate") return CommandType::CALIBRATE;
    return CommandType::MEASURE; // 默认为测量命令
}

class DeviceClient {
private:
    // 请求结构体，用于存储请求信息
    struct PendingRequest {
        std::shared_ptr<CommandResult> first;
        std::shared_ptr<std::promise<void>> second;
        CommandType cmdType;
    };
    
    // 迭代器类型定义
    using PendingRequestsIterator = 
        std::map<std::string, PendingRequest>::iterator;

public:
    DeviceClient() : m_done(false) {
        // 初始化WebSocket客户端
        m_client.init_asio();
        
        // 设置日志级别
        m_client.clear_access_channels(websocketpp::log::alevel::all);
        m_client.clear_error_channels(websocketpp::log::elevel::all);
        
        // 设置消息处理回调
        m_client.set_message_handler(bind(&DeviceClient::on_message, this, ::_1, ::_2));
        m_client.set_open_handler(bind(&DeviceClient::on_open, this, ::_1));
        m_client.set_close_handler(bind(&DeviceClient::on_close, this, ::_1));
        m_client.set_fail_handler(bind(&DeviceClient::on_fail, this, ::_1));
    }
    
    // 连接到服务器
    bool connect(const std::string& uri) {
        try {
            websocketpp::lib::error_code ec;
            client::connection_ptr con = m_client.get_connection(uri, ec);
            
            if (ec) {
                std::cerr << "Connect initialization error: " << ec.message() << std::endl;
                return false;
            }
            
            m_hdl = con->get_handle();
            m_client.connect(con);
            
            // 启动异步IO服务
            m_thread = std::thread([this]() {
                try {
                    m_client.run();
                } catch (const std::exception& e) {
                    std::cerr << "Client run error: " << e.what() << std::endl;
                }
            });
            
            return true;
        } catch (const websocketpp::exception& e) {
            std::cerr << "Connect error: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 生成时间戳格式的请求ID (年月日时分秒毫秒)
    std::string generate_request_id() {
        // 获取当前时间点
        auto now = std::chrono::system_clock::now();
        
        // 转换为时间结构
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        struct tm now_tm;
#ifdef _WIN32
        localtime_s(&now_tm, &now_time_t);
#else
        localtime_r(&now_time_t, &now_tm);
#endif
        
        // 获取毫秒部分
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch() % std::chrono::seconds(1)
        );
        
        // 格式化时间为YYYYMMDDHHMMSSMMM
        char buf[20];
        std::sprintf(buf, "%04d%02d%02d%02d%02d%02d%03d",
                     now_tm.tm_year + 1900,  // 年
                     now_tm.tm_mon + 1,      // 月
                     now_tm.tm_mday,         // 日
                     now_tm.tm_hour,         // 时
                     now_tm.tm_min,          // 分
                     now_tm.tm_sec,          // 秒
                     static_cast<int>(now_ms.count())); // 毫秒
        
        return std::string(buf);
    }
    
    // 发送通用命令并等待响应
    CommandResult send_command(CommandType cmdType, const json& params, int timeout_sec = 10) {
        if (!m_connected) {
            std::cerr << "Not connected to server" << std::endl;
            return {false, true, json(), cmdType, "Not connected to server"};
        }
        
        // 生成请求ID
        std::string requestId = generate_request_id();
        
        // 创建命令请求消息
        json request = {
            {"command", commandTypeToString(cmdType)},
            {"requestId", requestId},
            {"params", params}
        };
        
        // 创建等待结果对象并保存到映射表中
        auto result = std::make_shared<CommandResult>();
        result->type = cmdType;
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        
        // 保存到请求映射表中
        {
            std::lock_guard<std::mutex> lock(m_pending_mutex);
            PendingRequest req;
            req.first = result;
            req.second = promise;
            req.cmdType = cmdType;
            m_pending_requests[requestId] = req;
        }
        
        // 发送请求
        try {
            m_client.send(m_hdl, request.dump(), websocketpp::frame::opcode::text);
            std::cout << "Sent " << commandTypeToString(cmdType) << " request with ID: " << requestId << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error sending request: " << e.what() << std::endl;
            // 从映射表中移除请求
            std::lock_guard<std::mutex> lock(m_pending_mutex);
            m_pending_requests.erase(requestId);
            return {false, true, json(), cmdType, std::string("Error sending request: ") + e.what()};
        }
        
        // 等待响应或超时
        auto status = future.wait_for(std::chrono::seconds(timeout_sec));
        if (status == std::future_status::timeout) {
            std::cout << "Request timed out after " << timeout_sec << " seconds" << std::endl;
            // 将超时状态设置为true
            std::lock_guard<std::mutex> lock(m_pending_mutex);
            if (m_pending_requests.count(requestId) > 0) {
                m_pending_requests[requestId].first->timeout = true;
                m_pending_requests[requestId].first->errorMessage = "Request timed out";
                auto temp_result = *m_pending_requests[requestId].first;
                m_pending_requests.erase(requestId);
                return temp_result;
            }
        }
        
        // 获取结果并从映射表中移除请求
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        if (m_pending_requests.count(requestId) > 0) {
            auto temp_result = *m_pending_requests[requestId].first;
            m_pending_requests.erase(requestId);
            return temp_result;
        } else {
            // 请求已被处理，可能是在我们检查超时状态后，结果恰好被其他线程处理了
            return {false, true, json(), cmdType, "Request not found in pending requests"};
        }
    }
    
    // 发送测量命令的便捷方法
    CommandResult send_measurement_request(const json& params, int timeout_sec = 10) {
        return send_command(CommandType::MEASURE, params, timeout_sec);
    }
    
    // 发送设置观察模式命令的便捷方法
    CommandResult set_stream_mode(const std::string& mode, int timeout_sec = 5) {
        json params = {
            {"alignViewMode", mode}
        };
        return send_command(CommandType::SET_STREAM_MODE, params, timeout_sec);
    }
    
    // 发送获取取流模式命令的便捷方法
    CommandResult get_stream_mode(int timeout_sec = 5) {
        return send_command(CommandType::GET_STREAM_MODE, json(), timeout_sec);
    }
    
    // 发送获取设备状态命令的便捷方法
    CommandResult get_device_status(int timeout_sec = 5) {
        return send_command(CommandType::DEVICE_STATUS, json(), timeout_sec);
    }
    
    // 发送校准命令的便捷方法
    CommandResult calibrate(const json& params, int timeout_sec = 20) {
        return send_command(CommandType::CALIBRATE, params, timeout_sec);
    }
    
    // 关闭连接
    void close() {
        if (m_connected) {
            websocketpp::lib::error_code ec;
            m_client.close(m_hdl, websocketpp::close::status::normal, "Client closing connection", ec);
            if (ec) {
                std::cerr << "Error closing connection: " << ec.message() << std::endl;
            }
        }
        
            // 通知所有等待中的请求
            {
                std::lock_guard<std::mutex> lock(m_pending_mutex);
                for (auto& request : m_pending_requests) {
                    request.second.first->timeout = true;
                    request.second.first->errorMessage = "Connection closed";
                    request.second.second->set_value();
                }
                m_pending_requests.clear();
            }        m_done = true;
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
    
    ~DeviceClient() {
        close();
    }

private:
    void on_open(connection_hdl hdl) {
        std::cout << "Connection opened" << std::endl;
        m_connected = true;
    }
    
    void on_close(connection_hdl hdl) {
        std::cout << "Connection closed" << std::endl;
        m_connected = false;
    }
    
    void on_fail(connection_hdl hdl) {
        std::cout << "Connection failed" << std::endl;
        m_connected = false;
    }
    
    void on_message(connection_hdl hdl, message_ptr msg) {
        try {
            // 解析JSON消息
            std::string payload = msg->get_payload();
            json message = json::parse(payload);
            
            // 确保消息包含必要的字段
            if (!message.contains("command") || !message.contains("requestId")) {
                std::cerr << "Invalid message format: missing required fields" << std::endl;
                return;
            }
            
            std::string msgType = message["command"];
            std::string requestId = message["requestId"];
            
            // 查找对应的请求
            std::lock_guard<std::mutex> lock(m_pending_mutex);
            auto it = m_pending_requests.find(requestId);
            if (it != m_pending_requests.end()) {
                // 根据命令类型处理响应
                CommandType cmdType = it->second.cmdType;
                
                if (msgType == "executeMeasure") {
                    // 处理测量命令的响应
                    handle_measure_response(it, message);
                } else if (msgType == "setAlignViewMode" || msgType == "getAlignViewMode") {
                    // 处理观察模式命令的响应
                    handle_stream_mode_response(it, message);
                } else if (msgType == "getDeviceStatus") {
                    // 处理设备状态命令的响应
                    handle_device_status_response(it, message);
                } else if (msgType == "calibrate") {
                    // 处理校准命令的响应
                    handle_calibration_response(it, message);
                } else {
                    // 通用响应处理
                    handle_generic_response(it, message);
                }
            } else {
                std::cout << "Received response for unknown request ID: " << requestId << std::endl;
            }
        } catch (json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        } catch (std::exception& e) {
            std::cerr << "Error processing message: " << e.what() << std::endl;
        }
    }
    
    // 处理测量命令的响应
    void handle_measure_response(PendingRequestsIterator it, const json& message) {
        if (!message.contains("status")) {
            return;
        }
        
        std::string status = message["status"];
        if (status == "pending") {
            // 收到"正在处理"的状态，继续等待
            std::cout << "Measurement in progress for request: " << it->first << std::endl;
        } else if (status == "success") {
            // 收到"成功"的状态，记录结果并通知等待线程
            it->second.first->completed = true;
            if (message.contains("data")) {
                it->second.first->data = message["data"];
            }
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Measurement completed successfully for request: " << it->first << std::endl;
        } else if (status == "error") {
            // 处理错误状态
            it->second.first->completed = false;
            if (message.contains("errorMessage")) {
                it->second.first->errorMessage = message["errorMessage"];
            } else {
                it->second.first->errorMessage = "Unknown error";
            }
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Measurement error for request: " << it->first 
                      << " - " << it->second.first->errorMessage << std::endl;
        } else if (status == "timeout") {
            // 处理超时状态
            it->second.first->completed = false;
            it->second.first->timeout = true;
            it->second.first->errorMessage = "Measurement operation timed out";
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Measurement timeout for request: " << it->first << std::endl;
        }
    }
    
    // 处理取流模式命令的响应
    void handle_stream_mode_response(PendingRequestsIterator it, const json& message) {
        if (!message.contains("status")) {
            return;
        }

        std::string status = message["status"];
        if (status == "success") {
            // 设置或获取取流模式成功
            it->second.first->completed = true;
            
            if (it->second.cmdType == CommandType::SET_STREAM_MODE) {
                // 设置取流模式的响应
                if (message.contains("data") && message["data"].contains("currentMode")) {
                    it->second.first->data["mode"] = message["data"]["currentMode"];
                }
            } else if (it->second.cmdType == CommandType::GET_STREAM_MODE) {
                // 获取取流模式的响应
                if (message.contains("data") && message["data"].contains("mode")) {
                    it->second.first->data["mode"] = message["data"]["mode"];
                }
            }
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Stream mode operation successful for request: " << it->first << std::endl;
        } else if (status == "error") {
            // 处理错误状态
            it->second.first->completed = false;
            if (message.contains("errorMessage")) {
                it->second.first->errorMessage = message["errorMessage"];
            } else {
                it->second.first->errorMessage = "Unknown error";
            }
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Stream mode operation error for request: " << it->first 
                    << " - " << it->second.first->errorMessage << std::endl;
        } else if (status == "timeout") {
            // 处理超时状态
            it->second.first->completed = false;
            it->second.first->timeout = true;
            it->second.first->errorMessage = "Stream mode operation timed out";
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Stream mode operation timeout for request: " << it->first << std::endl;
        }
    }
    
    // 处理设备状态命令的响应
    void handle_device_status_response(PendingRequestsIterator it, const json& message) {
        if (!message.contains("status")) {
            return;
        }

        std::string status = message["status"];
        if (status == "success") {
            // 获取设备状态成功
            it->second.first->completed = true;
            if (message.contains("data")) {
                it->second.first->data = message["data"];
            }
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Device status query successful for request: " << it->first << std::endl;
        } else if (status == "error") {
            // 处理错误状态
            it->second.first->completed = false;
            if (message.contains("errorMessage")) {
                it->second.first->errorMessage = message["errorMessage"];
            } else {
                it->second.first->errorMessage = "Unknown error";
            }
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Device status query error for request: " << it->first 
                    << " - " << it->second.first->errorMessage << std::endl;
        } else if (status == "timeout") {
            // 处理超时状态
            it->second.first->completed = false;
            it->second.first->timeout = true;
            it->second.first->errorMessage = "Device status query timed out";
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Device status query timeout for request: " << it->first << std::endl;
        }
    }
    
    // 处理校准命令的响应
    void handle_calibration_response(PendingRequestsIterator it, const json& message) {
        if (!message.contains("status")) {
            return;
        }
        
        std::string status = message["status"];
        if (status == "pending") {
            // 校准进行中，继续等待
            std::cout << "Calibration in progress for request: " << it->first << std::endl;
            if (message.contains("progress")) {
                int progress = message["progress"];
                std::cout << "Calibration progress: " << progress << "%" << std::endl;
            }
        } else if (status == "success") {
            // 校准完成成功
            it->second.first->completed = true;
            if (message.contains("data")) {
                it->second.first->data = message["data"];
            }
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Calibration completed successfully for request: " << it->first << std::endl;
        } else if (status == "error") {
            // 校准错误
            it->second.first->completed = false;
            if (message.contains("errorMessage")) {
                it->second.first->errorMessage = message["errorMessage"];
            } else {
                it->second.first->errorMessage = "Unknown error";
            }
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Calibration error for request: " << it->first 
                      << " - " << it->second.first->errorMessage << std::endl;
        } else if (status == "timeout") {
            // 处理超时状态
            it->second.first->completed = false;
            it->second.first->timeout = true;
            it->second.first->errorMessage = "Calibration operation timed out";
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Calibration timeout for request: " << it->first << std::endl;
        }
    }
    
    // 处理通用响应
    void handle_generic_response(PendingRequestsIterator it, const json& message) {
        if (!message.contains("status")) {
            // 如果没有状态字段，尝试解析旧格式的消息
            it->second.first->completed = true;
            it->second.first->data = message;
            
            // 通知等待线程
            it->second.second->set_value();
            return;
        }
        
        std::string status = message["status"];
        if (status == "success") {
            // 操作成功
            it->second.first->completed = true;
            if (message.contains("data")) {
                it->second.first->data = message["data"];
            }
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Operation successful for request: " << it->first << std::endl;
        } else if (status == "error") {
            // 处理错误状态
            it->second.first->completed = false;
            if (message.contains("errorMessage")) {
                it->second.first->errorMessage = message["errorMessage"];
            } else {
                it->second.first->errorMessage = "Unknown error";
            }
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Operation error for request: " << it->first 
                    << " - " << it->second.first->errorMessage << std::endl;
        } else if (status == "pending") {
            // 操作正在进行中，可以记录但不通知等待线程
            std::cout << "Operation pending for request: " << it->first << std::endl;
        } else if (status == "timeout") {
            // 处理超时状态
            it->second.first->completed = false;
            it->second.first->timeout = true;
            it->second.first->errorMessage = "Operation timed out";
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Operation timeout for request: " << it->first << std::endl;
        } else {
            // 未知状态
            it->second.first->completed = false;
            it->second.first->errorMessage = "Unknown status: " + status;
            
            // 通知等待线程
            it->second.second->set_value();
            
            std::cout << "Unknown status for request: " << it->first << " - " << status << std::endl;
        }
    }

    client m_client;
    connection_hdl m_hdl;
    std::thread m_thread;
    bool m_connected = false;
    bool m_done;
    
    // 请求映射表，存储每个请求ID对应的结果、promise和命令类型
    std::mutex m_pending_mutex;
    std::map<std::string, PendingRequest> m_pending_requests;
};

int main() {
    // 设置控制台编码，以支持中文显示
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    std::locale::global(std::locale(""));
#endif

    // 测试一下requestId的生成格式
    DeviceClient client;
    std::string testId = client.generate_request_id();
    std::cout << "生成的requestId示例: " << testId << std::endl;
    
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
        std::cout << "1. 发送测量命令" << std::endl;
        std::cout << "2. 设置取流模式" << std::endl;
        std::cout << "3. 获取当前取流模式" << std::endl;
        std::cout << "4. 获取设备状态" << std::endl;
        std::cout << "5. 执行校准" << std::endl;
        std::cout << "0. 退出" << std::endl;
        std::cout << "请选择操作 (0-5): ";
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        CommandResult result;
        
        switch (choice) {
            case 1: {
                // 发送测量命令
                json params = {
                    {"mode", "standard"},
                    {"precision", "high"}
                };
                
                std::cout << "发送测量命令..." << std::endl;
                result = client.send_measurement_request(params);
                break;
            }
            case 2: {
                // 设置取流模式
                std::string mode;
                std::cout << "请输入取流模式 (continuous/trigger/snapshot): ";
                std::getline(std::cin, mode);
                
                std::cout << "设置取流模式为: " << mode << std::endl;
                result = client.set_stream_mode(mode);
                break;
            }
            case 3: {
                // 获取当前取流模式
                std::cout << "获取当前取流模式..." << std::endl;
                result = client.get_stream_mode();
                break;
            }
            case 4: {
                // 获取设备状态
                std::cout << "获取设备状态..." << std::endl;
                result = client.get_device_status();
                break;
            }
            case 5: {
                // 执行校准
                json params = {
                    {"type", "full"},
                    {"target_distance", 100.0}
                };
                
                std::cout << "执行设备校准..." << std::endl;
                result = client.calibrate(params);
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
