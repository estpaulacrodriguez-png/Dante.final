// Gltf.h - Cargador de archivos glTF 2.0 (.gltf + .bin + texturas PNG).
// Lee: mallas triangulares (POSITION, NORMAL, TEXCOORD_0, indices), jerarquia de
// nodos (matrix o TRS), materiales (baseColor o KHR_materials_pbrSpecularGlossiness)
// y sus texturas. Aplica la transformacion de cada nodo a los vertices al cargar.
// No soporta (todavia): esqueletos/skins, animaciones, .glb, buffers en base64.
#pragma once
#include "Json.h"
#include "Png.h"
#include <cmath>
#include <cstdio>

struct GltfPrim { std::vector<float> pos, nrm, uv; std::vector<unsigned> idx; int mat = -1; };

struct GltfMat
{
    std::string name;
    int w = 0, h = 0; std::vector<uint8_t> rgba;     // pixeles de la textura (se liberan tras subirla a la GPU)
    float diffuse[4] = { 1, 1, 1, 1 };
    float gloss = 0.4f;                              // 0..1 (que tan brillante es)
    unsigned glTex = 0;                              // id de textura de OpenGL
};

struct GltfModel
{
    std::vector<GltfPrim> prims;
    std::vector<GltfMat>  mats;
    float bmin[3] = { 1e9f, 1e9f, 1e9f }, bmax[3] = { -1e9f, -1e9f, -1e9f };

