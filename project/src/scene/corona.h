#pragma once

#include <GL/freeglut.h>

#include "../gfx/texture.h"   // CoronaShape

// 太阳外焰（日冕）渲染器。
//
// 固定管线没有片元着色器，所以"火焰"不是逐像素算出来的，而是三层拼出来的：
//   1. 启动时程序化生成两张 RGBA 贴图：带火舌的日冕 + 平滑的外层辉光；
//   2. 每帧画两张始终正对相机的公告板（billboard），用加色混合叠在一起；
//   3. 内层正转、中层反转、外层呼吸，让静态贴图看起来在翻卷。
//
// 公告板放在穿过太阳球心、垂直于视线的平面上；深度测试会自动把藏在日面
// 后面的整片中心裁掉，所以贴图中间不需要精确抠洞，日面纹理也完全不受影响。
//
// 所有可调参数都放在项目的 corona.cfg 里：窗口运行中按 F5 会重新读文件并
// 重建贴图，改一个数字不用重新编译、也不用关窗口。

// 一组外焰参数。构造函数里是内置默认值；corona.cfg 里出现的键会覆盖对应项。
struct CoronaParams
{
    // 主火舌层：贴着日面的白热亮边 + 短火舌
    float flameScale;
    float flameAlpha;
    float flameSpin;

    // 外层流焰：同一套生成器，但把亮环权重设为 0，只留淡淡的火舌
    float wispScale;
    float wispAlpha;
    float wispSpin;

    // 外围辉光：把亮度晕进背景
    float haloScale;
    float haloAlpha;

    // 呼吸幅度 / 主火舌 alpha 闪烁幅度 / 外辉光脉动幅度
    float pulse;
    float flicker;
    float haloPulse;

    // 贴图形状参数（改了会重建纹理）
    CoronaShape shape;

    CoronaParams()
        : flameScale(1.30f), flameAlpha(0.90f), flameSpin(6.0f),
          wispScale(1.55f), wispAlpha(0.14f), wispSpin(-3.0f),
          haloScale(2.30f), haloAlpha(0.20f),
          pulse(0.020f), flicker(0.040f), haloPulse(0.080f)
    {
    }
};

class SunCorona
{
public:
    SunCorona();

    void init();                 // 需要 GL 上下文；在 sceneInit 里调用一次
    void update(float deltaTime);
    void draw(float centerX, float centerY, float centerZ, float sunRadius) const;
    void release();

    // 重新读 corona.cfg 并重建两张贴图。返回是否找到了配置文件。
    bool reload();

    const CoronaParams& params() const { return p; }
    const char* status() const { return statusText; }   // HUD 用，只能放 ASCII

private:
    bool loadParams(const char* path);
    bool applyParam(const char* key, float value);
    void rebuildTextures();
    void updateStatus(bool foundFile);

    // 一层公告板：四个角由相机的 right / up 张开，rollDeg 是贴图绕视线轴的自转角。
    // 透视投影已经把世界空间的正方形映射成屏幕正方形，所以这里不需要宽高比补偿。
    void drawLayer(const float* right, const float* up,
                   float cx, float cy, float cz,
                   float halfSize, float rollDeg,
                   float alpha, GLuint texture) const;

    CoronaParams p;

    GLuint flameTexture;
    GLuint wispTexture;
    GLuint glowTexture;

    float time;                  // 累计动画时间（秒）；场景暂停时由外部停止推进
    int   reloadCount;           // 第几次加载，HUD 上用来确认 F5 真的生效了
    int   appliedCount;          // 本次从 cfg 里认出的键数量
    char  statusText[64];        // 纯 ASCII：glutBitmapCharacter 画不了中文
};
