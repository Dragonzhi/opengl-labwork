#include "body.h"

#include <cmath>

namespace {
const float PI = 3.1415926f;

float smoothstep(float e0, float e1, float x);   // 定义在下面，这里先声明

// 环影的全局可调项，默认值和 scene.cfg 里的默认值一致
float gShadowStrengthScale = 1.0f;
float gShadowFeather = 2.5f;   // 软边宽度，单位是屏幕像素（不是环宽的比例）
int   gShadowSubdiv = 4;

// 3x3 旋转矩阵，行主序。只为了在 CPU 侧把球面顶点搬到世界坐标去做阴影判断
struct Mat3
{
    float m[9];
};

Mat3 matMul(const Mat3& a, const Mat3& b)
{
    Mat3 r;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i * 3 + j] = a.m[i * 3 + 0] * b.m[0 * 3 + j] +
                             a.m[i * 3 + 1] * b.m[1 * 3 + j] +
                             a.m[i * 3 + 2] * b.m[2 * 3 + j];
        }
    }
    return r;
}

// 和 gluRotatef 的约定一致：绕 +Y 转 a 把 (1,0,0) 变成 (cos a, 0, -sin a)
Mat3 rotY(float deg)
{
    const float r = deg * PI / 180.0f;
    const float c = std::cos(r), s = std::sin(r);
    Mat3 m = {{ c, 0.0f,  s,
                0.0f, 1.0f, 0.0f,
               -s, 0.0f,  c }};
    return m;
}

Mat3 rotZ(float deg)
{
    const float r = deg * PI / 180.0f;
    const float c = std::cos(r), s = std::sin(r);
    Mat3 m = {{ c, -s, 0.0f,
                s,  c, 0.0f,
                0.0f, 0.0f, 1.0f }};
    return m;
}

Mat3 rotX(float deg)
{
    const float r = deg * PI / 180.0f;
    const float c = std::cos(r), s = std::sin(r);
    Mat3 m = {{ 1.0f, 0.0f, 0.0f,
                0.0f,    c,   -s,
                0.0f,    s,    c }};
    return m;
}

// 与 texture.cpp 里 TEX_RING 的径向剖面保持同一份数据：
// u=0 是内缘、u=1 是外缘，土星环系跨 1.12R~2.27R
float ringAlphaProfile(float u)
{
    const float R = 1.12f + u * 1.15f;
    float a = 0.08f;
    a += (0.26f - a) * smoothstep(1.22f, 1.26f, R);
    a += (0.88f - a) * smoothstep(1.51f, 1.56f, R);
    a += (0.05f - a) * smoothstep(1.94f, 1.96f, R);
    a += (0.58f - a) * smoothstep(2.02f, 2.05f, R);
    return a;
}

float smoothstep(float e0, float e1, float x)
{
    float t = (x - e0) / (e1 - e0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}
}  // namespace

// 由平近点角解出轨道上的相对位置（相对父坐标系原点）。
// 开普勒方程 M = E - e·sinE 没有解析解，牛顿迭代三五行就收敛到 1e-6。
// 椭圆：x = a(cosE - e)，z = b·sinE，b = a·sqrt(1-e²)
// 倾角：把 z 分量绕 X 轴抬起来，y = z·sin i，z = z·cos i
Vec3 orbitOffsetFor(float semiMajor, float ecc, float incDeg, float meanDeg)
{
    const float M = meanDeg * PI / 180.0f;
    float E = M;
    for (int k = 0; k < 5; ++k) {
        E -= (E - ecc * std::sin(E) - M) / (1.0f - ecc * std::cos(E));
    }

    const float b = semiMajor * std::sqrt(1.0f - ecc * ecc);
    const float x = semiMajor * (std::cos(E) - ecc);
    const float z = b * std::sin(E);

    const float inc = incDeg * PI / 180.0f;
    Vec3 p;
    p.x = x;
    p.y = z * std::sin(inc);
    p.z = z * std::cos(inc);
    return p;
}

