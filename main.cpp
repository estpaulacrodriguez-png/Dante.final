// ============================================================================
//  DanteViewer - Visor 3D de Dante (OpenGL + Win32)
//
//  Modelo 3D: "DMC 4 DANTE" por Tremolo_1404_ (CC-BY 4.0) - ver assets/dante/license.txt
// ============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "Gltf.h"

#ifdef USE_IMGUI                                  // se activa solo si existe la carpeta imgui/
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl2.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
#endif

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

static const float PI = 3.14159265358979f;

// ---------------------------------------------------------------- estado ---
static HWND  g_hwnd = nullptr;
static HDC   g_hdc  = nullptr;
static HGLRC g_rc   = nullptr;
static int   g_w = 1280, g_h = 720;

static float g_camYaw = 0.6f, g_camPitch = 0.35f, g_camDist = 5.5f;
static float g_camPan[3] = { 0, 0, 0 };           // traslacion extra de la camara
static int   g_lastMx = 0, g_lastMy = 0;

static float g_px = 0, g_pz = 0;      // posicion del personaje
static float g_yaw = 0;               // orientacion (grados)
static float g_userYaw = 0;           // giro extra del personaje controlado con Q / E (grados)
static float g_move = 0;              // 0 = quieto, 1 = corriendo
static const float g_run = 1.0f;      // el movimiento siempre es correr
static float g_phase = 0;             // fase del ciclo de carrera
static float g_speed = 4.0f;          // velocidad de carrera (m/s)

enum Mode { MODE_FREE = 0, MODE_BEZIER = 1 };
static int   g_mode = MODE_FREE;

static bool  g_lightMove = true;
static bool  g_wire = false;
static float g_lightAngle = 0;
static float g_time = 0;
static bool  g_showPath = true;
static bool  g_useModel = true;       // true: modelo GLTF, false: modelo basico de primitivas

static bool  g_shadow = false;        // (compatibilidad con col())
static GLUquadric* g_q = nullptr;

// ------------------------------------------------------------- utilidades ---
static void col(float r, float g, float b) { if (!g_shadow) glColor3f(r, g, b); }

static void setSpec(float s, float shin)
{
    GLfloat spec[4] = { s, s, s, 1 };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shin);
}

static void sphere(float r) { gluSphere(g_q, r, 24, 16); }

static void ellipsoid(float sx, float sy, float sz)
{
    glPushMatrix(); glScalef(sx, sy, sz); gluSphere(g_q, 1.0, 24, 16); glPopMatrix();
}

// Caja centrada en el origen de tamano (sx,sy,sz)
static void box(float sx, float sy, float sz)
{
    glPushMatrix();
    glScalef(sx, sy, sz);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);  glVertex3f(-.5f, -.5f, .5f);  glVertex3f(.5f, -.5f, .5f);  glVertex3f(.5f, .5f, .5f);  glVertex3f(-.5f, .5f, .5f);
    glNormal3f(0, 0, -1); glVertex3f(.5f, -.5f, -.5f);  glVertex3f(-.5f, -.5f, -.5f); glVertex3f(-.5f, .5f, -.5f); glVertex3f(.5f, .5f, -.5f);
    glNormal3f(1, 0, 0);  glVertex3f(.5f, -.5f, .5f);   glVertex3f(.5f, -.5f, -.5f);  glVertex3f(.5f, .5f, -.5f);  glVertex3f(.5f, .5f, .5f);
    glNormal3f(-1, 0, 0); glVertex3f(-.5f, -.5f, -.5f); glVertex3f(-.5f, -.5f, .5f);  glVertex3f(-.5f, .5f, .5f);  glVertex3f(-.5f, .5f, -.5f);
    glNormal3f(0, 1, 0);  glVertex3f(-.5f, .5f, .5f);   glVertex3f(.5f, .5f, .5f);    glVertex3f(.5f, .5f, -.5f);  glVertex3f(-.5f, .5f, -.5f);
    glNormal3f(0, -1, 0); glVertex3f(-.5f, -.5f, -.5f); glVertex3f(.5f, -.5f, -.5f);  glVertex3f(.5f, -.5f, .5f);  glVertex3f(-.5f, -.5f, .5f);
    glEnd();
    glPopMatrix();
}

// Cilindro/cono truncado que baja desde el origen hacia -Y
static void limb(float h, float rTop, float rBot)
{
    glPushMatrix();
    glRotatef(90, 1, 0, 0);
    gluCylinder(g_q, rTop, rBot, h, 18, 1);
    glPopMatrix();
}

