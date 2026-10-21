#include <GL/freeglut.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

// Traditional fixed-function OpenGL: no core-profile context is requested.
static bool smokeTest = false;
static int testResult = 0;
static float angle = 0.0f; // 旋转角度
static bool keyA = false, keyD = false;

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPushMatrix();
    glRotatef(angle, 0.0f, 0.0f, 1.0f);     // 围绕 X/Y/Z 轴旋转
    float t = angle * 0.0174533f;
    glBegin(GL_QUADS);
        glColor3f(0.5f + 0.5f * sinf(t), 0.5f + 0.5f * sinf(t + 2.094f), 0.5f + 0.5f * sinf(t + 4.189f)); glVertex2f(-0.7f, -0.7f); t += 1.57;
        glColor3f(0.5f + 0.5f * sinf(t), 0.5f + 0.5f * sinf(t + 2.094f), 0.5f + 0.5f * sinf(t + 4.189f)); glVertex2f(0.7f, -0.7f); t += 1.57;
        glColor3f(0.5f + 0.5f * sinf(t), 0.5f + 0.5f * sinf(t + 2.094f), 0.5f + 0.5f * sinf(t + 4.189f)); glVertex2f(0.7f, 0.7f); t += 1.57;
        glColor3f(0.5f + 0.5f * sinf(t), 0.5f + 0.5f * sinf(t + 2.094f), 0.5f + 0.5f * sinf(t + 4.189f)); glVertex2f(-0.7f, 0.7f);
    glEnd();
    glPopMatrix();
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
    if (aspect >= 1.0) gluOrtho2D(-aspect, aspect, -1.0, 1.0);
    else gluOrtho2D(-1.0, 1.0, -1.0/aspect, 1.0/aspect);
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
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL Class - freeglut - ESC to exit");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    std::printf("Vendor:   %s\nRenderer: %s\nVersion:  %s\n",
                glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));
    glClearColor(0.08f, 0.10f, 0.15f, 1.0f);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutIdleFunc(idle);
    glutMainLoop();
    return testResult;
}