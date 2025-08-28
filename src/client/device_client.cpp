#include "device_client.h"
#include "time_utils.h"
#include <iostream>
#include <chrono>

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

DeviceClient::DeviceClient()
    : m_done(false) {
    // 初始化WebSocket客户端
    m_client.init_asio();

    // 完全禁用所有日志通道
    m_client.clear_access_channels(websocketpp::log::alevel::all);
    m_client.clear_error_channels(websocketpp::log::elevel::all);
    // 明确设置为不记录任何日志
    m_client.set_access_channels(websocketpp::log::alevel::none);
    m_client.set_error_channels(websocketpp::log::elevel::none);

    // 设置消息处理回调
    m_client.set_message_handler(bind(&DeviceClient::onMessage, this, ::_1, ::_2));
    m_client.set_open_handler(bind(&DeviceClient::onOpen, this, ::_1));
    m_client.set_close_handler(bind(&DeviceClient::onClose, this, ::_1));
    m_client.set_fail_handler(bind(&DeviceClient::onFail, this, ::_1));
}

DeviceClient::~DeviceClient() { close(); }

// 连接到服务器
bool DeviceClient::connect(const std::string& uri) {
    try {
        websocketpp::lib::error_code ec;
        websocket_client::connection_ptr con = m_client.get_connection(uri, ec);

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

// 发送通用命令并等待响应
CommandResult DeviceClient::sendCommand(CommandType cmdType, const json& params, int timeout_sec) {
    if (!m_connected) {
        std::cerr << "Not connected to server" << std::endl;
        return {false, true, json(), cmdType, "Not connected to server"};
    }

    // 生成请求ID
    std::string requestId = generateTimestampId();

    // 创建命令请求消息
    json request = {
        {"command", commandTypeToString(cmdType)}, {"requestId", requestId}, {"params", params}};

    // 创建等待结果对象并保存到映射表中
    auto result = std::make_shared<CommandResult>();
    result->type = cmdType;
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    // 保存到请求映射表中
    {
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        PendingRequest req;
        req.result = result;
        req.promise = promise;
        req.cmdType = cmdType;
        req.isBlocking = true;  // 普通命令总是阻塞的
        m_pending_requests[requestId] = req;
    }

    // 发送请求
    try {
        m_client.send(m_hdl, request.dump(), websocketpp::frame::opcode::text);
        std::cout << "Sent " << commandTypeToString(cmdType) << " request with ID: " << requestId
                  << std::endl;
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
            m_pending_requests[requestId].result->timeout = true;
            m_pending_requests[requestId].result->errorMessage = "Request timed out";
            auto temp_result = *m_pending_requests[requestId].result;
            m_pending_requests.erase(requestId);
            return temp_result;
        }
    }

    // 获取结果并从映射表中移除请求
    std::lock_guard<std::mutex> lock(m_pending_mutex);
    if (m_pending_requests.count(requestId) > 0) {
        auto temp_result = *m_pending_requests[requestId].result;
        m_pending_requests.erase(requestId);
        return temp_result;
    } else {
        // 请求已被处理，可能是在我们检查超时状态后，结果恰好被其他线程处理了
        return {false, true, json(), cmdType, "Request not found in pending requests"};
    }
}

// 发送可能需要长时间处理的命令并支持阻塞/非阻塞模式
CommandResult DeviceClient::sendBlockingCommand(CommandType cmdType,
                                                const json& params,
                                                int timeout_sec,
                                                bool isBlocking) {
    if (!m_connected) {
        std::cerr << "Not connected to server" << std::endl;
        return {false, true, json(), cmdType, "Not connected to server"};
    }

    // 生成请求ID
    std::string requestId = generateTimestampId();

    // 创建命令请求消息
    json request = {
        {"command", commandTypeToString(cmdType)}, {"requestId", requestId}, {"params", params}};

    // 创建等待结果对象并保存到映射表中
    auto result = std::make_shared<CommandResult>();
    result->type = cmdType;
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    // 保存到请求映射表中
    {
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        PendingRequest req;
        req.result = result;
        req.promise = promise;
        req.cmdType = cmdType;
        req.isBlocking = isBlocking;  // 记录是否为阻塞模式
        m_pending_requests[requestId] = req;
    }

    // 发送请求
    try {
        m_client.send(m_hdl, request.dump(), websocketpp::frame::opcode::text);
        std::cout << "Sent " << commandTypeToString(cmdType) << " request with ID: " << requestId
                  << (isBlocking ? " (blocking mode)" : " (non-blocking mode)") << std::endl;
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
            m_pending_requests[requestId].result->timeout = true;
            m_pending_requests[requestId].result->errorMessage = "Request timed out";
            auto temp_result = *m_pending_requests[requestId].result;
            m_pending_requests.erase(requestId);
            return temp_result;
        }
    }

    // 获取结果并从映射表中移除请求
    std::lock_guard<std::mutex> lock(m_pending_mutex);
    if (m_pending_requests.count(requestId) > 0) {
        auto temp_result = *m_pending_requests[requestId].result;
        m_pending_requests.erase(requestId);
        return temp_result;
    } else {
        // 请求已被处理，可能是在我们检查超时状态后，结果恰好被其他线程处理了
        return {false, true, json(), cmdType, "Request not found in pending requests"};
    }
}

