#include "scene.h"
#include "body.h"
#include "corona.h"
#include "../gfx/texture.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

const float PI = 3.1415926f;

// 星空：顶点只在初始化时算一次，之后每帧只提交，不重建几何
const int kStarCount = 1200;
float gStarPosition[kStarCount * 3];
float gStarColor[kStarCount * 3];

// 小行星带（火星-木星之间）与柯伊伯带（海王星外侧）：都是"一堆点"，共用一份数据结构。
// theta 是方位角、t 是径向参数（0=内缘 1=外缘）、y 是垂直于轨道面的偏移。
// 半径在每次切换演示/真实间距时重算，所以这里存的是参数而不是坐标。
struct BeltPoint
{
    float theta;
    float t;
    float y;
};

const int kAsteroidCount = 900;
const int kKuiperCount = 1400;

BeltPoint gAsteroidParams[kAsteroidCount];
BeltPoint gKuiperParams[kKuiperCount];
float gAsteroidPosition[kAsteroidCount * 3];
float gKuiperPosition[kKuiperCount * 3];
float gBeltAngle = 0.0f;        // 整条带缓慢自转，暂停时一起停

// 真实半长轴区间（天文单位）：主带 2.1~3.3，柯伊伯带 33~45
const float kAsteroidAuInner = 2.1f;
const float kAsteroidAuOuter = 3.3f;
const float kKuiperAuInner = 33.0f;
const float kKuiperAuOuter = 45.0f;

// 所有天体都是静态对象，生命周期贯穿整个进程，指针永远有效
Body gSun;
Body gMercury;
Body gVenus;
Body gEarth;
Body gMoon;
Body gMars;
Body gJupiter;
Body gSaturn;
Body gUranus;
Body gNeptune;

// 太阳外焰：不属于任何天体，是围绕太阳中心的一层独立光效
SunCorona gCorona;

// 轨道参数集中放一张表：演示间距和真实间距共用同一份角速度，
// 切换时只改半径，不需要重新配置天体
struct OrbitEntry
{
    Body* body;
    float demoRadius;   // 手调的"看得舒服"的间距
    float au;           // 真实半长轴（天文单位）
    float speed;        // 公转角速度（度/秒，演示用的，不是真实周期）
};

OrbitEntry gOrbits[] = {
    {&gMercury,  5.0f,  0.387f, 55.0f},
    {&gVenus,    7.2f,  0.723f, 40.0f},
    {&gEarth,    9.6f,  1.000f, 30.0f},
    {&gMars,    12.0f,  1.524f, 22.0f},
    {&gJupiter, 16.0f,  5.203f, 13.0f},
    {&gSaturn,  21.0f,  9.537f,  9.0f},
    {&gUranus,  26.0f, 19.190f,  6.5f},
    {&gNeptune, 31.0f, 30.070f,  5.0f}
};
const int kOrbitCount = static_cast<int>(sizeof(gOrbits) / sizeof(gOrbits[0]));

// 真实间距的映射：r = 偏移 + 缩放 * 半长轴。
// 偏移是为了让水星（0.387 AU）不至于落进半径 2 的太阳和外焰辉光里；
// 缩放取到海王星刚好落在 31.8，和演示间距的最外圈齐平，切换时画面不跳。
const float kRealOrbitOffset = 4.8f;
const float kRealOrbitScale = 0.95f;
// 真实间距下行星必须缩小，否则内行星会互相穿插
const float kRealSizeScale = 0.15f;

bool gRealScale = false;

GLUquadric* gQuad = 0;
std::vector<GLuint> gTextures;

// 聚焦目标表：顺序即键盘 1-8 的顺序，名字用于 HUD（位图字体只支持 ASCII）
struct BodyEntry
{
    const char* name;
    Body* body;
};

const BodyEntry gBodyTable[] = {
    {"Sun",     &gSun},
    {"Mercury", &gMercury},
    {"Venus",   &gVenus},
    {"Earth",   &gEarth},
    {"Moon",    &gMoon},
    {"Mars",    &gMars},
    {"Jupiter", &gJupiter},
    {"Saturn",  &gSaturn},
    {"Uranus",  &gUranus},
    {"Neptune", &gNeptune}
};
const int kBodyCount = static_cast<int>(sizeof(gBodyTable) / sizeof(gBodyTable[0]));

