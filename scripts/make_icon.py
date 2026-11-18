#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_icon.py - 生成 ToolSets 应用图标 res/app.ico

绘制内容: 圆角方形蓝色渐变底 + 白色六边形(呼应 Hex 工具)。
纯标准库实现(无PIL依赖), 输出 16/32/48 像素 32bpp ICO。
"""

import math
import struct
from pathlib import Path

SIZES = (16, 32, 48)
GRAD_TOP = (0x6C, 0xA8, 0xFF)      # 渐变顶部 RGB
GRAD_BOTTOM = (0x2D, 0x5B, 0xE9)   # 渐变底部 RGB
HEX_COLOR = (255, 255, 255)        # 六边形填充色 RGB
BOX_RADIUS_RATIO = 0.24            # 圆角半径/边长
HEX_RADIUS_RATIO = 0.28            # 六边形外接圆半径/边长


def clamp(v, lo, hi):
    """限制数值到区间"""
    return max(lo, min(hi, v))


def rounded_box_coverage(x, y, s):
    """圆角矩形覆盖率: 1=内部, 0=外部, 区间值为抗锯齿边缘"""
    margin = 1.0 if s >= 24 else 0.5
    half = s / 2.0 - margin
    radius = BOX_RADIUS_RATIO * s
    c = s / 2.0
    dx = abs(x - c) - (half - radius)
    dy = abs(y - c) - (half - radius)
    dist = math.hypot(max(dx, 0.0), max(dy, 0.0)) + min(max(dx, dy), 0.0)
    return clamp(radius - dist + 0.5, 0.0, 1.0)


def in_hexagon(x, y, s):
    """点是否在平顶六边形内(凸多边形叉积同号法)"""
    cx, cy = s / 2.0, s / 2.0 + 0.5
    r = HEX_RADIUS_RATIO * s
    verts = []
    for k in range(6):
        ang = math.radians(30.0 + 60.0 * k)
        verts.append((cx + r * math.cos(ang), cy - r * math.sin(ang)))
    sign = 0
    for i in range(6):
        x1, y1 = verts[i]
        x2, y2 = verts[(i + 1) % 6]
        cross = ((x2 - x1) * (y - y1)) - ((y2 - y1) * (x - x1))
        if cross != 0.0:
            cur = 1 if cross > 0 else -1
            if sign == 0:
                sign = cur
            elif cur != sign:
                return False
    return True


def render(s):
    """渲染 s×s 一帧, 返回 32bpp BGRA 字节串(自底向上行序)"""
    buf = bytearray(s * s * 4)
    for row in range(s):
        y = float(s - 1 - row)
        t = clamp(y / (s - 1), 0.0, 1.0)
        base = tuple(GRAD_TOP[i] + (GRAD_BOTTOM[i] - GRAD_TOP[i]) * t for i in range(3))
        for x in range(s):
            cov = rounded_box_coverage(x + 0.5, y + 0.5, s)
            if cov <= 0.0:
                continue
            color = HEX_COLOR if in_hexagon(x + 0.5, y + 0.5, s) else base
            i = (row * s + x) * 4
            buf[i] = int(color[2])
            buf[i + 1] = int(color[1])
            buf[i + 2] = int(color[0])
            buf[i + 3] = int(255 * cov)
    return bytes(buf)


def ico_image(s):
    """打包单帧为 ICO 内的 BITMAPINFOHEADER 图像数据(含全零AND掩码)"""
    pixels = render(s)
    mask_row = (s + 7) // 8
    mask = b'\x00' * (mask_row * s)
    header = struct.pack('<IiiHHIIiiII',
                         40, s, s * 2, 1, 32, 0, len(pixels) + len(mask), 0, 0, 0, 0)
    return header + pixels + mask


def main():
    out_dir = Path(__file__).resolve().parent.parent / 'res'
    out_dir.mkdir(exist_ok=True)
    images = [ico_image(s) for s in SIZES]
    offset = 6 + 16 * len(SIZES)
    entries = b''
    for s, img in zip(SIZES, images):
        entries += struct.pack('<BBBBHHII', s, s, 0, 0, 1, 32, len(img), offset)
        offset += len(img)
    data = struct.pack('<HHH', 0, 1, len(SIZES)) + entries + b''.join(images)
    out_file = out_dir / 'app.ico'
    out_file.write_bytes(data)
    print(f'generated: {out_file} ({len(data)} bytes)')


if __name__ == '__main__':
    main()
