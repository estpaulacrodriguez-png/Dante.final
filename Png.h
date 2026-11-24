// Png.h - Decodificador PNG minimo (8 bits, RGB/RGBA/gris, sin entrelazado).
// Incluye su propio inflate (zlib), asi no se necesita ninguna libreria externa.
#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace png_detail
{
    struct Huff { short count[16]; short symbol[288]; };

    struct Inflater
    {
        const uint8_t* in; size_t inLen, inPos = 0; uint32_t bitBuf = 0; int bitCnt = 0;
        std::vector<uint8_t>& out; bool err = false;
        Inflater(const uint8_t* d, size_t n, std::vector<uint8_t>& o) : in(d), inLen(n), out(o) {}

        int bits(int need)
        {
            uint32_t v = bitBuf;
            while (bitCnt < need)
            {
                if (inPos >= inLen) { err = true; return 0; }
                v |= (uint32_t)in[inPos++] << bitCnt; bitCnt += 8;
            }
            bitBuf = v >> need; bitCnt -= need;
            return (int)(v & ((1u << need) - 1));
        }
        static void build(Huff& h, const short* len, int n)
        {
            short offs[16];
            for (int i = 0; i < 16; ++i) h.count[i] = 0;
            for (int s = 0; s < n; ++s) h.count[len[s]]++;
            offs[1] = 0;
            for (int l = 1; l < 15; ++l) offs[l + 1] = offs[l] + h.count[l];
            for (int s = 0; s < n; ++s) if (len[s]) h.symbol[offs[len[s]]++] = (short)s;
        }
        int decode(const Huff& h)
        {
            int code = 0, first = 0, index = 0;
            for (int l = 1; l <= 15; ++l)
            {
                code |= bits(1);
                int c = h.count[l];
                if (code - c < first) return h.symbol[index + (code - first)];
                index += c; first += c; first <<= 1; code <<= 1;
                if (err) return -1;
            }
            return -1;
        }
        bool codes(const Huff& lit, const Huff& dist)
        {
            static const short lb[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
            static const short le[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
            static const short db[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
            static const short de[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
            for (;;)
            {
                int s = decode(lit);
                if (s < 0 || err) return false;
                if (s < 256) out.push_back((uint8_t)s);
                else if (s == 256) return true;
                else
                {
                    s -= 257; if (s >= 29) return false;
                    int len = lb[s] + bits(le[s]);
                    int ds = decode(dist); if (ds < 0 || ds >= 30) return false;
                    size_t d = db[ds] + bits(de[ds]);
                    if (d > out.size()) return false;
                    size_t from = out.size() - d;
                    for (int k = 0; k < len; ++k) out.push_back(out[from + k]);
                }
            }
        }
        bool run()
        {
            if (inLen < 2) return false;
            inPos = 2;                                   // cabecera zlib
            int last;
            do
            {
                last = bits(1); int type = bits(2);
                if (err) return false;
                if (type == 0)
                {
                    bitBuf = 0; bitCnt = 0;
                    if (inPos + 4 > inLen) return false;
                    unsigned len = in[inPos] | (in[inPos + 1] << 8); inPos += 4;
                    if (inPos + len > inLen) return false;
                    out.insert(out.end(), in + inPos, in + inPos + len); inPos += len;
                }
                else if (type == 1)
                {
                    short l[288]; Huff lit, dist;
                    int i = 0;
                    for (; i < 144; ++i) l[i] = 8;
                    for (; i < 256; ++i) l[i] = 9;
                    for (; i < 280; ++i) l[i] = 7;
                    for (; i < 288; ++i) l[i] = 8;
                    build(lit, l, 288);
                    for (i = 0; i < 30; ++i) l[i] = 5;
                    build(dist, l, 30);
                    if (!codes(lit, dist)) return false;
                }
                else if (type == 2)
                {
                    static const short order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
                    short l[320]; Huff lencode, lit, dist;
                    int nlen = bits(5) + 257, ndist = bits(5) + 1, ncode = bits(4) + 4;
                    if (nlen > 286 || ndist > 30) return false;
                    int i = 0;
                    for (; i < ncode; ++i) l[order[i]] = (short)bits(3);
                    for (; i < 19; ++i) l[order[i]] = 0;
                    build(lencode, l, 19);
                    i = 0;
                    while (i < nlen + ndist)
                    {
                        int s = decode(lencode); if (s < 0 || err) return false;
                        if (s < 16) l[i++] = (short)s;
                        else
                        {
                            int rep, v = 0;
                            if (s == 16) { if (i == 0) return false; v = l[i - 1]; rep = 3 + bits(2); }
                            else if (s == 17) rep = 3 + bits(3);
                            else rep = 11 + bits(7);
                            if (i + rep > nlen + ndist) return false;
                            while (rep--) l[i++] = (short)v;
                        }
                    }
                    build(lit, l, nlen);
                    build(dist, l + nlen, ndist);
                    if (!codes(lit, dist)) return false;
                }
                else return false;
            } while (!last);
            return true;
        }
    };
}

// Devuelve true si logra decodificar. 'rgba' queda con w*h*4 bytes (fila superior primero).
inline bool loadPNG(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba)
{
    FILE* f = fopen(path.c_str(), "rb"); if (!f) return false;
    std::vector<uint8_t> file; { uint8_t buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) file.insert(file.end(), buf, buf + n); }
    fclose(f);
    static const uint8_t sig[8] = { 137,80,78,71,13,10,26,10 };
    if (file.size() < 33 || memcmp(file.data(), sig, 8)) return false;

    auto be32 = [&](size_t o) { return (uint32_t)file[o] << 24 | file[o + 1] << 16 | file[o + 2] << 8 | file[o + 3]; };
    int depth = 0, ctype = 0, interlace = 0; std::vector<uint8_t> idat;
    size_t pos = 8;
    while (pos + 8 <= file.size())
    {
        uint32_t len = be32(pos); std::string type((const char*)&file[pos + 4], 4); size_t d = pos + 8;
        if (d + len > file.size()) return false;
        if (type == "IHDR") { w = (int)be32(d); h = (int)be32(d + 4); depth = file[d + 8]; ctype = file[d + 9]; interlace = file[d + 12]; }
        else if (type == "IDAT") idat.insert(idat.end(), file.begin() + d, file.begin() + d + len);
        else if (type == "IEND") break;
        pos = d + len + 4;
    }
    int ch = ctype == 2 ? 3 : ctype == 6 ? 4 : ctype == 0 ? 1 : 0;
    if (depth != 8 || interlace != 0 || ch == 0 || w <= 0 || h <= 0) return false;

    std::vector<uint8_t> raw; raw.reserve((size_t)(w * ch + 1) * h);
    png_detail::Inflater inf(idat.data(), idat.size(), raw);
    if (!inf.run() || raw.size() < (size_t)(w * ch + 1) * h) return false;

    // quitar filtros por fila
    size_t stride = (size_t)w * ch; std::vector<uint8_t> img(stride * h);
    for (int y = 0; y < h; ++y)
    {
        uint8_t ft = raw[y * (stride + 1)]; const uint8_t* src = &raw[y * (stride + 1) + 1];
        uint8_t* cur = &img[y * stride]; const uint8_t* up = y ? &img[(y - 1) * stride] : nullptr;
        for (size_t x = 0; x < stride; ++x)
        {
            int a = x >= (size_t)ch ? cur[x - ch] : 0, b = up ? up[x] : 0, c = (up && x >= (size_t)ch) ? up[x - ch] : 0, v = src[x];
            switch (ft)
            {
            case 1: v += a; break;
            case 2: v += b; break;
            case 3: v += (a + b) / 2; break;
            case 4: { int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
                      v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c); break; }
            }
            cur[x] = (uint8_t)v;
        }
    }
    rgba.resize((size_t)w * h * 4);
    for (size_t i = 0, n = (size_t)w * h; i < n; ++i)
    {
        const uint8_t* s = &img[i * ch]; uint8_t* d = &rgba[i * 4];
        if (ch == 1) { d[0] = d[1] = d[2] = s[0]; d[3] = 255; }
        else { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = ch == 4 ? s[3] : 255; }
    }
    return true;
}
