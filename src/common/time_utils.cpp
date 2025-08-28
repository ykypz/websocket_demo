#include "time_utils.h"
#include <ctime>
#include <iomanip>
#include <cstdio>

// 生成时间戳格式的请求ID (年月日时分秒毫秒)
std::string generateTimestampId() {
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

// 解析时间戳格式的requestId为可读格式
std::string parseTimestampId(const std::string& requestId) {
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
