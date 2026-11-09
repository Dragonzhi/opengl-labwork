#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""下载行星贴图并转换成 assets/*.bmp（24 位、未压缩、2:1 等距圆柱投影）。

为什么要转 BMP：本项目用固定管线，纹理加载是自己写的 50 行 BMP 解析器，
不引入 stb_image 之类的解码库。BMP 免解码、无第三方依赖，代价是文件偏大，
所以这里统一压到"地图上的 1 度 ≈ 1.4 像素"级别的低精度，够课程实验用。

用法（在仓库根目录执行）：
    python project/tools/fetch_textures.py

依赖：Pillow、能访问下面 URL 的网络。生成的 assets/*.bmp 是可以直接提交的
成品；没有网络时删掉它们，程序会退化成 src/gfx/texture.cpp 里的程序化贴图。
"""

import io
import os
import sys
import urllib.request

try:
    from PIL import Image
except ImportError:
    sys.exit("需要 Pillow：python -m pip install pillow")

# name: (URL, 输出宽, 高, 来源说明)
# 说明会同时写进 assets/README.md，改这里记得同步那份文档。
SOURCES = [
    ("mercury", "https://www.solarsystemscope.com/textures/download/2k_mercury.jpg",
     512, 256, "Solar System Scope（CC BY 4.0），基于 NASA MESSENGER 影像"),
    ("venus", "https://www.solarsystemscope.com/textures/download/2k_venus_atmosphere.jpg",
     512, 256, "Solar System Scope（CC BY 4.0），基于 NASA Magellan 雷达数据重建的云顶"),
    ("earth", "https://www.solarsystemscope.com/textures/download/2k_earth_daymap.jpg",
     1024, 512, "Solar System Scope（CC BY 4.0），基于 NASA Blue Marble 合成"),
    ("moon", "https://svs.gsfc.nasa.gov/vis/a000000/a004700/a004720/lroc_color_poles_1k.jpg",
     512, 256, "NASA SVS CGI Moon Kit / LRO LROC WAC 彩色全球图（公有领域）"),
    ("mars", "https://www.solarsystemscope.com/textures/download/2k_mars.jpg",
     512, 256, "Solar System Scope（CC BY 4.0），基于 NASA MGS/MOLA + Viking 影像"),
    ("jupiter", "https://www.solarsystemscope.com/textures/download/2k_jupiter.jpg",
     1024, 512, "Solar System Scope（CC BY 4.0），基于 NASA Cassini 影像"),
    ("saturn", "https://www.solarsystemscope.com/textures/download/2k_saturn.jpg",
     1024, 512, "Solar System Scope（CC BY 4.0），基于 NASA Cassini 影像"),
]

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.normpath(os.path.join(HERE, "..", "assets"))


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "opengl-labwork/1.0"})
    with urllib.request.urlopen(req, timeout=90) as resp:
        return resp.read()


def main():
    os.makedirs(ASSETS, exist_ok=True)
    for name, url, w, h, note in SOURCES:
        out = os.path.join(ASSETS, name + ".bmp")
        print("下载 %-8s <- %s" % (name, url))
        raw = fetch(url)
        img = Image.open(io.BytesIO(raw)).convert("RGB")
        if img.width != 2 * img.height:
            print("  ! 源图不是 2:1（%dx%d），仍然是等距圆柱图吗？" % img.size)
        img = img.resize((w, h), Image.LANCZOS)
        img.save(out, "BMP")
        print("  写入 %s  %dx%d  %d KB" % (out, w, h, os.path.getsize(out) // 1024))


if __name__ == "__main__":
    main()
