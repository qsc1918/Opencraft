# Opencraft 开发约定

- 注释一律用简体中文，尽量短：能一行就一行，只说明“为什么 / 约束 / 坑”，不复述代码。
- 不允许英文或其他语言的注释；新增的用户可见文本也尽量用中文。
- `.ps1` 脚本含中文时必须存成 UTF-8 带 BOM，否则 Windows PowerShell 5.1 会乱码。
- 版本号只改 `version.txt` 的 `[current]` 行；编译时间戳由 `cmake/gen_version.cmake`
  在构建时自动追加，不要手写时间戳。
- 构建：`cmake -G Ninja -S . -B build` 然后 `cmake --build build`。
- 打包：默认构建后自动出 `releases/Opencraft-Installer.exe`（7-Zip SFX，开关
  `OPENCRAFT_AUTO_PACKAGE`，缺 `7-Zip\` 时只警告）；也可用 `--target package_7z`。
- 改完代码用 `build/opencraft.exe --version` 和
  `build/opencraft.exe --menu-shot build/menu.png` 做一次冒烟验证。
