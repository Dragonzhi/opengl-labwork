#include "corona.h"
#include "../gfx/texture.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const float kPi = 3.1415926f;

// 配置文件的候选路径：从仓库根目录跑、从 build 目录跑、或直接双击都能找到
const char* const kConfigPaths[] = {
    "corona.cfg",
    "../corona.cfg",
    "project/corona.cfg"
};
const int kConfigPathCount = 3;

float clampF(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// 原地去首尾空白；返回去掉前导空白后的起点
char* trim(char* s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
    char* end = s + std::strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\r' || end[-1] == '\n')) {
        --end;
    }
    *end = '\0';
    return s;
}

bool parseFloat(const char* text, float& out)
{
    char* end = 0;
    const double value = std::strtod(text, &end);
    if (end == text) return false;   // 一个数都没读出来
    out = static_cast<float>(value);
    return true;
}

}  // namespace

SunCorona::SunCorona()
    : flameTexture(0), wispTexture(0), glowTexture(0), time(0.0f),
      reloadCount(0), appliedCount(0)
{
    statusText[0] = '\0';
}

void SunCorona::init()
{
    reload();
}

bool SunCorona::reload()
{
    // 每次先回到内置默认值：cfg 里删掉的行才会真的恢复默认，而不是沿用上次的值
    p = CoronaParams();
    appliedCount = 0;

    bool found = false;
    for (int i = 0; i < kConfigPathCount && !found; ++i) {
        found = loadParams(kConfigPaths[i]);
    }

    ++reloadCount;
    rebuildTextures();
    updateStatus(found);
    return found;
}

bool SunCorona::loadParams(const char* path)
{
    std::FILE* fp = std::fopen(path, "r");
    if (fp == 0) return false;

    char line[256];
    while (std::fgets(line, sizeof(line), fp) != 0) {
        // 支持行尾注释：'#' 之后整段丢掉
        char* hash = std::strchr(line, '#');
        if (hash != 0) *hash = '\0';

        char* equals = std::strchr(line, '=');
        if (equals == 0) continue;      // 空行或不是 key = value 的行
        *equals = '\0';

        char* key = trim(line);
        char* value = trim(equals + 1);

        float number = 0.0f;
        if (!parseFloat(value, number)) continue;
        if (applyParam(key, number)) {
            ++appliedCount;
        }
    }

    std::fclose(fp);
    std::printf("CORONA: 读取 %s，应用 %d 项参数\n", path, appliedCount);
    return true;
}

// 认得的键才生效，未知键直接忽略；所有值都钳到一个安全范围，
// 免得改出一个负的半径或者巨大的透明度把画面弄炸。
bool SunCorona::applyParam(const char* key, float v)
{
    if (std::strcmp(key, "flameScale") == 0)  { p.flameScale = clampF(v, 0.9f, 4.0f); return true; }
    if (std::strcmp(key, "flameAlpha") == 0)  { p.flameAlpha = clampF(v, 0.0f, 2.0f); return true; }
    if (std::strcmp(key, "flameSpin") == 0)   { p.flameSpin  = clampF(v, -30.0f, 30.0f); return true; }

    if (std::strcmp(key, "wispScale") == 0)   { p.wispScale = clampF(v, 0.9f, 5.0f); return true; }
    if (std::strcmp(key, "wispAlpha") == 0)   { p.wispAlpha = clampF(v, 0.0f, 1.0f); return true; }
    if (std::strcmp(key, "wispSpin") == 0)    { p.wispSpin  = clampF(v, -30.0f, 30.0f); return true; }

    if (std::strcmp(key, "haloScale") == 0)   { p.haloScale = clampF(v, 0.9f, 8.0f); return true; }
    if (std::strcmp(key, "haloAlpha") == 0)   { p.haloAlpha = clampF(v, 0.0f, 1.5f); return true; }

    if (std::strcmp(key, "pulse") == 0)       { p.pulse     = clampF(v, 0.0f, 0.5f); return true; }
    if (std::strcmp(key, "flicker") == 0)     { p.flicker   = clampF(v, 0.0f, 0.5f); return true; }
    if (std::strcmp(key, "haloPulse") == 0)   { p.haloPulse = clampF(v, 0.0f, 0.5f); return true; }

    if (std::strcmp(key, "rimWeight") == 0)    { p.shape.rimWeight    = clampF(v, 0.0f, 2.5f); return true; }
    if (std::strcmp(key, "tongueWeight") == 0) { p.shape.tongueWeight = clampF(v, 0.0f, 4.0f); return true; }
    if (std::strcmp(key, "stripePower") == 0)  { p.shape.stripePower  = clampF(v, 1.0f, 10.0f); return true; }
    if (std::strcmp(key, "flameFreq") == 0)    { p.shape.flameFreq    = clampF(v, 4.0f, 64.0f); return true; }
    if (std::strcmp(key, "reachBase") == 0)    { p.shape.reachBase    = clampF(v, 0.82f, 0.95f); return true; }
    if (std::strcmp(key, "reachRange") == 0)   { p.shape.reachRange   = clampF(v, 0.0f, 0.15f); return true; }
    if (std::strcmp(key, "glowFalloff") == 0)  { p.shape.glowFalloff  = clampF(v, 0.5f, 8.0f); return true; }

    return false;
}

