#include "device_server.h"
#include "time_utils.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

DeviceServer::DeviceServer() {
    // 初始化WebSocket服务器
    m_server.init_asio();

    // 日志禁用
    m_server.set_access_channels(websocketpp::log::alevel::all);
    m_server.clear_access_channels(websocketpp::log::alevel::frame_payload |
                                   websocketpp::log::alevel::frame_header);

    // 设置消息处理回调
    m_server.set_message_handler(bind(&DeviceServer::onMessage, this, ::_1, ::_2));
    m_server.set_open_handler(bind(&DeviceServer::onOpen, this, ::_1));
    m_server.set_close_handler(bind(&DeviceServer::onClose, this, ::_1));
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
    } catch (websocketpp::exception const& e) {
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

        std::cout << "收到请求: [" << command << "], ID: " << requestId << " (" << readableTime
                  << ")" << std::endl;

        // 根据命令类型分发处理
        if (command == "setAlignViewMode") {
            json params = message.value("params", json());
            handleSetAlignViewMode(hdl, requestId, params);
        } else if (command == "getAlignViewMode") {
            handleGetAlignViewMode(hdl, requestId);
        } else if (command == "startStream") {
            json params = message.value("params", json());
            handleStartStream(hdl, requestId, params);
        } else if (command == "stopStream") {
            handleStopStream(hdl, requestId);
        } else if (command == "executeMeasure") {
            json params = message.value("params", json());
            handleExcuteMeasureRequest(hdl, requestId, params);
        } else if (command == "stopMeasure") {
            handleStopMeasure(hdl, requestId);
        } else if (command == "getMeasureStatus") {
            handleGetMeasureStatus(hdl, requestId);
        } else if (command == "getSufaceData") {
            // 处理获取设备状态请求
            handleGetMeasureStatus(hdl, requestId);
        } else {
            // 未知命令类型
            json response = {{"command", command},
                             {"requestId", requestId},
                             {"status", "error"},
                             {"errorMessage", "Unknown command: " + command}};

            m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        }
    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
    }
}

// 处理设置取流模式请求
void DeviceServer::handleSetAlignViewMode(connection_hdl hdl,
                                          const std::string& requestId,
                                          const json& params) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理设置观察模式请求: " << requestId << " (" << readableTime << ")" << std::endl;

    if (!params.contains("alignViewMode")) {
        // 发送错误响应
        json response = {{"command", "setAlignViewMode"},
                         {"requestId", requestId},
                         {"status", "error"},
                         {"errorMessage", "Missing alignViewMode parameter"}};

        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        return;
    }

    std::string mode = params["alignViewMode"];
    bool valid_mode = false;

    // 检查模式是否有效
    if (mode == "align" || mode == "view") {
        valid_mode = true;
    }

    if (valid_mode) {
        // 设置新的观察模式
        m_current_stream_mode = mode;

        // 发送成功响应
        json response = {{"command", "setAlignViewMode"},
                         {"requestId", requestId},
                         {"status", "success"},
                         {"data", {{"currentMode", mode}}}};

        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        std::cout << "观察模式已设置为: " << mode << std::endl;
    } else {
        // 发送错误响应
        json response = {
            {"command", "setAlignViewMode"},
            {"requestId", requestId},
            {"status", "error"},
            {"errorMessage", "Invalid mode: " + mode +
                                 ". Valid modes are: align, view, continuous, trigger, snapshot"}};

        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    }
}

// 处理获取观察模式请求
void DeviceServer::handleGetAlignViewMode(connection_hdl hdl, const std::string& requestId) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理获取观察模式请求: " << requestId << " (" << readableTime << ")" << std::endl;

    // 发送当前观察模式
    json response = {{"command", "getAlignViewMode"},
                     {"requestId", requestId},
                     {"status", "success"},
                     {"data", {{"alignViewMode", m_current_stream_mode}}}};

    m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
}

// 处理开始取流请求
void DeviceServer::handleStartStream(connection_hdl hdl,
                                     const std::string& requestId,
                                     const json& params) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理开始取流请求: " << requestId << " (" << readableTime << ")" << std::endl;

    if (m_is_streaming) {
        // 如果已经在取流中，返回错误
        json response = {{"command", "startStream"},
                         {"requestId", requestId},
                         {"status", "error"},
                         {"errorMessage", "Stream already running"}};

        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        return;
    }

    // 设置取流状态
    m_is_streaming = true;

    // 构建取流格式
    std::string format = params.value("format", "raw");

    // 返回成功响应
    json response = {{"command", "startStream"},
                     {"requestId", requestId},
                     {"status", "success"},
                     {"data",
                      {{"streamId", generateTimestampId()},
                       {"format", format},
                       {"mode", m_current_stream_mode}}}};

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
        json response = {{"command", "stopStream"},
                         {"requestId", requestId},
                         {"status", "error"},
                         {"errorMessage", "No active stream"}};

        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        return;
    }

    // 停止取流
    m_is_streaming = false;

    // 返回成功响应
    json response = {{"command", "stopStream"}, {"requestId", requestId}, {"status", "success"}};

    m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    std::cout << "取流已停止" << std::endl;
}

// 处理测量请求
void DeviceServer::handleExcuteMeasureRequest(connection_hdl hdl,
                                              const std::string& requestId,
                                              const json& params) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理测量请求: " << requestId << " (" << readableTime << ")" << std::endl;

    // 立即回复"正在测量"状态
    sendMeasuringStatus(hdl, requestId);

    // 启动模拟测量任务
    startMeasurement(hdl, requestId, params);
}

