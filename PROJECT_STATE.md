# Opencraft 项目状态（面向 AI 的上下文文档）

> 本文件是"压缩上下文"式的项目现状说明：换一个 AI/会话接手时，读这一份 + `AGENTS.md`
> 就能继续干活。**改动项目后请同步更新下面的「当前状态」一节。**
>
> 最后更新：**2026-10-06 三块大改动** ——
> ① **方块更新系统**（PP/NC 更新传播 + 计划刻 + 下落的方块，`src/blockupdate.*`）；
> ② **实体与生物**（玩家成为实体 + 猪牛羊鸡/僵尸骷髅苦力怕蜘蛛/烈焰人末影人，
>    原版模型与贴图，`src/mobs.*` `src/mobmodel.*` `src/mobspawn.*`）；
> ③ **Linux 兼容**（X11 后端 + 自带 PNG 编解码 + 平台抽象层，Windows/Linux 同一份代码）。

---

## 1. 项目是什么

Opencraft 是一个 C++20 写的单进程体素沙盒游戏，自带手写 Vulkan 渲染器（不依赖图形引擎），
玩法与数值对齐 Minecraft Java 版：

- 基础玩法对齐早期版本（Beta ~ 1.8）：草方块/矿石/橡树/海洋、创造模式飞行、快捷栏与背包。
- **下界与末地两个维度按 Java 版 26.2 的原版机制实现**（密度函数、结构、群系），
  反编译源码在 `D:\i\decompiled\26.2`（另一个是 `1.21.4`，26.2 更可读）。
- 主世界地形仍是早期的简易实现，**后续计划也翻新成原版机制**（尚未开始）。

构建产物 `build/opencraft.exe`，旁边需要 `assets/`、`shaders/*.spv`、`version.txt`。

## 2. 当前状态

### 已完成

| 模块 | 状态 |
|------|------|
| 渲染（Vulkan + 手写管线） | 可用；地形/水/天空/UI/菜单管线，图集 + mipmap；**标准 sRGB 工作流**（sRGB 贴图视图 + 线性着色 + sRGB 交换链，见 §4.11） |
| 区块系统 | 16×128×16 区块列，多线程生成 + 网格化，按维度分开存储 |
| 主世界生成 | 简易噪声（高度图 + 洞穴 + 矿石 + 树 + 水），非原版，待翻新 |
| **下界生成** | **已按 26.2 原版机制重写**：`nether/base_3d_noise` + slide 密度、基岩地板/天花板渐变、y=32 岩浆海、下界荒地群系表面规则 |
| **末地生成** | **已按 26.2 原版机制重写**：`end_islands` + `base_3d_noise` 密度、主岛、10 根黑曜石柱（含铁栏杆笼）、返回传送门、末地平台、外岛、紫颂植株 |
| 末地传送门框架 | 有朝向（facing）与有眼两种状态，共 8 个方块 id；按原版模型：框 13/16 高 + **居中**的 8×8 眼块（凸出框顶 3/16），逐面 UV/cullface 与原版 `end_portal_frame(_filled).json` 一致 |
| 末地门激活 | 12 个框架**朝向必须指向环心**且都有眼（规则与 `portalselftest` / `portalflowtest` 覆盖） |
| 维度传送 | 下界门（8:1 缩放，**目的地按原版 PortalForcer 找/造门 + 门内安全落点**）、末地门（进末地落在 y=49 的黑曜石平台上，朝向 WEST）；调试可用 `--tp-dim` 与 F6/F7 快速切维度 |
| 实体 | 末影龙、末影水晶、龙蛋（部分） |
| 存档 | 自定义二进制 v3，**按维度分段**保存/读取 |
| 紫颂花生长 | 方块随机刻（`mctick`）复刻 `ChorusFlowerBlock.randomTick` |
| 火把/铁栏杆/紫颂方块 | 已注册，有贴图；**紫颂植株/花已按原版模型渲染**（multipart 凸起 + 花杯形，见 §4.9） |
| 视频设置 | 垂直同步、渲染距离滑块、显卡设备选择（重启生效）、**近景清晰距离滑块**（默认 16 格，0=关；options.txt `texSharp:` 实时生效） |
| 控制台指令 | `tp`（坐标/跨维度）、`where`（坐标+维度）、`help`；后台线程读 stdin，主线程每帧执行（`src/console.hpp`） |
| 火把 | 原版细杆模型 + 朝向：地面火把 + 墙上火把 4 向（B_WALL_TORCH 基准朝南 + W/N/E），玩家按点击面选形态 |
| 纹理 | 末地门/火焰用原版贴图；图集按原生分辨率排版（材质包友好）；**颜色与原版对齐**：开阔地顶面显示值＝贴图字节原值；近景（默认 16 格）内锁 mip0 最近邻，远景 mip+各向异性 |
| 发版辅助 | `tools/tag_ci.ps1`：读 `version.txt` 的 `[current]` 打 `ci<版本>` 标签并推送 |
| **剔除与网格化（2026-10-06 一轮性能改造）** | 逐项见 §4.12：邻接面剔除（原有）、**贪心网格化**（新增，顶点数 −47%~−64%）、视锥剔除改用网格实际 y 范围、遮挡剔除（16³ 分段保守洪泛）、背面剔除（水面改双面） |
| 开发自检 | `meshtest`（网格化/合并/遮挡面判定）、`endprobe`、`portalselftest`、`portalflowtest`、**`blockupdatetest`（方块更新 15 项）**；`tools/imgdiff.py` 逐像素比对两张截图 |
| **方块更新（2026-10-06）** | PP/NC 更新按原版顺序与 DFS 传播 + 计划刻队列（`src/blockupdate.*`）：沙子/沙砾/龙蛋失去支撑变**下落的方块实体**、火把/墙火把/紫颂/火的支撑检查；世界生成写入不产生更新（原版行为） |
| **实体与生物（2026-10-06 / 10-11）** | 玩家是实体（`PlayerEntity` 注册进 World，可被攻击/死亡回生点）；猪牛羊鸡、僵尸骷髅苦力怕蜘蛛、烈焰人末影人共 10 种，按**原版模型**（texOffs 逐面取子矩形）渲染；**AI 已按原版架构移植**（`mobai.*` + `mobpath.*`：GoalSelector/Goal、MoveControl/LookControl/JumpControl、GroundPathNavigation + A* 寻路）；刷新按原版规则（亮度、方块、维度、24~128 格、被动 10 / 敌对 70 上限），烈焰人因没有下界要塞不自然刷新 |
| **Linux 兼容（2026-10-06）** | X11 窗口 + Vulkan Xlib surface + 平台抽象层（`src/platform.*` `src/window_x11.cpp`）；PNG 读写换成自带的（`png_decode.cpp`/`png.cpp`，不再依赖 WIC）；已实测在 WSL Ubuntu 上编译 + 无头运行（Xvfb + lavapipe），画面与 Windows 版一致 |

### 未完成 / 已知差距

- **主世界生成**没按原版，等后续翻新（用户明确说这是以后的事）。
- **末影龙 / 折跃门 / 龙蛋 / 末地城**暂不做（用户暂时不要）；龙相关代码是旧的，未与新生成对齐。
- 紫颂果（吃、传送）未做；紫颂花只做种植与生长。
- 下界群系只有 **下界荒地**；其他群系（灵魂沙峡谷、绯红/诡异森林、玄武岩三角洲）未做。
- 末地群系只做 **the_end**（中心）；外岛群系按"距离 > 64 区块"判定，没有做完整的多噪声群系选择。
- 下界的表面规则（灵魂沙/沙砾层）用"最高方块 + 同频噪声"近似，**没有**完整实现原版的
  `stone_depth` / `surface noise`；视觉效果接近但细节不完全一致。
