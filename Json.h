// Json.h - Lector JSON minimo (suficiente para archivos .gltf). Sin dependencias.
#pragma once
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>

struct JV
{
    enum Type { Null, Bool, Num, Str, Arr, Obj } t = Null;
    double n = 0; bool b = false; std::string s;
    std::vector<JV> a;                       // elementos (arreglo) o valores (objeto)
    std::vector<std::string> keys;           // claves (objeto)

    static const JV& none() { static JV j; return j; }
    bool has(const char* k) const { for (auto& x : keys) if (x == k) return true; return false; }
    const JV& operator[](const char* k) const { for (size_t i = 0; i < keys.size(); ++i) if (keys[i] == k) return a[i]; return none(); }
    const JV& operator[](size_t i) const { return i < a.size() ? a[i] : none(); }
    size_t size() const { return a.size(); }
    int    i() const { return (int)n; }
    float  f() const { return (float)n; }
};

class JsonParser
{
    const char* p; const char* e; bool ok = true;
    void ws() { while (p < e && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; }
    std::string str()
    {
        std::string r; ++p;                  // salta la comilla inicial
        while (p < e && *p != '"')
        {
            if (*p == '\\' && p + 1 < e)
            {
                ++p;
                switch (*p) { case 'n': r += '\n'; break; case 't': r += '\t'; break; case 'r': r += '\r'; break;
                              case 'u': r += '?'; p += 4; break; default: r += *p; }
                ++p;
            }
            else r += *p++;
        }
        ++p; return r;
    }
    JV val()
    {
        JV v; ws();
        if (p >= e) { ok = false; return v; }
        if (*p == '{')
        {
            v.t = JV::Obj; ++p; ws();
            while (p < e && *p != '}')
            {
                ws(); std::string k = str(); ws();
                if (*p != ':') { ok = false; return v; }
                ++p; v.keys.push_back(k); v.a.push_back(val()); ws();
                if (*p == ',') ++p;
                ws();
            }
            ++p;
        }
        else if (*p == '[')
        {
            v.t = JV::Arr; ++p; ws();
            while (p < e && *p != ']') { v.a.push_back(val()); ws(); if (*p == ',') ++p;
                ws(); }
            ++p;
        }
        else if (*p == '"') { v.t = JV::Str; v.s = str(); }
        else if (!strncmp(p, "true", 4))  { v.t = JV::Bool; v.b = true;  p += 4; }
        else if (!strncmp(p, "false", 5)) { v.t = JV::Bool; v.b = false; p += 5; }
        else if (!strncmp(p, "null", 4))  { p += 4; }
        else { char* q; v.t = JV::Num; v.n = strtod(p, &q); if (q == p) ok = false; p = q; }
        return v;
    }
public:
    static bool parse(const std::string& text, JV& out)
    {
        JsonParser jp; jp.p = text.c_str(); jp.e = jp.p + text.size();
        out = jp.val(); return jp.ok;
    }
};