// ----------------------------------------------------------- personaje ---
static void drawLeg(float s, float p, float m, float run)
{
    float ph   = p + (s > 0 ? 0.0f : PI);
    float amp  = 30.0f + 22.0f * run;
    float a    = -sinf(ph) * amp * m;
    float knee = (45.0f + 35.0f * run) * m * fmaxf(0.0f, cosf(ph)) + 2.0f;

    glPushMatrix();
    glTranslatef(s * 0.095f, -0.05f, 0);
    glRotatef(a, 1, 0, 0);

    setSpec(0.25f, 25);
    col(0.14f, 0.14f, 0.16f);                    // pantalon de cuero
    limb(0.45f, 0.088f, 0.072f);
    glTranslatef(0, -0.45f, 0);
    glRotatef(knee, 1, 0, 0);
    sphere(0.072f);
    limb(0.42f, 0.072f, 0.056f);

    // bota
    col(0.30f, 0.20f, 0.16f);
    glPushMatrix();
    glTranslatef(0, -0.12f, 0);
    limb(0.30f, 0.083f, 0.092f);
    col(0.20f, 0.13f, 0.10f);                    // correas
    for (int k = 0; k < 4; ++k)
    {
        glPushMatrix();
        glTranslatef(0, -0.04f - 0.06f * k, 0);
        limb(0.025f, 0.088f + 0.002f * k, 0.090f);
        glPopMatrix();
    }
    glPopMatrix();

    // pie
    col(0.28f, 0.19f, 0.15f);
    glPushMatrix();
    glTranslatef(0, -0.40f, 0.06f);
    box(0.12f, 0.075f, 0.27f);
    glPopMatrix();

    glPopMatrix();
}

static void drawArm(float s, float p, float m, float run, float t)
{
    float ph  = p + (s > 0 ? 0.0f : PI);
    float amp = 26.0f + 25.0f * run;
    float a   = sinf(ph) * amp * m;
    float idle = sinf(t * 1.8f) * 1.5f;

    glPushMatrix();
    glTranslatef(s * 0.215f, 0.48f, 0);
    glRotatef(s * (14.0f + idle), 0, 0, 1);
    glRotatef(a, 1, 0, 0);

    setSpec(0.35f, 30);
    col(0.46f, 0.12f, 0.17f);                    // manga de la gabardina
    sphere(0.065f);
    limb(0.28f, 0.065f, 0.058f);
    glTranslatef(0, -0.28f, 0);
    glRotatef(-(22.0f + 35.0f * m + 25.0f * run * m), 1, 0, 0);   // codo
    sphere(0.058f);
    limb(0.14f, 0.058f, 0.054f);                 // manga remangada

    glTranslatef(0, -0.16f, 0);
    setSpec(0.2f, 20);
    col(0.85f, 0.68f, 0.58f);                    // mano
    sphere(0.055f);
    glPopMatrix();
}

static void drawHead(float t, float m)
{
    glPushMatrix();
    glTranslatef(0, 0.74f, 0.01f);
    glRotatef(sinf(t * 0.7f) * 10.0f * (1 - m), 0, 1, 0);   // mira a los lados en idle
    setSpec(0.1f, 10);
    col(0.86f, 0.70f, 0.60f);                    // piel
    ellipsoid(0.092f, 0.115f, 0.10f);
    col(0.05f, 0.05f, 0.06f);                    // ojos
    for (int e = -1; e <= 1; e += 2)
    {
        glPushMatrix(); glTranslatef(e * 0.035f, 0.012f, 0.092f); sphere(0.011f); glPopMatrix();
    }
    glPopMatrix();
}

static void drawTorso(float p, float m, float t)
{
    float breath = sinf(t * 1.8f) * 0.006f;
    glPushMatrix();
    glRotatef(-sinf(p) * 6.0f * m, 0, 1, 0);     // giro del torso al caminar
    glScalef(1 + breath, 1 + breath, 1 + breath);

    // camiseta
    setSpec(0.05f, 8);
    col(0.30f, 0.35f, 0.44f);
    glPushMatrix(); glTranslatef(0, 0.27f, 0); box(0.31f, 0.52f, 0.17f); glPopMatrix();

    // chaqueta (version simple)
    setSpec(0.40f, 30);
    col(0.46f, 0.12f, 0.17f);
    glPushMatrix(); glTranslatef(0, 0.27f, -0.075f); box(0.37f, 0.54f, 0.06f); glPopMatrix();
    for (int e = -1; e <= 1; e += 2)
    {
        glPushMatrix(); glTranslatef(e * 0.165f, 0.27f, 0.0f); box(0.07f, 0.54f, 0.21f); glPopMatrix();
    }

    // cuello
    setSpec(0.1f, 10);
    col(0.86f, 0.70f, 0.60f);
    glPushMatrix(); glTranslatef(0, 0.64f, 0); limb(0.12f, 0.045f, 0.047f); glPopMatrix();

    drawHead(t, m);
    drawArm(+1, p, m, g_run, t);
    drawArm(-1, p, m, g_run, t);
    glPopMatrix();
}

static void drawCharacter(float t)
{
    float m = g_move, p = g_phase;
    float bob = fabsf(cosf(p)) * 0.035f * m;

    glPushMatrix();
    glTranslatef(g_px, 0.93f + bob, g_pz);
    glRotatef(g_yaw, 0, 1, 0);

    // pelvis
    setSpec(0.25f, 25);
    col(0.14f, 0.14f, 0.16f);
    glPushMatrix(); glTranslatef(0, -0.03f, 0); box(0.35f, 0.18f, 0.20f); glPopMatrix();

    drawLeg(+1, p, m, g_run);
    drawLeg(-1, p, m, g_run);

    drawTorso(p, m, t);
    glPopMatrix();
}

