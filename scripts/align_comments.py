#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
align_comments.py - 按项目编码规范垂直对齐注释列

对指定 C 源文件(.c/.h)做三类对齐:
  1. 连续的 #define 块: 宏名/宏值/行尾注释列对齐
  2. typedef enum 块:   枚举成员与注释列对齐
  3. typedef struct 块: 结构体成员与注释列对齐

用法: python scripts/align_comments.py [文件或目录 ...]
      (默认处理 app/ framework/ 及 tools/ 下各工具包根目录)
"""

import re
import sys
from pathlib import Path

MACRO_RE = re.compile(r'^(#define\s+)(\w+)(\s+)(.+?)(\s+)(/\*\*.*?\*/)\s*$')
ENUM_MEMBER_RE = re.compile(r'^(\s+)(\w+,?)\s+(/\*\*.*?\*/)\s*$')
STRUCT_MEMBER_RE = re.compile(r'^(\s+)(.+?;)\s+(/\*\*.*?\*/)\s*$')
OPEN_ENUM_RE = re.compile(r'typedef\s+enum')
OPEN_STRUCT_RE = re.compile(r'typedef\s+struct')
CLOSE_RE = re.compile(r'^\}')


def align_macros(lines):
    """对连续的带行尾注释的 #define 块做列对齐"""
    out = []
    block = []

    def flush():
        if not block:
            return
        max_name = max(len(m.group(2)) for m in block)
        max_val = max(len(m.group(4)) for m in block)
        for m in block:
            head = m.group(1)
            name = m.group(2).ljust(max_name + 1)
            val = m.group(4).ljust(max_val + 2)
            out.append(head + name + val + m.group(6))
        block.clear()

    for line in lines:
        m = MACRO_RE.match(line)
        if m:
            block.append(m)
        else:
            flush()
            out.append(line)
    flush()
    return out


def align_members(lines, open_re, member_re):
    """对 open_re 开启的块内的成员行做注释列对齐"""
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        out.append(line)
        if open_re.search(line):
            j = i + 1
            block = []
            while j < len(lines):
                if CLOSE_RE.match(lines[j]):
                    break
                m = member_re.match(lines[j])
                if m:
                    block.append(m)
                    j += 1
                else:
                    break
            if block:
                max_field = max(len(m.group(2)) for m in block)
                for m in block:
                    out.append(m.group(1) + m.group(2).ljust(max_field + 2) + m.group(3))
                i = j
                continue
        i += 1
    return out


def process_file(path):
    text = path.read_text(encoding='utf-8')
    lines = text.split('\n')
    lines = align_macros(lines)
    lines = align_members(lines, OPEN_ENUM_RE, ENUM_MEMBER_RE)
    lines = align_members(lines, OPEN_STRUCT_RE, STRUCT_MEMBER_RE)
    new_text = '\n'.join(lines)
    if new_text != text:
        path.write_text(new_text, encoding='utf-8')
        print(f'aligned: {path}')
    else:
        print(f'no change: {path}')


def default_targets():
    """模块化布局下的默认扫描目标: app/ framework/ 及 tools/ 下各工具包根目录"""
    targets = [Path('app'), Path('framework')]
    tools_dir = Path('tools')
    if tools_dir.is_dir():
        for pkg in sorted(tools_dir.iterdir()):
            if pkg.is_dir() and (pkg.name != 'framework'):
                targets.append(pkg)
    return targets


def collect_files(args):
    files = []
    if not args:
        for d in default_targets():
            files.extend(sorted(d.glob('*.[ch]')))
        return files
    for arg in args:
        p = Path(arg)
        if p.is_dir():
            files.extend(sorted(p.glob('*.[ch]')))
        else:
            files.append(p)
    return files


def main():
    for f in collect_files(sys.argv[1:]):
        process_file(f)


if __name__ == '__main__':
    main()
