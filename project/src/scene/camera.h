#pragma once

#include <GL/freeglut.h>

class Body;

// 球坐标轨道相机。
// 相机绕"焦点"转：只改变 gluLookAt 的 eye 和 center，场景世界坐标一概不动。
// 焦点默认是世界原点（太阳），也可以切换成任意天体（聚焦）。
// 三个状态量就是不变量：yaw 折回 (-180,180]，pitch 钳制 ±85°，dist 钳制 [2.5,80]。
class OrbitCamera
{
public:
    OrbitCamera();

    void orbit(float deltaYawDeg, float deltaPitchDeg);
    void zoom(float factor);
    void reset();

    // 聚焦目标：传 0 表示回到世界原点。
    // 内部会把距离调到"能看清这个天体"的档位，并保持当前方位角/俯仰角不变。
    void setFocus(const Body* body);
    const Body* focus() const { return focusTarget; }

    void apply() const;

    float yawDeg() const { return yaw; }
    float pitchDeg() const { return pitch; }
    float distance() const { return dist; }

private:
    static float clampPitch(float value);
    static float wrapYaw(float value);

    float yaw;    // 绕 +Y 轴的方位角（度）
    float pitch;  // 相对 XZ 平面的俯仰角（度）
    float dist;   // 相机到焦点的距离

    const Body* focusTarget;   // 只引用，不拥有（天体由场景持有）
};
