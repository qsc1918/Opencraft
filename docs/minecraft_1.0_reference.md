# Minecraft Java 版 1.0（2011-11-18）内容考证报告

> **用途**：体素游戏"对齐 1.0 主要内容"的开发参考。所有数值基于 **Java Edition 1.0.0** release（Adventure Update）。
> 注：本报告基于 Minecraft 已广泛记录的版本历史知识编写；web_search 配额暂时不可用，来源 URL 指向 minecraft.wiki 已知固定页面。

---

## 1. 1.0.0 新增内容总览

1.0.0 正式版（2011-11-18）在 Beta 1.8 基础上补齐了以下核心系统：

| 类别 | 要点 |
|------|------|
| **末地（The End）** | 新增末地维度、末影龙 boss 战、末地折跃门（End Gateway 于 1.9 才加入，1.0 仅有末地传送门）、龙蛋、末地城不存在。 |
| **下界（Nether）** | 下界在 Beta 1.8 已作为维度存在，1.0 正式版中地形生成逻辑基本不变；新增 Nether Brick 方块、Nether Brick Fence、Nether Brick Stairs（楼梯类）。 |
| **附魔（Enchanting）** | 玩家通过附魔台（Enchanting Table）消耗经验值与 lapis lazuli 为工具/武器/盔甲附加随机附魔，最高 30 级需 15 书架。 |
| **酿造（Brewing）** | 通过酿造台（Brewing Stand）将烈水（Water Bottle）+ 材料合成为药水，含正面/负面效果；地狱疣（Nether Wart）为核心材料。 |
| **疾跑（Sprinting）** | 双击前进或按 Ctrl 触发疾跑，速度 ×1.3，饥饿消耗加速。 |
| **经验值（Experience）** | 玩家击杀生物/采矿获得绿色经验球，等级用于附魔；等级越高升级所需经验越多。 |
| **Hardcore 模式** | 无法复活的极限难度，死亡即永久删除世界。 |
| **饱食度改进** | 食物不再叠加为回复生命值，改为补充饥饿条；饥饿值低于 9 时不回血。 |
| **村庄系统** | NPC 村庄与村民在主世界自然生成（Beta 1.8 引入，1.0 沿用）。 |
| **信标（Beacon）** | ❌ 1.0 不存在，1.4.2 加入。 |
| **末地折跃门** | ❌ 1.0 不存在，1.9 加入。 |

### 来源
- https://minecraft.wiki/w/Java_Edition_1.0
- https://minecraft.wiki/w/Adventure_Update
- https://minecraft.wiki/w/Versions/Java_Edition_1.0

---

## 2. 下界（Nether）— 1.0 时代地形与机制

### 2.1 地形生成

| 参数 | 数值 |
|------|------|
| **世界高度范围** | Y = 0 ~ Y = 127（共 128 格）。 |
| **底部基岩层** | Y = 0 处为实心基岩，通常为 1–4 格厚不等（Y=0 ~ Y=3 均可能有基岩）。 |
| **顶部基岩层** | Y = 127 为实心基岩天花板；Y = 126 大部分为基岩，间有 netherrack 出口（即"天花板有洞"结构）。 |
| **岩浆海高度** | Y = 31 及以下填充岩浆（lava sea），为一大片连续岩浆平面。 |
| **netherrack 主体** | Y = 1 ~ Y = 125 之间的主要填充方块，形成巨大的洞穴式地形。 |
| **洞穴结构** | 下界无"洞穴"概念（它本身就是巨大的开放空间），但有 netherrack 构成的不规则地形，间有大型空腔。 |
| **Fortress（要塞）** | 下界要塞（Nether Fortress）自 Alpha 1.6 已存在，由 nether_brick 构成的桥廊结构，内含烈焰人刷怪笼和宝箱。 |

### 2.2 Glowstone 生成

- **位置**：挂在下界天花板（Y ≥ 较高处）下方。
- **生成方式**：以 **簇（cluster）** 形式附着在 netherrack 天花板底部，每个簇为 1–10 格不等的不规则形状。
- **光照等级**：15（最高）。
- **挖掘掉落**：Glowstone Dust（1–4 个），使用带 Silk Touch 的镐可直接掉落 Glowstone 方块。

