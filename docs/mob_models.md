# Minecraft Java 版 26.2 — 生物/玩家实体模型几何与 UV 数据表

> 目的：给 C++ 侧渲染代码提供一份可直接照抄的模型数据（part 层级、offset、rotation、cube 包围盒、texOffs、形变）。
> 所有数值均逐字取自反编译源码根目录 `D:/i/decompiled/26.2`（包 `net/minecraft/client/model/...`），未凭记忆填写。
> 本文档只描述**成年（adult）**模型；幼年变体（BabyPigModel / BabyCowModel / BabySheepModel / BabyChickenModel / BabyZombieModel）由 `MeshTransformer`（`BabyModelTransform`）在运行时缩放，不在本表范围内。

---

## 1. 约定（最重要，先读这一节）

### 1.1 单位：模型空间 1 像素 = 1/16 方块

`net/minecraft/client/model/geom/ModelPart.java`

```java
public void translateAndRotate(final PoseStack poseStack) {
   poseStack.translate(this.x / 16.0F, this.y / 16.0F, this.z / 16.0F);   // part offset 除以 16
   ...
}
```
`ModelPart.Vertex`（`ModelPart.java`）中 cube 顶点同样除以 16：

```java
public float worldX() { return this.x / 16.0F; }
public static final float SCALE_FACTOR = 16.0F;
```

所以 `addBox(x0,y0,z0, w,h,d)` 与 `PartPose.offset(x,y,z)` 里的数字都是**像素**（16 px = 1 方块），
世界单位 = 像素值 / 16。

### 1.2 模型空间 → 世界空间（相对实体脚部）的换算

`net/minecraft/client/renderer/entity/LivingEntityRenderer.java#submit`（第 72~98 行）的变换序列：

```java
float scale = state.scale;
poseStack.scale(scale, scale, scale);                 // ① 实体缩放（默认 1）
this.setupRotations(state, poseStack, state.bodyRot, scale);   // ② 绕 Y 轴旋转 (180° - bodyRot)
poseStack.scale(-1.0F, -1.0F, 1.0F);                  // ③ Y 轴翻转（X 也被 ② 的 180° 抵消）
this.scale(state, poseStack);                         // ④ 渲染器自定义缩放（默认空；苦力怕有）
poseStack.translate(0.0F, -1.501F, 0.0F);             // ⑤ 抬高
```

`setupRotations` 中（非睡眠/非死亡/非倒立时）只有一句：

```java
poseStack.mulPose(Axis.YP.rotationDegrees(180.0F - bodyRot));
```

上面 5 步之后，才进入模型根节点（root 的 `PartPose` 恒为 `PartPose.ZERO`，见 `MeshDefinition` 构造函数）。

设 `b = bodyRot`（度，Minecraft 约定：0 = 朝南 +Z，90 = 朝西 -X，180 = 朝北 -Z，270 = 朝东 +X），
设 `s = state.scale`（默认 1.0F），模型空间点 `(mx, my, mz)`（像素），
相对**实体脚部**（实体 `getY()` 所在高度，即包围盒底面）的世界偏移为：

```
wx =  s * ( mx * cos(b) + mz * sin(b) ) / 16
wy =  s * ( 1.501 - my / 16 )
wz =  s * ( mx * sin(b) - mz * cos(b) ) / 16
```

**（单位：方块）**

推论 / 校验：

* **模型 y 轴朝下**：`my` 越大世界高度越低。`my = 24` → `wy = 1.501 - 1.5 = 0.001 ≈ 0`，即 **y = 24 是脚底**；
  `my = 0` → `wy = 1.501`，即模型原点位于脚底上方 24 px（1.5 方块）。
* 类人模型（玩家/僵尸/骷髅/苦力怕/四足/鸡）腿部最低点都正好是 `my = 24`，故脚底贴地。
  末影人为例外：腿 `offset y = -5`、cube 高 30 → 最低点 `my = 25` → `wy = -0.0615`（低于地面 1 px，为原版行为）。
* **模型 −Z 是实体的正面（朝向）**：`(mx,mz) = (0,-1)` → `(wx,wz) = (-sin b, cos b)`，正是实体 forward 向量。
* **模型 +X 在 b=0 时等于世界 +X**：`(1,0)` → `(1/16, 0)`。
  因此模型 **−X 是实体自己的右手侧**（`right_arm` 的 offset 为 `x = -5`，面向南时位于世界 −X = 西，正确）。
* `b = 0`（朝南）时的最简形式：`wx = mx/16, wy = 1.501 - my/16, wz = -mz/16`。

> 注：`state.scale` 来自 `entity.getScale()`；`LivingEntityRenderer#submit` 之前 `EntityRenderDispatcher`
> 已把 poseStack 平移到实体位置 `(x, y, z)`（`y` 即脚部），再叠加 `getRenderOffset`。

### 1.3 part 层级变换与旋转顺序

`ModelPart.translateAndRotate`：

```java
poseStack.translate(this.x / 16.0F, this.y / 16.0F, this.z / 16.0F);
poseStack.mulPose(new Quaternionf().rotationZYX(this.zRot, this.yRot, this.xRot));
if (xScale/yScale/zScale != 1) poseStack.scale(...);
```

即：

```
M_part = T(offset/16) · Rz(zRot) · Ry(yRot) · Rx(xRot) · S(scale)
```

点变换顺序为 **先缩放 → 先绕 X → 再绕 Y → 再绕 Z → 最后平移**（`Rz·Ry·Rx·p`）。

依据：`net/minecraft/client/resources/model/cuboid/CuboidRotation.java` 中
`EulerXYZRotation(x, y, z)` 的 `transformation()` 调用的就是 `rotationZYX(z, y, x)`，
说明 JOML 的 `rotationZYX(z,y,x)` = `Rz(z)·Ry(y)·Rx(x)`（XYZ 先后应用顺序）。

