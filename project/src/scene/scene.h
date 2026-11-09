#pragma once

#include <GL/freeglut.h>

// 场景层：负责建资源、按时间推进、按顺序绘制。
// 主程序只管窗口/事件/主循环，不碰场景内部细节。
void sceneInit();
void sceneUpdate(float deltaTime);
void sceneDraw();
void sceneRelease();
