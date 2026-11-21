/**
 * @file    filter_view.c
 * @author  herongshun
 * @date    2026-11-17
 * @brief   过滤日志独立窗口实现
 *          多行关键字过滤框(预置6行, 可[＋]增/[－]删, 上限FLT_ROW_MAX)
 *          + 只读等宽日志区(仅命中行, >>K行号 标记);
 *          行内容逗号/分号/空格分隔多关键字; 持久化到 ini [Filter] 节
 */

#include <windows.h>
#include <commdlg.h>
#include <stdint.h>
#include <string.h>
#include "ui_common.h"
#include "dbg_filter.h"
#include "dbg_conf.h"
#include "dbg_save.h"
#include "dbg_frame.h"
#include "filter_view.h"

/* -------------------------------------------------------------------------- */
/*  宏定义: 控件ID                                                             */
/* -------------------------------------------------------------------------- */
#define FLT_VIEW_IDC_ED_BASE     5000u  /**< 关键字行编辑框基址(5000+行号) */
#define FLT_VIEW_IDC_DEL_BASE    5100u  /**< 行删除按钮基址(5100+行号) */
#define FLT_VIEW_IDC_BTN_ADD     5200u  /**< 添加过滤行按钮 */
#define FLT_VIEW_IDC_BTN_CLRLOG  5201u  /**< 清空日志按钮 */
#define FLT_VIEW_IDC_ED_LOG      5202u  /**< 过滤日志区 */
#define FLT_VIEW_IDC_BTN_AUTOSAVE 5203u /**< 自动保存开关(勾选) */
#define FLT_VIEW_IDC_BTN_SNAP    5204u  /**< 立即保存按钮 */
#define FLT_VIEW_IDC_ST_STATUS   5205u  /**< 状态反馈文本 */
#define FLT_VIEW_IDC_CLR_BASE    5300u  /**< 行取色按钮基址(5300+行号, 自绘色块) */

/* -------------------------------------------------------------------------- */
/*  宏定义: 窗口与布局                                                         */
/* -------------------------------------------------------------------------- */
#define FLT_VIEW_CLASS_NAME    L"TS_FILTER_VIEW_WND"  /**< 窗口类名 */
#define FLT_VIEW_WND_TITLE     L"过滤日志"             /**< 窗口标题 */
#define FLT_VIEW_WND_WIDTH     760u                    /**< 初始宽度 */
#define FLT_VIEW_WND_HEIGHT    560u                    /**< 初始高度 */
#define FLT_VIEW_MIN_TRACK_W   520u                    /**< 最小轨宽 */
#define FLT_VIEW_MIN_TRACK_H   320u                    /**< 最小轨高 */
#define FLT_VIEW_MARGIN        12u                     /**< 页边距 */
#define FLT_VIEW_ROW_H         26u                     /**< 行控件高 */
#define FLT_VIEW_ROW_STEP      32u                     /**< 行步进 */
#define FLT_VIEW_DELBTN_W      30u                     /**< 行删除按钮宽 */
#define FLT_VIEW_CLRBTN_W      34u                     /**< 行取色色块宽 */
#define FLT_VIEW_BTN_W         96u                     /**< 添加/清空按钮宽 */
#define FLT_VIEW_CTRL_GAP      8u                      /**< 控件间距 */
#define FLT_VIEW_ROW_DEF       6u                      /**< 预置过滤行数 */
#define FLT_ROW_MAX            12u                     /**< 过滤行数上限 */
#define FLT_ROW_TEXT_MAX       128u                    /**< 行文本宽字符容量 */
#define FLT_VIEW_LOG_MIN_H     120u                    /**< 日志区最小高度 */
#define FLT_VIEW_TEXT_MAX      1024u                   /**< 通用文本缓冲 */
#define FLT_VIEW_LOG_CAP       (256u * 1024u)          /**< 日志区文本上限(宽字符) */
#define FLT_COLOR_LINE_LIMIT   150u                    /**< 单批着色行数上限(洪峰降级为纯追加) */
#define FLT_RICHED_DLL         L"riched20.dll"          /**< RichEdit组件库 */
#define FLT_RICHED_CLASS       L"RichEdit20W"           /**< RichEdit窗口类 */

/* RichEdit着色所需本地定义(避免旧头文件差异) */
#define FLT_EM_SETCHARFORMAT   (WM_USER + 68)           /**< 设置字符格式消息 */
#define FLT_EM_POSFROMCHAR     (WM_USER + 58)           /**< 查询字符客户区坐标 */
#define FLT_EM_EXGETSEL        (WM_USER + 52)           /**< 查询选区范围(CHARRANGE) */
#define FLT_EM_GETFIRSTVISIBLELINE 0x00CE                /**< 首可见行号 */
#define FLT_EM_LINEINDEX       0x00BB                   /**< 行号->字符索引 */
#define FLT_EM_GETLINECOUNT    0x00BA                   /**< 总行数 */
#define FLT_EM_SCROLLCARET     (WM_USER + 6)            /**< 最小滚动使光标可见 */
#define FLT_SCF_SELECTION      0x0001u                  /**< 作用于当前选区 */
#define FLT_CFM_COLOR          0x40000000u              /**< 颜色掩码 */
#define FLT_CFE_AUTOCOLOR      0x40000000u              /**< 自动色效果(清除用) */
#define FLT_VIEW_LINE_WCHAR    (DBG_FRAME_LINE_MAX * 2u) /**< 命中行显示缓冲宽字符容量(与行装配上限同步) */

/* -------------------------------------------------------------------------- */
/*  内部类型                                                                    */
/* -------------------------------------------------------------------------- */

/** RichEdit字符格式(CHARFORMATW布局, 本地定义避免头文件差异) */
typedef struct FLT_CF_TAG
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
} FLT_CF_T;