本表中绝大多数 part 只有一个非零旋转分量，此时顺序无关；**唯一例外是蜘蛛的 8 条腿**（同时有 `yRot` 和 `zRot`），
必须按 `Rz(zRot)·Ry(yRot)` 组合。

所有 `PartPose` 里的角度单位是**弧度**（源码写 `Math.PI / 2` 等），本表同时给出度数。

### 1.4 cube 的 UV 推导（六个面的精确像素矩形）

`net/minecraft/client/model/geom/ModelPart.java` 的 `Cube` 构造函数（第 239~319 行）：

```java
float u0 = xTexOffs;                        // = U
float u1 = xTexOffs + depth;                // U + d
float u2 = xTexOffs + depth + width;        // U + d + w
float u22 = xTexOffs + depth + width + width;
float u3 = xTexOffs + depth + width + depth;
float u4 = xTexOffs + depth + width + depth + width;
float v0 = yTexOffs;                        // = V
float v1 = yTexOffs + depth;                // V + d
float v2 = yTexOffs + depth + height;       // V + d + h
```

（代码里的 `u22` 其实就是 `u2 + width`，即 `U + d + 2w`；`u4 = U + 2d + 2w`。）

六个面在贴图上的**像素矩形**（左上角为 `(u_min, v_min)`，v 向下增长，取值左闭右开）：

| 面（Direction） | 对应 cube 的哪个面 | u 范围 | v 范围 |
|---|---|---|---|
| `DOWN`  | minY 面（**世界顶部**）| `[U+d, U+d+w]` | `[V, V+d]` |
| `UP`    | maxY 面（**世界底部**）| `[U+d+w, U+d+2w]` | `[V, V+d]` |
| `WEST`  | minX 面（实体**右**侧）| `[U, U+d]` | `[V+d, V+d+h]` |
| `NORTH` | minZ 面（实体**正**面）| `[U+d, U+d+w]` | `[V+d, V+d+h]` |
| `EAST`  | maxX 面（实体**左**侧）| `[U+d+w, U+d+w+d]` | `[V+d, V+d+h]` |
| `SOUTH` | maxZ 面（实体**背**面）| `[U+d+w+d, U+2d+2w]` | `[V+d, V+d+h]` |

> 注意 `Direction.DOWN` 的步进向量是 `(0,-1,0)`（`Direction.java` 第 33 行），而模型 +Y 朝世界下方，
> 所以 **`DOWN` 面 = 立方体的顶面**，`UP` 面 = 立方体的底面。

各面内部的方向映射（用于确定贴图不翻转、以及法线/缠绕）：

| 面 | u 较小 ↔ | u 较大 ↔ | v 较小 ↔ | v 较大 ↔ |
|---|---|---|---|---|
| DOWN  | minX | maxX | maxZ | minZ |
| UP    | minX | maxX | maxZ | minZ |
| WEST  | maxZ | minZ | minY | maxY |
| NORTH | minX | maxX | minY | maxY |
| EAST  | minZ | maxZ | minY | maxY |
| SOUTH | maxX | minX | minY | maxY |

归一化：`ModelPart.Polygon` 用 `u / xTexSize`、`v / yTexSize`，其中 `xTexSize, yTexSize` 就是
`LayerDefinition.create(mesh, W, H)` 的 `W, H`（由 `CubeDefinition.bake(texScaleX, texScaleY)` 传入，默认 texScale = 1.0）。
`CubeListBuilder.addBox` 默认 `texScale = 1.0F`，本表所有模型都没有改过 texScale。

**示例 1**（玩家头，`texOffs(0,0)`，`8×8×8`，贴图 64×64）：
DOWN/TOP `u∈[8,16] v∈[0,8]`；BOTTOM `u∈[16,24] v∈[0,8]`；
WEST `u∈[0,8] v∈[8,16]`；NORTH `u∈[8,16] v∈[8,16]`；EAST `u∈[16,24] v∈[8,16]`；SOUTH `u∈[24,32] v∈[8,16]`。

**示例 2**（猪身体，`texOffs(28,8)`，`10×16×8`，贴图 64×64）：
DOWN `u∈[36,46] v∈[8,16]`；UP `u∈[46,56] v∈[8,16]`；
WEST `u∈[28,36] v∈[16,32]`；NORTH `u∈[36,46] v∈[16,32]`；EAST `u∈[46,54] v∈[16,32]`；SOUTH `u∈[54,64] v∈[16,32]`。

### 1.5 `mirror`（CubeListBuilder.mirror()）

`Cube` 构造函数中 `mirror=true` 只做两件事：交换 `minX/maxX`（几何包围盒不变），
并在 `Polygon` 构造函数里把顶点数组整体反转。净效果：
**几何包围盒完全不变，每个面在各自的 UV 矩形内沿 u 方向水平翻转**（贴图镜像）。
六个面的 UV 矩形边界本身与 mirror 无关。

典型用法：左臂/左腿复用与右臂/右腿相同的 `texOffs`，靠 `mirror()` 得到镜像贴图。

### 1.6 `CubeDeformation`

`CubeDeformation(grow)` = 三轴各外扩 `grow` 像素（可为负）。`Cube` 构造里：

```java
minX -= growX; minY -= growY; minZ -= growZ;
maxX += growX; maxY += growY; maxZ += growZ;
```

`CubeDeformation.NONE = new CubeDeformation(0.0F)`；`extend(f)` 是三轴各加 `f`。
`addBox(...)` 不传 deformation 时默认 `CubeDeformation.NONE`。

本表涉及的非零 deformation：

| 位置 | 值 | 说明 |
|---|---|---|
| 玩家 / 僵尸 / 骷髅 的 `hat` | `0.5`（= `NONE.extend(0.5F)`）| 头部外壳 |
| 玩家 `jacket` / `left_sleeve` / `right_sleeve` / `left_pants` / `right_pants` | `0.25`（= `NONE.extend(0.25F)`）| 外套层 |
| 猪鞍层 `ModelLayers.PIG_SADDLE` | `0.5` | 整只猪按 0.5 外扩后画鞍 |
| 苦力怕盔甲层 `ModelLayers.CREEPER_ARMOR` | `2.0` | 充能光壳 |
| 羊羊毛层 head / body / legs | `0.6` / `1.75` / `0.5` | `SheepFurModel` |
| 末影人 `hat` | `-0.5` | 缩小 0.5 px |

