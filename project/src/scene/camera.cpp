#include "camera.h"

#include <cmath>

namespace {
const float PI = 3.1415926f;
}

// 默认站在略高一点的位置，能看清行星环的椭圆
OrbitCamera::OrbitCamera() : yaw(20.0f), pitch(26.0f), dist(40.0f) {}

float OrbitCamera::clampPitch(float value)
{
    // 视线不能与 up 向量平行，否则 gluLookAt 的视图矩阵退化
    const float limit = 85.0f;
    if (value < -limit) return -limit;
    if (value > limit) return limit;
    return value;
}

float OrbitCamera::wrapYaw(float value)
{
    while (value > 180.0f) value -= 360.0f;
    while (value < -180.0f) value += 360.0f;
    return value;
}

void OrbitCamera::orbit(float deltaYawDeg, float deltaPitchDeg)
{
    yaw = wrapYaw(yaw + deltaYawDeg);
    pitch = clampPitch(pitch + deltaPitchDeg);
}

void OrbitCamera::zoom(float factor)
{
    dist *= factor;
    if (dist < 3.0f) dist = 3.0f;
    if (dist > 80.0f) dist = 80.0f;
}

void OrbitCamera::reset()
{
    yaw = 20.0f;
    pitch = 26.0f;
    dist = 40.0f;
}

void OrbitCamera::apply() const
{
    // 球坐标 -> 直角坐标：
    //   x = d*cos(pitch)*sin(yaw)
    //   y = d*sin(pitch)
    //   z = d*cos(pitch)*cos(yaw)
    const float yawRad = yaw * PI / 180.0f;
    const float pitchRad = pitch * PI / 180.0f;
    const float horizontal = dist * std::cos(pitchRad);

    const float eyeX = horizontal * std::sin(yawRad);
    const float eyeY = dist * std::sin(pitchRad);
    const float eyeZ = horizontal * std::cos(yawRad);

    // 后乘当前矩阵：把"世界坐标 -> 相机坐标"的变换接在模型视图矩阵栈上
    gluLookAt(eyeX, eyeY, eyeZ,
              0.0f, 0.0f, 0.0f,
              0.0f, 1.0f, 0.0f);
}
