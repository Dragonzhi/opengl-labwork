#include "scene.h"
#include "body.h"
#include "corona.h"
#include "../gfx/texture.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

// 点数上限：数组按上限静态分配，scene.cfg 里的 count 只能在 [0, 上限] 之间调
const int kAsteroidMax = 2000;
const int kKuiperMax = 2400;

BeltPoint gAsteroidParams[kAsteroidMax];
BeltPoint gKuiperParams[kKuiperMax];
float gAsteroidPosition[kAsteroidMax * 3];
float gKuiperPosition[kKuiperMax * 3];
float gBeltAngle = 0.0f;        // 整条带缓慢自转，暂停时一起停

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
    float speed;        // 演示用公转角速度（度/秒），不是真实周期
    float ecc;          // 真实偏心率
    float inc;          // 真实轨道倾角（度，相对黄道）
    float radiusKm;     // 真实赤道半径（km），真实比例模式用它决定显示大小
};

OrbitEntry gOrbits[] = {
    {&gMercury,  5.0f,  0.387f, 55.0f, 0.206f, 7.00f,  2440.0f},
    {&gVenus,    7.2f,  0.723f, 40.0f, 0.007f, 3.39f,  6052.0f},
    {&gEarth,    9.6f,  1.000f, 30.0f, 0.017f, 0.00f,  6371.0f},
    {&gMars,    12.0f,  1.524f, 22.0f, 0.093f, 1.85f,  3390.0f},
    // 木星往外的四颗要比"真实距离"更靠外：演示里行星半径被放大了上千倍，
    // 木星 1.70 的半径在这个尺度上就是小半个天文单位的"实体"，
    // 按原来 16.0 放，近日点盘面会伸进小行星带里。
    {&gJupiter, 18.0f,  5.203f, 13.0f, 0.048f, 1.30f, 69911.0f},
    {&gSaturn,  23.6f,  9.537f,  9.0f, 0.054f, 2.49f, 58232.0f},
    {&gUranus,  29.0f, 19.190f,  6.5f, 0.047f, 0.77f, 25362.0f},
    {&gNeptune, 33.0f, 30.070f,  5.0f, 0.009f, 1.77f, 24622.0f}
};
const int kOrbitCount = static_cast<int>(sizeof(gOrbits) / sizeof(gOrbits[0]));

// 真实间距的映射：r = 偏移 + 缩放 * 半长轴。
// 偏移是为了让水星（0.387 AU）不至于落进半径 2 的太阳和外焰辉光里；
// 缩放取到海王星刚好落在 31.8，和演示间距的最外圈齐平，切换时画面不跳。
// 真实间距映射：r = 偏移 + 缩放 · a(AU)。两个数都能在 scene.cfg 里改
const float kRealOrbitOffsetDefault = 4.8f;
const float kRealOrbitScaleDefault = 0.95f;
// 真实间距下行星必须缩小，否则内行星会互相穿插
const float kRealSizeScaleDefault = 0.15f;

bool gRealScale = false;

// ------------------------------------------------------------------
// scene.cfg 里的全部可调参数。
// 改完保存，在窗口里按 F5 立即生效：不用重新编译，也不用关窗口。
// 加新参数只要在这里加字段 + 在 applySceneKey 里加一行。
// ------------------------------------------------------------------
struct SceneParams
{
    float spacing;            // 行星间距整体缩放（只放大轨道，不放大行星本身）
    float overviewDistance;   // 全局视角基准距离，实际 = 这个值 × spacing

    float shadowStrength;     // 环影强度倍数（0 = 关掉环投在行星上的影子）
    float shadowFeather;      // 环影软边宽度，单位是屏幕像素（不是环宽的比例）
    int   shadowSubdiv;       // 环影球面细分倍数