    bool load(const std::string& path, std::string& err)
    {
        base = path.substr(0, path.find_last_of("/\\") + 1);
        std::string text;
        if (!readFile(path, text)) { err = "No se pudo abrir " + path; return false; }
        if (!JsonParser::parse(text, g)) { err = "JSON invalido en " + path; return false; }

        // buffers
        for (size_t i = 0; i < g["buffers"].size(); ++i)
        {
            std::string bin;
            if (!readFile(base + g["buffers"][i]["uri"].s, bin)) { err = "No se pudo abrir el .bin: " + g["buffers"][i]["uri"].s; return false; }
            buffers.push_back(std::vector<uint8_t>(bin.begin(), bin.end()));
        }

        // materiales + texturas
        for (size_t i = 0; i < g["materials"].size(); ++i)
        {
            const JV& m = g["materials"][i]; GltfMat gm; gm.name = m["name"].s;
            int tex = -1;
            const JV& sg = m["extensions"]["KHR_materials_pbrSpecularGlossiness"];
            if (sg.t == JV::Obj)
            {
                if (sg.has("diffuseTexture")) tex = sg["diffuseTexture"]["index"].i();
                if (sg.has("diffuseFactor"))  for (int k = 0; k < 4; ++k) gm.diffuse[k] = sg["diffuseFactor"][(size_t)k].f();
                if (sg.has("glossinessFactor")) gm.gloss = sg["glossinessFactor"].f();
            }
            else
            {
                const JV& pbr = m["pbrMetallicRoughness"];
                if (pbr.has("baseColorTexture")) tex = pbr["baseColorTexture"]["index"].i();
                if (pbr.has("baseColorFactor"))  for (int k = 0; k < 4; ++k) gm.diffuse[k] = pbr["baseColorFactor"][(size_t)k].f();
                if (pbr.has("roughnessFactor"))  gm.gloss = 1.0f - pbr["roughnessFactor"].f();
            }
            if (tex >= 0)
            {
                int src = g["textures"][(size_t)tex]["source"].i();
                const std::string& uri = g["images"][(size_t)src]["uri"].s;
                if (!loadPNG(base + uri, gm.w, gm.h, gm.rgba)) { gm.rgba.clear(); fprintf(stderr, "Textura no cargada: %s\n", uri.c_str()); }
            }
            mats.push_back(std::move(gm));
        }

        // nodos -> mallas
        float I[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        const JV& sc = g["scenes"][(size_t)g["scene"].i()];
        for (size_t i = 0; i < sc["nodes"].size(); ++i) visit(sc["nodes"][i].i(), I);
        if (prims.empty()) { err = "El archivo no tiene mallas."; return false; }
        return true;
    }

private:
    JV g; std::string base; std::vector<std::vector<uint8_t>> buffers;

    static bool readFile(const std::string& p, std::string& out)
    {
        FILE* f = fopen(p.c_str(), "rb"); if (!f) return false;
        char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
        fclose(f); return true;
    }
    // m = a * b (matrices 4x4 en orden columna, como OpenGL)
    static void mul(const float* a, const float* b, float* m)
    {
        for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
        { float s = 0; for (int k = 0; k < 4; ++k) s += a[k * 4 + r] * b[c * 4 + k]; m[c * 4 + r] = s; }
    }
    void localMatrix(const JV& n, float* m) const
    {
        if (n.has("matrix")) { for (int i = 0; i < 16; ++i) m[i] = n["matrix"][(size_t)i].f(); return; }
        float t[3] = { 0,0,0 }, q[4] = { 0,0,0,1 }, s[3] = { 1,1,1 };
        if (n.has("translation")) for (int i = 0; i < 3; ++i) t[i] = n["translation"][(size_t)i].f();
        if (n.has("rotation"))    for (int i = 0; i < 4; ++i) q[i] = n["rotation"][(size_t)i].f();
        if (n.has("scale"))       for (int i = 0; i < 3; ++i) s[i] = n["scale"][(size_t)i].f();
        float x = q[0], y = q[1], z = q[2], w = q[3];
        float R[9] = { 1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),
                       2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
                       2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y) };
        for (int c = 0; c < 3; ++c) { for (int r = 0; r < 3; ++r) m[c * 4 + r] = R[c * 3 + r] * s[c]; m[c * 4 + 3] = 0; }
        m[12] = t[0]; m[13] = t[1]; m[14] = t[2]; m[15] = 1;
    }
    // Lee un accessor completo como doubles ('comps' valores por elemento)
    bool readAccessor(int ai, std::vector<double>& out, int& comps) const
    {
        const JV& a = g["accessors"][(size_t)ai]; const JV& bv = g["bufferViews"][(size_t)a["bufferView"].i()];
        const std::string& ty = a["type"].s;
        comps = ty == "SCALAR" ? 1 : ty == "VEC2" ? 2 : ty == "VEC3" ? 3 : ty == "VEC4" ? 4 : 0;
        int ct = a["componentType"].i(); int csz = (ct == 5126 || ct == 5125) ? 4 : (ct == 5123 || ct == 5122) ? 2 : 1;
        if (!comps) return false;
        const std::vector<uint8_t>& buf = buffers[(size_t)bv["buffer"].i()];
        size_t off = (size_t)bv["byteOffset"].n + (size_t)a["byteOffset"].n;
        size_t stride = bv.has("byteStride") ? (size_t)bv["byteStride"].n : (size_t)(csz * comps);
        size_t count = (size_t)a["count"].n; out.resize(count * comps);
        for (size_t k = 0; k < count; ++k) for (int c = 0; c < comps; ++c)
        {
            size_t o = off + k * stride + (size_t)(c * csz); if (o + csz > buf.size()) return false;
            const uint8_t* p = &buf[o]; double v = 0;
            if (ct == 5126) { float f; memcpy(&f, p, 4); v = f; }
            else if (ct == 5125) { uint32_t u; memcpy(&u, p, 4); v = u; }
            else if (ct == 5123) { uint16_t u; memcpy(&u, p, 2); v = u; }
            else if (ct == 5122) { int16_t u; memcpy(&u, p, 2); v = u; }
            else if (ct == 5121) v = *p; else v = (int8_t)*p;
            out[k * comps + c] = v;
        }
        return true;
    }
    void visit(int ni, const float* parent)
    {
        const JV& n = g["nodes"][(size_t)ni];
        float loc[16], W[16]; localMatrix(n, loc); mul(parent, loc, W);
        if (n.has("mesh"))
        {
            const JV& mesh = g["meshes"][(size_t)n["mesh"].i()];
            for (size_t pi = 0; pi < mesh["primitives"].size(); ++pi)
            {
                const JV& pr = mesh["primitives"][pi];
                if (pr.has("mode") && pr["mode"].i() != 4) continue;           // solo triangulos
                GltfPrim gp; gp.mat = pr.has("material") ? pr["material"].i() : -1;
                std::vector<double> P, N, T; int c;
                if (!pr["attributes"].has("POSITION") || !readAccessor(pr["attributes"]["POSITION"].i(), P, c)) continue;
                size_t nv = P.size() / 3;
                bool hasN = pr["attributes"].has("NORMAL") && readAccessor(pr["attributes"]["NORMAL"].i(), N, c);
                bool hasT = pr["attributes"].has("TEXCOORD_0") && readAccessor(pr["attributes"]["TEXCOORD_0"].i(), T, c);
                for (size_t v = 0; v < nv; ++v)
                {
                    float x = (float)P[v * 3], y = (float)P[v * 3 + 1], z = (float)P[v * 3 + 2];
                    float px = W[0] * x + W[4] * y + W[8] * z + W[12], py = W[1] * x + W[5] * y + W[9] * z + W[13], pz = W[2] * x + W[6] * y + W[10] * z + W[14];
                    gp.pos.push_back(px); gp.pos.push_back(py); gp.pos.push_back(pz);
                    float bb[3] = { px, py, pz }; for (int k = 0; k < 3; ++k) { if (bb[k] < bmin[k]) bmin[k] = bb[k]; if (bb[k] > bmax[k]) bmax[k] = bb[k]; }
                    float nx = hasN ? (float)N[v * 3] : 0, ny = hasN ? (float)N[v * 3 + 1] : 1, nz = hasN ? (float)N[v * 3 + 2] : 0;
                    gp.nrm.push_back(W[0] * nx + W[4] * ny + W[8] * nz); gp.nrm.push_back(W[1] * nx + W[5] * ny + W[9] * nz); gp.nrm.push_back(W[2] * nx + W[6] * ny + W[10] * nz);
                    gp.uv.push_back(hasT ? (float)T[v * 2] : 0); gp.uv.push_back(hasT ? (float)T[v * 2 + 1] : 0);
                }
                if (pr.has("indices")) { std::vector<double> I; readAccessor(pr["indices"].i(), I, c); for (double d : I) gp.idx.push_back((unsigned)d); }
                else for (size_t v = 0; v < nv; ++v) gp.idx.push_back((unsigned)v);
                prims.push_back(std::move(gp));
            }
        }
        for (size_t i = 0; i < n["children"].size(); ++i) visit(n["children"][i].i(), W);
    }
};
