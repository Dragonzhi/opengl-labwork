#include <GL/freeglut.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "scene/camera.h"
#include "scene/scene.h"

namespace {

bool  gSmokeTest = false;          // --smoke-test 无交互自检
int   gTestResult = 0;             // 自检结果作为进程退出码

OrbitCamera gCamera;

// 鼠标拖动状态
bool gDragging = false;
int  gLastMouseX = 0;
int  gLastMouseY = 0;

// 时间系统
int   gLastTimeMs = 0;
bool  gPaused = false;
float gTimeScale = 1.0f;

// 聚焦目标：-1 表示不聚焦（全局视角），0..sceneBodyCount()-1 对应天体表
int gFocusIndex = -1;

// 切换聚焦：index 传 -1 回到全局视角。
// 相机只改 center 和 eye，天体本身的世界坐标不动 —— 又一次"变换只改观察方式"。
void focusBody(int index)
{
    if (index < -1 || index >= sceneBodyCount()) return;

    gFocusIndex = index;
    gCamera.setFocus(index < 0 ? 0 : sceneBodyAt(index));
    glutPostRedisplay();
}

// Tab 循环：overview -> Sun -> ... -> Saturn -> overview
void cycleFocus(int step)
{
    const int count = sceneBodyCount();
    int next = gFocusIndex + step;
    if (next >= count) next = -1;
    if (next < -1) next = count - 1;
    focusBody(next);
}

// 位图字体：屏幕坐标下逐字符提交
void drawTextAt(int x, int y, const char* text)
{
    glRasterPos2i(x, y);
    for (const char* c = text; *c != '\0'; ++c) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
    }
}

// HUD 用一次性正交投影画在屏幕坐标系里，画完必须把矩阵和状态还原
void drawHud()
{
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, static_cast<double>(width), 0.0, static_cast<double>(height));

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);

    glColor3f(0.80f, 0.86f, 1.00f);
    drawTextAt(14, height - 26,
               "ESC quit  SPACE pause  +/- speed  mouse orbit/zoom  R reset  L scale  `~ view");

    char focusText[64];
    if (gFocusIndex < 0) {
        std::snprintf(focusText, sizeof(focusText), "overview");
    } else {
        std::snprintf(focusText, sizeof(focusText), "%s (%d/%d)",
                      sceneBodyName(gFocusIndex), gFocusIndex + 1, sceneBodyCount());
    }

    // 真实间距模式下在第二行末尾标出来，避免切过去之后忘了自己在哪个模式
    const char* scaleText = sceneScaleMode() ? "  REAL-DIST" : "";

    char line[192];
    if (gPaused) {
        std::snprintf(line, sizeof(line), "PAUSED  focus: %s  speed %.2fx  dist %.1f  keys 1-9/0/TAB%s",
                      focusText, gTimeScale, gCamera.distance(), scaleText);
    } else {
        std::snprintf(line, sizeof(line), "focus: %s  speed %.2fx  dist %.1f  keys 1-9/0/TAB%s",
                      focusText, gTimeScale, gCamera.distance(), scaleText);
    }
    drawTextAt(14, height - 48, line);

    // 第三行：外焰参数热重载的状态。改完 corona.cfg 按 F5，这里会显示第几次加载
    char coronaLine[128];
    std::snprintf(coronaLine, sizeof(coronaLine),
                  "F5 reload corona.cfg  |  %s", sceneCoronaStatus());
    drawTextAt(14, height - 70, coronaLine);

    glEnable(GL_FOG);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gCamera.apply();     // 先摆相机，场景所有物体的世界坐标都不动

    sceneDraw();
    drawHud();

    if (gSmokeTest) {
        // 自检契约：读后缓冲中心像素，太阳位于画面中心，因此应当命中太阳
        unsigned char pixel[3] = {};
        glReadBuffer(GL_BACK);
        glReadPixels(glutGet(GLUT_WINDOW_WIDTH) / 2,
                     glutGet(GLUT_WINDOW_HEIGHT) / 2,
                     1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);

        const GLenum error = glGetError();
        const bool ok = error == GL_NO_ERROR &&
                        pixel[0] > 30 && pixel[1] > 30 && pixel[2] > 30;
        std::printf("RENDER_TEST: %s; center RGB=%u,%u,%u; GL error=%u\n",
                    ok ? "PASS" : "FAIL", pixel[0], pixel[1], pixel[2], error);
        gTestResult = ok ? 0 : 2;
    }

    glutSwapBuffers();

    if (gSmokeTest) {
        glutLeaveMainLoop();
    }
}

