# Opencraft 开发约定（AI 入口）

Opencraft 是 C++20 写的类 Minecraft 体素沙盒游戏，自带手写 Vulkan 渲染器和多线程
区块生成/网格化。下界与末地已按 Minecraft Java 版 26.2 的原版机制实现；主世界地形
还是早期的简易实现，后续计划翻新。

**接手项目先读 `PROJECT_STATE.md`**：那里有完整的文件职责表、当前状态、未完成项和
踩过的坑。本文件只放约定和索引入口。

以下可参考：
MC反编译源码及工具：D:\i\decompiled，D:\i\tools
MC jar自带assets：G:\assets
MC jar自带数据包：G:\data
MC assets文件夹：C:\Users\abc\AppData\Roaming\.minecraft\assets

## 约定

- 注释一律用简体中文，尽量短：能一行就一行，只说明“为什么 / 约束 / 坑”，不复述代码。
- 不允许英文或其他语言的注释；新增的用户可见文本也尽量用中文。
- `.ps1` 脚本含中文时必须存成 UTF-8 带 BOM，否则 Windows PowerShell 5.1 会乱码。
  `.cpp/.hpp` 必须存成 UTF-8 **不带 BOM**；别用 `Set-Content`（会写成 ANSI 弄坏中文），
  用 `[IO.File]::WriteAllText($p, $t, (New-Object Text.UTF8Encoding($false)))`。
  正因如此 MSVC 侧必须加 `/utf-8`（`CMakeLists.txt` 已加）：MSVC 默认按系统代码页解析
  无 BOM 源码，中文机器上按 GBK 解析会报一堆“字符串字面量中的换行符”；GCC 默认 UTF-8，
  所以本地 MinGW 构建永远碰不到，只有 CI 的 MSVC 会炸。
- 版本号只改 `version.txt` 的 `[current]` 行；编译时间戳由 `cmake/gen_version.cmake`
  在构建时自动追加，不要手写时间戳。
- Git 提交要少而清晰：一个提交一件事，方便回退；提交信息用简短中文，别攒巨型提交。
- 新增方块要同时改 `src/blocks.hpp` 的 `Block` 枚举、`BLOCK_DEFS`（顺序即 id），
  `B_COUNT` 和文件末尾的 `static_assert`；贴图还要进 `tools/extract_assets.cpp`
  和 `src/atlas.cpp` 的 `kTileFiles`/`kTileTint`。
- 生成代码必须"逐区块自足、与顺序无关"：只写本区块的方块，跨区块的结构要把邻域
  候选算一遍再裁剪（见 `src/endgen.cpp` 的外岛）。
- **颜色是标准 sRGB 工作流**：交换链与贴图视图都是 sRGB，着色器在线性空间算，
  显示值＝贴图字节原值。给着色器加"乘性常量"（明暗、色调）要按 `系数^2.2` 用，
  否则会把暗部抬亮、颜色发灰；细节见 `PROJECT_STATE.md` §4.11。

## 构建 / 验证 / 打包

```sh
cmake -G Ninja -S . -B build
cmake --build build
build/opencraft.exe --version
build/opencraft.exe --menu-shot build/menu.png
build/endprobe.exe 12345 7      # 末地生成自检：高度剖面 + 方块统计
build/portalselftest.exe        # 末地门朝向规则 + 方环几何自检
build/portalflowtest.exe 12345  # 传送门流程自检：末地门插眼激活 + 下界门造门/回程落点
```