- 外岛/紫颂的 feature 随机种子用固定盐而不是原版的全局 feature index（`setFeatureSeed`），
  结构长相与原版一致但**具体位置与原版不同种子不一致**。
- 火把/墙火把**已按原版模型渲染并支持朝向**（2026-09-30）；铁栏杆还是没有连接模型
  （只有 id 与贴图）。
  （贴图本身已修好：`T_ITEM_BASE` 跟随 `T_COUNT`，不再被物品图标覆盖。）
- 方块碰撞箱仍是整格：末地门框架在原版是 13/16 高、有眼时再加中间的 8×8 柱，
  站在框上会比原版高 3/16（渲染轮廓已按原版形状画）。
- 下界门的目的地搜索只在**已生成区块**里找已有门（原版有 POI 索引，能搜 16/128 格方形
  范围内所有门）；找不到就按原版 `createPortal` 造一扇，所以回程一般能找到原门。
- 传送门方块没有 AXIS 状态位，门轴靠邻居推断（`portal::inferPortalAxis`）；
  1 格宽的畸形门会退化成 X 轴。
- 末地门激活用的是"12 框同 Y、都有眼、朝向都指向环心"这条等价规则，而不是原版
  `BlockPattern.find` 的 24 种朝向枚举；玩家正常摆法（站环心朝外放）与原版一致，
  但原版理论上还能凑出的几种畸形摆法（如框排/列朝向混搭）这里不激活。
- 存档不存方块状态/实体，只存 uint8 方块 id 数组（生物、掉落方块、箱子/熔炉这类方块实体都没做）。
- **下界要塞未做**，所以烈焰人/凋灵骷髅不会自然刷新（实体与 AI 都在，`--summon blaze` 可手动放）。
- **菜单只有 Windows 版**（GDI 实现）；Linux 上启动直接进世界，Esc 退出（见 README「Linux」）。
- AI 还缺的 Goal：繁殖/跟随父母/吃草/开门破门、爬墙（蜘蛛）、上下船、坐船/骑乘；
  寻路没有门/铁栏杆等特殊节点（铁栏杆按 FENCE 处理）、没有"挖开阻挡"（BreachGoal）。

### 最近一次验证

- **2026-10-06 方块更新 / 生物 / Linux**（本轮）：
  - `blockupdatetest` 15 项全过（沙子下落与落地、整列坍塌、火把/墙火把失去支撑、
    紫颂悬空被破坏、生成写入不产生更新）；`portalselftest`/`portalflowtest`/`meshtest` 全过。
  - 生物：`--testbed --zoo` 一排生物实拍（`build/zoo_win.png`）——牛/羊/鸡/猪/僵尸/骷髅/
    苦力怕/蜘蛛贴图与比例都对；自然刷新实测（`--pos 200,80,200`）：白天 4 只被动生物、
    夜里 61 只敌对生物，亮度与方块规则符合原版。
  - PNG 解码器换成自带的之后，与原 WIC 路径**逐像素相同**（同一场景 0 像素差异，
    `tools/imgdiff.py`）。
  - Linux（WSL Ubuntu 24.04）：`cmake -G Ninja -S . -B build-linux` 编译通过，
    `blockupdatetest`/`portalselftest`/`meshtest`/`portalflowtest`/`endprobe` 全过；
    Xvfb + lavapipe 无头运行出图，与 Windows 同场景对比：石台场景 6% 像素不同
    （全部 ≤8 色阶，平均 RGB 142.16 vs 142.15），差异来自软件光栅化器而非代码。
  - 踩到的坑：① mobmodel 的 uv 一开始按**顶点序号**取（`uu[i]`），而 4 个角点被 6 个
    顶点复用，越界 2 个元素——Release 下直接段错误，Debug/-Og 看不出来。必须按
    **角点下标** `uu[kIdx[i]]` 取。② 图集路径原来用 `"\"` 拼接，Linux 上一张贴图都
    读不到（画面全白、图集高度从 800 掉到 256）；Windows 也接受 `/`，统一改掉了。
    ③ Xlib 有个 typedef `Window` 和本工程的窗口类同名，包含 Xlib 前要用
    `#define Window XWindow` 临时改名；且 Xlib 头会定义 `None/Status/Success` 等宏，
    绝不能从 `vk.hpp` 这种 widely-included 的头里包含。

- **2026-10-06 剔除与网格化一轮改造**（详见 §4.12）：
  - 顶点/三角形（`--screenshot` 结尾的统计）：主世界 163.4 万 → 58.1 万（−64%）、
    末地 28.7 万 → 12.8 万（−55%）、下界 328 万 → 175 万（−47%）；
    同一视角绘制次数：主世界 100 → 83（加载完时 86）、末地 63 → 49、
    下界 74 → 72（其中 2 次来自遮挡剔除）。
  - 画面：主世界/末地/下界/地下四个场景在世界加载完时，与关掉对应优化
    的构建**逐字节相同**（`tools/imgdiff.py`）；贪心网格化只差 mip 边界
    的 ±3 以内（合并面的导数在大三角形内算，比逐格更准）。
  - `meshtest` / `portalselftest` / `portalflowtest` / `endprobe` 全过。
  - 踩到的坑：① 合并面的 uv 会超出图块，片元里要按图块矩形折回平铺，
    且 `vTileRect`/`vWrap` 必须 `flat` 插值（平滑插值在大三角形上会把
    1 插成 0.9999999，折回判断失效 → 整片糊掉）；`water.frag` 的 UBO
    漏声明 `texSharp` 会让 `tileRect` 整体错位。② 遮挡剔除里"面被挡死"
    **不等于**邻居看不见 —— 那一侧的表面本身就是洞壁；更激进的
    "我方整面实心 → 邻居也看不见"实测会误剔，已撤掉。

- **2026-10-01 sRGB 工作流改造**（详见 §4.11）：
  - 图集/菜单贴图改 `R8G8B8A8_SRGB` 视图；`terrain.vert`/`water.vert`/`water.frag`
    的乘性常量取 2.2 次幂；`mesher.cpp` 面亮度改原版固定值并删掉失效的太阳项；
  - **逐字节验证**：把纹理直接当颜色输出，石头顶面渲染值 127/116/143/104 与
    `stone.png` 字节一一对应；带完整光照后顶面区域均值 125.49 vs 贴图 125.55
    （`build/flat_stone2.png`）；
  - 末地门合成改盒式平均 + 原版层参数：渲染均值 **(5.6,14.8,25.6)**，原版参考
    (7,16,25)（`build/srgb3_portal.png`）；
  - 顺手修 `water.vert` 的旧图集布局（水面 UV 一直采错，之前水面几乎全黑）：
    `build/res_inv.png` → `build/srgb2_inv.png` 对比可见水变成正常蓝色；
  - 方块条对比 `build/res_blocks3.png` → `build/srgb2_blocks.png`（石头由灰白变正灰）；
  - portalselftest / portalflowtest / endprobe 全过。