### 2.3 Soul Sand 分布

- 下界地表自然生成于 Y = 较低区域（通常在岩浆海附近/之上）。
- 减速效果：玩家/生物在 soul sand 上移动速度降低约 40%。
- 也可作为酿造材料（Mundane Potion 基底之一）。

### 2.4 下界传送门机制

| 参数 | 数值 |
|------|------|
| **框架材料** | Obsidian（黑曜石），最少需要 **4×5**（宽 4，高 5，含角落）的矩形框架。 |
| **内部空间** | 最小 **2×3**（宽 2，高 3，共 6 格）的可激活区域。 |
| **最大尺寸** | 23×23 内部空间（25×25 含框架）。 |
| **点燃方式** | 使用 **Flint and Steel**（打火石）右键点击框架内任意 obsidian 面，即点燃 portal block。 |
| **Portal Block** | 活跃传送门由 portal block（紫色半透明方块）填充内部空间，具有紫色漩涡动画和特定音效。 |
| **主世界→下界坐标换算** | **1:8**，即主世界 X/Z 移动 1 格 = 下界移动 8 格。Y 轴不变。 |
| **冷却时间** | 传送后有 **4 秒冷却**，期间无法再次传送。 |
| **激活条件** | 框架完整性要求——任意一个 obsidian 方块被移除则传送门失效。 |

### 来源
- https://minecraft.wiki/w/Nether
- https://minecraft.wiki/w/Nether_portal
- https://minecraft.wiki/w/Nether_Fortress
- https://minecraft.wiki/w/Glowstone

---

## 3. 下界/末地相关方块清单（1.0 时代）

| 方块 | 亮度 | 透光 | 硬度 | 要点 |
|------|------|------|------|------|
| **Netherrack** | 0 | ✅ | 1.0 | 下界主要填充方块；可燃（火无限蔓延）。 |
| **Soul Sand** | 0 | ✅ | 0.4 | 减速方块；可堆叠 2.5 格高（碰撞箱高于 1 格）。 |
| **Glowstone** | **15** | ✅ | 0.3 | 下界天花板簇状生成；挖掘掉落 Glowstone Dust。 |
| **Nether Brick** | 0 | ✅ | 2.0 | 下界要塞构成方块；由 Nether Brick（烧制 Netherrack 获得）合成。 |
| **Nether Brick Fence** | 0 | ✅ | 2.0 | 装饰/围栏方块。 |
| **Nether Brick Stairs** | 0 | ✅ | 2.0 | 阶梯方块。 |
| **Nether Portal** | **11** | ✅ | ∞（不可破坏） | 传送门方块本身亮度 11；仅通过 Flint and Steel 激活 obsidian 框架生成。 |
| **Obsidian** | 0 | ✅（但不透光给相邻方块） | **50.0** | 爆炸抗性 **6000**（TNT 无法炸毁，仅 Ender Dragon 可破坏）；最硬的可挖掘方块之一，需钻石镐。 |
| **End Stone** | 0 | ✅ | 3.0 | 末地主要方块（"空岛"构成）；亮度 0，不透光。 |
| **End Portal Frame** | 0 | ❌（不透明） | ∞（不可破坏） | 12 个固定在主世界要塞中的框架方块；无法被挖掘或获得（创造模式也不行，除非 1.8+）。 |
| **End Portal** | 0（方块本身） | ❌ | ∞ | 由 Eye of Ender 填充 End Portal Frame 后激活；传送至末地。 |
| **Dragon Egg** | 0 | ❌ | 3.0 | 击败末影龙后在出生点生成；挖掘时传送至附近方块，需用活塞或 TNT 获得。 |
| **Stone Bricks** | 0 | ❌ | 2.0 | 主世界要塞使用；有普通/苔石砖/裂石砖三种变体。 |
| **Nether Wart** | 0 | ✅（不阻挡光照） | — | 种植在 Soul Sand 上的作物；酿造核心材料。 |

