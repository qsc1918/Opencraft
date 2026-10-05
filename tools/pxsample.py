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
    step = nch * (bd // 8)
    stride = w * step
    out = []; prev = bytearray(stride); p = 0
    for y in range(h):
        f = raw[p]; p += 1
        line = bytearray(raw[p:p+stride]); p += stride
        if f == 1:
            for i in range(step, stride): line[i] = (line[i] + line[i-step]) & 255
        elif f == 2:
            for i in range(stride): line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                a = line[i-step] if i >= step else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i-step] if i >= step else 0
                c = prev[i-step] if i >= step else 0
                b = prev[i]; pp = a + b - c
                pa, pb, pc = abs(pp-a), abs(pp-b), abs(pp-c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        out.append(bytes(line)); prev = line
    def px(x, y):
        i = x * step   # out 是按行存的，行内偏移即可
        if bd == 16:
            v = [out[y][i + c*2] for c in range(nch)]
        else:
            v = [out[y][i + c] for c in range(nch)]
        if ct == 6: return v[0], v[1], v[2]
        if ct == 2: return v[0], v[1], v[2]
        return v[0], v[0], v[0]
    return w, h, px

def stats(path, x0, y0, x1, y1):
    w, h, px = readpng(path)
    n = 0; sr = sg = sb = 0; mx = 0; hist = [0]*8
    for y in range(max(0,y0), min(h,y1)):
        for x in range(max(0,x0), min(w,x1)):
            r, g, b = px(x, y)
            sr += r; sg += g; sb += b; n += 1
            lum = (r*3 + g*4 + b) // 8
            mx = max(mx, lum)
            hist[min(7, lum // 32)] += 1
    print(f"{path.split('/')[-1]} 区域({x0},{y0})-({x1},{y1}) 共{n}px")
    print(f"  平均 RGB = ({sr//n}, {sg//n}, {sb//n})   最亮 = {mx}")
    print("  亮度直方图(0-31,32-63,...):", hist)

if __name__ == '__main__':
    stats(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]))