    int   asteroidCount;
    float asteroidDemoInner, asteroidDemoOuter;   // 演示间距下的半径区间
    float asteroidAuInner, asteroidAuOuter;       // 真实半长轴区间（AU）
    float asteroidColor[3];
    float asteroidPointSize;

    int   kuiperCount;
    float kuiperDemoInner, kuiperDemoOuter;
    float kuiperAuInner, kuiperAuOuter;
    float kuiperColor[3];
    float kuiperPointSize;

    float ambient[3];         // 全局环境光
    float fogStart, fogEnd;

    float orbitEccentricity;  // 偏心率缩放：1 = 真实椭圆，0 = 全画成正圆
    float orbitInclination;   // 倾角缩放：1 = 真实倾角，0 = 全压成共面
    int   keplerPeriods;      // 1 = 公转角速度按开普勒第三定律（T ∝ a^1.5）
    float realOrbitOffset;    // 真实间距映射的偏移
    float realOrbitScale;     // 真实间距映射的缩放
    float realEarthRadius;    // 真实比例下"地球"的显示半径，其余行星按真实半径比例换算
    float realSunLimit;       // 太阳显示半径的上限（× 水星轨道半径）

    SceneParams()
        : spacing(1.0f), overviewDistance(68.0f),
          shadowStrength(1.0f), shadowFeather(2.5f), shadowSubdiv(4),
          asteroidCount(900), asteroidDemoInner(13.9f), asteroidDemoOuter(15.0f),
          asteroidAuInner(2.1f), asteroidAuOuter(3.3f),
          asteroidPointSize(1.5f),
          kuiperCount(1400), kuiperDemoInner(35.5f), kuiperDemoOuter(42.5f),
          kuiperAuInner(33.0f), kuiperAuOuter(45.0f),
          kuiperPointSize(1.5f),
          fogStart(90.0f), fogEnd(380.0f),
          orbitEccentricity(1.0f), orbitInclination(1.0f), keplerPeriods(0),
          realOrbitOffset(kRealOrbitOffsetDefault),
          realOrbitScale(kRealOrbitScaleDefault),
          realEarthRadius(0.02f), realSunLimit(0.18f)
    {
        asteroidColor[0] = 0.30f; asteroidColor[1] = 0.28f; asteroidColor[2] = 0.24f;
        kuiperColor[0] = 0.42f;   kuiperColor[1] = 0.46f;   kuiperColor[2] = 0.52f;
        ambient[0] = 0.40f;       ambient[1] = 0.42f;       ambient[2] = 0.54f;
    }
};

SceneParams gParams;

// scene.cfg 的状态，HUD 上显示"第几次加载、读进来几个键"
char gConfigStatus[96] = "scene.cfg 未找到";
int gConfigReloadCount = 0;
int gConfigKeyCount = 0;