void SunCorona::rebuildTextures()
{
    releaseTexture(flameTexture);
    releaseTexture(wispTexture);
    releaseTexture(glowTexture);

    // 主火舌带白热亮环；外层流焰把亮环权重清零，只留淡火舌，免得出现第二道亮边
    CoronaShape wispShape = p.shape;
    wispShape.rimWeight = 0.0f;

    flameTexture = createProceduralTexture(TEX_CORONA, 31, 512, 512, p.shape);
    wispTexture  = createProceduralTexture(TEX_CORONA, 37, 256, 256, wispShape);
    glowTexture  = createProceduralTexture(TEX_GLOW,   32, 256, 256, p.shape);
}

void SunCorona::updateStatus(bool foundFile)
{
    // HUD 用 glutBitmapCharacter 画，只能是 ASCII
    if (foundFile) {
        std::snprintf(statusText, sizeof(statusText),
                      "corona.cfg #%d: %d params", reloadCount, appliedCount);
    } else {
        std::snprintf(statusText, sizeof(statusText),
                      "corona.cfg missing: built-in defaults");
    }
}

void SunCorona::update(float deltaTime)
{
    time += deltaTime;
    if (time > 3600.0f) time -= 3600.0f;   // 防止长时间运行后浮点精度下降
}

void SunCorona::drawLayer(const float* right, const float* up,
                          float cx, float cy, float cz,
                          float halfSize, float rollDeg,
                          float alpha, GLuint texture) const
{
    if (texture == 0 || alpha <= 0.0f) return;

    // 绕视线轴自转：在 right / up 这组基里做二维旋转，等价于旋转贴图本身
    const float roll = rollDeg * kPi / 180.0f;
    const float c = std::cos(roll);
    const float s = std::sin(roll);

    // 顺手把基向量放大到 halfSize，四个顶点就不用再逐个乘
    const float rx = (right[0] * c + up[0] * s) * halfSize;
    const float ry = (right[1] * c + up[1] * s) * halfSize;
    const float rz = (right[2] * c + up[2] * s) * halfSize;

    const float ux = (up[0] * c - right[0] * s) * halfSize;
    const float uy = (up[1] * c - right[1] * s) * halfSize;
    const float uz = (up[2] * c - right[2] * s) * halfSize;

    glBindTexture(GL_TEXTURE_2D, texture);
    glColor4f(1.0f, 1.0f, 1.0f, alpha);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(cx - rx - ux, cy - ry - uy, cz - rz - uz);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(cx + rx - ux, cy + ry - uy, cz + rz - uz);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(cx + rx + ux, cy + ry + uy, cz + rz + uz);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(cx - rx + ux, cy - ry + uy, cz - rz + uz);
    glEnd();
}

void SunCorona::draw(float cx, float cy, float cz, float sunRadius) const
{
    if (flameTexture == 0 && glowTexture == 0) return;

    // 视图矩阵是 V = [R | t]，R 的三行分别是相机在世界空间里的右、上、后轴。
    // OpenGL 用列主序存放：M[row][col] 在 m[col*4 + row]，
    // 所以 right = (m[0], m[4], m[8])，up = (m[1], m[5], m[9])。
    // 调用前模型视图矩阵必须正好是视图矩阵（不能套在任何天体的变换里）。
    GLfloat m[16] = {};
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    const float right[3] = {m[0], m[4], m[8]};
    const float up[3]    = {m[1], m[5], m[9]};

    glDisable(GL_LIGHTING);              // 关掉光照，glColor 才有效
    glDisable(GL_CULL_FACE);             // 公告板只有一个面，绕序无所谓
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);   // 加色混合：火焰只会让背景更亮
    glDepthMask(GL_FALSE);               // 不登记深度，但仍参与深度测试

    // 呼吸缩放 + alpha 闪烁：静止贴图靠这两下才有"在烧"的感觉
    const float pulse = 1.0f + p.pulse * std::sin(time * 1.7f);
    const float flameFlicker = p.flameAlpha + p.flicker * std::sin(time * 9.0f);
    const float haloAlpha = p.haloAlpha *
                            (1.0f - p.haloPulse + p.haloPulse * std::sin(time * 1.1f + 1.3f));

    // 主火舌：白热亮边 + 短火舌，缓慢正转
    drawLayer(right, up, cx, cy, cz, sunRadius * p.flameScale * pulse,
              time * p.flameSpin, flameFlicker, flameTexture);

    // 外层流焰：不带亮边，反向缓慢旋转，只加一点流动的层次
    drawLayer(right, up, cx, cy, cz, sunRadius * p.wispScale / pulse,
              time * p.wispSpin + 137.0f, p.wispAlpha, wispTexture);

    // 外围辉光：把整体亮度晕进背景
    drawLayer(right, up, cx, cy, cz, sunRadius * p.haloScale,
              0.0f, haloAlpha, glowTexture);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
}

void SunCorona::release()
{
    releaseTexture(flameTexture);
    releaseTexture(wispTexture);
    releaseTexture(glowTexture);
}
