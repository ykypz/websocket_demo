# WebSocket 测量通信协议演示

这个项目实现了一个基于WebSocket的测量通信协议，使用C++、websocketpp和nlohmann/json库。

## 协议说明

1. 客户端发送测量命令：

```json
{
  "type": "measure_request",
  "requestId": "xxxxxx", // 唯一标识本次请求
  "payload": {
    // 具体测量参数
  }
}
```

1. 服务端收到后，立即反馈"正在测量"：

```json
{
  "type": "measure_status",
  "requestId": "xxxxxx",
  "status": "measuring"
}
```

1. 测量完成后，服务端反馈"完成测量"：

```json
{
  "type": "measure_status",
  "requestId": "xxxxxx",
  "status": "done",
  "result": {
    // 测量结果数据
  }
}
```

## 项目结构

- `server/` - 服务端代码
- `client/` - 客户端代码
- `CMakeLists.txt` - CMake构建文件

## 依赖项

- [websocketpp](https://github.com/zaphoyd/websocketpp) - C++的WebSocket实现
- [nlohmann/json](https://github.com/nlohmann/json) - 现代C++ JSON库
- CMake 3.10+ - 构建系统
- C++14兼容的编译器

## 设置开发环境

1. 克隆项目后，创建extern目录并下载依赖库：

```bash
mkdir -p extern
cd extern

# 下载websocketpp
git clone https://github.com/zaphoyd/websocketpp.git

# 下载nlohmann/json
mkdir -p json
cd json
git clone https://github.com/nlohmann/json.git .
```

1. 使用CMake构建项目：

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 运行演示

1. 首先启动服务端：

```bash
./server     # Linux/Mac
server.exe   # Windows
```

1. 然后在另一个终端启动客户端：

```bash
./client     # Linux/Mac
client.exe   # Windows
```

1. 按照客户端提示发送测量请求并查看结果。

## 测试和扩展

服务端默认监听9002端口，客户端默认连接到`ws://localhost:9002`，测量超时时间默认为10秒。

您可以根据实际需求修改代码，例如：

- 添加更多类型的测量命令
- 扩展测量结果数据结构
- 实现安全连接(WSS)
- 添加认证机制

## 许可证

MIT