---

## 2. 模型清单：贴图路径与贴图尺寸

| 模型 | 层（`ModelLayers`） | 贴图（相对 `assets/minecraft/textures/`）| 尺寸 W×H（源码 / png 实测）|
|---|---|---|---|
| 玩家（wide） | `PLAYER` | `entity/player/wide/steve.png`（默认；wide 目录下还有 alex/ari/efe/kai/makena/noor/sunny/zuri）| 64×64（源码 & png 均为 64×64）|
| 玩家（slim） | `PLAYER_SLIM` | `entity/player/slim/*.png` | 64×64 |
| 猪 | `PIG` | `entity/pig/pig_temperate.png`（温）；`pig_warm.png` / `pig_cold.png`（冷用 `ColdPigModel`）| 64×64（png 实测 64×64）|
| 牛 | `COW` | `entity/cow/cow_temperate.png`（温/暖共用 `CowModel`，冷用 `ColdCowModel`）| 64×64（png 实测 64×64）|
| 羊 | `SHEEP` | `entity/sheep/sheep.png` | 64×32（png 实测 64×32）|
| 羊（羊毛层）| `SHEEP_WOOL` | `entity/sheep/sheep_wool.png` | 64×32（png 实测 64×32）|
| 鸡 | `CHICKEN` | `entity/chicken/chicken_temperate.png` | 64×32（png 实测 64×32）|
| 僵尸 | `ZOMBIE` | `entity/zombie/zombie.png` | 64×64（png 实测 64×64）|
| 骷髅 | `SKELETON` | `entity/skeleton/skeleton.png` | 64×32（png 实测 64×32）|
| 苦力怕 | `CREEPER` | `entity/creeper/creeper.png` | 64×32（png 实测 64×32）|
| 蜘蛛 | `SPIDER` | `entity/spider/spider.png` | 64×32（png 实测 64×32）|
| 烈焰人 | `BLAZE` | `entity/blaze/blaze.png` | 64×32（png 实测 64×32）|
| 末影人 | `ENDERMAN` | `entity/enderman/enderman.png` | 64×32（png 实测 64×32）|

源码侧尺寸（`LayerDefinition.create(mesh, W, H)`）出处：
`net/minecraft/client/model/geom/LayerDefinitions.java`
（`PLAYER` 第 415 行、`PIG` 第 404 行、`ZOMBIE` 第 509 行、`SPIDER` 第 456 行、`BLAZE` 第 273 行、`ENDERMAN` 第 336 行、`CREEPER` 第 314 行、`SHEEP` 第 434 行）；
其余在各模型的 `createBodyLayer()` 内（已逐字核对，见下）。

特殊缩放（渲染器 `scale()` 覆写）：只有 `CreeperRenderer#scale` 有（`swelling` 膨胀效果，非模型固有，C++ 可忽略）。
其余渲染器（Pig/Cow/Sheep/Chicken/Zombie/Skeleton/Spider/Blaze/Enderman）都不覆写 `scale()`。

---

## 3. 各模型详细数据

表格列说明：

* **offset** = `PartPose.offset(x, y, z)`，像素，相对父 part（未旋转前）。
* **rotation** = `PartPose` 的 `(xRot, yRot, zRot)`，**度**（括号里是源码里的弧度写法）。
* **cube** = `addBox(x0, y0, z0, w, h, d)`，像素，相对本 part 的原点。
* **texOffs** = `.texOffs(u, v)`。
* **def** = `CubeDeformation`（省略即 `NONE` = 0）。
* **mirror** 列只在 `true` 时标出。
* 所有模型根节点 `root` 的 pose 均为 `PartPose.ZERO`（0,0,0 / 0,0,0）。

### 3.1 玩家（PlayerModel，wide = 手臂 4 px）

源码：`net/minecraft/client/model/player/PlayerModel.java#createMesh` +
`net/minecraft/client/model/HumanoidModel.java#createMesh`
注册：`LayerDefinition.create(PlayerModel.createMesh(CubeDeformation.NONE, false), 64, 64)` → 贴图 64×64。

主体层（**C++ 只需要这一层即可；下方 3.1.2 的外套层可选**）：

| part | 父 | offset (x,y,z) | rotation (度) | cube (x0,y0,z0,w,h,d) | texOffs (u,v) | 备注 |
|---|---|---|---|---|---|---|
| `head` | root | (0, 0, 0) | (0,0,0) | (-4, -8, -4, 8, 8, 8) | (0, 0) | |
| `hat` | head | (0, 0, 0) | (0,0,0) | (-4, -8, -4, 8, 8, 8) | (32, 0) | def = 0.5 |
| `body` | root | (0, 0, 0) | (0,0,0) | (-4, 0, -2, 8, 12, 4) | (16, 16) | |
| `right_arm` | root | (-5, 2, 0) | (0,0,0) | (-3, -2, -2, 4, 12, 4) | (40, 16) | 宽手臂 4 px |
| `left_arm` | root | (5, 2, 0) | (0,0,0) | (-1, -2, -2, 4, 12, 4) | (32, 48) | 宽手臂 4 px；**未** mirror |
| `right_leg` | root | (-1.9, 12, 0) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 16) | |
| `left_leg` | root | (1.9, 12, 0) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (16, 48) | **未** mirror |

`head` 的局部包围盒 `y ∈ [-8, 0]`，`body` `y ∈ [0, 12]`，`*_leg` `y ∈ [12, 24]`，
`*_arm` `y ∈ [-2, 10]`（`offset y=2` → 世界 `y ∈ [0, 12]`）。头顶 `my = -8` → `wy = 1.501 + 0.5 = 2.001`（合 2 格高，正确）。