GLuint makeTexture(const char* name, TextureKind kind, int seed,
                   int width, int height, bool repeatT)
{
    const GLuint id = createTexture(name, kind, seed, width, height, repeatT);
    if (id != 0) {
        gTextures.push_back(id);
    }
    return id;
}

void buildStarfield()
{
    std::srand(20261109);
    for (int i = 0; i < kStarCount; ++i) {
        const float r1 = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        const float r2 = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        const float r3 = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);

        // 球面均匀采样：先取 z 均匀，再让方位角均匀
        const float theta = r1 * 2.0f * PI;
        const float z = 2.0f * r2 - 1.0f;
        const float ring = std::sqrt(1.0f - z * z);
        const float distance = 40.0f + 110.0f * r3;

        gStarPosition[i * 3 + 0] = distance * ring * std::cos(theta);
        gStarPosition[i * 3 + 1] = distance * z;
        gStarPosition[i * 3 + 2] = distance * ring * std::sin(theta);

        // 亮度按平方分布：绝大多数星很暗，只有极少数亮星，
        // 否则满屏亮点会盖过行星本身（背景喧宾夺主）
        const float b = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        const float brightness = 0.10f + 0.26f * b * b;
        gStarColor[i * 3 + 0] = brightness;
        gStarColor[i * 3 + 1] = brightness * 0.96f;
        gStarColor[i * 3 + 2] = brightness * (0.82f + 0.18f * r3);
    }
}

float rand01()
{
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

void buildBelt(BeltPoint* params, int count, int seed, float thickness)
{
    std::srand(seed);
    for (int i = 0; i < count; ++i) {
        params[i].theta = rand01() * 2.0f * PI;
        // t 用平方根分布：半径越大点越稀，内缘更密，像真实小天体盘
        const float r = rand01();
        params[i].t = 1.0f - r * r;
        // 真实的主带/柯伊伯带都很厚，这里压扁，否则点散开就看不出"带"
        params[i].y = (rand01() - 0.5f) * thickness;
    }
}

// 半径随"演示 / 真实间距"重算：演示模式在相邻两颗行星之间线性铺开，
// 真实模式直接用和行星相同的 r = 偏移 + 缩放·a(AU)
void rebuildBeltPositions()
{
    for (int i = 0; i < kAsteroidCount; ++i) {
        const float t = gAsteroidParams[i].t;
        const float radius = gRealScale
            ? (kRealOrbitOffset + kRealOrbitScale * (kAsteroidAuInner +
                                                     (kAsteroidAuOuter - kAsteroidAuInner) * t))
            : (12.9f + 1.4f * t);
        gAsteroidPosition[i * 3 + 0] = radius * std::cos(gAsteroidParams[i].theta);
        gAsteroidPosition[i * 3 + 1] = gAsteroidParams[i].y;
        gAsteroidPosition[i * 3 + 2] = radius * std::sin(gAsteroidParams[i].theta);
    }

    for (int i = 0; i < kKuiperCount; ++i) {
        const float t = gKuiperParams[i].t;
        const float radius = gRealScale
            ? (kRealOrbitOffset + kRealOrbitScale * (kKuiperAuInner +
                                                     (kKuiperAuOuter - kKuiperAuInner) * t))
            : (34.0f + 7.0f * t);
        gKuiperPosition[i * 3 + 0] = radius * std::cos(gKuiperParams[i].theta);
        gKuiperPosition[i * 3 + 1] = gKuiperParams[i].y;
        gKuiperPosition[i * 3 + 2] = radius * std::sin(gKuiperParams[i].theta);
    }
}

void buildBelts()
{
    buildBelt(gAsteroidParams, kAsteroidCount, 20261111, 0.5f);
    buildBelt(gKuiperParams, kKuiperCount, 20261110, 1.2f);
    rebuildBeltPositions();
}

void drawBelt(const float* positions, int count, float r, float g, float b)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glPointSize(1.5f);
    glColor3f(r, g, b);

    glPushMatrix();
    glRotatef(gBeltAngle, 0.0f, 1.0f, 0.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < count; ++i) {
        glVertex3fv(&positions[i * 3]);
    }
    glEnd();
    glPopMatrix();

    glPointSize(1.0f);
    glEnable(GL_LIGHTING);
}

