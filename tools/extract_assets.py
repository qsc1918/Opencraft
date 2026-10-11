#!/usr/bin/env python3
# extract_assets.py — 从 minecraft.jar 提取贴图到 assets/（跨平台版）
#
# 和 tools/extract_assets.cpp 等价，但只用 Python 标准库（zipfile），
# 所以 Windows / Linux 都能跑（C++ 那个版本要 Windows 的文件对话框 + tar.exe）。
#
# 用法：
#   python tools/extract_assets.py                 # 自动找 ~/.minecraft/versions/*/*.jar
#   python tools/extract_assets.py path/to/minecraft.jar
#   python tools/extract_assets.py <jar> <输出目录>  # 默认输出到 exe 同目录的 assets/
import os
import sys
import zipfile

# jar 内路径 → assets/ 下的相对路径（必须与 src/atlas.cpp 的清单对应）
ASSETS = [
    # 方块贴图（assets/block）
    ("assets/minecraft/textures/block/grass_block_top.png",       "block/grass_block_top.png"),
    ("assets/minecraft/textures/block/grass_block_side.png",       "block/grass_block_side.png"),
    ("assets/minecraft/textures/block/dirt.png",                   "block/dirt.png"),
    ("assets/minecraft/textures/block/stone.png",                  "block/stone.png"),
    ("assets/minecraft/textures/block/bedrock.png",                "block/bedrock.png"),
    ("assets/minecraft/textures/block/cobblestone.png",            "block/cobblestone.png"),
    ("assets/minecraft/textures/block/oak_planks.png",             "block/oak_planks.png"),
    ("assets/minecraft/textures/block/oak_log.png",                "block/oak_log.png"),
    ("assets/minecraft/textures/block/oak_log_top.png",            "block/oak_log_top.png"),
    ("assets/minecraft/textures/block/oak_leaves.png",             "block/oak_leaves.png"),
    ("assets/minecraft/textures/block/sand.png",                   "block/sand.png"),
    ("assets/minecraft/textures/block/gravel.png",                 "block/gravel.png"),
    ("assets/minecraft/textures/block/coal_ore.png",               "block/coal_ore.png"),
    ("assets/minecraft/textures/block/iron_ore.png",               "block/iron_ore.png"),
    ("assets/minecraft/textures/block/gold_ore.png",               "block/gold_ore.png"),
    ("assets/minecraft/textures/block/diamond_ore.png",            "block/diamond_ore.png"),
    ("assets/minecraft/textures/block/redstone_ore.png",           "block/redstone_ore.png"),
    ("assets/minecraft/textures/block/water_still.png",            "block/water_still.png"),
    ("assets/minecraft/textures/block/snow.png",                   "block/snow.png"),
    ("assets/minecraft/textures/block/glass.png",                  "block/glass.png"),
    ("assets/minecraft/textures/block/white_concrete.png",         "block/white_concrete.png"),
    ("assets/minecraft/textures/block/netherrack.png",             "block/netherrack.png"),
    ("assets/minecraft/textures/block/soul_sand.png",              "block/soul_sand.png"),
    ("assets/minecraft/textures/block/glowstone.png",              "block/glowstone.png"),
    ("assets/minecraft/textures/block/nether_bricks.png",          "block/nether_brick.png"),
    ("assets/minecraft/textures/block/obsidian.png",               "block/obsidian.png"),
    ("assets/minecraft/textures/block/lava_still.png",             "block/lava_still.png"),
    ("assets/minecraft/textures/block/nether_portal.png",          "block/nether_portal.png"),
    ("assets/minecraft/textures/block/end_stone.png",              "block/end_stone.png"),
    ("assets/minecraft/textures/block/end_portal_frame_top.png",   "block/end_portal_frame.png"),
    ("assets/minecraft/textures/block/end_portal_frame_side.png",  "block/end_portal_frame_side.png"),
    ("assets/minecraft/textures/block/end_portal_frame_eye.png",   "block/end_portal_frame_eye.png"),
    ("assets/minecraft/textures/block/dragon_egg.png",             "block/dragon_egg.png"),
    ("assets/minecraft/textures/block/end_sky.png",                "block/end_sky.png"),
    ("assets/minecraft/textures/block/chorus_plant.png",           "block/chorus_plant.png"),
    ("assets/minecraft/textures/block/chorus_flower.png",          "block/chorus_flower.png"),
    ("assets/minecraft/textures/block/chorus_flower_dead.png",     "block/chorus_flower_dead.png"),
    ("assets/minecraft/textures/block/iron_bars.png",              "block/iron_bars.png"),
    ("assets/minecraft/textures/block/torch.png",                  "block/torch.png"),
    # 实体贴图（assets/entity）：生物皮肤按原版 texOffs 逐面取子矩形，比例不能动
    ("assets/minecraft/textures/entity/pig/pig_temperate.png",        "entity/pig.png"),
    ("assets/minecraft/textures/entity/cow/cow_temperate.png",        "entity/cow.png"),
    ("assets/minecraft/textures/entity/sheep/sheep.png",              "entity/sheep.png"),
    ("assets/minecraft/textures/entity/sheep/sheep_wool.png",         "entity/sheep_wool.png"),
    ("assets/minecraft/textures/entity/chicken/chicken_temperate.png", "entity/chicken.png"),
    ("assets/minecraft/textures/entity/zombie/zombie.png",            "entity/zombie.png"),
    ("assets/minecraft/textures/entity/skeleton/skeleton.png",        "entity/skeleton.png"),
    ("assets/minecraft/textures/entity/creeper/creeper.png",          "entity/creeper.png"),
    ("assets/minecraft/textures/entity/spider/spider.png",            "entity/spider.png"),
    ("assets/minecraft/textures/entity/blaze/blaze.png",              "entity/blaze.png"),
    ("assets/minecraft/textures/entity/enderman/enderman.png",        "entity/enderman.png"),
    ("assets/minecraft/textures/entity/player/wide/steve.png",        "entity/steve.png"),
    ("assets/minecraft/textures/entity/projectiles/arrow.png",        "entity/arrow.png"),
    # GUI 贴图（assets/gui）
    ("assets/minecraft/textures/gui/sprites/widget/button.png",               "gui/button.png"),
    ("assets/minecraft/textures/gui/sprites/widget/button_highlighted.png",   "gui/button_highlighted.png"),
    ("assets/minecraft/textures/gui/sprites/widget/text_field.png",           "gui/text_field.png"),
    ("assets/minecraft/textures/gui/sprites/widget/slider.png",               "gui/slider.png"),
    ("assets/minecraft/textures/gui/sprites/widget/slider_highlighted.png",   "gui/slider_highlighted.png"),
    ("assets/minecraft/textures/gui/sprites/widget/slider_handle.png",        "gui/slider_handle.png"),
    ("assets/minecraft/textures/gui/sprites/widget/slider_handle_highlighted.png", "gui/slider_handle_highlighted.png"),
]