// 环影的质量旋钮：强度倍数 / 软边宽度（像素） / 球面细分倍数
void setRingShadowTuning(float strength, float feather, int subdiv)
{
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 2.0f) strength = 2.0f;
    gShadowStrengthScale = strength;

    // 0 会让 smoothstep 的区间退化成一点（0/0），所以留 0.25 像素的下限
    if (feather < 0.25f) feather = 0.25f;
    if (feather > 4.0f) feather = 4.0f;
    gShadowFeather = feather;

    if (subdiv < 1) subdiv = 1;
    if (subdiv > 8) subdiv = 8;
    gShadowSubdiv = subdiv;
}

Body::Body()
    : bodyRadius(1.0f), radiusScale(1.0f), slices(20), stacks(16),
      orbitDistance(0.0f), eccentricity(0.0f), inclinationDeg(0.0f),
      orbitAngle(0.0f), orbitSpeed(0.0f),
      spinAngle(0.0f), spinSpeed(0.0f), tiltDeg(0.0f),
      shininess(24.0f), texture(0), parent(0),
      atmoScale(1.0f), atmoAlpha(0.0f),
      ringInner(1.0f), ringOuter(1.0f), ringTexture(0), ringShadowStrength(0.0f)
{
    diffuse[0] = 0.8f; diffuse[1] = 0.8f; diffuse[2] = 0.8f;
    emission[0] = 0.0f; emission[1] = 0.0f; emission[2] = 0.0f;
    atmoColor[0] = 0.0f; atmoColor[1] = 0.0f; atmoColor[2] = 0.0f;
}

void Body::setShape(float r, int s, int t)
{
    bodyRadius = r;
    slices = s;
    stacks = t;
}

void Body::setOrbit(float radiusValue, float speed)
{
    orbitDistance = radiusValue;
    orbitSpeed = speed;
}

void Body::setOrbitRadius(float value)
{
    orbitDistance = value;
}

void Body::setRadiusScale(float value)
{
    radiusScale = value;
}

void Body::setOrbitPhase(float deg)
{
    orbitAngle = deg;
}

void Body::setOrbitShape(float ecc, float incDeg)
{
    if (ecc < 0.0f) ecc = 0.0f;
    if (ecc > 0.5f) ecc = 0.5f;
    eccentricity = ecc;
    inclinationDeg = incDeg;
}

void Body::setOrbitSpeed(float degPerSec)
{
    orbitSpeed = degPerSec;
}

Vec3 Body::orbitPointAt(float meanAnomalyDeg) const
{
    return orbitOffsetFor(orbitDistance, eccentricity, inclinationDeg, meanAnomalyDeg);
}

void Body::setSpin(float speed, float tilt)
{
    spinSpeed = speed;
    tiltDeg = tilt;
}

void Body::setDiffuse(float r, float g, float b)
{
    diffuse[0] = r; diffuse[1] = g; diffuse[2] = b;
}

void Body::setEmission(float r, float g, float b)
{
    emission[0] = r; emission[1] = g; emission[2] = b;
}

void Body::setShininess(float value)
{
    shininess = value;
}

void Body::setTexture(GLuint id)
{
    texture = id;
}

void Body::addChild(Body* child)
{
    if (child == 0) return;
    child->parent = this;      // 反向指针，只用于 CPU 侧算世界位置
    children.push_back(child);
}

// 与 draw() 里的矩阵栈一一对应：
//   每一级的偏移 (orbitDistance,0,0) 都被"该级 + 它所有祖先"的公转角累积旋转。
// 举例：月球的世界位置 = R(地球角)·(地球轨道半径) + R(地球角 + 月球角)·(月球轨道半径)。
// 只用自己的角度会漏掉父坐标系的旋转，聚焦到卫星上就会偏。
Vec3 Body::worldPosition() const
{
    // 先自底向上收集父链，再从根往下累加，才能拿到正确的累积角度
    const Body* chain[16];
    int depth = 0;
    for (const Body* p = this; p != 0 && depth < 16; p = p->parent) {
        chain[depth++] = p;
    }

    // 每一层贡献"它自己的椭圆轨道偏移"，直接相加即可：
    // 椭圆的倾角、偏心率都已经在 orbitOffsetFor 里算好，不再是"绕 Y 转一个角度"
    Vec3 pos;
    for (int i = depth - 1; i >= 0; --i) {
        const Vec3 p = chain[i]->orbitPointAt(chain[i]->orbitAngle);
        pos.x += p.x;
        pos.y += p.y;
        pos.z += p.z;
    }
    return pos;
}