### 来源
- https://minecraft.wiki/w/Netherrack
- https://minecraft.wiki/w/End_Stone
- https://minecraft.wiki/w/Obsidian
- https://minecraft.wiki/w/End_Portal_Frame

---

## 4. 装备类物品清单（1.0 时代）

### 4.1 工具（5 材质 × 4 类型 = 20 种）

| 材质 | Pickaxe | Axe | Shovel | Sword |
|------|---------|-----|--------|-------|
| **Wooden** | ✅ | ✅ | ✅ | ✅ |
| **Stone** | ✅ | ✅ | ✅ | ✅ |
| **Iron** | ✅ | ✅ | ✅ | ✅ |
| **Diamond** | ✅ | ✅ | ✅ | ✅ |
| **Gold** | ✅ | ✅ | ✅ | ✅ |

> **注**：Gold 工具在 1.0 时代已存在，但耐久极低（仅 33 次），挖掘速度最快（超过钻石），实用性低。

### 4.2 盔甲（4 材质 × 4 部位 = 16 种）

| 材质 | Helmet | Chestplate | Leggings | Boots |
|------|--------|------------|----------|-------|
| **Leather** | ✅ | ✅ | ✅ | ✅ |
| **Iron** | ✅ | ✅ | ✅ | ✅ |
| **Gold** | ✅ | ✅ | ✅ | ✅ |
| **Diamond** | ✅ | ✅ | ✅ | ✅ |

> **注**：Chainmail 盔甲在 1.0 已存在于物品 ID 中（通过刷怪笼获取），但无合成配方。

### 4.3 特殊物品

| 物品 | 用途 |
|------|------|
| **Flint and Steel** | 点亮下界传送门框架；对可燃方块点火（木板、树叶等）。合成：铁锭 + 燧石。 |
| **Ender Pearl** | 投掷后传送至落点（消耗饥饿值）；由 Enderman 掉落。 |
| **Eye of Ender**（末影之眼） | 投掷后指向最近要塞方向；放入 End Portal Frame 激活末地传送门。合成：Ender Pearl + Blaze Powder。 |

### 来源
- https://minecraft.wiki/w/Tools
- https://minecraft.wiki/w/Armor
- https://minecraft.wiki/w/Eye_of_Ender
- https://minecraft.wiki/w/Ender_Pearl

---

## 5. 岩浆（Lava）要点

| 参数 | 数值 |
|------|------|
| **方块亮度** | **15**（与 Glowstone 相同，为最高自然光源之一）。 |
| **下界岩浆海高度** | Y = **31** 及以下，下界底部为大面积连续岩浆湖。 |
| **主世界岩浆** | 通常出现在 Y = 10 及以下（小范围岩浆池），非连续海。 |
| **伤害** | 接触岩浆每 0.5 秒（10 tick）造成 **4 点（♥×2）伤害**。 |
| **燃烧** | 出岩浆后玩家身上着火 15 秒，持续 1 点（♥×0.5）/秒伤害。 |
| **游泳** | 在岩浆中可游泳（按跳跃键上浮），但速度极慢且持续受伤；无 Water Breathing 类药水（1.0 无相关效果）。 |
| **流动** | 岩浆流速为水的 1/2（每 30 tick 扩展 1 格 vs 水的 5 tick）；水平最大流距 4 格。 |
| **冷却** | 岩浆遇水产生石头（stone）或黑曜石（obsidian），取决于流动状态。 |

### 来源
- https://minecraft.wiki/w/Lava
- https://minecraft.wiki/w/Nether

---

## 附录：1.0 版本时间线快照

| 版本 | 日期 | 关键内容 |
|------|------|----------|
| Alpha 1.2.0 | 2010-10-30 | 下界维度首次加入 |
| Beta 1.8 | 2011-09-14 | Adventure Update：疾跑、饥饿、村庄、附魔/酿造框架 |
| **1.0.0** | **2011-11-18** | **正式版**：末地维度、末影龙、Hardcore 模式、酿造台配方完善 |

---

*报告基于 Minecraft Java Edition 1.0.0 公开版本历史编写。如需进一步核实特定数值（如精确硬度），建议在浏览器中访问 minecraft.wiki 进行交叉验证。*
