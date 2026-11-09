#include "camera.h"
#include "body.h"

#include <cmath>

namespace {
const float PI = 3.1415926f;
const float kMinDist = 2.5f;
const float kMaxDist = 80.0f;
}

// 默认站在略高一点的位置，能看清行星环的椭圆
OrbitCamera::OrbitCamera() : yaw(20.0f), pitch(26.0f), dist(40.0f), focusTarget(0) {}

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
    if (dist < kMinDist) dist = kMinDist;
    if (dist > kMaxDist) dist = kMaxDist;
}

void OrbitCamera::reset()
{
    yaw = 20.0f;
    pitch = 26.0f;

    // 保留当前聚焦目标，只把距离拉回到"适合看这个目标"的档位
    if (focusTarget != 0) {
        dist = focusTarget->radius() * 8.0f;
        if (dist < kMinDist) dist = kMinDist;
        if (dist > kMaxDist) dist = kMaxDist;
    } else {
        dist = 40.0f;
    }
}

void OrbitCamera::setFocus(const Body* body)
{
    focusTarget = body;

    if (body == 0) {
        dist = 40.0f;               // 全局视角
        return;
    }

    // 距离按天体半径定档：大的天体退远看全貌，小的天体拉近
    dist = body->radius() * 8.0f;
    if (dist < kMinDist) dist = kMinDist;
    if (dist > kMaxDist) dist = kMaxDist;
}

void OrbitCamera::apply() const
{
    // 焦点：默认世界原点，聚焦时跟着天体走
    Vec3 center;
    if (focusTarget != 0) {
        center = focusTarget->worldPosition();
    }

    // 球坐标 -> 直角坐标（相对焦点）：
    //   x = d*cos(pitch)*sin(yaw)
    //   y = d*sin(pitch)
    //   z = d*cos(pitch)*cos(yaw)
    const float yawRad = yaw * PI / 180.0f;
    const float pitchRad = pitch * PI / 180.0f;
    const float horizontal = dist * std::cos(pitchRad);

    const float eyeX = center.x + horizontal * std::sin(yawRad);
    const float eyeY = center.y + dist * std::sin(pitchRad);
    const float eyeZ = center.z + horizontal * std::cos(yawRad);

    // 后乘当前矩阵：把"世界坐标 -> 相机坐标"的变换接在模型视图矩阵栈上
    gluLookAt(eyeX, eyeY, eyeZ,
              center.x, center.y, center.z,
              0.0f, 1.0f, 0.0f);
}