/** RichEdit选区范围(CHARRANGE布局, 本地定义避免头文件差异) */
typedef struct FLT_CHARRANGE_TAG
{
    LONG      cpMin;                  /**< 选区首字符索引 */
    LONG      cpMax;                  /**< 选区尾字符索引 */
} FLT_CHARRANGE_T;

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static uint8_t   g_flt_class_ready = 0u;                       /**< 窗口类注册标志 */
static HINSTANCE g_flt_instance = NULL;                        /**< 应用实例句柄 */
static HWND      g_flt_hwnd = NULL;                            /**< 过滤窗口句柄 */
static uint32_t  g_flt_row_count = FLT_VIEW_ROW_DEF;           /**< 当前过滤行数 */
static wchar_t   g_flt_rows[FLT_ROW_MAX][FLT_ROW_TEXT_MAX];    /**< 行文本缓存 */
static uint32_t  g_flt_row_colors[FLT_ROW_MAX] = {0};          /**< 行显示色缓存 */
static uint32_t  g_flt_log_len = 0u;                           /**< 日志区文本长度缓存(EDIT降级路径) */
static uint32_t  g_flt_rich_pos = 0u;                          /**< 日志区末尾字符位置(RichEdit计量: CRLF=1字符) */
static uint32_t  g_flt_rich_len = 0u;                          /**< 日志区字符总数(RichEdit计量) */
static uint32_t  g_flt_batch = 0u;                             /**< 批处理嵌套计数 */
static uint32_t  g_flt_batch_lines = 0u;                       /**< 本批已追加行数(超限降级为纯追加) */
static uint8_t   g_flt_stick = 1u;                             /**< 批首底部跟随状态(用户上滚时暂停自动滚动) */
static uint32_t  g_flt_anchor_ch = 0u;                         /**< 锁定时的可视首行锚点字符(批量追加后钉回) */
static uint8_t   g_flt_rich_ready = 0u;                        /**< RichEdit探测标志 */
static uint8_t   g_flt_use_rich = 0u;                          /**< 日志区为RichEdit标志 */

static const uint32_t g_flt_palette[FLT_ROW_MAX] =
{
    0x003636D9u,   /* 红   #D93636 */
    0x00D57B3Au,   /* 蓝   #3A7BD5 */
    0x0039A336u,   /* 绿   #36A339 */
    0x003CA2E6u,   /* 橙   #E6A23C */
    0x00AD448Eu,   /* 紫   #8E44AD */
    0x00B8A217u,   /* 青   #17A2B8 */
    0x007B52C9u,   /* 粉   #C9527B */
    0x006B8E23u,   /* 橄榄 #238E6B */
    0x00A0522Du,   /* 棕   #2D52A0 */
    0x002F4F4Fu,   /* 灰蓝 #4F4F2F */
    0x009932CCu,   /* 洋红 #CC3299 */
    0x00808080u,   /* 灰   #808080 */
};

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 前向声明                                                     */
/* -------------------------------------------------------------------------- */
static void flt_apply_filters(void);
static void flt_save_conf(void);

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   从 ini 加载过滤行配置([Filter] Rows + Key1..N)
 * @return  无
 */
static void flt_load_conf(void)
{
    uint32_t i = 0u;

    g_flt_row_count = dbg_conf_get_uint(L"Filter", L"Rows", FLT_VIEW_ROW_DEF);
    if ((g_flt_row_count == 0u) || (g_flt_row_count > FLT_ROW_MAX)) {
        g_flt_row_count = FLT_VIEW_ROW_DEF;
    }
    for (i = 0u; i < FLT_ROW_MAX; i++) {
        wchar_t key[8u];

        memset(&g_flt_rows[i][0], 0, sizeof(g_flt_rows[i]));
        (void)wsprintfW(&key[0], L"Key%u", (unsigned int)(i + 1u));
        dbg_conf_get_str(L"Filter", &key[0], L"", &g_flt_rows[i][0], FLT_ROW_TEXT_MAX);
        g_flt_row_colors[i] = g_flt_palette[i];
        (void)wsprintfW(&key[0], L"Color%u", (unsigned int)(i + 1u));
        if (dbg_conf_get_uint(L"Filter", &key[0], 0xFFFFFFFFu) != 0xFFFFFFFFu) {
            g_flt_row_colors[i] = dbg_conf_get_uint(L"Filter", &key[0], g_flt_palette[i]);
        }
    }
}

/**
 * @brief   过滤行配置持久化到 ini
 * @return  无
 */
static void flt_save_conf(void)
{
    uint32_t i = 0u;

    dbg_conf_set_uint(L"Filter", L"Rows", g_flt_row_count);
    for (i = 0u; i < FLT_ROW_MAX; i++) {
        wchar_t key[8u];

        (void)wsprintfW(&key[0], L"Key%u", (unsigned int)(i + 1u));
        dbg_conf_set_str(L"Filter", &key[0], &g_flt_rows[i][0]);
        (void)wsprintfW(&key[0], L"Color%u", (unsigned int)(i + 1u));
        dbg_conf_set_uint(L"Filter", &key[0], g_flt_row_colors[i]);
    }
}

/**
 * @brief   将单行文本按分隔符切分为关键字写入过滤组
 * @param   group_idx 组号(=行号)
 * @param   p_text    行文本
 * @return  无
 */
static void flt_parse_row(uint32_t group_idx, const wchar_t *p_text)
{
    DBG_FILTER_GROUP_T grp;
    wchar_t token[DBG_FILTER_KEYWORD_WCHAR];
    uint32_t i = 0u;
    uint32_t tok_pos = 0u;

    memset(&grp, 0, sizeof(grp));
    memset(&token[0], 0, sizeof(token));
    (void)wsprintfW(&grp.name[0], L"行%u", (unsigned int)(group_idx + 1u));
    grp.enabled = 1u;
    grp.color = g_flt_row_colors[group_idx];

    for (i = 0u; (i < FLT_ROW_TEXT_MAX) && (p_text[i] != L'\0'); i++) {
        wchar_t ch = p_text[i];
        uint8_t is_sep = ((ch == L',') || (ch == L'，') || (ch == L';') ||
                          (ch == L'；') || (ch == L' ')) ? 1u : 0u;

        if (is_sep != 0u) {
            if (tok_pos > 0u) {
                token[tok_pos] = L'\0';
                if (grp.keyword_count < DBG_FILTER_KEYWORD_MAX) {
                    lstrcpynW(&grp.keyword[grp.keyword_count][0], &token[0],
                              (int)DBG_FILTER_KEYWORD_WCHAR);
                    grp.keyword_count++;
                }
                tok_pos = 0u;
                token[0] = L'\0';
            }
        } else if (tok_pos < (DBG_FILTER_KEYWORD_WCHAR - 1u)) {
            token[tok_pos] = ch;
            tok_pos++;
        } else {
            /* 超长关键字截断 */
        }
    }
    if (tok_pos > 0u) {
        token[tok_pos] = L'\0';
        if (grp.keyword_count < DBG_FILTER_KEYWORD_MAX) {
            lstrcpynW(&grp.keyword[grp.keyword_count][0], &token[0],
                      (int)DBG_FILTER_KEYWORD_WCHAR);
            grp.keyword_count++;
        }
    }

    dbg_filter_set_group(group_idx, &grp);
}