// 处理停止测量请求
void DeviceServer::handleStopMeasure(connection_hdl hdl, const std::string& requestId) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理停止测量请求: " << requestId << " (" << readableTime << ")" << std::endl;

    if (!m_is_measuring) {
        // 如果没有在测量，返回错误
        json response = {{"command", "stopMeasure"},
                         {"requestId", requestId},
                         {"status", "error"},
                         {"errorMessage", "No active measurement"}};

        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        return;
    }

    // 设置停止测量标志 - 这将导致测量线程检测到停止请求并退出
    m_is_measuring = false;

    // 返回成功响应
    json response = {{"command", "stopMeasure"}, 
                     {"requestId", requestId}, 
                     {"status", "success"},
                     {"data", {{"stopped", true}}}};

    m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    std::cout << "停止测量命令已发送" << std::endl;
}

// 处理获取设备状态请求
void DeviceServer::handleGetMeasureStatus(connection_hdl hdl, const std::string& requestId) {
    std::string readableTime = parseTimestampId(requestId);
    std::cout << "处理获取设备状态请求: " << requestId << " (" << readableTime << ")" << std::endl;

    bool isMeasuring = m_is_measuring;

    // 发送设备状态响应
    json response = {{"command", "getMeasureStatus"},
                     {"requestId", requestId},
                     {"status", "success"},
                     {"data", {{"isMeasuring", isMeasuring}}}};

    m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
}

void DeviceServer::handleGetSurfaceData(connection_hdl hdl, const std::string& requestId) {}

void DeviceServer::sendMeasuringStatus(connection_hdl hdl, const std::string& requestId) {
    std::string readableTime = parseTimestampId(requestId);
    json response = {
        {"command", "executeMeasure"}, {"requestId", requestId}, {"status", "pending"}};

    try {
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        std::cout << "发送'正在测量'状态: " << requestId << " (" << readableTime << ")"
                  << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Error sending measuring status: " << e.what() << std::endl;
    }
}

void DeviceServer::sendMeasurementComplete(connection_hdl hdl,
                                           const std::string& requestId,
                                           const json& params) {
    std::string readableTime = parseTimestampId(requestId);

    json response = {
        {"command", "executeMeasure"},
        {"requestId", requestId},
        {"status", "success"},
    };

    try {
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
        std::cout << "发送'测量完成'状态: " << requestId << " (" << readableTime << ")"
                  << std::endl;

        // 重置测量状态
        m_is_measuring = false;
    } catch (std::exception& e) {
        std::cerr << "Error sending measurement complete: " << e.what() << std::endl;
    }
}

void DeviceServer::startMeasurement(connection_hdl hdl,
                                    const std::string& requestId,
                                    const json& params) {
    std::string readableTime = parseTimestampId(requestId);
    // 启动一个新线程来模拟测量过程
    std::thread([this, hdl, requestId, params, readableTime]() {
        // 设置测量状态
        m_is_measuring = true;

        int delay_seconds = 5;  // 增加延迟时间，便于测试停止功能
        int check_interval_ms = 200; // 每200毫秒检查一次是否应该停止测量
        int elapsed_ms = 0; // 已经过去的毫秒数

        std::cout << "处理测量请求: " << requestId << " (" << readableTime << "), "
                  << "延迟: " << delay_seconds << "秒" << std::endl;

        // 模拟一个随机概率的超时情况（用于测试）
        bool simulate_timeout = (rand() % 100) < 5;  // 5%的概率模拟超时

        if (simulate_timeout) {
            // 模拟超时，但仍然支持提前停止
            while (elapsed_ms < 2000 && m_is_measuring) { // 2秒超时时间
                std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
                elapsed_ms += check_interval_ms;
                
                // 如果测量被停止，提前退出
                if (!m_is_measuring) {
                    std::cout << "测量已被中断 (超时模拟中): " << requestId << std::endl;
                    return;
                }
            }

            // 如果测量仍在进行中，则发送超时状态
            if (m_is_measuring) {
                // 发送超时状态
                json timeout_response = {{"command", "executeMeasure"},
                                        {"requestId", requestId},
                                        {"status", "timeout"},
                                        {"errorMessage", "Measurement operation timed out"}};

                m_server.send(hdl, timeout_response.dump(), websocketpp::frame::opcode::text);
                std::cout << "发送'测量超时'状态: " << requestId << " (" << readableTime << ")"
                        << std::endl;

                // 重置测量状态
                m_is_measuring = false;
            }
        } else {
            // 正常执行测量，支持提前停止
            int total_delay_ms = delay_seconds * 1000;
            
            while (elapsed_ms < total_delay_ms && m_is_measuring) {
                std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
                elapsed_ms += check_interval_ms;
                
                // 如果测量被停止，提前退出
                if (!m_is_measuring) {
                    std::cout << "测量已被中断: " << requestId << std::endl;
                    
                    // 发送测量中断消息
                    json interrupted_response = {
                        {"command", "executeMeasure"},
                        {"requestId", requestId},
                        {"status", "error"},
                        {"errorMessage", "Measurement was interrupted"}
                    };
                    
                    m_server.send(hdl, interrupted_response.dump(), websocketpp::frame::opcode::text);
                    return;
                }
                
                // 可选：发送进度更新消息
                if (elapsed_ms % 1000 == 0) { // 每秒发送一次进度更新
                    int progress = (elapsed_ms * 100) / total_delay_ms;
                    std::cout << "测量进度: " << progress << "% 完成" << std::endl;
                    
                    // 如果需要向客户端发送进度更新，可以在这里添加代码
                }
            }

            // 如果测量完成（未被中断），发送完成状态
            if (m_is_measuring) {
                // 测量完成，发送完成状态
                sendMeasurementComplete(hdl, requestId, params);
            }
        }
    }).detach();
}
