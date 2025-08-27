#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <map>
#include <thread>
#include <chrono>
#include <functional>
#include <atomic>
#include <ctime>
#include <iomanip>
#include <cstdio>

using json = nlohmann::json;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// 使用asio作为底层网络库
typedef websocketpp::server<websocketpp::config::asio> server;

// 消息处理回调函数的类型
typedef server::message_ptr message_ptr;

// 连接句柄类型
typedef websocketpp::connection_hdl connection_hdl;

class DeviceServer {
public:
    DeviceServer() {
        // 初始化WebSocket服务器
        m_server.init_asio();

        // 设置日志级别
        m_server.set_access_channels(websocketpp::log::alevel::all);
        m_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // 设置消息处理回调
        m_server.set_message_handler(bind(&DeviceServer::on_message, this, ::_1, ::_2));
        m_server.set_open_handler(bind(&DeviceServer::on_open, this, ::_1));
        m_server.set_close_handler(bind(&DeviceServer::on_close, this, ::_1));
        
        // 初始化默认流模式
        m_current_stream_mode = "continuous";
    }
    
    // 解析时间戳格式的requestId为可读格式
    std::string parse_timestamp_request_id(const std::string& requestId) {
        if (requestId.length() != 17) {
            return requestId; // 不是标准的时间戳格式，直接返回
        }
        
        try {
            // 提取年月日时分秒毫秒
            int year = std::stoi(requestId.substr(0, 4));
            int month = std::stoi(requestId.substr(4, 2));
            int day = std::stoi(requestId.substr(6, 2));
            int hour = std::stoi(requestId.substr(8, 2));
            int minute = std::stoi(requestId.substr(10, 2));
            int second = std::stoi(requestId.substr(12, 2));
            int millisecond = std::stoi(requestId.substr(14, 3));
            
            // 格式化为可读格式
            char buf[30];
            std::sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                        year, month, day, hour, minute, second, millisecond);
            
            return std::string(buf);
        } catch (const std::exception& e) {
            return requestId; // 解析失败，返回原始ID
        }
    }

    void run(uint16_t port) {
        // 设置服务器监听端口
        m_server.listen(port);

        // 开始接收连接
        m_server.start_accept();

        // 启动服务器
        try {
            std::cout << "服务器已启动，监听端口: " << port << std::endl;
            m_server.run();
        } catch (websocketpp::exception const & e) {
            std::cerr << "服务器异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "未知异常" << std::endl;
        }
    }