float clampF(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int clampI(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

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
    for (int i = 0; i < kAsteroidMax; ++i) {
        const float t = gAsteroidParams[i].t;
        const float radius = gRealScale
            ? (gParams.realOrbitOffset + gParams.realOrbitScale * (gParams.asteroidAuInner +
                                                     (gParams.asteroidAuOuter - gParams.asteroidAuInner) * t))
            : (gParams.asteroidDemoInner +
               (gParams.asteroidDemoOuter - gParams.asteroidDemoInner) * t) * gParams.spacing;
        gAsteroidPosition[i * 3 + 0] = radius * std::cos(gAsteroidParams[i].theta);
        gAsteroidPosition[i * 3 + 1] = gAsteroidParams[i].y;
        gAsteroidPosition[i * 3 + 2] = radius * std::sin(gAsteroidParams[i].theta);
    }

    for (int i = 0; i < kKuiperMax; ++i) {
        const float t = gKuiperParams[i].t;
        const float radius = gRealScale
            ? (gParams.realOrbitOffset + gParams.realOrbitScale * (gParams.kuiperAuInner +
                                                     (gParams.kuiperAuOuter - gParams.kuiperAuInner) * t))
            : (gParams.kuiperDemoInner +
               (gParams.kuiperDemoOuter - gParams.kuiperDemoInner) * t) * gParams.spacing;
        gKuiperPosition[i * 3 + 0] = radius * std::cos(gKuiperParams[i].theta);
        gKuiperPosition[i * 3 + 1] = gKuiperParams[i].y;
        gKuiperPosition[i * 3 + 2] = radius * std::sin(gKuiperParams[i].theta);
    }
}

void buildBelts()
{
    buildBelt(gAsteroidParams, kAsteroidMax, 20261111, 0.5f);
    buildBelt(gKuiperParams, kKuiperMax, 20261110, 1.2f);
    rebuildBeltPositions();
}

void drawBelt(const float* positions, int count, const float* color, float pointSize)
{
    if (count <= 0) return;
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glPointSize(pointSize);
    glColor3f(color[0], color[1], color[2]);

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
    // 主带偏暖灰（岩石），柯伊伯带偏冷蓝（冰）；颜色和点数都来自 scene.cfg
    drawBelt(gAsteroidPosition, gParams.asteroidCount, gParams.asteroidColor,
             gParams.asteroidPointSize);
    drawBelt(gKuiperPosition, gParams.kuiperCount, gParams.kuiperColor,
             gParams.kuiperPointSize);
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

// 轨道只是一圈线，不需要光照和纹理。
// 现在按椭圆 + 倾角采样：轨道线必须和天体实际走的位置用同一个函数，
// 否则线在一边、行星在另一边（这正是 §15 那个 bug 的另一种形态）
void drawOrbitRing(const Body& body)
{
    const int segments = 180;
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        const float meanDeg = 360.0f * static_cast<float>(i) / static_cast<float>(segments);
        const Vec3 p = body.orbitPointAt(meanDeg);
        glVertex3f(p.x, p.y, p.z);
    }
    glEnd();
}

void drawOrbits()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.28f, 0.36f, 0.52f);

    drawOrbitRing(gMercury);
    drawOrbitRing(gVenus);
    drawOrbitRing(gEarth);
    drawOrbitRing(gMars);
    drawOrbitRing(gJupiter);
    drawOrbitRing(gSaturn);
    drawOrbitRing(gUranus);
    drawOrbitRing(gNeptune);

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

// 把 ocbit 表里的偏心率/倾角/角速度套到天体上。
// scene.cfg 改了缩放系数或开普勒开关之后要再调一次。
void applyOrbitShape()
{
    for (int i = 0; i < kOrbitCount; ++i) {
        OrbitEntry& e = gOrbits[i];
        e.body->setOrbitShape(e.ecc * gParams.orbitEccentricity,
                              e.inc * gParams.orbitInclination);

        // 开普勒第三定律：周期 T ∝ a^1.5，所以角速度 ∝ a^-1.5。
        // 以地球 30 度/秒为基准，海王星就会慢到 0.18 度/秒——"几乎不动"正是真实的样子
        float speed = e.speed;
        if (gParams.keplerPeriods != 0) {
            const float au = (e.au > 0.01f) ? e.au : 1.0f;
            speed = 30.0f * std::pow(1.0f / au, 1.5f);
        }
        e.body->setOrbitSpeed(speed);
    }
}

