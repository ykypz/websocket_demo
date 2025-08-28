#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <string>
#include <chrono>

// 生成时间戳格式的请求ID (年月日时分秒毫秒)
std::string generateTimestampId();

// 解析时间戳格式的requestId为可读格式
std::string parseTimestampId(const std::string& requestId);

#endif // TIME_UTILS_H
