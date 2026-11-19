/**
 * @file    color_view.c
 * @author  herongshun
 * @date    2026-11-19
 * @brief   多彩关键字配置窗口实现
 *          布局: [色块][关键字编辑框(逗号分隔多词)][－] ×N + [＋ 添加];
 *          配置持久化 [Color] Rows/KeyN/ColorN, 变更即时回调主窗口
 */

#include <windows.h>
#include <commdlg.h>
#include <stdint.h>
#include <string.h>
#include "ui_common.h"
#include "dbg_conf.h"
#include "color_view.h"

/* -------------------------------------------------------------------------- */
/*  宏定义: 控件ID                                                             */
/* -------------------------------------------------------------------------- */
#define CLR_VIEW_IDC_ED_BASE    6000u  /**< 关键字行编辑框基址(6000+行号) */
#define CLR_VIEW_IDC_CLR_BASE   6100u  /**< 行取色按钮基址(6100+行号, 自绘色块) */
#define CLR_VIEW_IDC_DEL_BASE   6200u  /**< 行删除按钮基址(6200+行号) */
#define CLR_VIEW_IDC_BTN_ADD    6300u  /**< 添加行按钮 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 窗口与布局                                                         */
/* -------------------------------------------------------------------------- */
#define CLR_VIEW_CLASS_NAME   L"TS_COLOR_VIEW_WND"  /**< 窗口类名 */
#define CLR_VIEW_WND_TITLE    L"多彩关键字"          /**< 窗口标题 */
#define CLR_VIEW_WND_WIDTH    620u                   /**< 初始宽度 */
#define CLR_VIEW_WND_HEIGHT   440u                   /**< 初始高度 */
#define CLR_VIEW_MIN_TRACK_W  460u                   /**< 最小轨宽 */
#define CLR_VIEW_MIN_TRACK_H  260u                   /**< 最小轨高 */
#define CLR_VIEW_MARGIN       12u                    /**< 页边距 */
#define CLR_VIEW_ROW_H        26u                    /**< 行控件高 */
#define CLR_VIEW_ROW_STEP     32u                    /**< 行步进 */
#define CLR_VIEW_DELBTN_W     30u                    /**< 行删除按钮宽 */
#define CLR_VIEW_CLRBTN_W     34u                    /**< 行取色色块宽 */
#define CLR_VIEW_BTN_W        110u                   /**< 添加按钮宽 */
#define CLR_VIEW_CTRL_GAP     8u                     /**< 控件间距 */
#define CLR_VIEW_ROW_DEF      3u                     /**< 预置行数 */
#define CLR_ROW_MAX           12u                    /**< 行数上限 */
#define CLR_ROW_TEXT_MAX      128u                   /**< 行文本宽字符容量 */

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static uint8_t   g_clr_class_ready = 0u;                       /**< 窗口类注册标志 */
static HINSTANCE g_clr_instance = NULL;                        /**< 应用实例句柄 */
static HWND      g_clr_hwnd = NULL;                            /**< 配置窗口句柄 */
static uint32_t  g_clr_row_count = CLR_VIEW_ROW_DEF;           /**< 当前行数 */
static wchar_t   g_clr_rows[CLR_ROW_MAX][CLR_ROW_TEXT_MAX];    /**< 行文本缓存 */
static uint32_t  g_clr_row_colors[CLR_ROW_MAX] = {0};          /**< 行颜色缓存 */
static void      (*g_clr_pfn_changed)(void) = NULL;            /**< 配置变更回调 */

static const uint32_t g_clr_palette[CLR_ROW_MAX] =
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
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   从 ini 加载配置([Color] Rows + KeyN + ColorN)
 * @return  无
 */
static void clr_load_conf(void)
{
    uint32_t i = 0u;

    g_clr_row_count = dbg_conf_get_uint(L"Color", L"Rows", CLR_VIEW_ROW_DEF);
    if ((g_clr_row_count == 0u) || (g_clr_row_count > CLR_ROW_MAX)) {
        g_clr_row_count = CLR_VIEW_ROW_DEF;
    }
    for (i = 0u; i < CLR_ROW_MAX; i++) {
        wchar_t key[8u];

        memset(&g_clr_rows[i][0], 0, sizeof(g_clr_rows[i]));
        (void)wsprintfW(&key[0], L"Key%u", (unsigned int)(i + 1u));
        dbg_conf_get_str(L"Color", &key[0], L"", &g_clr_rows[i][0], CLR_ROW_TEXT_MAX);
        g_clr_row_colors[i] = g_clr_palette[i];
        (void)wsprintfW(&key[0], L"Color%u", (unsigned int)(i + 1u));
        if (dbg_conf_get_uint(L"Color", &key[0], 0xFFFFFFFFu) != 0xFFFFFFFFu) {
            g_clr_row_colors[i] = dbg_conf_get_uint(L"Color", &key[0], g_clr_palette[i]);
        }
    }
}

