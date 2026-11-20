/**
 * @file    tool_gb32960.c
 * @author  herongshun
 * @date    2026-11-20
 * @brief   GB32960 协议解析工具界面实现(单窗体)
 *          布局: hex输入区(等宽) + 操作行[解析|打开文件…|清空|复制结果] + 结果区(只读等宽)
 *          支持粘贴/文件加载hex报文, 一键生成结构化解析报告(多帧连报自动切分)
 */

#include <windows.h>
#include <commdlg.h>
#include <stdint.h>
#include <string.h>
#include "ui_common.h"
#include "gb32960.h"
#include "tool_gb32960.h"

/* -------------------------------------------------------------------------- */
/*  宏定义: 控件ID                                                             */
/* -------------------------------------------------------------------------- */
#define GBW_IDC_ED_IN     7001u  /**< hex输入区 */
#define GBW_IDC_ED_OUT    7002u  /**< 解析结果区(只读) */

/* 自绘按钮逻辑ID */
#define GBW_IDC_BTN_PARSE 7101u  /**< 解析 */
#define GBW_IDC_BTN_OPEN  7102u  /**< 打开文件… */
#define GBW_IDC_BTN_CLEAR 7103u  /**< 清空 */
#define GBW_IDC_BTN_COPY  7104u  /**< 复制结果 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 主题颜色 (0x00BBGGRR)                                              */
/* -------------------------------------------------------------------------- */
#define GBW_CLR_ACCENT         0x005EC522  /**< 主题绿 #22C55E */
#define GBW_CLR_ACCENT_HOVER   0x0085AB18  /**< 主题绿悬停 */
#define GBW_CLR_ACCENT_PRESS   0x00848D15  /**< 主题绿按下 */
#define GBW_CLR_BTN_NEUTRAL_BG 0x00F3F4F8  /**< 中性按钮底色 */
#define GBW_CLR_BTN_NEUTRAL_HV 0x00FFFFFF  /**< 中性按钮悬停底色 */
#define GBW_CLR_BTN_NEUTRAL_PR 0x00E7EAF1  /**< 中性按钮按下底色 */
#define GBW_CLR_BTN_BORDER     0x00D5DAE4  /**< 中性按钮边框 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 窗口与布局                                                         */
/* -------------------------------------------------------------------------- */
#define GBW_CLASS_NAME    L"TS_GBW_WND"      /**< 窗口类名 */
#define GBW_WND_TITLE     L"GB32960 协议解析"  /**< 窗口标题 */
#define GBW_WND_WIDTH     900u                /**< 初始宽度 */
#define GBW_WND_HEIGHT    640u                /**< 初始高度 */
#define GBW_MIN_TRACK_W   640u                /**< 最小轨宽 */
#define GBW_MIN_TRACK_H   420u                /**< 最小轨高 */

#define GBW_MARGIN        14u    /**< 页边距 */
#define GBW_IN_Y          14u    /**< 输入区Y */
#define GBW_IN_H          150u   /**< 输入区高 */
#define GBW_BTN_ROW_Y     176u   /**< 按钮行Y(输入区下) */
#define GBW_BTN_H         32u    /**< 按钮高 */
#define GBW_BTN_GAP       10u    /**< 按钮间距 */
#define GBW_BTN_W_MAIN    110u   /**< 主按钮(解析)宽 */
#define GBW_BTN_W_SUB     110u   /**< 次按钮宽 */
#define GBW_OUT_Y         220u   /**< 结果区Y */
#define GBW_HINT_H        18u    /**< 提示行高 */

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static uint8_t   g_gbw_class_ready = 0u;                 /**< 窗口类注册标志 */
static HINSTANCE g_gbw_instance = NULL;                  /**< 应用实例句柄 */
static uint32_t  g_gbw_hover_id = 0u;                    /**< 悬停按钮ID(0=无) */
static uint32_t  g_gbw_press_id = 0u;                    /**< 按下按钮ID(0=无) */
static uint8_t   g_gbw_mouse_tracking = 0u;              /**< 鼠标离开跟踪标志 */

static RECT g_gbw_btn_rects[4u] = {0};                   /**< 自绘按钮矩形缓存 */

static const uint32_t g_gbw_btn_ids[4u] =
{
    GBW_IDC_BTN_PARSE, GBW_IDC_BTN_OPEN, GBW_IDC_BTN_CLEAR, GBW_IDC_BTN_COPY
};