#### 3.1.2 玩家外套层（overlay，可忽略）

| part | 父 | offset | cube (x0,y0,z0,w,h,d) | texOffs | def |
|---|---|---|---|---|---|
| `jacket` | body | (0,0,0) | (-4, 0, -2, 8, 12, 4) | (16, 32) | 0.25 |
| `right_sleeve` | right_arm | (0,0,0) | (-3, -2, -2, 4, 12, 4) | (40, 32) | 0.25 |
| `left_sleeve` | left_arm | (0,0,0) | (-1, -2, -2, 4, 12, 4) | (48, 48) | 0.25 |
| `right_pants` | right_leg | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 32) | 0.25 |
| `left_pants` | left_leg | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 48) | 0.25 |

#### 3.1.3 玩家 slim（Alex）差异（仅手臂/袖子不同，其余同 wide）

| part | offset | cube | texOffs | def |
|---|---|---|---|---|
| `right_arm` | (-5, 2, 0) | (-2, -2, -2, **3**, 12, 4) | (40, 16) | 0 |
| `left_arm` | (5, 2, 0) | (-1, -2, -2, **3**, 12, 4) | (32, 48) | 0 |
| `right_sleeve` | (0,0,0) | (-2, -2, -2, **3**, 12, 4) | (40, 32) | 0.25 |
| `left_sleeve` | (0,0,0) | (-1, -2, -2, **3**, 12, 4) | (48, 48) | 0.25 |

（`head`/`hat`/`body`/`*_leg`/`*_pants`/`jacket` 与 wide 完全一致。）

---

### 3.2 猪（PigModel）

源码：`net/minecraft/client/model/animal/pig/PigModel.java`
网格 = `QuadrupedModel.createBodyMesh(6, /*mirrorLeftLeg*/true, /*mirrorRightLeg*/false, CubeDeformation.NONE)`，
随后 `head` 被 `addOrReplaceChild` 覆盖（加了猪鼻）。
`LayerDefinition.create(createBasePigModel(g), 64, 64)` → 贴图 64×64。

| part | 父 | offset (x,y,z) | rotation (度) | cube (x0,y0,z0,w,h,d) | texOffs (u,v) | mirror | 备注 |
|---|---|---|---|---|---|---|---|
| `head` | root | (0, **12**, -6) | (0,0,0) | (-4, -4, -8, 8, 8, 8) | (0, 0) | false | 头 |
| `head` | root | — | — | (-2, 0, -9, 4, 3, 1) | (16, 16) | false | 猪鼻（同一 part 的第 2 个 cube）|
| `body` | root | (0, 11, 2) | (**90**, 0, 0)（源码 `(float)(Math.PI/2), 0, 0`）| (-5, -10, -7, 10, 16, 8) | (28, 8) | false | 四足通用身体网格，`legSize=6` |
| `right_hind_leg` | root | (-3, 18, 7) | (0,0,0) | (-2, 0, -2, 4, **6**, 4) | (0, 16) | false | |
| `left_hind_leg` | root | (3, 18, 7) | (0,0,0) | (-2, 0, -2, 4, **6**, 4) | (0, 16) | **true** | |
| `right_front_leg` | root | (-3, 18, -5) | (0,0,0) | (-2, 0, -2, 4, **6**, 4) | (0, 16) | false | 与 `right_hind_leg` 共用同一 builder |
| `left_front_leg` | root | (3, 18, -5) | (0,0,0) | (-2, 0, -2, 4, **6**, 4) | (0, 16) | **true** | 与 `left_hind_leg` 共用同一 builder |

说明：

* `QuadrupedModel.createBodyMesh(legSize=6, ...)` 里 `head` 原本是 `offset(0, 18-6=12, -6)`，
  `body` 是 `offsetAndRotation(0, 17-6=11, 2, PI/2, 0, 0)`，腿 `offset(±3, 24-6=18, ±)`。猪覆盖 `head` 时 offset 与原来相同。
* 腿高 6 px，`18 + 6 = 24` → 脚底贴地。
* **鞍层** `ModelLayers.PIG_SADDLE = PigModel.createBodyLayer(new CubeDeformation(0.5F))` —— 同一套网格，
  所有 cube 三轴外扩 0.5 px。
* **冷猪** `ColdPigModel` 额外在 `body` 上叠一个 `texOffs(28, 32)`、`def = 0.5` 的同尺寸 cube（保暖层）。
* 温/暖猪共用 `ModelLayers.PIG`（几何完全相同，只是贴图不同）。

---

### 3.3 牛（CowModel）

源码：`net/minecraft/client/model/animal/cow/CowModel.java#createBaseCowModel`
（**不使用** `QuadrupedModel.createBodyMesh`，是独立网格；所有 `addBox` 未传 deformation → 全为 `NONE`）
`LayerDefinition.create(mesh, 64, 64)` → 贴图 64×64。

| part | 父 | offset (x,y,z) | rotation (度) | cube (x0,y0,z0,w,h,d) | texOffs (u,v) | mirror | 备注 |
|---|---|---|---|---|---|---|---|
| `head` | root | (0, 4, -8) | (0,0,0) | (-4, -4, -6, 8, 8, 6) | (0, 0) | false | 头 |
| `head` | root | — | — | (-3, 1, -7, 6, 3, 1) | (1, 33) | false | 口鼻部 |
| `head` | root | — | — | (-5, -5, -5, 1, 3, 1) | (22, 0) | false | cube id `right_horn` |
| `head` | root | — | — | (4, -5, -5, 1, 3, 1) | (22, 0) | false | cube id `left_horn` |
| `body` | root | (0, 5, 2) | (**90**, 0, 0)（`Math.PI/2`）| (-6, -10, -7, 12, 18, 10) | (18, 4) | false | |
| `body` | root | — | — | (-2, 2, -8, 4, 6, 1) | (52, 0) | false | 乳房 |
| `right_hind_leg` | root | (-4, 12, 7) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 16) | false | |
| `left_hind_leg` | root | (4, 12, 7) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 16) | **true** | |
| `right_front_leg` | root | (-4, 12, -5) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 16) | false | 与 `right_hind_leg` 同 builder |
| `left_front_leg` | root | (4, 12, -5) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 16) | **true** | 与 `left_hind_leg` 同 builder |

