#ifndef DEVICE_SERVER_H
#define DEVICE_SERVER_H

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_STL_

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <atomic>
#include <functional>

using json = nlohmann::json;

// 使用asio作为底层网络库
typedef websocketpp::server<websocketpp::config::asio> websocket_server;

// 消息处理回调函数的类型
typedef websocket_server::message_ptr message_ptr;

// 连接句柄类型
typedef websocketpp::connection_hdl connection_hdl;

class DeviceServer {
public:
    DeviceServer();
    
    // 运行服务器
    void run(uint16_t port);
    
private:
    void onOpen(connection_hdl hdl);
    void onClose(connection_hdl hdl);
    void onMessage(connection_hdl hdl, message_ptr msg);
    
    // 处理测量请求
    void handleMeasureRequest(connection_hdl hdl, const std::string& requestId, const json& params);
    
    // 处理设置取流模式请求
    void handleSetStreamMode(connection_hdl hdl, const std::string& requestId, const json& params);
    
    // 处理获取观察模式请求
    void handleGetStreamMode(connection_hdl hdl, const std::string& requestId);
    
    // 处理获取设备状态请求
    void handleMeasureStatus(connection_hdl hdl, const std::string& requestId);
    
    // 处理校准请求
    void handleCalibrate(connection_hdl hdl, const std::string& requestId, const json& params);
    
    // 处理开始取流请求
    void handleStartStream(connection_hdl hdl, const std::string& requestId, const json& params);
    
    // 处理停止取流请求
    void handleStopStream(connection_hdl hdl, const std::string& requestId);
    
    // 处理停止测量请求
    void handleStopMeasure(connection_hdl hdl, const std::string& requestId);
    
    void sendMeasuringStatus(connection_hdl hdl, const std::string& requestId);
    void sendMeasurementComplete(connection_hdl hdl, const std::string& requestId, const json& params);
    void startMeasurement(connection_hdl hdl, const std::string& requestId, const json& params);

    websocket_server m_server;
    std::set<connection_hdl, std::owner_less<connection_hdl>> m_connections;
    std::string m_current_stream_mode; // 当前取流模式
    std::atomic<bool> m_is_calibrated{false}; // 是否已校准
    std::atomic<bool> m_is_streaming{false}; // 是否正在取流
    std::atomic<bool> m_is_measuring{false}; // 是否正在测量
};

#endif // DEVICE_SERVER_H