/**
 * @brief   将全部过滤行应用到过滤模块(空行组关键字数为0, 不参与匹配)
 * @return  无
 */
static void flt_apply_filters(void)
{
    uint32_t i = 0u;

    for (i = 0u; i < (uint32_t)DBG_FILTER_GROUP_MAX; i++) {
        if (i < g_flt_row_count) {
            flt_parse_row(i, &g_flt_rows[i][0]);
        } else {
            DBG_FILTER_GROUP_T grp;

            memset(&grp, 0, sizeof(grp));
            (void)wsprintfW(&grp.name[0], L"行%u", (unsigned int)(i + 1u));
            grp.enabled = 0u;
            grp.keyword_count = 0u;
            dbg_filter_set_group(i, &grp);
        }
    }
    dbg_filter_set_enable(1u);   /* 过滤窗口打开即启用 */
}

/**
 * @brief   创建一行过滤控件(色块+编辑框+删除按钮)
 * @param   hwnd     过滤窗口句柄
 * @param   row_idx  行号
 * @return  无
 */
static void flt_create_row(HWND hwnd, uint32_t row_idx)
{
    HWND h_clr = CreateWindowExW(0u, L"BUTTON", L"",
                                 (WS_CHILD | WS_VISIBLE | BS_OWNERDRAW),
                                 0, 0, 10, 10,
                                 hwnd, (HMENU)(uintptr_t)(FLT_VIEW_IDC_CLR_BASE + row_idx),
                                 NULL, NULL);
    HWND h_edit = CreateWindowExW(0u, L"EDIT", &g_flt_rows[row_idx][0],
                                  (WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                   ES_AUTOHSCROLL | WS_BORDER),
                                  0, 0, 10, 10,
                                  hwnd, (HMENU)(uintptr_t)(FLT_VIEW_IDC_ED_BASE + row_idx),
                                  NULL, NULL);
    HWND h_del = CreateWindowExW(0u, L"BUTTON", L"－",
                                 (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON),
                                 0, 0, 10, 10,
                                 hwnd, (HMENU)(uintptr_t)(FLT_VIEW_IDC_DEL_BASE + row_idx),
                                 NULL, NULL);

    if (h_clr != NULL) {
        (void)h_clr;
    }
    if (h_edit != NULL) {
        ui_common_apply_font_kind(h_edit, UI_FONT_KIND_EDIT);
    }
    if (h_del != NULL) {
        ui_common_apply_font_kind(h_del, UI_FONT_KIND_BTN);
    }
}

/**
 * @brief   重建全部行控件(增删行后调用)
 * @param   hwnd 过滤窗口句柄
 * @return  无
 */
static void flt_rebuild_rows(HWND hwnd)
{
    uint32_t i = 0u;

    for (i = 0u; i < FLT_ROW_MAX; i++) {
        HWND h_ed = GetDlgItem(hwnd, (int)(FLT_VIEW_IDC_ED_BASE + i));
        HWND h_del = GetDlgItem(hwnd, (int)(FLT_VIEW_IDC_DEL_BASE + i));
        HWND h_clr = GetDlgItem(hwnd, (int)(FLT_VIEW_IDC_CLR_BASE + i));

        if (h_ed != NULL) {
            (void)DestroyWindow(h_ed);
        }
        if (h_del != NULL) {
            (void)DestroyWindow(h_del);
        }
        if (h_clr != NULL) {
            (void)DestroyWindow(h_clr);
        }
    }
    for (i = 0u; i < g_flt_row_count; i++) {
        flt_create_row(hwnd, i);
    }
}

/**
 * @brief   打开取色对话框修改指定行颜色
 * @param   hwnd     过滤窗口句柄
 * @param   row_idx  行号
 * @return  1: 颜色已更换; 0: 取消
 */
static uint8_t flt_pick_color(HWND hwnd, uint32_t row_idx)
{
    CHOOSECOLORW cc;
    COLORREF cust[16u];
    uint32_t i = 0u;

    memset(&cust[0], 0, sizeof(cust));
    for (i = 0u; i < 16u; i++) {
        if (i < FLT_ROW_MAX) {
            cust[i] = (COLORREF)g_flt_row_colors[i];
        }
    }

    memset(&cc, 0, sizeof(cc));
    cc.lStructSize = (DWORD)sizeof(cc);
    cc.hwndOwner = hwnd;
    cc.rgbResult = (COLORREF)g_flt_row_colors[row_idx];
    cc.lpCustColors = &cust[0];
    cc.Flags = (CC_FULLOPEN | CC_RGBINIT);

    if (ChooseColorW(&cc) == 0) {
        return 0u;
    }
    g_flt_row_colors[row_idx] = (uint32_t)cc.rgbResult;
    flt_apply_filters();
    flt_save_conf();
    InvalidateRect(GetDlgItem(hwnd, (int)(FLT_VIEW_IDC_CLR_BASE + row_idx)), NULL, TRUE);
    return 1u;
}

/**
 * @brief   探测并加载 RichEdit 组件(一次性)
 * @return  1: 可用; 0: 不可用(日志区降级普通EDIT)
 */
static uint8_t flt_ensure_richedit(void)
{
    if (g_flt_rich_ready == 0u) {
        HMODULE h_lib = LoadLibraryW(FLT_RICHED_DLL);

        g_flt_use_rich = (h_lib != NULL) ? 1u : 0u;
        g_flt_rich_ready = 1u;
        /* 库保持加载(窗口类需要), 不释放 */
    }
    return g_flt_use_rich;
}

/**
 * @brief   按客户区尺寸排布控件(行数动态)
 * @param   hwnd 过滤窗口句柄
 * @return  无
 */