void configureBodies()
{
    // 轨道半径、偏心率、倾角、角速度统一由 gOrbits 提供，避免两处各写一份而不同步
    for (int i = 0; i < kOrbitCount; ++i) {
        gOrbits[i].body->setOrbitRadius(gOrbits[i].demoRadius * gParams.spacing);
    }
    applyOrbitShape();

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

    // 读 scene.cfg：环境光、雾、间距、环影、小天体带的参数都在那里。
    // 必须在 configureBodies 之前，轨道半径要用到里面的 spacing
    sceneReloadConfig();

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

    for (int i = 0; i < kOrbitCount; ++i) {
        OrbitEntry& entry = gOrbits[i];
        const float radius = realScale
            ? (gParams.realOrbitOffset + gParams.realOrbitScale * entry.au)
            : entry.demoRadius * gParams.spacing;
        entry.body->setOrbitRadius(radius);

        if (realScale) {
            // 真实模式：显示半径按真实半径比例（以地球为基准），而不是所有行星乘同一个系数
            const float display = entry.radiusKm / 6371.0f * gParams.realEarthRadius;
            entry.body->setRadiusScale(display / entry.body->baseRadius());
        } else {
            entry.body->setRadiusScale(1.0f);
        }
    }

    // 太阳：真实比例下太阳是地球的 109 倍，按真实值画会正好吞掉水星轨道，
    // 所以再压一个上限（realSunLimit × 水星轨道半径），保证内圈还看得见
    if (realScale) {
        const float mercuryOrbit = gParams.realOrbitOffset + gParams.realOrbitScale * gOrbits[0].au;
        float sunDisplay = 109.0f * gParams.realEarthRadius;
        const float limit = gParams.realSunLimit * mercuryOrbit;
        if (sunDisplay > limit) sunDisplay = limit;
        gSun.setRadiusScale(sunDisplay / gSun.baseRadius());
    } else {
        gSun.setRadiusScale(1.0f);
    }

    // 月球：半径按真实比例，绕地半径取"别钻进地球里"的下限
    if (realScale) {
        const float moonDisplay = 1737.0f / 6371.0f * gParams.realEarthRadius;
        gMoon.setRadiusScale(moonDisplay / gMoon.baseRadius());
        float moonOrbit = 0.00257f * gParams.realOrbitScale;
        const float minOrbit = 2.8f * (6371.0f / 6371.0f * gParams.realEarthRadius);
        if (moonOrbit < minOrbit) moonOrbit = minOrbit;
        gMoon.setOrbitRadius(moonOrbit);
    } else {
        gMoon.setRadiusScale(1.0f);
        gMoon.setOrbitRadius(1.55f);
    }

    // 两条带也得跟着换半径，否则真实间距下它们会跑到错误的行星之间
    rebuildBeltPositions();
}

bool sceneScaleMode()
{
    return gRealScale;
}

void sceneSetSpacingScale(float scale)
{
    gParams.spacing = clampF(scale, 0.8f, 2.0f);

    // 真实间距模式下轨道半径由天文单位算，间距缩放不参与
    if (!gRealScale) {
        for (int i = 0; i < kOrbitCount; ++i) {
            gOrbits[i].body->setOrbitRadius(gOrbits[i].demoRadius * gParams.spacing);
        }
    }
    rebuildBeltPositions();
}

float sceneSpacingScale()
{
    return gParams.spacing;
}

float sceneOverviewDistance()
{
    // 真实模式：要让海王星整圈都落在视锥里。45 度竖直角 + 4:3 下，
    // 横向半宽 ≈ dist × tan(22.5°) × 1.333 ≈ 0.552 × dist，取 1.85 留一点余量
    if (gRealScale) {
        return (gParams.realOrbitOffset + gParams.realOrbitScale * 30.07f) * 1.85f;
    }
    // 演示模式下跟着间距一起放大，行星拉开多少、相机就退多少
    return gParams.overviewDistance * gParams.spacing;
}

// ================= scene.cfg =================
namespace {

const char* const kSceneConfigPaths[] = {
    "scene.cfg",
    "../scene.cfg",
    "project/scene.cfg"
};
const int kSceneConfigPathCount = 3;

char* trimText(char* s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
    char* end = s + std::strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\r' || end[-1] == '\n')) {
        --end;
    }
    *end = '\0';
    return s;
}

