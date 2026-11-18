# dbgterm — 调试终端（ADB/串口数据源）设计方案

| 项目 | 内容 |
|---|---|
| 工具包 | `tools\dbgterm\`（toolsets 模块化工具包架构，纯 C Win32） |
| 版本 | v1.1 设计稿（2026-11-06，按"ADB 优先、串口第二步"重排） |
| 参考形态 | getsscom（CetSScom，Qt 多数据源调试工具：ADB logcat 接收 + 多组关键字过滤 + 分级录制）、SSCOM 系串口助手惯例 |
| 实现约束 | 纯 C + Win32 API、单 EXE 零第三方依赖、MinGW.org GCC 9.2、遵循仓库编码规范 |

---

## 1. 概述与定位

**多数据源调试终端**：外部数据流（v1.1 第一阶段为 **ADB**（adb shell / logcat），
第二阶段为**串口**）统一接入同一套"接收管线"——行装配 → 关键字过滤 → 显示/录制。

核心架构即原设计预留的"数据源抽象"，本版将其提前为第一天的基础设施：
**ADB 与串口是同一接口的两个实现**，显示/过滤/录制/发送 UI 完全复用，
未来可继续挂 TCP 等新源（getsscom 多源形态的演进路线）。

## 2. 参考调研结论

### 2.1 getsscom（CetSScom.exe）形态拆解（配置与产物分析）

| getsscom 形态 | 证据 | 本工具取舍 |
|---|---|---|
| **ADB 数据源**：`adb.exe shell` + 登录后自动执行命令 | `AdbDataSource.ini`：`AdbShell=adb.exe shell`、`LoginRun1=true`、`Command1=logcat -s yaxon` | **第一阶段核心**：同等形态（shell + 可配置自动命令，支持多条顺序执行） |
| 多组关键字过滤（6 组） | `SerialDataFilter.ini`：Key1~Key6，每组多关键字（`bj`、`yx_gsm_drv.c`、`mcu`…——即 logcat 标签/文件名） | 第一阶段实现（过滤是 ADB 场景刚需）：组名可编辑、组内多关键字、按组着色/标记 |
| 分级录制：全量/过滤/自动过滤，按天归档 | Record 文件名前缀 + 日期子目录 | 实现"全量/仅匹配"两档；`Record\yyyy-MM-dd\adb@<serial>_<时间>.txt` |
| 录制格式 `[15:13:57.430]RX←行 原文` | 日志样本 | 同形态，方向标记 ASCII 化 `[RX]/[TX]/[SYS]` |
| 串口源 | Qt5SerialPort | 第二阶段，实现同一数据源接口 |
| AI 分析（LLM API）、插件架构 | `AIAnalysis.ini`、plugins\*.dll | P2 / 不做 |

### 2.2 logcat 行形态（过滤设计的现实输入）

```
11-06 10:00:01.123  D/YaxonTag ( 1234): message text...
```

过滤默认按"子串命中"即可覆盖标签/文件名/内容三类用法（getsscom 同款）；
按组着色用于多主题并行观察（如 CAN / MCU / MQTT 各一组一色）。

## 3. 范围界定

- **第一阶段（ADB）**：adb 子进程数据源——设备枚举、连接、自动命令、流式接收、
  过滤显示、录制、文本发送（shell stdin）、配置持久化。
- **第二阶段（串口）**：serial_core 以同一数据源接口落地，补 HEX 收发、
  DTR/RTS、热插拔等串口专属功能。
- **架构预留（不实现）**：TCP/网口源、协议帧解析、AI 分析、数据转发。
- **明确不做**：插件 DLL、皮肤、多语言。

## 4. 功能形态（界面布局）

### 4.0 界面形态（v1.2 修订：单窗体 + 菜单栏 + 状态栏）

对标 SSCOM/getsscom 的**经典单窗体布局**（不再使用卡片菜单嵌套）：

```
┌──────────────────────────────────────────────────────────────┐
│ 文件(&F)  连接(&C)  视图(&V)  帮助(&H)                          │ ← 菜单栏
├──────────────────────────────────────────────────────────────┤
│ [ 连接… ] [ 断开 ] [ 清空 ]                                    │ ← 工具栏(自绘按钮行)
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  接收区 (弹性主体, 等宽只读, 自动滚动)                           │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ [✓启用过滤][组1▼][关键字...........][显示:全部▼]               │ ← 过滤行
├──────────────────────────────────────────────────────────────┤
│ [发送输入框......................][✓加回车][ 发送 ]            │ ← 发送行
├──────────────────────────────────────────────────────────────┤
│ 就绪 │ RX 0 │ TX 0 │ 过滤:关 │ 编码:UTF-8 │ ADB:未连接        │ ← 状态栏
└──────────────────────────────────────────────────────────────┘
```

- **菜单栏**：文件(清空显示/退出)、连接(ADB 连接…/断开/串口连接…占位灰)、
  视图(时间戳/暂停显示/启用过滤/显示模式三选)、帮助(关于)；
- **连接对话框**（`adb_dlg`）：ADB 路径/浏览、设备下拉/刷新、**账号/密码（掩码）**、
  自动命令×3、**失败自动重登开关 + 重登间隔（秒）**、连接/取消，
  从菜单"ADB 连接…"或工具栏 [连接…] 打开（模态）；
- **状态栏**（comctl32 statusbar，6 格）：状态文本 / RX / TX / 过滤 / 编码 / 数据源连接态；
- 串口数据源（第二阶段）= "连接"菜单新菜单项 + 新对话框，主界面管线复用不变。

延续 toolsets 自绘视觉（圆角按钮、扁平配色、ui_common 共享字体）。
**源选择区**随模式切换显示 ADB 参数组或串口参数组：

```
┌─ 调试终端 (ADB · 0123456789ABCDEF · RX 1234 · TX 0) ────────[—][□][×]┐
│ 数据源[ (●)ADB  ( )串口 ]                                             │
│ ┌ ADB 参数组 ────────────────────────────────────────────────────┐   │
│ │ adb.exe 路径[C:\...\adb.exe][浏览] 设备[ABC123 · device ▼][刷新]│   │
│ │ 自动命令[1][logcat -v time -s yaxon]  [+加一条]                  │   │
│ │                                       [ 连接 ]  [ 断开 ]        │   │
│ └────────────────────────────────────────────────────────────────┘   │
│ ┌ 串口参数组(第二阶段, ADB 模式下隐藏) ──────────────────────────┐   │
│ │ 端口[COM3▼] 波特率[115200▼] 校验[N▼] 数据位[8▼] 停止位[1▼]      │   │
│ │ 流控[无▼]                            [ 打开串口 ] [DTR] [RTS]  │   │
│ └────────────────────────────────────────────────────────────────┘   │
├──────────────────────────────────────────────┬────────────────────────┤
│ 接收区 (只读, 等宽字体, 自动滚动)               │ 发送栏1 [内容...] [发送]│
│ [10:00:01.123][RX] 11-06 10:00:01.097 D/Tag.. │         [回车✓][循环 ] │
│ [10:00:01.301][RX] ...命中行按组着色/标记...     │ 发送栏2 ...（共8条）    │
│ [10:00:02.005][TX] ls                        │ ─────────────────────  │
│                                              │ [全部循环: ▶ 500ms ▼]  │
├──────────────────────────────────────────────┴────────────────────────┤
│ 发送区: [============输入框============] [加CR/LF▼] [发送]  (串口阶段追加[HEX✓]) │
│ 选项: [时间戳✓] [解码: UTF-8▼] [过滤: 全部▼] [组: 组1▼] [录制●] [清空]   │
└────────────────────────────────────────────────────────────────────────┘
```

- 标题栏常驻：当前源与目标（`ADB · 设备序列号` / `串口 · COM3-115200`）+ 收发计数。
- 发送栏：ADB 阶段为纯文本（追加 CR/LF 可选）；串口阶段补 HEX 开关。
- 解码选项按源给默认值：ADB→UTF-8，串口→ANSI(GBK)。

### 4.1 界面布局规格（实现必须遵守）

所有控件位置由 `tool_layout()` 单点计算，禁止散落的硬编码坐标。规则：

| 规则 | 值 |
|---|---|
| 页边距 / 标签列宽 / 单行控件高 / 行距 / 同行间距 | 16 / 130 / 26 / 10 / 8 px |
| 标签列 | 每行左起 `页边距`，宽 `130`（标签文本必须完整显示，不下截断） |
| 输入控件 | 从标签列右起延伸到该行右侧锚点控件左缘 -8 |
| 右对齐锚点 | 浏览/刷新贴右缘；连接/断开贴右缘成对；发送/清空贴右缘成对 |
| 顶部行序（Y 固定） | 源说明(16) → adb路径(52) → 设备(88) → 自动命令1/2/3(124/160/196) → 开关行+连接按钮(232) |
| 底部锚定栈（自下而上） | 状态栏(贴底) ← 10 ← 过滤行(26) ← 10 ← 发送行(32) ← 10 ← 接收区底 |
| 接收区 | 弹性伸缩：顶部固定 278，底部随窗口高度由锚定栈推出（最小高 60 保护） |
| 窗口 | 初始 900×780；WM_GETMINMAXINFO 限制最小轨宽高 780×740，防止布局挤压 |
| 重排时机 | WM_CREATE（显示前，避免闪烁）与 WM_SIZE 全量重排 |
| 背景擦除 | 由窗口类背景刷(COLOR_BTNFACE)负责——本工具 WM_PAINT 只画自绘按钮，**不得**吞掉 WM_ERASEBKGND（main.c 吞除是因为其自绘全屏渐变背景，不可照抄）；窗口带 WS_CLIPCHILDREN 防拖动闪烁 |

## 5. 功能点清单（按实施阶段分组）

### 第一阶段 ADB —— M1（MVP，核心可用）

| # | 功能点 | 说明 |
|---|---|---|
| 1 | 数据源接口 | `DATA_SOURCE_T` 统一 open/close/write + RX 回调（§6.1），显示/过滤/录制管线源无关 |
| 2 | adb.exe 定位 | ini 配置路径；缺省探测 PATH；未找到时语义化提示并引导配置 |
| 3 | 设备枚举 | `adb devices` 输出解析（序列号 + device/offline/unauthorized 状态），下拉选择，手动刷新 |
| 4 | 连接/断开 | `CreateProcess("adb.exe [-s 序列号] shell")` 管道子进程（§6.3 生命周期） |
| 5 | 自动命令 | 连接后自动逐条下发（如 `logcat -v time -s yaxon`），多条顺序执行、间隔可配；命令列表可增删，随配置持久化 |
| 6 | 流式接收 | stdout 独立读线程 → 环形缓冲 → 节流通知 UI（不阻塞不丢，溢出有提示） |
| 7 | 行装配 | 跨分片行状态机（\r\n/\n/\r 归一）、UTF-8 解码、超长行截断、空闲断行 |
| 8 | 时间戳 | `[HH:MM:SS.mmm][RX]` 行前缀，可开关 |
| 9 | 关键字过滤 | 6 组可编辑（组名+关键字+组色）；三档显示：全部(命中标记/着色)/仅匹配/仅不匹配 |
| 10 | 收发计数与清空 | 标题栏实时计数；清空/复制剪贴板（复用 ui_common） |
| 11 | 文本发送 | 大发送框 + 8 条发送栏；追加 CR/LF 可选（ADB 模式无 HEX 发送） |
| 12 | 系统消息 | adb stderr 合入接收区，`[SYS]` 前缀区分（设备 offline、无权限等一目了然） |
| 13 | 暂停显示 | 只停显示追加，管线与计数照常 |

### 第一阶段 ADB —— M2（增强）

| # | 功能点 | 说明 |
|---|---|---|
| 14 | 会话录制 | 全量/仅匹配两档；`Record\yyyy-MM-dd\adb@<serial>_<HHMMSS>.txt` 按天归档，`[时间][RX/TX/SYS] 行` 格式 |
| 15 | 配置持久化 | exe 同目录 `dbgterm.ini`：adb 路径、设备、自动命令、发送栏、过滤组、各开关（GetPrivateProfile 系，无新依赖） |
| 16 | 循环发送 | 每条发送栏独立循环开关，全局周期可调（10ms~24h），单定时器扫描 |
| 17 | 显示节流 | RX 高频合并 ~100ms 刷新；显示缓冲 256KB 裁头 |

### 第二阶段 串口 —— M3（P0）

| # | 功能点 | 说明 |
|---|---|---|
| 18 | 串口数据源 | serial_core 实现 `DATA_SOURCE_T`：SERIALCOMM 枚举、参数五件套、WaitCommEvent+重叠IO 读线程、`SetCommMask(0)` 唤醒关闭（XP 兼容） |
| 19 | HEX 收发 | 收 HEX 转储 / 发 HEX 解析（复用 framework/hex_conv，见 M3 前置重构） |
| 20 | DTR/RTS | EscapeCommFunction 实时控制 |
| 21 | 解码默认值 | 串口模式默认 ANSI(GBK) |
| 22 | 真机验证清单 | 开闭 100 次/1Mbps 连续 10 分钟计数对账/拔线关闭不卡死/被占用提示 |

### 第二阶段 串口 —— M4（增强，原 P1 项）

发送文件、USB 友好名枚举、串口热插拔（WM_DEVICECHANGE）、RichEdit 按组着色升级
（ADB 阶段 P0 用 `>>K1` 前缀标记降级方案）。

### 演进项（架构预留，不排期）

TCP 源、协议帧解析、COM↔TCP 转发、AI 分析。

## 6. 设计概要

### 6.1 模块划分与数据源接口

```
tools/dbgterm/
├── dbg_source.c/.h       # 数据源接口定义 + RX 分发公共逻辑(环形缓冲/节流通知)
├── adb_core.c/.h         # ADB 数据源: 子进程管理/管道读写/设备枚举/生命周期
├── dbg_frame.c/.h        # 接收流处理(纯C): 行装配/时间戳/编码解码
├── dbg_filter.c/.h       # 关键字过滤(纯C): 6组管理/子串匹配/三档判定
├── dbg_record.c/.h       # 会话录制(纯C): 按天目录/两档落盘
├── serial_core.c/.h      # (M3) 串口数据源, 实现同一接口
├── tool_dbgterm.c/.h     # UI: 源选择区/接收区/发送栏/选项/循环发送定时器
├── entry.c               # 独立入口薄壳
├── build.bat / tests/ / DESIGN.md / README.md
```

依赖铁律：所有模块只依赖 framework；模块间仅 tool_dbgterm 编排数据源与管线。

```c
/* dbg_source.h —— 数据源统一接口 */
typedef struct DATA_SOURCE_CFG_T {
    uint8_t type;                       /* SRC_ADB / SRC_SERIAL */
    union  { ADB_CFG_T adb; SERIAL_CFG_T serial; } cfg;
    void (*pfn_rx)(const uint8_t *p_data, uint32_t len, void *p_user);
    void (*pfn_status)(uint32_t event, const wchar_t *p_msg, void *p_user);
} DATA_SOURCE_CFG_T;