static const wchar_t *const g_gbw_btn_labels[4u] =
{
    L"解 析", L"打开文件…", L"清 空", L"复制结果"
};

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   创建子控件通用封装(位置尺寸为占位值, 实际由 gbw_layout 统一排布)
 * @param   h_parent   父窗口句柄
 * @param   p_class    控件窗口类名
 * @param   style      控件扩展样式
 * @param   ctrl_id    控件ID
 * @param   font_kind  字体种类
 * @return  控件窗口句柄
 */
static HWND gbw_create_child(HWND h_parent, const wchar_t *p_class,
                             uint32_t style, uint32_t ctrl_id, uint32_t font_kind)
{
    HWND h_ctrl = CreateWindowExW(0u, p_class, L"",
                                  (WS_CHILD | WS_VISIBLE | style),
                                  0, 0, 10, 10,
                                  h_parent, (HMENU)(uintptr_t)ctrl_id, NULL, NULL);

    if (h_ctrl != NULL) {
        ui_common_apply_font_kind(h_ctrl, font_kind);
    }

    return h_ctrl;
}

/**
 * @brief   创建全部子控件
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_create_controls(HWND hwnd)
{
    (void)gbw_create_child(hwnd, L"EDIT",
                           (uint32_t)(ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN |
                                      WS_VSCROLL | WS_HSCROLL | WS_BORDER),
                           GBW_IDC_ED_IN, UI_FONT_KIND_EDIT);
    (void)gbw_create_child(hwnd, L"EDIT",
                           (uint32_t)(ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
                                      WS_VSCROLL | WS_HSCROLL | WS_BORDER),
                           GBW_IDC_ED_OUT, UI_FONT_KIND_EDIT);
    (void)SendMessageW(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN), EM_SETLIMITTEXT,
                       (WPARAM)GB32960_IN_MAX_WCHAR, (LPARAM)0);
}

/**
 * @brief   按客户区尺寸排布子控件并缓存按钮矩形
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_layout(HWND hwnd)
{
    RECT rc = {0};
    uint32_t cw = 0u;
    uint32_t ch = 0u;
    uint32_t out_y = 0u;

    GetClientRect(hwnd, &rc);
    cw = (uint32_t)(rc.right - rc.left);
    ch = (uint32_t)(rc.bottom - rc.top);
    if (cw < 300u) {
        return;
    }

    (void)MoveWindow(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN),
                     (int)GBW_MARGIN, (int)GBW_IN_Y,
                     (int)(cw - GBW_MARGIN * 2u), (int)GBW_IN_H, TRUE);

    {
        uint32_t x = GBW_MARGIN;

        g_gbw_btn_rects[0].left = (LONG)x;
        g_gbw_btn_rects[0].top = (LONG)GBW_BTN_ROW_Y;
        g_gbw_btn_rects[0].right = (LONG)(x + GBW_BTN_W_MAIN);
        g_gbw_btn_rects[0].bottom = (LONG)(GBW_BTN_ROW_Y + GBW_BTN_H);
        x += GBW_BTN_W_MAIN + GBW_BTN_GAP;
        g_gbw_btn_rects[1].left = (LONG)x;
        g_gbw_btn_rects[1].top = (LONG)GBW_BTN_ROW_Y;
        g_gbw_btn_rects[1].right = (LONG)(x + GBW_BTN_W_SUB);
        g_gbw_btn_rects[1].bottom = (LONG)(GBW_BTN_ROW_Y + GBW_BTN_H);
        x += GBW_BTN_W_SUB + GBW_BTN_GAP;
        g_gbw_btn_rects[2].left = (LONG)x;
        g_gbw_btn_rects[2].top = (LONG)GBW_BTN_ROW_Y;
        g_gbw_btn_rects[2].right = (LONG)(x + GBW_BTN_W_SUB);
        g_gbw_btn_rects[2].bottom = (LONG)(GBW_BTN_ROW_Y + GBW_BTN_H);
        x += GBW_BTN_W_SUB + GBW_BTN_GAP;
        g_gbw_btn_rects[3].left = (LONG)x;
        g_gbw_btn_rects[3].top = (LONG)GBW_BTN_ROW_Y;
        g_gbw_btn_rects[3].right = (LONG)(x + GBW_BTN_W_SUB);
        g_gbw_btn_rects[3].bottom = (LONG)(GBW_BTN_ROW_Y + GBW_BTN_H);
    }

    out_y = GBW_BTN_ROW_Y + GBW_BTN_H + GBW_MARGIN;
    if (out_y >= ch) {
        return;
    }
    (void)MoveWindow(GetDlgItem(hwnd, (int)GBW_IDC_ED_OUT),
                     (int)GBW_MARGIN, (int)out_y,
                     (int)(cw - GBW_MARGIN * 2u),
                     (int)(ch - out_y - GBW_MARGIN), TRUE);

    InvalidateRect(hwnd, NULL, FALSE);
}

/**
 * @brief   绘制单个自绘按钮(解析=主按钮, 其余中性)
 * @param   hdc   目标设备上下文
 * @param   index 按钮索引
 * @return  无
 */