- **2026-10-01 火把裂缝 / 末地门原版星空**：
  - `kTileFiles` 里 `end_portal.png` 原先是**填错下标**（T_END_PORTAL 一直走
    程序化占位，png 落到了 T_END_GATEWAY）——这才是"传送门不是原版材质"的根因；
  - 末地门改为建图集时预合成原版 17 层星空（`compositeEndPortal`：end_sky×
    COLORS[0] 打底 + 16 层"星空掩膜×固定色"叠加）+ **反向 sRGB 预补偿**
    （交换链是 sRGB，线性值会被伽马编码，暗贴图会被洗亮）；
  - 着色器 UV 去掉半像素偏移（子矩形面会采到相邻透明纹素 → 火把裂缝）；
  - 实测：末地门平均 RGB (17,19,30)、82% 暗部像素，与原版参考 (12,22,29)/86%
    一致（`build/srgb_portal2.png`）；火把柱面连续无裂缝（`build/fix3_torch.png`）；
    火焰整片、墙上火把 22.5° 外倾（`build/srgb_portal2.png`）；
    portalselftest / portalflowtest / endprobe 全过。
- **2026-10-01 图集原生分辨率改造**：
  - `Atlas` 从"32px 固定网格"改为"每 tile 独立矩形 + 书架式打包"，着色器经
    UBO `tileRect[128]` 查表取 UV（terrain.vert）；地形 UBO 扩到 `sizeof(TerrainUbo)`
    （tileRect 放最后，水/天空等其他 UBO 布局不受影响）；
  - end_portal 256×256 原样入图集，传送门呈现高清星空（`build/res_portal.png`）；
  - 16 种方块横排实拍无 UV 错位/渗色（`build/res_blocks3.png`）、
    火焰/火把/祭坛全部正常（`build/res_torch.png`）、背包图标正常
    （`build/res_inv.png`）；portalselftest / portalflowtest 全过。
- **2026-10-01 修复**（火细线 / 火把 22.5° / 传送门星空）：
  - `cmake --build build` 通过，portalselftest / portalflowtest / endprobe 全过；
  - 火焰十字面片实拍恢复整片原版火焰（`build/fix2_torch.png`）；
  - 墙上火把按原版 22.5° 外倾、贴墙而立（`build/fix2_torch.png` 顶部、
    `build/fix2_portal.png` 祭坛柱四面）；地面火把近拍与原版 UV 逐像素一致
    （`build/fix2_floor.png`）；
  - 末地门贴图改为盒式均值+增益的连续暗星空（`build/fix2_portal.png`）。
- **2026-09-30 第三批**（详见文件头「最后更新」）：
  - `cmake --build build` 通过，portalselftest / portalflowtest / endprobe 复测全过；
  - **近景清晰**：Intel UHD 770（`--gpu-index 0`）实拍对比 `texSharp:0` vs `16`
    （`build/graze_off.png` / `build/graze_on.png`）——掠射角下旧路径中近景发糊，
    新路径近处像素清晰、远景不变；视频设置滑块正常（`build/menu_video3.png`）；
  - **祭坛不悬空**：种子 12345 / 424242 / 777 从高处实拍（`build/podium3_s*.png`）
    祭坛均落在地表，传送门贴图为原版星点（`build/podium_close.png`）；
  - **火把**：高空测试台实拍（`build/torch_test2.png`）——地面火把细杆、
    四向墙上火把贴墙伸出且头朝外、火为十字面片 + 原版贴图。
- **2026-09-27 第二批 4 项**：
  - `cmake --build build` 通过，`portalselftest` / `endprobe` 复测全过；
  - 紫颂植株/花：主世界高空摆测试台截图（`build/chorus3.png` 全景、`build/chorus4.png`
    特写）——茎是 8/16 细柱 + 凸起、孤立植株是 8×8 小方块、花是花杯形；
  - 显卡选择：`--menu-shot build/menu_video.png --menu-screen 5` 显示"显卡：自动（优先
    独显）"按钮 + "重启生效"提示；本机枚举到核显 + RTX 4070，自动选中 GPU[1]；
  - 控制台指令：管道输入实跑通过——`help` / `where`（坐标+维度）/ `tp 120.5 70 30.5`
    （脚底精确落在 y=70）/ `tp nether` / `tp end` / `tp 主世界` / 未知指令报错，
    中文参数在管道（UTF-8）与真控制台（GBK→UTF-8 转换）两条路径都通；
  - `tools/tag_ci.ps1 -DryRun`：正确解析出 `ci0.4.0-snapshot-4`；脏工作区被拦
    （提示先提交或 `-Force`）；标签已存在且指向别的提交时报错提示先升版本号。
- **2026-09-27 末地 bug 批量修复**：
  - `cmake --build build` 通过；`portalselftest` 全过；
  - `portalflowtest` 新增 2 个用例全过：挖框架 → 环内 end_portal 全碎、挖门方块 → 相连门方块全碎；
  - 截图验证：柱子从岛面立起且顶部接到铁栏杆笼子与火（`build/fix_pillars.png`）、
    返回传送门基岩环完整（`build/fix_podium.png`）、末地平台完整 5×5
    （`build/fix_platform2.png`）、背包里紫颂/铁栏杆/火把图标正常（`build/fix_inv.png`）。
- **MSVC 构建 + CI 发布链路**（新增）：`.github/workflows/release.yml` 的
  configure/构建/打包/校验步骤在本机用 `vcvars64` + Ninja 实跑通过，产出
  `releases/Opencraft-Installer.exe`，PE 里不含 `VCRUNTIME140.dll`/`MSVCP140.dll`（`/MT` 生效）；
- **volk 自带化验证**：造了一个只含 `Include/vulkan` + `Include/vk_video`（**无 Volk**）
  的 SDK 目录，MSVC 全量构建通过 —— 证明已不依赖 SDK 的 volk 布局；
- `cmake --build build` 通过；
- `build/opencraft.exe --version`、`--menu-shot build/menu.png` 正常；
- `build/endprobe.exe 12345 7`：末地地形有山有谷（顶面高度 40~64），有黑曜石柱/传送门/紫颂统计；
- `build/portalselftest.exe`：朝向规则 + 16 项方环几何用例全部通过；
- `build/portalflowtest.exe 12345`：19 项通过（末地门插眼激活链路、下界门造门/回程落点）；
- 截图脚本：`--dim end` / `--dim nether` + `--pos/--yaw/--pitch` + `--screenshot out.png --no-ui`；
- 末地门端到端：`--place` 摆 12 框 + `--use-eye` 插最后一个眼 → 中心 3×3 变 end_portal，
  玩家站进去 → 传送到末地平台（截图 `build/ring_lit.png` / `build/ring_tp.png`）；
- 下界门端到端：黑曜石框 + `--light-portal` → 玩家走进去 → 下界生成新门并落在门内。

## 3. 目录与文件职责

### 构建与资源