static void flt_layout(HWND hwnd)
{
    RECT rc = {0};
    uint32_t cw = 0u;
    uint32_t ch = 0u;
    uint32_t y = FLT_VIEW_MARGIN;
    uint32_t i = 0u;
    uint32_t log_y = 0u;
    uint32_t log_h = 0u;

    GetClientRect(hwnd, &rc);
    cw = (uint32_t)(rc.right - rc.left);
    ch = (uint32_t)(rc.bottom - rc.top);
    if (cw < 200u) {
        return;
    }

    /* 关键字行: [色块][编辑框][－] */
    for (i = 0u; i < g_flt_row_count; i++) {
        (void)MoveWindow(GetDlgItem(hwnd, (int)(FLT_VIEW_IDC_CLR_BASE + i)),
                         (int)FLT_VIEW_MARGIN, (int)y,
                         (int)FLT_VIEW_CLRBTN_W, (int)FLT_VIEW_ROW_H, TRUE);
        (void)MoveWindow(GetDlgItem(hwnd, (int)(FLT_VIEW_IDC_ED_BASE + i)),
                         (int)(FLT_VIEW_MARGIN + FLT_VIEW_CLRBTN_W + FLT_VIEW_CTRL_GAP), (int)y,
                         (int)(cw - FLT_VIEW_MARGIN * 2u - FLT_VIEW_CLRBTN_W -
                               FLT_VIEW_DELBTN_W - FLT_VIEW_CTRL_GAP * 2u),
                         (int)FLT_VIEW_ROW_H, TRUE);
        (void)MoveWindow(GetDlgItem(hwnd, (int)(FLT_VIEW_IDC_DEL_BASE + i)),
                         (int)(cw - FLT_VIEW_MARGIN - FLT_VIEW_DELBTN_W), (int)(y - 1u),
                         (int)FLT_VIEW_DELBTN_W, (int)FLT_VIEW_ROW_H, TRUE);
        y += FLT_VIEW_ROW_STEP;
    }

    /* 工具行: [＋ 添加过滤] [清空日志] [自动保存] [立即保存] [状态文本...] */
    y += FLT_VIEW_CTRL_GAP;
    {
        uint32_t bx = FLT_VIEW_MARGIN;
        uint32_t status_x = 0u;

        (void)MoveWindow(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_BTN_ADD),
                         (int)bx, (int)y, (int)FLT_VIEW_BTN_W, (int)FLT_VIEW_ROW_H, TRUE);
        bx += FLT_VIEW_BTN_W + FLT_VIEW_CTRL_GAP;
        (void)MoveWindow(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_BTN_CLRLOG),
                         (int)bx, (int)y, (int)FLT_VIEW_BTN_W, (int)FLT_VIEW_ROW_H, TRUE);
        bx += FLT_VIEW_BTN_W + FLT_VIEW_CTRL_GAP;
        (void)MoveWindow(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_BTN_AUTOSAVE),
                         (int)bx, (int)y, (int)FLT_VIEW_BTN_W, (int)FLT_VIEW_ROW_H, TRUE);
        bx += FLT_VIEW_BTN_W + FLT_VIEW_CTRL_GAP;
        (void)MoveWindow(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_BTN_SNAP),
                         (int)bx, (int)y, (int)FLT_VIEW_BTN_W, (int)FLT_VIEW_ROW_H, TRUE);
        bx += FLT_VIEW_BTN_W + FLT_VIEW_CTRL_GAP;
        status_x = bx;
        if (status_x < (cw - 60u)) {
            (void)MoveWindow(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_ST_STATUS),
                             (int)status_x, (int)(y + 4u),
                             (int)(cw - FLT_VIEW_MARGIN - status_x),
                             (int)FLT_VIEW_ROW_H, TRUE);
        }
    }

    /* 日志区(弹性, 保底最小高度) */
    log_y = y + FLT_VIEW_ROW_H + FLT_VIEW_CTRL_GAP;
    log_h = (ch - FLT_VIEW_MARGIN - log_y);
    if ((log_h < FLT_VIEW_LOG_MIN_H) &&
        (ch > (FLT_VIEW_LOG_MIN_H + log_y + FLT_VIEW_MARGIN))) {
        log_h = FLT_VIEW_LOG_MIN_H;
    }
    (void)MoveWindow(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_ED_LOG),
                     (int)FLT_VIEW_MARGIN, (int)log_y,
                     (int)(cw - FLT_VIEW_MARGIN * 2u), (int)log_h, TRUE);
}

/**
 * @brief   过滤窗口消息处理过程
 * @param   hwnd  窗口句柄
 * @param   msg   消息类型
 * @param   wp    消息参数1
 * @param   lp    消息参数2
 * @return  消息处理结果
 */
/**
 * @brief   更新状态反馈文本
 * @param   hwnd   过滤窗口句柄
 * @param   p_text 状态文本
 * @return  无
 */
static void flt_set_status(HWND hwnd, const wchar_t *p_text)
{
    SetWindowTextW(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_ST_STATUS),
                   (p_text != NULL) ? p_text : L"");
}

/**
 * @brief   立即保存: 过滤日志区当前全部文本快照落盘
 * @param   hwnd 过滤窗口句柄
 * @return  无
 */
static void flt_on_snapshot(HWND hwnd)
{
    HWND h_log = GetDlgItem(hwnd, (int)FLT_VIEW_IDC_ED_LOG);
    uint32_t len = (uint32_t)SendMessageW(h_log, WM_GETTEXTLENGTH, 0u, 0u);
    wchar_t msg[DBG_SAVE_MSG_MAX];
    wchar_t *p_buf = NULL;

    if (len == 0u) {
        flt_set_status(hwnd, L"提示: 过滤日志为空, 无可保存内容");
        return;
    }
    p_buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0u, ((SIZE_T)len + 2u) * sizeof(wchar_t));
    if (p_buf == NULL) {
        flt_set_status(hwnd, L"错误: 内存不足");
        return;
    }
    (void)SendMessageW(h_log, WM_GETTEXT, (WPARAM)(len + 1u), (LPARAM)p_buf);
    if (dbg_save_snapshot_ch(DBG_SAVE_CH_FILTER, p_buf, &msg[0]) == 0) {
        flt_set_status(hwnd, &msg[0]);
    } else {
        flt_set_status(hwnd, &msg[0]);
    }
    (void)HeapFree(GetProcessHeap(), 0u, p_buf);
}

