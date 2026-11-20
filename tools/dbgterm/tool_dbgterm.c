/**
 * @file    tool_dbgterm.c
 * @author  herongshun
 * @date    2026-11-19
 * @brief   调试终端主窗口实现(单窗体形态, 对标 SSCOM/getsscom)
 *          菜单栏(文件/连接/视图/帮助) + 接收区(RichEdit, 多彩关键字着色)
 *          + 发送行(自绘发送按钮) + 状态栏(6格)
 *          连接/断开经连接菜单; 清空显示在视图菜单; 串口为占位项(第二阶段)
 */

#include <windows.h>
#include <commctrl.h>
#include <stdint.h>
#include <string.h>
#include "ui_common.h"
#include "dbg_source.h"
#include "adb_core.h"
#include "adb_dlg.h"
#include "dbg_frame.h"
#include "dbg_filter.h"
#include "filter_view.h"
#include "color_view.h"
#include "dbg_conf.h"
#include "dbg_save.h"
#include "version.h"
#include "tool_dbgterm.h"

/* -------------------------------------------------------------------------- */
/*  宏定义: 控件ID                                                             */
/* -------------------------------------------------------------------------- */
#define TOOL_DBG_IDC_ED_RX         2010u  /**< 接收区(RichEdit/EDIT只读) */
#define TOOL_DBG_IDC_ED_TX         2011u  /**< 发送输入框 */
#define TOOL_DBG_IDC_CK_APPEND     2015u  /**< 发送追加回车开关(发送行内) */

/* 自绘按钮逻辑ID */
#define TOOL_DBG_IDC_BTN_SEND      2104u  /**< 发送 */

/* 菜单项ID */
#define TOOL_DBG_MID_CLEAR         3001u  /**< 视图-清空显示 */
#define TOOL_DBG_MID_EXIT          3002u  /**< 文件-退出 */
#define TOOL_DBG_MID_ADB_CONNECT   3003u  /**< 连接-ADB连接… */
#define TOOL_DBG_MID_DISCONNECT    3004u  /**< 连接-断开连接 */
#define TOOL_DBG_MID_TS            3005u  /**< 视图-时间戳 */
#define TOOL_DBG_MID_PAUSE         3006u  /**< 视图-暂停显示 */
#define TOOL_DBG_MID_FILTERWIN     3012u  /**< 视图-过滤窗口(独立) */
#define TOOL_DBG_MID_STRIPDEV      3016u  /**< 视图-精简设备时间戳 */
#define TOOL_DBG_MID_COLORWIN      3017u  /**< 视图-多彩关键字(独立配置窗) */
#define TOOL_DBG_MID_ABOUT         3011u  /**< 帮助-关于 */
#define TOOL_DBG_MID_SAVE_NOW      3013u  /**< 文件-立即保存 */
#define TOOL_DBG_MID_AUTOSAVE      3014u  /**< 文件-自动保存到文件 */
#define TOOL_DBG_MID_SAVE_DIR      3015u  /**< 文件-设置保存文件夹 */
#define TOOL_DBG_MID_CACHE_BASE    3020u  /**< 视图-接收缓存子菜单基址(3020+idx) */
#define TOOL_DBG_CACHE_COUNT       5u     /**< 接收缓存档位数 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 主题颜色 (0x00BBGGRR)                                              */
/* -------------------------------------------------------------------------- */
#define TOOL_DBG_CLR_ACCENT         0x00F6823B  /**< 主题蓝 #3B82F6 */
#define TOOL_DBG_CLR_ACCENT_HOVER   0x00D96F2E  /**< 主题蓝悬停 */
#define TOOL_DBG_CLR_ACCENT_PRESS   0x00BB5F27  /**< 主题蓝按下 */
#define TOOL_DBG_CLR_BTN_NEUTRAL_BG 0x00F3F4F8  /**< 中性按钮底色 */
#define TOOL_DBG_CLR_BTN_NEUTRAL_HV 0x00FFFFFF  /**< 中性按钮悬停底色 */
#define TOOL_DBG_CLR_BTN_NEUTRAL_PR 0x00E7EAF1  /**< 中性按钮按下底色 */
#define TOOL_DBG_CLR_BTN_BORDER     0x00D5DAE4  /**< 中性按钮边框 */
#define TOOL_DBG_CLR_BTN_DISABLE    0x00E4E8F0  /**< 按钮禁用底色 */
#define TOOL_DBG_CLR_TEXT_LIGHT     0x00FFFFFF  /**< 亮色文字(主按钮) */
#define TOOL_DBG_CLR_TEXT_DARK      0x00283040  /**< 深色文字(中性按钮) */
#define TOOL_DBG_CLR_TEXT_DISABLE   0x0090A0B0  /**< 禁用文字 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 窗口与布局 (规格见 DESIGN.md 4.0/4.1)                              */
/* -------------------------------------------------------------------------- */
#define TOOL_DBG_CLASS_NAME     L"TS_DBG_MAIN_WND"     /**< 窗口类名 */
#define TOOL_DBG_WND_TITLE      L"调试终端"             /**< 窗口标题 */
#define TOOL_DBG_WND_WIDTH      900u                    /**< 初始宽度 */
#define TOOL_DBG_WND_HEIGHT     640u                    /**< 初始高度 */
#define TOOL_DBG_MIN_TRACK_W    720u                    /**< 最小轨宽 */
#define TOOL_DBG_MIN_TRACK_H    440u                    /**< 最小轨高 */

#define TOOL_DBG_MARGIN         14u    /**< 页边距 */
#define TOOL_DBG_CTRL_H         26u    /**< 单行控件高 */
#define TOOL_DBG_CK_H           24u    /**< 复选框高 */
#define TOOL_DBG_ROW_GAP        10u    /**< 行距 */
#define TOOL_DBG_CTRL_GAP       8u     /**< 同行控件间距 */
#define TOOL_DBG_RX_Y           14u    /**< 接收区顶Y */

#define TOOL_DBG_BTN_TX_W       90u    /**< 发送按钮宽 */
#define TOOL_DBG_BTN_TX_H       32u    /**< 发送按钮高 */

#define TOOL_DBG_TX_H           32u    /**< 发送行高 */
#define TOOL_DBG_CK_W           100u   /**< 复选框宽度 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 行为参数                                                           */
/* -------------------------------------------------------------------------- */
#define TOOL_DBG_TIMER_RX       1u                        /**< 接收刷新定时器ID */
#define TOOL_DBG_TIMER_RX_MS    100u                      /**< 接收刷新周期(毫秒) */
#define TOOL_DBG_MSG_RX         (WM_APP + 1u)             /**< 数据到达通知消息 */
#define TOOL_DBG_MSG_STATUS     (WM_APP + 2u)             /**< 连接状态通知消息 */
#define TOOL_DBG_BTN_COUNT      1u                        /**< 自绘按钮数量(发送) */
#define TOOL_DBG_RX_CAP_WCHAR   (256u * 1024u)            /**< 接收区文本上限(宽字符) */
#define TOOL_DBG_TX_MAX_WCHAR   512u                      /**< 发送输入宽字符上限 */
#define TOOL_DBG_LINE_BUF_WCHAR (DBG_FRAME_LINE_MAX * 2u) /**< 行缓冲宽字符容量 */
#define TOOL_DBG_TX_UTF8_MAX    2048u                     /**< 发送UTF-8字节缓冲容量 */
#define TOOL_DBG_TEXT_TEMP_MAX  1024u                     /**< 通用文本临时缓冲 */
#define TOOL_DBG_STATUS_PARTS   6u                        /**< 状态栏格数 */

/* 接收区RichEdit与多彩关键字 */
#define TOOL_DBG_RICHED_DLL     L"riched20.dll"            /**< RichEdit组件库 */
#define TOOL_DBG_RICHED_CLASS   L"RichEdit20W"             /**< RichEdit窗口类 */
#define TOOL_DBG_EM_SETCHARFORMAT (WM_USER + 68)           /**< 设置字符格式消息 */
#define TOOL_DBG_EM_POSFROMCHAR (WM_USER + 58)             /**< 查询字符客户区坐标 */
#define TOOL_DBG_EM_GETSCROLLPOS (WM_USER + 221)           /**< 获取滚动像素位置 */
#define TOOL_DBG_EM_SETSCROLLPOS (WM_USER + 222)           /**< 恢复滚动像素位置 */
#define TOOL_DBG_EM_GETFIRSTVISIBLELINE 0x00CEu             /**< 首可见行号 */
#define TOOL_DBG_EM_LINEINDEX    0x00BBu                   /**< 行号->字符索引 */
#define TOOL_DBG_EM_GETLINECOUNT 0x00BAu                   /**< 总行数 */
#define TOOL_DBG_SCF_SELECTION  0x0001u                    /**< 作用于当前选区 */
#define TOOL_DBG_CFM_COLOR      0x40000000u                /**< 颜色掩码 */
#define TOOL_DBG_COLOR_MAX      32u                        /**< 多彩关键字词条上限 */
#define TOOL_DBG_COLOR_KW_WCHAR 64u                        /**< 关键字宽字符容量 */
#define TOOL_DBG_COLOR_SEG_MAX  48u                        /**< 单行着色片段上限(超出降级纯文本) */
#define TOOL_DBG_COLOR_LINE_LIMIT 150u                     /**< 单拍着色行数上限(洪峰降级) */
#define TOOL_DBG_TRIM_HARD_MULT  16u                       /**< 上滚浏览不裁剪倍数(超过强制回底防内存无限增长) */

/* -------------------------------------------------------------------------- */
/*  内部类型                                                                    */
/* -------------------------------------------------------------------------- */

