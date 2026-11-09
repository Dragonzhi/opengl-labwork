#pragma once

#include <GL/freeglut.h>

#include <vector>

// 世界坐标点的最小载体：只需要位置，不需要完整的向量运算
struct Vec3
{
    float x;
    float y;
    float z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float px, float py, float pz) : x(px), y(py), z(pz) {}
};

// 一个天体 = 一层"公转 + 自转"的局部坐标系，可以再挂子天体（卫星）。
// 层级不是靠父子关系自动推导的，而是靠 glPushMatrix / glPopMatrix 的矩阵栈现算出来的。
// 环影的全局调节（由 scene.cfg 提供）：强度倍数、边界柔化比例、球面细分倍数。
// 放在这里而不是 Body 成员，是因为它描述的是"渲染质量"，不属于某颗行星。
void setRingShadowTuning(float strength, float feather, int subdiv);

class Body
{
public:
    Body();

    void setShape(float radius, int slices, int stacks);
    void setOrbit(float orbitRadius, float orbitSpeedDegPerSec);
    void setOrbitPhase(float deg);          // 初始公转相位，避免所有天体都从 +X 轴出发
    void setSpin(float spinSpeedDegPerSec, float tiltDeg);
    void setDiffuse(float r, float g, float b);
    void setEmission(float r, float g, float b);
    void setShininess(float value);
    void setTexture(GLuint texture);
    void setOrbitRadius(float value);      // 只改轨道半径，不动角速度（真实/演示间距切换用）
    void setRadiusScale(float value);      // 只缩放绘制尺寸，不改轨道（同上）
    void addChild(Body* child);

    // 透明效果必须在所有不透明物体画完之后再画，所以单独留一个 late 通道
    void enableAtmosphere(float scale, float r, float g, float b, float alpha);
    // shadowStrength：环投在行星上的影子强度（0 = 不算，土星 0.85，天王星环太暗可以给 0）
    void enableRing(float innerScale, float outerScale, GLuint texture,
                    float shadowStrength = 0.0f);

    void update(float deltaTime);
    void draw(GLUquadric* quad) const;
    void drawTransparent(GLUquadric* quad) const;

    bool hasTransparent() const;

    // 供场景画轨道线 / 布置灯光 / 相机聚焦使用
    float orbitRadius() const { return orbitDistance; }
    float radius() const { return bodyRadius * radiusScale; }

    // 当前帧的世界坐标位置：沿父链把每一级的"公转 + 平移"叠加起来。
    // draw() 里的矩阵栈是 GPU 侧的同一件事，这里是 CPU 侧算一份给相机用。
    Vec3 worldPosition() const;

private:
    void applyMaterial() const;
    void drawRing() const;
    void drawShadowedSphere(float radius, int slices, int stacks) const;
    void drawAtmosphere(GLUquadric* quad) const;

    float bodyRadius;
    float radiusScale;     // 1.0 = 演示尺寸；真实间距模式下整颗星缩成小点
    int slices;
    int stacks;

    float orbitDistance;   // 公转半径（沿父坐标系 +X）
    float orbitAngle;
    float orbitSpeed;

    float spinAngle;
    float spinSpeed;
    float tiltDeg;

    float diffuse[3];
    float emission[3];
    float shininess;

    GLuint texture;

    std::vector<Body*> children;   // 只引用，不拥有（对象由场景以静态方式持有）
    const Body* parent;            // 由 addChild 反向写入，用于计算世界位置。根天体为 0

    float atmoScale;
    float atmoColor[3];
    float atmoAlpha;

    float ringInner;
    float ringOuter;
    GLuint ringTexture;
    float ringShadowStrength;
};
