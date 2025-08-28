#include "command_types.h"

// 将CommandType转换为字符串
std::string commandTypeToString(CommandType type) {
    switch (type) {
        case CommandType::SetAlignViewMode: return "setAlignViewMode";
        case CommandType::GetAlignViewMode: return "getAlignViewMode";
        case CommandType::StartStream: return "startStream";
        case CommandType::StopStream: return "stopStream";
        case CommandType::ExcuteMeasurement: return "executeMeasure";
        case CommandType::StopMeasure: return "stopMeasure";
        case CommandType::GetMeasureStatus: return "getMeasureStatus";
        case CommandType::GetSurfaceData: return "getSurfaceData";
        
        default: return "unknown";
    }
}

// 将字符串转换为CommandType
CommandType stringToCommandType(const std::string& typeStr) {
    if (typeStr == "setAlignViewMode") return CommandType::SetAlignViewMode;
    if (typeStr == "getAlignViewMode") return CommandType::GetAlignViewMode;
    if (typeStr == "startStream") return CommandType::StartStream;
    if (typeStr == "stopStream") return CommandType::StopStream;
    if (typeStr == "executeMeasure") return CommandType::ExcuteMeasurement;
    if (typeStr == "stopMeasure") return CommandType::StopMeasure;
    if (typeStr == "getMeasureStatus") return CommandType::GetMeasureStatus;
    if (typeStr == "getSurfaceData") return CommandType::GetSurfaceData;

    return CommandType::Unknown; // 默认返回
}