static LRESULT CALLBACK flt_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            flt_load_conf();
            flt_rebuild_rows(hwnd);
            (void)CreateWindowExW(0u, L"BUTTON", L"＋ 添加过滤",
                                  (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON),
                                  0, 0, 10, 10,
                                  hwnd, (HMENU)(uintptr_t)FLT_VIEW_IDC_BTN_ADD, NULL, NULL);
            (void)CreateWindowExW(0u, L"BUTTON", L"清空日志",
                                  (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON),
                                  0, 0, 10, 10,
                                  hwnd, (HMENU)(uintptr_t)FLT_VIEW_IDC_BTN_CLRLOG, NULL, NULL);
            (void)CreateWindowExW(0u, L"BUTTON", L"自动保存",
                                  (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX),
                                  0, 0, 10, 10,
                                  hwnd, (HMENU)(uintptr_t)FLT_VIEW_IDC_BTN_AUTOSAVE, NULL, NULL);
            (void)CreateWindowExW(0u, L"BUTTON", L"立即保存",
                                  (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON),
                                  0, 0, 10, 10,
                                  hwnd, (HMENU)(uintptr_t)FLT_VIEW_IDC_BTN_SNAP, NULL, NULL);
            (void)CreateWindowExW(0u, L"STATIC", L"",
                                  (WS_CHILD | WS_VISIBLE | SS_LEFT),
                                  0, 0, 10, 10,
                                  hwnd, (HMENU)(uintptr_t)FLT_VIEW_IDC_ST_STATUS, NULL, NULL);
            if (dbg_save_autosave_on_ch(DBG_SAVE_CH_FILTER) != 0u) {
                (void)SendMessageW(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_BTN_AUTOSAVE),
                                   BM_SETCHECK, (WPARAM)BST_CHECKED, (LPARAM)0);
            }
            ui_common_apply_font_kind(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_BTN_AUTOSAVE), UI_FONT_KIND_BTN);
            ui_common_apply_font_kind(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_BTN_SNAP), UI_FONT_KIND_BTN);
            ui_common_apply_font_kind(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_ST_STATUS), UI_FONT_KIND_BTN);
            (void)CreateWindowExW(0u,
                                  (flt_ensure_richedit() != 0u) ? FLT_RICHED_CLASS : L"EDIT", L"",
                                  (WS_CHILD | WS_VISIBLE | ES_MULTILINE |
                                   ES_AUTOVSCROLL | WS_VSCROLL | WS_BORDER),
                                  0, 0, 10, 10,
                                  hwnd, (HMENU)(uintptr_t)FLT_VIEW_IDC_ED_LOG, NULL, NULL);
            ui_common_apply_font_kind(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_BTN_ADD), UI_FONT_KIND_BTN);
            ui_common_apply_font_kind(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_BTN_CLRLOG), UI_FONT_KIND_BTN);
            ui_common_apply_font_kind(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_ED_LOG), UI_FONT_KIND_EDIT);
            (void)SendMessageW(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_ED_LOG), EM_SETLIMITTEXT,
                               (WPARAM)FLT_VIEW_LOG_CAP, (LPARAM)0);
            if (g_flt_use_rich != 0u) {
                /* RichEdit只读(挡用户输入, 不挡程序追加) */
                (void)SendMessageW(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_ED_LOG),
                                   EM_SETREADONLY, (WPARAM)TRUE, (LPARAM)0);
            }
            flt_apply_filters();
            return 0;
        }
        case WM_SIZE:
        {
            flt_layout(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO *p_mmi = (MINMAXINFO *)lp;

            p_mmi->ptMinTrackSize.x = FLT_VIEW_MIN_TRACK_W;
            p_mmi->ptMinTrackSize.y = FLT_VIEW_MIN_TRACK_H;
            return 0;
        }
        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT *p_dis = (const DRAWITEMSTRUCT *)lp;

            if ((p_dis->CtlType == ODT_BUTTON) &&
                (p_dis->CtlID >= FLT_VIEW_IDC_CLR_BASE) &&
                (p_dis->CtlID < (FLT_VIEW_IDC_CLR_BASE + FLT_ROW_MAX))) {
                HBRUSH h_brush = CreateSolidBrush(
                                     (COLORREF)g_flt_row_colors[p_dis->CtlID - FLT_VIEW_IDC_CLR_BASE]);

                FillRect(p_dis->hDC, &p_dis->rcItem, h_brush);
                (void)FrameRect(p_dis->hDC, &p_dis->rcItem,
                                (HBRUSH)GetStockObject(BLACK_BRUSH));
                DeleteObject(h_brush);
                return (LRESULT)TRUE;
            }
            break;
        }
        case WM_COMMAND:
        {
            uint32_t id = (uint32_t)LOWORD(wp);
            uint32_t code = (uint32_t)HIWORD(wp);

            if ((id >= FLT_VIEW_IDC_CLR_BASE) && (id < (FLT_VIEW_IDC_CLR_BASE + FLT_ROW_MAX)) &&
                (code == BN_CLICKED)) {
                (void)flt_pick_color(hwnd, id - FLT_VIEW_IDC_CLR_BASE);
            } else if ((id >= FLT_VIEW_IDC_ED_BASE) && (id < (FLT_VIEW_IDC_ED_BASE + FLT_ROW_MAX)) &&
                (code == EN_CHANGE)) {
                uint32_t row = id - FLT_VIEW_IDC_ED_BASE;

                (void)GetWindowTextW((HWND)lp, &g_flt_rows[row][0], (int32_t)FLT_ROW_TEXT_MAX);
                flt_apply_filters();
                flt_save_conf();
            } else if ((id >= FLT_VIEW_IDC_DEL_BASE) && (id < (FLT_VIEW_IDC_DEL_BASE + FLT_ROW_MAX)) &&
                       (code == BN_CLICKED)) {
                uint32_t row = id - FLT_VIEW_IDC_DEL_BASE;
                uint32_t i = 0u;

                if ((g_flt_row_count > 1u) && (row < g_flt_row_count)) {
                    for (i = row; (i + 1u) < g_flt_row_count; i++) {
                        (void)lstrcpynW(&g_flt_rows[i][0], &g_flt_rows[i + 1u][0],
                                       (int)FLT_ROW_TEXT_MAX);
                    }
                    memset(&g_flt_rows[g_flt_row_count - 1u][0], 0, FLT_ROW_TEXT_MAX);
                    g_flt_row_count--;
                    flt_rebuild_rows(hwnd);
                    flt_layout(hwnd);
                    flt_apply_filters();
                    flt_save_conf();
                }
            } else if ((id == FLT_VIEW_IDC_BTN_ADD) && (code == BN_CLICKED)) {
                if (g_flt_row_count < FLT_ROW_MAX) {
                    memset(&g_flt_rows[g_flt_row_count][0], 0, FLT_ROW_TEXT_MAX);
                    g_flt_row_count++;
                    flt_rebuild_rows(hwnd);
                    flt_layout(hwnd);
                    flt_apply_filters();
                    flt_save_conf();
                }
            } else if ((id == FLT_VIEW_IDC_BTN_CLRLOG) && (code == BN_CLICKED)) {
                HWND h_log = GetDlgItem(hwnd, (int)FLT_VIEW_IDC_ED_LOG);

                if (g_flt_use_rich != 0u) {
                    (void)SendMessageW(h_log, EM_SETREADONLY, (WPARAM)FALSE, (LPARAM)0);
                    SetWindowTextW(h_log, L"");
                    (void)SendMessageW(h_log, EM_SETREADONLY, (WPARAM)TRUE, (LPARAM)0);
                    g_flt_rich_pos = 0u;
                    g_flt_rich_len = 0u;
                } else {
                    SetWindowTextW(h_log, L"");
                }
                g_flt_log_len = 0u;
                flt_set_status(hwnd, L"");
            } else if (id == FLT_VIEW_IDC_BTN_AUTOSAVE) {
                uint8_t on = (SendMessageW(GetDlgItem(hwnd, (int)FLT_VIEW_IDC_BTN_AUTOSAVE),
                                           BM_GETCHECK, 0u, 0u) == BST_CHECKED) ? 1u : 0u;

                dbg_save_set_autosave_ch(DBG_SAVE_CH_FILTER, on);
                flt_set_status(hwnd, (on != 0u) ? L"过滤自动保存: 开(命中行持续写入保存文件夹)"
                                                : L"过滤自动保存: 关");
            } else if ((id == FLT_VIEW_IDC_BTN_SNAP) && (code == BN_CLICKED)) {
                flt_on_snapshot(hwnd);
            } else {
                /* 其余通知不处理 */
            }
            return 0;
        }
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY:
        {
            flt_save_conf();
            g_flt_hwnd = NULL;
            dbg_filter_set_enable(0u);
            return 0;
        }
        default:
        {
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
    }

    return DefWindowProcW(hwnd, msg, wp, lp);   /* switch内break路径(如WM_DRAWITEM未命中)兜底 */
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void filter_view_open(HINSTANCE h_inst, HWND h_owner)
{
    WNDCLASSW wc;

    if (g_flt_hwnd != NULL) {
        (void)SetForegroundWindow(g_flt_hwnd);
        return;
    }

    if (g_flt_class_ready == 0u) {
        memset(&wc, 0, sizeof(wc));
        wc.style = (UINT)(CS_HREDRAW | CS_VREDRAW);
        wc.lpfnWndProc = flt_wnd_proc;
        wc.hInstance = h_inst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = FLT_VIEW_CLASS_NAME;
        if (RegisterClassW(&wc) == 0) {
            return;
        }
        g_flt_class_ready = 1u;
    }

    g_flt_instance = h_inst;
    g_flt_hwnd = CreateWindowW(FLT_VIEW_CLASS_NAME, FLT_VIEW_WND_TITLE,
                               (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN),
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               FLT_VIEW_WND_WIDTH, FLT_VIEW_WND_HEIGHT,
                               h_owner, NULL, h_inst, NULL);
    if (g_flt_hwnd != NULL) {
        ShowWindow(g_flt_hwnd, SW_SHOWNORMAL);
        UpdateWindow(g_flt_hwnd);
    }
}