打包：默认构建后自动出 `releases/Opencraft-Installer.exe`（7-Zip SFX，开关
`OPENCRAFT_AUTO_PACKAGE`，缺 `7-Zip\` 时只警告）；也可用 `--target package_7z`。

发版：`tools\tag_ci.ps1` 读 `version.txt` 的 `[current]` 打 `ci<版本>` 标签并推送
（工作区不干净会拦，`-DryRun` 预览、`-Force` 强行、重复标签指向别的提交时报错）。

截图调试：`build/opencraft.exe --dim end|nether --seed N --pos x,y,z --yaw R --pitch R
--render-dist N --frames 300 --screenshot out.png --no-ui`（`--frames` 要给够，等区块生成完）。

调试用参数（见 `PROJECT_STATE.md` §5）：`--tp-dim overworld|nether|end` 进世界后直接切维度
（游戏里 F7 正向、F6 反向循环切主世界/下界/末地）；`--place x,y,z,id` 放方块、
`--use-eye x,y,z` 对末地门框架用末影之眼、`--light-portal x,y,z` 点燃下界门。

## 目录入口

- `src/specs.hpp` 世界常量唯一来源；`src/dimensions.hpp` 维度表；`src/blocks.hpp` 方块表。
- `third_party/volk/` 是自带的 volk（MIT），**别改回用 SDK 的 `Include/Volk`**：那是大写目录，
  本地大小写不敏感能过、CI 上会 `fatal error C1083: volk.h`；`src/volk_impl.c` 是它的
  包装编译单元（先包含 Win32 头，才会生成 KHR_win32_surface 的函数指针）。
- `src/generator.cpp` 按维度分发的入口；下界在 `src/nethergen.cpp`，末地在 `src/endgen.cpp`。
- `src/mcnoise.cpp` 原版噪声/随机数复刻（LegacyRandomSource、ImprovedNoise、SimplexNoise、
  PerlinNoise、BlendedNoise）；改生成前先确认这里的参数与顺序。
- `src/world.cpp` 区块存储与工作线程；`src/mesher.cpp` 网格化与多 element 方块模型
  （紫颂植株/花模型见 `PROJECT_STATE.md` §4.9）；`src/renderer.cpp` 渲染；`src/portal.cpp` 传送门。
- `src/console.hpp` 控制台指令输入（`tp`/`where`/`help`，主循环每帧取队列执行）。
- `docs/standards.md` 世界规范；`docs/minecraft_1.0_reference.md` MC 对照资料。
- 反编译源码在 `D:\i\decompiled\26.2`（首选）与 `D:\i\decompiled\1.21.4`。

### 提交格式（重要）

提交格式适用于单行提交信息和 PR 标题。

```text
type(scope?): digest
```

各字段含义如下：

| 字段       | 要求                   |
|----------|----------------------|
| `type`   | 必须使用英文小写，表示提交类型      |
| `scope`  | 可选，应使用英文(中文也行，更好一些，应该没有其他母语者开发这个项目的...)说明影响的模块或子系统  |
| `digest` | 应使用中文概括提交内容，并以动词原型开头 |

`scope` 应使用英文小写。若包含多个单词，应使用短横线连接，例如 `profile`、`lifecycle`、`java-manage`、`rpc`。

`digest` 应说明提交具体做了什么。必要的专业名词可以使用英文。推荐写法包括：

```text
feat(profile): 支持可选的高级材质
fix(java-manage): 修复 Java 8 及以前的版本无法被正确识别
imp(rpc): 默认启用 RPC 服务
```

::: tip 提示
`scope` 后面的冒号与 `digest` 之间必须保留一个空格。
:::

### 提交类型

| 类型                     | 说明                                                         |
|------------------------|------------------------------------------------------------|
| `feat` / `feature`     | 引入新特性。通常是用户可感知的更改，例如深色模式、Java 管理；也可以是仅开发者可感知的能力，例如引入生命周期管理 |
| `fix`                  | 修复线上或开发中发现的 bug，使程序行为恢复预期                                  |
| `imp` / `improve`      | 优化现有功能的交互体验、可读性等，通常是用户可感知的更改                               |
| `ref` / `refactor`     | 重构现有代码结构或实现方式，不大幅改变外部功能，主要改善可维护性                           |
| `docs`                 | 仅修改文档内容，例如 README、注释、API 文档等，不涉及程序逻辑                       |
| `style`                | 与功能无关的代码格式调整，包括但不限于空格、缩进、分号、命名风格等                          |
| `perf` / `performance` | 专门针对性能的优化，例如算法改进、缓存方案、渲染优化等                                |
| `test`                 | 添加或修改单元测试、集成测试、测试脚本等                                       |
| `chore`                | 构建流程维护、依赖升级、项目配置调整等与业务逻辑无关的杂项                              |
| `bump`                 | 版本相关更改，例如发布新 release 版本                                    |
| `ci`                   | 修改 CI/CD 配置文件、自动化脚本、构建管道等                                  |

提交类型应尽量选择最贴近修改内容的类型。部分类型较为泛用，但在存在更具体分类时，不应使用泛用分类。

例如：

* 修复 CI 构建失败时，应使用 `ci`，而不是 `fix`；
* 修复可通过提升设备性能缓解的问题时，应优先考虑是否属于 `perf`；
* 修改构建配置或添加依赖项时，应使用 `chore`，而不是 `feat`。

### `digest` 编写要求

`digest` 应简洁、明确，并能直接看出本次提交的实际修改内容。

`digest` 不应使用以下写法：

* 过去时或将来时；
* 单纯的名词短语；
* 含糊不清、无法看出具体关联的描述；
* 末尾句号。

错误示例：

```text
docs: 深色模式
fix: 更正了 typo
imp: 一个高占用功能现在会默认关闭
```

推荐写法：

```text
docs: 补充深色主题适配说明
fix: 更正下载页面的 NeoForge 拼写错误
imp: 默认关闭资源扫描中的高占用检查
```

一个清晰的 `digest` 通常不应超过 30 个汉字或 50 个英文字母。

### 多行格式

多行格式适用于需要详细说明修改内容的提交信息。

```text
type(scope?): subject

body

footer
```

第一行的规则与简单格式基本一致，但 `subject` 不需要完整展开所有细节。

`body` 用于详细描述提交内容，应包含修改动机，以及与之前行为的差异。

`footer` 可包含破坏性改动、合作作者信息或关联 issue。若没有对应内容，可以省略。

关联 issue 可使用以下格式：

```text
Close #234
Close #234, #114, #514
```

若存在破坏性改动，应在 `footer` 中明确说明。

### 回滚提交

回滚某个提交或 PR 时，提交信息应以 `revert:` 开头。

```text
revert: digest (hash)
```

其中：

| 字段       | 说明                                  |
|----------|-------------------------------------|
| `digest` | 原提交信息首行                             |
| `hash`   | 原提交节点 hash，或原 PR ID。PR ID 应以 `#` 开头 |

示例：

```text
revert: feat(profile): 支持可选的高级材质 (#123)
```

## 当前状态

见 `PROJECT_STATE.md` 的「当前状态」一节（完成项、未完成项、最近验证、已知差距）。
改完项目请同步更新那一节。