private:
    void on_open(connection_hdl hdl) {
        std::cout << "Connection opened" << std::endl;
        m_connections.insert(hdl);
    }

    void on_close(connection_hdl hdl) {
        std::cout << "Connection closed" << std::endl;
        m_connections.erase(hdl);
    }
    
    // 处理测量请求
    void handle_measure_request(connection_hdl hdl, const std::string& requestId, const json& params) {
        std::string readableTime = parse_timestamp_request_id(requestId);
        std::cout << "处理测量请求: " << requestId << " (" << readableTime << ")" << std::endl;
        
        // 立即回复"正在测量"状态
        send_measuring_status(hdl, requestId);
        
        // 启动模拟测量任务
        start_measurement(hdl, requestId, params);
    }
    
    // 处理设置取流模式请求
    void handle_set_stream_mode(connection_hdl hdl, const std::string& requestId, const json& params) {
        std::string readableTime = parse_timestamp_request_id(requestId);
        std::cout << "处理设置取流模式请求: " << requestId << " (" << readableTime << ")" << std::endl;
        
        if (!params.contains("mode")) {
            // 发送错误响应
            json response = {
                {"type", "stream_mode_status"},
                {"requestId", requestId},
                {"success", false},
                {"error", "Missing mode parameter"},
                {"current_mode", m_current_stream_mode}
            };
            
            m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
            return;
        }
        
        std::string mode = params["mode"];
        bool valid_mode = false;
        
        // 检查模式是否有效
        if (mode == "continuous" || mode == "trigger" || mode == "snapshot") {
            valid_mode = true;
        }
        
        if (valid_mode) {
            // 设置新的取流模式
            m_current_stream_mode = mode;
            
            // 发送成功响应
            json response = {
                {"type", "stream_mode_status"},
                {"requestId", requestId},
                {"success", true},
                {"current_mode", m_current_stream_mode}
            };
            
            m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
            std::cout << "取流模式已设置为: " << mode << std::endl;
        } else {
            // 发送错误响应
            json response = {
                {"type", "stream_mode_status"},
                {"requestId", requestId},
                {"success", false},
                {"error", "Invalid mode: " + mode + ". Valid modes are: continuous, trigger, snapshot"},
                {"current_mode", m_current_stream_mode}
            };
            
            m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        }
    }
    
    // 处理获取取流模式请求
    void handle_get_stream_mode(connection_hdl hdl, const std::string& requestId) {
        std::string readableTime = parse_timestamp_request_id(requestId);
        std::cout << "处理获取取流模式请求: " << requestId << " (" << readableTime << ")" << std::endl;
        
        // 发送当前取流模式
        json response = {
            {"type", "stream_mode_status"},
            {"requestId", requestId},
            {"mode", m_current_stream_mode}
        };
        
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    }
    
    // 处理获取设备状态请求
    void handle_device_status(connection_hdl hdl, const std::string& requestId) {
        std::string readableTime = parse_timestamp_request_id(requestId);
        std::cout << "处理获取设备状态请求: " << requestId << " (" << readableTime << ")" << std::endl;
        
        // 模拟设备状态信息
        json status = {
            {"device_id", "DEV12345"},
            {"firmware_version", "2.5.1"},
            {"temperature", 36.7},
            {"uptime", 12345},
            {"stream_mode", m_current_stream_mode},
            {"is_calibrated", true},
            {"battery", 85}
        };
        
        // 发送设备状态响应
        json response = {
            {"type", "device_status_response"},
            {"requestId", requestId},
            {"status", status}
        };
        
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    }
    
    // 处理校准请求
    void handle_calibrate(connection_hdl hdl, const std::string& requestId, const json& params) {
        std::string readableTime = parse_timestamp_request_id(requestId);
        std::cout << "处理校准请求: " << requestId << " (" << readableTime << ")" << std::endl;
        
        // 立即发送校准开始状态
        json start_response = {
            {"type", "calibration_status"},
            {"requestId", requestId},
            {"status", "calibrating"},
            {"progress", 0}
        };
        
        m_server.send(hdl, start_response.dump(), websocketpp::frame::opcode::text);
        
        // 启动校准任务
        std::thread([this, hdl, requestId, params]() {
            // 模拟校准过程
            std::string calibration_type = params.value("type", "standard");
            int steps = (calibration_type == "full") ? 5 : 3;
            
            for (int i = 1; i <= steps; i++) {
                // 每步暂停一段时间
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
                // 发送进度更新
                int progress = (i * 100) / steps;
                json progress_response = {
                    {"type", "calibration_status"},
                    {"requestId", requestId},
                    {"status", "calibrating"},
                    {"progress", progress}
                };
                
                m_server.send(hdl, progress_response.dump(), websocketpp::frame::opcode::text);
            }
            
            // 校准完成
            json result = {
                {"success", true},
                {"calibration_type", calibration_type},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
                {"offset", 0.05}
            };
            
            json complete_response = {
                {"type", "calibration_status"},
                {"requestId", requestId},
                {"status", "done"},
                {"result", result}
            };
            
            m_server.send(hdl, complete_response.dump(), websocketpp::frame::opcode::text);
            std::cout << "校准完成: " << requestId << std::endl;
        }).detach();
    }

    void on_message(connection_hdl hdl, message_ptr msg) {
        try {
            // 解析JSON消息
            std::string payload = msg->get_payload();
            json message = json::parse(payload);

            // 检查消息类型
            if (!message.contains("type") || !message.contains("requestId")) {
                std::cerr << "Invalid message format" << std::endl;
                return;
            }
            
            std::string type = message["type"];
            std::string requestId = message["requestId"];
            std::string readableTime = parse_timestamp_request_id(requestId);
            
            std::cout << "收到请求: [" << type << "], ID: " << requestId 
                      << " (" << readableTime << ")" << std::endl;
            
            // 根据消息类型分发处理
            if (type == "measure_request") {
                // 处理测量请求
                json params = message.value("payload", json());
                handle_measure_request(hdl, requestId, params);
            } else if (type == "set_stream_mode") {
                // 处理设置取流模式请求
                json params = message.value("payload", json());
                handle_set_stream_mode(hdl, requestId, params);
            } else if (type == "get_stream_mode") {
                // 处理获取取流模式请求
                handle_get_stream_mode(hdl, requestId);
            } else if (type == "device_status") {
                // 处理获取设备状态请求
                handle_device_status(hdl, requestId);
            } else if (type == "calibrate") {
                // 处理校准请求
                json params = message.value("payload", json());
                handle_calibrate(hdl, requestId, params);
            } else {
                // 未知命令类型
                json response = {
                    {"type", "error"},
                    {"requestId", requestId},
                    {"error", "Unknown command type: " + type}
                };
                
                m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
            }
        } catch (json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        } catch (std::exception& e) {
            std::cerr << "Error processing message: " << e.what() << std::endl;
        }
    }

    void send_measuring_status(connection_hdl hdl, const std::string& requestId) {
        std::string readableTime = parse_timestamp_request_id(requestId);
        json response = {
            {"type", "measure_status"},
            {"requestId", requestId},
            {"status", "measuring"}
        };
        
        try {
            m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
            std::cout << "发送'正在测量'状态: " << requestId << " (" << readableTime << ")" << std::endl;
        } catch (std::exception& e) {
            std::cerr << "Error sending measuring status: " << e.what() << std::endl;
        }
    }

    void send_measurement_complete(connection_hdl hdl, const std::string& requestId, const json& params) {
        std::string readableTime = parse_timestamp_request_id(requestId);
        // 创建一个包含测量结果的响应
        json result;
        
        // 根据不同的测量模式和精度生成不同的结果
        if (params["mode"] == "standard") {
            result = {
                {"value", 42.5},
                {"unit", "mm"},
                {"precision", params["precision"]},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
            };
        } else if (params["mode"] == "quick") {
            result = {
                {"value", 37.2},
                {"unit", "mm"},
                {"precision", params["precision"]},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
            };
        } else if (params["mode"] == "detailed") {
            result = {
                {"value", 42.567},
                {"unit", "mm"},
                {"precision", params["precision"]},
                {"details", {
                    {"min", 42.1},
                    {"max", 43.0},
                    {"std_dev", 0.23}
                }},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
            };
        } else {
            // 默认结果
            result = {
                {"value", 0.0},
                {"unit", "mm"},
                {"error", "Unknown measurement mode"},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
            };
        }
        
        json response = {
            {"type", "measure_status"},
            {"requestId", requestId},
            {"status", "done"},
            {"result", result}
        };
        
        try {
            m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
            std::cout << "发送'测量完成'状态: " << requestId << " (" << readableTime << ")" << std::endl;
        } catch (std::exception& e) {
            std::cerr << "Error sending measurement complete: " << e.what() << std::endl;
        }
    }

    void start_measurement(connection_hdl hdl, const std::string& requestId, const json& params) {
        std::string readableTime = parse_timestamp_request_id(requestId);
        // 启动一个新线程来模拟测量过程
        std::thread([this, hdl, requestId, params, readableTime]() {
            // 根据不同的测量模式和精度，模拟不同的测量时间
            int delay_seconds = 2; // 默认延迟
            
            if (params["mode"] == "quick") {
                delay_seconds = 1;
            } else if (params["mode"] == "standard") {
                delay_seconds = 3;
            } else if (params["mode"] == "detailed") {
                delay_seconds = 5;
            }
            
            if (params["precision"] == "high" || params["precision"] == "very-high") {
                delay_seconds += 1;
            }
            
            std::cout << "处理测量请求: " << requestId 
                      << " (" << readableTime << "), "
                      << "延迟: " << delay_seconds << "秒" << std::endl;
            
            // 模拟测量过程需要一些时间
            std::this_thread::sleep_for(std::chrono::seconds(delay_seconds));
            
            // 测量完成，发送完成状态
            send_measurement_complete(hdl, requestId, params);
        }).detach();
    }

    server m_server;
    std::set<connection_hdl, std::owner_less<connection_hdl>> m_connections;
    std::string m_current_stream_mode; // 当前取流模式
    std::atomic<bool> m_is_calibrated{false}; // 是否已校准
};

int main() {
    DeviceServer server;
    
    std::cout << "===== 设备服务器 =====" << std::endl;
    std::cout << "支持的命令类型:" << std::endl;
    std::cout << "1. 测量命令 (measure_request)" << std::endl;
    std::cout << "2. 设置取流模式 (set_stream_mode)" << std::endl;
    std::cout << "3. 获取取流模式 (get_stream_mode)" << std::endl;
    std::cout << "4. 获取设备状态 (device_status)" << std::endl;
    std::cout << "5. 校准命令 (calibrate)" << std::endl;
    std::cout << "============================" << std::endl;
    
    server.run(9002);  // 在9002端口启动服务器
    return 0;
}
