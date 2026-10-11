// ===========================================================================
// window_x11.cpp — X11 窗口与输入（Linux 后端）
//
// 与 window_win32.cpp 实现同一个 Window 接口：
//   * 主循环 pump() 抽事件，endFrame() 清瞬时状态；
//   * 捕获鼠标后每帧把指针 warp 回窗口中心，靠位移差算视角转动（原版同做法）；
//   * 键位统一映射到 platform.hpp 的 Key（取值与 Win32 虚拟键码一致），
//     所以游戏代码里 in.keys['W'] / in.pressed[KEY_F5] 两边行为相同。
// 需要 Xlib（libx11-dev）。Vulkan surface 用 VkXlibSurfaceKHR（vk.cpp 里选）。
// ===========================================================================
// 本工程的窗口类也叫 Window，而 X11 有同名的 typedef（XID）——
// 包含 Xlib 之前先把 Xlib 那个改名，否则和我们的类在同一作用域里撞车。
#define Window XWindow
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#undef Window

#include "platform.hpp"
#include "window.hpp"
#include <cstring>
#include <string>

namespace {

Display* g_display = nullptr;
XWindow g_win = 0;
Atom g_wmDeleteWindow = 0;
Input* g_input = nullptr;
int g_screen = 0;
bool g_warpPending = false;

// X11 的 keysym → 我们的 Key 码（只映射用得到的）
int keysymToKey(KeySym ks) {
    if (ks >= XK_a && ks <= XK_z) return 'A' + (int)(ks - XK_a);
    if (ks >= XK_A && ks <= XK_Z) return 'A' + (int)(ks - XK_A);
    if (ks >= XK_0 && ks <= XK_9) return '0' + (int)(ks - XK_0);
    if (ks >= XK_F1 && ks <= XK_F12) return KEY_F1 + (int)(ks - XK_F1);
    switch (ks) {
    case XK_Escape:  return KEY_ESCAPE;
    case XK_Return:   return KEY_ENTER;
    case XK_Tab:      return KEY_TAB;
    case XK_BackSpace:return KEY_BACKSPACE;
    case XK_space:    return KEY_SPACE;
    case XK_Shift_L: case XK_Shift_R: return KEY_SHIFT;
    case XK_Control_L: case XK_Control_R: return KEY_CTRL;
    case XK_Alt_L: case XK_Alt_R: return KEY_ALT;
    case XK_Up:       return KEY_UP;
    case XK_Down:     return KEY_DOWN;
    case XK_Left:     return KEY_LEFT;
    case XK_Right:    return KEY_RIGHT;
    case XK_Home:     return KEY_HOME;
    case XK_End:      return KEY_END;
    case XK_Page_Up:  return KEY_PAGE_UP;
    case XK_Page_Down:return KEY_PAGE_DOWN;
    case XK_Insert:   return KEY_INSERT;
    case XK_Delete:   return KEY_DELETE;
    case XK_Pause:    return KEY_PAUSE;
    case XK_Caps_Lock:return KEY_CAPS_LOCK;
    default: return KEY_NONE;
    }
}

// 鼠标按钮 → Input::mouse 下标（1=左 2=中 3=右 → 0/2/1）
int buttonToMouseIndex(unsigned int b) {
    switch (b) {
    case Button1: return 0;
    case Button2: return 2;
    case Button3: return 1;
    default: return -1;
    }
}

} // 匿名命名空间

// X11 后端私有数据。放在类外定义：Window 类里只存 void*，
// 这样 window.hpp 不用包含 Xlib 头（Xlib 的 Window 与本工程同名，见文件开头）。
struct X11State {
    Display* display = nullptr;
    XWindow win = 0;
    bool cursorHidden = false;
};

bool Window::init(int w, int h, const char* title) {
    w_ = w;
    h_ = h;
    X11State* st = new X11State();
    state_ = st;

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        fprintf(stderr, "[window] 打不开 X display（需要 DISPLAY 环境变量，或用 xvfb-run）\n");
        delete st;
        state_ = nullptr;
        return false;
    }
    int scr = DefaultScreen(dpy);
    XWindow win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0,
                                       (unsigned)w, (unsigned)h, 0,
                                       BlackPixel(dpy, scr), BlackPixel(dpy, scr));
    if (!win) {
        XCloseDisplay(dpy);
        delete st;
        state_ = nullptr;
        return false;
    }
    // 要收 KeyPressRelease / ButtonPress / Motion / StructureNotify
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                              ButtonReleaseMask | PointerMotionMask | StructureNotifyMask |
                              FocusChangeMask);
    XStoreName(dpy, win, title);
    // 关闭按钮：让 WM 发 ClientMessage，我们才能走正常退出流程
    g_wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &g_wmDeleteWindow, 1);
    XMapWindow(dpy, win);

    st->display = dpy;
    st->win = win;
    hwnd_ = (void*)(uintptr_t)win;   // Vulkan 的 VkXlibSurfaceKHR 要用这个值
    g_display = dpy;
    g_win = win;
    g_screen = scr;
    g_input = &in_;
    return true;
}