void drawBelts()
{
    // 主带偏暖灰（岩石），柯伊伯带偏冷蓝（冰）
    drawBelt(gAsteroidPosition, kAsteroidCount, 0.50f, 0.46f, 0.40f);
    drawBelt(gKuiperPosition, kKuiperCount, 0.42f, 0.46f, 0.52f);
}

void drawStarfield()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glPointSize(1.5f);

    glBegin(GL_POINTS);
    for (int i = 0; i < kStarCount; ++i) {
        glColor3fv(&gStarColor[i * 3]);
        glVertex3fv(&gStarPosition[i * 3]);
    }
    glEnd();

    glPointSize(1.0f);
    glEnable(GL_LIGHTING);
}

// 轨道只是一圈线，不需要光照和纹理
void drawOrbitRing(float radius)
{
    const int segments = 96;
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        const float angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
        glVertex3f(radius * std::cos(angle), 0.0f, radius * std::sin(angle));
    }
    glEnd();
}

void drawOrbits()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.28f, 0.36f, 0.52f);

    drawOrbitRing(gMercury.orbitRadius());
    drawOrbitRing(gVenus.orbitRadius());
    drawOrbitRing(gEarth.orbitRadius());
    drawOrbitRing(gMars.orbitRadius());
    drawOrbitRing(gJupiter.orbitRadius());
    drawOrbitRing(gSaturn.orbitRadius());
    drawOrbitRing(gUranus.orbitRadius());
    drawOrbitRing(gNeptune.orbitRadius());

    glEnable(GL_LIGHTING);
}

void drawOpaqueBodies()
{
    glEnable(GL_LIGHTING);
    gSun.draw(gQuad);
    gMercury.draw(gQuad);
    gVenus.draw(gQuad);
    gEarth.draw(gQuad);
    gMars.draw(gQuad);
    gJupiter.draw(gQuad);
    gSaturn.draw(gQuad);
    gUranus.draw(gQuad);
    gNeptune.draw(gQuad);
}

// 透明物体统一放到最后：先不透明后透明，混合才可能正确
void drawTransparentBodies()
{
    glEnable(GL_LIGHTING);
    // 固定顺序里加进天王星：它的环和本体一样是"躺着"的，
    // 在默认视角下几乎侧对相机，只剩一条细线
    gEarth.drawTransparent(gQuad);
    gSaturn.drawTransparent(gQuad);
    gUranus.drawTransparent(gQuad);
}

