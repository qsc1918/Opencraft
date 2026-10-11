// mobmodel_data.inc — 生物/玩家模型数据（include 进 mobmodel.cpp 的匿名命名空间）
//
// 全部来自 docs/mob_models.md（逐字取自 MC Java 26.2 反编译源码）：
//   part offset/rotation、cube addBox 包围盒、texOffs、mirror、CubeDeformation。
//   单位是"模型像素"，1 像素 = 1/16 方块；模型 y 轴朝下，y=24 是脚底。
// 换算与出面公式见 mobmodel.hpp 与 mob_models.md §1.2/§1.4。
//
// 注意：指定初始化器必须按结构体声明顺序写
//   MobCube : x0,y0,z0,w,h,d,def,u,v,mirror
//   MobPart : ox,oy,oz,rx,ry,rz,parent,anim,cube0,cubeN

// ---------------------------------------------------------------------------
// 玩家（wide = steve）64×64
// ---------------------------------------------------------------------------
static const MobCube kCubesPlayer[] = {
    {.x0=-4, .y0=-8, .z0=-4, .w=8, .h=8,  .d=8, .u=0,  .v=0},
    {.x0=-4, .y0=-8, .z0=-4, .w=8, .h=8,  .d=8, .def=0.5f, .u=32, .v=0},
    {.x0=-4, .y0=0,  .z0=-2, .w=8, .h=12, .d=4, .u=16, .v=16},
    {.x0=-3, .y0=-2, .z0=-2, .w=4, .h=12, .d=4, .u=40, .v=16},
    {.x0=-1, .y0=-2, .z0=-2, .w=4, .h=12, .d=4, .u=32, .v=48},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4, .u=0,  .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4, .u=16, .v=48},
};
static const MobPart kPartsPlayer[] = {
    {.ox=0, .oy=0, .oz=0, .parent=-1, .anim=AN_HEAD, .cube0=0, .cubeN=1},
    {.ox=0, .oy=0, .oz=0, .parent=0, .cube0=1, .cubeN=1},
    {.ox=0, .oy=0, .oz=0, .parent=-1, .cube0=2, .cubeN=1},
    {.ox=-5, .oy=2, .oz=0, .parent=-1, .anim=AN_ARM_R, .cube0=3, .cubeN=1},
    {.ox=5, .oy=2, .oz=0, .parent=-1, .anim=AN_ARM_L, .cube0=4, .cubeN=1},
    {.ox=-1.9f, .oy=12, .oz=0, .parent=-1, .anim=AN_LEG_R, .cube0=5, .cubeN=1},
    {.ox=1.9f, .oy=12, .oz=0, .parent=-1, .anim=AN_LEG_L, .cube0=6, .cubeN=1},
};
static const MobModel kModelPlayer{T_ENTITY_STEVE, 64, 64, false,
                                   kPartsPlayer, (int)(sizeof(kPartsPlayer)/sizeof(MobPart)),
                                   kCubesPlayer, (int)(sizeof(kCubesPlayer)/sizeof(MobCube))};

// ---------------------------------------------------------------------------
// 猪 64×64（QuadrupedModel.createBodyMesh(6,...) + 覆盖后的头）
// ---------------------------------------------------------------------------
static const MobCube kCubesPig[] = {
    {.x0=-4, .y0=-4, .z0=-8, .w=8,  .h=8,  .d=8, .u=0,  .v=0},
    {.x0=-2, .y0=0,  .z0=-9, .w=4,  .h=3,  .d=1, .u=16, .v=16},
    {.x0=-5, .y0=-10,.z0=-7, .w=10, .h=16, .d=8, .u=28, .v=8},
    {.x0=-2, .y0=0,  .z0=-2, .w=4,  .h=6,  .d=4, .u=0,  .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4,  .h=6,  .d=4, .u=0,  .v=16, .mirror=true},
    {.x0=-2, .y0=0,  .z0=-2, .w=4,  .h=6,  .d=4, .u=0,  .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4,  .h=6,  .d=4, .u=0,  .v=16, .mirror=true},
};
static const MobPart kPartsPig[] = {
    {.ox=0, .oy=12, .oz=-6, .parent=-1, .anim=AN_HEAD, .cube0=0, .cubeN=2},
    {.ox=0, .oy=11, .oz=2, .rx=PI/2, .parent=-1, .cube0=2, .cubeN=1},
    {.ox=-3, .oy=18, .oz=7, .parent=-1, .anim=AN_LEG_R2, .cube0=3, .cubeN=1},
    {.ox=3, .oy=18, .oz=7, .parent=-1, .anim=AN_LEG_L2, .cube0=4, .cubeN=1},
    {.ox=-3, .oy=18, .oz=-5, .parent=-1, .anim=AN_LEG_R, .cube0=5, .cubeN=1},
    {.ox=3, .oy=18, .oz=-5, .parent=-1, .anim=AN_LEG_L, .cube0=6, .cubeN=1},
};
static const MobModel kModelPig{T_ENTITY_PIG, 64, 64, true,
                                kPartsPig, (int)(sizeof(kPartsPig)/sizeof(MobPart)),
                                kCubesPig, (int)(sizeof(kCubesPig)/sizeof(MobCube))};