// --------------------------------------------------------------- escena ---
static void drawFloor()
{
    setSpec(0.08f, 6);
    int cx = (int)floorf(g_px), cz = (int)floorf(g_pz);
    const int R = 14;
    const float st = 0.5f;
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    for (int i = -R * 2; i < R * 2; ++i)
        for (int j = -R * 2; j < R * 2; ++j)
        {
            float x = cx + i * st, z = cz + j * st;
            int ix = (int)floorf(x), iz = (int)floorf(z);
            if (((ix + iz) & 1) == 0) glColor3f(0.30f, 0.30f, 0.34f);
            else                      glColor3f(0.17f, 0.17f, 0.20f);
            glVertex3f(x, 0, z + st); glVertex3f(x + st, 0, z + st);
            glVertex3f(x + st, 0, z); glVertex3f(x, 0, z);
        }
    glEnd();
}

// ============================================================== MATRICES ===
// Todas en orden columna (como OpenGL): m[columna*4 + fila].
static void matLookAt(float* m, float ex, float ey, float ez, float tx, float ty, float tz)
{
    float fx = tx - ex, fy = ty - ey, fz = tz - ez;                 // direccion de mirada
    float fl = sqrtf(fx * fx + fy * fy + fz * fz); fx /= fl; fy /= fl; fz /= fl;
    float sx = -fz, sy = 0, sz = fx;                                // lado = f x arriba(0,1,0)
    float sl = sqrtf(sx * sx + sz * sz); sx /= sl; sz /= sl;
    float ux = sy * fz - sz * fy, uy = sz * fx - sx * fz, uz = sx * fy - sy * fx;   // arriba = s x f
    // Rotacion (filas s, u, -f) por Traslacion(-ojo)
    m[0] = sx;  m[4] = sy;  m[8]  = sz;  m[12] = -(sx * ex + sy * ey + sz * ez);
    m[1] = ux;  m[5] = uy;  m[9]  = uz;  m[13] = -(ux * ex + uy * ey + uz * ez);
    m[2] = -fx; m[6] = -fy; m[10] = -fz; m[14] =  (fx * ex + fy * ey + fz * ez);
    m[3] = 0;   m[7] = 0;   m[11] = 0;   m[15] = 1;
}

static void matPerspective(float* m, float fovyDeg, float aspect, float n, float f)
{
    float t = 1.0f / tanf(fovyDeg * PI / 360.0f);
    for (int i = 0; i < 16; ++i) m[i] = 0;
    m[0] = t / aspect; m[5] = t; m[10] = (f + n) / (n - f); m[11] = -1; m[14] = 2 * f * n / (n - f);
}

// Matriz de modelo del personaje = Traslacion(x,0,z) * RotacionY(yaw)
static void matModel(float* m, float x, float z, float yawDeg)
{
    float c = cosf(yawDeg * PI / 180.0f), s = sinf(yawDeg * PI / 180.0f);
    for (int i = 0; i < 16; ++i) m[i] = 0;
    m[0] = c; m[2] = -s; m[5] = 1; m[8] = s; m[10] = c;
    m[12] = x; m[13] = 0; m[14] = z; m[15] = 1;
}

// ================================================================ BEZIER ===
// Curva cubica B(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t) t^2 P2 + t^3 P3   (plano XZ)
static float g_bz[4][2] = { { -8, 0 }, { -3, 6 }, { 3, -6 }, { 8, 0 } };   // A, control 1, control 2, B
static const int BZ_N = 200;
static float g_arc[BZ_N + 1];         // longitud acumulada (para velocidad constante)
static float g_pathLen = 0, g_pathS = 0;
static bool  g_bzRun = false, g_bzLoop = false, g_onPath = false;

static void bezierPoint(float t, float& x, float& z)
{
    float u = 1 - t, a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
    x = a * g_bz[0][0] + b * g_bz[1][0] + c * g_bz[2][0] + d * g_bz[3][0];
    z = a * g_bz[0][1] + b * g_bz[1][1] + c * g_bz[2][1] + d * g_bz[3][1];
}
static void bezierTangent(float t, float& x, float& z)
{
    float u = 1 - t, a = 3 * u * u, b = 6 * u * t, c = 3 * t * t;
    x = a * (g_bz[1][0] - g_bz[0][0]) + b * (g_bz[2][0] - g_bz[1][0]) + c * (g_bz[3][0] - g_bz[2][0]);
    z = a * (g_bz[1][1] - g_bz[0][1]) + b * (g_bz[2][1] - g_bz[1][1]) + c * (g_bz[3][1] - g_bz[2][1]);
}
static void buildArc()
{
    float px, pz; bezierPoint(0, px, pz); g_arc[0] = 0;
    for (int i = 1; i <= BZ_N; ++i)
    {
        float x, z; bezierPoint((float)i / BZ_N, x, z);
        g_arc[i] = g_arc[i - 1] + sqrtf((x - px) * (x - px) + (z - pz) * (z - pz)); px = x; pz = z;
    }
    g_pathLen = g_arc[BZ_N];
    if (g_pathS > g_pathLen) g_pathS = g_pathLen;
}
static float tFromS(float s)                       // distancia recorrida -> parametro t
{
    int lo = 0, hi = BZ_N;
    while (hi - lo > 1) { int mid = (lo + hi) / 2; if (g_arc[mid] <= s) lo = mid; else hi = mid; }
    float seg = g_arc[hi] - g_arc[lo];
    float f = seg > 1e-6f ? (s - g_arc[lo]) / seg : 0;
    return ((float)lo + f) / BZ_N;
}
static void resetBezier()
{
    g_mode = MODE_BEZIER; g_pathS = 0; g_bzRun = false; g_onPath = true;
    float tx, tz; bezierPoint(0, g_px, g_pz); bezierTangent(0, tx, tz);
    g_yaw = atan2f(tx, tz) * 180.0f / PI;
}
static void startBezier()
{
    if (!g_onPath || g_mode != MODE_BEZIER || g_pathS >= g_pathLen - 0.01f) resetBezier();
    g_bzRun = true;
}