# 物品图标：按层级规律展开（顺序必须与 src/items.hpp 的分配一致）
for tier in ("wooden", "stone", "iron", "golden", "diamond"):
    for kind in ("pickaxe", "axe", "shovel", "sword"):
        ASSETS.append((f"assets/minecraft/textures/item/{tier}_{kind}.png",
                       f"item/{tier}_{kind}.png"))
for mat in ("leather", "iron", "golden", "diamond"):
    for part in ("helmet", "chestplate", "leggings", "boots"):
        ASSETS.append((f"assets/minecraft/textures/item/{mat}_{part}.png",
                       f"item/{mat}_{part}.png"))
for misc in ("flint_and_steel", "ender_pearl", "ender_eye"):
    ASSETS.append((f"assets/minecraft/textures/item/{misc}.png",
                   f"item/{misc}.png"))


def find_jar():
    """在常见的 Minecraft 目录里找最新的 jar"""
    import glob
    patterns = [
        os.path.expanduser("~/.minecraft/versions/*/*/*.jar"),
        os.path.expanduser("~/Library/Application Support/minecraft/versions/*/*/*.jar"),
        "/mnt/c/Users/*/AppData/Roaming/.minecraft/versions/*/*/*.jar",
    ]
    found = []
    for pat in patterns:
        found += glob.glob(pat)
    # 排除 libraries 与 .fabric 之类，只留版本目录下的正式 jar
    found = [f for f in found if os.path.basename(f).endswith(".jar")
             and "libraries" not in f]
    if not found:
        return None
    found.sort(key=lambda p: os.path.getmtime(p))
    return found[-1]


def main():
    jar = sys.argv[1] if len(sys.argv) > 1 else find_jar()
    if not jar or not os.path.exists(jar):
        print("找不到 minecraft.jar，请把路径作为第一个参数传进来")
        return 1
    out_root = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "assets")
    out_root = os.path.abspath(out_root)

    print(f"从 {jar} 提取到 {out_root}")
    copied = missing = 0
    with zipfile.ZipFile(jar) as z:
        names = set(z.namelist())
        for src, dst in ASSETS:
            if src not in names:
                print(f"  缺失: {src}")
                missing += 1
                continue
            target = os.path.join(out_root, dst)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with z.open(src) as fsrc, open(target, "wb") as fdst:
                fdst.write(fsrc.read())
            copied += 1
    print(f"完成：{copied} 个资源，{missing} 个缺失")
    return 0


if __name__ == "__main__":
    sys.exit(main())
