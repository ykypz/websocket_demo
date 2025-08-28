#ifndef DEVICE_CLIENT_H
#define DEVICE_CLIENT_H

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_STL_

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include "command_types.h"
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <map>
#include <future>
#include <functional>

// 使用asio作为底层网络库
typedef websocketpp::client<websocketpp::config::asio_client> websocket_client;

// 消息处理回调函数的类型
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

// 连接句柄类型
typedef websocketpp::connection_hdl connection_hdl;

class DeviceClient {
private:
    // 请求结构体，用于存储请求信息
    struct PendingRequest {
        CommandType cmdType;                          // 命令类型
        std::shared_ptr<std::promise<void>> promise;  // 用于异步等待的promise
        std::shared_ptr<CommandResult> result;        // 命令执行结果
        bool isBlocking = false;                      // 是否为阻塞模式
        bool pendingReceived = false;                 // 是否已收到pending响应
    };

    // 迭代器类型定义
    using PendingRequestsIterator = std::map<std::string, PendingRequest>::iterator;

public:
    DeviceClient();
    ~DeviceClient();

    // 连接到服务器
    bool connect(const std::string &uri);
    // 关闭连接
    void close();

    // 设置视频流模式
    CommandResult setAlignViewMode(const std::string &mode);
    // 获取视频流模式
    CommandResult getAlignViewMode();
    // 开始视频流
    CommandResult startStream();
    // 停止视频流
    CommandResult stopStream();
    // 开始测量
    // isBlocking:
    //   - true: 函数会等待测量完成，直到收到success或error响应后返回
    //   - false: 函数在收到pending响应后立即返回，可以通过getMeasureStatus检查测量状态
    CommandResult executeMeasurement(bool isBlocking = false);
    // 停止测量
    CommandResult stopMeasure();
    // 查询测量状态
    CommandResult getMeasureStatus();
    // 获取面形数据
    CommandResult getSurfaceData();

    // 发送通用命令并等待响应
    CommandResult sendCommand(CommandType cmdType,
                              const json &params = json(),
                              int timeout_sec = 3);

    // 发送可能需要长时间处理的命令并支持阻塞/非阻塞模式
    // isBlocking:
    //   - true: 阻塞模式，等待完整操作完成（成功/失败），适用于需要等待最终结果的场景
    //   - false: 非阻塞模式，收到pending状态后立即返回，适用于启动长时间操作后不需要等待结果的场景
    CommandResult sendBlockingCommand(CommandType cmdType,
                                      const json &params = json(),
                                      int timeout_sec = 30,
                                      bool isBlocking = false);

private:
    void onOpen(connection_hdl hdl);
    void onClose(connection_hdl hdl);
    void onFail(connection_hdl hdl);
    void onMessage(connection_hdl hdl, message_ptr msg);

    // 处理测量命令的响应
    void handleMeasureResponse(PendingRequestsIterator it, const json &message);

    // 处理取流模式命令的响应
    void handleStreamModeResponse(PendingRequestsIterator it, const json &message);

    // 处理设备状态命令的响应
    void handleDeviceStatusResponse(PendingRequestsIterator it, const json &message);

    // 处理通用响应
    void handleGenericResponse(PendingRequestsIterator it, const json &message);

    websocket_client m_client;
    connection_hdl m_hdl;
    std::thread m_thread;
    bool m_connected = false;
    bool m_done;

    // 请求映射表，存储每个请求ID对应的结果、promise和命令类型
    std::mutex m_pending_mutex;
    std::map<std::string, PendingRequest> m_pending_requests;
};

#endif  // DEVICE_CLIENT_H
