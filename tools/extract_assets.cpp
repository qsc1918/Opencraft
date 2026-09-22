// extract_assets.cpp — 从官方 minecraft.jar 提取方块/GUI/物品贴图到 exe 旁的 assets/
// 编译: 正常流程由 CMake 构建，产物 build/extract_assets.exe
//       单独编译: g++ tools/extract_assets.cpp -o extract_assets.exe -lcomdlg32
// 用法: extract_assets.exe [minecraft.jar 路径]
// 注意: 贴图属于 Minecraft 资源，不得再分发（遵守 Mojang EULA）

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// jar 内路径 → assets/ 下的相对路径
struct AssetEntry {
    const char* src;
    const char* dst;
};

static const AssetEntry kAssets[] = {
    // 方块贴图（assets/block）
    {"assets/minecraft/textures/block/grass_block_top.png",       "block/grass_block_top.png"},
    {"assets/minecraft/textures/block/grass_block_side.png",       "block/grass_block_side.png"},
    {"assets/minecraft/textures/block/dirt.png",                   "block/dirt.png"},
    {"assets/minecraft/textures/block/stone.png",                  "block/stone.png"},
    {"assets/minecraft/textures/block/bedrock.png",                "block/bedrock.png"},
    {"assets/minecraft/textures/block/cobblestone.png",            "block/cobblestone.png"},
    {"assets/minecraft/textures/block/oak_planks.png",             "block/oak_planks.png"},
    {"assets/minecraft/textures/block/oak_log.png",                "block/oak_log.png"},
    {"assets/minecraft/textures/block/oak_log_top.png",            "block/oak_log_top.png"},
    {"assets/minecraft/textures/block/oak_leaves.png",             "block/oak_leaves.png"},
    {"assets/minecraft/textures/block/sand.png",                   "block/sand.png"},
    {"assets/minecraft/textures/block/gravel.png",                 "block/gravel.png"},
    {"assets/minecraft/textures/block/coal_ore.png",               "block/coal_ore.png"},
    {"assets/minecraft/textures/block/iron_ore.png",               "block/iron_ore.png"},
    {"assets/minecraft/textures/block/gold_ore.png",               "block/gold_ore.png"},
    {"assets/minecraft/textures/block/diamond_ore.png",            "block/diamond_ore.png"},
    {"assets/minecraft/textures/block/redstone_ore.png",           "block/redstone_ore.png"},
    {"assets/minecraft/textures/block/water_still.png",            "block/water_still.png"},
    {"assets/minecraft/textures/block/snow.png",                   "block/snow.png"},
    {"assets/minecraft/textures/block/glass.png",                  "block/glass.png"},
    {"assets/minecraft/textures/block/white_concrete.png",         "block/white_concrete.png"},
    // 下界/末地方块贴图；1.21.4 起部分贴图改名（nether_bricks、
    // end_portal_frame_top/eye）；end_portal / end_gateway 是 entity 贴图，用程序化纹理兜底
    {"assets/minecraft/textures/block/netherrack.png",             "block/netherrack.png"},
    {"assets/minecraft/textures/block/soul_sand.png",              "block/soul_sand.png"},
    {"assets/minecraft/textures/block/glowstone.png",              "block/glowstone.png"},
    {"assets/minecraft/textures/block/nether_bricks.png",          "block/nether_brick.png"},
    {"assets/minecraft/textures/block/obsidian.png",               "block/obsidian.png"},
    {"assets/minecraft/textures/block/lava_still.png",             "block/lava_still.png"},
    {"assets/minecraft/textures/block/nether_portal.png",          "block/nether_portal.png"},
    {"assets/minecraft/textures/block/end_stone.png",              "block/end_stone.png"},
    {"assets/minecraft/textures/block/end_portal_frame_top.png",   "block/end_portal_frame.png"},
    {"assets/minecraft/textures/block/end_portal_frame_side.png",  "block/end_portal_frame_side.png"},
    {"assets/minecraft/textures/block/end_portal_frame_eye.png",   "block/end_portal_frame_eye.png"},
    {"assets/minecraft/textures/block/dragon_egg.png",             "block/dragon_egg.png"},
    // GUI 贴图（assets/gui）
    {"assets/minecraft/textures/gui/sprites/widget/button.png",               "gui/button.png"},
    {"assets/minecraft/textures/gui/sprites/widget/button_highlighted.png",   "gui/button_highlighted.png"},
    {"assets/minecraft/textures/gui/sprites/widget/text_field.png",           "gui/text_field.png"},
    {"assets/minecraft/textures/gui/sprites/widget/slider.png",               "gui/slider.png"},
    {"assets/minecraft/textures/gui/sprites/widget/slider_highlighted.png",   "gui/slider_highlighted.png"},
    {"assets/minecraft/textures/gui/sprites/widget/slider_handle.png",        "gui/slider_handle.png"},
    {"assets/minecraft/textures/gui/sprites/widget/slider_handle_highlighted.png", "gui/slider_handle_highlighted.png"},
};

