#include "device_server.h"
#include "time_utils.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

DeviceServer::DeviceServer() {
    // 初始化WebSocket服务器
    m_server.init_asio();

    // 设置日志级别
    m_server.set_access_channels(websocketpp::log::alevel::all);
    m_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

    // 设置消息处理回调
    m_server.set_message_handler(bind(&DeviceServer::onMessage, this, ::_1, ::_2));
    m_server.set_open_handler(bind(&DeviceServer::onOpen, this, ::_1));
    m_server.set_close_handler(bind(&DeviceServer::onClose, this, ::_1));
    
    // 初始化默认流模式
    m_current_stream_mode = "continuous";
}

void DeviceServer::run(uint16_t port) {
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

void DeviceServer::onOpen(connection_hdl hdl) {
    std::cout << "Connection opened" << std::endl;
    m_connections.insert(hdl);
}

void DeviceServer::onClose(connection_hdl hdl) {
    std::cout << "Connection closed" << std::endl;
    m_connections.erase(hdl);
}

void DeviceServer::onMessage(connection_hdl hdl, message_ptr msg) {
    try {
        // 解析JSON消息
        std::string payload = msg->get_payload();
        json message = json::parse(payload);

        // 检查消息格式
        if (!message.contains("command") || !message.contains("requestId")) {
            std::cerr << "Invalid message format" << std::endl;
            return;
        }
        
        std::string command = message["command"];
        std::string requestId = message["requestId"];
        std::string readableTime = parseTimestampId(requestId);
        
        std::cout << "收到请求: [" << command << "], ID: " << requestId 
                  << " (" << readableTime << ")" << std::endl;
        
        // 根据命令类型分发处理
        if (command == "executeMeasure") {
            // 处理测量请求
            json params = message.value("params", json());
            handleMeasureRequest(hdl, requestId, params);
        } else if (command == "setAlignViewMode") {
            // 处理设置观察模式请求
            json params = message.value("params", json());
            handleSetStreamMode(hdl, requestId, params);
        } else if (command == "getAlignViewMode") {
            // 处理获取观察模式请求
            handleGetStreamMode(hdl, requestId);
        } else if (command == "getMeasureStatus") {
            // 处理获取设备状态请求
            handleDeviceStatus(hdl, requestId);
        } else if (command == "startStream") {
            // 处理开始取流请求
            json params = message.value("params", json());
            handleStartStream(hdl, requestId, params);
        } else if (command == "stopStream") {
            // 处理停止取流请求
            handleStopStream(hdl, requestId);
        } else if (command == "stopMeasure") {
            // 处理停止测量请求
            handleStopMeasure(hdl, requestId);
        } else {
            // 未知命令类型
            json response = {
                {"command", command},
                {"requestId", requestId},
                {"status", "error"},
                {"errorMessage", "Unknown command: " + command}
            };
            
            m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        }
    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
    }
}

// 处理测量请求
void DeviceServer::handleMeasureRequest(connection_hdl hdl, const std::string& requestId, const json& params) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理测量请求: " << requestId << " (" << readableTime << ")" << std::endl;
    
    // 立即回复"正在测量"状态
    sendMeasuringStatus(hdl, requestId);
    
    // 启动模拟测量任务
    startMeasurement(hdl, requestId, params);
}

// 处理设置取流模式请求
void DeviceServer::handleSetStreamMode(connection_hdl hdl, const std::string& requestId, const json& params) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理设置观察模式请求: " << requestId << " (" << readableTime << ")" << std::endl;
    
    if (!params.contains("alignViewMode")) {
        // 发送错误响应
        json response = {
            {"command", "setAlignViewMode"},
            {"requestId", requestId},
            {"status", "error"},
            {"errorMessage", "Missing alignViewMode parameter"}
        };
        
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        return;
    }
    
    std::string mode = params["alignViewMode"];
    bool valid_mode = false;
    
    // 检查模式是否有效
    if (mode == "align" || mode == "view" || mode == "continuous" || mode == "trigger" || mode == "snapshot") {
        valid_mode = true;
    }
    
    if (valid_mode) {
        // 设置新的观察模式
        m_current_stream_mode = mode;
        
        // 发送成功响应
        json response = {
            {"command", "setAlignViewMode"},
            {"requestId", requestId},
            {"status", "success"},
            {"data", {
                {"currentMode", mode}
            }}
        };
        
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        std::cout << "观察模式已设置为: " << mode << std::endl;
    } else {
        // 发送错误响应
        json response = {
            {"command", "setAlignViewMode"},
            {"requestId", requestId},
            {"status", "error"},
            {"errorMessage", "Invalid mode: " + mode + ". Valid modes are: align, view, continuous, trigger, snapshot"}
        };
        
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    }
}