// ---------------------------------------------------------------------------
// 牛 64×64
// ---------------------------------------------------------------------------
static const MobCube kCubesCow[] = {
    {.x0=-4, .y0=-4, .z0=-6, .w=8, .h=8,  .d=6,  .u=0,  .v=0},
    {.x0=-3, .y0=1,  .z0=-7, .w=6, .h=3,  .d=1,  .u=1,  .v=33},
    {.x0=-5, .y0=-5, .z0=-5, .w=1, .h=3,  .d=1,  .u=22, .v=0},
    {.x0=4,  .y0=-5, .z0=-5, .w=1, .h=3,  .d=1,  .u=22, .v=0},
    {.x0=-6, .y0=-10,.z0=-7, .w=12,.h=18, .d=10, .u=18, .v=4},
    {.x0=-2, .y0=2,  .z0=-8, .w=4, .h=6,  .d=1,  .u=52, .v=0},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4,  .u=0,  .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4,  .u=0,  .v=16, .mirror=true},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4,  .u=0,  .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4,  .u=0,  .v=16, .mirror=true},
};
static const MobPart kPartsCow[] = {
    {.ox=0, .oy=4, .oz=-8, .parent=-1, .anim=AN_HEAD, .cube0=0, .cubeN=4},
    {.ox=0, .oy=5, .oz=2, .rx=PI/2, .parent=-1, .cube0=4, .cubeN=2},
    {.ox=-4, .oy=12, .oz=7, .parent=-1, .anim=AN_LEG_R2, .cube0=6, .cubeN=1},
    {.ox=4, .oy=12, .oz=7, .parent=-1, .anim=AN_LEG_L2, .cube0=7, .cubeN=1},
    {.ox=-4, .oy=12, .oz=-5, .parent=-1, .anim=AN_LEG_R, .cube0=8, .cubeN=1},
    {.ox=4, .oy=12, .oz=-5, .parent=-1, .anim=AN_LEG_L, .cube0=9, .cubeN=1},
};
static const MobModel kModelCow{T_ENTITY_COW, 64, 64, true,
                                kPartsCow, (int)(sizeof(kPartsCow)/sizeof(MobPart)),
                                kCubesCow, (int)(sizeof(kCubesCow)/sizeof(MobCube))};

