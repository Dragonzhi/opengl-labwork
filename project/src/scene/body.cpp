#include "body.h"

#include <cmath>

namespace {
const float PI = 3.1415926f;
}

Body::Body()
    : bodyRadius(1.0f), slices(20), stacks(16),
      orbitDistance(0.0f), orbitAngle(0.0f), orbitSpeed(0.0f),
      spinAngle(0.0f), spinSpeed(0.0f), tiltDeg(0.0f),
      shininess(24.0f), texture(0),
      atmoScale(1.0f), atmoAlpha(0.0f),
      ringInner(1.0f), ringOuter(1.0f), ringTexture(0)
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

void Body::setOrbitPhase(float deg)
{
    orbitAngle = deg;
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
    children.push_back(child);
}

void Body::enableAtmosphere(float scale, float r, float g, float b, float alpha)
{
    atmoScale = scale;
    atmoColor[0] = r; atmoColor[1] = g; atmoColor[2] = b;
    atmoAlpha = alpha;
}

void Body::enableRing(float innerScale, float outerScale, GLuint id)
{
    ringInner = innerScale;
    ringOuter = outerScale;
    ringTexture = id;
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
    const GLfloat ambient[4]  = {diffuse[0] * 0.40f, diffuse[1] * 0.40f, diffuse[2] * 0.40f, 1.0f};
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

    // 第 1 层：公转。绕父坐标系的原点转，再沿局部 +X 平移出轨道半径
    glRotatef(orbitAngle, 0.0f, 1.0f, 0.0f);
    glTranslatef(orbitDistance, 0.0f, 0.0f);

    // 第 2 层：自转。先倾斜自转轴（绕 Z），再绕自身 Y 轴自转
    glPushMatrix();
    glRotatef(tiltDeg, 0.0f, 0.0f, 1.0f);
    glRotatef(spinAngle, 0.0f, 1.0f, 0.0f);

    applyMaterial();
    if (texture != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    gluSphere(quad, bodyRadius, slices, stacks);
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

void Body::drawTransparent(GLUquadric* quad) const
{
    if (!hasTransparent()) return;

    glPushMatrix();
    glRotatef(orbitAngle, 0.0f, 1.0f, 0.0f);
    glTranslatef(orbitDistance, 0.0f, 0.0f);
    glRotatef(tiltDeg, 0.0f, 0.0f, 1.0f);

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

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glNormal3f(0.0f, 1.0f, 0.0f);

    const int segments = 96;
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        const float angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
        const float cx = std::cos(angle);
        const float cz = std::sin(angle);
        const float t = static_cast<float>(i) / static_cast<float>(segments);

        glTexCoord2f(0.0f, t);
        glVertex3f(inner * cx, 0.0f, inner * cz);

        glTexCoord2f(1.0f, t);
        glVertex3f(outer * cx, 0.0f, outer * cz);
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