static void gbw_draw_button(HDC hdc, uint32_t index)
{
    uint32_t ctrl_id = g_gbw_btn_ids[index];
    uint8_t primary = ((index == 0u) ? 1u : 0u);
    uint8_t hovered = ((g_gbw_hover_id == ctrl_id) ? 1u : 0u);
    uint8_t pressed = ((g_gbw_press_id == ctrl_id) ? 1u : 0u);
    uint32_t fill_clr = 0u;
    uint32_t border_clr = 0u;
    uint32_t text_clr = 0u;
    RECT rc = g_gbw_btn_rects[index];
    HFONT h_font_old = NULL;

    if (primary != 0u) {
        if (pressed != 0u) {
            fill_clr = GBW_CLR_ACCENT_PRESS;
        } else if (hovered != 0u) {
            fill_clr = GBW_CLR_ACCENT_HOVER;
        } else {
            fill_clr = GBW_CLR_ACCENT;
        }
        border_clr = fill_clr;
        text_clr = 0x00FFFFFFu;
    } else {
        if (pressed != 0u) {
            fill_clr = GBW_CLR_BTN_NEUTRAL_PR;
        } else if (hovered != 0u) {
            fill_clr = GBW_CLR_BTN_NEUTRAL_HV;
        } else {
            fill_clr = GBW_CLR_BTN_NEUTRAL_BG;
        }
        border_clr = GBW_CLR_BTN_BORDER;
        text_clr = 0x00283040u;
    }

    SetBkMode(hdc, TRANSPARENT);
    ui_common_fill_round_rect(hdc, &rc, 8u, fill_clr, border_clr, 1u);

    InflateRect(&rc, -9, -7);
    h_font_old = (HFONT)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_BTN));
    SetTextColor(hdc, (COLORREF)text_clr);
    ui_common_text_crisp(hdc, &rc, g_gbw_btn_labels[index],
                         (DT_CENTER | DT_VCENTER | DT_SINGLELINE), fill_clr);
    (void)SelectObject(hdc, h_font_old);
}

/**
 * @brief   命中测试
 * @param   pt 客户区坐标
 * @return  按钮逻辑ID; 无命中返回0
 */