static std::string pickJarFile() {
    char buf[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Minecraft jar\0*.jar\0All files\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Select a Minecraft jar";
    if (!GetOpenFileNameA(&ofn)) return "";
    return std::string(buf);
}

static std::string outputRoot() {
    // 输出到 exe 所在目录的 assets/（把 extract_assets.exe 放在项目根目录）
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    return pos == std::string::npos ? "." : p.substr(0, pos);
}

static bool runTar(const std::string& jar, const std::string& outDir) {
    std::string cmd = "tar -xf \"" + jar + "\" -C \"" + outDir
                      + "\" assets/minecraft/textures/block assets/minecraft/textures/gui/sprites/widget"
                        " assets/minecraft/textures/item 2>nul";
    return system(cmd.c_str()) == 0;
}

// 物品贴图：assets/minecraft/textures/item/*.png → assets/item/
static void appendItemEntries(std::vector<AssetEntry>& out) {
    static const char* kToolTiers[] = {"wooden", "stone", "iron", "golden", "diamond"};
    static const char* kToolKinds[] = {"pickaxe", "axe", "shovel", "sword"};
    static const char* kArmorMats[] = {"leather", "iron", "golden", "diamond"};
    static const char* kArmorParts[] = {"helmet", "chestplate", "leggings", "boots"};
    static const char* kMisc[] = {"flint_and_steel", "ender_pearl", "ender_eye"};
    char buf[128];
    for (const char* tier : kToolTiers)
        for (const char* kind : kToolKinds) {
            _snprintf(buf, sizeof(buf) - 1, "assets/minecraft/textures/item/%s_%s.png", tier, kind);
            std::string src = buf;
            _snprintf(buf, sizeof(buf) - 1, "item/%s_%s.png", tier, kind);
            out.push_back({strdup(src.c_str()), strdup(buf)});
        }
    for (const char* mat : kArmorMats)
        for (const char* part : kArmorParts) {
            _snprintf(buf, sizeof(buf) - 1, "assets/minecraft/textures/item/%s_%s.png", mat, part);
            std::string src = buf;
            _snprintf(buf, sizeof(buf) - 1, "item/%s_%s.png", mat, part);
            out.push_back({strdup(src.c_str()), strdup(buf)});
        }
    for (const char* m : kMisc) {
        _snprintf(buf, sizeof(buf) - 1, "assets/minecraft/textures/item/%s.png", m);
        std::string src = buf;
        _snprintf(buf, sizeof(buf) - 1, "item/%s.png", m);
        out.push_back({strdup(src.c_str()), strdup(buf)});
    }
}

int main(int argc, char** argv) {
    std::string jar = argc > 1 ? argv[1] : pickJarFile();
    if (jar.empty()) {
        printf("No jar selected.\n");
        return 1;
    }
    if (!fs::exists(jar)) {
        printf("File not found: %s\n", jar.c_str());
        return 1;
    }

    bool haveOldTextField = false;
    {
        // 检查该 jar 用的是哪个 text_field 路径
        std::string chk = "tar -tf \"" + jar + "\" 2>nul | findstr /C:\"textures/gui/sprites/widget/text_field.png\" >nul";
        haveOldTextField = system(chk.c_str()) == 0;
        (void)haveOldTextField;
    }

    fs::path tmp = fs::temp_directory_path() / "opencraft_assets";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    printf("Extracting from %s ...\n", jar.c_str());
    if (!runTar(jar, tmp.string())) {
        printf("tar extraction failed. Is bsdtar available? (Win10+ ships tar.exe)\n");
        fs::remove_all(tmp);
        return 1;
    }

    fs::path dstRoot = fs::path(outputRoot()) / "assets";
    std::vector<AssetEntry> entries(std::begin(kAssets), std::end(kAssets));
    appendItemEntries(entries);
    int copied = 0, missing = 0;
    for (const AssetEntry& e : entries) {
        fs::path src = tmp / e.src;
        fs::path dst = dstRoot / e.dst;
        fs::create_directories(dst.parent_path());
        if (!fs::exists(src)) {
            // 旧版 text_field 路径兜底
            fs::path alt = tmp / "assets/minecraft/textures/gui/sprites/widget/text_field.png";
            if (std::string(e.src).find("text_field") != std::string::npos && fs::exists(alt)) {
                src = alt;
            } else {
                printf("  MISSING: %s\n", e.src);
                missing++;
                continue;
            }
        }
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        copied++;
    }

    printf("Done: %d resources copied to %s, %d missing.\n", copied,
           dstRoot.string().c_str(), missing);
    fs::remove_all(tmp);
    return 0;
}