// ---------------------------------------------------------------------------
// 羊 64×32（头的 d=8 不是 6；mirror 在右腿）
// ---------------------------------------------------------------------------
// 羊分两层（与渲染器的两层模型一致）：
//   基础层 = SheepModel.createBodyLayer（sheep.png，剪毛后的身体）
//   羊毛层 = SheepFurModel.createFurLayer（sheep_wool.png，未剪毛时叠加）
//   fur 层：头 6×6×6 def0.6、身体 8×16×6 def1.75、腿 4×6×4 def0.5
static const MobCube kCubesSheep[] = {
    // 基础层
    {.x0=-3, .y0=-4, .z0=-6, .w=6, .h=6,  .d=8, .u=0,  .v=0},
    // 羊毛头：z0=-4、深 6（比基础头短），前面 1.4 格露出来才不会盖住脸
    {.x0=-3, .y0=-4, .z0=-4, .w=6, .h=6,  .d=6, .def=0.6f, .u=0, .v=0, .tile=T_ENTITY_SHEEP_WOOL},
    {.x0=-4, .y0=-10,.z0=-7, .w=8, .h=16, .d=6, .u=28, .v=8},
    {.x0=-4, .y0=-10,.z0=-7, .w=8, .h=16, .d=6, .def=1.75f, .u=28, .v=8, .tile=T_ENTITY_SHEEP_WOOL},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4, .u=0,  .v=16, .mirror=true},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=6,  .d=4, .def=0.5f, .u=0, .v=16, .mirror=true, .tile=T_ENTITY_SHEEP_WOOL},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4, .u=0,  .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=6,  .d=4, .def=0.5f, .u=0, .v=16, .tile=T_ENTITY_SHEEP_WOOL},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4, .u=0,  .v=16, .mirror=true},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=6,  .d=4, .def=0.5f, .u=0, .v=16, .mirror=true, .tile=T_ENTITY_SHEEP_WOOL},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4, .u=0,  .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=6,  .d=4, .def=0.5f, .u=0, .v=16, .tile=T_ENTITY_SHEEP_WOOL},
};
static const MobPart kPartsSheep[] = {
    {.ox=0, .oy=6, .oz=-8, .parent=-1, .anim=AN_HEAD, .cube0=0, .cubeN=2},
    {.ox=0, .oy=5, .oz=2, .rx=PI/2, .parent=-1, .cube0=2, .cubeN=2},
    {.ox=-3, .oy=12, .oz=7, .parent=-1, .anim=AN_LEG_R2, .cube0=4, .cubeN=2},
    {.ox=3, .oy=12, .oz=7, .parent=-1, .anim=AN_LEG_L2, .cube0=6, .cubeN=2},
    {.ox=-3, .oy=12, .oz=-5, .parent=-1, .anim=AN_LEG_R, .cube0=8, .cubeN=2},
    {.ox=3, .oy=12, .oz=-5, .parent=-1, .anim=AN_LEG_L, .cube0=10, .cubeN=2},
};
static const MobModel kModelSheep{T_ENTITY_SHEEP, 64, 32, true,
                                  kPartsSheep, (int)(sizeof(kPartsSheep)/sizeof(MobPart)),
                                  kCubesSheep, (int)(sizeof(kCubesSheep)/sizeof(MobCube))};

// ---------------------------------------------------------------------------
// 鸡 64×32（喙与鸡冠是挂在 head 上的子 part）
// ---------------------------------------------------------------------------
static const MobCube kCubesChicken[] = {
    {.x0=-2, .y0=-6, .z0=-2, .w=4, .h=6, .d=3, .u=0,  .v=0},
    {.x0=-2, .y0=-4, .z0=-4, .w=4, .h=2, .d=2, .u=14, .v=0},
    {.x0=-1, .y0=-2, .z0=-3, .w=2, .h=2, .d=2, .u=14, .v=4},
    {.x0=-3, .y0=-4, .z0=-3, .w=6, .h=8, .d=6, .u=0,  .v=9},
    {.x0=-1, .y0=0,  .z0=-3, .w=3, .h=5, .d=3, .u=26, .v=0},
    {.x0=-1, .y0=0,  .z0=-3, .w=3, .h=5, .d=3, .u=26, .v=0},
    {.x0=0,  .y0=0,  .z0=-3, .w=1, .h=4, .d=6, .u=24, .v=13},
    {.x0=-1, .y0=0,  .z0=-3, .w=1, .h=4, .d=6, .u=24, .v=13},
};
static const MobPart kPartsChicken[] = {
    {.ox=0, .oy=15, .oz=-4, .parent=-1, .anim=AN_HEAD, .cube0=0, .cubeN=1},
    {.ox=0, .oy=0, .oz=0, .parent=0, .cube0=1, .cubeN=1},
    {.ox=0, .oy=0, .oz=0, .parent=0, .cube0=2, .cubeN=1},
    {.ox=0, .oy=16, .oz=0, .rx=PI/2, .parent=-1, .cube0=3, .cubeN=1},
    {.ox=-2, .oy=19, .oz=1, .parent=-1, .anim=AN_LEG_R, .cube0=4, .cubeN=1},
    {.ox=1, .oy=19, .oz=1, .parent=-1, .anim=AN_LEG_L, .cube0=5, .cubeN=1},
    {.ox=-4, .oy=13, .oz=0, .parent=-1, .anim=AN_WING_R, .cube0=6, .cubeN=1},
    {.ox=4, .oy=13, .oz=0, .parent=-1, .anim=AN_WING_L, .cube0=7, .cubeN=1},
};
static const MobModel kModelChicken{T_ENTITY_CHICKEN, 64, 32, true,
                                    kPartsChicken, (int)(sizeof(kPartsChicken)/sizeof(MobPart)),
                                    kCubesChicken, (int)(sizeof(kCubesChicken)/sizeof(MobCube))};