| 路径 | 作用 |
|------|------|
| `CMakeLists.txt` | 主程序 + `extract_assets` / `endprobe` / `portalselftest` / `portalflowtest` 四个小工具 + 打包目标 |
| `cmake/gen_version.cmake` | 构建时拼版本号（`version.txt` 的 `[current]` + 时间戳） |
| `version.txt` | 版本名唯一来源；只改 `[current]` 行，别写时间戳 |
| `shaders/*.vert/.frag` | GLSL，构建时 `glslc` 编译成 `build/shaders/*.spv` |
| `assets/block`、`assets/item`、`assets/gui` | 官方贴图（**不入库**，由 `extract_assets` 从玩家 jar 提取） |
| `tools/extract_assets.cpp` | 从 `minecraft.jar` 提取所需贴图到 `assets/`；新增方块贴图要加在这里 |
| `tools/pack_7z.ps1`、`7-Zip/` | 7-Zip SFX 自解压安装包（`releases/Opencraft-Installer.exe`） |
| `.github/workflows/release.yml` | CI：推 `ci*` tag 时在 windows-latest 用 MSVC 构建并发布安装包 |
| `third_party/volk/volk.h`、`volk.c` | 自带的 volk（MIT，v1.4.357.0 随 SDK 版本）。**不用 SDK 里那份**：SDK 装的是 `Include/Volk`（大写），且不同装法/精简过的 SDK 可能没有它，CI 上会 `fatal error C1083: 无法打开包括文件 volk.h`。自带一份让本地和 CI 行为一致 |
| `docs/standards.md` | 世界结构规范（常量来源、命名空间、存档格式） |
| `docs/minecraft_1.0_reference.md` | MC 1.0 对照参考 |
| `PROJECT_STATE.md` | **本文件** |

### 小工具（开发用，不随包分发）

| 工具 | 作用 |
|------|------|
| `tools/endprobe.cpp` → `build/endprobe.exe` | 打印末地高度剖面、方块统计、密度中间量（排查生成问题先跑它） |
| `tools/portalselftest.cpp` → `build/portalselftest.exe` | 末地门朝向规则 + 5×5 方环几何自检（16 个用例，期望值写死，不调用被测函数） |
| `tools/portalflowtest.cpp` → `build/portalflowtest.exe` | 传送门流程自检：末地门插眼激活链路（真 World）+ 下界门造门/回程落点 |

### 核心源码 `src/`

**基础/规范**

| 文件 | 作用 |
|------|------|
| `specs.hpp` | 世界常量唯一来源：`CHUNK_SIZE=16`、`WORLD_HEIGHT=128`、`SEA_LEVEL=62`、坐标换算（floor 语义） |
| `dimensions.hpp` | 三维度表：Y 范围、坐标缩放、天光/天花板、雾与天空色 |
| `util.hpp` | 小数学库（`Vec3`/`Mat4`/`Frustum`）、`hash32`、`Rng` |
| `blocks.hpp` | 方块注册表（id/中文名/是否透明/碰撞/发光/衰减/六面图块）+ 图块枚举 + 末地框架朝向辅助 |
| `items.hpp` | 物品注册表 + 图块基址 |
| `entity.hpp` / `entities.hpp` | 实体基类与末影龙/末影水晶等具体实体 |

**世界与生成**

| 文件 | 作用 |
|------|------|
| `world.hpp/.cpp` | 世界：按维度的区块表、任务队列、工作线程、方块读写、实体管理、区块卸载 |
| `generator.hpp/.cpp` | **维度分发入口** `gen::generateForDim`；主世界生成（简易）；下界/末地转发到各自文件 |
| `nethergen.hpp/.cpp` | 下界生成（原版密度 + 表面规则 + 下界荒地） |
| `endgen.hpp/.cpp` | 末地生成（原版密度 + 黑曜石柱 + 返回传送门 + 末地平台 + 外岛 + 紫颂） |
| `mcnoise.hpp/.cpp` | **原版噪声复刻**：`McCnRandom`(LegacyRandomSource)、`McImprovedNoise`、`McSimplexNoise`、`McPerlinNoise`、`McBlendedNoise` |
| `noise.hpp/.cpp` | 早期简易 value noise（主世界还在用） |
| `mesher.hpp/.cpp` | 区块网格化：逐面 cullface、AO、多 element 方块模型（末地门框架）、12 字节顶点 |
| `blockupdate.hpp/.cpp` | **方块更新**：PP/NC 更新传播（顺序 + DFS + 上限）、计划刻队列、受重力方块与附着类方块的接收行为；`mctick.hpp/.cpp` 里的紫颂存活判断也搬到这里共用 |
| `mobs.hpp/.cpp` | 生物基类 `Mob`（每整刻 = 原版 serverAiStep + travel：goals → navigation → move/jump/look 控制器 → 移动输入 → 重力/阻力）与 10 种生物、弹射物（箭/小火球）、玩家实体 `PlayerEntity`；另有近似光照 `mobs::lightAt`（供刷怪判定） |
| `mobai.hpp/.cpp` | **原版 AI 架构**：`ai::GoalSelector`/`Goal`、`MoveControl`（rotlerp 转身 + setSpeed→zza）、`LookControl`、`JumpControl`、`PathNavigation`（跟随/卡住检测），以及 FloatGoal/PanicGoal/RandomStrollGoal/WaterAvoidingRandomStrollGoal/LookAtPlayerGoal/RandomLookAroundGoal/MeleeAttackGoal |
| `mobpath.hpp/.cpp` | **原版寻路**：Node/Target/BinaryHeap/Path/PathFinder（A*，h×1.5、walkedDistance、maxVisitedNodes）+ `WalkNodeEvaluator`（PathType、跳跃/下落节点、对角邻居） |
| `mobmodel.hpp/.cpp` + `mobmodel_data.inc` | 原版实体模型数据（part 层级 / cube 包围盒 / texOffs / mirror）与盒模型生成；数据抄自 `docs/mob_models.md` |
| `mobspawn.hpp/.cpp` | 生物刷新（原版规则：亮度、方块、维度、距离、数量上限）与 `summon` |
| `platform.hpp/.cpp` | 平台抽象：键位枚举、`exeDir`、线程优先级、控制台输入线程 |
| `window_win32.cpp` / `window_x11.cpp` | 两个窗口后端（同一 `Window` 接口） |
| `png_decode.cpp` / `png.cpp` | 自带的 PNG 解码（含 inflate）与编码（截图用），不依赖 WIC/zlib |
| `mctick.hpp/.cpp` | 方块随机刻（紫颂花生长） |
| `portal.hpp/.cpp` | 下界门框架识别与点燃、末地门方环识别与激活、门破碎、**下界门目的地找/造门** |
| `raycast.hpp/.cpp` | 方块射线检测 |
| `save.hpp/.cpp` | 存档（v3，按维度分段） |

**渲染与界面**

| 文件 | 作用 |
|------|------|
| `vk.hpp/.cpp`、`volk_impl.c` | Vulkan 设备/交换链/内存/管线基建 |
| `renderer.hpp/.cpp` | 渲染器：地形/水/天空/实体/UI/背包、区块缓冲上传、截图 |
| `atlas.hpp/.cpp` | 图集：读 `assets/block/*.png`，缺失/需要程序化的图块代码生成 |
| `shaders/terrain.vert` 等 | 着色器；顶点位置是 1/16 格单位的整数，着色器除以 16 |
| `window.hpp/.cpp` | Win32 窗口、输入、鼠标捕获 |
| `menu.hpp/.cpp` | 主菜单/存档选择/新建世界/选项/暂停 |
| `camera.hpp/.cpp` | 相机（`yaw=0` 朝 +Z 即南，`forward=(sin yaw,*,cos yaw)`） |
| `player.hpp/.cpp` | 玩家物理、碰撞、飞行、游泳 |
| `png.hpp/.cpp` | PNG 读写（截图、贴图） |
| `version.hpp/.cpp` | 版本号读取与打印 |
| `main.cpp` | 命令行参数、主循环、菜单状态机、维度传送、方块交互 |

