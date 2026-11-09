#include "texture.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Windows 的 gl.h 只到 OpenGL 1.1，GL_CLAMP_TO_EDGE 是 1.2 的常量，这里补上
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace {

// 内存里的位图，统一存成 RGBA 每通道 8 位（24 位真彩 + 8 位 alpha）
struct Bitmap
{
    int width;
    int height;
    unsigned char* rgba;

    Bitmap() : width(0), height(0), rgba(nullptr) {}
    ~Bitmap() { delete[] rgba; }
};

// 头文件里字段不对齐，直接用 memcpy 取，避免 reinterpret_cast 的对齐问题
unsigned int readU32(const unsigned char* p)
{
    unsigned int v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

unsigned short readU16(const unsigned char* p)
{
    unsigned short v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

int readI32(const unsigned char* p)
{
    int v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

// 只支持最常见的 24/32 位、未压缩 BMP；其余情况返回 false 交给程序化兜底
bool loadBmp(const char* path, Bitmap& out)
{
    std::FILE* fp = std::fopen(path, "rb");
    if (!fp) return false;

    unsigned char header[54] = {};
    if (std::fread(header, 1, 54, fp) != 54) {
        std::fclose(fp);
        return false;
    }

    const unsigned int dataOffset = readU32(header + 10);
    const int width = readI32(header + 18);
    const int rawHeight = readI32(header + 22);
    const unsigned short bitsPerPixel = readU16(header + 28);
    const unsigned int compression = readU32(header + 30);

    if (width <= 0 || rawHeight == 0 || (bitsPerPixel != 24 && bitsPerPixel != 32) ||
        compression != 0) {
        std::fclose(fp);
        return false;
    }

    // BMP 的 height 为正表示自下而上存储，为负表示自上而下
    const bool bottomUp = rawHeight > 0;
    const int height = bottomUp ? rawHeight : -rawHeight;
    const int bytesPerPixel = bitsPerPixel / 8;
    const int rowBytes = width * bytesPerPixel;

    out.width = width;
    out.height = height;
    out.rgba = new unsigned char[static_cast<size_t>(width) * height * 4];

    unsigned char* row = new unsigned char[rowBytes];
    bool ok = true;

    // 像素数据不一定紧跟在 54 字节文件头后面，必须按头里的偏移跳过去
    std::fseek(fp, static_cast<long>(dataOffset), SEEK_SET);

    for (int i = 0; i < height && ok; ++i) {
        if (std::fread(row, 1, static_cast<size_t>(rowBytes), fp) !=
            static_cast<size_t>(rowBytes)) {
            ok = false;
            break;
        }
        const int destRow = bottomUp ? (height - 1 - i) : i;
        unsigned char* dst = out.rgba + static_cast<size_t>(destRow) * width * 4;
        for (int x = 0; x < width; ++x) {
            // BMP 是 BGR 顺序，转成 RGB；32 位多出来的第 4 字节当 alpha
            dst[x * 4 + 0] = row[x * bytesPerPixel + 2];
            dst[x * 4 + 1] = row[x * bytesPerPixel + 1];
            dst[x * 4 + 2] = row[x * bytesPerPixel + 0];
            dst[x * 4 + 3] = (bytesPerPixel == 4) ? row[x * 4 + 3] : 255;
        }
    }

    delete[] row;
    std::fclose(fp);
    return ok;
}

// ---------------- 程序化纹理：确定性哈希 + 值噪声 ----------------

unsigned int hash2(int x, int y, int seed)
{
    unsigned int h = static_cast<unsigned int>(x) * 374761393u +
                     static_cast<unsigned int>(y) * 668265263u +
                     static_cast<unsigned int>(seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// 在 x 方向按 period 取模，让噪声在 u=0/1 处无缝拼上（球体贴图不留接缝）
float lattice(int ix, int iy, int period, int seed)
{
    int px = ix % period;
    if (px < 0) px += period;
    return static_cast<float>(hash2(px, iy, seed) & 0xFFFFu) / 65535.0f;
}

float valueNoise(float x, float y, int period, int seed)
{
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const float fx = x - ix;
    const float fy = y - iy;
    // smoothstep 插值：让格子之间平滑过渡，而不是双线性那种菱形纹
    const float ux = fx * fx * (3.0f - 2.0f * fx);
    const float uy = fy * fy * (3.0f - 2.0f * fy);

    const float a = lattice(ix, iy, period, seed);
    const float b = lattice(ix + 1, iy, period, seed);
    const float c = lattice(ix, iy + 1, period, seed);
    const float d = lattice(ix + 1, iy + 1, period, seed);

    const float top = a + (b - a) * ux;
    const float bottom = c + (d - c) * ux;
    return top + (bottom - top) * uy;
}

// 多倍频叠加：低频给大块结构，高频给细节
float fbm(float x, float y, int period, int seed, int octaves)
{
    float sum = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += valueNoise(x * frequency, y * frequency,
                          period * static_cast<int>(frequency), seed + i) * amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return sum;
}

// 直接在 CPU 上算出一整张 RGBA 位图（不依赖任何外部文件）
void buildProcedural(TextureKind kind, int seed, int width, int height,
                     unsigned char* rgba)
{
    for (int y = 0; y < height; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
        for (int x = 0; x < width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
            unsigned char* px = rgba + (static_cast<size_t>(y) * width + x) * 4;
            px[3] = 255;

            switch (kind) {
            case TEX_SUN: {
                const float n = fbm(u * 8.0f, v * 4.0f, 8, seed, 4);
                const float t = 0.55f + 0.45f * n;
                px[0] = 255;
                px[1] = static_cast<unsigned char>(205 + 40 * t);
                px[2] = static_cast<unsigned char>(70 + 90 * t);
                break;
            }
            case TEX_EARTH: {
                const float n = fbm(u * 6.0f, v * 6.0f, 6, seed, 5);
                const float lat = std::fabs(v - 0.5f) * 2.0f;  // 0=赤道 1=极点
                if (lat > 0.86f) {
                    px[0] = 235; px[1] = 240; px[2] = 250;     // 极冠
                } else if (n > 0.52f) {
                    px[0] = static_cast<unsigned char>(60 + 70 * n);
                    px[1] = static_cast<unsigned char>(120 + 90 * n);
                    px[2] = static_cast<unsigned char>(55 + 40 * n);
                } else {
                    px[0] = 12;
                    px[1] = static_cast<unsigned char>(50 + 40 * n);
                    px[2] = static_cast<unsigned char>(120 + 60 * n);
                }
                break;
            }
            case TEX_GAS: {
                // 只做灰度亮度，色相交给材质 diffuse 决定
                const float wobble = fbm(u * 4.0f, v * 10.0f, 4, seed, 3) * 3.0f;
                const float band = std::sin(v * 28.0f + wobble);
                const float t = 0.5f + 0.5f * band;
                const unsigned char level = static_cast<unsigned char>(200 + 55 * t);
                px[0] = level; px[1] = level; px[2] = level;
                break;
            }
            case TEX_ROCKY: {
                // 同样是灰度亮度，色相交给材质 diffuse
                const float n = fbm(u * 10.0f, v * 10.0f, 10, seed, 4);
                const unsigned char level = static_cast<unsigned char>(140 + 110 * (0.4f + 0.6f * n));
                px[0] = level; px[1] = level; px[2] = level;
                break;
            }
            case TEX_RING: {
                // u 是径向：条纹沿半径变化，v 方向基本不变
                const float n = fbm(u * 24.0f, 0.5f, 24, seed, 2);
                const float fine = std::sin(u * 90.0f + n * 6.0f);
                float alpha = 0.30f + 0.55f * n + 0.18f * fine;
                if (alpha < 0.0f) alpha = 0.0f;
                if (alpha > 1.0f) alpha = 1.0f;
                px[0] = 228; px[1] = 210; px[2] = 176;
                px[3] = static_cast<unsigned char>(255 * alpha);
                break;
            }
            default:
                px[0] = 128; px[1] = 128; px[2] = 128;
                break;
            }
        }
    }
}

}  // namespace

GLuint createTexture(const char* name, TextureKind kind, int seed,
                     int width, int height, bool repeatT)
{
    Bitmap bmp;
    char path[512] = {};

    // 运行目录既可能是 project/ 也可能是 project/build/，两处都试一下
    std::snprintf(path, sizeof(path), "assets/%s.bmp", name);
    bool loaded = loadBmp(path, bmp);

    if (!loaded) {
        std::snprintf(path, sizeof(path), "../assets/%s.bmp", name);
        loaded = loadBmp(path, bmp);
    }

    if (!loaded) {
        std::printf("TEXTURE: %s.bmp 未找到，使用程序化兜底纹理\n", name);
        bmp.width = width;
        bmp.height = height;
        bmp.rgba = new unsigned char[static_cast<size_t>(width) * height * 4];
        buildProcedural(kind, seed, width, height, bmp.rgba);
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    // 球面 s 方向必须环绕；t 方向到极点本来就是边界，用 clamp 更干净
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeatT ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // 缩小过滤走 mipmap：远处不闪烁。前提是纹理分辨率高于屏幕上的像素数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    const int mipResult = gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, bmp.width, bmp.height,
                                            GL_RGBA, GL_UNSIGNED_BYTE, bmp.rgba);
    if (mipResult != 0) {
        std::printf("TEXTURE: %s mipmap 构建失败(%d)\n", name, mipResult);
        glDeleteTextures(1, &id);
        return 0;
    }

    return id;
}

void releaseTexture(GLuint& id)
{
    if (id != 0) {
        glDeleteTextures(1, &id);
        id = 0;
    }
}