// ---------------------------------------------------------------------------
// 僵尸 64×64（几何与玩家 wide 相同，左臂/左腿 mirror 复用右侧贴图）
// ---------------------------------------------------------------------------
static const MobCube kCubesZombie[] = {
    {.x0=-4, .y0=-8, .z0=-4, .w=8, .h=8,  .d=8, .u=0,  .v=0},
    {.x0=-4, .y0=-8, .z0=-4, .w=8, .h=8,  .d=8, .def=0.5f, .u=32, .v=0},
    {.x0=-4, .y0=0,  .z0=-2, .w=8, .h=12, .d=4, .u=16, .v=16},
    {.x0=-3, .y0=-2, .z0=-2, .w=4, .h=12, .d=4, .u=40, .v=16},
    {.x0=-1, .y0=-2, .z0=-2, .w=4, .h=12, .d=4, .u=40, .v=16, .mirror=true},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4, .u=0,  .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=12, .d=4, .u=0,  .v=16, .mirror=true},
};
static const MobPart kPartsZombie[] = {
    {.ox=0, .oy=0, .oz=0, .parent=-1, .anim=AN_HEAD, .cube0=0, .cubeN=1},
    {.ox=0, .oy=0, .oz=0, .parent=0, .cube0=1, .cubeN=1},
    {.ox=0, .oy=0, .oz=0, .parent=-1, .cube0=2, .cubeN=1},
    {.ox=-5, .oy=2, .oz=0, .parent=-1, .anim=AN_ARM_R, .cube0=3, .cubeN=1},
    {.ox=5, .oy=2, .oz=0, .parent=-1, .anim=AN_ARM_L, .cube0=4, .cubeN=1},
    {.ox=-1.9f, .oy=12, .oz=0, .parent=-1, .anim=AN_LEG_R, .cube0=5, .cubeN=1},
    {.ox=1.9f, .oy=12, .oz=0, .parent=-1, .anim=AN_LEG_L, .cube0=6, .cubeN=1},
};
static const MobModel kModelZombie{T_ENTITY_ZOMBIE, 64, 64, false,
                                   kPartsZombie, (int)(sizeof(kPartsZombie)/sizeof(MobPart)),
                                   kCubesZombie, (int)(sizeof(kCubesZombie)/sizeof(MobCube))};

// ---------------------------------------------------------------------------
// 骷髅 64×32（四肢是 2×2 细杆，腿 offset x=±2）
// ---------------------------------------------------------------------------
static const MobCube kCubesSkeleton[] = {
    {.x0=-4, .y0=-8, .z0=-4, .w=8, .h=8,  .d=8, .u=0,  .v=0},
    {.x0=-4, .y0=-8, .z0=-4, .w=8, .h=8,  .d=8, .def=0.5f, .u=32, .v=0},
    {.x0=-4, .y0=0,  .z0=-2, .w=8, .h=12, .d=4, .u=16, .v=16},
    {.x0=-1, .y0=-2, .z0=-1, .w=2, .h=12, .d=2, .u=40, .v=16},
    {.x0=-1, .y0=-2, .z0=-1, .w=2, .h=12, .d=2, .u=40, .v=16, .mirror=true},
    {.x0=-1, .y0=0,  .z0=-1, .w=2, .h=12, .d=2, .u=0,  .v=16},
    {.x0=-1, .y0=0,  .z0=-1, .w=2, .h=12, .d=2, .u=0,  .v=16, .mirror=true},
};
static const MobPart kPartsSkeleton[] = {
    {.ox=0, .oy=0, .oz=0, .parent=-1, .anim=AN_HEAD, .cube0=0, .cubeN=1},
    {.ox=0, .oy=0, .oz=0, .parent=0, .cube0=1, .cubeN=1},
    {.ox=0, .oy=0, .oz=0, .parent=-1, .cube0=2, .cubeN=1},
    {.ox=-5, .oy=2, .oz=0, .parent=-1, .anim=AN_ARM_R, .cube0=3, .cubeN=1},
    {.ox=5, .oy=2, .oz=0, .parent=-1, .anim=AN_ARM_L, .cube0=4, .cubeN=1},
    {.ox=-2, .oy=12, .oz=0, .parent=-1, .anim=AN_LEG_R, .cube0=5, .cubeN=1},
    {.ox=2, .oy=12, .oz=0, .parent=-1, .anim=AN_LEG_L, .cube0=6, .cubeN=1},
};
static const MobModel kModelSkeleton{T_ENTITY_SKELETON, 64, 32, false,
                                     kPartsSkeleton, (int)(sizeof(kPartsSkeleton)/sizeof(MobPart)),
                                     kCubesSkeleton, (int)(sizeof(kCubesSkeleton)/sizeof(MobCube))};