## 4. 关键机制与坑（改代码前必读）

### 4.1 方块只能是 uint8 id，没有状态位

原版一个方块可以有 facing/age/half 等状态，本工程 `Chunk::blocks` 是纯 `uint8_t`。
所以：

- **末地传送门框架**：朝向 × 有眼 = 8 种状态，展开成 8 个独立 id
  （`B_END_PORTAL_FRAME` 是"南"，`_W/_N/_E` 是其余三向，`B_END_PORTAL_FRAME_EYE*` 同理）。
  辅助函数在 `blocks.hpp`：`blockIsPortalFrame` / `blockFrameHasEye` / `blockFrameFacing` /
  `frameId(facing,eye)` / `frameFacingDir`。
  - **注意**：`B_END_PORTAL_FRAME` = 27，是"南"，**不是**原版的默认朝北。
  - 新增框态时必须同时改 `Block` 枚举、`BLOCK_DEFS` 和
    `blocks.hpp` 末尾的 `static_assert`（数组长度必须等于 `B_COUNT`）。
- 如果以后要加更多有状态的方块，正确做法是给方块加独立的状态字节/调色板，
  而不是继续把状态展开成 id（会把 id 空间撑爆，并让存档变大）。

### 4.2 生成必须"逐区块自足、与顺序无关"

`World::generateChunk` 在工作线程里只写自己这一块，**不会**写邻块。
所以：

- 地形（密度函数）天然自足，没问题。
- **结构/植被跨区块时要"把邻域候选都算一遍，只保留落在本区块的方块"**：
  外岛就是遍历 3×3 邻域区块各跑一次 `end_island`，`put()` 丢弃不属于本块的方块。
- 不要图省事在生成时 `world->setBlock`，那会依赖邻块是否已生成、且顺序不同结果不同。

### 4.3 末地密度函数的求值顺序不能"化简"

`noise_settings/end.json` 的 `slide` 形态是：

```text
s = y_clamped_gradient(56,312, 1,0) * (sloped_cheese + 23.4375)
t = lerp(y_clamped_gradient(4,32, 0,1), -0.234375, s)
final = squeeze(0.64 * (t - 23.4375))
```

**必须保持 `lerp -> 减 23.4375 -> 乘 0.64 -> squeeze` 的顺序。**
把 `-23.4375` 折进 `lerp` 的参数里（代数上看着等价）会让整片地形变成实心——
这个 bug 实际发生过，排查花了不少时间（症状：`endprobe` 里密度恒为 0.45833）。

其他相关事实：

- 末地的格是 **水平 8×8、垂直 4** 方块（`size_horizontal=2,size_vertical=1`）；
  但 `NoiseInterpolator` 对每个方块插值，所以本实现按 **4×4×4** 采样再三线性插值，结果等价。
- `end_islands` 的 simplex 种子固定是 `LegacyRandomSource(0)`（先 `consumeCount(17292)`），
  **与世界种子无关**，这是原版行为，不要"修"成用世界种子。
- `base_3d_noise` 用 `legacy_random_source: true` 的路径：`LegacyRandomSource(世界种子)`，
  再按 min/max/main 的顺序创建三段 `PerlinNoise`（八度 -15..0、-15..0、-7..0）。
  **三段的创建顺序不能变**，否则噪声完全不同。
- 末地主岛半径约 100 方块；中央 `-32..32` 区域几乎是整柱实心（原版也这样）。

### 4.4 随机刻与特征种子

- 紫颂花生长靠 `mctick::tickRandomBlocks`，主循环每帧调用（内部按 20TPS 累积）。
  它复刻 `ChorusFlowerBlock.randomTick`，用 `B_CHORUS_FLOWER`(活) / `B_CHORUS_FLOWER_DEAD`(枯萎)
  两种 id 代替原版的 `AGE 0..4 / 5`；因此分叉次数比原版略多（原版 age>=4 不再分叉）。
- 外岛/紫颂的 per-chunk 随机源用"世界种子 + 区块坐标 + 固定盐"合成，
  没有复刻原版的 `setFeatureSeed(decorationSeed, globalFeatureIndex, step)`。

### 4.5 方块模型（多 element）

`mesher.cpp` 的 `buildParts()` 把一个方块拆成若干长方体（原版 element，1/16 格坐标）：

- 普通方块 = 一个 `0..16` 的整块。
- 末地门框架（原版 `end_portal_frame(_filled).json`）：element0 = `0..16 × 0..13 × 0..16`，
  顶面 frame_top（**无 cullface**）、底面 end_stone（cullface）、四侧 side 用 UV `[0,3,16,16]`
  （侧面贴图顶部 3 行是透明的，正好跳过）；有眼时再加 element1 = `4..12 × 13..16 × 4..12`
  （**居中**、凸出框顶 3/16），顶面 UV `[4,4,12,12]`（cullface up）、四侧 UV `[4,0,12,3]`（无 cullface）。
- **cullface 是唯一的邻居遮挡开关**：没写 cullface 的面（框顶面、眼块四周）永远画，
  不能按"邻居不透明"一刀切，否则把框架贴着方块摆就会缺面。
- UV 用原版 FaceBakery 的角点顺序（见 `mesher.cpp` 的 `kCorner` / `kUvCorner`），
  不能用坐标轴各自反推符号（NORTH/EAST/DOWN 是镜像的）。
- `TerrainVertex` 是 12 字节：`x,y,z,w`(int16，**1/16 格单位**) + `u,v,tex,shade`(u8)。
  着色器里 `p = inPos.xyz / 16.0`；uv 是原版语义 0..16，着色器再偏移半像素取纹素中心。

### 4.6 相机 yaw 约定

`forward = (sin yaw, sin pitch, cos yaw)`：`yaw=0` 看 **+Z（南）**，`yaw=π/2` 看 +X（东）。
放末地门框架时朝向取玩家朝向的反方向（`portal::frameIdForPlacement`）。

### 4.7 末地门朝向规则（踩过坑）

原版 `BlockPattern` 要 12 个框架同 Y、都有眼、**朝向指向环心**（要塞布局实证：
北边朝南、南边朝北、西边朝东、东边朝西）。曾经的 `frameRequiredFacing` 写成
"先看 oz 再看 ox"，导致西/东边非中格的 6 个框要求了错误朝向 —— 玩家按原版摆法
永远激活不了。**自检必须写死期望值**，别用被测函数生成期望（`portalselftest` 已改成这样）。

### 4.8 图集图块分配（踩过坑）

- `T_ITEM_BASE` 在 `items.hpp` 里定义为 `T_COUNT`（include 了 blocks.hpp），
  物品图标永远排在全部方块图块之后。**别写死数字**：曾经写死 37，后来方块图块
  扩到 41，物品图标把紫颂植株/紫颂花/铁栏杆/火把的贴图覆盖成了工具图标
  （症状：末地的铁栏杆笼子显示成木剑、背包里多出四个"工具方块"）。
- 新增方块要同时检查 `kTileFiles`/`kTileTint` 数组元素个数与 `T_COUNT` 一致。

### 4.9 紫颂植株/花的模型（原版 multipart）