static uint32_t gbw_hit_btn(POINT pt)
{
    uint32_t index = 0u;

    for (index = 0u; index < 4u; index++) {
        if (PtInRect(&g_gbw_btn_rects[index], pt) != 0) {
            return g_gbw_btn_ids[index];
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
static void gbw_invalidate_btn(HWND hwnd, uint32_t ctrl_id)
{
    uint32_t index = 0u;

    if (ctrl_id == 0u) {
        return;
    }
    for (index = 0u; index < 4u; index++) {
        if (g_gbw_btn_ids[index] == ctrl_id) {
            InvalidateRect(hwnd, &g_gbw_btn_rects[index], FALSE);
            return;
        }
    }
}

/**
 * @brief   执行解析: 输入区hex -> 核心解析 -> 结果区
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_on_parse(HWND hwnd)
{
    wchar_t *p_in = NULL;
    wchar_t *p_out = NULL;
    uint32_t in_len = 0u;
    uint32_t frames = 0u;

    in_len = (uint32_t)GetWindowTextLengthW(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN));
    if (in_len == 0u) {
        SetWindowTextW(GetDlgItem(hwnd, (int)GBW_IDC_ED_OUT),
                       L"提示: 请先粘贴或加载 hex 报文");
        return;
    }
    p_in = (wchar_t *)HeapAlloc(GetProcessHeap(), 0u,
                               ((SIZE_T)in_len + 2u) * sizeof(wchar_t));
    p_out = (wchar_t *)HeapAlloc(GetProcessHeap(), 0u,
                                 ((SIZE_T)GB32960_OUT_MAX_WCHAR + 2u) * sizeof(wchar_t));
    if ((p_in == NULL) || (p_out == NULL)) {
        if (p_in != NULL) {
            (void)HeapFree(GetProcessHeap(), 0u, p_in);
        }
        if (p_out != NULL) {
            (void)HeapFree(GetProcessHeap(), 0u, p_out);
        }
        SetWindowTextW(GetDlgItem(hwnd, (int)GBW_IDC_ED_OUT), L"错误: 内存不足");
        return;
    }
    (void)GetWindowTextW(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN),
                         p_in, (int32_t)(in_len + 1u));
    frames = gb32960_parse_text(p_in, p_out, GB32960_OUT_MAX_WCHAR);
    if (frames == 0u) {
        /* 全部失败: 报告本身已含原因; 若输出为空给出兜底 */
        if (p_out[0] == L'\0') {
            (void)lstrcpynW(p_out, L"✗ 解析失败", GB32960_OUT_MAX_WCHAR);
        }
    }
    SetWindowTextW(GetDlgItem(hwnd, (int)GBW_IDC_ED_OUT), p_out);
    (void)HeapFree(GetProcessHeap(), 0u, p_in);
    (void)HeapFree(GetProcessHeap(), 0u, p_out);
}

/**
 * @brief   打开报文文件(读取全部内容到输入区)
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_on_open(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t path[MAX_PATH];
    HANDLE h_file = NULL;
    DWORD size = 0u;
    DWORD read_back = 0u;
    char *p_raw = NULL;
    int32_t wide_len = 0u;
    wchar_t *p_wide = NULL;

    memset(&ofn, 0, sizeof(ofn));
    memset(&path[0], 0, sizeof(path));
    ofn.lStructSize = (DWORD)sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"报文文本\0*.txt;*.log;*.hex\0所有文件\0*.*\0";
    ofn.lpstrFile = &path[0];
    ofn.nMaxFile = (DWORD)MAX_PATH;
    ofn.Flags = (DWORD)(OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);

    if (GetOpenFileNameW(&ofn) == 0) {
        return;
    }
    h_file = CreateFileW(&path[0], GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, 0u, NULL);
    if (h_file == INVALID_HANDLE_VALUE) {
        return;
    }
    size = GetFileSize(h_file, NULL);
    if ((size == 0u) || (size > (GB32960_IN_MAX_WCHAR * 2u))) {
        (void)CloseHandle(h_file);
        SetWindowTextW(GetDlgItem(hwnd, (int)GBW_IDC_ED_OUT),
                       L"错误: 文件为空或过大(上限64KB)");
        return;
    }
    p_raw = (char *)HeapAlloc(GetProcessHeap(), 0u, (SIZE_T)size + 2u);
    p_wide = (wchar_t *)HeapAlloc(GetProcessHeap(), 0u,
                                  ((SIZE_T)GB32960_IN_MAX_WCHAR + 2u) * sizeof(wchar_t));
    if ((p_raw == NULL) || (p_wide == NULL)) {
        if (p_raw != NULL) {
            (void)HeapFree(GetProcessHeap(), 0u, p_raw);
        }
        if (p_wide != NULL) {
            (void)HeapFree(GetProcessHeap(), 0u, p_wide);
        }
        (void)CloseHandle(h_file);
        return;
    }
    (void)ReadFile(h_file, p_raw, size, &read_back, NULL);
    (void)CloseHandle(h_file);
    p_raw[read_back] = '\0';
    p_raw[read_back + 1u] = '\0';

    wide_len = MultiByteToWideChar(CP_ACP, 0u, p_raw, -1, p_wide,
                                   (int32_t)GB32960_IN_MAX_WCHAR);
    if (wide_len <= 0) {
        wide_len = MultiByteToWideChar(CP_UTF8, 0u, p_raw, -1, p_wide,
                                       (int32_t)GB32960_IN_MAX_WCHAR);
    }
    if (wide_len > 0) {
        SetWindowTextW(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN), p_wide);
        gbw_on_parse(hwnd);   /* 加载后立即解析 */
    }
    (void)HeapFree(GetProcessHeap(), 0u, p_raw);
    (void)HeapFree(GetProcessHeap(), 0u, p_wide);
}

/**
 * @brief   处理自绘按钮动作
 * @param   hwnd    主窗口句柄
 * @param   ctrl_id 按钮逻辑ID
 * @return  无
 */
static void gbw_on_button(HWND hwnd, uint32_t ctrl_id)
{
    switch (ctrl_id)
    {
        case GBW_IDC_BTN_PARSE:
        {
            gbw_on_parse(hwnd);
            break;
        }
        case GBW_IDC_BTN_OPEN:
        {
            gbw_on_open(hwnd);
            break;
        }
        case GBW_IDC_BTN_CLEAR:
        {
            SetWindowTextW(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN), L"");
            SetWindowTextW(GetDlgItem(hwnd, (int)GBW_IDC_ED_OUT), L"");
            SetFocus(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN));
            break;
        }
        case GBW_IDC_BTN_COPY:
        {
            wchar_t *p_buf = NULL;
            uint32_t len = (uint32_t)SendMessageW(GetDlgItem(hwnd, (int)GBW_IDC_ED_OUT),
                                                  WM_GETTEXTLENGTH, 0u, 0u);

            if (len == 0u) {
                return;
            }
            p_buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0u,
                                         ((SIZE_T)len + 2u) * sizeof(wchar_t));
            if (p_buf == NULL) {
                return;
            }
            (void)SendMessageW(GetDlgItem(hwnd, (int)GBW_IDC_ED_OUT),
                               WM_GETTEXT, (WPARAM)(len + 1u), (LPARAM)p_buf);
            (void)ui_common_copy_to_clipboard(hwnd, p_buf);
            (void)HeapFree(GetProcessHeap(), 0u, p_buf);
            break;
        }
        default:
        {
            break;
        }
    }
}

