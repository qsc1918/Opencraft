#pragma once
// ===========================================================================
// platform.hpp — 平台抽象层（Windows / Linux 共用同一套接口）
//
// 只放"操作系统相关、且没法用标准库替代"的东西：
//   * 键位：游戏代码用 Key 枚举，不再直接用 Win32 的 VK_*；
//     字母/数字的取值与 ASCII 一致（所以 in.keys['W'] 这种写法两边都能用）。
//   * 窗口：见 window.hpp（Win32 与 X11 两套实现，同一个类）。
//   * 可执行文件目录、线程优先级、鼠标指针显示、控制台输入。
// ===========================================================================
#include <cstdint>
#include <string>

// 键位（与 Win32 虚拟键码取值一致，Linux 侧映射到同一套）
enum Key : int {
    KEY_NONE      = 0,
    KEY_BACKSPACE = 0x08,
    KEY_TAB       = 0x09,
    KEY_ENTER     = 0x0D,
    KEY_SHIFT     = 0x10,
    KEY_CTRL      = 0x11,
    KEY_ALT       = 0x12,
    KEY_PAUSE     = 0x13,
    KEY_CAPS_LOCK = 0x14,
    KEY_ESCAPE    = 0x1B,
    KEY_SPACE     = 0x20,
    KEY_PAGE_UP   = 0x21,
    KEY_PAGE_DOWN = 0x22,
    KEY_END       = 0x23,
    KEY_HOME      = 0x24,
    KEY_LEFT      = 0x25,
    KEY_UP        = 0x26,
    KEY_RIGHT     = 0x27,
    KEY_DOWN      = 0x28,
    KEY_INSERT    = 0x2D,
    KEY_DELETE    = 0x2E,
    // 0x30..0x39 = '0'..'9'，0x41..0x5A = 'A'..'Z'（与 ASCII 相同，直接用字符字面量）
    KEY_F1  = 0x70, KEY_F2  = 0x71, KEY_F3  = 0x72, KEY_F4  = 0x73,
    KEY_F5  = 0x74, KEY_F6  = 0x75, KEY_F7  = 0x76, KEY_F8  = 0x77,
    KEY_F9  = 0x78, KEY_F10 = 0x79, KEY_F11 = 0x7A, KEY_F12 = 0x7B,
};

namespace platform {

// 可执行文件所在目录（assets/、shaders/、version.txt、options.txt 都按它找）
std::string exeDir();
// 目录分隔符（Windows '\\'，Linux '/'）
char pathSep();
// 拼路径：join(a, b)
std::string join(const std::string& a, const std::string& b);

// 生成/网格化工作线程降优先级（进新维度时别和渲染主线程抢核）
void setThreadPriorityLow();

// 显示/隐藏鼠标指针（菜单里要显示，游戏中隐藏）
void showCursor(bool on);

// 控制台输入线程：start() 起后台线程读整行，poll() 主线程取。
// Windows 侧额外做 OEM(GBK) → UTF-8 转换；Linux 侧终端本来就是 UTF-8，原样收。
class ConsoleInput {
public:
    void start();
    void stop();
    bool poll(std::string& line);
    bool running() const;

private:
    bool running_ = false;
    void* th_ = nullptr;   // std::thread*，头文件不暴露 <thread>
};

} // namespace platform