// ============================================================ MODELO GLTF ===
static GltfModel g_model;
static bool g_modelOk = false;
static std::string g_loadErr;
static std::vector<std::vector<float>> g_posedPos, g_posedNrm;     // vertices ya posados

static bool loadModelFiles()
{
    char exe[MAX_PATH]; GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir(exe); dir = dir.substr(0, dir.find_last_of("\\/") + 1);
    const std::string rel = "assets/dante/scene.gltf";
    const std::string tries[] = { rel, "DanteViewer/" + rel, dir + rel, dir + "../" + rel, dir + "../../" + rel, dir + "../../../" + rel };
    for (const std::string& t : tries)
    {
        FILE* f = fopen(t.c_str(), "rb"); if (!f) continue; fclose(f);
        if (!g_model.load(t, g_loadErr)) return false;

        // Normalizar: centrar en X y apoyar los pies en y = 0
        float cx = 0.5f * (g_model.bmin[0] + g_model.bmax[0]), by = g_model.bmin[1];
        for (GltfPrim& p : g_model.prims)
            for (size_t i = 0; i < p.pos.size(); i += 3) { p.pos[i] -= cx; p.pos[i + 1] -= by; }

        // Subir texturas a la GPU
        for (GltfMat& m : g_model.mats)
        {
            if (m.rgba.empty()) continue;
            glGenTextures(1, &m.glTex);
            glBindTexture(GL_TEXTURE_2D, m.glTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, m.w, m.h, GL_RGBA, GL_UNSIGNED_BYTE, m.rgba.data());
            std::vector<uint8_t>().swap(m.rgba);
        }
        g_posedPos.resize(g_model.prims.size()); g_posedNrm.resize(g_model.prims.size());
        for (size_t i = 0; i < g_model.prims.size(); ++i) { g_posedPos[i] = g_model.prims[i].pos; g_posedNrm[i] = g_model.prims[i].nrm; }
        return true;
    }
    g_loadErr = "No se encontro assets/dante/scene.gltf";
    return false;
}

// ---- Pose procedural (el GLTF esta en pose de T y no trae esqueleto) --------
// Cada vertice se mueve segun su zona del cuerpo (brazos, piernas, cuerpo entero).
static inline float sstep(float a, float b, float x)
{
    float t = (x - a) / (b - a); t = t < 0 ? 0 : (t > 1 ? 1 : t); return t * t * (3 - 2 * t);
}
static const float SH_X = 0.23f, SH_Y = 1.60f;      // hombro
static const float HIP_X = 0.10f, HIP_Y = 0.92f;    // cadera
static const float KNEE_Y = 0.50f, ELBOW = 0.36f;   // rodilla y codo
static inline bool isCoat(size_t prim) { return prim == 0 || prim == 1 || prim == 7; }   // mallas de la gabardina

struct V3 { float x, y, z; };
static inline V3 rotX(V3 v, V3 piv, float c, float s)       // rota alrededor del eje X en 'piv'
{
    float dy = v.y - piv.y, dz = v.z - piv.z;
    return { v.x, piv.y + dy * c - dz * s, piv.z + dy * s + dz * c };
}
static inline V3 rotXn(V3 n, float c, float s) { return { n.x, n.y * c - n.z * s, n.y * s + n.z * c }; }
static inline V3 mix(V3 a, V3 b, float t) { return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t }; }

