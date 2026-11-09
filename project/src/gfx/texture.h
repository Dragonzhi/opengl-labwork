#pragma once

#include <GL/freeglut.h>

// 程序化兜底纹理的外观类型。真正的贴图优先从 assets/<name>.bmp 读取，
// 读不到时才用这里给出的类型现场生成一张。
enum TextureKind
{
    TEX_ROCKY,   // 岩石天体：灰度凹凸
    TEX_EARTH,   // 类地行星：海洋 + 大陆 + 极冠
    TEX_GAS,     // 类木行星：灰度水平条带
    TEX_SUN,     // 恒星：高亮暖色斑驳
    TEX_RING,    // 土星环：按真实环系半径分段的径向剖面（带 alpha）
    TEX_RING_THIN, // 天王星环：几条极窄的暗环，其余几乎全透明（带 alpha）
    TEX_CORONA,  // 日冕：以贴图中心为极点的环形火舌（带 alpha，只做程序化）
    TEX_GLOW     // 柔和辉光：径向平滑衰减（带 alpha，只做程序化）
};

// 日冕 / 辉光的形状参数。由 corona.cfg 提供，运行时重建贴图，
// 所以可以直接在窗口里按 F5 热改，不用重新编译。
struct CoronaShape
{
    float rimWeight;      // 日面边缘白热亮环的强度
    float tongueWeight;   // 火舌强度
    float stripePower;    // 角向条纹指数：越大火舌越细、暗隙越宽
    float flameFreq;      // 火舌的角向频率：越大火舌越多越密
    float reachBase;      // 火舌基础长度（占贴图半径的比例）
    float reachRange;     // 火舌长度随角度起伏的范围
    float glowFalloff;    // 外层辉光的径向衰减指数

    CoronaShape()
        : rimWeight(0.95f), tongueWeight(1.50f), stripePower(1.7f),
          flameFreq(12.0f), reachBase(0.88f), reachRange(0.08f),
          glowFalloff(1.6f) {}
};

// 创建一张 RGBA 纹理并自动生成 mipmap。
//   name    : assets/<name>.bmp 文件名（不含扩展名）
//   kind    : BMP 缺失时用哪种程序化外观
//   seed    : 程序化噪声的随机种子（同 seed 结果可复现）
//   width/height : 兜底纹理尺寸，必须是 2 的幂（mipmap 的前提）
//   repeatT : t 方向是否平铺（行星环要平铺，普通球体贴图不要）
// 返回 0 表示创建失败；失败调用方必须能退化成"有材质无纹理"继续画。
GLuint createTexture(const char* name, TextureKind kind, int seed,
                     int width, int height, bool repeatT);

// 纯程序化生成，完全不尝试读 BMP。
// 用在必须依赖 alpha 通道的贴图上（日冕、辉光）：24 位 BMP 没有 alpha，
// 一旦被 24 位图顶替，加色混合就会把整个四边形加成一块实心色块。
// shape 只对 TEX_CORONA / TEX_GLOW 有意义，其他类型忽略。
GLuint createProceduralTexture(TextureKind kind, int seed, int width, int height,
                               const CoronaShape& shape);

// 删除纹理并把句柄清零，避免二次删除
void releaseTexture(GLuint& id);
