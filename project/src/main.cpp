#include <GL/freeglut.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

const float PI = 3.1415926f;

/** 烟雾测试开关 */
static bool smokeTest = false;
static int testResult = 0;

// GLU 二次曲面的“绘制设置”句柄：它不保存几何，只保存绘制风格（填充/线框）、
// 法线生成方式、是否生成纹理坐标等状态，供 gluSphere / gluCylinder / gluDisk 共用。
// 用 gluNewQuadric() 创建，用 gluDeleteQuadric() 释放。
GLUquadric* quad;

static float angle = 0.0f; // 旋转角度
static bool keyA = false, keyD = false;

/** 鼠标拖动状态 */
static bool dragging = false;
static int lastMouseX = 0;
static int lastMouseY = 0;

class OrbitEye
{
private:
    float yaw;    // 方位角（度），绕 +Y 轴
    float pitch;  // 俯仰角（度），相对 XZ 平面
    float dist;   // 相机到焦点的距离

    // 守住俯仰角不变量：避免视线与 up 向量接近平行
    static float clampPitch(float value)
    {
        const float limit = 85.0f;
        if (value < -limit) return -limit;
        if (value > limit) return limit;
        return value;
    }

    // 守住方位角不变量：把角度折回 (-180, 180]
    static float wrapYaw(float value)
    {
        if (value > 180.0f) value -= 360.0f;
        if (value < -180.0f) value += 360.0f;
        return value;
    }

public:
    OrbitEye() : yaw(0.0f), pitch(14.0f), dist(8.25f) {}

    // 参数是角度增量（不是像素），鼠标灵敏度留在回调里换算
    void orbit(float deltaYawDeg, float deltaPitchDeg)
    {
        yaw = wrapYaw(yaw + deltaYawDeg);
        pitch = clampPitch(pitch + deltaPitchDeg);
    }

    // factor < 1 拉近，factor > 1 拉远
    void zoom(float factor)
    {
        dist *= factor;
        if (dist < 2.0f) dist = 2.0f;
        if (dist > 60.0f) dist = 60.0f;
    }

    void reset()
    {
        yaw = 0.0f;
        pitch = 14.0f;
        dist = 8.25f;
    }

    // 由球坐标算出 eye，再把视图矩阵提交给固定管线
    void apply() const
    {
        const float yawRad = yaw * PI / 180.0f;
        const float pitchRad = pitch * PI / 180.0f;
        const float horizontal = dist * std::cos(pitchRad);

        const float eyeX = horizontal * std::sin(yawRad);
        const float eyeY = dist * std::sin(pitchRad);
        const float eyeZ = horizontal * std::cos(yawRad);

        gluLookAt(eyeX, eyeY, eyeZ,
                  0.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f);
    }

    float yawDeg() const { return yaw; }
    float pitchDeg() const { return pitch; }
    float distance() const { return dist; }
};

class Planet
{
public:
    Planet(float radius, float orbitRadius, float r, float g, float b)
    : radius(radius), orbitRadius(orbitRadius), orbitAngle(0.0f)
    {
        color[0] = r;
        color[1] = g;
        color[2] = b;
    }

    void draw(GLUquadric* quad) const
    {
        glPushMatrix();
        glRotatef(orbitAngle, 0.0f, 1.0f, 0.0f);
        glTranslatef(orbitRadius, 0.0f, 0.0f);
        glColor3fv(color);
        gluSphere(quad, radius, 16, 8);
        glPopMatrix();
    }

private:
    float radius;
    float orbitRadius;
    float orbitAngle;   // 先固定 0，M1 再更新
    float color[3];
};

/** 对象实例化 */
static OrbitEye gEye;
static Planet Earth(0.3f, 3.0f, 0.0f, 0.5f, 1.0f);

void mouse(int button, int state, int x, int y)
{
    if (state != GLUT_DOWN) return;

    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            dragging = true;
            lastMouseX = x;
            lastMouseY = y;
        } else {
            dragging = false;
        }
        return;
    }

    if (button == 3) {
        gEye.zoom(0.9f);
        glutPostRedisplay();
    } else if (button == 4) {
        gEye.zoom(1.1f);
        glutPostRedisplay();
    }
}

void motion(int x, int y)
{
    if (dragging) {
        const float deltaX = x - lastMouseX;
        const float deltaY = y - lastMouseY;
        lastMouseX = x;
        lastMouseY = y;
        gEye.orbit(-deltaX * 0.5f, deltaY * 0.5f); // TODO 0.5 是灵敏度控制参数，目前为硬编码
        glutPostRedisplay();
    }
}

/** 绘制轨道 */
static void drawOrbit(float radius, int segments)
{
    glColor3f(0.35f, 0.45f, 0.65f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        const float a = 2.0f * PI * i / segments;
        glVertex3f(radius * std::cos(a), 0.0f, radius * std::sin(a));
    }
    glEnd();
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    gEye.apply();

    glColor3f(0.9f, 0.8f, 0.2f);
    gluSphere(quad, 1.0f, 24, 12);

    drawOrbit(3.0f, 96);
    Earth.draw(quad);
    
    if (smokeTest) {
        unsigned char pixel[3] = {};
        glReadBuffer(GL_BACK);
        glReadPixels(glutGet(GLUT_WINDOW_WIDTH)/2,
                     glutGet(GLUT_WINDOW_HEIGHT)/2, 1, 1,
                     GL_RGB, GL_UNSIGNED_BYTE, pixel);
        const GLenum error = glGetError();
        const bool ok = error == GL_NO_ERROR && pixel[0] > 30 && pixel[1] > 30 && pixel[2] > 30;
        std::printf("RENDER_TEST: %s; center RGB=%u,%u,%u; GL error=%u\n",
                    ok ? "PASS" : "FAIL", pixel[0], pixel[1], pixel[2], error);
        testResult = ok ? 0 : 2;
    }
    glutSwapBuffers();
    if (smokeTest) glutLeaveMainLoop();
}

void reshape(int width, int height)
{
    if (height < 1) height = 1;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const double aspect = static_cast<double>(width) / height;
    gluPerspective(45.0, aspect, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int, int)
{
    if (key == 27) {glutLeaveMainLoop(); return;}
    if (key == 'a' || key == 'A') keyA = true;
    if (key == 'd' || key == 'D') keyD = true;
}

void keyboardUp(unsigned char key, int, int)
{
    if (key == 'a' || key == 'A') keyA = false;
    if (key == 'd' || key == 'D') keyD = false;
}

void idle()
{
    // angle += 0.01f;
    if (keyA) angle += 0.01f;
    if (keyD) angle -= 0.01f;
    if (angle <= -360.0f || angle >= 360.0f) angle = 0.0f;
    if (keyA || keyD) glutPostRedisplay();
}

int main(int argc, char** argv)
{
    smokeTest = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;
    if (smokeTest) argc = 1;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL Class - freeglut - ESC to exit");
    
    quad = gluNewQuadric();
    gluQuadricDrawStyle(quad, GLU_FILL);
    gluQuadricNormals(quad, GLU_SMOOTH);

    glEnable(GL_DEPTH_TEST);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    std::printf("Vendor:   %s\nRenderer: %s\nVersion:  %s\n",
                glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));
    glClearColor(0.08f, 0.10f, 0.15f, 1.0f);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutIdleFunc(idle);
    glutMainLoop();
    return testResult;
}