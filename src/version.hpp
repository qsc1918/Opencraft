#pragma once
#include <string>

// 版本号：version.txt 里的 [current] + 编译时间戳，例如 0.4.0-snapshot-1-20260214-153012
namespace version {

// 版本名（运行时优先读 exe 旁的 version.txt，便于手动改）
const std::string& name();

// 编译时的本地时间戳 YYYYMMDD-HHMMSS
const char* buildStamp();

// 完整版本号：版本名-编译时间戳
const std::string& full();

} // namespace version