腿高 12，`12 + 12 = 24` → 脚底贴地。
冷牛 `ColdCowModel` 会把 `body` 换成 `texOffs(20,32) def=0.5` + `texOffs(18,4)` 两个 cube、`head` 去掉角、
并把角改成 `head` 的子 part `right_horn`/`left_horn`（`PartPose.offsetAndRotation(∓4.5, -2.5, -3.5, 1.5708, 0, 0)`）；
暖牛 `WarmCowModel` 的 `head` 带 4 个耳朵 cube。温/暖/冷之外的普通牛用上表。

---

### 3.4 羊（SheepModel）

源码：`net/minecraft/client/model/animal/sheep/SheepModel.java`
网格 = `QuadrupedModel.createBodyMesh(12, /*mirrorLeftLeg*/false, /*mirrorRightLeg*/true, CubeDeformation.NONE)`，
随后 `head`、`body` 被覆盖。`LayerDefinition.create(mesh, 64, 32)` → 贴图 64×32。

| part | 父 | offset (x,y,z) | rotation (度) | cube (x0,y0,z0,w,h,d) | texOffs (u,v) | mirror | 备注 |
|---|---|---|---|---|---|---|---|
| `head` | root | (0, 6, -8) | (0,0,0) | (-3, -4, -6, 6, 6, **8**) | (0, 0) | false | 注意 d = 8（不是 6） |
| `body` | root | (0, 5, 2) | (**90**, 0, 0)（`Math.PI/2`）| (-4, -10, -7, 8, 16, 6) | (28, 8) | false | |
| `right_hind_leg` | root | (-3, 12, 7) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 16) | **true** | |
| `left_hind_leg` | root | (3, 12, 7) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 16) | false | |
| `right_front_leg` | root | (-3, 12, -5) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 16) | **true** | |
| `left_front_leg` | root | (3, 12, -5) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 16) | false | |

`head` 的源码原文（避免上面表格歧义）：

```java
root.addOrReplaceChild("head",
    CubeListBuilder.create().texOffs(0, 0).addBox(-3.0F, -4.0F, -6.0F, 6.0F, 6.0F, 8.0F),
    PartPose.offset(0.0F, 6.0F, -8.0F));
```

即 `cube = (-3, -4, -6, w=6, h=6, d=8)`，`texOffs = (0, 0)`。

腿高 12，`12 + 12 = 24` → 脚底贴地。
注意羊的 mirror 参数与猪相反（`mirrorLeftLeg=false, mirrorRightLeg=true` → 右腿 mirror、左腿不 mirror）。

#### 3.4.2 羊毛层（SheepFurModel，`ModelLayers.SHEEP_WOOL`，贴图 `entity/sheep/sheep_wool.png` 64×32）

源码：`net/minecraft/client/model/animal/sheep/SheepFurModel.java#createFurLayer`

| part | 父 | offset (x,y,z) | rotation (度) | cube (x0,y0,z0,w,h,d) | texOffs (u,v) | def | 备注 |
|---|---|---|---|---|---|---|---|
| `head` | root | (0, 6, -8) | (0,0,0) | (-3, -4, -4, 6, 6, 6) | (0, 0) | **0.6** | 注意 d 是 6 不是 8 |
| `body` | root | (0, 5, 2) | (**90**, 0, 0)（`Math.PI/2`）| (-4, -10, -7, 8, 16, 6) | (28, 8) | **1.75** | |
| `right_hind_leg` | root | (-3, 12, 7) | (0,0,0) | (-2, 0, -2, 4, **6**, 4) | (0, 16) | **0.5** | 四条腿共用同一 builder，**均未 mirror** |
| `left_hind_leg` | root | (3, 12, 7) | (0,0,0) | (-2, 0, -2, 4, **6**, 4) | (0, 16) | **0.5** | |
| `right_front_leg` | root | (-3, 12, -5) | (0,0,0) | (-2, 0, -2, 4, **6**, 4) | (0, 16) | **0.5** | |
| `left_front_leg` | root | (3, 12, -5) | (0,0,0) | (-2, 0, -2, 4, **6**, 4) | (0, 16) | **0.5** | |

（`ModelLayers.SHEEP_WOOL_UNDERCOAT` 直接复用 `sheepBodyLayer`，即上表 3.4 的本体网格。）

---

### 3.5 鸡（AdultChickenModel）

源码：`net/minecraft/client/model/animal/chicken/AdultChickenModel.java#createBaseChickenModel`
`LayerDefinition.create(mesh, 64, 32)` → 贴图 64×32。所有 deformation 均为 `NONE`。

| part | 父 | offset (x,y,z) | rotation (度) | cube (x0,y0,z0,w,h,d) | texOffs (u,v) | mirror | 备注 |
|---|---|---|---|---|---|---|---|
| `head` | root | (0, 15, -4) | (0,0,0) | (-2, -6, -2, 4, 6, 3) | (0, 0) | false | |
| `beak` | head | (0, 0, 0) | (0,0,0) | (-2, -4, -4, 4, 2, 2) | (14, 0) | false | `PartPose.ZERO` |
| `red_thing` | head | (0, 0, 0) | (0,0,0) | (-1, -2, -3, 2, 2, 2) | (14, 4) | false | 鸡冠，`PartPose.ZERO` |
| `body` | root | (0, 16, 0) | (**90**, 0, 0)（`Math.PI/2`）| (-3, -4, -3, 6, 8, 6) | (0, 9) | false | |
| `right_leg` | root | (-2, 19, 1) | (0,0,0) | (-1, 0, -3, 3, 5, 3) | (26, 0) | false | 两腿共用同一 builder |
| `left_leg` | root | (1, 19, 1) | (0,0,0) | (-1, 0, -3, 3, 5, 3) | (26, 0) | false | |
| `right_wing` | root | (-4, 13, 0) | (0,0,0) | (0, 0, -3, 1, 4, 6) | (24, 13) | false | |
| `left_wing` | root | (4, 13, 0) | (0,0,0) | (-1, 0, -3, 1, 4, 6) | (24, 13) | false | |