static void poseModel()
{
    const float m = g_move, ph = g_phase;
    // parametros por lado: 0 = +x, 1 = -x
    float armC[2], armS[2], hipC[2], hipS[2], kneeC[2], kneeS[2]; V3 kneePiv[2];
    for (int i = 0; i < 2; ++i)
    {
        float sd = i == 0 ? 1.0f : -1.0f, p = ph + (i == 0 ? 0.0f : PI);
        float sw = -(35.0f * PI / 180.0f) * m * sinf(p);          // balanceo del brazo
        float hp = -(48.0f * PI / 180.0f) * m * sinf(p);          // balanceo de la pierna
        float kn = (85.0f * PI / 180.0f) * m * fmaxf(0.0f, cosf(p));   // flexion de rodilla
        armC[i] = cosf(sw); armS[i] = sinf(sw); hipC[i] = cosf(hp); hipS[i] = sinf(hp); kneeC[i] = cosf(kn); kneeS[i] = sinf(kn);
        kneePiv[i] = rotX({ sd * HIP_X, KNEE_Y, 0 }, { sd * HIP_X, HIP_Y, 0 }, hipC[i], hipS[i]);
    }
    const float th = 68.0f * PI / 180.0f, cth = cosf(th), sth = sinf(th);   // brazos bajados respecto a la T
    const float beta = 60.0f * PI / 180.0f * m;                              // flexion del codo
    const float lean = 10.0f * PI / 180.0f * m, lc = cosf(lean), ls = sinf(lean);
    const float bob = fabsf(cosf(ph)) * 0.04f * m;

    for (size_t k = 0; k < g_model.prims.size(); ++k)
    {
        const GltfPrim& pr = g_model.prims[k]; const bool coat = isCoat(k);
        float* op = g_posedPos[k].data(); float* on = g_posedNrm[k].data();
        for (size_t i = 0, n = pr.pos.size() / 3; i < n; ++i)
        {
            V3 p = { pr.pos[i * 3], pr.pos[i * 3 + 1], pr.pos[i * 3 + 2] };
            V3 nr = { pr.nrm[i * 3], pr.nrm[i * 3 + 1], pr.nrm[i * 3 + 2] };
            const V3 p0 = p; V3 q = p, qn = nr;

            // ---- brazos
            float wA = sstep(0.15f, 0.30f, fabsf(p0.x)) * sstep(1.38f, 1.50f, p0.y);
            if (wA > 0 && p0.x != 0)
            {
                int si = p0.x > 0 ? 0 : 1; float s = p0.x > 0 ? 1.0f : -1.0f;
                float dy = p0.y - SH_Y, vx = s * (p0.x - s * SH_X), vz = p0.z;
                float vx2 = vx, vz2 = vz;
                if (vx > ELBOW) { float b = beta * sstep(ELBOW - 0.05f, ELBOW + 0.05f, vx); vx2 = ELBOW + (vx - ELBOW) * cosf(b); vz2 = vz + (vx - ELBOW) * sinf(b); }
                float ax = vx2 * cth + dy * sth, ay = -vx2 * sth + dy * cth, az = vz2;
                V3 r = { ax, ay * armC[si] - az * armS[si], ay * armS[si] + az * armC[si] };
                V3 np = { s * SH_X + s * r.x, SH_Y + r.y, r.z };
                q = mix(p0, np, wA);
                float nx = s * nr.x, ax_ = nx * cth + nr.y * sth, ay_ = -nx * sth + nr.y * cth;
                V3 nn = { s * ax_, ay_ * armC[si] - nr.z * armS[si], ay_ * armS[si] + nr.z * armC[si] };
                qn = mix(nr, nn, wA);
            }

            // ---- piernas (cada lado se mezcla segun la posicion en X)
            float wL = sstep(HIP_Y, HIP_Y - 0.25f, p0.y) * (coat ? 0.40f : 1.0f);
            if (wL > 0)
            {
                float t = q.x / 0.14f * 0.5f + 0.5f; t = t < 0 ? 0 : (t > 1 ? 1 : t);   // 1 = pierna +x
                float wk = sstep(KNEE_Y + 0.08f, KNEE_Y - 0.08f, p0.y) * (coat ? 0.15f : 1.0f);
                V3 legP[2], legN[2];
                for (int sdI = 0; sdI < 2; ++sdI)
                {
                    float sd = sdI == 0 ? 1.0f : -1.0f;
                    V3 v1 = rotX(q, { sd * HIP_X, HIP_Y, 0 }, hipC[sdI], hipS[sdI]);
                    V3 v2 = rotX(v1, kneePiv[sdI], kneeC[sdI], kneeS[sdI]);
                    legP[sdI] = mix(v1, v2, wk);
                    V3 n1 = rotXn(qn, hipC[sdI], hipS[sdI]), n2 = rotXn(n1, kneeC[sdI], kneeS[sdI]);
                    legN[sdI] = mix(n1, n2, wk);
                }
                V3 blendP = mix(legP[1], legP[0], t), blendN = mix(legN[1], legN[0], t);
                q = mix(q, blendP, wL); qn = mix(qn, blendN, wL);
            }

            // ---- cuerpo completo: rebote vertical + inclinacion hacia delante
            q.y += bob;
            V3 fp = { q.x, q.y * lc - q.z * ls, q.y * ls + q.z * lc };
            V3 fn = rotXn(qn, lc, ls);
            op[i * 3] = fp.x; op[i * 3 + 1] = fp.y; op[i * 3 + 2] = fp.z;
            on[i * 3] = fn.x; on[i * 3 + 1] = fn.y; on[i * 3 + 2] = fn.z;
        }
    }
}

static void drawModel()
{
    poseModel();
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_NORMAL_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    for (size_t i = 0; i < g_model.prims.size(); ++i)
    {
        const GltfPrim& pr = g_model.prims[i];
        const GltfMat* mt = pr.mat >= 0 ? &g_model.mats[(size_t)pr.mat] : nullptr;
        if (mt && mt->glTex) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, mt->glTex); glColor3f(1, 1, 1); }
        else { glDisable(GL_TEXTURE_2D); if (mt) glColor3f(mt->diffuse[0], mt->diffuse[1], mt->diffuse[2]); else glColor3f(0.7f, 0.7f, 0.7f); }
        setSpec(0.25f, 8.0f + (mt ? mt->gloss : 0.4f) * 50.0f);
        glVertexPointer(3, GL_FLOAT, 0, g_posedPos[i].data());
        glNormalPointer(GL_FLOAT, 0, g_posedNrm[i].data());
        glTexCoordPointer(2, GL_FLOAT, 0, pr.uv.data());
        glDrawElements(GL_TRIANGLES, (GLsizei)pr.idx.size(), GL_UNSIGNED_INT, pr.idx.data());
    }
    glDisableClientState(GL_VERTEX_ARRAY); glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_TEXTURE_2D);
}