- **植株** = 1 个内层 cube `[4,4,4]->[12,12,12]`（每个面用 `#inside`，即植株贴图）
  + 每个连接方向最多 6 个凸起 `[4,4,0]->[12,12,4]`（轴向平移到对应边，盖面 cullface、
  贴内层那面不画）。连接规则 = `ChorusPlantBlock#connectsTo`：邻块是紫颂植株/紫颂花，
  朝下额外认末地石。所以一株孤立的植株只是一颗悬空的 8×8 小方块。
- **花** = `template_chorus_flower` 的 6 个 element（花杯形、四角内凹），`#bottom` 用
  植株贴图、`#texture` 用花贴图，全部面无 cullface。枯花只是换了贴图。
- 原版这几个模型写了 `"ambientocclusion": false`：按整格邻块算 AO 会在细柱子上糊出
  暗角，`noAoBlock()` 对紫颂三个方块直接给满 AO 等级。
- 原版 element 没写 uv 时由 from/to 推导默认 UV（`defaultUv()`）：north/east/down 的
  u 是反向的（16-x / 16-z / 16-x），照抄 `kCorner` 那套角点顺序即可对上。
- 三个紫颂方块在 `BLOCK_DEFS` 里 `opaque=false`：别去剔邻居的面，也别挡光；
  `solid=true` 保留碰撞。
- 注意 `buildParts()` 的 `Box parts[]` 大小是 `kMaxParts=8`（植株 1+6，花 6），加新
  多 element 模型时数一下上限。
- 火把/火：地面火把 = `[7,0,7]->[9,10,9]` 杆（原版 template_torch 逐字转写，
  侧面 [7,6,9,16] 本来就含顶部焰心行，不是错位）；墙上火把 = 原版 22.5° 斜杆，
  走 `Box::custom` 自定义 8 角点（竖杆绕 z 转 -22.5° 再按朝向绕块中心转 y），
  UV 与地面火把一致；火 = 两片 1px 十字面片，**必须画薄板的一对大面**
  （板A 画 F_NX/F_PX、板B 画 F_NZ/F_PZ），画成薄边整片火就只剩细线。
- 原版贴图进图集的尺寸规则（atlas.cpp）：16×N 动画条取首帧（fire_0）；
  非正方形裁左上角正方形；末地门走专用合成（见 §4.11），不参与通用降采样。

### 4.10 近景清晰纹理（texSharp）

- 采样路径有两条：`atlasSampler_`（mip+各向异性，远景）与 `atlasNearSampler_`
  （锁 mip0 最近邻，近景）。片元着色器按 `vDist` 选：`texSharp.x`（默认 16 格，
  options.txt `texSharp:`）内走清晰采样，之后 6 格线性过渡回 mip 采样；
  `texSharp.x<=0` 时全走 mip（功能关）。
- 背景：部分核显驱动（实机 Intel UHD 770 复现）开各向异性后在近处也选高 mip，
  方块近看发糊；两条采样路径 + 距离切换在任何驱动上都保证近景清晰。
- 超尺寸原版贴图进图集的规则（atlas.cpp，2026-10-01 起图集按原生分辨率排版）：
  图集不再固定 16×16 网格——每个 tile 按png 原生尺寸独立排版（书架式打包，
  `Atlas::Rect` 记录像素矩形），着色器经 UBO `tileRect[]` 查表取 UV，材质包换成
  32×32/64×64 或混入高清图都不用改代码。动画条（fire_0 16×512）取首帧；
  程序化贴图按材质包主流分辨率生成。

### 4.11 sRGB 工作流（颜色不对先看这里）

- **交换链是 `B8G8R8A8_SRGB`**（`vk.cpp selectSwapFormat` 优先选它）。这意味着
  着色器写出的值被硬件当成**线性**、输出时做一次 sRGB 编码。
- 所以贴图必须用 **sRGB 视图**采样：图集图（`atlasImage_`/`atlasView_`）与菜单图
  （`menu.cpp` 的 `img_`/`view_`）都是 `VK_FORMAT_R8G8B8A8_SRGB`。字节是"显示色"，
  采样时硬件解码成线性 → 着色器算 → 交换链再编码 → **显示值＝贴图字节原值**。
- **乘性常量要取 2.2 次幂**：原版（以及本引擎原来那套常量）是"显示空间乘法"
  （显示值 = 贴图 × 系数）。线性空间里要得到同样结果必须乘 `系数^2.2`——
  所以 `terrain.vert`/`water.vert` 里 `vShade = pow(inMeta.w/255, 2.2)`，
  `water.frag` 的水色 `pow(vec3(0.40,0.62,1.10), vec3(2.2))`。
- **加性/结果型常量不用改**：天空色、雾色、水下色调是"算出来的颜色"，
  直接输出即可（显示值 = encode(常量)，改前改后一致）。
- 面亮度表（`mesher.cpp kFaceBright`）用原版固定值：顶 1.0、底 0.5、南北 0.8、
  东西 0.6，配合上面的 2.2 次幂 → 开阔地顶面显示值 = 贴图原值。**不要再加"太阳
  方向"项**：原来那行 `reinterpret_cast<const Vec3*>(&kNormal[f][0])` 把 `int[3]`
  当 float 读，点积恒为 0，等于所有面固定乘 0.775（死代码，已删）。
- 排版系数：`kAoBright` 仍是 {0.42,0.64,0.82,1.0}（原版 AO 更狠，这里刻意温和）。
- 末地门（`compositeEndPortal`）也按这个语义：**输出字节 = 原版着色器算出的显示色**，
  不要再做"反向 sRGB 预补偿"（那是 UNORM 时代为抵消双重编码打的补丁，现在是负负
  得负）。合成方式：底色 `end_sky×COLORS[0]` + 16 层"星图 × COLORS[i]"，
  层变换照抄原版 `end_portal_layer()`（SCALE_TRANSLATE 0.5x+0.25 → 平移 17/i 与
  `(2+i/1.5)·GameTime·1.5`（静态取 0.35 相位）→ 旋转 (i²·4321+i·9)·2° → 缩放
  `2·(4.5-i/4)`），每层在覆盖范围里做 **4×4 盒式平均**（原版是 textureProj + mip，
  点采样只会零星命中 0.5% 的亮星，整体比原版暗一倍）。
- 排查套路（本次就是这么做出来的）：把可疑量当颜色输出（`outColor = vec4(vShade,…)`
  或 UV / tileRect），截图后用 `tools/pxsample.py` 读像素；注意交换链会做一次
  sRGB 编码，**读到的字节要 `decode` 回线性再判断**。判断"贴图有没有走对"最快的
  办法是把纹理直接当颜色输出，与 png 字节逐字节对比（stone 的 127/116/143/104 能
  一一对上就说明整条往返是准的）。

### 4.13 方块更新（PP / NC / 计划刻）

- 入口只有 `World::setBlock`（带 `bu::U_*` 标志），生成器直接写 `Chunk::blocks`
  不经过它 —— 所以世界生成时悬空的沙子不会自己掉（原版行为，方块更新.txt 明说）。
- 传播顺序：NC 更新核 **西、东、下、上、北、南**；PP 更新 **西、东、北、南、下、上**；
  深度优先（栈里每帧只推一个方向，新产生的更新先入 pending 再压栈），
  总量超 `kMaxUpdates`(65536) 后不再新增。