// ---------------------------------------------------------------------------
// 苦力怕 64×32
// ---------------------------------------------------------------------------
static const MobCube kCubesCreeper[] = {
    {.x0=-4, .y0=-8, .z0=-4, .w=8, .h=8, .d=8, .u=0,  .v=0},
    {.x0=-4, .y0=0,  .z0=-2, .w=8, .h=12,.d=4, .u=16, .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=6, .d=4, .u=0,  .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=6, .d=4, .u=0,  .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=6, .d=4, .u=0,  .v=16},
    {.x0=-2, .y0=0,  .z0=-2, .w=4, .h=6, .d=4, .u=0,  .v=16},
};
static const MobPart kPartsCreeper[] = {
    {.ox=0, .oy=6, .oz=0, .parent=-1, .anim=AN_HEAD, .cube0=0, .cubeN=1},
    {.ox=0, .oy=6, .oz=0, .parent=-1, .cube0=1, .cubeN=1},
    {.ox=-2, .oy=18, .oz=4, .parent=-1, .anim=AN_LEG_R2, .cube0=2, .cubeN=1},
    {.ox=2, .oy=18, .oz=4, .parent=-1, .anim=AN_LEG_L2, .cube0=3, .cubeN=1},
    {.ox=-2, .oy=18, .oz=-4, .parent=-1, .anim=AN_LEG_R, .cube0=4, .cubeN=1},
    {.ox=2, .oy=18, .oz=-4, .parent=-1, .anim=AN_LEG_L, .cube0=5, .cubeN=1},
};
static const MobModel kModelCreeper{T_ENTITY_CREEPER, 64, 32, false,
                                    kPartsCreeper, (int)(sizeof(kPartsCreeper)/sizeof(MobPart)),
                                    kCubesCreeper, (int)(sizeof(kCubesCreeper)/sizeof(MobCube))};

