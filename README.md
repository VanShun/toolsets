# toolsets — Windows 轻量级工具合集

纯 C (Win32 API) 实现的 Windows 桌面小工具合集：**单 EXE、零第三方依赖、无需安装运行库**。

采用**模块化工具包架构**：每个工具位于 `tools\<名称>\` 自包含目录内，可整体拷走独立构建运行；
也可通过一行登记加入合集主界面。合集版与独立版编译的是**同一份模块源码**，仅入口文件不同。

## 功能

- **主界面**：根据工具登记表动态生成自绘圆角卡片入口（渐变背景、彩色徽章图标、悬停/按下效果、手型光标），点击卡片进入对应工具窗口，窗口高度随工具数量自适应
- **应用图标**：构建时自动生成 `res\app.ico`（蓝色渐变 + 白色六边形）并嵌入 exe，窗口与任务栏均显示
- **Hex ⇄ ASCII 转换**（首个工具包 `tools\hexascii`）：
  - 输入/粘贴文本后点击方向按钮即可转换，结果可一键复制
  - Hex 输入支持空格/逗号/分号/换行等分隔符，大小写均可，输出为大写空格分隔（等宽字体显示）
  - ASCII 侧按 UTF-8 处理，中文输入转换不乱码
  - 格式错误时提示出错字符位置（第 N 个字符），状态栏按语义着色（成功绿/错误红/提示灰）
  - 单次转换上限 8192 字节，超限有明确提示

## 目录结构

```
toolsets/
├── build.bat                  # 合集版一键构建 (MinGW GCC)
├── framework/                 # 平台公共层 —— 工具包唯一允许的外部依赖
│   ├── ui_common.c/.h         #   共享字体/应用图标/圆角矩形绘制/剪贴板
│   └── ui_menu.c/.h           #   卡片式菜单主界面组件(合集主界面与工具包功能菜单复用)
├── app/                       # 合集版专属
│   ├── main.c                 #   程序入口 + 主界面(工具卡片入口)
│   └── tool_manifest.c/.h     #   工具登记表(新增工具在此登记一行)
├── tools/                     # ★ 工具包(每个自包含, 可整体拷走独立构建)
│   └── hexascii/              #   Hex ⇄ ASCII 转换工具(详见包内 README)
│       ├── tool_hexascii.c/.h #     界面模块
│       ├── hex_conv.c/.h      #     核心逻辑(纯C, 无界面依赖)
│       ├── entry.c            #     独立运行入口(合集版不编译此文件)
│       ├── build.bat          #     独立构建: 产出 hexascii.exe + 自动跑单测
│       └── tests/             #     模块单元测试
├── scripts/                   # 开发辅助脚本
│   ├── make_icon.py           #   生成合集版应用图标 res\app.ico
│   └── align_comments.py      #   注释列对齐(编码规范辅助)
├── res/                       # 合集版资源
└── bin/                       # 合集版构建产物
```

## 架构依赖规则

```
framework  ──✗──▶ 不依赖任何 tools / app (公共层永远在最底层)
tool       ──▶ 仅依赖 framework + 本包内文件
app        ──▶ framework + 各工具 (仅经 app\tool_manifest.c 一个文件触达)
```

## 构建

依赖：MinGW GCC（已在 MinGW.org GCC 9.2.0 / Win11 验证）。

合集版（产出 `bin\toolsets.exe`，并构建运行全部工具包单元测试）：

```
build.bat
```

单独构建某个工具（开发调试 / 独立交付）：

```
tools\hexascii\build.bat
```

产出 `tools\hexascii\bin\hexascii.exe`，构建时自动运行包内单元测试。
工具包构建时自动从 `..\..\framework` 同步公共源码到包内 `framework\`
（vendor-on-build，该副本已被 gitignore）；因此 `tools\<名称>\` 整个目录
拷到任意位置均可独立构建。

## 如何新增一个工具

1. **建包**：复制现有工具包为模板（如 `tools\hexascii` → `tools\mytool`），改写界面模块、
   核心逻辑、`entry.c`（改为调用本工具的 open 函数）与 `build.bat`（产物名、源文件清单）；
   开发期间可只用自己的包内 `build.bat` 独立构建调试，不碰合集；
2. **登记**：在 `app\tool_manifest.c` 登记表加一行（图标字形/名称/描述/主题色/打开回调）；
3. **编译**：在根 `build.bat` 的源文件列表加入工具模块源文件；
4. 重跑 `build.bat`，主界面自动出现新工具卡片（窗口高度随工具数自适应）。

## 独立交付单个工具

将 `tools\<名称>\` 整个目录拷走（建议先在本机跑一次包内 `build.bat`，使 framework 副本就位），
任意装有 MinGW 的机器执行包内 `build.bat` 即得独立 EXE，零第三方依赖。

## 编码规范

全部代码遵循仓库既定 C 语言编码规范（Doxygen 注释、模块化、`_T`/`_E` 类型后缀、
宏全大写、static 最小暴露等）。提交前可运行注释列对齐工具：

```
python scripts\align_comments.py
```