// 一行里可能有多个数（颜色的 r g b），最多读 3 个
int parseFloats(const char* text, float* out, int maxCount)
{
    int n = 0;
    char* p = const_cast<char*>(text);
    while (n < maxCount) {
        char* end = 0;
        const double value = std::strtod(p, &end);
        if (end == p) break;
        out[n++] = static_cast<float>(value);
        p = end;
    }
    return n;
}

bool applySceneKey(const char* key, const float* v, int n)
{
    const float a = (n > 0) ? v[0] : 0.0f;

    if (std::strcmp(key, "spacing") == 0) {
        gParams.spacing = clampF(a, 0.8f, 2.0f); return true;
    }
    if (std::strcmp(key, "overviewDistance") == 0) {
        gParams.overviewDistance = clampF(a, 20.0f, 160.0f); return true;
    }
    if (std::strcmp(key, "ringShadowStrength") == 0) {
        gParams.shadowStrength = clampF(a, 0.0f, 2.0f); return true;
    }
    if (std::strcmp(key, "ringShadowFeather") == 0) {
        gParams.shadowFeather = clampF(a, 0.25f, 4.0f); return true;
    }
    if (std::strcmp(key, "ringShadowSubdiv") == 0) {
        gParams.shadowSubdiv = clampI(static_cast<int>(a + 0.5f), 1, 8); return true;
    }

    if (std::strcmp(key, "asteroidCount") == 0) {
        gParams.asteroidCount = clampI(static_cast<int>(a + 0.5f), 0, kAsteroidMax); return true;
    }
    if (std::strcmp(key, "asteroidDemoInner") == 0) {
        gParams.asteroidDemoInner = clampF(a, 1.0f, 60.0f); return true;
    }
    if (std::strcmp(key, "asteroidDemoOuter") == 0) {
        gParams.asteroidDemoOuter = clampF(a, 1.0f, 60.0f); return true;
    }
    if (std::strcmp(key, "asteroidAuInner") == 0) {
        gParams.asteroidAuInner = clampF(a, 1.5f, 4.0f); return true;
    }
    if (std::strcmp(key, "asteroidAuOuter") == 0) {
        gParams.asteroidAuOuter = clampF(a, 1.5f, 6.0f); return true;
    }
    if (std::strcmp(key, "asteroidColor") == 0 && n >= 3) {
        for (int i = 0; i < 3; ++i) gParams.asteroidColor[i] = clampF(v[i], 0.0f, 1.0f);
        return true;
    }
    if (std::strcmp(key, "asteroidPointSize") == 0) {
        gParams.asteroidPointSize = clampF(a, 0.5f, 6.0f); return true;
    }

    if (std::strcmp(key, "kuiperCount") == 0) {
        gParams.kuiperCount = clampI(static_cast<int>(a + 0.5f), 0, kKuiperMax); return true;
    }
    if (std::strcmp(key, "kuiperDemoInner") == 0) {
        gParams.kuiperDemoInner = clampF(a, 10.0f, 120.0f); return true;
    }
    if (std::strcmp(key, "kuiperDemoOuter") == 0) {
        gParams.kuiperDemoOuter = clampF(a, 10.0f, 140.0f); return true;
    }
    if (std::strcmp(key, "kuiperAuInner") == 0) {
        gParams.kuiperAuInner = clampF(a, 28.0f, 60.0f); return true;
    }
    if (std::strcmp(key, "kuiperAuOuter") == 0) {
        gParams.kuiperAuOuter = clampF(a, 28.0f, 120.0f); return true;
    }
    if (std::strcmp(key, "kuiperColor") == 0 && n >= 3) {
        for (int i = 0; i < 3; ++i) gParams.kuiperColor[i] = clampF(v[i], 0.0f, 1.0f);
        return true;
    }
    if (std::strcmp(key, "kuiperPointSize") == 0) {
        gParams.kuiperPointSize = clampF(a, 0.5f, 6.0f); return true;
    }

    if (std::strcmp(key, "ambient") == 0 && n >= 3) {
        for (int i = 0; i < 3; ++i) gParams.ambient[i] = clampF(v[i], 0.0f, 1.0f);
        return true;
    }
    if (std::strcmp(key, "fogStart") == 0) {
        gParams.fogStart = clampF(a, 10.0f, 1000.0f); return true;
    }
    if (std::strcmp(key, "fogEnd") == 0) {
        gParams.fogEnd = clampF(a, 20.0f, 2000.0f); return true;
    }

    if (std::strcmp(key, "orbitEccentricity") == 0) {
        gParams.orbitEccentricity = clampF(a, 0.0f, 1.0f); return true;
    }
    if (std::strcmp(key, "orbitInclination") == 0) {
        gParams.orbitInclination = clampF(a, 0.0f, 3.0f); return true;
    }
    if (std::strcmp(key, "keplerPeriods") == 0) {
        gParams.keplerPeriods = (a > 0.5f) ? 1 : 0; return true;
    }
    if (std::strcmp(key, "realOrbitOffset") == 0) {
        gParams.realOrbitOffset = clampF(a, 0.0f, 60.0f); return true;
    }
    if (std::strcmp(key, "realOrbitScale") == 0) {
        gParams.realOrbitScale = clampF(a, 0.05f, 5.0f); return true;
    }
    if (std::strcmp(key, "realEarthRadius") == 0) {
        gParams.realEarthRadius = clampF(a, 0.002f, 0.3f); return true;
    }
    if (std::strcmp(key, "realSunLimit") == 0) {
        gParams.realSunLimit = clampF(a, 0.02f, 1.0f); return true;
    }

    return false;   // 不认识的键直接忽略，不报错
}

