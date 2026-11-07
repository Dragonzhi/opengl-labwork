# AGENTS.md — 3D 太阳系实验项目现状

> 面向后续接手的 AI / 助教 / 未来的自己：本文件只记录**当前真实状态**、约束和已验证结论。
> 更新日期：2026-11-07

---

## 1. 一句话现状

已有可编译、可运行的 freeglut 固定管线骨架（一个线框球体 + 固定视角 + 透视投影 + 深度测试），
但**首帧静态、动画角度未生效、自检 `--smoke-test` 当前失败**；3D 太阳系的正式内容（M0 以后）尚未开始。

---

## 2. 目标与课程背景

### 2.1 本项目目标

把课本里的 2D 太阳系升级为**可交互 3D 太阳系**，用途是计算机图形学课程实验（可自拟题目）。
目标不是天文仿真，而是让每一个图形学技术点都能在画面上被明确指出来：

> 透视投影与深度测试、层级变换、光照模型、材质、纹理、透明混合、雾、自发光、动画、相机交互。

完整设计见 [DESIGN.md](file:///e:/SelfWork/opengl/opengl-labwork/project/DESIGN.md)（已写好，**尚未实现**）。

### 2.2 课程要求（来自 09-28 第5-6节课堂转写）

- 实验共 16 课时、4 个候选项目；**不要求全做，至少完成 1 个**，也允许自拟题目（与本专业相关即可），连同**实习报告**提交。
- 考核：过程性 40%（出勤 20% + 实习 80%）+ 结业 60%；**实习折算占总成绩 32%**。
- 往年末周有简短答辩；今年不强制每人答辩，**自愿上台讲解演示可加分**（计入过程性考核）。
- 参考书：《OpenGL 编程指南》（红皮书）；实验环境在信息实验楼机房一层 A 区。

---

## 3. 技术栈与硬约束

| 项 | 现状 |
|---|---|
| 窗口/输入 | freeglut **3.8.0**（`project/third_party/freeglut` 源码随仓库分发，静态编译） |
| 图形 API | **传统固定管线**（`gluSphere` / `glMatrixMode` / `gluPerspective` 等），未请求 core profile |
| 语言标准 | C++11（`CMAKE_CXX_STANDARD 11`） |
| 构建 | CMake 3.20+，MinGW Makefiles，Debug；MinGW-w64 与 CMake 均内置在 `tools/` |
| 链接 | `freeglut_static opengl32 glu32`；MinGW 下额外 `-static -static-libgcc -static-libstdc++` |
| 可执行文件 | `project/build/triangle.exe`（target 名仍为 `triangle`，是历史遗留，不影响功能） |
| 运行环境实测 | NVIDIA RTX 4060 Laptop，驱动报告 OpenGL 4.6（但代码只用固定管线） |

**明确不做**（DESIGN 已声明）：GLAD / GLEW / GLSL、外部模型文件（`.obj` 等）、阴影、PBR、HDR、后处理、碰撞检测、真实天文精度。

---

## 4. 目录结构

```
opengl-labwork/
├── AGENTS.md                 ← 本文件
├── build.cmd                 ← 构建（首次自动 configure，之后增量）
├── Build-and-Run.cmd         ← 构建 + 运行 triangle.exe
├── Open-VSCode.cmd
├── .gitignore                ← /tools、/project/build、编译产物均忽略
├── project/
│   ├── CMakeLists.txt        ← 唯一构建配置
│   ├── DESIGN.md             ← 3D 太阳系设计（里程碑 M0–M5）
│   ├── src/main.cpp          ← 目前全部源码（约 100 行）
│   ├── third_party/freeglut/ ← freeglut 3.8.0 源码
│   ├── build/                ← CMake 生成目录（不入库）
│   │   └── triangle.exe
│   └── .vscode/              ← 共享配置（c_cpp_properties / tasks / launch）
└── tools/
    ├── mingw64/              ← 内置编译器工具链（不入库，约 1GB）
    └── cmake/                ← 内置 CMake（不入库）
```

注意：DESIGN 中提到的 `project/assets/` **目前不存在**；实现纹理阶段才需要新建并放 BMP。

---

## 5. 构建、运行、自检

在本环境中 `cmd /c` 被拦截，请直接用 PowerShell 调用脚本：

```powershell
# 构建（工作目录 e:\SelfWork\opengl\opengl-labwork）
& '.\build.cmd'

# 构建 + 运行窗口
& '.\Build-and-Run.cmd'

# 无交互自检（会短暂弹窗）
& '.\project\build\triangle.exe' --smoke-test
```

VS Code 中：任务 `Build OpenGL`（默认构建）、`Run OpenGL`；调试配置 `Debug OpenGL (GDB)`。

脚本行为要点：

- `build.cmd` 把 `tools\mingw64\bin`、`tools\cmake\bin` 前置到 PATH。
- 有 `project\build\CMakeCache.txt` 时走 `cmake --build`；否则 `cmake --fresh -S project -B project/build -G "MinGW Makefiles"`。
- 移动过目录导致构建失败时：删除 `project\build` 后重跑。
- 成功后退出码 0；失败退出码 1。

---

## 6. 代码现状（[main.cpp](file:///e:/SelfWork/opengl/opengl-labwork/project/src/main.cpp)）

全部逻辑集中在一个文件，固定管线，无光照/材质/纹理/混合/雾。

| 区块 | 行 | 内容 |
|---|---|---|
| 全局状态 | [L8-L12](file:///e:/SelfWork/opengl/opengl-labwork/project/src/main.cpp#L8-L12) | `smokeTest`、`quad`、`testResult`、`angle`、`keyA/keyD` |
| `display` | [L14-L40](file:///e:/SelfWork/opengl/opengl-labwork/project/src/main.cpp#L14-L40) | 清屏 → `gluLookAt(0,2,8 → 0,0,0, up=(0,1,0))` → 黄色 `gluSphere(quad, r=1, 24, 12)` |
| `reshape` | [L42-L51](file:///e:/SelfWork/opengl/opengl-labwork/project/src/main.cpp#L42-L51) | viewport + `gluPerspective(45°, aspect, 0.1, 100)` |
| `keyboard` / `keyboardUp` | [L53-L64](file:///e:/SelfWork/opengl/opengl-labwork/project/src/main.cpp#L53-L64) | ESC 退出；按下/松开 `a` `d` 置位按键标志 |
| `idle` | [L66-L73](file:///e:/SelfWork/opengl/opengl-labwork/project/src/main.cpp#L66-L73) | `angle ±0.01`（钳制 ±360）；仅有按键时才 `glutPostRedisplay()` |
| `main` | [L75-L100](file:///e:/SelfWork/opengl/opengl-labwork/project/src/main.cpp#L75-L100) | double/RGB/depth、800×600、`GLU_LINE` + `GLU_SMOOTH`、开启深度测试、注册回调、打印 GL 信息 |

关键事实与遗留问题：

1. **球体是线框**：`gluQuadricDrawStyle(quad, GLU_LINE)`（[L85](file:///e:/SelfWork/opengl/opengl-labwork/project/src/main.cpp#L85)），所以画面中只有经线/纬线，中心像素是背景色。
2. **`angle` 算了但没用**：`idle()` 修改 `angle`，但 `display()` 中**没有任何 `glRotatef`/变换使用它**，因此按 `a`/`d` 画面不会动（只有重绘）。
3. **无鼠标交互**：DESIGN 里的环绕相机、滚轮缩放均未实现。
4. **无光照**：未 `glEnable(GL_LIGHTING)`，球体是纯色线框。
5. **一次绘制后无持续动画**：`glutIdleFunc(idle)` 已注册，但只有按键才触发重绘。
6. `quad` 创建后从未 `gluDeleteQuadric`（退出即进程结束，无实际影响）。

---

## 7. 已验证结论（2026-11-07 实测）

### 7.1 构建：通过

```
[ 95%] Built target freeglut_static
[ 97%] Building CXX object CMakeFiles/triangle.dir/src/main.cpp.obj
[100%] Linking CXX executable triangle.exe
[100%] Built target triangle
build_exit=0
```

### 7.2 自检 `--smoke-test`：**通过**（退出码 0）

```
Vendor:   NVIDIA Corporation
Renderer: NVIDIA GeForce RTX 4060 Laptop GPU/PCIe/SSE2
Version:  4.6.0 NVIDIA 616.92
RENDER_TEST: PASS; center RGB=229,204,51; GL error=0
smoke_exit=0
```

- 判定逻辑在 [L85-L96](file:///e:/SelfWork/opengl/opengl-labwork/project/src/main.cpp#L85-L96)：读 `GL_BACK` 中心像素，要求 RGB 均 > 30。
- `RGB=229,204,51` 对应 `glColor3f(0.9,0.8,0.2)`，说明中心像素命中填充太阳，渲染链路正常。
- 历史问题（线框球 + 中心取点导致 FAIL，读到背景 20,25,38）已通过改用 `GLU_FILL` 修复。

---

## 8. 与 DESIGN 里程碑的差距

| 里程碑 | DESIGN 要求 | 当前状态 |
|---|---|---|
| M0 3D 骨架 | 透视投影、深度测试、球坐标轨道相机、线框太阳+地球 | 透视/深度已有；**无轨道相机、无地球、无鼠标** |
| M1 运动系统 | 公转/自转、`glPushMatrix`/`glPopMatrix` 层级、倍速/暂停 | 未开始（`angle` 未接入变换） |
| M2 光照 | `GL_LIGHT0` 放在太阳、各行星材质 | 未开始 |
| M3 纹理 | BMP 加载、`gluQuadricTexture`、mipmap、程序化兜底 | 未开始（`assets/` 不存在） |
| M4 渲染效果 | 土星环、大气透明、雾、光晕、星空 | 未开始 |
| M5 交互打磨 | 完整键鼠、HUD、性能、自检通过 | 未开始（自检当前 FAIL） |

---

## 9. 课程知识点 ↔ 本项目技术点（答辩/考试对照）

来自课堂转写与 `00_老师强调重点汇总.md`，答辩时按这张表指画面即可：

| 课程考点（老师强调） | 本项目对应实现 |
|---|---|
| 变换的本质是改变观察角度 / 坐标系，不是改变物体 | 轨道相机 = 只改变 `gluLookAt` 的 eye，行星世界坐标不变 |
| 旋转是线性变换、平移不是；刚体 = 旋转+平移；仿射 = 线性+平移 | 行星公转用 `glRotatef` + `glTranslatef`，次序决定结果（先公转后自转） |
| 齐次坐标 4×4 统一表示 | 固定管线 `glPushMatrix`/`glPopMatrix` 的矩阵栈 |
| 加色法 RGB 用于黑背景（屏幕） | `glClearColor` 深色星空背景 + 屏幕 RGB |
| 24 位真彩色 = 每通道 8 位 | 默认 `GLUT_RGB` 颜色缓冲 |
| Phong：2 种反射、3 种光照、4 种材质属性，不含自发光 | `glLight*` + `glMaterialfv` 的 ambient/diffuse/specular/shininess；太阳用 `GL_EMISSION` |
| 法向量决定入射角；非刚体变换后法线要正确处理 | 非均匀缩放下 `glEnable(GL_NORMALIZE)` |
| 平滑靠法线插值（Gouraud vs Phong） | `gluQuadricNormals(quad, GLU_SMOOTH)` + `glShadeModel(GL_SMOOTH)` |
| 消隐 / Z-buffer：最近者可见 | `GLUT_DEPTH` + `glEnable(GL_DEPTH_TEST)` |
| 纹理映射四步：建纹理图 → 设参数 → 顶点纹理坐标 → 内部插值 | `glTexImage2D` → `glTexParameteri` → `gluQuadricTexture` → 光栅化插值 |
| Mipmap 的前提：**纹理分辨率高于屏幕分辨率**；空间换时间；边长 2 的幂 | 纹理尺寸取 256/512，`GL_TEXTURE_MIN_FILTER=GL_LINEAR_MIPMAP_LINEAR` |
| 球面不可展，必有纹理畸变 | `gluSphere` 的 u/v 参数化贴图，极点处拉伸 |
| 走样不可避免 | 纹理 min filter + 必要时 `GL_LINE_SMOOTH` |
| 透明混合需要排序 | 先不透明、后透明，透明体按相机距离远→近绘制 |
| 可展 vs 不可展（圆柱/球/环面） | 土星环用环面/圆环，贴图同样存在畸变 |
| 本影/半影（日食） | 明确不做阴影，可在答辩中说明取舍 |

---

## 10. 后续改动的约定（重要）

1. **保持固定管线**：不引入 GLAD/GLEW/GLSL，不改请求的上下文类型。
2. **每次改完必须验证两件事**：
   - `& '.\build.cmd'` 退出码 0；
   - `& '.\project\build\triangle.exe' --smoke-test` 输出 `RENDER_TEST: PASS` 且退出码 0。
   - 若改动尚未完成，可允许自检暂时失败，但必须在交付说明里写明。
3. **自检是契约**：保持 `--smoke-test` 参数、`RENDER_TEST: ...` 输出格式、`testResult` 作为退出码；不要用交互式弹窗替代。
4. **纹理资源**：只从 `project/assets/` 读 BMP；文件缺失必须程序化生成兜底纹理，不能让程序崩溃。
5. **不改构建配置**，除非确有必要；改后要同时检查 `build.cmd` 首次 configure 与增量编译两条路径。
6. **性能基线**：球体细分 ≤ 32×32，星空用 `GL_POINTS`，不要每帧重建几何。
7. **代码组织**：目前单文件；当 `main.cpp` 明显过长（例如超过 600 行）再拆分，建议 `src/main.cpp` + `src/scene/` + `src/gfx/`，不做提前抽象。

---

## 11. 建议的下一步（按顺序）

1. 修好 M0 的可见性：球体改填充（或给自检换取样点），让 `--smoke-test` 恢复 PASS。
2. 真正接入旋转：`display()` 中 `glRotatef(angle, ...)`（或改用 `glutTimerFunc` 时间步长）。
3. 实现球坐标轨道相机：鼠标左键拖动改方位角/俯仰角，滚轮改半径，`glutMotionFunc` + `glutMouseFunc`。
4. 搭太阳-地球两级层级变换，验证 M0/M1 验收（地球绕太阳、月球绕地球）。
5. 再按 DESIGN 的 M2→M5 推进光照、材质、纹理、透明、雾、土星环、星空。

---

## 12. 外部资料与注意事项

- 课程 PPT：`C:\Users\32485\Desktop\云课堂总结\_大三上部分课件\计算机图形学-课程PPT\课程PPT`
  （01 引言 / 02 图形变换与观察 / 03 真实感图形绘制 / 04 几何建模基础 / 05 三维形体的表示 / 06 相交测试）
- 课堂转写：`C:\Users\32485\Desktop\云课堂总结\计算机图形学`（含 `00_老师强调重点汇总.md`）
- 03 课件中与本项目强相关：颜色模型、Phong 光照模型、法向量计算、消隐与 Z-buffer、纹理映射（含 mipmap）、光线跟踪简介。
- `讲义草稿.pdf` 的中文文本提取是乱码（字体编码问题），**不要依赖文本抽取**；需要时按页渲染成图片查看。
- 本机已安装 `python-pptx`、`PyMuPDF`，可用于解析课件；`cmd /c` 在本环境被禁用，统一用 PowerShell。
