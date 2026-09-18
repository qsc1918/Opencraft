// sfxstub.c — Opencraft 一键自解压启动器（SFX stub）
//
// 自身结构: [stub 本体][文件数据...][文件表][u64 表偏移]
// 文件表: u32 magic('OCSX') u32 count，之后每项 u32 名长 u64 大小 u64 偏移 名字
// 偏移为相对 SFX 文件起始的绝对偏移；包内路径用 '/'，解压时转 '\'。
//
// 行为: 解压到 <SFX 所在目录>\Opencraft\ → 询问是否运行 extract_assets.exe
//       → 询问是否启动游戏（资源不随包分发，遵守 Mojang EULA）
//
// 编译: gcc -O2 -s -mwindows -o sfxstub.exe tools/sfxstub.c -luser32

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define PAYLOAD_MAGIC 0x5853434Fu /* 'OCSX' 小端 */

static BOOL writeFileFrom(HANDLE src, uint64_t blobOff, uint64_t size, const char *path) {
    HANDLE dst = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dst == INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER li; li.QuadPart = (LONGLONG)blobOff;
    if (!SetFilePointerEx(src, li, NULL, FILE_BEGIN)) { CloseHandle(dst); return FALSE; }
    char buf[65536];
    while (size > 0) {
        DWORD chunk = size > sizeof(buf) ? sizeof(buf) : (DWORD)size, got = 0;
        if (!ReadFile(src, buf, chunk, &got, NULL) || got == 0) break;
        DWORD written = 0;
        if (!WriteFile(dst, buf, got, &written, NULL) || written != got) break;
        size -= got;
    }
    CloseHandle(dst);
    return size == 0;
}

static void mkdirs(const char *path) {
    char tmp[MAX_PATH]; strncpy(tmp, path, sizeof(tmp) - 1); tmp[sizeof(tmp) - 1] = 0;
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '\\' || *p == '/') { char c = *p; *p = 0; CreateDirectoryA(tmp, NULL); *p = c; }
    }
    CreateDirectoryA(tmp, NULL);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int nShow) {
    (void)hInst; (void)hPrev; (void)cmdLine; (void)nShow;
    char self[MAX_PATH];
    GetModuleFileNameA(NULL, self, MAX_PATH);
    HANDLE f = CreateFileA(self, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { MessageBoxA(NULL, "无法读取自身文件。", "Opencraft 安装器", MB_ICONERROR); return 1; }

    LARGE_INTEGER fsz;
    if (!GetFileSizeEx(f, &fsz) || fsz.QuadPart < 16) { CloseHandle(f); MessageBoxA(NULL, "安装包损坏（过小）。", "Opencraft 安装器", MB_ICONERROR); return 1; }

    LARGE_INTEGER li; li.QuadPart = fsz.QuadPart - 8;
    uint64_t tableOff = 0;
    SetFilePointerEx(f, li, NULL, FILE_BEGIN);
    DWORD rd = 0;
    if (!ReadFile(f, &tableOff, 8, &rd, NULL) || rd != 8 || tableOff == 0 || tableOff >= (uint64_t)fsz.QuadPart) {
        CloseHandle(f); MessageBoxA(NULL, "安装包损坏（无 payload 表）。", "Opencraft 安装器", MB_ICONERROR); return 1;
    }
    li.QuadPart = (LONGLONG)tableOff;
    SetFilePointerEx(f, li, NULL, FILE_BEGIN);
    uint32_t magic = 0, count = 0;
    if (!ReadFile(f, &magic, 4, &rd, NULL) || rd != 4 || magic != PAYLOAD_MAGIC ||
        !ReadFile(f, &count, 4, &rd, NULL) || rd != 4 || count == 0 || count > 4096) {
        CloseHandle(f); MessageBoxA(NULL, "安装包损坏（payload 表无效）。", "Opencraft 安装器", MB_ICONERROR); return 1;
    }

    // 目标目录: <exe所在目录>\Opencraft
    char dest[MAX_PATH];
    strncpy(dest, self, sizeof(dest) - 1); dest[sizeof(dest) - 1] = 0;
    char *slash = strrchr(dest, '\\'); if (slash) *slash = 0;
    strncat(dest, "\\Opencraft", sizeof(dest) - strlen(dest) - 1);
    mkdirs(dest);

    int ok = 0, fail = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t nameLen = 0; uint64_t size = 0, off = 0;
        if (!ReadFile(f, &nameLen, 4, &rd, NULL) || rd != 4 || nameLen == 0 || nameLen > 512) { fail++; break; }
        if (!ReadFile(f, &size, 8, &rd, NULL) || rd != 8 || !ReadFile(f, &off, 8, &rd, NULL) || rd != 8) { fail++; break; }
        char name[513];
        if (!ReadFile(f, name, nameLen, &rd, NULL) || rd != nameLen) { fail++; break; }
        name[nameLen] = 0;
        char path[MAX_PATH];
        _snprintf(path, sizeof(path) - 1, "%s\\%s", dest, name);
        for (char *p = path; *p; p++) if (*p == '/') *p = '\\';
        // 创建父目录
        char *last = strrchr(path, '\\');
        if (last) { *last = 0; mkdirs(path); *last = '\\'; }
        if (writeFileFrom(f, off, size, path)) ok++; else fail++;
    }
    CloseHandle(f);

    char msg[1024];
    _snprintf(msg, sizeof(msg) - 1,
              "解压完成：%d 个文件 -> %s\n\n"
              "注意：安装包不含游戏资源（遵守 Mojang EULA）。\n"
              "首次使用请运行 extract_assets.exe，选择你自己的 minecraft.jar\n"
              "（或 versions 里的版本 jar）提取方块/物品贴图。\n\n"
              "是否现在运行资源提取工具？", ok, dest);
    if (MessageBoxA(NULL, msg, "Opencraft 安装器", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
        char toolPath[MAX_PATH];
        _snprintf(toolPath, sizeof(toolPath) - 1, "%s\\extract_assets.exe", dest);
        PROCESS_INFORMATION pi; STARTUPINFOA si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
        if (CreateProcessA(toolPath, NULL, NULL, NULL, FALSE, 0, NULL, dest, &si, &pi)) {
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        } else {
            MessageBoxA(NULL, "找不到 extract_assets.exe。", "Opencraft 安装器", MB_ICONWARNING);
        }
    }

    _snprintf(msg, sizeof(msg) - 1, "是否立即启动 Opencraft？\n(%s\\opencraft.exe)", dest);
    if (MessageBoxA(NULL, msg, "Opencraft 安装器", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        char exePath[MAX_PATH];
        _snprintf(exePath, sizeof(exePath) - 1, "%s\\opencraft.exe", dest);
        PROCESS_INFORMATION pi; STARTUPINFOA si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
        if (!CreateProcessA(exePath, NULL, NULL, NULL, FALSE, 0, NULL, dest, &si, &pi)) {
            MessageBoxA(NULL, "启动失败，请手动运行 Opencraft\\opencraft.exe。", "Opencraft 安装器", MB_ICONERROR);
        } else {
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        }
    }
    return fail ? 2 : 0;
}