// 设置视频流模式
CommandResult DeviceClient::setAlignViewMode(const std::string& mode) {
    json params = {{"alignViewMode", mode}};
    return sendCommand(CommandType::SetAlignViewMode, params);
}

// 获取视频流模式
CommandResult DeviceClient::getAlignViewMode() {
    return sendCommand(CommandType::GetAlignViewMode);
}

// 开始视频流
CommandResult DeviceClient::startStream() { return sendCommand(CommandType::StartStream); }

// 停止视频流
CommandResult DeviceClient::stopStream() { return sendCommand(CommandType::StopStream); }

// 开始测量
CommandResult DeviceClient::executeMeasurement(bool isBlocking) {
    // 使用sendBlockingCommand，支持阻塞/非阻塞模式
    return sendBlockingCommand(CommandType::ExcuteMeasurement, json(), 30, isBlocking);
}

// 停止测量
CommandResult DeviceClient::stopMeasure() { return sendCommand(CommandType::StopMeasure); }

// 查询测量状态
CommandResult DeviceClient::getMeasureStatus() {
    return sendCommand(CommandType::GetMeasureStatus);
}

// 获取面形数据
CommandResult DeviceClient::getSurfaceData() { return sendCommand(CommandType::GetSurfaceData); }

// 关闭连接
void DeviceClient::close() {
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
            request.second.result->timeout = true;
            request.second.result->errorMessage = "Connection closed";
            request.second.promise->set_value();
        }
        m_pending_requests.clear();
    }

    m_done = true;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void DeviceClient::onOpen(connection_hdl hdl) {
    std::cout << "Connection opened" << std::endl;
    m_connected = true;
}

void DeviceClient::onClose(connection_hdl hdl) {
    std::cout << "Connection closed" << std::endl;
    m_connected = false;
}

void DeviceClient::onFail(connection_hdl hdl) {
    std::cout << "Connection failed" << std::endl;
    m_connected = false;
}