typedef struct DATA_SOURCE_T {
    int32_t (*pfn_open)(const DATA_SOURCE_CFG_T *p_cfg, void **pp_impl);
    void    (*pfn_close)(void *p_impl);                     /* 幂等 */
    int32_t (*pfn_write)(void *p_impl, const uint8_t *p_data, uint32_t len);
} DATA_SOURCE_T;

const DATA_SOURCE_T *dbg_source_get(uint8_t type);          /* 取 ADB/串口实现 */
```

`pfn_rx` 在数据源内部线程上下文回调（读线程/管道读线程），实现方只做
"入环形缓冲 + 节流 PostMessage"，不做显示。

### 6.2 线程与数据流

```
[adb_core 读线程]                          [UI 线程]
  阻塞 ReadFile(stdout 管道)                ◀── WM_APP_RX (≥30ms 节流或过半即发)
    → 环形缓冲(8KB, 临界区) ── PostMessage ──┘
                                            100ms 刷新定时器:
                                              取走新字节 → dbg_frame 行装配/UTF-8解码
                                                → dbg_filter 三档判定+组标记
                                                → 显示追加(节流批量) + dbg_record 落盘
[TX] UI 线程 → pfn_write → stdin 管道 (临界区保护进程句柄生命周期)
[循环发送] UI 线程单 SetTimer(10ms) 扫描发送条到期
```

### 6.3 ADB 数据源 adb_core（第一阶段核心）

- **adb.exe 定位**：ini `adb_path` 优先 → `SearchPathW("adb.exe")` 兜底；都失败则
  连接时提示并在 UI 提供浏览选择。
- **设备枚举**：`CreateProcess("adb.exe devices", 重定向 stdout)` → 解析
  `List of devices attached` 后每行 `<serial>\t<state>`（device/offline/unauthorized），
  状态随序列号显示于下拉项。
- **连接**：`CreateProcessW("\"<adb_path>\" [-s <serial>] shell",
    CREATE_NO_WINDOW, STARTF_USESTDHANDLES)`，继承 stdout/stdin 匿名管道；
  stderr 同管道口径合入（`[SYS]` 前缀行）。`CREATE_NO_WINDOW` 防黑框。
- **登录序列**：连接成功后若配置了账号（可选密码），先按行下发账号/密码
  （每步间隔 300ms 等 shell 就绪，形态对标 getsscom User/Password），再执行自动命令。
- **自动命令**：连接成功后逐条向 stdin 写 `<命令>\r\n`，间隔 200ms（等 shell 就绪）。
- **失败自动重登**：勾选后，连接异常断开（stdout EOF 且非用户主动断开）时投递
  RELOGIN 事件并启动重登线程——按配置间隔（秒，下限 1s）清理残余句柄后重新拉起
  子进程、重发登录序列与自动命令，失败继续按间隔重试；期间状态栏显示"ADB: 重登中…"，
  用户随时可[断开]取消（user_close 标志，重登线程 200ms 步进内退出）。
- **TX**：UI 文本 + 可选 `\r\n` 写 stdin；logcat 持续输出期间发送新命令同样有效
  （同一 shell 会话）。
- **关闭时序**（防残留）：
  1. 向 stdin 写 `0x03`（Ctrl+C，中断设备侧 logcat）→ 50ms → 写 `exit\r\n`；
  2. 100ms 后若进程未退出 → `TerminateProcess`；
  3. `WaitForSingleObject` 收尸 → 关闭全部句柄。
  （设备侧 logcat 进程由 Ctrl+C 负责终止；adb server 进程属 adb 自身生命周期，不越权管理。）
- **异常形态**：读线程 `ReadFile` 返回 0/失败 = 进程退出或管道断 → 通知 UI
  `[SYS] 连接已断开` 并复位按钮状态；`adb` 未启动 server 时首次连接会有 1~3 秒延迟，
  状态栏提示"adb 服务启动中…"。

### 6.4 接收流处理 dbg_frame

- 行状态机跨分片维护半行缓冲（1KB）；`\r\n/\n/\r` 归一；≥512B 强制截断；
  空闲 ≥100ms 且半行非空视作一行。
- 解码：`MultiByteToWideChar`，源默认编码（ADB→CP_UTF8，串口→CP_ACP），
  非法字节替换 `·`。
- 行装配完成即拼可选前缀 `[HH:MM:SS.mmm][RX] `（GetTickCount 起算墙钟，格式化进行缓冲）。

### 6.5 关键字过滤 dbg_filter（第一阶段刚需）

```c
typedef struct DBG_FILTER_GROUP_T {
    wchar_t   name[32];                                 /* 组名 */
    wchar_t   keyword[DBG_FILTER_MAX_KEYWORDS][64];     /* 组内关键字 */
    uint32_t  keyword_count;
    uint32_t  color;                                    /* 组色(P1 RichEdit着色, P0映射前缀) */
    uint8_t   enabled;
} DBG_FILTER_GROUP_T;
/* 判定: 组内任一关键字子串命中(大小写不敏感)即命中; 返回 HIT(附组号)/MISS/BYPASS */
```

- 三档显示：全部（P0 命中行前缀 `>>K<组号>`，P1 RichEdit 按组色着色）/ 仅匹配 / 仅不匹配。
- 6 组缺省模板参照 getsscom `SerialDataFilter.ini` 形态（组1"关键日志"组2"驱动"…），可编辑持久化。

### 6.6 会话录制 dbg_record

`Record\yyyy-MM-dd\adb@<serial>_<HHMMSS>.txt`（第二阶段串口为
`serial@COM3[115200]_<HHMMSS>.txt`），行格式 `[HH:MM:SS.mmm][RX/TX/SYS] 内容`，
全量/仅匹配两档，4KB 缓冲定时落盘，写失败自动停录并提示。

### 6.7 UI 模块 tool_dbgterm

- 源选择区：ADB/串口单选切换，两组参数面板显隐；连接/断开按钮状态机
  （未连/连接中/已连）。
- 接收区：P0 普通 EDIT（Consolas 等宽、只读、增量 `EM_REPLACESEL` 追加、256KB 裁头）；
  P1 升 RichEdit（`LoadLibrary("riched20.dll")`，编译探针先行，缺失则维持 EDIT 方案）。
- 暂停显示：停显示不停管线。
- 多实例：窗口类注册一次 + 可重复开窗（hexascii 同模式）；独立模式 `WM_DESTROY`
  无 owner 分支 `PostQuitMessage`；连接状态下关窗先走数据源关闭时序再退出。

### 6.8 持久化 dbgterm.ini

`[Adb]`（路径/设备/自动命令1..N/间隔）、`[Send1..8]`、`[Filter1..6]`（名/关键字/色）、
`[Options]`（时间戳/解码/过滤档位/录制模式）。保存时机：断开连接与关窗时。

## 7. 测试计划

**纯C模块单测**（控制台，构建自动运行）：

| 测试文件 | 覆盖 |
|---|---|
| `tests/test_dbg_frame.c` | 分片/粘连行、三种行尾、UTF-8 解码（含非法字节）、超长截断、空闲断行、时间戳前缀 |
| `tests/test_dbg_filter.c` | 组管理、子串命中/不命中、三档判定、组号返回、空组 BYPASS |
| `tests/test_dbg_record.c` | 目录/文件名拼接、行格式、两档落盘（写临时目录验证） |
| `tests/test_adb_core.c` | **不依赖真机**：以 `cmd /c ping -t 127.0.0.1` 作为 stub 子进程验证管道读线程/节流/关闭时序（杀进程收尸） |

**ADB 真机验证清单**（M1 验收）：无 adb 提示 → 配置路径 → 设备枚举（含 unauthorized 态）
→ 连接 + 自动命令 logcat 流 → 过滤三档切换 → 发送命令生效 → 断开后设备侧 logcat 停止
→ 反复连接 20 次（进程/句柄不泄漏，任务管理器核对 adb 进程数）。

## 8. 构建与集成

- 包内 `build.bat`：照抄 hexascii 模式（vendor-on-build 同步 framework；编
  entry + 6 模块 + framework；跑单测；**纯 ASCII**）。
- 合集集成：根 `build.bat` 加源文件列表；`app/tool_manifest.c` 登记
  `L"⇅", L"调试终端", L"ADB logcat/串口 数据源接收·过滤·录制", TOOL_ACCENT_GREEN,
  tool_dbgterm_open`。

## 9. 里程碑计划

| 里程碑 | 内容 | 预估 |
|---|---|---|
| M1 | 包骨架 + 数据源接口 + adb_core（stub 管道测试）+ frame + filter + 最小 UI + 真机清单 | 3~4 天 |
| M2 | 录制 + 持久化 + 循环发送 + 显示节流 | 2 天 |
| M3 | 前置：hex_conv 提升 framework（0.5 天）；serial_core 数据源 + UI 串口参数组 + HEX 收发 + DTR/RTS + 真机清单 | 2.5~3.5 天 |
| M4 | 发送文件、串口热插拔、USB 友好名、RichEdit 按组着色 | 2 天 |

## 10. 风险与规避

| 风险 | 规避 |
|---|---|
| adb 首连启动 server 慢、进程树残留 | 连接时序含 Ctrl+C + exit + TerminateProcess 三步收尸；状态栏提示启动延迟 |
| `adb shell` 经管道无回显/无 PTY | 明确定位为"流式输出+单条命令"形态（getsscom 同款），不做全功能终端仿真 |
| logcat 高频输出冲爆 UI | 环形缓冲 + 30ms 节流通知 + 100ms 合并刷新 + 256KB 裁头 |
| 旧 MinGW 头文件缺声明 | 统一 0x0501 兼容 API；RichEdit 先做编译探针，失败降级 EDIT |
| 设备 unauthorized/offline 状态迷惑 | 枚举时状态直显于下拉项 + `[SYS]` 行透传 adb 报错 |
| 中文乱码 | 解码编码按源默认（ADB=UTF-8, 串口=GBK）且 UI 可切换 |

## 11. 明确不做（v1）

插件 DLL、皮肤主题、多语言、全功能终端仿真（PTY/ANSI 转义序列解析）、自动升级。