// ---------------------------------------------------------------------------
// 蜘蛛 64×32（8 条腿同时有 yRot/zRot，必须按 Rz·Ry 组合）
// ---------------------------------------------------------------------------
static const MobCube kCubesSpider[] = {
    {.x0=-4, .y0=-4, .z0=-8, .w=8,  .h=8, .d=8,  .u=32, .v=4},
    {.x0=-3, .y0=-3, .z0=-3, .w=6,  .h=6, .d=6,  .u=0,  .v=0},
    {.x0=-5, .y0=-4, .z0=-6, .w=10, .h=8, .d=12, .u=0,  .v=12},
    {.x0=-15,.y0=-1, .z0=-1, .w=16, .h=2, .d=2,  .u=18, .v=0},
    {.x0=-1, .y0=-1, .z0=-1, .w=16, .h=2, .d=2,  .u=18, .v=0},
    {.x0=-15,.y0=-1, .z0=-1, .w=16, .h=2, .d=2,  .u=18, .v=0},
    {.x0=-1, .y0=-1, .z0=-1, .w=16, .h=2, .d=2,  .u=18, .v=0},
    {.x0=-15,.y0=-1, .z0=-1, .w=16, .h=2, .d=2,  .u=18, .v=0},
    {.x0=-1, .y0=-1, .z0=-1, .w=16, .h=2, .d=2,  .u=18, .v=0},
    {.x0=-15,.y0=-1, .z0=-1, .w=16, .h=2, .d=2,  .u=18, .v=0},
    {.x0=-1, .y0=-1, .z0=-1, .w=16, .h=2, .d=2,  .u=18, .v=0},
};
static const MobPart kPartsSpider[] = {
    {.ox=0, .oy=15, .oz=-3, .parent=-1, .anim=AN_HEAD, .cube0=0, .cubeN=1},
    {.ox=0, .oy=15, .oz=0, .parent=-1, .cube0=1, .cubeN=1},
    {.ox=0, .oy=15, .oz=9, .parent=-1, .cube0=2, .cubeN=1},
    // 后腿 → 中后腿 → 中前腿 → 前腿，左右对称（y 取负、z 取反）
    {.ox=-4, .oy=15, .oz=2, .ry=45*DEG, .rz=-45*DEG, .parent=-1, .anim=AN_SPIDER_LEG, .cube0=3, .cubeN=1},
    {.ox=4, .oy=15, .oz=2, .ry=-45*DEG, .rz=45*DEG, .parent=-1, .anim=AN_SPIDER_LEG, .cube0=4, .cubeN=1},
    {.ox=-4, .oy=15, .oz=1, .ry=22.5f*DEG, .rz=-33.3f*DEG, .parent=-1, .anim=AN_SPIDER_LEG, .cube0=5, .cubeN=1},
    {.ox=4, .oy=15, .oz=1, .ry=-22.5f*DEG, .rz=33.3f*DEG, .parent=-1, .anim=AN_SPIDER_LEG, .cube0=6, .cubeN=1},
    {.ox=-4, .oy=15, .oz=0, .ry=-22.5f*DEG, .rz=-33.3f*DEG, .parent=-1, .anim=AN_SPIDER_LEG, .cube0=7, .cubeN=1},
    {.ox=4, .oy=15, .oz=0, .ry=22.5f*DEG, .rz=33.3f*DEG, .parent=-1, .anim=AN_SPIDER_LEG, .cube0=8, .cubeN=1},
    {.ox=-4, .oy=15, .oz=-1, .ry=-45*DEG, .rz=-45*DEG, .parent=-1, .anim=AN_SPIDER_LEG, .cube0=9, .cubeN=1},
    {.ox=4, .oy=15, .oz=-1, .ry=45*DEG, .rz=45*DEG, .parent=-1, .anim=AN_SPIDER_LEG, .cube0=10, .cubeN=1},
};
static const MobModel kModelSpider{T_ENTITY_SPIDER, 64, 32, false,
                                  kPartsSpider, (int)(sizeof(kPartsSpider)/sizeof(MobPart)),
                                  kCubesSpider, (int)(sizeof(kCubesSpider)/sizeof(MobCube))};