/**
 * @brief   配置持久化到 ini
 * @return  无
 */
static void clr_save_conf(void)
{
    uint32_t i = 0u;

    dbg_conf_set_uint(L"Color", L"Rows", g_clr_row_count);
    for (i = 0u; i < CLR_ROW_MAX; i++) {
        wchar_t key[8u];

        (void)wsprintfW(&key[0], L"Key%u", (unsigned int)(i + 1u));
        dbg_conf_set_str(L"Color", &key[0], &g_clr_rows[i][0]);
        (void)wsprintfW(&key[0], L"Color%u", (unsigned int)(i + 1u));
        dbg_conf_set_uint(L"Color", &key[0], g_clr_row_colors[i]);
    }
}

/**
 * @brief   通知主窗口配置已变更
 * @return  无
 */
static void clr_notify_changed(void)
{
    if (g_clr_pfn_changed != NULL) {
        g_clr_pfn_changed();
    }
}

/**
 * @brief   创建一行控件(色块+编辑框+删除按钮)
 * @param   hwnd     配置窗口句柄
 * @param   row_idx  行号
 * @return  无
 */
static void clr_create_row(HWND hwnd, uint32_t row_idx)
{
    HWND h_clr = CreateWindowExW(0u, L"BUTTON", L"",
                                 (WS_CHILD | WS_VISIBLE | BS_OWNERDRAW),
                                 0, 0, 10, 10,
                                 hwnd, (HMENU)(uintptr_t)(CLR_VIEW_IDC_CLR_BASE + row_idx),
                                 NULL, NULL);
    HWND h_edit = CreateWindowExW(0u, L"EDIT", &g_clr_rows[row_idx][0],
                                  (WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                   ES_AUTOHSCROLL | WS_BORDER),
                                  0, 0, 10, 10,
                                  hwnd, (HMENU)(uintptr_t)(CLR_VIEW_IDC_ED_BASE + row_idx),
                                  NULL, NULL);
    HWND h_del = CreateWindowExW(0u, L"BUTTON", L"－",
                                 (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON),
                                 0, 0, 10, 10,
                                 hwnd, (HMENU)(uintptr_t)(CLR_VIEW_IDC_DEL_BASE + row_idx),
                                 NULL, NULL);

    (void)h_clr;
    if (h_edit != NULL) {
        ui_common_apply_font_kind(h_edit, UI_FONT_KIND_EDIT);
    }
    if (h_del != NULL) {
        ui_common_apply_font_kind(h_del, UI_FONT_KIND_BTN);
    }
}

/**
 * @brief   重建全部行控件(增删行后调用)
 * @param   hwnd 配置窗口句柄
 * @return  无
 */