void DeviceClient::onMessage(connection_hdl hdl, message_ptr msg) {
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
                handleMeasureResponse(it, message);
            } else if (msgType == "setAlignViewMode" || msgType == "getAlignViewMode") {
                // 处理观察模式命令的响应
                handleStreamModeResponse(it, message);
            } else if (msgType == "getDeviceStatus") {
                // 处理设备状态命令的响应
                handleDeviceStatusResponse(it, message);
            } else {
                // 通用响应处理
                handleGenericResponse(it, message);
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
void DeviceClient::handleMeasureResponse(PendingRequestsIterator it, const json& message) {
    if (!message.contains("status")) {
        return;
    }

    std::string status = message["status"];
    std::string requestId = it->first;
    bool isBlocking = it->second.isBlocking;

    if (status == "pending") {
        // 收到"正在处理"的状态
        std::cout << "Measurement in progress for request: " << requestId << std::endl;

        // 设置结果状态为"已接收但处理中"
        it->second.result->completed = true;  // 表示命令已被成功接收
        it->second.result->timeout = false;
        it->second.result->data = {{"status", "pending"},
                                   {"message", "Measurement request accepted and in progress"}};

        // 标记已收到pending响应
        it->second.pendingReceived = true;

        // 如果是非阻塞模式，通知等待线程并移除请求
        if (!isBlocking) {
            // 通知等待线程，让sendBlockingCommand方法返回
            it->second.promise->set_value();

            // 从映射表中移除请求（非阻塞模式下，此时已经完成请求处理）
            m_pending_requests.erase(it);
        }
        // 如果是阻塞模式，不通知等待线程，继续等待最终结果
    } else if (status == "success") {
        // 收到"成功"的状态
        std::cout << "Measurement completed successfully for request: " << requestId << std::endl;

        // 检查请求是否仍在映射表中
        if (m_pending_requests.count(requestId) > 0) {
            // 设置结果
            it->second.result->completed = true;
            if (message.contains("data")) {
                it->second.result->data = message["data"];
            }

            // 通知等待线程
            it->second.promise->set_value();

            // 从映射表中移除请求
            m_pending_requests.erase(it);
        }
    } else if (status == "error") {
        // 处理错误状态
        std::cout << "Measurement error for request: " << requestId;
        if (message.contains("errorMessage")) {
            std::cout << " - " << message["errorMessage"].get<std::string>();
        }
        std::cout << std::endl;

        // 检查请求是否仍在映射表中
        if (m_pending_requests.count(requestId) > 0) {
            // 设置错误结果
            it->second.result->completed = false;
            if (message.contains("errorMessage")) {
                it->second.result->errorMessage = message["errorMessage"];
            } else {
                it->second.result->errorMessage = "Unknown error";
            }

            // 通知等待线程
            it->second.promise->set_value();

            // 从映射表中移除请求
            m_pending_requests.erase(it);
        }
    } else if (status == "timeout") {
        // 处理超时状态
        std::cout << "Measurement timeout for request: " << requestId << std::endl;

        // 检查请求是否仍在映射表中
        if (m_pending_requests.count(requestId) > 0) {
            // 设置超时结果
            it->second.result->completed = false;
            it->second.result->timeout = true;
            it->second.result->errorMessage = "Measurement operation timed out";

            // 通知等待线程
            it->second.promise->set_value();

            // 从映射表中移除请求
            m_pending_requests.erase(it);
        }
    }
}

// 处理取流模式命令的响应
void DeviceClient::handleStreamModeResponse(PendingRequestsIterator it, const json& message) {
    if (!message.contains("status")) {
        return;
    }

    std::string status = message["status"];
    if (status == "success") {
        // 设置或获取取流模式成功
        it->second.result->completed = true;

        if (it->second.cmdType == CommandType::SetAlignViewMode) {
            // 设置取流模式的响应
            if (message.contains("data") && message["data"].contains("currentMode")) {
                it->second.result->data["mode"] = message["data"]["currentMode"];
            }
        } else if (it->second.cmdType == CommandType::GetAlignViewMode) {
            // 获取取流模式的响应
            if (message.contains("data") && message["data"].contains("mode")) {
                it->second.result->data["mode"] = message["data"]["mode"];
            }
        }

        // 通知等待线程
        it->second.promise->set_value();

        std::cout << "Stream mode operation successful for request: " << it->first << std::endl;
    } else if (status == "error") {
        // 处理错误状态
        it->second.result->completed = false;
        if (message.contains("errorMessage")) {
            it->second.result->errorMessage = message["errorMessage"];
        } else {
            it->second.result->errorMessage = "Unknown error";
        }

        // 通知等待线程
        it->second.promise->set_value();

        std::cout << "Stream mode operation error for request: " << it->first << " - "
                  << it->second.result->errorMessage << std::endl;
    } else if (status == "timeout") {
        // 处理超时状态
        it->second.result->completed = false;
        it->second.result->timeout = true;
        it->second.result->errorMessage = "Stream mode operation timed out";

        // 通知等待线程
        it->second.promise->set_value();

        std::cout << "Stream mode operation timeout for request: " << it->first << std::endl;
    }
}

// 处理设备状态命令的响应
void DeviceClient::handleDeviceStatusResponse(PendingRequestsIterator it, const json& message) {
    if (!message.contains("status")) {
        return;
    }

    std::string status = message["status"];
    if (status == "success") {
        // 获取设备状态成功
        it->second.result->completed = true;
        if (message.contains("data")) {
            it->second.result->data = message["data"];
        }

        // 通知等待线程
        it->second.promise->set_value();

        std::cout << "Device status query successful for request: " << it->first << std::endl;
    } else if (status == "error") {
        // 处理错误状态
        it->second.result->completed = false;
        if (message.contains("errorMessage")) {
            it->second.result->errorMessage = message["errorMessage"];
        } else {
            it->second.result->errorMessage = "Unknown error";
        }

        // 通知等待线程
        it->second.promise->set_value();

        std::cout << "Device status query error for request: " << it->first << " - "
                  << it->second.result->errorMessage << std::endl;
    } else if (status == "timeout") {
        // 处理超时状态
        it->second.result->completed = false;
        it->second.result->timeout = true;
        it->second.result->errorMessage = "Device status query timed out";

        // 通知等待线程
        it->second.promise->set_value();

        std::cout << "Device status query timeout for request: " << it->first << std::endl;
    }
}

// 处理通用响应
void DeviceClient::handleGenericResponse(PendingRequestsIterator it, const json& message) {
    if (!message.contains("status")) {
        // 如果没有状态字段，尝试解析旧格式的消息
        it->second.result->completed = true;
        it->second.result->data = message;

        // 通知等待线程
        it->second.promise->set_value();
        return;
    }

    std::string status = message["status"];
    if (status == "success") {
        // 操作成功
        it->second.result->completed = true;
        if (message.contains("data")) {
            it->second.result->data = message["data"];
        }

        // 通知等待线程
        it->second.promise->set_value();

        std::cout << "Operation successful for request: " << it->first << std::endl;
    } else if (status == "error") {
        // 处理错误状态
        it->second.result->completed = false;
        if (message.contains("errorMessage")) {
            it->second.result->errorMessage = message["errorMessage"];
        } else {
            it->second.result->errorMessage = "Unknown error";
        }

        // 通知等待线程
        it->second.promise->set_value();

        std::cout << "Operation error for request: " << it->first << " - "
                  << it->second.result->errorMessage << std::endl;
    } else if (status == "pending") {
        // 操作正在进行中，可以记录但不通知等待线程
        std::cout << "Operation pending for request: " << it->first << std::endl;
    } else if (status == "timeout") {
        // 处理超时状态
        it->second.result->completed = false;
        it->second.result->timeout = true;
        it->second.result->errorMessage = "Operation timed out";

        // 通知等待线程
        it->second.promise->set_value();

        std::cout << "Operation timeout for request: " << it->first << std::endl;
    } else {
        // 未知状态
        it->second.result->completed = false;
        it->second.result->errorMessage = "Unknown status: " + status;

        // 通知等待线程
        it->second.promise->set_value();

        std::cout << "Unknown status for request: " << it->first << " - " << status << std::endl;
    }
}