bool loadSceneConfig(const char* path)
{
    std::FILE* fp = std::fopen(path, "r");
    if (fp == 0) return false;

    char line[256];
    while (std::fgets(line, sizeof(line), fp) != 0) {
        char* hash = std::strchr(line, '#');
        if (hash != 0) *hash = '\0';

        char* equals = std::strchr(line, '=');
        if (equals == 0) continue;
        *equals = '\0';

        char* key = trimText(line);
        char* value = trimText(equals + 1);

        float numbers[3] = {0.0f, 0.0f, 0.0f};
        const int count = parseFloats(value, numbers, 3);
        if (count > 0 && applySceneKey(key, numbers, count)) {
            ++gConfigKeyCount;
        }
    }

    std::fclose(fp);
    std::printf("SCENE: 读取 %s，应用 %d 项参数\n", path, gConfigKeyCount);
    return true;
}

}  // namespace

bool sceneReloadConfig()
{
    // 先回到内置默认值：cfg 里删掉的行才会真的恢复默认，而不是沿用上次的值
    gParams = SceneParams();
    gConfigKeyCount = 0;
    ++gConfigReloadCount;

    bool found = false;
    for (int i = 0; i < kSceneConfigPathCount && !found; ++i) {
        found = loadSceneConfig(kSceneConfigPaths[i]);
    }

    // 应用：环影质量 / 环境光 / 雾 / 小天体带 + 行星间距
    setRingShadowTuning(gParams.shadowStrength, gParams.shadowFeather, gParams.shadowSubdiv);

    const GLfloat ambient[4] = {gParams.ambient[0], gParams.ambient[1],
                                gParams.ambient[2], 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    glFogf(GL_FOG_START, gParams.fogStart);
    glFogf(GL_FOG_END, gParams.fogEnd);

    sceneSetSpacingScale(gParams.spacing);
    applyOrbitShape();   // 偏心率/倾角缩放、开普勒周期都在这里生效

    std::snprintf(gConfigStatus, sizeof(gConfigStatus), "scene.cfg #%d: %d keys%s",
                  gConfigReloadCount, gConfigKeyCount,
                  found ? "" : " (缺失,用默认)");
    return found;
}

const char* sceneConfigStatus()
{
    return gConfigStatus;
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
