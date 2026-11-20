#!/usr/bin/env python3
"""把方舟像素字体的 BDF 点阵烘焙成游戏用的字形图集与码点表。

字体本体是 SIL OFL-1.1，构建程序用标准库即可。图集只收录项目里实际出现的字符，
避免把整套 CJK 字体塞进安装包。
"""
import re
import struct
import sys
import zlib
from pathlib import Path

CELL = 24  # 12px 点阵的 FONTBOUNDINGBOX 高 16，留足上下伸空间
MAGIC = 0x464E544F  # 'OTNF'，小端读出来是 FONT
VERSION = 1


def parse_bdf(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    ascent = descent = 0
    for line in lines:
        if line.startswith("FONT_ASCENT "):
            ascent = int(line.split()[1])
        elif line.startswith("FONT_DESCENT "):
            descent = int(line.split()[1])
    glyphs = {}
    codepoint = None
    width = height = xoff = yoff = 0
    in_bitmap = False
    rows = []
    for line in lines:
        if line.startswith("ENCODING "):
            codepoint = int(line.split()[1])
        elif line.startswith("DWIDTH "):
            advance = int(line.split()[1])
        elif line.startswith("BBX "):
            _, width, height, xoff, yoff = line.split()
            width, height, xoff, yoff = int(width), int(height), int(xoff), int(yoff)
        elif line == "BITMAP":
            in_bitmap = True
            rows = []
        elif line == "ENDCHAR":
            in_bitmap = False
            if codepoint is not None and width and height:
                bits = bytearray()
                for row in rows:
                    # BDF 的十六进制行按 nibble 左对齐，真正的高宽只有 BBX 里的 width 位。
                    value = int(row, 16) >> (len(row) * 4 - width)
                    for bit in range(width):
                        bits.append(0xFF if value & (1 << (width - 1 - bit)) else 0)
                glyphs[codepoint] = {
                    "w": width, "h": height, "xoff": xoff, "yoff": yoff,
                    "advance": advance, "bits": bytes(bits),
                }
            codepoint = None
        elif in_bitmap and line.strip():
            rows.append(line.strip())
    return glyphs, ascent, descent


def collect_chars(root):
    chars = set(range(0x20, 0x7F))
    chars.update(ord(c) for c in "：，。、（）＊—·…「」《》￥％")
    pattern = re.compile(r'"([^"\\]*(?:\\.[^"\\]*)*)"')
    for path in Path(root).glob("src/*"):
        if path.suffix not in (".cpp", ".hpp"):
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for match in pattern.finditer(text):
            for ch in match.group(1):
                if ord(ch) > 0x7F:
                    chars.add(ord(ch))
    return sorted(chars)


def write_png(path, width, height, rgba):
    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += rgba[y * width * 4:(y + 1) * width * 4]
    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    out += chunk(b"IEND", b"")
    Path(path).write_bytes(out)


def main():
    if len(sys.argv) < 6:
        print("用法: bake_font.py <latin.bdf> <zh_hans.bdf> <源码目录> <输出图集.png> <输出码点表.fnt>")
        return 1
    latin, zh_hans, root, out_png, out_fnt = sys.argv[1:6]
    glyphs, ascent, descent = parse_bdf(Path(latin))
    cjk, asc2, desc2 = parse_bdf(Path(zh_hans))
    assert asc2 == ascent and desc2 == descent, "两个 BDF 的基线不一致"
    glyphs.update(cjk)

    wanted = [cp for cp in collect_chars(root) if cp in glyphs]
    missing = [cp for cp in collect_chars(root) if cp not in glyphs]
    if missing:
        print("缺字 %d 个: %s" % (len(missing), "".join(chr(c) for c in missing[:60])))

    cols = max(16, min(64, 1 << (len(wanted) - 1).bit_length()))
    cols = min(cols, 64)
    rows = (len(wanted) + cols - 1) // cols
    atlas_w, atlas_h = cols * CELL, rows * CELL
    rgba = bytearray(atlas_w * atlas_h * 4)
    records = bytearray()
    for index, cp in enumerate(wanted):
        g = glyphs[cp]
        cell_x = (index % cols) * CELL
        cell_y = (index // cols) * CELL
        # 基线放在格子中部偏下，给上伸/下伸字符都留空间。
        base_y = cell_y + ascent
        for row in range(g["h"]):
            for col in range(g["w"]):
                alpha = g["bits"][row * g["w"] + col]
                if not alpha:
                    continue
                px = cell_x + g["xoff"] + col
                py = base_y - (g["yoff"] + g["h"]) + row
                if 0 <= px < atlas_w and 0 <= py < atlas_h:
                    off = (py * atlas_w + px) * 4
                    rgba[off:off + 4] = b"\xff\xff\xff\xff"
        # 只记录墨迹矩形；烘焙时已把 bearing 应用到图集位置，运行时直接贴到笔尖即可。
        ink_x = cell_x + g["xoff"]
        ink_y = cell_y + ascent - g["yoff"] - g["h"]
        records += struct.pack("<IHHHHhhH", cp, ink_x, ink_y, g["w"], g["h"], 0, 0, g["advance"])

    header = struct.pack("<IIIIHHHHHI", MAGIC, VERSION, atlas_w, atlas_h,
                         CELL, CELL, ascent, descent, ascent + descent + 1, len(wanted))
    Path(out_fnt).write_bytes(header + bytes(records))
    write_png(out_png, atlas_w, atlas_h, bytes(rgba))
    print("烘焙 %d 字形，图集 %dx%d，缺字 %d" % (len(wanted), atlas_w, atlas_h, len(missing)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