static void drawBezierPath()
{
    glDisable(GL_LIGHTING);
    glLineWidth(3.0f);
    glColor3f(1.0f, 0.75f, 0.2f);                       // la curva
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 100; ++i) { float x, z; bezierPoint(i / 100.0f, x, z); glVertex3f(x, 0.03f, z); }
    glEnd();
    glLineWidth(1.0f);
    glColor3f(0.4f, 0.4f, 0.5f);                        // poligono de control
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < 4; ++i) glVertex3f(g_bz[i][0], 0.02f, g_bz[i][1]);
    glEnd();
    for (int i = 0; i < 4; ++i)                         // A (verde), B (rojo), controles (cian)
    {
        if (i == 0) glColor3f(0.2f, 0.9f, 0.3f); else if (i == 3) glColor3f(0.95f, 0.25f, 0.25f); else glColor3f(0.3f, 0.8f, 0.95f);
        glPushMatrix(); glTranslatef(g_bz[i][0], 0.12f, g_bz[i][1]); sphere(i == 0 || i == 3 ? 0.16f : 0.09f); glPopMatrix();
    }
    glEnable(GL_LIGHTING);
}

// ================================================================== UI ===
#ifdef USE_IMGUI
static void drawUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(330, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("DanteViewer");
    ImGui::Text("Animacion: %s", g_move > 0.5f ? "correr" : "quieto");
    ImGui::Separator();
    if (ImGui::RadioButton("Libre (WASD)", g_mode == MODE_FREE)) { g_mode = MODE_FREE; g_bzRun = false; g_onPath = false; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Curva Bezier", g_mode == MODE_BEZIER)) { g_mode = MODE_BEZIER; }
    if (ImGui::Button("Correr de A a B")) startBezier();
    ImGui::SameLine(); if (ImGui::Button("Volver a A")) resetBezier();
    ImGui::Checkbox("Repetir", &g_bzLoop);
    ImGui::SliderFloat("Velocidad", &g_speed, 1.0f, 8.0f);
    if (ImGui::CollapsingHeader("Puntos de la curva Bezier"))
    {
        bool ch = false;
        ch |= ImGui::DragFloat2("A (inicio)", g_bz[0], 0.1f);
        ch |= ImGui::DragFloat2("Control 1", g_bz[1], 0.1f);
        ch |= ImGui::DragFloat2("Control 2", g_bz[2], 0.1f);
        ch |= ImGui::DragFloat2("B (final)", g_bz[3], 0.1f);
        if (ch) buildArc();
        ImGui::Text("Longitud de la curva: %.1f m", g_pathLen);
    }
    if (ImGui::CollapsingHeader("Camara"))
    {
        ImGui::SliderFloat("Distancia", &g_camDist, 2.0f, 14.0f);
        ImGui::SliderFloat("Giro", &g_camYaw, -PI, PI);
        ImGui::SliderFloat("Elevacion", &g_camPitch, -0.1f, 1.4f);
        ImGui::SliderFloat3("Traslacion", g_camPan, -3.0f, 3.0f);
    }
    ImGui::SliderFloat("Giro del personaje", &g_userYaw, -180.0f, 180.0f);
    ImGui::TextWrapped("Teclado: WASD correr, B curva, R volver a A, flechas mover camara, Q/E girar personaje.");
    ImGui::Checkbox("Luz orbital", &g_lightMove);
    ImGui::Checkbox("Alambre", &g_wire);
    ImGui::Checkbox("Mostrar curva", &g_showPath);
    ImGui::BeginDisabled(!g_modelOk);
    ImGui::Checkbox("Modelo GLTF (apagado = basico)", &g_useModel);
    ImGui::EndDisabled();
    if (!g_modelOk) ImGui::TextWrapped("GLTF no cargado: %s", g_loadErr.c_str());
    ImGui::Separator();
    ImGui::TextWrapped("Modelo: DMC 4 DANTE por Tremolo_1404_ (CC-BY 4.0)");
    ImGui::Text("%.0f FPS", ImGui::GetIO().Framerate);
    ImGui::End();
}
static bool uiWantsMouse() { return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse; }
static bool uiWantsKeys()  { return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard; }
#else
static bool uiWantsMouse() { return false; }
static bool uiWantsKeys()  { return false; }
#endif

// ============================================================ ACTUALIZAR ===
static void update(float dt)
{
    g_time += dt;
    if (g_lightMove) g_lightAngle += dt * 0.8f;

    const bool keys = !uiWantsKeys();
    // flechas / RePag / AvPag: trasladan la camara
    if (keys)
    {
        float cs = 2.5f * dt;
        if (GetAsyncKeyState(VK_LEFT)  & 0x8000) { g_camPan[0] -= cosf(g_camYaw) * cs; g_camPan[2] += sinf(g_camYaw) * cs; }
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { g_camPan[0] += cosf(g_camYaw) * cs; g_camPan[2] -= sinf(g_camYaw) * cs; }
        if (GetAsyncKeyState(VK_UP)    & 0x8000) { g_camPan[0] -= sinf(g_camYaw) * cs; g_camPan[2] -= cosf(g_camYaw) * cs; }
        if (GetAsyncKeyState(VK_DOWN)  & 0x8000) { g_camPan[0] += sinf(g_camYaw) * cs; g_camPan[2] += cosf(g_camYaw) * cs; }
        if (GetAsyncKeyState(VK_PRIOR) & 0x8000) g_camPan[1] += cs;
        if (GetAsyncKeyState(VK_NEXT)  & 0x8000) g_camPan[1] -= cs;
        // Q / E: rotan el personaje (rotacion de objetos)
        if (GetAsyncKeyState('Q') & 0x8000) g_userYaw -= 90.0f * dt;
        if (GetAsyncKeyState('E') & 0x8000) g_userYaw += 90.0f * dt;
    }

    float fx = -sinf(g_camYaw), fz = -cosf(g_camYaw);
    float rx = cosf(g_camYaw),  rz = -sinf(g_camYaw);
    float f = 0, r = 0;
    if (keys)
    {
        if (GetAsyncKeyState('W') & 0x8000) f += 1;
        if (GetAsyncKeyState('S') & 0x8000) f -= 1;
        if (GetAsyncKeyState('D') & 0x8000) r += 1;
        if (GetAsyncKeyState('A') & 0x8000) r -= 1;
    }
    float dx = fx * f + rx * r, dz = fz * f + rz * r;
    float len = sqrtf(dx * dx + dz * dz);
    if (len > 0.001f && g_mode == MODE_BEZIER) { g_mode = MODE_FREE; g_bzRun = false; g_onPath = false; }

    bool moving = false; float targetYaw = g_yaw;
    if (g_mode == MODE_BEZIER)
    {
        if (g_bzRun)
        {
            g_pathS += g_speed * dt;
            if (g_pathS >= g_pathLen) { if (g_bzLoop) g_pathS = 0; else { g_pathS = g_pathLen; g_bzRun = false; } }
        }
        if (g_bzRun)                                   // sigue la curva a velocidad constante
        {
            float t = tFromS(g_pathS), tx, tz;
            bezierPoint(t, g_px, g_pz); bezierTangent(t, tx, tz);
            targetYaw = atan2f(tx, tz) * 180.0f / PI;
            moving = true;
        }
    }
    else if (len > 0.001f)                             // modo libre con WASD
    {
        dx /= len; dz /= len;
        g_px += dx * g_speed * dt; g_pz += dz * g_speed * dt;
        targetYaw = atan2f(dx, dz) * 180.0f / PI;
        moving = true;
    }

    float diff = fmodf(targetYaw - g_yaw + 540.0f, 360.0f) - 180.0f;
    g_yaw += diff * fminf(1.0f, dt * 10.0f);

    float k = fminf(1.0f, dt * 8.0f);
    g_move += ((moving ? 1.0f : 0.0f) - g_move) * k;   // mezcla suave quieto <-> correr
    if (moving) g_phase += dt * g_speed * 3.6f;
}

// ================================================================ RENDER ===
static void render()
{
    glViewport(0, 0, g_w, g_h);
    glClearColor(0.05f, 0.05f, 0.07f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Proyeccion y vista construidas con nuestras propias matrices
    float P[16], V[16];
    matPerspective(P, 45.0f, (float)g_w / (g_h > 0 ? (float)g_h : 1.0f), 0.1f, 100.0f);
    float tx = g_px + g_camPan[0], ty = 1.0f + g_camPan[1], tz = g_pz + g_camPan[2];
    float ex = tx + g_camDist * cosf(g_camPitch) * sinf(g_camYaw);
    float ey = ty + g_camDist * sinf(g_camPitch);
    float ez = tz + g_camDist * cosf(g_camPitch) * cosf(g_camYaw);
    matLookAt(V, ex, ey, ez, tx, ty, tz);
    glMatrixMode(GL_PROJECTION); glLoadMatrixf(P);
    glMatrixMode(GL_MODELVIEW);  glLoadMatrixf(V);

    glPolygonMode(GL_FRONT_AND_BACK, g_wire ? GL_LINE : GL_FILL);

    // luces (modelo de Phong: ambiente + difusa + especular)
    float lx = g_px + 3.2f * cosf(g_lightAngle);
    float ly = 2.8f + 0.5f * sinf(g_lightAngle * 1.7f);
    float lz = g_pz + 3.2f * sinf(g_lightAngle);
    GLfloat l0pos[4] = { lx, ly, lz, 1 };
    GLfloat l0dif[4] = { 1.0f, 0.88f, 0.75f, 1 };
    GLfloat l0amb[4] = { 0.05f, 0.05f, 0.05f, 1 };
    GLfloat l0spc[4] = { 1, 1, 1, 1 };
    glLightfv(GL_LIGHT0, GL_POSITION, l0pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, l0dif);
    glLightfv(GL_LIGHT0, GL_AMBIENT, l0amb);
    glLightfv(GL_LIGHT0, GL_SPECULAR, l0spc);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.03f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.01f);

    GLfloat l1pos[4] = { -0.5f, 0.6f, -0.7f, 0 };   // luz de relleno azulada direccional
    GLfloat l1dif[4] = { 0.18f, 0.24f, 0.45f, 1 };
    GLfloat l1spc[4] = { 0.2f, 0.25f, 0.4f, 1 };
    glLightfv(GL_LIGHT1, GL_POSITION, l1pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, l1dif);
    glLightfv(GL_LIGHT1, GL_SPECULAR, l1spc);

    GLfloat amb[4] = { 0.20f, 0.20f, 0.24f, 1 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);

    drawFloor();
    if (g_showPath) drawBezierPath();

    // personaje: matriz de modelo = Traslacion * RotacionY
    float Mdl[16]; matModel(Mdl, g_px, g_pz, g_yaw + g_userYaw);
    if (g_modelOk && g_useModel)
    {
        glPushMatrix(); glMultMatrixf(Mdl);
        drawModel();
        glPopMatrix();
    }
    else drawCharacter(g_time);

#ifdef USE_IMGUI
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    drawUI();
    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
#endif
    SwapBuffers(g_hdc);
}

// --------------------------------------------------------------- Win32 ---
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
#ifdef USE_IMGUI
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
#endif
    switch (msg)
    {
    case WM_SIZE:
        g_w = LOWORD(lp); g_h = HIWORD(lp);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { PostQuitMessage(0); return 0; }
        if (!(lp & (1 << 30)) && !uiWantsKeys())
        {
            if (wp == 'L') g_lightMove = !g_lightMove;
            if (wp == 'F') g_wire = !g_wire;
            if (wp == 'M' && g_modelOk) g_useModel = !g_useModel;
            if (wp == 'B') startBezier();                       // correr de A a B
            if (wp == 'R') { resetBezier(); g_userYaw = 0; }    // volver a A
            if (wp == '1') { g_mode = MODE_FREE; g_bzRun = false; g_onPath = false; }
            if (wp == '2') g_mode = MODE_BEZIER;
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (uiWantsMouse()) return 0;
        SetCapture(hwnd);
        g_lastMx = GET_X_LPARAM(lp); g_lastMy = GET_Y_LPARAM(lp);
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE:
        if ((wp & MK_LBUTTON) && !uiWantsMouse())
        {
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
            g_camYaw   -= (mx - g_lastMx) * 0.008f;
            g_camPitch += (my - g_lastMy) * 0.008f;
            if (g_camPitch < -0.1f) g_camPitch = -0.1f;
            if (g_camPitch > 1.4f)  g_camPitch = 1.4f;
            g_lastMx = mx; g_lastMy = my;
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (uiWantsMouse()) return 0;
        g_camDist -= GET_WHEEL_DELTA_WPARAM(wp) / 120.0f * 0.5f;
        if (g_camDist < 2.0f)  g_camDist = 2.0f;
        if (g_camDist > 14.0f) g_camDist = 14.0f;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow)
{
    WNDCLASSA wc = {};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "DanteViewerWnd";
    RegisterClassA(&wc);

    RECT rc = { 0, 0, g_w, g_h };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowExA(0, wc.lpszClassName, "DanteViewer - OpenGL",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
    g_hdc = GetDC(g_hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    int pf = ChoosePixelFormat(g_hdc, &pfd);
    if (!pf || !SetPixelFormat(g_hdc, pf, &pfd))
    {
        MessageBoxA(nullptr, "No se pudo crear el formato de pixel.", "Error", MB_ICONERROR);
        return 1;
    }
    g_rc = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, g_rc);

    g_q = gluNewQuadric();
    gluQuadricNormals(g_q, GLU_SMOOTH);

    buildArc();
    SetWindowTextA(g_hwnd, "DanteViewer - cargando modelo GLTF...");
    g_modelOk = loadModelFiles();                      // GLTF + texturas (puede tardar unos segundos)
    if (!g_modelOk) MessageBoxA(g_hwnd, (g_loadErr + "\nSe usara el modelo basico.").c_str(), "DanteViewer", MB_ICONWARNING);

#ifdef USE_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplOpenGL2_Init();
#endif

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    float fpsAcc = 0; int fpsCnt = 0;

    MSG msg;
    bool running = true;
    while (running)
    {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        update(dt);
        render();

        fpsAcc += dt; fpsCnt++;
        if (fpsAcc >= 0.5f)
        {
            char title[160];
            sprintf_s(title, "DanteViewer | %s | %s | WASD, B, R, 1/2, M, L, F | %.0f FPS",
                      g_mode == MODE_BEZIER ? "Bezier" : "Libre", g_move > 0.5f ? "correr" : "quieto", fpsCnt / fpsAcc);
            SetWindowTextA(g_hwnd, title);
            fpsAcc = 0; fpsCnt = 0;
        }
    }

#ifdef USE_IMGUI
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif
    gluDeleteQuadric(g_q);
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(g_rc);
    ReleaseDC(g_hwnd, g_hdc);
    return 0;
}
