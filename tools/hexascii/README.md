# hexascii — Hex ⇄ ASCII 转换工具包

toolsets 合集的一个**自包含工具包**：可独立构建运行，也可一行登记进合集主界面。
两种方式编译的是同一份模块源码，无需任何修改。

## 包内结构

```
hexascii/
├── tool_hexascii.c/.h   # 界面模块(窗口/自绘按钮/转换交互/状态栏)
├── hex_conv.c/.h        # 核心逻辑(纯C, 无界面依赖, 可单独复用)
├── entry.c              # 独立运行入口薄壳(合集版不编译)
├── build.bat            # 独立构建脚本(自动同步 framework + 运行单测)
└── tests/test_hex_conv.c
```

## 独立构建

```
build.bat
```

- 产出 `bin\hexascii.exe`（单 EXE、零第三方依赖）并自动运行单元测试；
- vendor-on-build：构建时自动从 `..\..\framework` 同步 `ui_common` 源码到包内
  `framework\`（该副本已被 gitignore）；**整个目录拷走后**，在任意 MinGW
  环境同样直接执行 `build.bat` 即可，仓库外构建使用随包携带的 framework 副本；
- 可选图标：如需自定义图标，在包内放置 `res\app.rc` 与 `res\app.ico`
  （资源 ID 固定为 1，与 `framework\ui_common.h` 的 `UI_COMMON_APP_ICON_ID` 一致）；
  无资源时正常构建，仅使用系统默认图标。

## 产物说明

| 产物 | 类型 | 用途 |
|---|---|---|
| `bin\hexascii.exe` | GUI（`-mwindows`） | **模块本体**：双击即打开 Hex ⇄ ASCII 转换窗口，可独立使用/交付 |
| `bin\test_hex_conv.exe` | 控制台 | 单元测试：供构建脚本自动运行；想看结果需在终端里执行，双击会因窗口瞬间关闭而"看不到输出" |

## 集成进 toolsets 合集

1. 在 `app\tool_manifest.c` 登记表添加一行（图标/名称/描述/主题色/`tool_hexascii_open`）；
2. 在根 `build.bat` 源文件列表加入 `tools\hexascii\tool_hexascii.c` 与 `hex_conv.c`。

## 依赖

- `framework\ui_common`（共享字体/图标/圆角矩形/剪贴板），经 vendor-on-build 同步；
- 系统库：gdi32、user32。