uint8_t filter_view_is_open(void)
{
    return ((g_flt_hwnd != NULL) ? 1u : 0u);
}

/**
 * @brief   大小写不敏感宽字符子串查找(自指定位置起)
 * @param   p_line 行文本
 * @param   p_kw   关键字
 * @param   from   起始索引
 * @return  命中索引; 未命中返回-1
 */
static int32_t flt_find_ci(const wchar_t *p_line, const wchar_t *p_kw, uint32_t from)
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
 * @brief   判断过滤日志区是否位于最底部(自动跟随的条件)
 *          首可见行+可见行数>=总行数(±2行容差), 纯返回值消息(不依赖POSCFROMCHAR)
 * @param   h_log 日志区句柄
 * @return  1: 在底部(或内容不足一屏); 0: 用户已上滚
 */
static uint8_t flt_at_bottom(HWND h_log)
{
    if (g_flt_use_rich != 0u) {
        LRESULT first = 0;
        LRESULT lines = 0;
        RECT rc;
        uint32_t line_h = 16u;
        uint32_t visible_lines = 1u;
        HDC hdc = GetDC(h_log);
        HFONT h_font = (HFONT)SendMessageW(h_log, WM_GETFONT, 0u, 0u);
        HFONT h_old = (HFONT)SelectObject(hdc, h_font);
        TEXTMETRICW tm;

        memset(&tm, 0, sizeof(tm));
        if (GetTextMetricsW(hdc, &tm) != 0) {
            uint32_t h = (uint32_t)(tm.tmHeight + tm.tmExternalLeading);

            if (h > 0u) {
                line_h = h;
            }
        }
        (void)SelectObject(hdc, h_old);
        ReleaseDC(h_log, hdc);

        first = SendMessageW(h_log, FLT_EM_GETFIRSTVISIBLELINE, 0u, 0u);
        lines = SendMessageW(h_log, FLT_EM_GETLINECOUNT, 0u, 0u);
        memset(&rc, 0, sizeof(rc));
        GetClientRect(h_log, &rc);
        visible_lines = ((uint32_t)(rc.bottom - rc.top) + line_h - 1u) / line_h;
        if (visible_lines < 1u) {
            visible_lines = 1u;
        }

        return ((first + (LRESULT)visible_lines + 2) >= lines) ? 1u : 0u;
    }

    {
        SCROLLINFO si;

        memset(&si, 0, sizeof(si));
        si.cbSize = (UINT)sizeof(si);
        si.fMask = (SIF_PAGE | SIF_RANGE | SIF_POS);
        if (GetScrollInfo(h_log, SB_VERT, &si) == 0) {
            return 1u;
        }
        if (si.nMax <= (int)(si.nPage)) {
            return 1u;
        }

        return ((si.nPos + (int)(si.nPage) + 2) >= si.nMax) ? 1u : 0u;
    }
}