// 光源位置必须在视图矩阵就位之后提交：
// glLightfv 会把位置换算成"当时的眼坐标"存进 GL 里，之后模型视图再变也不影响它
void applyLighting()
{
    const GLfloat lightPosition[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
}

void configureBodies()
{
    // 轨道半径与角速度统一由 gOrbits 提供，避免两处各写一份而不同步
    for (int i = 0; i < kOrbitCount; ++i) {
        gOrbits[i].body->setOrbit(gOrbits[i].demoRadius, gOrbits[i].speed);
    }

    // ---------- 太阳：位置在原点，靠 emission 自发光 ----------
    gSun.setShape(2.0f, 32, 24);
    gSun.setOrbit(0.0f, 0.0f);
    gSun.setSpin(4.0f, 0.0f);
    gSun.setDiffuse(0.95f, 0.72f, 0.30f);
    gSun.setEmission(1.00f, 0.90f, 0.56f);
    gSun.setShininess(8.0f);
    // 512x256：米粒组织要够细，2 的幂才能建 mipmap
    gSun.setTexture(makeTexture("sun", TEX_SUN, 7, 512, 256, false));

    // ---------- 水星 ----------
    gMercury.setShape(0.42f, 16, 12);
    gMercury.setOrbitPhase(205.0f);
    gMercury.setSpin(18.0f, 1.0f);
    gMercury.setDiffuse(0.88f, 0.88f, 0.88f);
    gMercury.setTexture(makeTexture("mercury", TEX_ROCKY, 11, 512, 256, false));

    // ---------- 金星 ----------
    gVenus.setShape(0.72f, 20, 14);
    gVenus.setOrbitPhase(285.0f);
    gVenus.setSpin(-12.0f, 2.0f);
    gVenus.setDiffuse(1.00f, 1.00f, 1.00f);
    gVenus.setTexture(makeTexture("venus", TEX_GAS, 12, 512, 256, false));

    // ---------- 地球：带大气层 ----------
    gEarth.setShape(0.80f, 28, 20);
    gEarth.setOrbitPhase(12.0f);
    gEarth.setSpin(90.0f, 23.5f);       // 90 度/秒 = 4 秒自转一圈
    gEarth.setDiffuse(0.95f, 0.95f, 0.95f);
    gEarth.setShininess(40.0f);
    gEarth.setTexture(makeTexture("earth", TEX_EARTH, 13, 1024, 512, false));
    gEarth.enableAtmosphere(1.05f, 0.32f, 0.58f, 1.00f, 0.20f);

    // ---------- 月球：地球的子天体，验证两级层级变换 ----------
    gMoon.setShape(0.22f, 12, 10);
    gMoon.setOrbit(1.55f, 80.0f);
    gMoon.setSpin(80.0f, 6.0f);         // 潮汐锁定：自转与公转同周期
    gMoon.setDiffuse(0.95f, 0.95f, 0.95f);
    gMoon.setTexture(makeTexture("moon", TEX_ROCKY, 14, 512, 256, false));
    gEarth.addChild(&gMoon);

    // ---------- 火星 ----------
    gMars.setShape(0.55f, 18, 12);
    gMars.setOrbitPhase(150.0f);
    gMars.setSpin(40.0f, 25.0f);
    gMars.setDiffuse(1.00f, 1.00f, 1.00f);
    gMars.setTexture(makeTexture("mars", TEX_ROCKY, 15, 512, 256, false));

    // ---------- 木星 ----------
    gJupiter.setShape(1.70f, 28, 20);
    gJupiter.setOrbitPhase(255.0f);
    gJupiter.setSpin(130.0f, 3.0f);
    gJupiter.setDiffuse(1.00f, 1.00f, 1.00f);
    gJupiter.setTexture(makeTexture("jupiter", TEX_GAS, 16, 1024, 512, false));

    // ---------- 土星：带环 ----------
    gSaturn.setShape(1.45f, 28, 20);
    gSaturn.setOrbitPhase(330.0f);
    gSaturn.setSpin(110.0f, 26.7f);
    gSaturn.setDiffuse(1.00f, 1.00f, 1.00f);
    gSaturn.setTexture(makeTexture("saturn", TEX_GAS, 17, 1024, 512, false));
    gSaturn.enableRing(1.12f, 2.27f, makeTexture("ring", TEX_RING, 18, 512, 8, true), 0.85f);

    // ---------- 天王星：自转轴倾角 98 度，几乎是"躺在轨道面上滚着走" ----------
    gUranus.setShape(1.05f, 24, 16);
    gUranus.setOrbitPhase(75.0f);
    gUranus.setSpin(60.0f, 97.8f);
    gUranus.setDiffuse(1.00f, 1.00f, 1.00f);
    gUranus.setTexture(makeTexture("uranus", TEX_GAS, 19, 512, 256, false));
    // 环面跟着 97.8 度的自转轴一起躺下：drawRing 画在倾角之后的局部坐标系里，
    // 所以这里不需要为"躺着"写任何额外代码
    gUranus.enableRing(1.63f, 2.05f,
                       makeTexture("uranus_ring", TEX_RING_THIN, 23, 512, 8, true));

    // ---------- 海王星 ----------
    gNeptune.setShape(1.00f, 24, 16);
    gNeptune.setOrbitPhase(190.0f);
    gNeptune.setSpin(70.0f, 28.3f);
    gNeptune.setDiffuse(1.00f, 1.00f, 1.00f);
    gNeptune.setTexture(makeTexture("neptune", TEX_GAS, 20, 512, 256, false));

}

}  // namespace