腿高 5，`19 + 5 = 24` → 脚底贴地。
冷鸡 `ColdChickenModel` 在 `body` 上多一个 `texOffs(38,9) addBox(0, 3, -1, 0, 3, 5)`、
在 `head` 上多一个 `texOffs(44,0) addBox(-3, -7, -2.015, 6, 3, 4)`。

---

### 3.6 僵尸（ZombieModel）

源码：`net/minecraft/client/model/monster/zombie/ZombieModel.java` → `AbstractZombieModel` → `HumanoidModel`。
**没有自己的网格**：`ModelLayers.ZOMBIE = humanoidBodyLayer = LayerDefinition.create(HumanoidModel.createMesh(CubeDeformation.NONE, 0.0F), 64, 64)`
（`LayerDefinitions.java` 第 171 行 + 第 509 行）→ 贴图 64×64。
几何与"标准类人（wide 手臂）"完全相同，只是 `setupAnim` 里加了 `AnimationUtils.animateZombieArms`（手臂前伸，动画层面）。

| part | 父 | offset (x,y,z) | rotation (度) | cube (x0,y0,z0,w,h,d) | texOffs (u,v) | mirror | 备注 |
|---|---|---|---|---|---|---|---|
| `head` | root | (0, 0, 0) | (0,0,0) | (-4, -8, -4, 8, 8, 8) | (0, 0) | false | |
| `hat` | head | (0, 0, 0) | (0,0,0) | (-4, -8, -4, 8, 8, 8) | (32, 0) | false | def = **0.5** |
| `body` | root | (0, 0, 0) | (0,0,0) | (-4, 0, -2, 8, 12, 4) | (16, 16) | false | |
| `right_arm` | root | (-5, 2, 0) | (0,0,0) | (-3, -2, -2, 4, 12, 4) | (40, 16) | false | |
| `left_arm` | root | (5, 2, 0) | (0,0,0) | (-1, -2, -2, 4, 12, 4) | (40, 16) | **true** | |
| `right_leg` | root | (-1.9, 12, 0) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 16) | false | |
| `left_leg` | root | (1.9, 12, 0) | (0,0,0) | (-2, 0, -2, 4, 12, 4) | (0, 16) | **true** | |

---

### 3.7 骷髅（SkeletonModel）

源码：`net/minecraft/client/model/monster/skeleton/SkeletonModel.java#createBodyLayer`
= `HumanoidModel.createMesh(CubeDeformation.NONE, 0.0F)` + `createDefaultSkeletonMesh(root)`（把四肢换成 2×2 细杆）
`LayerDefinition.create(mesh, 64, 32)` → 贴图 64×32。

| part | 父 | offset (x,y,z) | rotation (度) | cube (x0,y0,z0,w,h,d) | texOffs (u,v) | mirror | 备注 |
|---|---|---|---|---|---|---|---|
| `head` | root | (0, 0, 0) | (0,0,0) | (-4, -8, -4, 8, 8, 8) | (0, 0) | false | |
| `hat` | head | (0, 0, 0) | (0,0,0) | (-4, -8, -4, 8, 8, 8) | (32, 0) | false | def = **0.5** |
| `body` | root | (0, 0, 0) | (0,0,0) | (-4, 0, -2, 8, 12, 4) | (16, 16) | false | |
| `right_arm` | root | (-5, 2, 0) | (0,0,0) | (-1, -2, -1, **2**, 12, **2**) | (40, 16) | false | 细手臂 |
| `left_arm` | root | (5, 2, 0) | (0,0,0) | (-1, -2, -1, **2**, 12, **2**) | (40, 16) | **true** | 细手臂 |
| `right_leg` | root | (-2, 12, 0) | (0,0,0) | (-1, 0, -1, **2**, 12, **2**) | (0, 16) | false | 细腿；注意 x = ±2（不是 ±1.9）|
| `left_leg` | root | (2, 12, 0) | (0,0,0) | (-1, 0, -1, **2**, 12, **2**) | (0, 16) | **true** | |

（另有 `SkeletonModel.createSingleModelDualBodyLayer()`，给"single model dual"用，64×64，
手臂/腿部各叠一层 3 px 宽外壳；本表不展开，**未确认**其用途实体。）

---

### 3.8 苦力怕（CreeperModel）

源码：`net/minecraft/client/model/monster/creeper/CreeperModel.java#createBodyLayer`
`ModelLayers.CREEPER = CreeperModel.createBodyLayer(CubeDeformation.NONE)`，`LayerDefinition.create(mesh, 64, 32)` → 64×32。

| part | 父 | offset (x,y,z) | rotation (度) | cube (x0,y0,z0,w,h,d) | texOffs (u,v) | mirror | 备注 |
|---|---|---|---|---|---|---|---|
| `head` | root | (0, 6, 0) | (0,0,0) | (-4, -8, -4, 8, 8, 8) | (0, 0) | false | |
| `body` | root | (0, 6, 0) | (0,0,0) | (-4, 0, -2, 8, 12, 4) | (16, 16) | false | |
| `right_hind_leg` | root | (-2, 18, 4) | (0,0,0) | (-2, 0, -2, 4, 6, 4) | (0, 16) | false | 四条腿共用同一 builder |
| `left_hind_leg` | root | (2, 18, 4) | (0,0,0) | (-2, 0, -2, 4, 6, 4) | (0, 16) | false | |
| `right_front_leg` | root | (-2, 18, -4) | (0,0,0) | (-2, 0, -2, 4, 6, 4) | (0, 16) | false | |
| `left_front_leg` | root | (2, 18, -4) | (0,0,0) | (-2, 0, -2, 4, 6, 4) | (0, 16) | false | |

