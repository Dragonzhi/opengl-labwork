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
    TEX_RING     // 行星环：沿径向的明暗与透明度变化（带 alpha）
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

// 删除纹理并把句柄清零，避免二次删除
void releaseTexture(GLuint& id);
