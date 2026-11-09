#pragma once

#include <GL/freeglut.h>

#include <vector>

// 一个天体 = 一层"公转 + 自转"的局部坐标系，可以再挂子天体（卫星）。
// 层级不是靠父子关系自动推导的，而是靠 glPushMatrix / glPopMatrix 的矩阵栈现算出来的。
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
    void addChild(Body* child);

    // 透明效果必须在所有不透明物体画完之后再画，所以单独留一个 late 通道
    void enableAtmosphere(float scale, float r, float g, float b, float alpha);
    void enableRing(float innerScale, float outerScale, GLuint texture);

    void update(float deltaTime);
    void draw(GLUquadric* quad) const;
    void drawTransparent(GLUquadric* quad) const;

    bool hasTransparent() const;

    // 供场景画轨道线 / 布置灯光使用
    float orbitRadius() const { return orbitDistance; }
    float radius() const { return bodyRadius; }

private:
    void applyMaterial() const;
    void drawRing() const;
    void drawAtmosphere(GLUquadric* quad) const;

    float bodyRadius;
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

    float atmoScale;
    float atmoColor[3];
    float atmoAlpha;

    float ringInner;
    float ringOuter;
    GLuint ringTexture;
};