void Body::enableAtmosphere(float scale, float r, float g, float b, float alpha)
{
    atmoScale = scale;
    atmoColor[0] = r; atmoColor[1] = g; atmoColor[2] = b;
    atmoAlpha = alpha;
}

void Body::enableRing(float innerScale, float outerScale, GLuint id, float shadowStrength)
{
    ringInner = innerScale;
    ringOuter = outerScale;
    ringTexture = id;
    ringShadowStrength = shadowStrength;
}

bool Body::hasTransparent() const
{
    return ringTexture != 0 || atmoAlpha > 0.0f;
}

void Body::update(float deltaTime)
{
    orbitAngle += orbitSpeed * deltaTime;
    spinAngle += spinSpeed * deltaTime;
    orbitAngle = std::fmod(orbitAngle, 360.0f);
    spinAngle = std::fmod(spinAngle, 360.0f);

    for (std::size_t i = 0; i < children.size(); ++i) {
        children[i]->update(deltaTime);
    }
}

// Phong 模型的 4 个材质属性：ambient / diffuse / specular / shininess，
// 再加上太阳需要的 emission（自发光）。光照开着时 glColor 是无效的，必须走材质。
void Body::applyMaterial() const
{
    // ambient 系数给得偏高：太空里没有别的补光，纯按物理会让背光面完全死黑，
    // 这里用一点环境反射把暗面抬到"看得见"的程度，明暗交界仍然保留
    const GLfloat ambient[4]  = {diffuse[0] * 0.55f, diffuse[1] * 0.55f, diffuse[2] * 0.55f, 1.0f};
    const GLfloat diffuseV[4] = {diffuse[0], diffuse[1], diffuse[2], 1.0f};
    const GLfloat specular[4] = {0.40f, 0.40f, 0.40f, 1.0f};
    const GLfloat emissionV[4] = {emission[0], emission[1], emission[2], 1.0f};

    glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseV);
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT, GL_EMISSION, emissionV);
    glMaterialf(GL_FRONT, GL_SHININESS, shininess);
}

void Body::draw(GLUquadric* quad) const
{
    glPushMatrix();

    // 第 1 层：公转。位置由开普勒方程解出的椭圆给出，直接平移到那个点；
    // 不能再写成"绕 Y 转一个角度"，因为椭圆 + 倾角已经不是纯旋转了
    const Vec3 orbitPos = orbitPointAt(orbitAngle);
    glTranslatef(orbitPos.x, orbitPos.y, orbitPos.z);

    // 第 2 层：自转。先倾斜自转轴（绕 Z），再绕自身 Y 轴自转
    glPushMatrix();
    glRotatef(tiltDeg, 0.0f, 0.0f, 1.0f);
    glRotatef(spinAngle, 0.0f, 1.0f, 0.0f);

    // gluSphere 的纹理极点在 Z 轴上，正对相机时会变成一团放射状的"风扇"。
    // 先把极轴转到 Y 轴：极点被赶到画面上下边缘，气态行星的条带也才是水平的。
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);

    // 尺寸缩放只作用在本体上：真实间距模式下行星缩成小点，
    // 但轨道半径不跟着变，否则"真实比例"就白展示了
    glScalef(radiusScale, radiusScale, radiusScale);

    applyMaterial();
    if (texture != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    if (ringTexture != 0 && ringShadowStrength > 0.0f) {
        // 环会挡光：换成逐顶点的球面，把影子和本体一次画完
        drawShadowedSphere(bodyRadius, slices * gShadowSubdiv, stacks * gShadowSubdiv);
    } else {
        gluSphere(quad, bodyRadius, slices, stacks);
    }
    if (texture != 0) {
        glDisable(GL_TEXTURE_2D);
    }
    glPopMatrix();

    // 子天体挂在本天体的轨道坐标系里，因此不受自转影响（月球不会跟着地球自转甩动）
    for (std::size_t i = 0; i < children.size(); ++i) {
        children[i]->draw(quad);
    }

    glPopMatrix();
}