腿高 6，`18 + 6 = 24` → 脚底贴地。
注意：代码里 `this.leftHindLeg = root.getChild("right_hind_leg");`（字段名与 part 名交叉），
即 **Java 侧变量名与 part 名是反的**，C++ 侧请按 part 名（`right_hind_leg` 在 x = −2）来对应。
`ModelLayers.CREEPER_ARMOR = CreeperModel.createBodyLayer(new CubeDeformation(2.0F))`（同一网格外扩 2 px）。

---

### 3.9 蜘蛛（SpiderModel）

源码：`net/minecraft/client/model/monster/spider/SpiderModel.java#createSpiderBodyLayer`
`LayerDefinition.create(mesh, 64, 32)` → 64×32。所有 deformation = `NONE`。

| part | 父 | offset (x,y,z) | rotation (度)（源码弧度）| cube (x0,y0,z0,w,h,d) | texOffs (u,v) | mirror |
|---|---|---|---|---|---|---|
| `head` | root | (0, 15, -3) | (0,0,0) | (-4, -4, -8, 8, 8, 8) | (32, 4) | false |
| `body0` | root | (0, 15, 0) | (0,0,0) | (-3, -3, -3, 6, 6, 6) | (0, 0) | false |
| `body1` | root | (0, 15, 9) | (0,0,0) | (-5, -4, -6, 10, 8, 12) | (0, 12) | false |
| `right_hind_leg` | root | (-4, 15, 2) | (0, **45**, **-45**)（`0, Math.PI/4, -Math.PI/4`）| (-15, -1, -1, 16, 2, 2) | (18, 0) | false |
| `left_hind_leg` | root | (4, 15, 2) | (0, **-45**, **45**)（`0, -Math.PI/4, Math.PI/4`）| (-1, -1, -1, 16, 2, 2) | (18, 0) | **true** |
| `right_middle_hind_leg` | root | (-4, 15, 1) | (0, **22.5**, **-33.3**)（`0, Math.PI/8, -0.58119464`）| (-15, -1, -1, 16, 2, 2) | (18, 0) | false |
| `left_middle_hind_leg` | root | (4, 15, 1) | (0, **-22.5**, **33.3**)（`0, -Math.PI/8, 0.58119464`）| (-1, -1, -1, 16, 2, 2) | (18, 0) | **true** |
| `right_middle_front_leg` | root | (-4, 15, 0) | (0, **-22.5**, **-33.3**)（`0, -Math.PI/8, -0.58119464`）| (-15, -1, -1, 16, 2, 2) | (18, 0) | false |
| `left_middle_front_leg` | root | (4, 15, 0) | (0, **22.5**, **33.3**)（`0, Math.PI/8, 0.58119464`）| (-1, -1, -1, 16, 2, 2) | (18, 0) | **true** |
| `right_front_leg` | root | (-4, 15, -1) | (0, **-45**, **-45**)（`0, -Math.PI/4, -Math.PI/4`）| (-15, -1, -1, 16, 2, 2) | (18, 0) | false |
| `left_front_leg` | root | (4, 15, -1) | (0, **45**, **45**)（`0, Math.PI/4, Math.PI/4`）| (-1, -1, -1, 16, 2, 2) | (18, 0) | **true** |

要点：

* 腿的 cube 沿 **−X** 方向伸展 16 px（右腿 cube `x ∈ [-15, 1]`，左腿 cube `x ∈ [-1, 15]`），
  即"从躯干向外伸出"，靠 `rotation` 摆位。
* 两条腿同时有 `yRot` 与 `zRot`，组合顺序必须是 `Rz(zRot) · Ry(yRot)`（见 1.3）。
* 源码里有 `float legZRot = (float)(Math.PI/4); float legYRotSpan = (float)(Math.PI/8);` 这两个局部变量**未被使用**，可忽略。
* `0.58119464` rad = 33.3°。
* 蜘蛛没有"脚底 = 24"这一说：躯干在 `y = 15`，腿末端因旋转会伸到 `y ≈ 15 + 16·sin(45°)/... `（实际由动画再叠加），
  本体网格的静态最低点需按 1.3 的矩阵实际计算（**未确认**具体落地高度）。

---

### 3.10 烈焰人（BlazeModel）

源码：`net/minecraft/client/model/monster/blaze/BlazeModel.java#createBodyLayer`
`LayerDefinition.create(mesh, 64, 32)` → 64×32。deformation 全部 `NONE`。

| part | 父 | offset (x,y,z) | rotation | cube (x0,y0,z0,w,h,d) | texOffs (u,v) | mirror |
|---|---|---|---|---|---|---|
| `head` | root | (0, 0, 0) | (0,0,0)（`PartPose.ZERO`）| (-4, -4, -4, 8, 8, 8) | (0, 0) | false |
| `part0` | root | (9.0, -1.0, 0.0) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |
| `part1` | root | (0.0, -1.1224087, 9.0) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |
| `part2` | root | (-9.0, -1.4596672, 0.0) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |
| `part3` | root | (0.0, -1.9292085, -9.0) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |
| `part4` | root | (4.9497476, 1.5839192, 4.9497476) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |
| `part5` | root | (-4.9497476, 1.1989107, 4.9497476) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |
| `part6` | root | (-4.9497476, 1.0100093, -4.9497476) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |
| `part7` | root | (4.9497476, 1.0635324, -4.9497476) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |
| `part8` | root | (4.4550762, 11.9601631, 2.2698669) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |
| `part9` | root | (-2.2698669, 11.8930082, 4.4550762) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |
| `part10` | root | (-4.4550762, 11.3467102, -2.2698669) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |
| `part11` | root | (2.2698669, 10.6143074, -4.4550762) | (0,0,0) | (0, 0, 0, 2, 8, 2) | (0, 16) | false |

