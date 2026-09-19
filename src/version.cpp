#include "version.hpp"
#include "generated/version_gen.hpp"   // 编译时自动生成：版本名 + 时间戳
#include <windows.h>
#include <fstream>
#include <string>

// exe 所在目录，version.txt 与 exe 放在一起
static std::string exeDir() {
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    return pos == std::string::npos ? "." : p.substr(0, pos);
}

// 取 version.txt 中 [current] 行的版本名；失败返回空串
static std::string readVersionName(const std::string& path) {
    std::ifstream f(path);
    if (!f) return std::string();
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("[current]", 0) != 0) continue;
        std::string v = line.substr(9);
        size_t b = v.find_first_not_of(" \t");
        if (b == std::string::npos) return std::string();
        size_t e = v.find_last_not_of(" \t");
        return v.substr(b, e - b + 1);
    }
    return std::string();
}

const std::string& version::name() {
    static const std::string n = [] {
        std::string v = readVersionName(exeDir() + "\\version.txt");
        if (v.empty()) v = readVersionName("version.txt");
        return v.empty() ? std::string(OPENCRAFT_VERSION_NAME) : v;
    }();
    return n;
}

const char* version::buildStamp() { return OPENCRAFT_BUILD_STAMP; }

const std::string& version::full() {
    static const std::string s = version::name() + "-" + version::buildStamp();
    return s;
}