/**
 * @brief   判断用户是否正在过滤日志区选择文本(拖选进行中或已有选区)
 *          选区存续期间不得做任何光标操作/追加(会清掉选区, 导致无法复制)
 * @param   h_log 日志区句柄
 * @return  1: 正在选择或已有选区; 0: 无
 */
static uint8_t flt_user_selecting(HWND h_log)
{
    FLT_CHARRANGE_T cr;

    if (GetCapture() == h_log) {
        return 1u;   /* 鼠标按住拖选中 */
    }
    if (g_flt_use_rich == 0u) {
        return 0u;
    }

    memset(&cr, 0, sizeof(cr));
    (void)SendMessageW(h_log, FLT_EM_EXGETSEL, 0u, (LPARAM)&cr);
    return (cr.cpMin != cr.cpMax) ? 1u : 0u;
}

void filter_view_begin_batch(void)
{
    if ((g_flt_hwnd == NULL) || (g_flt_batch != 0u)) {
        g_flt_batch++;   /* 嵌套调用: 仅计数 */
        return;
    }
    g_flt_batch = 1u;
    g_flt_batch_lines = 0u;
    if (g_flt_use_rich != 0u) {
        HWND h_log = GetDlgItem(g_flt_hwnd, (int)FLT_VIEW_IDC_ED_LOG);

        g_flt_stick = flt_at_bottom(h_log);   /* 批首捕捉跟随状态 */
        if (g_flt_stick == 0u) {
            /* 锁定: 记录可视首行锚点字符供批尾钉回(对排版漂移免疫) */
            LRESULT first_line = SendMessageW(h_log, FLT_EM_GETFIRSTVISIBLELINE, 0u, 0u);
            LRESULT ch = SendMessageW(h_log, FLT_EM_LINEINDEX, (WPARAM)first_line, 0u);

            g_flt_anchor_ch = (ch >= 0) ? (uint32_t)ch : 0u;
        }
        /* 整批仅一次: 解锁+关重绘, 避免逐行解锁引发的重排/重绘风暴 */
        (void)SendMessageW(h_log, WM_SETREDRAW, (WPARAM)FALSE, (LPARAM)0);
        (void)SendMessageW(h_log, EM_SETREADONLY, (WPARAM)FALSE, (LPARAM)0);
    }
}

void filter_view_end_batch(void)
{
    if ((g_flt_batch == 0u) || (g_flt_hwnd == NULL)) {
        return;
    }
    g_flt_batch--;
    if (g_flt_batch != 0u) {
        return;
    }
    if (g_flt_use_rich != 0u) {
        HWND h_log = GetDlgItem(g_flt_hwnd, (int)FLT_VIEW_IDC_ED_LOG);

        (void)SendMessageW(h_log, EM_SETREADONLY, (WPARAM)TRUE, (LPARAM)0);
        if (g_flt_batch_lines == 0u) {
            /* 本批无追加: 只恢复控件状态, 不做任何失效/光标操作
               (每拍空转整域失效=无数据也闪烁) */
            (void)SendMessageW(h_log, WM_SETREDRAW, (WPARAM)TRUE, (LPARAM)0);
            return;
        }
        if (g_flt_stick != 0u) {
            /* 跟随: 开重绘+整域失效+拉底 */
            (void)SendMessageW(h_log, WM_SETREDRAW, (WPARAM)TRUE, (LPARAM)0);
            (void)InvalidateRect(h_log, NULL, FALSE);
            (void)SendMessageW(h_log, EM_SETSEL, (WPARAM)g_flt_rich_pos,
                               (LPARAM)g_flt_rich_pos);
            (void)SendMessageW(h_log, WM_VSCROLL, (WPARAM)SB_BOTTOM, (LPARAM)0);
        } else {
            /* 锁定: 光标钉回锚点行并最小滚回(批量追加把文末光标滚入视野),
               开重绘后不做整域失效——可视区内容未变, 整域重绘=闪烁 */
            (void)SendMessageW(h_log, EM_SETSEL, (WPARAM)g_flt_anchor_ch,
                               (LPARAM)g_flt_anchor_ch);
            (void)SendMessageW(h_log, WM_SETREDRAW, (WPARAM)TRUE, (LPARAM)0);
            (void)SendMessageW(h_log, FLT_EM_SCROLLCARET, 0u, 0u);
        }
    }
}

