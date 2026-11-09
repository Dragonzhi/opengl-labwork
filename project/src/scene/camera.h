#pragma once

#include <GL/freeglut.h>

// 球坐标轨道相机。
// 相机永远绕着焦点(0,0,0)转：只改变 gluLookAt 的 eye，场景世界坐标一概不动。
// 三个状态量就是不变量：yaw 折回 (-180,180]，pitch 钳制 ±85°，dist 钳制 [3,80]。
class OrbitCamera
{
public:
    OrbitCamera();

    void orbit(float deltaYawDeg, float deltaPitchDeg);
    void zoom(float factor);
    void reset();
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
};