/** RichEdit字符格式(CHARFORMATW布局, 本地定义避免头文件差异) */
typedef struct TOOL_CF_TAG
{
    UINT      cbSize;                 /**< 结构大小 */
    DWORD     dwMask;                 /**< 有效字段掩码 */
    DWORD     dwEffects;              /**< 字符效果 */
    LONG      yHeight;                /**< 字高 */
    LONG      yOffset;                /**< 基线偏移 */
    COLORREF  crTextColor;            /**< 文字色 */
    BYTE      bCharSet;               /**< 字符集 */
    BYTE      bPitchAndFamily;        /**< 字族 */
    wchar_t   szFaceName[LF_FACESIZE]; /**< 字体名 */
} TOOL_CF_T;

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static uint8_t   g_tool_class_ready = 0u;                  /**< 窗口类注册标志 */
static HINSTANCE g_tool_instance = NULL;                   /**< 应用实例句柄 */
static HWND      g_tool_hwnd = NULL;                       /**< 主窗口句柄 */
static HWND      g_tool_hstatus = NULL;                    /**< 状态栏句柄 */
static uint8_t   g_tool_connected = 0u;                    /**< 连接状态 */
static uint8_t   g_tool_relogining = 0u;                   /**< 自动重登进行中标志 */
static uint32_t  g_tool_rx_bytes = 0u;                     /**< 接收字节计数 */
static uint32_t  g_tool_tx_bytes = 0u;                     /**< 发送字节计数 */
static uint32_t  g_tool_rx_pending = 0u;                   /**< 数据到达待处理标志 */
static uint32_t  g_tool_hover_id = 0u;                     /**< 悬停按钮ID(0=无) */
static uint32_t  g_tool_press_id = 0u;                     /**< 按下按钮ID(0=无) */
static uint8_t   g_tool_mouse_tracking = 0u;               /**< 鼠标离开跟踪标志 */
static uint32_t  g_tool_rx_pos = 0u;                       /**< 接收区字符位置(RichEdit计量: CRLF=1) */
static uint32_t  g_tool_rx_cap = (256u * 1024u);           /**< 接收区文本上限(宽字符, 菜单可配) */
static uint32_t  g_tool_cache_kb = 256u;                   /**< 接收缓存档位(KB) */
static uint8_t   g_tool_strip_dev = 1u;                    /**< 精简设备时间戳开关 */
static uint8_t   g_tool_ts_on = 1u;                        /**< 时间戳开关(视图菜单, 缺省开) */
static uint8_t   g_tool_pause_on = 0u;                     /**< 暂停显示开关(视图菜单, 缺省关) */
static uint8_t   g_tool_in_batch = 0u;                     /**< 接收批量事务进行中标志 */
static uint8_t   g_tool_batch_lock = 0u;                   /**< 批级滚动锁定标志(拍首用新鲜布局判定, 批内不再重查) */
static wchar_t   g_tool_status_text[160] = {0};            /**< 状态栏首格文本 */
static uint8_t   g_tool_use_rich = 0u;                     /**< 接收区为RichEdit标志 */
static uint32_t  g_tool_color_tick_lines = 0u;             /**< 本拍已着色行数(洪峰降级) */

static const uint32_t g_tool_cache_table[TOOL_DBG_CACHE_COUNT] =
{
    256u, 512u, 1024u, 2048u, 4096u
};

static DBG_SOURCE_RX_T g_tool_rx;                          /**< 接收环形缓冲 */
static DBG_FRAME_T     g_tool_frame;                       /**< 行装配上下文 */

static RECT g_tool_btn_rects[TOOL_DBG_BTN_COUNT] = {0};    /**< 自绘按钮矩形缓存 */

static const uint32_t g_tool_btn_ids[TOOL_DBG_BTN_COUNT] =
{
    TOOL_DBG_IDC_BTN_SEND
};

static const wchar_t *const g_tool_btn_labels[TOOL_DBG_BTN_COUNT] =
{
    L"发送"
};

/* 多彩关键字词表(行->逗号分隔词展开) */
static wchar_t   g_color_kw_text[TOOL_DBG_COLOR_MAX][TOOL_DBG_COLOR_KW_WCHAR];
static uint32_t  g_color_kw_color[TOOL_DBG_COLOR_MAX];
static uint32_t  g_color_kw_count = 0u;

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 前向声明                                                     */
/* -------------------------------------------------------------------------- */
static void tool_status_cb(uint32_t event, const wchar_t *p_msg, void *p_user);
static void tool_on_send(HWND hwnd);
static void tool_rx_apply_charcolor(HWND h_rx, uint32_t color);
static void tool_rebuild_color_words(void);

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 状态栏                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief   刷新状态栏全部格子内容
 * @param   无
 * @return  无
 */
static void tool_update_statusbar(void)
{
    wchar_t cell[160];

    memset(&cell[0], 0, sizeof(cell));
    if (g_tool_hstatus == NULL) {
        return;
    }

    SendMessageW(g_tool_hstatus, SB_SETTEXTW, 0u, (LPARAM)&g_tool_status_text[0]);

    (void)wsprintfW(&cell[0], L"RX %u", (unsigned int)g_tool_rx_bytes);
    SendMessageW(g_tool_hstatus, SB_SETTEXTW, 1u, (LPARAM)&cell[0]);

    (void)wsprintfW(&cell[0], L"TX %u", (unsigned int)g_tool_tx_bytes);
    SendMessageW(g_tool_hstatus, SB_SETTEXTW, 2u, (LPARAM)&cell[0]);

    (void)lstrcpynW(&cell[0],
                    (filter_view_is_open() != 0u) ? L"过滤: 开" : L"过滤: 关", 160);
    SendMessageW(g_tool_hstatus, SB_SETTEXTW, 3u, (LPARAM)&cell[0]);

    (void)lstrcpynW(&cell[0],
                    (dbg_save_autosave_on() != 0u) ? L"保存: 开" : L"保存: 关", 160);
    SendMessageW(g_tool_hstatus, SB_SETTEXTW, 4u, (LPARAM)&cell[0]);

    (void)lstrcpynW(&cell[0],
                    (g_tool_relogining != 0u) ? L"ADB: 重登中…" :
                    (g_tool_connected != 0u) ? L"ADB: 已连接" : L"ADB: 未连接", 160);
    SendMessageW(g_tool_hstatus, SB_SETTEXTW, 5u, (LPARAM)&cell[0]);
}

/**
 * @brief   更新状态栏首格文本
 * @param   p_text  状态文本
 * @return  无
 */
static void tool_set_status(const wchar_t *p_text)
{
    (void)lstrcpynW(&g_tool_status_text[0], (p_text != NULL) ? p_text : L"", 160);
    SendMessageW(g_tool_hstatus, SB_SETTEXTW, 0u, (LPARAM)&g_tool_status_text[0]);
}

/**
 * @brief   按客户区宽度重设状态栏分格
 * @param   hwnd 主窗口句柄
 * @param   cw   客户区宽度
 * @return  无
 */
static void tool_statusbar_resize(HWND hwnd, uint32_t cw)
{
    int32_t edges[TOOL_DBG_STATUS_PARTS];
    uint32_t w_rx = 90u;
    uint32_t w_tx = 80u;
    uint32_t w_filter = 90u;
    uint32_t w_enc = 100u;
    uint32_t w_src = 120u;

    if (g_tool_hstatus == NULL) {
        return;
    }
    (void)SendMessageW(g_tool_hstatus, WM_SIZE, 0u, 0u);

    edges[0] = (int32_t)(cw - (w_rx + w_tx + w_filter + w_enc + w_src));
    if (edges[0] < 80) {
        edges[0] = 80;
    }
    edges[1] = edges[0] + (int32_t)w_rx;
    edges[2] = edges[1] + (int32_t)w_tx;
    edges[3] = edges[2] + (int32_t)w_filter;
    edges[4] = edges[3] + (int32_t)w_enc;
    edges[5] = (int32_t)cw;
    (void)SendMessageW(g_tool_hstatus, SB_SETPARTS, (WPARAM)TOOL_DBG_STATUS_PARTS, (LPARAM)&edges[0]);
    (void)hwnd;
}

/**
 * @brief   查询状态栏占用的客户区高度
 * @return  状态栏高度(像素); 无状态栏时返回0
 */