void Window::shutdown() {
    if (state_) {
        X11State* st = static_cast<X11State*>(state_);
        if (st->display) {
            if (st->win) XDestroyWindow(st->display, st->win);
            XCloseDisplay(st->display);
        }
        delete st;
        state_ = nullptr;
    }
    hwnd_ = nullptr;
    g_input = nullptr;
    g_display = nullptr;
    g_win = 0;
    g_warpPending = false;
}

bool Window::pump() {
    if (!g_display) return false;
    // 捕获时把指针锁回中心：warp 本身会产生一个 Motion 事件，用 g_warpPending 忽略掉
    if (capture_ && !g_warpPending) {
            XWindowAttributes attr;
        if (XGetWindowAttributes(g_display, g_win, &attr)) {
            int cx = attr.width / 2, cy = attr.height / 2;
            XWarpPointer(g_display, None, g_win, 0, 0, 0, 0, cx, cy);
            lastMX_ = (float)cx;
            lastMY_ = (float)cy;
            haveMouse_ = true;
        }
        g_warpPending = true;
    }
    while (XPending(g_display) > 0) {
        XEvent ev;
        XNextEvent(g_display, &ev);
        switch (ev.type) {
        case KeyPress: {
            if (!g_input) break;
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            int k = keysymToKey(ks);
            // 字符键（字母数字）直接用 ASCII，Shift 的大小写由 XLookupString 决定
            if (k == KEY_NONE && ks >= 32 && ks < 127) k = (int)ks;
            if (k > 0 && k < 256) {
                if (!g_input->keys[k]) g_input->pressed[k] = true;   // 过滤自动重复
                g_input->keys[k] = true;
            }
            break;
        }
        case KeyRelease: {
            if (!g_input) break;
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            int k = keysymToKey(ks);
            if (k == KEY_NONE && ks >= 32 && ks < 127) k = (int)ks;
            if (k > 0 && k < 256) g_input->keys[k] = false;
            break;
        }
        case ButtonPress:
        case ButtonRelease: {
            if (!g_input) break;
            int mi = buttonToMouseIndex(ev.xbutton.button);
            if (mi >= 0) g_input->mouse[mi] = (ev.type == ButtonPress);
            break;
        }
        case MotionNotify: {
            if (g_warpPending) { g_warpPending = false; break; }   // 自己 warp 出来的位移不算
            if (g_input && capture_) {
                g_input->scrollAccum = 0.0f;
            }
            lastMX_ = (float)ev.xmotion.x;
            lastMY_ = (float)ev.xmotion.y;
            break;
        }
        case ConfigureNotify:
            w_ = ev.xconfigure.width;
            h_ = ev.xconfigure.height;
            break;
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == g_wmDeleteWindow) return false;
            break;
        default: break;
        }
    }
    return true;
}

void Window::endFrame() { in_.clear(); }

void Window::setTitle(const char* utf8) {
    if (!g_display || !g_win) return;
    XStoreName(g_display, g_win, utf8);   // X11 的标题就是 UTF-8
    XFlush(g_display);
}

void Window::pollMouse(float& dx, float& dy) {
    dx = dy = 0.0f;
    if (!capture_ || !g_display) return;
    XWindowAttributes attr;
    if (!XGetWindowAttributes(g_display, g_win, &attr)) return;
    int cx = attr.width / 2, cy = attr.height / 2;
    int px = attr.x, py = attr.y;
    XWindow child;
    int rx = 0, ry = 0;
    if (XTranslateCoordinates(g_display, g_win, RootWindow(g_display, g_screen), 0, 0, &rx, &ry, &child)) {
        // 屏幕坐标 → 窗口内坐标
        px = rx - attr.x;
        py = ry - attr.y;
    }
    if (haveMouse_) {
        dx = (float)(px - cx);
        dy = (float)(py - cy);
    }
    haveMouse_ = true;
}

void Window::cursorPos(float& x, float& y) {
    x = y = 0;
    if (!g_display) return;
    XWindow child;
    int rx = 0, ry = 0;
    if (!XTranslateCoordinates(g_display, g_win, RootWindow(g_display, g_screen), 0, 0, &rx, &ry, &child))
        return;
    XWindowAttributes attr;
    if (XGetWindowAttributes(g_display, g_win, &attr)) {
        x = (float)(rx - attr.x);
        y = (float)(ry - attr.y);
    }
}