- 接收方只实现了本工程已有方块会受影响的部分：
  受重力（沙子/沙砾/龙蛋 → 1 刻后变 `FallingBlock` 实体）、附着（火把/墙火把/紫颂/火）。
  红石与流体没做，所以 `receiveNC` 目前是空的（结构留着，加红石时只补它）。
- 计划刻队列在 `World::blockTicks()`，由 `World::tickEntities` 在整刻时执行。
- **踩过的坑**：`FallingBlock` 落地时如果目标格是火把之类非固体方块，原版会掉落为物品；
  本工程还没有物品实体，所以这种情况直接消失（会"少一块"，是已知差距）。

### 4.14 生物 AI 与物理（2026-10-11 起是原版移植）

**每整刻的执行顺序**（`Mob::tick`，逐条对应原版）

```
serverAiStep：goalSelector.tick() → navigation.tick() → moveControl.tick()
              → jumpControl.tick() → lookControl.tick() → aiExtra()
travel：applyInput(zza*=0.98) → moveRelative → move(碰撞/摔落) → 重力/阻力
```

- **Goal 系统**（`src/mobai.*`）：优先级 + `Flag{MOVE,LOOK,JUMP,TARGET}` 互斥锁，
  与 Java 版 `GoalSelector` 同构；各生物的 `registerGoals()` 抄同名方法的表
  （猪羊：Float 0 / Panic 1.25 / Stroll 6 / LookAt 7 / RandomLook 8）。
- **速度的坑（务必记住）**：原版 `Mob.setSpeed(s)` **同时**把前进输入 `zza` 设为 s，
  而地面加速度 = `speed * zza` = **speed²**（摩擦 0.6 时稳态位移 ≈ 2.2·speed²
  格/刻，猪 0.25 → ≈2.8 m/s）。只写 `speed` 不写 `zza` 的话生物会"转身但不走路"。
- **MoveControl**：每刻朝目标点 `rotlerp` 转身（上限 90°/刻，所以不会出现"倒着走"），
  距离够近（yd > maxUpStep=0.6 且水平 < 1 格）就跳。
- **LookControl / BodyRotationControl**：`yHeadRot` 是头部绝对朝向，渲染时取
  `yHeadRot - yaw` 并 clamp ±50°（getMaxHeadYRot）；**模型空间 ry 正方向与世界 yaw
  相反，所以传进 `MobAnimState::headYaw` 时要取负**（否则头永远扭向另一边）。
  生物的 `pitch`（正=低头）由 LookControl 写，模型 rx 正方向正好也是低头。
- **寻路**（`src/mobpath.*`）：A* + `WalkNodeEvaluator`。`getPathTypeOfMob` 收集实体
  包围盒覆盖的格子类型后，**必须按 PathType 的枚举顺序**取"最大扣分"（原版是 EnumSet
  的迭代顺序）—— 按空间遍历顺序取最后一个是错的：OPEN(1) 排在 WALKABLE(2) 前面，
  站在地上的生物会被判成"悬空"，起点邻居一个都找不到，A* 直接失败。
- PathType 与方块对应：空气/火把=OPEN，实心地面之上=WALKABLE，水=WATER(8)、
  岩浆/火=负分、铁栏杆=FENCE(不可通行)、树叶=LEAVES(不可通行)。
- 调试：`OPENCRAFT_MOB_DEBUG=1`（每 20 刻打 mob 状态）、`OPENCRAFT_PATH_DEBUG=1`
  （起点/邻居）、`OPENCRAFT_STROLL_DEBUG=1`（游走 goal 找点与结果）。
- **性能**：寻路很贵（一次 A* 最多 512 节点，每节点十几次类型查询）。两条已经踩过的优化：
  ① 单格 PathType 必须缓存（`getCachedCellType`，否则每格要重复扫 9 邻域，
  70 只怪时帧时间 ~19ms → ~10ms）；② 离玩家 48 格以外的生物不跑 AI（只留物理）。
  实测 6 区块视距 + 72 只怪：2500 帧 24.6s（≈10ms/帧）。
- 模型 UV 展开的那套坑见下一节（面-角点配对、贴图旋转 90° 的教训）。

### 4.15 实体模型与贴图（UV 展开）

- 实体管线的 uv 是 **0..255 映射整张贴图**（`shaders/entity.vert` 里 `/256`），
  不是地形那套 0..16 —— 因为生物要按原版 texOffs 精确取到 1 像素。
- **面的 uv 必须按角点位置推导**（`buildMobMesh` 里逐面算 su/sv），不能拿一套
  固定的 {u0,u1,u1,u0} 模式去套所有面的角点顺序：`kFace` 各面的 c0→c1 方向
  不统一（-X/+Z/顶三个面 c0→c1 沿的是 y 或别的轴），套模板等于把贴图旋转
  90° —— 袖子横条变竖条、腿的鞋底变竖灰条、躯干下摆变紫竖条。
  排查手段：把 mesh dump 成文本、用 Python 软件光栅化同一张贴图，
  软件渲染正确 ⇒ 问题在 GPU 采样路径；软件渲染同样错 ⇒ 问题在几何/uv 生成。
- 展开图约定（同原版 Box）：一行 [西|北|东|南] 连排，相邻面共享边；
  所以 -X 面 u 朝 -z 增、+Z 面 u 随 x 减（从背面看不镜像）；顶/底面 u 沿 x、v 沿 z。
- 模型换算（`docs/mob_models.md` §1.2）：模型像素 → 相对脚部的方块偏移
  `wx=(mx·cos r − mz·sin r)/16, wy=1.501−my/16, wz=(−mx·sin r − mz·cos r)/16`
  （r = 实体 yaw；模型 y 轴朝下，y=24 是脚底）。
- 图集里实体贴图**必须保留原始宽高**（64×64 / 64×32），一旦被裁成正方形，
  所有 `texOffs` 都会错位。
- 生物贴图要一起进 `tools/extract_assets.*` 的清单（`assets/entity/*.png`）。
- **羊分两层**（与原版 `SheepModel` + `SheepFurModel` 一致）：基础层用
  `sheep.png`；羊毛覆盖层用 `sheep_wool.png`，靠 `MobCube.tile` 逐 cube 覆盖
  模型贴图：头 6×6×6 def0.6（z0=-4，比基础头短 2 格，脸才露得出来）、
  身体 8×16×6 def1.75、腿 4×6×4 def0.5（只包上段）。

### 4.16 平台抽象（Windows / Linux）

- 键位统一用 `platform.hpp` 的 `Key`（取值与 Win32 虚拟键码一致，字母数字就是 ASCII，
  所以 `in.keys['W']` 两边通用）。**不要在游戏代码里写 `VK_*`**。
- 窗口只有 `Window` 一个类，两个后端：`src/window_win32.cpp` / `src/window_x11.cpp`，
  由 CMake 按平台挑一个。
- 路径拼接用 `platform::join()`；**图集里统一用 `/`**（Windows 也接受），
  用 `"\"` 拼的路径在 Linux 上一张贴图都读不到。
- Xlib 有个 typedef `Window` 与本工程同名，包含 Xlib 之前要
  `#define Window XWindow` … `#undef Window`；而且 Xlib 会定义 `None/Status/Success`
  等宏，**不要从 `vk.hpp` 这种到处被 include 的头里包含 Xlib**。