12 根"棒"的 offset 由循环算出（**不是字面量**），源码：

```java
float angle = 0.0F;
CubeListBuilder rod = CubeListBuilder.create().texOffs(0, 16).addBox(0.0F, 0.0F, 0.0F, 2.0F, 8.0F, 2.0F);
for (int i = 0; i < 4; i++) {                      // part0..part3
   float x = Mth.cos(angle) * 9.0F;
   float y = -2.0F + Mth.cos(i * 2 * 0.25F);
   float z = Mth.sin(angle) * 9.0F;
   root.addOrReplaceChild(getPartName(i), rod, PartPose.offset(x, y, z));
   angle += (float)(Math.PI / 2);
}
angle = (float)(Math.PI / 4);
for (int i = 4; i < 8; i++) {                      // part4..part7，半径 7，基准 y = +2
   float x = Mth.cos(angle) * 7.0F;
   float y = 2.0F + Mth.cos(i * 2 * 0.25F);
   float z = Mth.sin(angle) * 7.0F;
   ...
}
angle = 0.47123894F;                               // = 27°
for (int i = 8; i < 12; i++) {                     // part8..part11，半径 5，基准 y = +11
   float x = Mth.cos(angle) * 5.0F;
   float y = 11.0F + Mth.cos(i * 1.5F * 0.5F);
   float z = Mth.sin(angle) * 5.0F;
   ...
}
```

上表数值是按 `float32` + `Mth.cos/sin`（65536 项查找表，`Mth.java` 第 35~55 行）逐位复算的结果。
其中 `part1.x`、`part2.z`、`part3.x` 理论上是 `±1.1e-15`（≈0），已按 0.0 给出。
`i * 2 * 0.25F` 中 `i` 是 int：`i=0→0.0, 1→0.5, 2→1.0, 3→1.5, 4→2.0, 5→2.5, 6→3.0, 7→3.5`；
`i * 1.5F * 0.5F`：`i=8→6.0, 9→6.75, 10→7.5, 11→8.25`。

注意烈焰人的 `head` 在 `PartPose.ZERO`（即模型原点，世界 y = 1.501），整体是"漂浮"的，没有贴地的腿。

---

### 3.11 末影人（EndermanModel）

源码：`net/minecraft/client/model/monster/enderman/EndermanModel.java#createBodyLayer`
= `HumanoidModel.createMesh(CubeDeformation.NONE, -14.0F)`，随后 **head / body / 四肢全部被覆盖**。
`LayerDefinition.create(mesh, 64, 32)` → 64×32。

| part | 父 | offset (x,y,z) | rotation (度) | cube (x0,y0,z0,w,h,d) | texOffs (u,v) | mirror | 备注 |
|---|---|---|---|---|---|---|---|
| `head` | root | (0, **-13**, 0) | (0,0,0) | (-4, -8, -4, 8, 8, 8) | (0, 0) | false | 覆盖后 y = -13（createMesh 给的 -14 被替换）|
| `hat` | head | (0, 0, 0) | (0,0,0) | (-4, -8, -4, 8, 8, 8) | (0, 16) | false | def = **-0.5**（缩小 0.5 px）|
| `body` | root | (0, **-14**, 0) | (0,0,0) | (-4, 0, -2, 8, 12, 4) | (32, 16) | false | |
| `right_arm` | root | (-5, **-12**, 0) | (0,0,0) | (-1, -2, -1, 2, **30**, 2) | (56, 0) | false | 长手臂 |
| `left_arm` | root | (5, **-12**, 0) | (0,0,0) | (-1, -2, -1, 2, **30**, 2) | (56, 0) | **true** | |
| `right_leg` | root | (-2, **-5**, 0) | (0,0,0) | (-1, 0, -1, 2, **30**, 2) | (56, 0) | false | 注意 x = ±2（不是 ±1.9）|
| `left_leg` | root | (2, **-5**, 0) | (0,0,0) | (-1, 0, -1, 2, **30**, 2) | (56, 0) | **true** | |

几何范围：头顶 `my = -13 - 8 = -21` → `wy = 1.501 + 21/16 = 2.8125`；
脚底 `my = -5 + 30 = 25` → `wy = 1.501 - 25/16 = -0.0615`（比地面低 1 px，原版行为）。
总高 46 px = 2.875 方块（末影人碰撞箱高 2.9）。

---

## 4. C++ 实现检查清单

1. **模型单位**：内部用"像素"，最终乘 `1/16`。
2. **根节点**：所有模型 root 的 pose 都是零。
3. **part 变换**：`T(offset/16) · Rz(zRot) · Ry(yRot) · Rx(xRot)`。角度用弧度（源码值见各表）。
4. **世界变换**：`T(entityPos) · S(scale) · Ry(180° - bodyRot) · S(-1,-1,1) · T(0,-1.501,0)`，
   等价公式见 1.2。若只想取"面向南（bodyRot=0）"的静态姿态，用
   `wx = mx/16, wy = 1.501 - my/16, wz = -mz/16`。
5. **面与 UV**：六面矩形公式见 1.4；`v` 向下增长；`DOWN` = cube 顶面，`NORTH` = 正面（−Z）。
6. **mirror**：只翻转面内 u 方向，不改包围盒、不改 UV 矩形边界。
7. **UV 归一化**：`u_norm = u_px / texW`，`v_norm = v_px / texH`（`texW/texH` 见表 2）。

## 5. 未确认项

* 蜘蛛静态姿态下腿末端实际触地高度（需按 1.3 矩阵实测，未计算）。
* `SkeletonModel.createSingleModelDualBodyLayer()` 对应哪个实体（源码中未追踪到调用点）。
* 猪/牛/鸡的贴图由 `variant`（数据包 registry）决定，`pig_temperate.png` 等文件名来自 `G:/assets`，
  具体 variant→贴图映射未在源码中枚举（在 `data/minecraft/.../pig_variant/*.json` 里）。