// 处理获取观察模式请求
void DeviceServer::handleGetStreamMode(connection_hdl hdl, const std::string& requestId) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理获取观察模式请求: " << requestId << " (" << readableTime << ")" << std::endl;
    
    // 发送当前观察模式
    json response = {
        {"command", "getAlignViewMode"},
        {"requestId", requestId},
        {"status", "success"},
        {"data", {
            {"mode", m_current_stream_mode}
        }}
    };
    
    m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
}

// 处理获取设备状态请求
void DeviceServer::handleDeviceStatus(connection_hdl hdl, const std::string& requestId) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理获取设备状态请求: " << requestId << " (" << readableTime << ")" << std::endl;
    
    // 模拟设备状态信息
    json deviceStatus = {
        {"deviceId", "DEV12345"},
        {"firmwareVersion", "2.5.1"},
        {"temperature", 36.7},
        {"uptime", 12345},
        {"alignViewMode", m_current_stream_mode},
        {"isCalibrated", static_cast<bool>(m_is_calibrated)},
        {"isStreaming", static_cast<bool>(m_is_streaming)},
        {"isMeasuring", static_cast<bool>(m_is_measuring)},
        {"battery", 85}
    };
    
    // 发送设备状态响应
    json response = {
        {"command", "getMeasureStatus"},
        {"requestId", requestId},
        {"status", "success"},
        {"data", {
            {"deviceStatus", deviceStatus}
        }}
    };
    
    m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
}

// 处理校准请求 (这里我们将其作为一种特殊的测量请求处理)
void DeviceServer::handleCalibrate(connection_hdl hdl, const std::string& requestId, const json& params) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理校准请求: " << requestId << " (" << readableTime << ")" << std::endl;
    
    // 立即发送校准开始状态
    json start_response = {
        {"command", "executeMeasure"},
        {"requestId", requestId},
        {"status", "pending"},
        {"data", {
            {"progress", 0},
            {"calibration", true}
        }}
    };
    
    m_server.send(hdl, start_response.dump(), websocketpp::frame::opcode::text);
    
    // 启动校准任务
    std::thread([this, hdl, requestId, params]() {
        // 模拟校准过程
        std::string calibrationType = params.value("type", "standard");
        int steps = (calibrationType == "full") ? 5 : 3;
        
        m_is_calibrated = false;
        
        for (int i = 1; i <= steps; i++) {
            // 每步暂停一段时间
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 发送进度更新
            int progress = (i * 100) / steps;
            json progress_response = {
                {"command", "executeMeasure"},
                {"requestId", requestId},
                {"status", "pending"},
                {"data", {
                    {"progress", progress},
                    {"calibration", true}
                }}
            };
            
            m_server.send(hdl, progress_response.dump(), websocketpp::frame::opcode::text);
        }
        
        // 校准完成
        m_is_calibrated = true;
        
        json result = {
            {"calibrationType", calibrationType},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"offset", 0.05},
            {"calibration", true}
        };
        
        json complete_response = {
            {"command", "executeMeasure"},
            {"requestId", requestId},
            {"status", "success"},
            {"data", result}
        };
        
        m_server.send(hdl, complete_response.dump(), websocketpp::frame::opcode::text);
        std::cout << "校准完成: " << requestId << std::endl;
    }).detach();
}

// 处理开始取流请求
void DeviceServer::handleStartStream(connection_hdl hdl, const std::string& requestId, const json& params) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理开始取流请求: " << requestId << " (" << readableTime << ")" << std::endl;
    
    if (m_is_streaming) {
        // 如果已经在取流中，返回错误
        json response = {
            {"command", "startStream"},
            {"requestId", requestId},
            {"status", "error"},
            {"errorMessage", "Stream already running"}
        };
        
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        return;
    }
    
    // 设置取流状态
    m_is_streaming = true;
    
    // 构建取流格式
    std::string format = params.value("format", "raw");
    
    // 返回成功响应
    json response = {
        {"command", "startStream"},
        {"requestId", requestId},
        {"status", "success"},
        {"data", {
            {"streamId", generateTimestampId()},
            {"format", format},
            {"mode", m_current_stream_mode}
        }}
    };
    
    m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    
    // 在真实实现中，这里会开启一个线程持续发送数据流
    // 为了演示，我们不实际发送数据
    std::cout << "开始取流，格式: " << format << ", 模式: " << m_current_stream_mode << std::endl;
}