void sceneInit()
{
    // 清屏色与雾色保持一致，远景才能"融进背景"
    glClearColor(0.02f, 0.025f, 0.05f, 1.0f);

    // 线性雾：按视点距离衰减，近端故意放得比太阳系更远，只作用于远景星空
    const GLfloat fogColor[4] = {0.02f, 0.025f, 0.05f, 1.0f};
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_START, 90.0f);
    glFogf(GL_FOG_END, 380.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);   // 非均匀缩放或旋转后，法线必须重新归一化

    // ---------- 光照 ----------
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    const GLfloat lightDiffuse[4]  = {1.00f, 0.95f, 0.88f, 1.0f};
    const GLfloat lightSpecular[4] = {1.00f, 1.00f, 1.00f, 1.0f};
    const GLfloat lightAmbient[4]  = {0.00f, 0.00f, 0.00f, 1.0f};
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);

    // 全局环境光：让背光面不至于纯黑，否则行星暗面完全看不见
    const GLfloat modelAmbient[4] = {0.40f, 0.42f, 0.54f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, modelAmbient);

    // 纹理颜色与材质/光照结果相乘（默认就是 MODULATE，这里显式写出来）
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // ---------- 共用的二次曲面句柄 ----------
    gQuad = gluNewQuadric();
    gluQuadricDrawStyle(gQuad, GLU_FILL);
    gluQuadricNormals(gQuad, GLU_SMOOTH);     // 顶点法线插值 -> Gouraud 平滑
    gluQuadricTexture(gQuad, GL_TRUE);        // 让 gluSphere 生成 s/t 纹理坐标

    buildStarfield();
    buildBelts();
    configureBodies();
    gCorona.init();
}

void sceneUpdate(float deltaTime)
{
    gSun.update(deltaTime);
    gMercury.update(deltaTime);
    gVenus.update(deltaTime);
    gEarth.update(deltaTime);   // 地球的子天体（月球）在内部递归更新
    gMars.update(deltaTime);
    gJupiter.update(deltaTime);
    gSaturn.update(deltaTime);
    gUranus.update(deltaTime);
    gNeptune.update(deltaTime);

    // 外焰也按同一套时间推进：暂停时火焰一起停，比"行星停了、火还在烧"更自洽
    gCorona.update(deltaTime);

    // 柯伊伯带整体缓慢绕太阳转，让静止的场景里有一点"公转"的暗示
    gBeltAngle = std::fmod(gBeltAngle + 0.6f * deltaTime, 360.0f);
}

void sceneDraw()
{
    applyLighting();
    drawStarfield();
    drawBelts();
    drawOrbits();
    drawOpaqueBodies();
    drawTransparentBodies();

    // 外焰放在最后画：它是纯加色的光，应该叠在所有已画内容之上；
    // 深度测试仍在，挡在太阳前面的行星会正确地把光晕挡住。
    const Vec3 sunPosition = gSun.worldPosition();
    gCorona.draw(sunPosition.x, sunPosition.y, sunPosition.z, gSun.radius());
}

void sceneRelease()
{
    gCorona.release();

    for (std::size_t i = 0; i < gTextures.size(); ++i) {
        GLuint id = gTextures[i];
        releaseTexture(id);
    }
    gTextures.clear();

    if (gQuad != 0) {
        gluDeleteQuadric(gQuad);
        gQuad = 0;
    }
}

int sceneBodyCount()
{
    return kBodyCount;
}

const Body* sceneBodyAt(int index)
{
    if (index < 0 || index >= kBodyCount) return 0;
    return gBodyTable[index].body;
}

void sceneSetScaleMode(bool realScale)
{
    gRealScale = realScale;

    const float sizeScale = realScale ? kRealSizeScale : 1.0f;
    for (int i = 0; i < kOrbitCount; ++i) {
        OrbitEntry& entry = gOrbits[i];
        const float radius = realScale
            ? (kRealOrbitOffset + kRealOrbitScale * entry.au)
            : entry.demoRadius;
        entry.body->setOrbitRadius(radius);
        entry.body->setRadiusScale(sizeScale);
    }

    // 月球跟着地球一起缩，绕地半径也一起缩，否则"小地球 + 远月球"很别扭
    gMoon.setOrbitRadius(1.55f * sizeScale);
    gMoon.setRadiusScale(sizeScale);

    // 两条带也得跟着换半径，否则真实间距下它们会跑到错误的行星之间
    rebuildBeltPositions();
}

bool sceneScaleMode()
{
    return gRealScale;
}

const char* sceneBodyName(int index)
{
    if (index < 0 || index >= kBodyCount) return "";
    return gBodyTable[index].name;
}

bool sceneReloadCorona()
{
    return gCorona.reload();
}

const char* sceneCoronaStatus()
{
    return gCorona.status();
}
