// ===========================================================================
// platform.cpp — 平台抽象实现（Windows / Linux）
// ===========================================================================
#include "platform.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace platform {

#ifdef _WIN32
std::string exeDir() {
    char buf[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    return pos == std::string::npos ? "." : p.substr(0, pos);
}
char pathSep() { return '\\'; }

void setThreadPriorityLow() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
}

void showCursor(bool on) { ::ShowCursor(on ? TRUE : FALSE); }
#else
std::string exeDir() {
    // 读 /proc/self/exe：比 argv[0] 可靠（符号链接也会解析成真路径）
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return ".";
    buf[n] = 0;
    std::string p(buf);
    size_t pos = p.find_last_of('/');
    return pos == std::string::npos ? "." : p.substr(0, pos);
}
char pathSep() { return '/'; }

void setThreadPriorityLow() {
    // POSIX 没有"低优先级"线程优先级，用 nice 值代替（越大越"客气"）
    setpriority(PRIO_PROCESS, 0, 10);
}

void showCursor(bool on) {
    // X11 的光标是窗口属性，由 window_x11.cpp 处理；这里留个空实现
    (void)on;
}
#endif

std::string join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (!b.empty() && (b[0] == '/' || b[0] == '\\')) return b;
    if (a.back() == '/' || a.back() == '\\') return a + b;
    return a + pathSep() + b;
}

// ---------------------------------------------------------------------------
// 控制台输入：一个后台线程读整行，主线程 poll 取走
// ---------------------------------------------------------------------------
namespace {

std::mutex g_consoleLock;
std::deque<std::string> g_consoleQueue;
std::atomic<bool> g_consoleRunning{false};

#ifdef _WIN32
// 真控制台按 OEM 代码页进来（中文系统是 GBK），要转成 UTF-8；
// 管道/重定向进来的已经是 UTF-8，原样收。
std::string consoleToUtf8(const std::string& in) {
    if (in.empty()) return in;
    int n = MultiByteToWideChar(CP_OEMCP, 0, in.c_str(), (int)in.size(), nullptr, 0);
    if (n <= 0) return in;
    std::wstring w((size_t)n, 0);
    MultiByteToWideChar(CP_OEMCP, 0, in.c_str(), (int)in.size(), &w[0], n);
    int m = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), n, nullptr, 0, nullptr, nullptr);
    if (m <= 0) return in;
    std::string out((size_t)m, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), n, &out[0], m, nullptr, nullptr);
    return out;
}
#endif

void pushLine(const std::string& line) {
    std::lock_guard<std::mutex> lk(g_consoleLock);
    g_consoleQueue.push_back(line);
}

} // 匿名命名空间

void ConsoleInput::start() {
    if (running_) return;
    g_consoleRunning.store(true);
    running_ = true;
    th_ = new std::thread([]() {
#ifdef _WIN32
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE || h == nullptr) { g_consoleRunning.store(false); return; }
        DWORD mode = 0;
        bool isConsole = GetConsoleMode(h, &mode) != 0;   // 管道/文件没有 console mode
        std::string buf;
        char ch[256];
        DWORD n = 0;
        while (g_consoleRunning.load(std::memory_order_relaxed)) {
            if (!ReadFile(h, ch, sizeof(ch), &n, nullptr) || n == 0) break;
            for (DWORD i = 0; i < n; i++) {
                unsigned char c = (unsigned char)ch[i];   // 有符号 char 会让中文 >= 0x20 判假
                if (c == '\r' || c == '\n') {
                    if (!buf.empty()) {
                        pushLine(isConsole ? consoleToUtf8(buf) : buf);
                        buf.clear();
                    }
                } else if (c >= 0x20) {                    // 跳过控制字符，中文等多字节原样收
                    buf += (char)c;
                }
            }
        }
#else
        std::string buf;
        char ch[256];
        while (g_consoleRunning.load(std::memory_order_relaxed)) {
            ssize_t n = ::read(STDIN_FILENO, ch, sizeof(ch));
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (n == 0) break;   // EOF（没有控制台时立刻退出，不拖住进程）
            for (ssize_t i = 0; i < n; i++) {
                char c = ch[i];
                if (c == '\r' || c == '\n') {
                    if (!buf.empty()) {
                        pushLine(buf);   // Linux 终端本来就是 UTF-8
                        buf.clear();
                    }
                } else if ((unsigned char)c >= 0x20) {
                    buf += c;
                }
            }
        }
#endif
        g_consoleRunning.store(false, std::memory_order_relaxed);
    });
}

void ConsoleInput::stop() {
    if (!running_) return;
    g_consoleRunning.store(false, std::memory_order_relaxed);
    auto* th = (std::thread*)th_;
    if (th) {
        if (th->joinable()) th->join();
        delete th;
        th_ = nullptr;
    }
    running_ = false;
}

bool ConsoleInput::poll(std::string& line) {
    std::lock_guard<std::mutex> lk(g_consoleLock);
    if (g_consoleQueue.empty()) return false;
    line = g_consoleQueue.front();
    g_consoleQueue.pop_front();
    return true;
}

bool ConsoleInput::running() const { return g_consoleRunning.load(); }

} // namespace platform