static void clr_rebuild_rows(HWND hwnd)
{
    uint32_t i = 0u;

    for (i = 0u; i < CLR_ROW_MAX; i++) {
        HWND h_ed = GetDlgItem(hwnd, (int)(CLR_VIEW_IDC_ED_BASE + i));
        HWND h_del = GetDlgItem(hwnd, (int)(CLR_VIEW_IDC_DEL_BASE + i));
        HWND h_clr = GetDlgItem(hwnd, (int)(CLR_VIEW_IDC_CLR_BASE + i));

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
    for (i = 0u; i < g_clr_row_count; i++) {
        clr_create_row(hwnd, i);
    }
}

/**
 * @brief   打开取色对话框修改指定行颜色
 * @param   hwnd     配置窗口句柄
 * @param   row_idx  行号
 * @return  1: 颜色已更换; 0: 取消
 */
static uint8_t clr_pick_color(HWND hwnd, uint32_t row_idx)
{
    CHOOSECOLORW cc;
    COLORREF cust[16u];
    uint32_t i = 0u;

    memset(&cust[0], 0, sizeof(cust));
    for (i = 0u; ((i < 16u) && (i < CLR_ROW_MAX)); i++) {
        cust[i] = (COLORREF)g_clr_row_colors[i];
    }

    memset(&cc, 0, sizeof(cc));
    cc.lStructSize = (DWORD)sizeof(cc);
    cc.hwndOwner = hwnd;
    cc.rgbResult = (COLORREF)g_clr_row_colors[row_idx];
    cc.lpCustColors = &cust[0];
    cc.Flags = (CC_FULLOPEN | CC_RGBINIT);

    if (ChooseColorW(&cc) == 0) {
        return 0u;
    }
    g_clr_row_colors[row_idx] = (uint32_t)cc.rgbResult;
    clr_save_conf();
    clr_notify_changed();
    InvalidateRect(GetDlgItem(hwnd, (int)(CLR_VIEW_IDC_CLR_BASE + row_idx)), NULL, TRUE);
    return 1u;
}

/**
 * @brief   按客户区尺寸排布控件
 * @param   hwnd 配置窗口句柄
 * @return  无
 */
static void clr_layout(HWND hwnd)
{
    RECT rc = {0};
    uint32_t cw = 0u;
    uint32_t y = CLR_VIEW_MARGIN;
    uint32_t i = 0u;

    GetClientRect(hwnd, &rc);
    cw = (uint32_t)(rc.right - rc.left);
    if (cw < 200u) {
        return;
    }

    for (i = 0u; i < g_clr_row_count; i++) {
        (void)MoveWindow(GetDlgItem(hwnd, (int)(CLR_VIEW_IDC_CLR_BASE + i)),
                         (int)CLR_VIEW_MARGIN, (int)y,
                         (int)CLR_VIEW_CLRBTN_W, (int)CLR_VIEW_ROW_H, TRUE);
        (void)MoveWindow(GetDlgItem(hwnd, (int)(CLR_VIEW_IDC_ED_BASE + i)),
                         (int)(CLR_VIEW_MARGIN + CLR_VIEW_CLRBTN_W + CLR_VIEW_CTRL_GAP), (int)y,
                         (int)(cw - CLR_VIEW_MARGIN * 2u - CLR_VIEW_CLRBTN_W -
                               CLR_VIEW_DELBTN_W - CLR_VIEW_CTRL_GAP * 2u),
                         (int)CLR_VIEW_ROW_H, TRUE);
        (void)MoveWindow(GetDlgItem(hwnd, (int)(CLR_VIEW_IDC_DEL_BASE + i)),
                         (int)(cw - CLR_VIEW_MARGIN - CLR_VIEW_DELBTN_W), (int)(y - 1u),
                         (int)CLR_VIEW_DELBTN_W, (int)CLR_VIEW_ROW_H, TRUE);
        y += CLR_VIEW_ROW_STEP;
    }

    y += CLR_VIEW_CTRL_GAP;
    (void)MoveWindow(GetDlgItem(hwnd, (int)CLR_VIEW_IDC_BTN_ADD),
                     (int)CLR_VIEW_MARGIN, (int)y,
                     (int)CLR_VIEW_BTN_W, (int)CLR_VIEW_ROW_H, TRUE);
}

/**
 * @brief   配置窗口消息处理过程
 * @param   hwnd  窗口句柄
 * @param   msg   消息类型
 * @param   wp    消息参数1
 * @param   lp    消息参数2
 * @return  消息处理结果
 */
static LRESULT CALLBACK clr_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            clr_load_conf();
            clr_rebuild_rows(hwnd);
            (void)CreateWindowExW(0u, L"BUTTON", L"＋ 添加关键字",
                                  (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON),
                                  0, 0, 10, 10,
                                  hwnd, (HMENU)(uintptr_t)CLR_VIEW_IDC_BTN_ADD, NULL, NULL);
            ui_common_apply_font_kind(GetDlgItem(hwnd, (int)CLR_VIEW_IDC_BTN_ADD),
                                      UI_FONT_KIND_BTN);
            clr_notify_changed();   /* 主窗口启动时词表可能尚未建立 */
            return 0;
        }
        case WM_SIZE:
        {
            clr_layout(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO *p_mmi = (MINMAXINFO *)lp;

            p_mmi->ptMinTrackSize.x = CLR_VIEW_MIN_TRACK_W;
            p_mmi->ptMinTrackSize.y = CLR_VIEW_MIN_TRACK_H;
            return 0;
        }
        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT *p_dis = (const DRAWITEMSTRUCT *)lp;

            if ((p_dis->CtlType == ODT_BUTTON) &&
                (p_dis->CtlID >= CLR_VIEW_IDC_CLR_BASE) &&
                (p_dis->CtlID < (CLR_VIEW_IDC_CLR_BASE + CLR_ROW_MAX))) {
                HBRUSH h_brush = CreateSolidBrush(
                                     (COLORREF)g_clr_row_colors[p_dis->CtlID - CLR_VIEW_IDC_CLR_BASE]);

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

            if ((id >= CLR_VIEW_IDC_CLR_BASE) && (id < (CLR_VIEW_IDC_CLR_BASE + CLR_ROW_MAX)) &&
                (code == BN_CLICKED)) {
                (void)clr_pick_color(hwnd, id - CLR_VIEW_IDC_CLR_BASE);
            } else if ((id >= CLR_VIEW_IDC_ED_BASE) && (id < (CLR_VIEW_IDC_ED_BASE + CLR_ROW_MAX)) &&
                       (code == EN_CHANGE)) {
                uint32_t row = id - CLR_VIEW_IDC_ED_BASE;

                (void)GetWindowTextW((HWND)lp, &g_clr_rows[row][0], (int32_t)CLR_ROW_TEXT_MAX);
                clr_save_conf();
                clr_notify_changed();
            } else if ((id >= CLR_VIEW_IDC_DEL_BASE) && (id < (CLR_VIEW_IDC_DEL_BASE + CLR_ROW_MAX)) &&
                       (code == BN_CLICKED)) {
                uint32_t row = id - CLR_VIEW_IDC_DEL_BASE;
                uint32_t i = 0u;

                if ((g_clr_row_count > 1u) && (row < g_clr_row_count)) {
                    for (i = row; (i + 1u) < g_clr_row_count; i++) {
                        (void)lstrcpynW(&g_clr_rows[i][0], &g_clr_rows[i + 1u][0],
                                       (int)CLR_ROW_TEXT_MAX);
                        g_clr_row_colors[i] = g_clr_row_colors[i + 1u];
                    }
                    memset(&g_clr_rows[g_clr_row_count - 1u][0], 0, CLR_ROW_TEXT_MAX);
                    g_clr_row_colors[g_clr_row_count - 1u] = 0u;
                    g_clr_row_count--;
                    clr_rebuild_rows(hwnd);
                    clr_layout(hwnd);
                    clr_save_conf();
                    clr_notify_changed();
                }
            } else if ((id == CLR_VIEW_IDC_BTN_ADD) && (code == BN_CLICKED)) {
                if (g_clr_row_count < CLR_ROW_MAX) {
                    memset(&g_clr_rows[g_clr_row_count][0], 0, CLR_ROW_TEXT_MAX);
                    g_clr_row_count++;
                    clr_rebuild_rows(hwnd);
                    clr_layout(hwnd);
                    clr_save_conf();
                    clr_notify_changed();
                }
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
            clr_save_conf();
            g_clr_hwnd = NULL;
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

void color_view_open(HINSTANCE h_inst, HWND h_owner)
{
    WNDCLASSW wc;

    if (g_clr_hwnd != NULL) {
        (void)SetForegroundWindow(g_clr_hwnd);
        return;
    }

    if (g_clr_class_ready == 0u) {
        memset(&wc, 0, sizeof(wc));
        wc.style = (UINT)(CS_HREDRAW | CS_VREDRAW);
        wc.lpfnWndProc = clr_wnd_proc;
        wc.hInstance = h_inst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = CLR_VIEW_CLASS_NAME;
        if (RegisterClassW(&wc) == 0) {
            return;
        }
        g_clr_class_ready = 1u;
    }

    g_clr_instance = h_inst;
    g_clr_hwnd = CreateWindowW(CLR_VIEW_CLASS_NAME, CLR_VIEW_WND_TITLE,
                               (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN),
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               CLR_VIEW_WND_WIDTH, CLR_VIEW_WND_HEIGHT,
                               h_owner, NULL, h_inst, NULL);
    if (g_clr_hwnd != NULL) {
        ShowWindow(g_clr_hwnd, SW_SHOWNORMAL);
        UpdateWindow(g_clr_hwnd);
    }
}

uint8_t color_view_is_open(void)
{
    return ((g_clr_hwnd != NULL) ? 1u : 0u);
}

void color_view_close(void)
{
    if (g_clr_hwnd != NULL) {
        (void)DestroyWindow(g_clr_hwnd);
        g_clr_hwnd = NULL;
    }
}

void color_view_set_changed_fn(void (*pfn)(void))
{
    g_clr_pfn_changed = pfn;
}