void reshape(int width, int height)
{
    if (height < 1) height = 1;
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const double aspect = static_cast<double>(width) / static_cast<double>(height);
    gluPerspective(45.0, aspect, 0.1, 400.0);   // 远裁剪面要包住星空与雾

    glMatrixMode(GL_MODELVIEW);
}

void timer(int)
{
    const int now = glutGet(GLUT_ELAPSED_TIME);
    float deltaTime = static_cast<float>(now - gLastTimeMs) * 0.001f;
    gLastTimeMs = now;

    // 只钳制上限：窗口失焦回来后 dt 会突然变大，不限就会"跳帧"
    if (deltaTime > 0.1f) deltaTime = 0.1f;

    if (!gPaused) {
        sceneUpdate(deltaTime * gTimeScale);
    }

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void mouse(int button, int state, int x, int y)
{
    // 左键必须完整处理 DOWN 和 UP，否则永远松不开拖动
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            gDragging = true;
            gLastMouseX = x;
            gLastMouseY = y;
        } else {
            gDragging = false;
        }
        return;
    }

    if (state != GLUT_DOWN) return;

    // freeglut：滚轮在 mouse 回调里表现为 button 3(上)/4(下)
    if (button == 3) {
        gCamera.zoom(0.9f);
        glutPostRedisplay();
    } else if (button == 4) {
        gCamera.zoom(1.1f);
        glutPostRedisplay();
    }
}

void motion(int x, int y)
{
    if (!gDragging) return;

    const float deltaX = static_cast<float>(x - gLastMouseX);
    const float deltaY = static_cast<float>(y - gLastMouseY);
    gLastMouseX = x;
    gLastMouseY = y;

    // 屏幕横向拖动改方位角，纵向拖动改俯仰角；0.5 是灵敏度
    gCamera.orbit(-deltaX * 0.5f, deltaY * 0.5f);
    glutPostRedisplay();
}

void keyboard(unsigned char key, int, int)
{
    switch (key) {
    case 27:                                   // ESC
        glutLeaveMainLoop();
        break;
    case ' ':
        gPaused = !gPaused;
        break;
    case '+':
    case '=':
        gTimeScale = std::min(gTimeScale * 2.0f, 8.0f);
        break;
    case '-':
        gTimeScale = std::max(gTimeScale * 0.5f, 0.125f);
        break;
    case 'r':
        gCamera.reset();
        break;
    case '\t':                       // Tab：循环切换聚焦目标
        cycleFocus(1);
        break;
    case '0':                        // 海王星（第 10 个聚焦目标）
        focusBody(9);
        break;
    case '`':                        // 全景视角；` 和 ~ 是同一个键的两种状态
    case '~':
        focusBody(-1);
        break;
    case 'l':                        // 演示间距 <-> 真实间距
    case 'L':
        sceneSetScaleMode(!sceneScaleMode());
        break;
    default:
        if (key >= '1' && key <= '9') {
            focusBody(key - '1');
        }
        break;
    }
    glutPostRedisplay();
}

void special(int key, int, int)
{
    // F5：重新读 corona.cfg 并重建外焰贴图，调参不用重新编译
    if (key == GLUT_KEY_F5) {
        sceneReloadCorona();
        glutPostRedisplay();
    }
}

}  // namespace

int main(int argc, char** argv)
{
    gSmokeTest = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;
    if (gSmokeTest) argc = 1;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL Class - 3D Solar System");

    std::printf("Vendor:   %s\nRenderer: %s\nVersion:  %s\n",
                reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
                reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    sceneInit();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);

    if (!gSmokeTest) {
        gLastTimeMs = glutGet(GLUT_ELAPSED_TIME);
        glutTimerFunc(16, timer, 0);
    }

    glutMainLoop();

    sceneRelease();
    return gTestResult;
}