static uint32_t tool_statusbar_height(void)
{
    RECT rs;

    memset(&rs, 0, sizeof(rs));
    if (g_tool_hstatus == NULL) {
        return 0u;
    }
    (void)GetWindowRect(g_tool_hstatus, &rs);
    return (uint32_t)(rs.bottom - rs.top);
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 菜单栏                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief   创建主窗口菜单栏
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void tool_create_menu(HWND hwnd)
{
    HMENU h_menu = CreateMenu();
    HMENU h_file = CreatePopupMenu();
    HMENU h_conn = CreatePopupMenu();
    HMENU h_view = CreatePopupMenu();
    HMENU h_help = CreatePopupMenu();

    (void)AppendMenuW(h_file, MF_STRING, (UINT_PTR)TOOL_DBG_MID_SAVE_NOW, L"立即保存…");
    (void)AppendMenuW(h_file, MF_STRING, (UINT_PTR)TOOL_DBG_MID_AUTOSAVE, L"自动保存到文件");
    (void)AppendMenuW(h_file, MF_STRING, (UINT_PTR)TOOL_DBG_MID_SAVE_DIR, L"设置保存文件夹…");
    (void)AppendMenuW(h_file, MF_SEPARATOR, 0u, NULL);
    (void)AppendMenuW(h_file, MF_STRING, (UINT_PTR)TOOL_DBG_MID_EXIT, L"退出");

    (void)AppendMenuW(h_conn, MF_STRING, (UINT_PTR)TOOL_DBG_MID_ADB_CONNECT, L"ADB 连接…");
    (void)AppendMenuW(h_conn, MF_STRING, (UINT_PTR)TOOL_DBG_MID_DISCONNECT, L"断开连接");
    (void)AppendMenuW(h_conn, MF_SEPARATOR, 0u, NULL);
    (void)AppendMenuW(h_conn, (MF_STRING | MF_GRAYED), 0u, L"串口连接… (第二阶段)");

    (void)AppendMenuW(h_view, MF_STRING, (UINT_PTR)TOOL_DBG_MID_TS, L"时间戳");
    (void)AppendMenuW(h_view, MF_STRING, (UINT_PTR)TOOL_DBG_MID_PAUSE, L"暂停显示");
    (void)AppendMenuW(h_view, MF_STRING, (UINT_PTR)TOOL_DBG_MID_CLEAR, L"清空显示");
    (void)AppendMenuW(h_view, MF_SEPARATOR, 0u, NULL);
    (void)AppendMenuW(h_view, MF_STRING, (UINT_PTR)TOOL_DBG_MID_FILTERWIN, L"过滤窗口");
    (void)AppendMenuW(h_view, MF_STRING, (UINT_PTR)TOOL_DBG_MID_COLORWIN, L"多彩关键字…");
    (void)AppendMenuW(h_view, MF_STRING, (UINT_PTR)TOOL_DBG_MID_STRIPDEV, L"精简设备时间戳");

    /* 接收缓存子菜单(单选) */
    {
        HMENU h_cache = CreatePopupMenu();
        uint32_t i = 0u;
        wchar_t label[32u];

        for (i = 0u; i < TOOL_DBG_CACHE_COUNT; i++) {
            if (g_tool_cache_table[i] >= 1024u) {
                (void)wsprintfW(&label[0], L"%u MB", (unsigned int)(g_tool_cache_table[i] / 1024u));
            } else {
                (void)wsprintfW(&label[0], L"%u KB", (unsigned int)g_tool_cache_table[i]);
            }
            (void)AppendMenuW(h_cache, MF_STRING | MF_UNCHECKED,
                              (UINT_PTR)(TOOL_DBG_MID_CACHE_BASE + i), &label[0]);
        }
        (void)AppendMenuW(h_view, MF_POPUP, (UINT_PTR)h_cache, L"接收缓存");
    }

    (void)AppendMenuW(h_help, MF_STRING, (UINT_PTR)TOOL_DBG_MID_ABOUT, L"关于 调试终端…");

    (void)AppendMenuW(h_menu, MF_POPUP, (UINT_PTR)h_file, L"文件(&F)");
    (void)AppendMenuW(h_menu, MF_POPUP, (UINT_PTR)h_conn, L"连接(&C)");
    (void)AppendMenuW(h_menu, MF_POPUP, (UINT_PTR)h_view, L"视图(&V)");
    (void)AppendMenuW(h_menu, MF_POPUP, (UINT_PTR)h_help, L"帮助(&H)");
    (void)SetMenu(hwnd, h_menu);
}

/**
 * @brief   同步菜单勾选态与界面控件状态(WM_INITMENUPOPUP)
 * @param   hwnd   主窗口句柄
 * @param   h_menu 视图子菜单句柄
 * @return  无
 */
static void tool_sync_menu_checks(HWND hwnd, HMENU h_menu)
{
    (void)hwnd;
    (void)CheckMenuItem(h_menu, (UINT)TOOL_DBG_MID_TS,
                        MF_BYCOMMAND | ((g_tool_ts_on != 0u) ? MF_CHECKED : MF_UNCHECKED));
    (void)CheckMenuItem(h_menu, (UINT)TOOL_DBG_MID_PAUSE,
                        MF_BYCOMMAND | ((g_tool_pause_on != 0u) ? MF_CHECKED : MF_UNCHECKED));
    (void)CheckMenuItem(h_menu, (UINT)TOOL_DBG_MID_FILTERWIN,
                        MF_BYCOMMAND | ((filter_view_is_open() != 0u) ? MF_CHECKED : MF_UNCHECKED));
    (void)CheckMenuItem(h_menu, (UINT)TOOL_DBG_MID_COLORWIN,
                        MF_BYCOMMAND | ((color_view_is_open() != 0u) ? MF_CHECKED : MF_UNCHECKED));
    (void)CheckMenuItem(h_menu, (UINT)TOOL_DBG_MID_STRIPDEV,
                        MF_BYCOMMAND | ((g_tool_strip_dev != 0u) ? MF_CHECKED : MF_UNCHECKED));
    (void)CheckMenuItem(h_menu, (UINT)TOOL_DBG_MID_AUTOSAVE,
                        MF_BYCOMMAND | ((dbg_save_autosave_on() != 0u) ? MF_CHECKED : MF_UNCHECKED));
    {
        uint32_t i = 0u;

        for (i = 0u; i < TOOL_DBG_CACHE_COUNT; i++) {
            (void)CheckMenuItem(h_menu, (UINT)(TOOL_DBG_MID_CACHE_BASE + i),
                                MF_BYCOMMAND | ((g_tool_cache_table[i] == g_tool_cache_kb)
                                                    ? MF_CHECKED : MF_UNCHECKED));
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 控件创建与布局                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief   创建子控件通用封装(位置尺寸为占位值, 实际由 tool_layout 统一排布)
 * @param   h_parent   父窗口句柄
 * @param   p_class    控件窗口类名
 * @param   p_text     控件初始文本
 * @param   style      控件扩展样式
 * @param   ctrl_id    控件ID
 * @param   font_kind  字体种类(UI_FONT_KIND_E)
 * @return  控件窗口句柄; 失败返回NULL
 */
static HWND tool_create_child(HWND h_parent,
                              const wchar_t *p_class,
                              const wchar_t *p_text,
                              uint32_t style,
                              uint32_t ctrl_id,
                              uint32_t font_kind)
{
    HWND h_ctrl = CreateWindowExW(0u, p_class, p_text,
                                  (WS_CHILD | WS_VISIBLE | style),
                                  0, 0, 10, 10,
                                  h_parent, (HMENU)(uintptr_t)ctrl_id, NULL, NULL);
    if (h_ctrl != NULL) {
        ui_common_apply_font_kind(h_ctrl, font_kind);
    }

    return h_ctrl;
}

/**
 * @brief   探测RichEdit组件(一次性)
 * @return  1: 可用; 0: 不可用(接收区降级普通EDIT, 无着色)
 */
static uint8_t tool_ensure_richedit(void)
{
    static uint8_t s_probed = 0u;

    if (s_probed == 0u) {
        HMODULE h_lib = LoadLibraryW(TOOL_DBG_RICHED_DLL);

        g_tool_use_rich = (h_lib != NULL) ? 1u : 0u;
        s_probed = 1u;
    }
    return g_tool_use_rich;
}

/**
 * @brief   从ini重建多彩关键字词表([Color]各行逗号分隔展开)
 * @return  无
 */
static void tool_rebuild_color_words(void)
{
    uint32_t rows = 0u;
    uint32_t r = 0u;

    g_color_kw_count = 0u;
    rows = dbg_conf_get_uint(L"Color", L"Rows", 0u);
    if (rows > 12u) {
        rows = 12u;
    }
    for (r = 0u; (r < rows) && (g_color_kw_count < TOOL_DBG_COLOR_MAX); r++) {
        wchar_t row_text[128u];
        wchar_t token[TOOL_DBG_COLOR_KW_WCHAR];
        wchar_t keyname[8u];
        uint32_t color = 0u;
        uint32_t i = 0u;
        uint32_t tok_pos = 0u;

        memset(&row_text[0], 0, sizeof(row_text));
        (void)wsprintfW(&keyname[0], L"Key%u", (unsigned int)(r + 1u));
        dbg_conf_get_str(L"Color", &keyname[0], L"", &row_text[0], 128u);
        (void)wsprintfW(&keyname[0], L"Color%u", (unsigned int)(r + 1u));
        color = dbg_conf_get_uint(L"Color", &keyname[0], 0x00000000u);

        for (i = 0u; (row_text[i] != L'\0'); i++) {
            wchar_t ch = row_text[i];
            uint8_t is_sep = ((ch == L',') || (ch == L'，') || (ch == L';') ||
                              (ch == L'；') || (ch == L' ')) ? 1u : 0u;

            if (is_sep != 0u) {
                if (tok_pos > 0u) {
                    token[tok_pos] = L'\0';
                    if (g_color_kw_count < TOOL_DBG_COLOR_MAX) {
                        lstrcpynW(&g_color_kw_text[g_color_kw_count][0], &token[0],
                                  (int)TOOL_DBG_COLOR_KW_WCHAR);
                        g_color_kw_color[g_color_kw_count] = color;
                        g_color_kw_count++;
                    }
                    tok_pos = 0u;
                    token[0] = L'\0';
                }
            } else if (tok_pos < (TOOL_DBG_COLOR_KW_WCHAR - 1u)) {
                token[tok_pos] = ch;
                tok_pos++;
            } else {
                /* 超长关键字截断 */
            }
        }
        if (tok_pos > 0u) {
            token[tok_pos] = L'\0';
            if (g_color_kw_count < TOOL_DBG_COLOR_MAX) {
                lstrcpynW(&g_color_kw_text[g_color_kw_count][0], &token[0],
                          (int)TOOL_DBG_COLOR_KW_WCHAR);
                g_color_kw_color[g_color_kw_count] = color;
                g_color_kw_count++;
            }
        }
    }
}

/**
 * @brief   多彩关键字配置变更回调(配置窗口触发)
 * @return  无
 */
static void tool_on_color_changed(void)
{
    tool_rebuild_color_words();
}

/**
 * @brief   创建主窗口内全部子控件(工作区+状态栏)
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void tool_create_controls(HWND hwnd)
{
    HWND h_rx = NULL;

    /* 接收区: RichEdit(可着色)优先, 失败降级普通EDIT */
    h_rx = tool_create_child(hwnd,
                             (tool_ensure_richedit() != 0u) ? TOOL_DBG_RICHED_CLASS : L"EDIT",
                             L"",
                             (uint32_t)(ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_BORDER),
                             TOOL_DBG_IDC_ED_RX, UI_FONT_KIND_EDIT);
    if ((h_rx != NULL) && (g_tool_use_rich != 0u)) {
        (void)SendMessageW(h_rx, EM_SETREADONLY, (WPARAM)TRUE, (LPARAM)0);
    }

    /* 发送输入 */
    (void)tool_create_child(hwnd, L"EDIT", L"",
                            (uint32_t)(ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL |
                                       WS_BORDER),
                            TOOL_DBG_IDC_ED_TX, UI_FONT_KIND_EDIT);

    /* 开关控件: 时间戳/暂停显示经视图菜单(无界面控件), 此处仅发送行的加回车 */
    (void)tool_create_child(hwnd, L"BUTTON", L"发送加回车",
                            (uint32_t)BS_AUTOCHECKBOX, TOOL_DBG_IDC_CK_APPEND, UI_FONT_KIND_BTN);

    /* 接收区文本上限(接收缓存档位从ini加载) */
    g_tool_cache_kb = dbg_conf_get_uint(L"View", L"RxCacheKB", 256u);
    g_tool_rx_cap = g_tool_cache_kb * 1024u;
    g_tool_strip_dev = (dbg_conf_get_uint(L"View", L"StripDevTs", 1u) != 0u) ? 1u : 0u;
    (void)SendMessageW(GetDlgItem(hwnd, (int)TOOL_DBG_IDC_ED_RX), EM_SETLIMITTEXT,
                       (WPARAM)g_tool_rx_cap, (LPARAM)0);
    (void)SendMessageW(GetDlgItem(hwnd, (int)TOOL_DBG_IDC_ED_TX), EM_SETLIMITTEXT,
                       (WPARAM)(TOOL_DBG_TX_MAX_WCHAR - 1u), (LPARAM)0);
    dbg_save_init();

    /* 多彩关键字词表与配置变更回调 */
    tool_rebuild_color_words();
    color_view_set_changed_fn(&tool_on_color_changed);

    /* 状态栏(comctl32, 6格) */
    g_tool_hstatus = CreateWindowExW(0u, STATUSCLASSNAMEW, NULL,
                                     (WS_CHILD | WS_VISIBLE),
                                     0, 0, 0, 0, hwnd, NULL, g_tool_instance, NULL);
}

/**
 * @brief   按客户区尺寸排布全部子控件并缓存自绘按钮矩形
 *          规格见 DESIGN.md 4.0/4.1: 接收区弹性主体, 底部锚定发送行+状态栏
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void tool_layout(HWND hwnd)
{
    RECT rc = {0};
    uint32_t cw = 0u;                 /* 客户区宽度 */
    uint32_t ch = 0u;                 /* 客户区高度 */
    uint32_t ch_usable = 0u;          /* 扣除状态栏后的可用高度 */
    uint32_t tx_y = 0u;               /* 发送行Y */

    GetClientRect(hwnd, &rc);
    cw = (uint32_t)(rc.right - rc.left);
    ch = (uint32_t)(rc.bottom - rc.top);
    if (cw < 300u) {
        return;
    }

    tool_statusbar_resize(hwnd, cw);
    ch_usable = ch - tool_statusbar_height();

    /* 底部锚定栈(自下而上, 以状态栏上缘为基): 发送行 <- 接收区底 */
    tx_y = (ch_usable - TOOL_DBG_MARGIN - TOOL_DBG_TX_H);
    if (tx_y <= (TOOL_DBG_RX_Y + 60u)) {
        return;   /* 纵向空间不足, 保持上次布局 */
    }

    /* 接收区(弹性主体) */
    (void)MoveWindow(GetDlgItem(hwnd, (int)TOOL_DBG_IDC_ED_RX),
                     (int)TOOL_DBG_MARGIN, (int)TOOL_DBG_RX_Y,
                     (int)(cw - (TOOL_DBG_MARGIN * 2u)),
                     (int)(tx_y - TOOL_DBG_ROW_GAP - TOOL_DBG_RX_Y), TRUE);

    /* 发送行: [发送输入延伸][加回车][发送] 贴右 */
    g_tool_btn_rects[0].right = (LONG)(cw - TOOL_DBG_MARGIN);
    g_tool_btn_rects[0].top = (LONG)tx_y;
    g_tool_btn_rects[0].left = (LONG)(cw - TOOL_DBG_MARGIN - TOOL_DBG_BTN_TX_W);
    g_tool_btn_rects[0].bottom = (LONG)(tx_y + TOOL_DBG_BTN_TX_H);
    (void)MoveWindow(GetDlgItem(hwnd, (int)TOOL_DBG_IDC_CK_APPEND),
                     (int)(cw - TOOL_DBG_MARGIN - TOOL_DBG_BTN_TX_W - TOOL_DBG_CTRL_GAP - TOOL_DBG_CK_W),
                     (int)(tx_y + 4u), (int)TOOL_DBG_CK_W, (int)TOOL_DBG_CK_H, TRUE);
    (void)MoveWindow(GetDlgItem(hwnd, (int)TOOL_DBG_IDC_ED_TX),
                     (int)TOOL_DBG_MARGIN, (int)tx_y,
                     (int)((uint32_t)g_tool_btn_rects[0].left - TOOL_DBG_CTRL_GAP -
                           TOOL_DBG_CK_W - TOOL_DBG_CTRL_GAP - TOOL_DBG_MARGIN),
                     (int)TOOL_DBG_TX_H, TRUE);

    InvalidateRect(hwnd, NULL, FALSE);
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 自绘按钮(发送)                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   判断自绘按钮当前是否可用
 * @param   index 按钮索引
 * @return  1: 可用; 0: 禁用
 */
static uint8_t tool_btn_enabled(uint32_t index)
{
    (void)index;
    return (g_tool_connected != 0u) ? 1u : 0u;   /* 发送: 已连接时可用 */
}

/**
 * @brief   绘制单个自绘按钮(主题色主按钮, 禁用置灰)
 * @param   hdc   目标设备上下文
 * @param   index 按钮索引
 * @return  无
 */
static void tool_draw_button(HDC hdc, uint32_t index)
{
    uint32_t ctrl_id = g_tool_btn_ids[index];
    uint8_t hovered = ((g_tool_hover_id == ctrl_id) ? 1u : 0u);
    uint8_t pressed = ((g_tool_press_id == ctrl_id) ? 1u : 0u);
    uint8_t enabled = tool_btn_enabled(index);
    uint32_t fill_clr = 0u;
    uint32_t border_clr = 0u;
    uint32_t text_clr = 0u;
    RECT rc = g_tool_btn_rects[index];
    HFONT h_font_old = NULL;

    if (enabled == 0u) {
        fill_clr = TOOL_DBG_CLR_BTN_DISABLE;
        border_clr = TOOL_DBG_CLR_BTN_DISABLE;
        text_clr = TOOL_DBG_CLR_TEXT_DISABLE;
    } else {
        if (pressed != 0u) {
            fill_clr = TOOL_DBG_CLR_ACCENT_PRESS;
        } else if (hovered != 0u) {
            fill_clr = TOOL_DBG_CLR_ACCENT_HOVER;
        } else {
            fill_clr = TOOL_DBG_CLR_ACCENT;
        }
        border_clr = fill_clr;
        text_clr = TOOL_DBG_CLR_TEXT_LIGHT;
    }

    SetBkMode(hdc, TRANSPARENT);
    ui_common_fill_round_rect(hdc, &rc, 8u, fill_clr, border_clr, 1u);

    /* 文字内缩后按不透明模式绘制(启用真ClearType); 内缩量大于圆角半径 */
    InflateRect(&rc, -9, -7);
    h_font_old = (HFONT)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_BTN));
    SetTextColor(hdc, (COLORREF)text_clr);
    ui_common_text_crisp(hdc, &rc, g_tool_btn_labels[index],
                         (DT_CENTER | DT_VCENTER | DT_SINGLELINE), fill_clr);
    (void)SelectObject(hdc, h_font_old);
}

/**
 * @brief   命中测试: 客户区坐标落在哪个自绘按钮上(仅可用按钮)
 * @param   pt 客户区坐标
 * @return  按钮逻辑ID; 无命中返回0
 */
static uint32_t tool_hit_btn(POINT pt)
{
    uint32_t index = 0u;

    for (index = 0u; index < TOOL_DBG_BTN_COUNT; index++) {
        if ((PtInRect(&g_tool_btn_rects[index], pt) != 0) &&
            (tool_btn_enabled(index) != 0u)) {
            return g_tool_btn_ids[index];
        }
    }

    return 0u;
}

/**
 * @brief   使指定按钮矩形区域重绘
 * @param   hwnd    主窗口句柄
 * @param   ctrl_id 按钮逻辑ID(0忽略)
 * @return  无
 */
static void tool_invalidate_btn(HWND hwnd, uint32_t ctrl_id)
{
    uint32_t index = 0u;

    if (ctrl_id == 0u) {
        return;
    }
    for (index = 0u; index < TOOL_DBG_BTN_COUNT; index++) {
        if (g_tool_btn_ids[index] == ctrl_id) {
            InvalidateRect(hwnd, &g_tool_btn_rects[index], FALSE);
            return;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 接收管线(RichEdit着色)                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief   查询RichEdit单行像素高度(用控件字体实测, 拉取DC临时测量)
 * @param   h_rx 接收区句柄
 * @return  行高(像素); 失败返回16
 */
static uint32_t tool_rx_line_height(HWND h_rx)
{
    HDC hdc = GetDC(h_rx);
    HFONT h_font = (HFONT)SendMessageW(h_rx, WM_GETFONT, 0u, 0u);
    HFONT h_old = (HFONT)SelectObject(hdc, h_font);
    TEXTMETRICW tm;
    uint32_t height = 16u;

    memset(&tm, 0, sizeof(tm));
    if (GetTextMetricsW(hdc, &tm) != 0) {
        height = (uint32_t)(tm.tmHeight + tm.tmExternalLeading);
        if (height == 0u) {
            height = 16u;
        }
    }
    (void)SelectObject(hdc, h_old);
    ReleaseDC(h_rx, hdc);

    return height;
}

/**
 * @brief   判断接收区是否位于最底部(自动跟随的条件)
 *          首可见行 + 可见行数 >= 总行数(±2行容差); 全部走返回值型消息,
 *          不依赖POSCROLLINFO/POSCFROMCHAR(关重绘或riched20版本下不可靠)
 * @param   h_rx 接收区句柄
 * @return  1: 在底部(或内容不足一屏); 0: 用户已上滚
 */
static uint8_t tool_rx_at_bottom(HWND h_rx)
{
    if (g_tool_use_rich != 0u) {
        LRESULT first = 0;
        LRESULT lines = 0;
        RECT rc;
        uint32_t line_h = 0u;
        uint32_t visible_lines = 0u;

        first = SendMessageW(h_rx, TOOL_DBG_EM_GETFIRSTVISIBLELINE, 0u, 0u);
        lines = SendMessageW(h_rx, TOOL_DBG_EM_GETLINECOUNT, 0u, 0u);
        memset(&rc, 0, sizeof(rc));
        GetClientRect(h_rx, &rc);
        line_h = tool_rx_line_height(h_rx);
        visible_lines = ((uint32_t)(rc.bottom - rc.top) + line_h - 1u) / line_h;
        if (visible_lines < 1u) {
            visible_lines = 1u;
        }

        return ((first + (LRESULT)visible_lines + 2) >= lines) ? 1u : 0u;
    }

    /* 普通EDIT降级路径: 标准滚动条信息 */
    {
        SCROLLINFO si;

        memset(&si, 0, sizeof(si));
        si.cbSize = (UINT)sizeof(si);
        si.fMask = (SIF_PAGE | SIF_RANGE | SIF_POS);
        if (GetScrollInfo(h_rx, SB_VERT, &si) == 0) {
            return 1u;
        }
        if (si.nMax <= (int)(si.nPage)) {
            return 1u;   /* 内容不足一屏 */
        }

        return ((si.nPos + (int)(si.nPage) + 2) >= si.nMax) ? 1u : 0u;
    }
}

/**
 * @brief   恢复锁定时的视口: 复位滚动像素位置并把光标移回可视首行
 *          (RichEdit绘制时会把文末光标滚入视野, 仅恢复滚动位置会被再次拉底)
 * @param   h_rx 接收区句柄
 * @param   p_sp 批首保存的滚动像素位置
 * @return  无
 */
static void tool_rx_restore_view(HWND h_rx, const POINT *p_sp)
{
    LRESULT first_line = 0u;
    LRESULT ch = 0u;

    (void)SendMessageW(h_rx, TOOL_DBG_EM_SETSCROLLPOS, 0u, (LPARAM)p_sp);
    first_line = SendMessageW(h_rx, TOOL_DBG_EM_GETFIRSTVISIBLELINE, 0u, 0u);
    ch = SendMessageW(h_rx, TOOL_DBG_EM_LINEINDEX, (WPARAM)first_line, 0u);
    if (ch >= 0) {
        (void)SendMessageW(h_rx, EM_SETSEL, (WPARAM)ch, (LPARAM)ch);
    }
}

/**
 * @brief   设置接收区插入点字符颜色(RichEdit; 须在解锁只读期间调用)
 * @param   h_rx   接收区句柄
 * @param   color  文字色(0x00BBGGRR)
 * @return  无
 */
static void tool_rx_apply_charcolor(HWND h_rx, uint32_t color)
{
    TOOL_CF_T cf;

    memset(&cf, 0, sizeof(cf));
    cf.cbSize = (UINT)sizeof(cf);
    cf.dwMask = TOOL_DBG_CFM_COLOR;
    cf.dwEffects = 0u;   /* 清除CFE_AUTOCOLOR, 启用crTextColor */
    cf.crTextColor = (COLORREF)color;
    (void)SendMessageW(h_rx, TOOL_DBG_EM_SETCHARFORMAT,
                       (WPARAM)TOOL_DBG_SCF_SELECTION, (LPARAM)&cf);
}

/**
 * @brief   接收区超限裁头(RichEdit计量)
 *          底部跟随时按上限裁剪; 用户上滚浏览时不裁剪(裁剪会移动视口内容,
 *          表现为"锁定失效"), 仅在达到16倍硬上限时强制回底裁剪防内存无限增长
 * @param   h_rx  接收区句柄
 * @param   stick 1=当前在底部
 * @return  1=已强制回底(调用方本拍按跟随处理); 0=正常
 */
static uint8_t tool_rx_trim(HWND h_rx, uint8_t stick)
{
    if (g_tool_rx_pos <= g_tool_rx_cap) {
        return 0u;
    }
    if (stick == 0u) {
        if (g_tool_rx_pos <= (g_tool_rx_cap * TOOL_DBG_TRIM_HARD_MULT)) {
            return 0u;   /* 上滚浏览: 不裁剪, 保视口稳定 */
        }
        /* 硬上限: 强制回底并裁剪(极罕见, 防内存无限增长) */
        (void)SendMessageW(h_rx, WM_VSCROLL, (WPARAM)SB_BOTTOM, (LPARAM)0);
        stick = 1u;
        if (g_tool_hwnd != NULL) {
            tool_set_status(L"缓冲已达上限, 已回底裁剪(可调大 视图→接收缓存)");
        }
    }
    {
        uint32_t del_chars = g_tool_rx_pos / 2u;

        (void)SendMessageW(h_rx, EM_SETREADONLY, (WPARAM)FALSE, (LPARAM)0);
        (void)SendMessageW(h_rx, EM_SETSEL, 0, (LPARAM)del_chars);
        (void)SendMessageW(h_rx, EM_REPLACESEL, FALSE, (LPARAM)L"");
        (void)SendMessageW(h_rx, EM_SETREADONLY, (WPARAM)TRUE, (LPARAM)0);
        g_tool_rx_pos -= del_chars;
    }

    return stick;
}

/**
 * @brief   追加纯文本到接收区(默认黑色; TX回显/降级路径)
 * @param   hwnd   主窗口句柄
 * @param   p_text 待追加文本(可含CRLF; RichEdit把\r\n记为1字符)
 * @return  无
 */
static void tool_append_rx(HWND hwnd, const wchar_t *p_text)
{
    HWND h_rx = NULL;
    uint32_t len = 0u;
    uint32_t crlf = 0u;
    const wchar_t *p = p_text;
    uint8_t stick = 0u;
    POINT sp = {0, 0};

    if (g_tool_pause_on != 0u) {
        return;
    }
    h_rx = GetDlgItem(hwnd, (int)TOOL_DBG_IDC_ED_RX);
    if (g_tool_in_batch != 0u) {
        /* 批内: 信任拍首判定(关重绘期间布局过期, 逐行重查POSFROMCHAR得脏值) */
        stick = (g_tool_batch_lock != 0u) ? 0u : 1u;
    } else {
        stick = tool_rx_at_bottom(h_rx);   /* 独立调用(如TX回显): 自查 */
    }

    if (g_tool_use_rich != 0u) {
        if (stick == 0u) {
            /* 锁定: 记录滚动像素位置, 追加后恢复(EM_REPLACESEL会拉光标入视野) */
            (void)SendMessageW(h_rx, TOOL_DBG_EM_GETSCROLLPOS, 0u, (LPARAM)&sp);
        }
        (void)SendMessageW(h_rx, EM_SETREADONLY, (WPARAM)FALSE, (LPARAM)0);
        if (tool_rx_trim(h_rx, stick) != 0u) {
            stick = 1u;   /* 硬上限强制回底: 本拍转跟随 */
        }
        (void)SendMessageW(h_rx, EM_SETSEL, (WPARAM)g_tool_rx_pos, (LPARAM)g_tool_rx_pos);
        tool_rx_apply_charcolor(h_rx, 0x00000000u);
        (void)SendMessageW(h_rx, EM_REPLACESEL, FALSE, (LPARAM)p_text);
        (void)SendMessageW(h_rx, EM_SETREADONLY, (WPARAM)TRUE, (LPARAM)0);
        len = (uint32_t)lstrlenW(p_text);
        for (; *p != L'\0'; p++) {
            if ((*p == L'\r') && (*(p + 1) == L'\n')) {
                crlf++;
                p++;
            }
        }
        g_tool_rx_pos += len - crlf;
        if (stick != 0u) {
            (void)SendMessageW(h_rx, WM_VSCROLL, (WPARAM)SB_BOTTOM, (LPARAM)0);
        } else {
            tool_rx_restore_view(h_rx, &sp);
        }
    } else {
        uint32_t cur = 0u;

        if (SendMessageW(h_rx, WM_GETTEXTLENGTH, 0u, 0u) > g_tool_rx_cap) {
            (void)SendMessageW(h_rx, EM_SETSEL, 0,
                               (LPARAM)((uint32_t)SendMessageW(h_rx, WM_GETTEXTLENGTH, 0u, 0u) / 2u));
            (void)SendMessageW(h_rx, EM_REPLACESEL, FALSE, (LPARAM)L"");
        }
        cur = (uint32_t)SendMessageW(h_rx, WM_GETTEXTLENGTH, 0u, 0u);
        (void)SendMessageW(h_rx, EM_SETSEL, (WPARAM)cur, (LPARAM)cur);
        (void)SendMessageW(h_rx, EM_REPLACESEL, FALSE, (LPARAM)p_text);
        if (stick != 0u) {
            (void)SendMessageW(h_rx, WM_VSCROLL, (WPARAM)SB_BOTTOM, (LPARAM)0);
        }
    }
}

/**
 * @brief   大小写不敏感宽字符子串查找(自指定位置起)
 * @param   p_line 行文本
 * @param   p_kw   关键字
 * @param   from   起始索引
 * @return  命中索引; 未命中返回-1
 */
static int32_t tool_find_ci(const wchar_t *p_line, const wchar_t *p_kw, uint32_t from)
{
    uint32_t line_len = (uint32_t)lstrlenW(p_line);
    uint32_t kw_len = (uint32_t)lstrlenW(p_kw);
    uint32_t i = from;
    uint32_t j = 0u;

    if ((kw_len == 0u) || (kw_len > line_len)) {
        return -1;
    }
    for (i = from; (i + kw_len) <= line_len; i++) {
        uint8_t match = 1u;

        for (j = 0u; j < kw_len; j++) {
            wchar_t a = p_line[i + j];
            wchar_t b = p_kw[j];

            if ((a >= L'A') && (a <= L'Z')) {
                a = (wchar_t)(a + (L'a' - L'A'));
            }
            if ((b >= L'A') && (b <= L'Z')) {
                b = (wchar_t)(b + (L'a' - L'A'));
            }
            if (a != b) {
                match = 0u;
                break;
            }
        }
        if (match != 0u) {
            return (int32_t)i;
        }
    }

    return -1;
}

/**
 * @brief   按多彩关键字着色追加一行到接收区(未命中部分黑色)
 *          洪峰/超长行/片段超限自动降级为纯文本追加
 * @param   hwnd   主窗口句柄
 * @param   p_line 行文本(不含行尾)
 * @return  无
 */
static void tool_append_rx_colored(HWND hwnd, const wchar_t *p_line)
{
    HWND h_rx = GetDlgItem(hwnd, (int)TOOL_DBG_IDC_ED_RX);
    uint32_t line_len = (uint32_t)lstrlenW(p_line);
    uint32_t seg_start[TOOL_DBG_COLOR_SEG_MAX];
    uint32_t seg_len[TOOL_DBG_COLOR_SEG_MAX];
    uint32_t seg_color[TOOL_DBG_COLOR_SEG_MAX];
    uint32_t seg_count = 0u;
    uint32_t k = 0u;
    uint32_t i = 0u;

    if ((g_color_kw_count == 0u) || (g_tool_use_rich == 0u) ||
        (line_len == 0u) ||
        (g_tool_color_tick_lines >= TOOL_DBG_COLOR_LINE_LIMIT)) {
        wchar_t plain[TOOL_DBG_LINE_BUF_WCHAR + 4u];

        memset(&plain[0], 0, sizeof(plain));
        (void)lstrcpynW(&plain[0], p_line, (int)(TOOL_DBG_LINE_BUF_WCHAR + 2u));
        (void)lstrcatW(&plain[0], L"\r\n");
        tool_append_rx(hwnd, &plain[0]);
        return;
    }

    /* 收集不重叠命中片段(按起始位置贪心, 行序靠前的关键字优先) */
    {
        uint32_t scan = 0u;

        while ((scan < line_len) && (seg_count < TOOL_DBG_COLOR_SEG_MAX)) {
            int32_t best = -1;
            uint32_t best_len = 0u;
            uint32_t best_color = 0u;

            for (k = 0u; k < g_color_kw_count; k++) {
                uint32_t kw_len = (uint32_t)lstrlenW(&g_color_kw_text[k][0]);
                int32_t hit = -1;

                if (kw_len == 0u) {
                    continue;
                }
                hit = tool_find_ci(&p_line[scan], &g_color_kw_text[k][0], 0u);
                if (hit < 0) {
                    continue;
                }
                if ((best < 0) || (((uint32_t)hit) < (uint32_t)best)) {
                    best = hit;
                    best_len = kw_len;
                    best_color = g_color_kw_color[k];
                }
            }
            if (best < 0) {
                break;
            }
            seg_start[seg_count] = scan + (uint32_t)best;
            seg_len[seg_count] = best_len;
            seg_color[seg_count] = best_color;
            seg_count++;
            scan = scan + (uint32_t)best + best_len;
        }
    }

    /* 分段追加: 未命中段黑色, 命中段按词色; 行尾CRLF黑色
       (滚动跟随与裁剪由外层 tool_process_rx 批量统一处理) */
    (void)SendMessageW(h_rx, EM_SETREADONLY, (WPARAM)FALSE, (LPARAM)0);
    {
        wchar_t piece[TOOL_DBG_LINE_BUF_WCHAR + 4u];
        uint32_t cursor = 0u;

        for (i = 0u; i <= seg_count; i++) {
            uint32_t from = (i < seg_count) ? seg_start[i] : line_len;
            uint32_t gap = from - cursor;

            if (gap > 0u) {
                memset(&piece[0], 0, sizeof(piece));
                for (k = 0u; k < gap; k++) {
                    piece[k] = p_line[cursor + k];
                }
                piece[gap] = L'\0';
                (void)SendMessageW(h_rx, EM_SETSEL, (WPARAM)g_tool_rx_pos, (LPARAM)g_tool_rx_pos);
                tool_rx_apply_charcolor(h_rx, 0x00000000u);
                (void)SendMessageW(h_rx, EM_REPLACESEL, FALSE, (LPARAM)&piece[0]);
                g_tool_rx_pos += gap;
            }
            if (i < seg_count) {
                memset(&piece[0], 0, sizeof(piece));
                for (k = 0u; k < seg_len[i]; k++) {
                    piece[k] = p_line[seg_start[i] + k];
                }
                piece[seg_len[i]] = L'\0';
                (void)SendMessageW(h_rx, EM_SETSEL, (WPARAM)g_tool_rx_pos, (LPARAM)g_tool_rx_pos);
                tool_rx_apply_charcolor(h_rx, seg_color[i]);
                (void)SendMessageW(h_rx, EM_REPLACESEL, FALSE, (LPARAM)&piece[0]);
                g_tool_rx_pos += seg_len[i];
                cursor = seg_start[i] + seg_len[i];
            }
        }
        (void)SendMessageW(h_rx, EM_SETSEL, (WPARAM)g_tool_rx_pos, (LPARAM)g_tool_rx_pos);
        tool_rx_apply_charcolor(h_rx, 0x00000000u);
        (void)SendMessageW(h_rx, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
        g_tool_rx_pos += 1u;   /* 段落符记1字符 */
    }
    (void)SendMessageW(h_rx, EM_SETREADONLY, (WPARAM)TRUE, (LPARAM)0);
    g_tool_color_tick_lines++;
}

/**
 * @brief   单行落地: 主窗口追加(可着色); 过滤窗口打开时命中行同步
 * @param   hwnd   主窗口句柄
 * @param   p_line 行文本(不含行尾)
 * @return  无
 */
static void tool_handle_line(HWND hwnd, const wchar_t *p_line)
{
    uint32_t group_idx = 0u;
    uint32_t ret = 0u;

    if (g_tool_pause_on != 0u) {
        return;   /* 暂停显示: 主窗与过滤窗都不追加(管线与保存照常) */
    }

    tool_append_rx_colored(hwnd, p_line);

    /* 过滤窗口: 命中行(>>K行号标记, 行内关键字按行色高亮), 不与全量日志混排 */
    ret = dbg_filter_match(p_line, &group_idx);
    if ((ret == (uint32_t)DBG_FILTER_RET_HIT) && (filter_view_is_open() != 0u)) {
        filter_view_append_hit(p_line, group_idx);
    }
}

/**
 * @brief   将装配输出按行切分并逐行落地
 * @param   hwnd   主窗口句柄
 * @param   p_text 装配输出文本(行以CRLF结尾)
 * @return  无
 */
static void tool_split_and_handle(HWND hwnd, const wchar_t *p_text)
{
    wchar_t line_buf[TOOL_DBG_LINE_BUF_WCHAR];
    uint32_t len = 0u;
    uint32_t i = 0u;

    memset(&line_buf[0], 0, sizeof(line_buf));
    len = (uint32_t)lstrlenW(p_text);

    for (i = 0u; i < len; i++) {
        if ((p_text[i] == L'\r') && ((i + 1u) < len) && (p_text[i + 1u] == L'\n')) {
            tool_handle_line(hwnd, &line_buf[0]);
            memset(&line_buf[0], 0, sizeof(line_buf));
            i++;   /* 跳过LF */
        } else if ((uint32_t)lstrlenW(&line_buf[0]) < (TOOL_DBG_LINE_BUF_WCHAR - 1u)) {
            uint32_t cur = (uint32_t)lstrlenW(&line_buf[0]);
            line_buf[cur] = p_text[i];
            line_buf[cur + 1u] = L'\0';
        } else {
            /* 行缓冲满: 直接落地防丢 */
            tool_handle_line(hwnd, &line_buf[0]);
            memset(&line_buf[0], 0, sizeof(line_buf));
        }
    }
}

/**
 * @brief   处理接收: 取走环形缓冲 -> 行装配 -> 着色/过滤 -> 显示
 *          (RichEdit整个节拍关重绘一次, 洪峰保UI响应)
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void tool_process_rx(HWND hwnd)
{
    uint8_t  bytes[4096u];
    wchar_t  out[DBG_FRAME_OUT_MAX];
    DBG_FRAME_OPT_T opt;
    HWND h_rx = GetDlgItem(hwnd, (int)TOOL_DBG_IDC_ED_RX);
    uint32_t pos = 0u;
    uint32_t got = 0u;
    uint32_t pass = 0u;
    uint32_t overflow = 0u;
    uint8_t  stick = 1u;   /* 批处理开始时的底部跟随状态 */
    POINT    sp = {0, 0};  /* 锁定时的滚动像素位置 */

    memset(&bytes[0], 0, sizeof(bytes));
    memset(&out[0], 0, sizeof(out));
    memset(&opt, 0, sizeof(opt));
    opt.ts_enable = g_tool_ts_on;
    opt.p_dir_tag = NULL;   /* RX行不带方向标记; TX回显自带[TX] */
    opt.enc = (uint32_t)DBG_FRAME_ENC_UTF8;
    opt.strip_dev_ts = g_tool_strip_dev;

    g_tool_color_tick_lines = 0u;
    stick = tool_rx_at_bottom(h_rx);   /* 拍首捕捉(此时布局新鲜): 用户上滚时整拍锁定 */
    g_tool_batch_lock = (stick != 0u) ? 0u : 1u;
    g_tool_in_batch = 1u;
    if ((stick == 0u) && (g_tool_use_rich != 0u)) {
        /* 锁定: 记录滚动像素位置, 批量追加后恢复 */
        (void)SendMessageW(h_rx, TOOL_DBG_EM_GETSCROLLPOS, 0u, (LPARAM)&sp);
    }
    if (tool_rx_trim(h_rx, stick) != 0u) {
        stick = 1u;   /* 硬上限强制回底: 本拍转跟随 */
        g_tool_batch_lock = 0u;
    }

    /* 批量事务: 过滤窗口整个节拍只解锁/重绘一次 */
    if (filter_view_is_open() != 0u) {
        filter_view_begin_batch();
    }
    /* 主窗RichEdit整拍关重绘(着色重排一次完成) */
    if (g_tool_use_rich != 0u) {
        (void)SendMessageW(h_rx, WM_SETREDRAW, (WPARAM)FALSE, (LPARAM)0);
    }

    for (pass = 0u; pass < 8u; pass++) {
        got = dbg_source_rx_drain(&g_tool_rx, &bytes[0], (uint32_t)sizeof(bytes));
        if (got == 0u) {
            break;
        }
        g_tool_rx_bytes += got;
        if (pos >= (DBG_FRAME_OUT_MAX - DBG_FRAME_LINE_MAX)) {
            dbg_save_feed(&out[0]);   /* 自动保存喂入(全量行, 含时间戳) */
            tool_split_and_handle(hwnd, &out[0]);
            pos = 0u;
            out[0] = L'\0';
        }
        (void)dbg_frame_feed(&g_tool_frame, &bytes[0], got, &opt, &out[0], DBG_FRAME_OUT_MAX, &pos);
    }

    (void)dbg_frame_flush_idle(&g_tool_frame, &opt, &out[0], DBG_FRAME_OUT_MAX, &pos);
    if (pos > 0u) {
        dbg_save_feed(&out[0]);
        tool_split_and_handle(hwnd, &out[0]);
    }

    if (filter_view_is_open() != 0u) {
        filter_view_end_batch();
    }
    g_tool_in_batch = 0u;
    if (g_tool_use_rich != 0u) {
        (void)SendMessageW(h_rx, WM_SETREDRAW, (WPARAM)TRUE, (LPARAM)0);
        (void)InvalidateRect(h_rx, NULL, FALSE);
        if (stick != 0u) {
            (void)SendMessageW(h_rx, WM_VSCROLL, (WPARAM)SB_BOTTOM, (LPARAM)0);
        } else {
            tool_rx_restore_view(h_rx, &sp);
        }
    }

    overflow = dbg_source_rx_take_overflow(&g_tool_rx);
    if (overflow != 0u) {
        tool_set_status(L"警告: 接收缓冲溢出, 已丢弃部分数据");
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 动作                                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief   状态回调(数据源线程上下文): 仅投递消息, 保证线程安全
 * @param   event  事件码(DBG_SOURCE_EVT_E)
 * @param   p_msg  消息(未使用)
 * @param   p_user 用户指针(未使用)
 * @return  无
 */
static void tool_status_cb(uint32_t event, const wchar_t *p_msg, void *p_user)
{
    (void)p_msg;
    (void)p_user;
    if (g_tool_hwnd != NULL) {
        (void)PostMessageW(g_tool_hwnd, TOOL_DBG_MSG_STATUS, (WPARAM)event, 0);
    }
}

/**
 * @brief   处理连接状态通知(来自数据源线程, 经PostMessage投递)
 * @param   hwnd  主窗口句柄
 * @param   event 事件码(DBG_SOURCE_EVT_E)
 * @return  无
 */
static void tool_on_status(HWND hwnd, uint32_t event)
{
    wchar_t err[DBG_SOURCE_STATUS_LEN];

    memset(&err[0], 0, sizeof(err));
    switch (event)
    {
        case DBG_SOURCE_EVT_OPENED:
        {
            g_tool_connected = 1u;
            g_tool_relogining = 0u;
            InvalidateRect(hwnd, NULL, FALSE);
            tool_set_status(L"已连接 adb shell");
            break;
        }
        case DBG_SOURCE_EVT_CLOSED:
        {
            if (g_tool_connected != 0u) {
                g_tool_connected = 0u;
                g_tool_relogining = 0u;
                InvalidateRect(hwnd, NULL, FALSE);
                tool_set_status(L"连接已断开");
            }
            break;
        }
        case DBG_SOURCE_EVT_RELOGIN:
        {
            /* 异常断开且已配置自动重登: 保持占用态, 可随时断开取消 */
            g_tool_relogining = 1u;
            InvalidateRect(hwnd, NULL, FALSE);
            tool_set_status(L"连接断开, 自动重登中…(可点菜单 连接→断开连接 取消)");
            break;
        }
        case DBG_SOURCE_EVT_ERROR:
        {
            adb_core_get_last_error(&err[0], DBG_SOURCE_STATUS_LEN);
            if (g_tool_relogining == 0u) {
                tool_set_status(&err[0]);
            } else {
                tool_set_status(L"重登尝试失败, 按间隔继续重试…");
            }
            break;
        }
        default:
        {
            break;
        }
    }
    tool_update_statusbar();
}

/**
 * @brief   打开ADB连接对话框(菜单 连接→ADB连接…)
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void tool_on_connect_dlg(HWND hwnd)
{
    ADB_DLG_CTX_T ctx;

    if (g_tool_connected != 0u) {
        return;
    }
    memset(&ctx, 0, sizeof(ctx));
    ctx.p_rx = &g_tool_rx;
    ctx.pfn_status = &tool_status_cb;
    ctx.p_user = NULL;

    adb_dlg_open(g_tool_instance, hwnd, &ctx);
}

/**
 * @brief   断开连接(菜单)
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void tool_on_disconnect(HWND hwnd)
{
    const DBG_SOURCE_T *p_src = dbg_source_get((uint32_t)DBG_SRC_ADB);

    (void)hwnd;
    if ((p_src != NULL) && (g_tool_connected != 0u)) {
        p_src->pfn_close();
    }
}

/**
 * @brief   清空显示与计数(视图菜单)
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void tool_on_clear(HWND hwnd)
{
    HWND h_rx = GetDlgItem(hwnd, (int)TOOL_DBG_IDC_ED_RX);

    if (g_tool_use_rich != 0u) {
        (void)SendMessageW(h_rx, EM_SETREADONLY, (WPARAM)FALSE, (LPARAM)0);
        SetWindowTextW(h_rx, L"");
        (void)SendMessageW(h_rx, EM_SETREADONLY, (WPARAM)TRUE, (LPARAM)0);
    } else {
        SetWindowTextW(h_rx, L"");
    }
    g_tool_rx_pos = 0u;
    g_tool_rx_bytes = 0u;
    g_tool_tx_bytes = 0u;
    tool_update_statusbar();
    tool_set_status(L"已清空");
}

/**
 * @brief   处理发送动作: 文本转UTF-8 -> 写入数据源 -> TX回显
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void tool_on_send(HWND hwnd)
{
    const DBG_SOURCE_T *p_src = NULL;
    wchar_t text[TOOL_DBG_TX_MAX_WCHAR];
    wchar_t echo[TOOL_DBG_TX_MAX_WCHAR + 64u];
    uint8_t utf8[TOOL_DBG_TX_UTF8_MAX];
    SYSTEMTIME st;
    int32_t conv_len = 0;
    uint32_t len = 0u;

    if (g_tool_connected == 0u) {
        tool_set_status(L"未连接, 无法发送");
        return;
    }

    memset(&text[0], 0, sizeof(text));
    memset(&echo[0], 0, sizeof(echo));
    memset(&utf8[0], 0, sizeof(utf8));
    memset(&st, 0, sizeof(st));

    if (GetWindowTextW(GetDlgItem(hwnd, (int)TOOL_DBG_IDC_ED_TX),
                       &text[0], (int32_t)TOOL_DBG_TX_MAX_WCHAR) == 0) {
        tool_set_status(L"提示: 发送内容为空");
        return;
    }

    conv_len = WideCharToMultiByte(CP_UTF8, 0u, &text[0], -1,
                                   (char *)&utf8[0], (int32_t)TOOL_DBG_TX_UTF8_MAX, NULL, NULL);
    if (conv_len <= 0) {
        tool_set_status(L"错误: 发送编码失败");
        return;
    }
    len = (uint32_t)conv_len;   /* 含终结符 */
    if (SendMessageW(GetDlgItem(hwnd, (int)TOOL_DBG_IDC_CK_APPEND),
                     BM_GETCHECK, 0u, 0u) == BST_CHECKED) {
        utf8[len - 1u] = (uint8_t)'\n';   /* 以回车替换终结符 */
    } else {
        len = (len - 1u);                 /* 不带回车: 去掉终结符 */
    }

    p_src = dbg_source_get((uint32_t)DBG_SRC_ADB);
    if (p_src == NULL) {
        return;
    }
    if (p_src->pfn_write(&utf8[0], len) != 0) {
        tool_set_status(L"错误: 发送失败(连接可能已断开)");
        return;
    }
    g_tool_tx_bytes += len;

    GetLocalTime(&st);
    (void)wsprintfW(&echo[0], L"[%02u:%02u:%02u.%03u][TX] %s\r\n",
                    (unsigned int)st.wHour, (unsigned int)st.wMinute,
                    (unsigned int)st.wSecond, (unsigned int)st.wMilliseconds,
                    &text[0]);
    tool_append_rx(hwnd, &echo[0]);   /* TX回显直写主窗口(不进过滤窗口) */
    tool_update_statusbar();
    tool_set_status(L"已发送");
}

/**
 * @brief   立即保存: 接收区当前全部文本快照落盘
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void tool_on_save_now(HWND hwnd)
{
    HWND h_rx = GetDlgItem(hwnd, (int)TOOL_DBG_IDC_ED_RX);
    uint32_t len = (uint32_t)SendMessageW(h_rx, WM_GETTEXTLENGTH, 0u, 0u);
    wchar_t msg[DBG_SAVE_MSG_MAX];
    wchar_t *p_buf = NULL;

    memset(&msg[0], 0, sizeof(msg));
    if (len == 0u) {
        tool_set_status(L"提示: 接收区为空, 无可保存内容");
        return;
    }
    p_buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0u, ((SIZE_T)len + 2u) * sizeof(wchar_t));
    if (p_buf == NULL) {
        tool_set_status(L"错误: 内存不足");
        return;
    }
    (void)SendMessageW(h_rx, WM_GETTEXT, (WPARAM)(len + 1u), (LPARAM)p_buf);
    if (dbg_save_snapshot(p_buf, &msg[0]) == 0) {
        tool_set_status(&msg[0]);
    } else {
        tool_set_status(&msg[0]);
    }
    (void)HeapFree(GetProcessHeap(), 0u, p_buf);
}

/**
 * @brief   设置接收缓存档位(立即生效并持久化)
 * @param   hwnd 主窗口句柄
 * @param   idx  档位索引(TOOL_DBG_CACHE_COUNT内)
 * @return  无
 */
static void tool_on_cache(HWND hwnd, uint32_t idx)
{
    wchar_t msg[64u];

    memset(&msg[0], 0, sizeof(msg));
    if (idx >= TOOL_DBG_CACHE_COUNT) {
        return;
    }
    g_tool_cache_kb = g_tool_cache_table[idx];
    g_tool_rx_cap = g_tool_cache_kb * 1024u;
    (void)SendMessageW(GetDlgItem(hwnd, (int)TOOL_DBG_IDC_ED_RX), EM_SETLIMITTEXT,
                       (WPARAM)g_tool_rx_cap, (LPARAM)0);
    dbg_conf_set_uint(L"View", L"RxCacheKB", g_tool_cache_kb);
    (void)wsprintfW(&msg[0], L"接收缓存: %u KB", (unsigned int)g_tool_cache_kb);
    tool_set_status(&msg[0]);
}

/**
 * @brief   关于对话框
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void tool_on_about(HWND hwnd)
{
    wchar_t msg[160];

    memset(&msg[0], 0, sizeof(msg));
    (void)wsprintfW(&msg[0], L"dbgterm v" VERSION_WSTR L"\n多数据源调试终端\n\nADB 数据源已实现\n串口数据源: 第二阶段");
    (void)MessageBoxW(hwnd, &msg[0], L"关于 调试终端", (MB_OK | MB_ICONINFORMATION));
}

/**
 * @brief   处理自绘按钮动作
 * @param   hwnd    主窗口句柄
 * @param   ctrl_id 按钮逻辑ID
 * @return  无
 */
static void tool_on_button(HWND hwnd, uint32_t ctrl_id)
{
    if (ctrl_id == TOOL_DBG_IDC_BTN_SEND) {
        tool_on_send(hwnd);
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 消息处理                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief   主窗口消息处理过程
 * @param   hwnd  窗口句柄
 * @param   msg   消息类型
 * @param   wp    消息参数1
 * @param   lp    消息参数2
 * @return  消息处理结果
 */
static LRESULT CALLBACK tool_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            g_tool_hwnd = hwnd;
            dbg_source_rx_init(&g_tool_rx, hwnd, TOOL_DBG_MSG_RX);
            dbg_frame_init(&g_tool_frame);
            tool_create_menu(hwnd);
            tool_create_controls(hwnd);
            tool_layout(hwnd);
            (void)SetTimer(hwnd, TOOL_DBG_TIMER_RX, TOOL_DBG_TIMER_RX_MS, NULL);
            tool_set_status(L"就绪: 菜单 连接 → ADB 连接…");
            tool_update_statusbar();
            return 0;
        }
        case WM_SIZE:
        {
            tool_layout(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO *p_mmi = (MINMAXINFO *)lp;

            p_mmi->ptMinTrackSize.x = TOOL_DBG_MIN_TRACK_W;
            p_mmi->ptMinTrackSize.y = TOOL_DBG_MIN_TRACK_H;
            return 0;
        }
        case WM_TIMER:
        {
            if ((uint32_t)wp == TOOL_DBG_TIMER_RX) {
                if (g_tool_rx_pending != 0u) {
                    g_tool_rx_pending = 0u;
                    tool_process_rx(hwnd);
                    tool_update_statusbar();
                } else {
                    tool_process_rx(hwnd);   /* 低频处理空闲断行 */
                }
            }
            return 0;
        }
        case TOOL_DBG_MSG_RX:
        {
            g_tool_rx_pending = 1u;
            return 0;
        }
        case TOOL_DBG_MSG_STATUS:
        {
            tool_on_status(hwnd, (uint32_t)wp);
            return 0;
        }
        case WM_INITMENUPOPUP:
        {
            tool_sync_menu_checks(hwnd, (HMENU)wp);
            break;
        }
        case WM_COMMAND:
        {
            uint32_t id = (uint32_t)LOWORD(wp);
            uint32_t code = (uint32_t)HIWORD(wp);

            if (code == 0u) {
                /* 菜单项 */
                switch (id)
                {
                    case TOOL_DBG_MID_CLEAR:
                    {
                        tool_on_clear(hwnd);
                        break;
                    }
                    case TOOL_DBG_MID_EXIT:
                    {
                        DestroyWindow(hwnd);
                        break;
                    }
                    case TOOL_DBG_MID_ADB_CONNECT:
                    {
                        tool_on_connect_dlg(hwnd);
                        break;
                    }
                    case TOOL_DBG_MID_DISCONNECT:
                    {
                        tool_on_disconnect(hwnd);
                        break;
                    }
                    case TOOL_DBG_MID_TS:
                    {
                        g_tool_ts_on = (g_tool_ts_on != 0u) ? 0u : 1u;
                        tool_set_status((g_tool_ts_on != 0u)
                                            ? L"时间戳: 开(对新到日志生效)"
                                            : L"时间戳: 关");
                        break;
                    }
                    case TOOL_DBG_MID_PAUSE:
                    {
                        g_tool_pause_on = (g_tool_pause_on != 0u) ? 0u : 1u;
                        tool_set_status((g_tool_pause_on != 0u)
                                            ? L"暂停显示: 开(接收与保存继续)"
                                            : L"暂停显示: 关");
                        break;
                    }
                    case TOOL_DBG_MID_FILTERWIN:
                    {
                        if (filter_view_is_open() != 0u) {
                            filter_view_close();
                        } else {
                            filter_view_open(g_tool_instance, hwnd);
                        }
                        tool_update_statusbar();
                        break;
                    }
                    case TOOL_DBG_MID_COLORWIN:
                    {
                        if (color_view_is_open() != 0u) {
                            color_view_close();
                        } else {
                            color_view_open(g_tool_instance, hwnd);
                        }
                        break;
                    }
                    case TOOL_DBG_MID_STRIPDEV:
                    {
                        g_tool_strip_dev = (g_tool_strip_dev != 0u) ? 0u : 1u;
                        dbg_conf_set_uint(L"View", L"StripDevTs", (uint32_t)g_tool_strip_dev);
                        tool_set_status((g_tool_strip_dev != 0u)
                                            ? L"已精简设备时间戳(对新到日志生效)"
                                            : L"已保留设备时间戳");
                        break;
                    }
                    case TOOL_DBG_MID_SAVE_NOW:
                    {
                        tool_on_save_now(hwnd);
                        break;
                    }
                    case TOOL_DBG_MID_AUTOSAVE:
                    {
                        uint8_t on = (dbg_save_autosave_on() != 0u) ? 0u : 1u;

                        dbg_save_set_autosave(on);
                        tool_set_status((on != 0u) ? L"自动保存已开启(记录至保存文件夹)"
                                                   : L"自动保存已关闭");
                        tool_update_statusbar();
                        break;
                    }
                    case TOOL_DBG_MID_SAVE_DIR:
                    {
                        if (dbg_save_pick_dir(hwnd) != 0u) {
                            wchar_t dir[DBG_SAVE_DIR_MAX];
                            wchar_t msg[DBG_SAVE_DIR_MAX + 32u];

                            memset(&dir[0], 0, sizeof(dir));
                            memset(&msg[0], 0, sizeof(msg));
                            dbg_save_get_dir(&dir[0], DBG_SAVE_DIR_MAX);
                            (void)wsprintfW(&msg[0], L"保存文件夹: %s", &dir[0]);
                            tool_set_status(&msg[0]);
                        }
                        break;
                    }
                    case TOOL_DBG_MID_ABOUT:
                    {
                        tool_on_about(hwnd);
                        break;
                    }
                    default:
                    {
                        /* 接收缓存档位(3020+idx) */
                        if ((id >= TOOL_DBG_MID_CACHE_BASE) &&
                            (id < (TOOL_DBG_MID_CACHE_BASE + TOOL_DBG_CACHE_COUNT))) {
                            tool_on_cache(hwnd, (id - TOOL_DBG_MID_CACHE_BASE));
                        }
                        break;
                    }
                }
                return 0;
            }

            /* 控件通知: 其余不处理 */
            return 0;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc;
            uint32_t index = 0u;

            memset(&ps, 0, sizeof(ps));
            hdc = BeginPaint(hwnd, &ps);
            SetBkMode(hdc, TRANSPARENT);
            for (index = 0u; index < TOOL_DBG_BTN_COUNT; index++) {
                tool_draw_button(hdc, index);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            POINT pt;
            uint32_t id = 0u;
            uint32_t old_id = g_tool_hover_id;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            id = tool_hit_btn(pt);
            if (id != old_id) {
                g_tool_hover_id = id;
                tool_invalidate_btn(hwnd, old_id);
                tool_invalidate_btn(hwnd, id);
            }
            if (g_tool_mouse_tracking == 0u) {
                TRACKMOUSEEVENT tme;
                memset(&tme, 0, sizeof(tme));
                tme.cbSize = (DWORD)sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                (void)TrackMouseEvent(&tme);
                g_tool_mouse_tracking = 1u;
            }
            break;
        }
        case WM_MOUSELEAVE:
        {
            tool_invalidate_btn(hwnd, g_tool_hover_id);
            g_tool_hover_id = 0u;
            g_tool_mouse_tracking = 0u;
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            POINT pt;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            g_tool_press_id = tool_hit_btn(pt);
            if (g_tool_press_id != 0u) {
                tool_invalidate_btn(hwnd, g_tool_press_id);
                SetCapture(hwnd);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        {
            if (g_tool_press_id != 0u) {
                POINT pt;
                uint32_t id = 0u;

                ReleaseCapture();
                pt.x = (int32_t)(int16_t)LOWORD(lp);
                pt.y = (int32_t)(int16_t)HIWORD(lp);
                id = tool_hit_btn(pt);
                if (id == g_tool_press_id) {
                    tool_on_button(hwnd, id);
                }
                tool_invalidate_btn(hwnd, g_tool_press_id);
                g_tool_press_id = 0u;
            }
            return 0;
        }
        case WM_SETCURSOR:
        {
            if ((LOWORD(wp) == HTCLIENT) && (g_tool_hover_id != 0u)) {
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND));
                return (LRESULT)1;
            }
            break;
        }
        case WM_DESTROY:
        {
            const DBG_SOURCE_T *p_src = dbg_source_get((uint32_t)DBG_SRC_ADB);

            (void)KillTimer(hwnd, TOOL_DBG_TIMER_RX);
            color_view_close();
            filter_view_close();
            color_view_set_changed_fn(NULL);
            dbg_save_shutdown();
            if ((p_src != NULL) && (g_tool_connected != 0u)) {
                p_src->pfn_close();
            }
            /* 独立运行模式: 无owner顶层窗口销毁即整个进程退出 */
            if (GetWindow(hwnd, GW_OWNER) == NULL) {
                PostQuitMessage(0);
            }
            return 0;
        }
        default:
        {
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   打开调试终端主窗口(可重复打开多个实例)
 *          无owner独立运行时, 窗口销毁(WM_DESTROY)自行PostQuitMessage结束进程
 * @param   h_inst  应用实例句柄
 * @param   h_owner 所有者窗口句柄
 * @return  无
 */
void tool_dbgterm_open(HINSTANCE h_inst, HWND h_owner)
{
    WNDCLASSW wc;
    HWND h_wnd = NULL;

    (void)InitCommonControls();

    if (g_tool_class_ready == 0u) {
        memset(&wc, 0, sizeof(wc));
        wc.style = (UINT)(CS_HREDRAW | CS_VREDRAW);
        wc.lpfnWndProc = tool_wnd_proc;
        wc.hInstance = h_inst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = TOOL_DBG_CLASS_NAME;
        (void)RegisterClassW(&wc);
        g_tool_class_ready = 1u;
    }

    g_tool_instance = h_inst;

    h_wnd = CreateWindowW(TOOL_DBG_CLASS_NAME, TOOL_DBG_WND_TITLE,
                          (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN),
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          TOOL_DBG_WND_WIDTH, TOOL_DBG_WND_HEIGHT,
                          h_owner, NULL, h_inst, NULL);
    if (h_wnd != NULL) {
        ShowWindow(h_wnd, SW_SHOWNORMAL);
        UpdateWindow(h_wnd);
    }
}