// ---------------------------------------------------------------------------
// 烈焰人 64×32（1 个头 + 12 根绕 Y 自旋的棒；整体漂浮，没有腿）
// 棒的 offset 是源码循环算出来的，这里是按 float32 + Mth 表复算后的数值
// ---------------------------------------------------------------------------
static const MobCube kCubesBlaze[] = {
    {.x0=-4, .y0=-4, .z0=-4, .w=8, .h=8, .d=8, .u=0,  .v=0},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
    {.x0=0,  .y0=0,  .z0=0,  .w=2, .h=8, .d=2, .u=0,  .v=16},
};
static const MobPart kPartsBlaze[] = {
    {.ox=0, .oy=0, .oz=0, .parent=-1, .anim=AN_HEAD, .cube0=0, .cubeN=1},
    // 半径 9 的 4 根（y≈-2）
    {.ox=9, .oy=-1, .oz=0, .parent=-1, .anim=AN_ROD, .cube0=1, .cubeN=1},
    {.ox=0, .oy=-1.1224f, .oz=9, .parent=-1, .anim=AN_ROD, .cube0=2, .cubeN=1},
    {.ox=-9, .oy=-1.4597f, .oz=0, .parent=-1, .anim=AN_ROD, .cube0=3, .cubeN=1},
    {.ox=0, .oy=-1.9292f, .oz=-9, .parent=-1, .anim=AN_ROD, .cube0=4, .cubeN=1},
    // 半径 7 的 4 根（y≈+2）
    {.ox=4.9497f, .oy=1.5839f, .oz=4.9497f, .parent=-1, .anim=AN_ROD, .cube0=5, .cubeN=1},
    {.ox=-4.9497f, .oy=1.1989f, .oz=4.9497f, .parent=-1, .anim=AN_ROD, .cube0=6, .cubeN=1},
    {.ox=-4.9497f, .oy=1.0100f, .oz=-4.9497f, .parent=-1, .anim=AN_ROD, .cube0=7, .cubeN=1},
    {.ox=4.9497f, .oy=1.0635f, .oz=-4.9497f, .parent=-1, .anim=AN_ROD, .cube0=8, .cubeN=1},
    // 半径 5 的 4 根（y≈+11）
    {.ox=4.4551f, .oy=11.9602f, .oz=2.2699f, .parent=-1, .anim=AN_ROD, .cube0=9, .cubeN=1},
    {.ox=-2.2699f, .oy=11.8930f, .oz=4.4551f, .parent=-1, .anim=AN_ROD, .cube0=10, .cubeN=1},
    {.ox=-4.4551f, .oy=11.3467f, .oz=-2.2699f, .parent=-1, .anim=AN_ROD, .cube0=11, .cubeN=1},
    {.ox=2.2699f, .oy=10.6143f, .oz=-4.4551f, .parent=-1, .anim=AN_ROD, .cube0=12, .cubeN=1},
};
static const MobModel kModelBlaze{T_ENTITY_BLAZE, 64, 32, false,
                                  kPartsBlaze, (int)(sizeof(kPartsBlaze)/sizeof(MobPart)),
                                  kCubesBlaze, (int)(sizeof(kCubesBlaze)/sizeof(MobCube))};

// ---------------------------------------------------------------------------
// 末影人 64×32（长手长腿；脚底比地面低 1px 是原版行为）
// ---------------------------------------------------------------------------
static const MobCube kCubesEnderman[] = {
    {.x0=-4, .y0=-8, .z0=-4, .w=8, .h=8,  .d=8, .u=0,  .v=0},
    {.x0=-4, .y0=-8, .z0=-4, .w=8, .h=8,  .d=8, .def=-0.5f, .u=0, .v=16},
    {.x0=-4, .y0=0,  .z0=-2, .w=8, .h=12, .d=4, .u=32, .v=16},
    {.x0=-1, .y0=-2, .z0=-1, .w=2, .h=30, .d=2, .u=56, .v=0},
    {.x0=-1, .y0=-2, .z0=-1, .w=2, .h=30, .d=2, .u=56, .v=0, .mirror=true},
    {.x0=-1, .y0=0,  .z0=-1, .w=2, .h=30, .d=2, .u=56, .v=0},
    {.x0=-1, .y0=0,  .z0=-1, .w=2, .h=30, .d=2, .u=56, .v=0, .mirror=true},
};
static const MobPart kPartsEnderman[] = {
    {.ox=0, .oy=-13, .oz=0, .parent=-1, .anim=AN_HEAD, .cube0=0, .cubeN=1},
    {.ox=0, .oy=0, .oz=0, .parent=0, .cube0=1, .cubeN=1},
    {.ox=0, .oy=-14, .oz=0, .parent=-1, .cube0=2, .cubeN=1},
    {.ox=-5, .oy=-12, .oz=0, .parent=-1, .anim=AN_ARM_R, .cube0=3, .cubeN=1},
    {.ox=5, .oy=-12, .oz=0, .parent=-1, .anim=AN_ARM_L, .cube0=4, .cubeN=1},
    {.ox=-2, .oy=-5, .oz=0, .parent=-1, .anim=AN_LEG_R, .cube0=5, .cubeN=1},
    {.ox=2, .oy=-5, .oz=0, .parent=-1, .anim=AN_LEG_L, .cube0=6, .cubeN=1},
};
static const MobModel kModelEnderman{T_ENTITY_ENDERMAN, 64, 32, false,
                                     kPartsEnderman, (int)(sizeof(kPartsEnderman)/sizeof(MobPart)),
                                     kCubesEnderman, (int)(sizeof(kCubesEnderman)/sizeof(MobCube))};
