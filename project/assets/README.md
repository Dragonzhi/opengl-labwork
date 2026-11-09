# assets/ —— 行星贴图

这里的 BMP 不是手绘也不是程序化噪声，是**真实探测数据合成的地图**，由
[`project/tools/fetch_textures.py`](../tools/fetch_textures.py) 下载原图后转成
24 位未压缩 BMP。运行时由 [`src/gfx/texture.cpp`](../src/gfx/texture.cpp) 里的
`loadBmp()` 直接解析，不引入任何图片解码库。

| 文件 | 尺寸 | 源文件 | 数据来源 | 许可 |
|---|---|---|---|---|
| `mercury.bmp` | 512×256 | `2k_mercury.jpg` | Solar System Scope，基于 NASA **MESSENGER** 影像 | CC BY 4.0 |
| `venus.bmp` | 512×256 | `2k_venus_atmosphere.jpg` | Solar System Scope，基于 NASA **Magellan** 雷达数据重建的云顶 | CC BY 4.0 |
| `earth.bmp` | 1024×512 | `2k_earth_daymap.jpg` | Solar System Scope，基于 NASA **Blue Marble** | CC BY 4.0 |
| `moon.bmp` | 512×256 | `lroc_color_poles_1k.jpg` | **NASA SVS**（CGI Moon Kit）／LRO LROC WAC 彩色全球图 | 公有领域 |
| `mars.bmp` | 512×256 | `2k_mars.jpg` | Solar System Scope，基于 NASA **MGS/MOLA + Viking** | CC BY 4.0 |
| `jupiter.bmp` | 1024×512 | `2k_jupiter.jpg` | Solar System Scope，基于 NASA **Cassini** 影像 | CC BY 4.0 |
| `saturn.bmp` | 1024×512 | `2k_saturn.jpg` | Solar System Scope，基于 NASA **Cassini** 影像 | CC BY 4.0 |
| `uranus.bmp` | 512×256 | `2k_uranus.jpg` | Solar System Scope，基于 NASA **Voyager 2** 影像 | CC BY 4.0 |
| `neptune.bmp` | 512×256 | `2k_neptune.jpg` | Solar System Scope，基于 NASA **Voyager 2** 影像 | CC BY 4.0 |

> 署名要求：CC BY 4.0 的七张来自 Solar System Scope（https://www.solarsystemscope.com/textures/），
> 报告中请写一句 "行星贴图：Solar System Scope（CC BY 4.0，基于 NASA 探测影像）"。
> 月球图来自 NASA SVS（LRO LROC），属公有领域，无需署名但建议标注出处。

## 为什么是 BMP

固定管线项目不引入 stb_image / libpng 这类解码库，`loadBmp()` 自己写 24 行解析：
读 54 字节文件头、按 `dataOffset` 跳到像素、BGR 转 RGB。BMP 免解码、零依赖、
格式稳定，代价是文件大（每像素 3 字节、无压缩），所以下面这条尤其重要。

## 为什么分辨率这么低

课程实验的画面里，行星直径通常只有几百像素，`gluSphere` 的细分是 16×12～28×20。
1 度经纬度 ≈ 1.4 像素的贴图（512×256 即每度 1.4 像素）在这个尺寸下已经肉眼看不出
模糊；再往上加分辨率只是把 8 MB 变成 80 MB。真要更细，改
`fetch_textures.py` 里的宽高再跑一遍即可。

## 方向约定

所有图都是**等距圆柱投影**（equirectangular）：宽高比 2:1，**上北下南**，经度 0°
（本初子午线）在图片水平方向的中点。`gluSphere` 的极点被 `glRotatef(90,1,0,0)`
转到 ±Y，所以画面里"北"朝上；如果发现某张图上下颠倒，是源图本身不是这个约定，
在 `fetch_textures.py` 里加一行 `img = img.transpose(Image.FLIP_TOP_BOTTOM)`。

## 删掉会怎样

任意一个文件缺失都不影响运行：启动时会打印
`TEXTURE: xxx.bmp 未找到，使用程序化兜底纹理`，然后用 `buildProcedural()` 现场
生成一张同尺寸的噪声贴图。这条兜底路径是刻意保留的——机房机器没网、资源没拷贝
过去时，程序仍然能跑起来。

太阳和土星环故意**没有**对应的 BMP：

- 太阳走程序化贴图，因为它和 `corona.cfg` 的外焰参数是配套调过的；
- 土星环必须有 alpha 通道（环缝要真透明），24 位 BMP 存不下，只走
  `createProceduralTexture()`。