// 逐顶点阴影球面：参数化和 gluSphere 一样（极点在局部 Z 轴、u 沿经度、v 从北到南），
// 只是顶点由我们自己提交，才能把"环挡不挡这颗顶点"算进顶点颜色。
// 顶点颜色只挂到 GL_COLOR_MATERIAL 的 GL_DIFFUSE 上：阴影的本质是"少了直射光"，
// 环境光必须原样留着。否则夜半球（本来就没有直射光）也会被画上影子，
// 变成一条现实中根本看不见的黑带。
void Body::drawShadowedSphere(float radius, int slices, int stacks) const
{
    const Vec3 center = worldPosition();

    // 世界变换：椭圆位置用平移，姿态只剩 Rz(倾角) * Ry(自转) * Rx(90)，
    // 公转不再贡献旋转（这是改成椭圆轨道后唯一需要同步的地方）
    const Mat3 rot = matMul(matMul(rotZ(tiltDeg), rotY(spinAngle)), rotX(90.0f));
    // 环面法线：环画在 Rz(tilt) 之后的 XZ 平面上，法线就是那里的 +Y
    const Mat3 ringRot = rotZ(tiltDeg);
    const Vec3 normal = {ringRot.m[1], ringRot.m[4], ringRot.m[7]};

    const float innerR = radius * radiusScale * ringInner;
    const float outerR = radius * radiusScale * ringOuter;

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_DIFFUSE);   // 只乘漫反射，环境光不动

    // ---------- 抗锯齿：先量出"一个屏幕像素在世界坐标里有多宽" ----------
    // 阴影的软边宽度必须按**像素**给。原因在求交公式里：t = -h/c，c = s·法线
    // = sinθ（θ 是太阳高出环面的角度）。c 越小，同样的表面位移对应的交点半径 r
    // 变化越大，放大约 1/|c| 倍。按"环宽的比例"给死一个 feather，到了太阳贴近
    // 环面（θ 小）的时候它会缩到半个像素以下 —— 滤波窗口比采样间隔还窄，
    // 等于没滤波，硬边和锯齿就回来了。所以这里先算出 worldPerPixel，
    // 后面按每个顶点的 1/|c| 放大，让边界在屏幕上永远是固定几个像素。
    GLfloat mvMat[16], projMat[16];
    GLint vp[4];
    glGetFloatv(GL_MODELVIEW_MATRIX, mvMat);
    glGetFloatv(GL_PROJECTION_MATRIX, projMat);
    glGetIntegerv(GL_VIEWPORT, vp);
    // 局部原点就是这个天体的中心，它的眼坐标 z 直接取平移分量
    const float eyeZ = mvMat[14];
    // gluPerspective 的 m[5] = cot(fovy/2)，所以"一像素 = 2·|z| /(m5·高)"世界单位
    float worldPerPixel = 0.0f;
    if (projMat[5] > 1e-4f && vp[3] > 0) {
        worldPerPixel = 2.0f * std::fabs(eyeZ) / (projMat[5] * static_cast<float>(vp[3]));
    }

    for (int j = 0; j < stacks; ++j) {
        const float v0 = static_cast<float>(j) / static_cast<float>(stacks);
        const float v1 = static_cast<float>(j + 1) / static_cast<float>(stacks);
        const float phi0 = PI * v0;
        const float phi1 = PI * v1;

        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= slices; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(slices);
            const float theta = 2.0f * PI * u;
            const float ct = std::cos(theta), st = std::sin(theta);

            const float ringA[2] = {phi0, phi1};
            for (int k = 0; k < 2; ++k) {
                const float sp = std::sin(ringA[k]);
                const float nx = sp * ct;
                const float ny = sp * st;
                const float nz = std::cos(ringA[k]);

                // 该顶点在世界坐标里的位置
                const float s = radius * radiusScale;
                const Vec3 world = {center.x + s * (rot.m[0] * nx + rot.m[1] * ny + rot.m[2] * nz),
                                    center.y + s * (rot.m[3] * nx + rot.m[4] * ny + rot.m[5] * nz),
                                    center.z + s * (rot.m[6] * nx + rot.m[7] * ny + rot.m[8] * nz)};

                float shade = 1.0f;
                const float len = std::sqrt(world.x * world.x + world.y * world.y + world.z * world.z);
                if (len > 1e-4f) {
                    // 指向太阳（世界原点）的单位向量
                    const float sx = -world.x / len, sy = -world.y / len, sz = -world.z / len;
                    const float c = sx * normal.x + sy * normal.y + sz * normal.z;
                    const float vx = world.x - center.x, vy = world.y - center.y, vz = world.z - center.z;
                    const float h = vx * normal.x + vy * normal.y + vz * normal.z;

                    // 沿光线方向求与环面（过行星中心、法线 normal）的交点
                    if (c < -1e-4f || c > 1e-4f) {
                        const float tHit = -h / c;
                        if (tHit > 0.0f) {
                            const float qx = vx + tHit * sx;
                            const float qy = vy + tHit * sy;
                            const float qz = vz + tHit * sz;
                            const float r = std::sqrt(qx * qx + qy * qy + qz * qz);
                            // 软边宽度：gShadowFeather 是"想要几个像素"，乘上
                            // 一个像素对应的 Δr 就是世界单位下的滤波窗口。
                            // |c| 就是前面说的那个放大因子，夹到 0.05 免得除爆；
                            // 上限压到环带宽度的 30%，防止掠射时整条带糊成一片。
                            const float cAbs = c < 0.0f ? -c : c;
                            float feather = gShadowFeather * worldPerPixel /
                                            (cAbs > 0.05f ? cAbs : 0.05f);
                            if (feather < 1e-4f) feather = 1e-4f;
                            const float maxFeather = (outerR - innerR) * 0.3f;
                            if (feather > maxFeather) feather = maxFeather;
                            const float cover = smoothstep(innerR, innerR + feather, r) *
                                                (1.0f - smoothstep(outerR - feather, outerR, r));
                            if (cover > 0.0f) {
                                const float uu = (r - innerR) / (outerR - innerR);
                                float strength = ringShadowStrength * gShadowStrengthScale;
                                if (strength > 1.0f) strength = 1.0f;

                                // 环剖面本身也不能直接采：C 环 0.26 → B 环 0.88 是一条
                                // 硬跳变（卡西尼缝更是一刀切断），上面的 cover 只负责
                                // 环带整体的内外缘，管不到带子内部的这些边界。只采一次
                                // 的话，暗带里面会沿环缝一圈一圈地冒出锯齿。用同样的
                                // 像素宽度做一次三角滤波：三次采样加权平均，约等于
                                // 把这个一维函数按一个像素的窗口预滤波。
                                const float du = feather / (outerR - innerR);
                                const float ringAlpha = ringAlphaProfile(uu) * 0.5f +
                                    (ringAlphaProfile(uu - du) + ringAlphaProfile(uu + du)) * 0.25f;

                                // 掠射角处收敛：漫反射项本身已经乘过一次 N·L，
                                // 这里再照抄一个 N·L 就成了双重计数——亮面中段的暗带
                                // 会随角度变淡发灰。所以只在快到明暗交界的地方把影子
                                // 平滑收到 0：那儿直射光本来就快没了，硬切一道影更假。
                                const float wnx = rot.m[0] * nx + rot.m[1] * ny + rot.m[2] * nz;
                                const float wny = rot.m[3] * nx + rot.m[4] * ny + rot.m[5] * nz;
                                const float wnz = rot.m[6] * nx + rot.m[7] * ny + rot.m[8] * nz;
                                const float facing = smoothstep(0.0f, 0.15f,
                                                                wnx * sx + wny * sy + wnz * sz);

                                shade = 1.0f - strength * ringAlpha * cover * facing;
                            }
                        }
                    }
                }

                glColor3f(shade, shade, shade);
                glNormal3f(nx, ny, nz);
                glTexCoord2f(u, v0 + (v1 - v0) * static_cast<float>(k));
                glVertex3f(radius * nx, radius * ny, radius * nz);
            }
        }
        glEnd();
    }

    glDisable(GL_COLOR_MATERIAL);
}

