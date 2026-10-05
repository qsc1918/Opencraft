# imgdiff.py — 比较两张 png 的差异，用来确认"优化没有改变画面"
# 用法: python imgdiff.py a.png b.png [阈值]
import struct, zlib, sys

def readpng(path):
    d = open(path, 'rb').read()
    pos = 8; idat = b''; w = h = bd = ct = 0
    while pos < len(d):
        ln = struct.unpack('>I', d[pos:pos+4])[0]
        typ = d[pos+4:pos+8]; data = d[pos+8:pos+8+ln]
        if typ == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', data[:10])
        elif typ == b'IDAT':
            idat += data
        pos += 12 + ln
    raw = zlib.decompress(idat)
    nch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]
    stride = w * nch * (bd // 8)
    out = bytearray(); prev = bytearray(stride); p = 0
    for y in range(h):
        f = raw[p]; p += 1
        line = bytearray(raw[p:p+stride]); p += stride
        if f == 1:
            for i in range(nch, stride): line[i] = (line[i] + line[i-nch]) & 255
        elif f == 2:
            for i in range(stride): line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                a = line[i-nch] if i >= nch else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i-nch] if i >= nch else 0
                c = prev[i-nch] if i >= nch else 0
                b = prev[i]; pp = a + b - c
                pa, pb, pc = abs(pp-a), abs(pp-b), abs(pp-c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        out += line; prev = line
    return w, h, nch, bytes(out)

def main():
    a, b = sys.argv[1], sys.argv[2]
    tol = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    w1, h1, n1, p1 = readpng(a)
    w2, h2, n2, p2 = readpng(b)
    if (w1, h1, n1) != (w2, h2, n2):
        print(f"尺寸不同: {a}={w1}x{h1}x{n1}  {b}={w2}x{h2}x{n2}")
        return
    nch = n1
    diff = 0; big = 0; maxd = 0; worst = None
    sumA = sumB = 0
    first = None
    for i in range(0, len(p1), nch):
        d0 = abs(p1[i] - p2[i]) + abs(p1[i+1] - p2[i+1]) + abs(p1[i+2] - p2[i+2])
        sumA += p1[i] + p1[i+1] + p1[i+2]
        sumB += p2[i] + p2[i+1] + p2[i+2]
        if d0 > tol * 3:
            diff += 1
            if d0 > maxd: maxd = d0; worst = (i // nch) % w1, (i // nch) // w1
            if d0 > 24: big += 1
            if first is None: first = ((i // nch) % w1, (i // nch) // w1)
    # 可选第三参：把差异可视化成 png（白=相同，红=不同）
    if len(sys.argv) > 3:
        import zlib as z  # 差异可视化
        vis = bytearray()
        for i in range(0, len(p1), nch):
            d0 = abs(p1[i]-p2[i]) + abs(p1[i+1]-p2[i+1]) + abs(p1[i+2]-p2[i+2])
            if d0 > tol*3: vis += bytearray([220,30,30]); vis += bytearray([255])
            else: vis += bytearray([255,255,255]); vis += bytearray([255])
        raw = b''
        for y in range(h1):
            raw += b'\x00' + bytes(vis[y*w1*4:(y+1)*w1*4])
        def chunk(t, d):
            c = t + d
            return struct.pack('>I', len(d)) + c + struct.pack('>I', z.crc32(c) & 0xffffffff)
        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w1, h1, 8, 6, 0, 0, 0)) + chunk(b'IDAT', z.compress(raw, 6)) + chunk(b'IEND', b'')
        open(sys.argv[3], 'wb').write(png)
        print("差异图 ->", sys.argv[3])
    px = w1 * h1
    print(f"{a} vs {b}: {w1}x{h1}")
    print(f"  不同像素 {diff} / {px} ({diff*100.0/px:.3f}%)，色差>8 的像素 {big}")
    print(f"  最大通道差合计 {maxd}（左上角第一个不同点 {first}，最大差异点 {worst}）")
    print(f"  全图平均 RGB  {sumA/(px*3):.2f}  vs  {sumB/(px*3):.2f}")

main()
