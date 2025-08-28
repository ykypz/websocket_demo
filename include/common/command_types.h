#ifndef COMMAND_TYPES_H
#define COMMAND_TYPES_H

#include <string>
#include <nlohmann/json.hpp>

// 使用nlohmann/json作为JSON库
using json = nlohmann::json;

// 命令类型定义
enum class CommandType {
    SetAlignViewMode,   // 设置取流模式
    GetAlignViewMode,   // 获取取流模式
    StartStream,        // 开始取流
    StopStream,         // 停止取流
    ExcuteMeasure,  // 测量命令
    StopMeasure,        // 停止测量
    GetMeasureStatus,   // 获取测量状态
    GetSurfaceData,     // 获取面形数据
    Unknown             // 位置命令
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
std::string commandTypeToString(CommandType type);

// 将字符串转换为CommandType
CommandType stringToCommandType(const std::string& typeStr);

#endif  // COMMAND_TYPES_H
