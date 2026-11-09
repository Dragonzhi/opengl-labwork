#pragma once

#include <GL/freeglut.h>

class Body;

// 场景层：负责建资源、按时间推进、按顺序绘制。
// 主程序只管窗口/事件/主循环，不碰场景内部细节。
void sceneInit();
void sceneUpdate(float deltaTime);
void sceneDraw();
void sceneRelease();

// 聚焦功能的查询接口：主程序拿到的只是 const Body*，不需要知道 Body 的内部结构
int sceneBodyCount();
const Body* sceneBodyAt(int index);
const char* sceneBodyName(int index);
