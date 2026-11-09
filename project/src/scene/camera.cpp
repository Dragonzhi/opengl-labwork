#include "camera.h"
#include "body.h"

#include <cmath>

namespace {
const float PI = 3.1415926f;
const float kMinDist = 2.5f;
const float kMaxDist = 110.0f;
}

// 默认站在略高一点的位置，能看清行星环的椭圆
OrbitCamera::OrbitCamera() : yaw(20.0f), pitch(26.0f), dist(62.0f), focusTarget(0) {}

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
        dist = 62.0f;
    }
}

void OrbitCamera::setFocus(const Body* body)
{
    focusTarget = body;

    if (body == 0) {
        dist = 62.0f;               // 全局视角：刚好装得下 31 的海王星轨道
        return;
    }

    // 距离按天体半径定档：大的天体退远看全貌，小的天体拉近
    dist = body->radius() * 8.0f;
    if (dist < kMinDist) dist = kMinDist;
    if (dist > kMaxDist) dist = kMaxDist;

    // 顺手把相机转到受光侧：光源（太阳）在世界原点，天体在 P(px, 0, pz)。
    // 相机方位取"天体指向太阳"的方向再偏 55 度——正对受光面会丢掉明暗交界，
    // 偏一点接近经典的三分之四光，既看得清贴图又能看出球体的立体感。
    // 不做这一步的话，相机停在原方位角，很可能正对着夜半球，贴图再好也是一团黑。
    const Vec3 pos = body->worldPosition();
    if (pos.x * pos.x + pos.z * pos.z > 1.0f) {      // 太阳自己就在原点，跳过
        const float toSunDeg = std::atan2(-pos.x, -pos.z) * 180.0f / PI;
        yaw = wrapYaw(toSunDeg + 55.0f);
    }
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