/**
 * @brief   主窗口消息处理过程
 * @param   hwnd  窗口句柄
 * @param   msg   消息类型
 * @param   wp    消息参数1
 * @param   lp    消息参数2
 * @return  消息处理结果
 */
static LRESULT CALLBACK gbw_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            gbw_create_controls(hwnd);
            SetFocus(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN));
            return 0;
        }
        case WM_SIZE:
        {
            gbw_layout(hwnd);
            return 0;
        }
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO *p_mmi = (MINMAXINFO *)lp;

            p_mmi->ptMinTrackSize.x = GBW_MIN_TRACK_W;
            p_mmi->ptMinTrackSize.y = GBW_MIN_TRACK_H;
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
            for (index = 0u; index < 4u; index++) {
                gbw_draw_button(hdc, index);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            POINT pt;
            uint32_t id = 0u;
            uint32_t old_id = g_gbw_hover_id;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            id = gbw_hit_btn(pt);
            if (id != old_id) {
                g_gbw_hover_id = id;
                gbw_invalidate_btn(hwnd, old_id);
                gbw_invalidate_btn(hwnd, id);
            }
            if (g_gbw_mouse_tracking == 0u) {
                TRACKMOUSEEVENT tme;
                memset(&tme, 0, sizeof(tme));
                tme.cbSize = (DWORD)sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                (void)TrackMouseEvent(&tme);
                g_gbw_mouse_tracking = 1u;
            }
            break;
        }
        case WM_MOUSELEAVE:
        {
            gbw_invalidate_btn(hwnd, g_gbw_hover_id);
            g_gbw_hover_id = 0u;
            g_gbw_mouse_tracking = 0u;
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            POINT pt;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            g_gbw_press_id = gbw_hit_btn(pt);
            if (g_gbw_press_id != 0u) {
                gbw_invalidate_btn(hwnd, g_gbw_press_id);
                SetCapture(hwnd);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        {
            if (g_gbw_press_id != 0u) {
                POINT pt;
                uint32_t id = 0u;

                ReleaseCapture();
                pt.x = (int32_t)(int16_t)LOWORD(lp);
                pt.y = (int32_t)(int16_t)HIWORD(lp);
                id = gbw_hit_btn(pt);
                if (id == g_gbw_press_id) {
                    gbw_on_button(hwnd, id);
                }
                gbw_invalidate_btn(hwnd, g_gbw_press_id);
                g_gbw_press_id = 0u;
            }
            return 0;
        }
        case WM_SETCURSOR:
        {
            if ((LOWORD(wp) == HTCLIENT) && (g_gbw_hover_id != 0u)) {
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND));
                return (LRESULT)1;
            }
            break;
        }
        case WM_DESTROY:
        {
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
 * @brief   打开 GB32960 解析工具窗口(可重复打开多个实例)
 * @param   h_inst  应用实例句柄
 * @param   h_owner 所有者窗口句柄
 * @return  无
 */
void tool_gb32960_open(HINSTANCE h_inst, HWND h_owner)
{
    WNDCLASSW wc;
    HWND h_wnd = NULL;

    if (g_gbw_class_ready == 0u) {
        memset(&wc, 0, sizeof(wc));
        wc.style = (UINT)(CS_HREDRAW | CS_VREDRAW);
        wc.lpfnWndProc = gbw_wnd_proc;
        wc.hInstance = h_inst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = GBW_CLASS_NAME;
        (void)RegisterClassW(&wc);
        g_gbw_class_ready = 1u;
    }

    g_gbw_instance = h_inst;

    h_wnd = CreateWindowW(GBW_CLASS_NAME, GBW_WND_TITLE,
                          (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN),
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          GBW_WND_WIDTH, GBW_WND_HEIGHT,
                          h_owner, NULL, h_inst, NULL);
    if (h_wnd != NULL) {
        ShowWindow(h_wnd, SW_SHOWNORMAL);
        UpdateWindow(h_wnd);
    }
}
