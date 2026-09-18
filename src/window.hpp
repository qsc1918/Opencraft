#pragma once
#include <cstdint>

// 每帧输入快照；数组下标为虚拟键码（见 window.cpp）。
struct Input {
    bool keys[256] = {false};    // 持续按住状态
    bool pressed[256] = {false}; // 上升沿：仅按下当帧为 true
    bool mouse[3] = {false};     // 0 左键 1 右键 2 中键
    bool wheelUp = false;
    bool wheelDown = false;
    float scrollAccum = 0.0f;
    void clear() {
        wheelUp = wheelDown = false;
        scrollAccum = 0.0f;
        for (int i = 0; i < 256; i++) pressed[i] = false;
    }
};

class Window {
public:
    bool init(int w, int h, const char* title);
    void shutdown();

    // 窗口请求关闭时返回 false。
    bool pump();

    // 每帧调用一次，清除瞬时输入状态。
    void endFrame();

    void* hwnd() const { return hwnd_; }
    int width() const { return w_; }
    int height() const { return h_; }

    Input& input() { return in_; }

    void setCapture(bool on) { capture_ = on; }
    bool captured() const { return capture_; }
    // 返回相对上次调用的鼠标位移（仅捕获时有效）。
    void pollMouse(float& dx, float& dy);
    // 客户端坐标下的当前光标位置（原点在左上）。
    void cursorPos(float& x, float& y);

private:
    void* hwnd_ = nullptr;
    int w_ = 0, h_ = 0;
    Input in_;
    bool capture_ = false;
    float lastMX_ = 0, lastMY_ = 0;
    bool haveMouse_ = false;
    int winID_ = 0;
};
