#pragma once
// ---------------------------------------------------------------------------
// 控制台指令输入：后台线程从 stdin 收整行，主循环每帧 poll 取走执行。
// 指令执行放主线程（要动 world/player，IO 线程只负责收集）。
// 用 ReadFile 而不是 std::getline：不碰 C++ 流对象，进程退出时阻塞中的
// 线程由 ExitProcess 直接终止，没有流析构竞态。
// 编码：真控制台输入按 OEM 代码页（中文系统 GBK）进来，转成 UTF-8；
// 管道/重定向进来的按原样收（约定 UTF-8）。
// ---------------------------------------------------------------------------
#include <windows.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class ConsoleInput {
public:
    void start() {
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return;
        // 真控制台才有 console mode；管道/文件没有
        bool isConsole = GetConsoleMode(h, &modeTmp_) != 0;
        running_.store(true);
        th_ = std::thread([this, h, isConsole] {
            std::string buf;
            char ch[64];
            DWORD n = 0;
            while (running_.load(std::memory_order_relaxed)) {
                if (!ReadFile(h, ch, sizeof(ch), &n, nullptr) || n == 0) break;
                for (DWORD i = 0; i < n; i++) {
                    unsigned char c = (unsigned char)ch[i]; // 有符号 char 会让中文 >= 0x20 判假
                    if (c == '\r' || c == '\n') {
                        if (!buf.empty()) {
                            std::string line = isConsole ? consoleToUtf8(buf) : buf;
                            { std::lock_guard<std::mutex> lk(m_); q_.push_back(std::move(line)); }
                            buf.clear();
                        }
                    } else if (c >= 0x20) { // 跳过控制字符，中文等多字节原样收
                        buf += (char)c;
                    }
                }
            }
            running_.store(false);
        });
    }

    // 取走一条指令；没有则返回 false
    bool poll(std::string& out) {
        std::lock_guard<std::mutex> lk(m_);
        if (q_.empty()) return false;
        out = std::move(q_.front());
        q_.pop_front();
        return true;
    }

    ~ConsoleInput() {
        running_.store(false);
        if (th_.joinable()) th_.detach(); // 不等 stdin，退出不卡
    }

private:
    // 控制台行输入是 ANSI/OEM 代码页（中文系统 = GBK/cp936），转 UTF-8
    static std::string consoleToUtf8(const std::string& in) {
        int wlen = MultiByteToWideChar(CP_OEMCP, 0, in.c_str(), (int)in.size(), nullptr, 0);
        if (wlen <= 0) return in;
        std::wstring w((size_t)wlen, 0);
        MultiByteToWideChar(CP_OEMCP, 0, in.c_str(), (int)in.size(), &w[0], wlen);
        int ulen = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wlen, nullptr, 0, nullptr, nullptr);
        if (ulen <= 0) return in;
        std::string u((size_t)ulen, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wlen, &u[0], ulen, nullptr, nullptr);
        return u;
    }

    std::atomic<bool> running_{false};
    std::mutex m_;
    std::deque<std::string> q_;
    std::thread th_;
    DWORD modeTmp_ = 0; // 仅用于探测是否为真控制台
};