// 处理停止取流请求
void DeviceServer::handleStopStream(connection_hdl hdl, const std::string& requestId) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理停止取流请求: " << requestId << " (" << readableTime << ")" << std::endl;
    
    if (!m_is_streaming) {
        // 如果没有在取流，返回错误
        json response = {
            {"command", "stopStream"},
            {"requestId", requestId},
            {"status", "error"},
            {"errorMessage", "No active stream"}
        };
        
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        return;
    }
    
    // 停止取流
    m_is_streaming = false;
    
    // 返回成功响应
    json response = {
        {"command", "stopStream"},
        {"requestId", requestId},
        {"status", "success"}
    };
    
    m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    std::cout << "取流已停止" << std::endl;
}

// 处理停止测量请求
void DeviceServer::handleStopMeasure(connection_hdl hdl, const std::string& requestId) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理停止测量请求: " << requestId << " (" << readableTime << ")" << std::endl;
    
    if (!m_is_measuring) {
        // 如果没有在测量，返回错误
        json response = {
            {"command", "stopMeasure"},
            {"requestId", requestId},
            {"status", "error"},
            {"errorMessage", "No active measurement"}
        };
        
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        return;
    }
    
    // 停止测量
    m_is_measuring = false;
    
    // 返回成功响应
    json response = {
        {"command", "stopMeasure"},
        {"requestId", requestId},
        {"status", "success"}
    };
    
    m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    std::cout << "测量已停止" << std::endl;
}

void DeviceServer::sendMeasuringStatus(connection_hdl hdl, const std::string& requestId) {
    std::string readableTime = parseTimestampId(requestId);
    json response = {
        {"command", "executeMeasure"},
        {"requestId", requestId},
        {"status", "pending"}
    };
    
    try {
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        std::cout << "发送'正在测量'状态: " << requestId << " (" << readableTime << ")" << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Error sending measuring status: " << e.what() << std::endl;
    }
}

void DeviceServer::sendMeasurementComplete(connection_hdl hdl, const std::string& requestId, const json& params) {
    std::string readableTime = parseTimestampId(requestId);
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
                {"stdDev", 0.23}
            }},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
        };
    } else {
        // 默认结果
        result = {
            {"value", 0.0},
            {"unit", "mm"},
            {"errorMessage", "Unknown measurement mode"},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
        };
    }
    
    json response = {
        {"command", "executeMeasure"},
        {"requestId", requestId},
        {"status", "success"},
        {"data", result}
    };
    
    try {
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        std::cout << "发送'测量完成'状态: " << requestId << " (" << readableTime << ")" << std::endl;
        
        // 重置测量状态
        m_is_measuring = false;
    } catch (std::exception& e) {
        std::cerr << "Error sending measurement complete: " << e.what() << std::endl;
    }
}

void DeviceServer::startMeasurement(connection_hdl hdl, const std::string& requestId, const json& params) {
    std::string readableTime = parseTimestampId(requestId);
    // 启动一个新线程来模拟测量过程
    std::thread([this, hdl, requestId, params, readableTime]() {
        // 设置测量状态
        m_is_measuring = true;
        
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
        
        // 模拟一个随机概率的超时情况（用于测试）
        bool simulate_timeout = (rand() % 100) < 5; // 5%的概率模拟超时
        
        if (simulate_timeout) {
            // 模拟超时
            std::this_thread::sleep_for(std::chrono::seconds(2)); // 短暂延迟
            
            // 发送超时状态
            json timeout_response = {
                {"command", "executeMeasure"},
                {"requestId", requestId},
                {"status", "timeout"},
                {"errorMessage", "Measurement operation timed out"}
            };
            
            m_server.send(hdl, timeout_response.dump(), websocketpp::frame::opcode::text);
            std::cout << "发送'测量超时'状态: " << requestId << " (" << readableTime << ")" << std::endl;
            
            // 重置测量状态
            m_is_measuring = false;
        } else {
            // 正常执行测量
            // 模拟测量过程需要一些时间
            std::this_thread::sleep_for(std::chrono::seconds(delay_seconds));
            
            // 测量完成，发送完成状态
            sendMeasurementComplete(hdl, requestId, params);
        }
    }).detach();
}