void Body::drawTransparent(GLUquadric* quad) const
{
    if (!hasTransparent()) return;

    glPushMatrix();
    const Vec3 orbitPos = orbitPointAt(orbitAngle);
    glTranslatef(orbitPos.x, orbitPos.y, orbitPos.z);
    glRotatef(tiltDeg, 0.0f, 0.0f, 1.0f);
    glScalef(radiusScale, radiusScale, radiusScale);

    if (ringTexture != 0) {
        drawRing();
    }
    if (atmoAlpha > 0.0f) {
        drawAtmosphere(quad);
    }

    glPopMatrix();
}

// 行星环：手写三角形带，纹理坐标 s 沿半径、t 沿圆周
void Body::drawRing() const
{
    const float inner = bodyRadius * ringInner;
    const float outer = bodyRadius * ringOuter;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);        // 透明物体不写深度，但保留深度测试
    glDisable(GL_LIGHTING);       // 环用纹理本色，避免斜射时整片变黑
    glDisable(GL_CULL_FACE);      // 从上下两侧都要能看到
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, ringTexture);

    glNormal3f(0.0f, 1.0f, 0.0f);

    // ---------- 行星本影投在环上的那一段 ----------
    // 太阳固定在世界的原点，所以"行星挡住太阳"就是一条圆柱：
    // 位于行星背后、且离"太阳-行星"这条轴的距离小于行星半径的点，就在本影里。
    // 全部在眼坐标系里算，这样不用关心相机和轨道角度——
    // 当前模型视图矩阵直接把局部坐标搬到眼坐标，光源位置也是眼坐标。
    GLfloat mv[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    GLfloat lightEye[4];
    glGetLightfv(GL_LIGHT0, GL_POSITION, lightEye);

    const float centerX = mv[12], centerY = mv[13], centerZ = mv[14];
    float axisX = centerX - lightEye[0];
    float axisY = centerY - lightEye[1];
    float axisZ = centerZ - lightEye[2];
    const float axisLen = std::sqrt(axisX * axisX + axisY * axisY + axisZ * axisZ);
    if (axisLen > 1e-6f) {
        axisX /= axisLen; axisY /= axisLen; axisZ /= axisLen;
    } else {
        axisX = 0.0f; axisY = 0.0f; axisZ = 1.0f;
    }
    const float shadowRadius = bodyRadius * radiusScale;

    // 返回 1 = 全亮，0.06 = 落在本影里；边缘用 smoothstep 留一圈很窄的半影
    const float* m = mv;
    struct RingShade
    {
        const float* m;
        float cx, cy, cz, ax, ay, az, radius;
        float operator()(float lx, float lz) const
        {
            const float qx = m[0] * lx + m[8] * lz + m[12];
            const float qy = m[1] * lx + m[9] * lz + m[13];
            const float qz = m[2] * lx + m[10] * lz + m[14];
            const float dx = qx - cx, dy = qy - cy, dz = qz - cz;
            const float along = dx * ax + dy * ay + dz * az;
            if (along <= 0.0f) return 1.0f;          // 在行星朝向太阳的一侧
            const float px = dx - ax * along, py = dy - ay * along, pz = dz - az * along;
            const float perp = std::sqrt(px * px + py * py + pz * pz);
            return 0.06f + 0.94f * smoothstep(radius * 0.88f, radius * 1.02f, perp);
        }
    } shade = {m, centerX, centerY, centerZ, axisX, axisY, axisZ, shadowRadius};

    const int segments = 96;
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        const float angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
        const float cx = std::cos(angle);
        const float cz = std::sin(angle);
        const float t = static_cast<float>(i) / static_cast<float>(segments);

        const float sIn = shade(inner * cx, inner * cz);
        const float sOut = shade(outer * cx, outer * cz);

        glColor4f(sOut, sOut, sOut, 1.0f);
        glTexCoord2f(1.0f, t);
        glVertex3f(outer * cx, 0.0f, outer * cz);

        glColor4f(sIn, sIn, sIn, 1.0f);
        glTexCoord2f(0.0f, t);
        glVertex3f(inner * cx, 0.0f, inner * cz);
    }
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// 大气层：比本体略大的球，只画背面 + 加色混合，看起来像一层光晕
void Body::drawAtmosphere(GLUquadric* quad) const
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);   // 加色混合：只会让颜色更亮，不会变暗
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);                 // 剔除正面，只留背面

    glColor4f(atmoColor[0], atmoColor[1], atmoColor[2], atmoAlpha);
    gluSphere(quad, bodyRadius * atmoScale, 24, 12);

    glCullFace(GL_BACK);
    glEnable(GL_LIGHTING);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