void filter_view_append_hit(const wchar_t *p_line, uint32_t group_idx)
{
    HWND h_log = NULL;
    DBG_FILTER_GROUP_T grp;
    wchar_t buf[FLT_VIEW_LINE_WCHAR];
    uint32_t base = 0u;
    uint32_t marker_len = 0u;
    uint32_t len = 0u;
    uint32_t k = 0u;
    uint8_t self_batch = 0u;

    if ((g_flt_hwnd == NULL) || (p_line == NULL)) {
        return;
    }
    h_log = GetDlgItem(g_flt_hwnd, (int)FLT_VIEW_IDC_ED_LOG);
    memset(&grp, 0, sizeof(grp));
    dbg_filter_get_group(group_idx, &grp);

    if (flt_user_selecting(h_log) != 0u) {
        /* 用户在日志区选择中: 命中行仅落保存通道, 暂不显示追加
           (任何光标操作/EM_REPLACESEL都会清掉选区, 导致无法复制) */
        wchar_t sbuf[FLT_VIEW_LINE_WCHAR];
        uint32_t ml = (uint32_t)wsprintfW(&sbuf[0], L">>K%u ",
                                          (unsigned int)(group_idx + 1u));

        (void)lstrcpynW(&sbuf[ml], p_line,
                        (int)(FLT_VIEW_LINE_WCHAR - ml - 2u));
        (void)lstrcatW(&sbuf[0], L"\r\n");
        dbg_save_feed_ch(DBG_SAVE_CH_FILTER, &sbuf[0]);
        return;
    }

    if (g_flt_batch == 0u) {
        filter_view_begin_batch();   /* 独立调用: 自包裹批量上下文 */
        self_batch = 1u;
    }

    if (g_flt_use_rich != 0u) {
        /* RichEdit路径: 位置/长度均按其内部计量(CRLF=1字符)自维护,
           不得使用GetWindowTextLength(其按CRLF=2计数, 会导致着色逐行右漂);
           上滚浏览时不裁剪(裁剪移动视口内容=锁定失效), 4倍硬上限时强制回底 */
        base = g_flt_rich_pos;
        if (g_flt_rich_len > FLT_VIEW_LOG_CAP) {
            uint8_t may_trim = (g_flt_stick != 0u) ? 1u : 0u;

            if ((may_trim == 0u) && (g_flt_rich_len > (FLT_VIEW_LOG_CAP * 16u))) {
                (void)SendMessageW(h_log, WM_VSCROLL, (WPARAM)SB_BOTTOM, (LPARAM)0);
                g_flt_stick = 1u;   /* 硬上限: 本批转跟随 */
                may_trim = 1u;
            }
            if (may_trim != 0u) {
                uint32_t del_chars = g_flt_rich_len / 2u;

                (void)SendMessageW(h_log, EM_SETSEL, 0, (LPARAM)del_chars);
                (void)SendMessageW(h_log, EM_REPLACESEL, FALSE, (LPARAM)L"");
                g_flt_rich_pos -= del_chars;
                g_flt_rich_len -= del_chars;
                base = g_flt_rich_pos;
            }
        }
    } else {
        if (g_flt_log_len > FLT_VIEW_LOG_CAP) {
            (void)SendMessageW(h_log, EM_SETSEL, 0, (LPARAM)(g_flt_log_len / 2u));
            (void)SendMessageW(h_log, EM_REPLACESEL, FALSE, (LPARAM)L"");
        }
        base = (uint32_t)GetWindowTextLengthW(h_log);
    }

    /* 追加整行: >>K行号 前缀 + 行文本 + CRLF (默认黑色) */
    marker_len = (uint32_t)wsprintfW(&buf[0], L">>K%u ", (unsigned int)(group_idx + 1u));
    (void)lstrcpynW(&buf[marker_len], p_line, (int)(FLT_VIEW_LINE_WCHAR - marker_len - 2u));
    (void)lstrcatW(&buf[0], L"\r\n");

    if (g_flt_use_rich != 0u) {
        /* 插入格式重置为黑色: 防止继承上一行末尾关键字的颜色 */
        FLT_CF_T cf;

        (void)SendMessageW(h_log, EM_SETSEL, (WPARAM)base, (LPARAM)base);
        memset(&cf, 0, sizeof(cf));
        cf.cbSize = (UINT)sizeof(cf);
        cf.dwMask = FLT_CFM_COLOR;
        cf.dwEffects = 0u;
        cf.crTextColor = (COLORREF)0x00000000u;
        (void)SendMessageW(h_log, FLT_EM_SETCHARFORMAT,
                           (WPARAM)FLT_SCF_SELECTION, (LPARAM)&cf);
    }
    (void)SendMessageW(h_log, EM_SETSEL, (WPARAM)base, (LPARAM)base);
    (void)SendMessageW(h_log, EM_REPLACESEL, FALSE, (LPARAM)&buf[0]);

    /* 行内关键字片段按组颜色高亮(位置=base+标记长+行内偏移);
       洪峰保护: 单批超过限额后降级为纯追加, 保UI响应 */
    if ((g_flt_use_rich != 0u) && (g_flt_batch_lines < FLT_COLOR_LINE_LIMIT)) {
        for (k = 0u; (k < grp.keyword_count) && (k < DBG_FILTER_KEYWORD_MAX); k++) {
            const wchar_t *p_kw = &grp.keyword[k][0];
            uint32_t kw_len = (uint32_t)lstrlenW(p_kw);
            uint32_t pos = 0u;

            if (kw_len == 0u) {
                continue;
            }
            for (;;) {
                int32_t hit = flt_find_ci(p_line, p_kw, pos);

                if (hit < 0) {
                    break;
                }
                {
                    uint32_t abs_start = base + marker_len + (uint32_t)hit;
                    FLT_CF_T cf;

                    memset(&cf, 0, sizeof(cf));
                    cf.cbSize = (UINT)sizeof(cf);
                    cf.dwMask = FLT_CFM_COLOR;
                    cf.dwEffects = 0u;
                    cf.crTextColor = (COLORREF)grp.color;
                    (void)SendMessageW(h_log, EM_SETSEL, (WPARAM)abs_start,
                                       (LPARAM)(abs_start + kw_len));
                    (void)SendMessageW(h_log, FLT_EM_SETCHARFORMAT,
                                       (WPARAM)FLT_SCF_SELECTION, (LPARAM)&cf);
                }
                pos = (uint32_t)hit + kw_len;
            }
        }
    }

    if (g_flt_use_rich != 0u) {
        /* 计数推进: 标记 + 行(不含\r\n) + 1(RichEdit把\r\n记为1个段落符) */
        g_flt_rich_pos += marker_len + (uint32_t)lstrlenW(p_line) + 1u;
        g_flt_rich_len = g_flt_rich_pos;
        g_flt_batch_lines++;
    } else {
        g_flt_log_len = (uint32_t)GetWindowTextLengthW(h_log);
        len = g_flt_log_len;
        (void)SendMessageW(h_log, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    }

    /* 过滤通道自动保存: 命中行(>>K标记+行文本)持续落盘 */
    dbg_save_feed_ch(DBG_SAVE_CH_FILTER, &buf[0]);

    if (self_batch != 0u) {
        filter_view_end_batch();   /* 滚动跟随在批尾按g_flt_stick决定 */
    }
}

void filter_view_close(void)
{
    if (g_flt_hwnd != NULL) {
        (void)DestroyWindow(g_flt_hwnd);
        g_flt_hwnd = NULL;
    }
}