- PNG 读写都是自带的（`png_decode.cpp` / `png.cpp`）：不依赖 WIC、zlib、libpng，
  两个平台行为一致（已用 99 张贴图与独立参考解码器逐张比对过）。
- 菜单（GDI）只在 Windows 编译；Linux 上 `main.cpp` 直接进世界。

### 4.12 剔除与网格化（改性能前必读）

按 `TODO.txt` 的顺序逐项做了一遍，落点都在 `src/mesher.cpp`（网格化）与
`src/renderer.cpp`（剔除）。改这块之前先读下面几条坑。

1. **邻接面剔除**（`mesher.cpp` 的 `cullFace` / `onBoundary` / `fluidCull`）：
   早就实现，本次只补了 `meshtest` 自检。注意 cullface 是唯一的邻居遮挡开关，
   原版模型里没写 cullface 的面（框顶、眼块四周）永远要画。
2. **贪心网格化**（`mergeFaceLayer` + `emitRect`）：
   - 只有"整方块 + 整张贴图（uv 恰好 0..16）+ 四角亮度相等"的面才参与合并；
     多 element 模型（火把/火/紫颂/末地门框架）走原来的逐张路径。
   - 四角亮度必须相等：合并后整张只剩四个角插值，与原来的逐格渐变不一致。
   - uv 是 uint8（1/16 格为单位）→ **单个方向最多合并 15 格**，超了会溢出 255。
   - 合并面的 uv 跨多个图块，片元里按 `vTileRect` 折回平铺；`vTileRect`/`vWrap`
     必须 `flat` 插值（平滑插值在大三角形上把 1 插成 0.9999999，折回判断失效，
     画面会整片糊掉），导数取**折回前**的 uv 否则接缝处跳 mip。
   - 水/岩浆单独一层合并，高度不同的水面不共面、不并。
3. **视锥剔除**（`chunkCulled`）：用网格化时记录的 `geomMinY/geomMaxY` 当紧密
   AABB，不再按整柱 0..128 测；雾距判定也跟着变准（整块都超过雾端就整块不画）。
4. **遮挡剔除**（`updateOcclusion` + `chunkOccluded`）：16³ 分段的可见性洪泛。
   分段之间只有"能穿过"才继续往外传；**穿不过去也要标可见**（那一侧的表面
   本身就是洞壁）。贴着未加载区域的分区一律标可见（视线可能从加载区外绕进来）。
   实测在开阔地形几乎没有收益（下界 74→72），真正被实心岩体封住的场景才有效。
   不要再加"我方整面实心就认为邻居也看不见"这种推论 —— 试过，会误剔。
5. **背面剔除**：地形/水都是 `VK_CULL_MODE_BACK_BIT` + `VK_FRONT_FACE_COUNTER_CLOCKWISE`
   （与 mesher 的角点顺序一致）。水管线后来改成 `CULL_MODE_NONE`：网格化只出流体
   顶层顶面，从水下抬头看是背面，开了背面剔除整片水面会消失。天空/UI 本来就不
   需要剔除，实体盒模型刻意关掉。

**验证套路**：`--screenshot` 结尾会打印"区块数/顶点数/三角形数/绘制次数"，
配合 `tools/imgdiff.py a.png b.png [差异图]` 逐像素比对，能确认优化有没有改变
画面。注意截图要等世界加载完（`--frames` 给够，截图放在倒数第 10 帧），
拍早了画面会缺区块、跑两次还不一样，对比出来的噪声会掩盖真问题。

## 5. 常用命令

```sh
# 构建（Ninja）
cmake -G Ninja -S . -B build
cmake --build build

# 冒烟
build/opencraft.exe --version
build/opencraft.exe --menu-shot build/menu.png

# 看末地/下界（截图模式；--frames 大一点等区块生成完）
build/opencraft.exe --dim end    --seed 12345 --pos 0,130,190 --yaw 3.14159 --pitch -0.42 \
                    --render-dist 10 --frames 400 --screenshot build/end.png --no-ui
build/opencraft.exe --dim nether --seed 12345 --pos 0,70,0      --yaw 0.7    --pitch -0.1  \
                    --render-dist 8  --frames 300 --screenshot build/nether.png --no-ui

# 生成自检
build/endprobe.exe 12345 7        # 末地高度剖面 + 方块统计
build/blockupdatetest.exe 12345   # 方块更新自检（15 项）
build/portalselftest.exe          # 末地门朝向规则 + 方环几何
build/portalflowtest.exe 12345    # 末地门插眼激活 + 下界门造门/回程落点

# 调试传送：进入世界后直接切维度（F6/F7 也能在游戏里循环切）
build/opencraft.exe --seed 12345 --tp-dim end --pos 8,80,8 \
                    --render-dist 6 --frames 200 --screenshot build/end.png --no-ui

# 打包
cmake --build build --target package_7z
```

命令行参数（`src/main.cpp` 顶部）：`--seed --dim(overworld|nether|end) --pos x,y,z --yaw --pitch
--render-dist --threads --frames --no-ui --no-vsync --screenshot --menu-shot --menu-screen
--place x,y,z,id --use-eye x,y,z --light-portal x,y,z --break x,y,z --crystal x,y,z --drive
--inventory --inv-page --spawn-portal --tp-dim(overworld|nether|end) --gpu-index --time --version`。

## 6. 约定速查

- 注释、提交信息、用户可见文本一律**简短中文**，一行能说清就不写两行，不复述代码。
- 版本号只改 `version.txt` 的 `[current]`。
- `.ps1` 含中文必须存成 UTF-8 **带 BOM**；`.cpp/.hpp` 存成 UTF-8 **不带 BOM**
  （用 PowerShell `Set-Content` 会写成 ANSI 把中文弄坏，用 `[IO.File]::WriteAllText` +
  `UTF8Encoding($false)`；改完务必确认文件头不是 `EF BB BF`、中文不花）。
- **源码含中文 ⇒ MSVC 必须加 `/utf-8`**（`CMakeLists.txt` 已加）。MSVC 不指定时按系统
  代码页（中文机器是 GBK）解析 UTF-8 无 BOM 源码，某些中文字节会把换行"吃掉"，表现为
  一堆莫名其妙的 `error C2001: 字符串字面量中的换行符`。GCC 默认 UTF-8，所以本地用
  MinGW 永远碰不到，只有 MSVC/CI 会炸。
- 交叉编译器差异（改 `CMakeLists.txt` 时注意）：编译选项要分 MSVC/GCC 两套；`windows.h`
  的 `min/max` 宏要 `NOMINMAX`；`M_PI` 在 MSVC 要 `_USE_MATH_DEFINES`；MSVC 用 `/MT`
  静态链接运行库，玩家机不需要装 VC++ 可再发行组件。
- 第三方头文件（volk）走 `third_party/`，**不要引用 SDK 目录内的头**。SDK 里 Volk 在
  `Include/Volk`（大写 V），而 Windows 文件系统大小写不敏感，写 `Include/volk` 本地能过、
  CI 上找不到；精简过或不同装法的 SDK 甚至没有该目录。
- 提交要少而清晰，一个提交一件事（方便回退），别攒成一个巨型提交。
- 新增方块贴图：加到 `tools/extract_assets.cpp` 的清单、`assets/block/`（本地）、
  `atlas.cpp` 的 `kTileFiles` + `kTileTint`。
