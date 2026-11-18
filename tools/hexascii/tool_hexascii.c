/**
 * @file    tool_hexascii.c
 * @author  herongshun
 * @date    2026-10-26
 * @brief   Hex-ASCII 双向转换工具界面实现
 *          输入/粘贴文本后点击方向按钮执行转换, 结果可一键复制
 *          按钮为自绘彩色扁平样式, 状态栏按语义着色
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "hex_conv.h"
#include "tool_hexascii.h"
#include "ui_common.h"

/* -------------------------------------------------------------------------- */
/*  宏定义: 控件ID                                                             */
/* -------------------------------------------------------------------------- */
#define TOOL_HEX_IDC_EDIT_INPUT    2001u  /**< 输入编辑框 */
#define TOOL_HEX_IDC_EDIT_OUTPUT   2002u  /**< 输出编辑框 */
#define TOOL_HEX_IDC_STATUS        2003u  /**< 状态提示文本 */
#define TOOL_HEX_IDC_LBL_INPUT     2004u  /**< 输入区标签 */
#define TOOL_HEX_IDC_LBL_OUTPUT    2005u  /**< 输出区标签 */
#define TOOL_HEX_IDC_BTN_HEX2ASCII 2101u  /**< 自绘按钮逻辑ID: Hex转ASCII */
#define TOOL_HEX_IDC_BTN_ASCII2HEX 2102u  /**< 自绘按钮逻辑ID: ASCII转Hex */
#define TOOL_HEX_IDC_BTN_COPY      2103u  /**< 自绘按钮逻辑ID: 复制结果 */
#define TOOL_HEX_IDC_BTN_CLEAR     2104u  /**< 自绘按钮逻辑ID: 清空 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 主题颜色 (0x00BBGGRR)                                              */
/* -------------------------------------------------------------------------- */
#define TOOL_HEX_CLR_ACCENT         0x00F6823B  /**< 主题蓝 #3B82F6 */
#define TOOL_HEX_CLR_ACCENT_HOVER   0x00D96F2E  /**< 主题蓝悬停 #2E6FD9 */
#define TOOL_HEX_CLR_ACCENT_PRESS   0x00BB5F27  /**< 主题蓝按下 #275FBB */
#define TOOL_HEX_CLR_BTN_NEUTRAL_BG 0x00F3F4F8  /**< 中性按钮底色 */
#define TOOL_HEX_CLR_BTN_NEUTRAL_HV 0x00FFFFFF  /**< 中性按钮悬停底色 */
#define TOOL_HEX_CLR_BTN_NEUTRAL_PR 0x00E7EAF1  /**< 中性按钮按下底色 */
#define TOOL_HEX_CLR_BTN_BORDER     0x00D5DAE4  /**< 中性按钮边框 */
#define TOOL_HEX_CLR_TEXT_LIGHT     0x00FFFFFF  /**< 亮色文字(主按钮) */
#define TOOL_HEX_CLR_TEXT_DARK      0x00283040  /**< 深色文字(中性按钮) */
#define TOOL_HEX_CLR_LABEL          0x00303A4A  /**< 标签文字色 */
#define TOOL_HEX_CLR_STATUS_OK      0x00388018  /**< 状态: 成功绿 */
#define TOOL_HEX_CLR_STATUS_ERR     0x002530D9  /**< 状态: 错误红 */
#define TOOL_HEX_CLR_STATUS_INFO    0x0068635F  /**< 状态: 提示灰 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 窗口与布局                                                         */
/* -------------------------------------------------------------------------- */
#define TOOL_HEX_CLASS_NAME        L"TS_TOOL_HEX_WND"   /**< 窗口类名 */
#define TOOL_HEX_WND_TITLE         L"Hex ⇄ ASCII 字符转换"  /**< 窗口标题 */
#define TOOL_HEX_WND_WIDTH         720u                 /**< 初始宽度 */
#define TOOL_HEX_WND_HEIGHT        560u                 /**< 初始高度 */
#define TOOL_HEX_MARGIN            16u                  /**< 四周边距 */
#define TOOL_HEX_GAP               8u                   /**< 控件间距 */
#define TOOL_HEX_LABEL_H           22u                  /**< 标签高度 */
#define TOOL_HEX_BTN_H             36u                  /**< 按钮高度 */
#define TOOL_HEX_BTN_W             140u                 /**< 按钮宽度 */
#define TOOL_HEX_BTN_RADIUS        8u                   /**< 按钮圆角半径 */
#define TOOL_HEX_BTN_BORDER_W      1u                   /**< 按钮边框宽度 */
#define TOOL_HEX_BTN_COUNT         4u                   /**< 自绘按钮数量 */
#define TOOL_HEX_BTN_IDX_HEX2ASCII 0u                   /**< 按钮索引: Hex转ASCII */
#define TOOL_HEX_BTN_IDX_ASCII2HEX 1u                   /**< 按钮索引: ASCII转Hex */
#define TOOL_HEX_BTN_IDX_COPY      2u                   /**< 按钮索引: 复制结果 */
#define TOOL_HEX_BTN_IDX_CLEAR     3u                   /**< 按钮索引: 清空 */
#define TOOL_HEX_INPUT_EDIT_H      150u                 /**< 输入编辑框高度 */
#define TOOL_HEX_STATUS_H          22u                  /**< 状态栏高度 */
#define TOOL_HEX_TRUE              1u                   /**< 布尔真 */
#define TOOL_HEX_FALSE             0u                   /**< 布尔假 */
#define TOOL_HEX_DIR_HEX2ASCII     1u                   /**< 转换方向: Hex转ASCII */
#define TOOL_HEX_DIR_ASCII2HEX     0u                   /**< 转换方向: ASCII转Hex */

/* -------------------------------------------------------------------------- */
/*  宏定义: 缓冲区容量                                                         */
/* -------------------------------------------------------------------------- */
#define TOOL_HEX_INPUT_MAX_WCHAR  (HEX_CONV_MAX_BYTES * 2u)    /**< 输入宽字符缓冲容量 */
#define TOOL_HEX_OUTPUT_MAX_WCHAR (HEX_CONV_HEX_STR_MAX + 2u)  /**< 输出宽字符缓冲容量 */
#define TOOL_HEX_STATUS_MAX_WCHAR 256u                         /**< 状态文本缓冲容量 */

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static uint8_t  g_tool_hex_class_ready = TOOL_HEX_FALSE;               /**< 窗口类注册标志 */
static HINSTANCE g_tool_hex_instance = NULL;                           /**< 应用实例句柄 */
static uint32_t g_tool_hex_status_color = TOOL_HEX_CLR_STATUS_INFO;    /**< 状态栏当前文字色 */
static uint32_t g_tool_hex_hover_id = 0u;                              /**< 悬停按钮控件ID(0=无) */
static uint32_t g_tool_hex_press_id = 0u;                              /**< 按下按钮控件ID(0=无) */
static uint8_t  g_tool_hex_mouse_tracking = TOOL_HEX_FALSE;            /**< 鼠标离开跟踪标志 */
static RECT     g_tool_hex_btn_rects[TOOL_HEX_BTN_COUNT] = {0};        /**< 自绘按钮矩形缓存 */

static const uint32_t g_tool_hex_btn_ids[TOOL_HEX_BTN_COUNT] =
{
    TOOL_HEX_IDC_BTN_HEX2ASCII,
    TOOL_HEX_IDC_BTN_ASCII2HEX,
    TOOL_HEX_IDC_BTN_COPY,
    TOOL_HEX_IDC_BTN_CLEAR
};

static const wchar_t *const g_tool_hex_btn_labels[TOOL_HEX_BTN_COUNT] =
{
    L"Hex → ASCII", L"ASCII → Hex", L"复制结果", L"清空"
};

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   创建子控件通用封装(自动附加WS_CHILD|WS_VISIBLE并应用共享字体)
 * @param   h_parent   父窗口句柄
 * @param   p_class    控件窗口类名(如L"EDIT"/L"STATIC")
 * @param   p_text     控件初始文本
 * @param   style      控件扩展样式
 * @param   ctrl_id    控件ID
 * @param   font_kind  字体种类(UI_FONT_KIND_E)
 * @return  控件窗口句柄; 失败返回NULL
 */
static HWND tool_hex_create_child(HWND h_parent,
                                  const wchar_t *p_class,
                                  const wchar_t *p_text,
                                  uint32_t style,
                                  uint32_t ctrl_id,
                                  uint32_t font_kind)
{
    HWND h_ctrl = CreateWindowExW(0u, p_class, p_text,
                                  (WS_CHILD | WS_VISIBLE | style),
                                  TOOL_HEX_MARGIN, TOOL_HEX_MARGIN, TOOL_HEX_BTN_W, TOOL_HEX_BTN_H,
                                  h_parent, (HMENU)(uintptr_t)ctrl_id, NULL, NULL);
    if (h_ctrl != NULL) {
        ui_common_apply_font_kind(h_ctrl, font_kind);
    }

    return h_ctrl;
}

/**
 * @brief   创建工具窗口内的全部子控件(标签/编辑框)
 * @param   hwnd 工具窗口句柄
 * @return  无
 */
static void tool_hex_create_controls(HWND hwnd)
{
    uint32_t edit_input_style  = (uint32_t)(ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                                            ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL | WS_BORDER);
    uint32_t edit_output_style = (uint32_t)(ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                                            ES_READONLY | WS_VSCROLL | WS_HSCROLL | WS_BORDER);

    (void)tool_hex_create_child(hwnd, L"STATIC", L"输入 (Hex 字符串或 ASCII 文本):",
                                SS_LEFT, TOOL_HEX_IDC_LBL_INPUT, UI_FONT_KIND_TEXT);
    (void)tool_hex_create_child(hwnd, L"EDIT", L"", edit_input_style,
                                TOOL_HEX_IDC_EDIT_INPUT, UI_FONT_KIND_EDIT);
    (void)tool_hex_create_child(hwnd, L"STATIC", L"输出 (只读):", SS_LEFT,
                                TOOL_HEX_IDC_LBL_OUTPUT, UI_FONT_KIND_TEXT);
    (void)tool_hex_create_child(hwnd, L"EDIT", L"", edit_output_style,
                                TOOL_HEX_IDC_EDIT_OUTPUT, UI_FONT_KIND_EDIT);
    (void)tool_hex_create_child(hwnd, L"STATIC", L"", SS_LEFT,
                                TOOL_HEX_IDC_STATUS, UI_FONT_KIND_BTN);

    /* 限制输入长度, 防止超出转换缓冲容量 */
    SendMessageW(GetDlgItem(hwnd, (int)TOOL_HEX_IDC_EDIT_INPUT), EM_SETLIMITTEXT,
                 (WPARAM)(TOOL_HEX_INPUT_MAX_WCHAR - 1u), (LPARAM)0);
}

/**
 * @brief   按客户区尺寸排布子控件并缓存自绘按钮矩形
 * @param   hwnd 工具窗口句柄
 * @return  无
 */
static void tool_hex_layout(HWND hwnd)
{
    RECT rc = {0};
    uint32_t client_w = 0u;
    uint32_t client_h = 0u;
    uint32_t x = 0u;
    uint32_t y = 0u;
    uint32_t w = 0u;
    uint32_t btn_x = 0u;
    uint32_t index = 0u;

    GetClientRect(hwnd, &rc);
    client_w = (uint32_t)(rc.right - rc.left);
    client_h = (uint32_t)(rc.bottom - rc.top);

    x = TOOL_HEX_MARGIN;
    y = TOOL_HEX_MARGIN;
    w = client_w - (TOOL_HEX_MARGIN * 2u);

    MoveWindow(GetDlgItem(hwnd, (int)TOOL_HEX_IDC_LBL_INPUT), x, y, w, TOOL_HEX_LABEL_H, TRUE);
    y += TOOL_HEX_LABEL_H;
    MoveWindow(GetDlgItem(hwnd, (int)TOOL_HEX_IDC_EDIT_INPUT), x, y, w, TOOL_HEX_INPUT_EDIT_H, TRUE);
    y += TOOL_HEX_INPUT_EDIT_H + TOOL_HEX_GAP;

    /* 自绘按钮行 */
    btn_x = x;
    for (index = 0u; index < TOOL_HEX_BTN_COUNT; index++) {
        g_tool_hex_btn_rects[index].left = (LONG)btn_x;
        g_tool_hex_btn_rects[index].top = (LONG)y;
        g_tool_hex_btn_rects[index].right = (LONG)(btn_x + TOOL_HEX_BTN_W);
        g_tool_hex_btn_rects[index].bottom = (LONG)(y + TOOL_HEX_BTN_H);
        btn_x += TOOL_HEX_BTN_W + TOOL_HEX_GAP;
    }
    y += TOOL_HEX_BTN_H + TOOL_HEX_GAP;

    MoveWindow(GetDlgItem(hwnd, (int)TOOL_HEX_IDC_LBL_OUTPUT), x, y, w, TOOL_HEX_LABEL_H, TRUE);
    y += TOOL_HEX_LABEL_H;

    MoveWindow(GetDlgItem(hwnd, (int)TOOL_HEX_IDC_STATUS), x,
               (client_h - TOOL_HEX_MARGIN - TOOL_HEX_STATUS_H), w, TOOL_HEX_STATUS_H, TRUE);

    {
        uint32_t out_h = (client_h - TOOL_HEX_MARGIN - TOOL_HEX_STATUS_H - TOOL_HEX_GAP) - y;
        MoveWindow(GetDlgItem(hwnd, (int)TOOL_HEX_IDC_EDIT_OUTPUT), x, y, w, out_h, TRUE);
    }
}

/**
 * @brief   绘制单个自绘按钮(主按钮为主题色填充, 中性按钮为浅色底描边)
 * @param   hdc     目标设备上下文
 * @param   index   按钮索引(对应ID/标签表)
 * @return  无
 */
static void tool_hex_draw_button(HDC hdc, uint32_t index)
{
    uint32_t ctrl_id = g_tool_hex_btn_ids[index];
    uint8_t primary = (((index == TOOL_HEX_BTN_IDX_HEX2ASCII) || (index == TOOL_HEX_BTN_IDX_ASCII2HEX))
                       ? TOOL_HEX_TRUE : TOOL_HEX_FALSE);
    uint8_t hovered = ((g_tool_hex_hover_id == ctrl_id) ? TOOL_HEX_TRUE : TOOL_HEX_FALSE);
    uint8_t pressed = ((g_tool_hex_press_id == ctrl_id) ? TOOL_HEX_TRUE : TOOL_HEX_FALSE);
    uint32_t fill_clr = 0u;
    uint32_t border_clr = 0u;
    uint32_t text_clr = 0u;
    RECT rc = g_tool_hex_btn_rects[index];
    HFONT h_font_old = NULL;

    if (primary != TOOL_HEX_FALSE) {
        if (pressed != TOOL_HEX_FALSE) {
            fill_clr = TOOL_HEX_CLR_ACCENT_PRESS;
        } else if (hovered != TOOL_HEX_FALSE) {
            fill_clr = TOOL_HEX_CLR_ACCENT_HOVER;
        } else {
            fill_clr = TOOL_HEX_CLR_ACCENT;
        }
        border_clr = fill_clr;
        text_clr = TOOL_HEX_CLR_TEXT_LIGHT;
    } else {
        if (pressed != TOOL_HEX_FALSE) {
            fill_clr = TOOL_HEX_CLR_BTN_NEUTRAL_PR;
        } else if (hovered != TOOL_HEX_FALSE) {
            fill_clr = TOOL_HEX_CLR_BTN_NEUTRAL_HV;
        } else {
            fill_clr = TOOL_HEX_CLR_BTN_NEUTRAL_BG;
        }
        border_clr = TOOL_HEX_CLR_BTN_BORDER;
        text_clr = TOOL_HEX_CLR_TEXT_DARK;
    }

    SetBkMode(hdc, TRANSPARENT);
    ui_common_fill_round_rect(hdc, &rc, TOOL_HEX_BTN_RADIUS, fill_clr, border_clr, TOOL_HEX_BTN_BORDER_W);

    h_font_old = (HFONT)SelectObject(hdc, ui_common_get_font(
                     (primary != TOOL_HEX_FALSE) ? UI_FONT_KIND_BTN_COLOR : UI_FONT_KIND_BTN));
    SetTextColor(hdc, (COLORREF)text_clr);
    DrawTextW(hdc, g_tool_hex_btn_labels[index], -1, &rc, (DT_CENTER | DT_VCENTER | DT_SINGLELINE));
    (void)SelectObject(hdc, h_font_old);
}

/**
 * @brief   命中测试: 判断客户区坐标落在哪个自绘按钮上
 * @param   pt  客户区坐标
 * @return  按钮控件ID; 不在任何按钮上返回0
 */
static uint32_t tool_hex_hit_btn(POINT pt)
{
    uint32_t index = 0u;

    for (index = 0u; index < TOOL_HEX_BTN_COUNT; index++) {
        if (PtInRect(&g_tool_hex_btn_rects[index], pt) != 0) {
            return g_tool_hex_btn_ids[index];
        }
    }

    return 0u;
}

/**
 * @brief   使指定按钮矩形区域重绘
 * @param   hwnd        工具窗口句柄
 * @param   ctrl_id     按钮控件ID(0时忽略)
 * @return  无
 */
static void tool_hex_invalidate_btn(HWND hwnd, uint32_t ctrl_id)
{
    uint32_t index = 0u;

    if (ctrl_id == 0u) {
        return;
    }
    for (index = 0u; index < TOOL_HEX_BTN_COUNT; index++) {
        if (g_tool_hex_btn_ids[index] == ctrl_id) {
            InvalidateRect(hwnd, &g_tool_hex_btn_rects[index], FALSE);
            return;
        }
    }
}

/**
 * @brief   更新状态提示文本及其语义颜色
 * @param   hwnd    工具窗口句柄
 * @param   p_msg   状态文本(以'\0'结尾)
 * @param   color   状态文字色(TOOL_HEX_CLR_STATUS_xxx)
 * @return  无
 */
static void tool_hex_set_status(HWND hwnd, const wchar_t *p_msg, uint32_t color)
{
    g_tool_hex_status_color = color;
    SetWindowTextW(GetDlgItem(hwnd, (int)TOOL_HEX_IDC_STATUS), p_msg);
}

/**
 * @brief   执行转换并刷新输出与状态栏
 * @param   hwnd          工具窗口句柄
 * @param   hex_to_ascii  转换方向: 1=Hex转ASCII, 0=ASCII转Hex
 * @return  无
 */
static void tool_hex_do_convert(HWND hwnd, uint8_t hex_to_ascii)
{
    wchar_t   input_text[TOOL_HEX_INPUT_MAX_WCHAR]   = {0};
    wchar_t   output_text[TOOL_HEX_OUTPUT_MAX_WCHAR] = {0};
    wchar_t   status_text[TOOL_HEX_STATUS_MAX_WCHAR] = {0};
    char      utf8_buf[HEX_CONV_MAX_BYTES + 1]       = {0};
    char      hex_buf[HEX_CONV_HEX_STR_MAX]          = {0};
    uint8_t   bytes_buf[HEX_CONV_MAX_BYTES]          = {0};
    uint32_t  input_len    = 0u;
    uint32_t  byte_count   = 0u;
    uint32_t  err_pos      = 0u;
    int32_t   conv_len     = 0;
    HEX_CONV_RET_E ret     = HEX_CONV_RET_OK;
    HWND h_edit_in  = GetDlgItem(hwnd, (int)TOOL_HEX_IDC_EDIT_INPUT);
    HWND h_edit_out = GetDlgItem(hwnd, (int)TOOL_HEX_IDC_EDIT_OUTPUT);

    input_len = (uint32_t)GetWindowTextLengthW(h_edit_in);
    if (input_len == 0u) {
        tool_hex_set_status(hwnd, L"提示: 输入为空", TOOL_HEX_CLR_STATUS_INFO);
        return;
    }
    if (input_len >= (TOOL_HEX_INPUT_MAX_WCHAR - 1u)) {
        tool_hex_set_status(hwnd, L"错误: 输入过长", TOOL_HEX_CLR_STATUS_ERR);
        return;
    }

    (void)GetWindowTextW(h_edit_in, input_text, (int32_t)TOOL_HEX_INPUT_MAX_WCHAR);

    if (hex_to_ascii == TOOL_HEX_DIR_HEX2ASCII) {
        /* Hex字符串 -> 字节 -> UTF-8文本 */
        conv_len = WideCharToMultiByte(CP_UTF8, 0u, input_text, (int32_t)input_len,
                                       utf8_buf, (int32_t)HEX_CONV_MAX_BYTES, NULL, NULL);
        if ((conv_len <= 0) || (conv_len >= (int32_t)HEX_CONV_MAX_BYTES)) {
            tool_hex_set_status(hwnd, L"错误: 输入过长", TOOL_HEX_CLR_STATUS_ERR);
            return;
        }
        utf8_buf[conv_len] = '\0';

        ret = hex_conv_hex_str_to_bytes(utf8_buf, bytes_buf, HEX_CONV_MAX_BYTES, &byte_count, &err_pos);
        if (ret != HEX_CONV_RET_OK) {
            switch (ret)
            {
                case HEX_CONV_RET_ERR_FORMAT:
                {
                    (void)wsprintfW(status_text, L"格式错误: 第 %u 个字符非法或十六进制位数为奇数",
                                    (unsigned int)(err_pos + 1u));
                    break;
                }
                case HEX_CONV_RET_ERR_OVERFLOW:
                {
                    (void)wsprintfW(status_text, L"错误: 输入超过最大 %u 字节", (unsigned int)HEX_CONV_MAX_BYTES);
                    break;
                }
                default:
                {
                    (void)lstrcpyW(status_text, L"错误: 转换失败");
                    break;
                }
            }
            tool_hex_set_status(hwnd, status_text, TOOL_HEX_CLR_STATUS_ERR);
            return;
        }

        conv_len = MultiByteToWideChar(CP_UTF8, 0u, (LPCCH)bytes_buf, (int32_t)byte_count,
                                       output_text, (int32_t)TOOL_HEX_OUTPUT_MAX_WCHAR);
        if (conv_len <= 0) {
            tool_hex_set_status(hwnd, L"错误: 结果转换失败", TOOL_HEX_CLR_STATUS_ERR);
            return;
        }
        output_text[conv_len] = L'\0';
        SetWindowTextW(h_edit_out, output_text);

        (void)wsprintfW(status_text, L"转换成功: %u 字节 -> 文本", (unsigned int)byte_count);
        tool_hex_set_status(hwnd, status_text, TOOL_HEX_CLR_STATUS_OK);
    } else {
        /* ASCII文本 -> UTF-8字节 -> Hex字符串 */
        conv_len = WideCharToMultiByte(CP_UTF8, 0u, input_text, (int32_t)input_len,
                                       utf8_buf, (int32_t)HEX_CONV_MAX_BYTES, NULL, NULL);
        if ((conv_len <= 0) || (conv_len >= (int32_t)HEX_CONV_MAX_BYTES)) {
            tool_hex_set_status(hwnd, L"错误: 输入过长", TOOL_HEX_CLR_STATUS_ERR);
            return;
        }
        byte_count = (uint32_t)conv_len;

        ret = hex_conv_bytes_to_hex_str((const uint8_t *)utf8_buf, byte_count,
                                        hex_buf, HEX_CONV_HEX_STR_MAX);
        if (ret != HEX_CONV_RET_OK) {
            tool_hex_set_status(hwnd, L"错误: 十六进制生成失败", TOOL_HEX_CLR_STATUS_ERR);
            return;
        }

        conv_len = MultiByteToWideChar(CP_UTF8, 0u, hex_buf, -1,
                                       output_text, (int32_t)TOOL_HEX_OUTPUT_MAX_WCHAR);
        if (conv_len <= 0) {
            tool_hex_set_status(hwnd, L"错误: 结果转换失败", TOOL_HEX_CLR_STATUS_ERR);
            return;
        }
        SetWindowTextW(h_edit_out, output_text);

        (void)wsprintfW(status_text, L"转换成功: %u 字节 -> 十六进制", (unsigned int)byte_count);
        tool_hex_set_status(hwnd, status_text, TOOL_HEX_CLR_STATUS_OK);
    }
}

/**
 * @brief   复制输出框内容到剪贴板
 * @param   hwnd 工具窗口句柄
 * @return  无
 */
static void tool_hex_on_copy(HWND hwnd)
{
    wchar_t output_text[TOOL_HEX_OUTPUT_MAX_WCHAR] = {0};
    HWND h_edit_out = GetDlgItem(hwnd, (int)TOOL_HEX_IDC_EDIT_OUTPUT);

    if (GetWindowTextLengthW(h_edit_out) == 0) {
        tool_hex_set_status(hwnd, L"提示: 输出为空, 无可复制内容", TOOL_HEX_CLR_STATUS_INFO);
        return;
    }

    (void)GetWindowTextW(h_edit_out, output_text, (int32_t)TOOL_HEX_OUTPUT_MAX_WCHAR);
    if (ui_common_copy_to_clipboard(hwnd, output_text) == 0) {
        tool_hex_set_status(hwnd, L"已复制到剪贴板", TOOL_HEX_CLR_STATUS_OK);
    } else {
        tool_hex_set_status(hwnd, L"错误: 复制失败", TOOL_HEX_CLR_STATUS_ERR);
    }
}

/**
 * @brief   清空输入与输出框
 * @param   hwnd 工具窗口句柄
 * @return  无
 */
static void tool_hex_on_clear(HWND hwnd)
{
    SetWindowTextW(GetDlgItem(hwnd, (int)TOOL_HEX_IDC_EDIT_INPUT), L"");
    SetWindowTextW(GetDlgItem(hwnd, (int)TOOL_HEX_IDC_EDIT_OUTPUT), L"");
    tool_hex_set_status(hwnd, L"已清空", TOOL_HEX_CLR_STATUS_INFO);
}

/**
 * @brief   触发自绘按钮动作(按下并释放于同一按钮时调用)
 * @param   hwnd     工具窗口句柄
 * @param   ctrl_id  按钮控件ID
 * @return  无
 */
static void tool_hex_on_button(HWND hwnd, uint32_t ctrl_id)
{
    uint32_t index = 0u;

    for (index = 0u; index < TOOL_HEX_BTN_COUNT; index++) {
        if (g_tool_hex_btn_ids[index] == ctrl_id) {
            break;
        }
    }
    if (index >= TOOL_HEX_BTN_COUNT) {
        return;
    }

    switch (index)
    {
        case TOOL_HEX_BTN_IDX_HEX2ASCII:
        {
            tool_hex_do_convert(hwnd, TOOL_HEX_DIR_HEX2ASCII);
            break;
        }
        case TOOL_HEX_BTN_IDX_ASCII2HEX:
        {
            tool_hex_do_convert(hwnd, TOOL_HEX_DIR_ASCII2HEX);
            break;
        }
        case TOOL_HEX_BTN_IDX_COPY:
        {
            tool_hex_on_copy(hwnd);
            break;
        }
        case TOOL_HEX_BTN_IDX_CLEAR:
        {
            tool_hex_on_clear(hwnd);
            break;
        }
        default:
        {
            break;
        }
    }
}

/**
 * @brief   工具窗口消息处理过程
 * @param   hwnd  窗口句柄
 * @param   msg   消息类型
 * @param   wp    消息参数1
 * @param   lp    消息参数2
 * @return  消息处理结果
 */
static LRESULT CALLBACK tool_hex_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            HICON h_icon = ui_common_load_app_icon(g_tool_hex_instance);
            tool_hex_create_controls(hwnd);
            SendMessageW(hwnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)h_icon);
            SendMessageW(hwnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)h_icon);
            return 0;
        }
        case WM_SIZE:
        {
            tool_hex_layout(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
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
            for (index = 0u; index < TOOL_HEX_BTN_COUNT; index++) {
                tool_hex_draw_button(hdc, index);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC hdc_static = (HDC)wp;

            SetBkMode(hdc_static, TRANSPARENT);
            if ((HWND)lp == GetDlgItem(hwnd, (int)TOOL_HEX_IDC_STATUS)) {
                SetTextColor(hdc_static, (COLORREF)g_tool_hex_status_color);
            } else {
                SetTextColor(hdc_static, (COLORREF)TOOL_HEX_CLR_LABEL);
            }
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        case WM_MOUSEMOVE:
        {
            POINT pt;
            uint32_t id = 0u;
            uint32_t old_id = g_tool_hex_hover_id;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            id = tool_hex_hit_btn(pt);
            if (id != old_id) {
                g_tool_hex_hover_id = id;
                tool_hex_invalidate_btn(hwnd, old_id);
                tool_hex_invalidate_btn(hwnd, id);
            }
            if (g_tool_hex_mouse_tracking == TOOL_HEX_FALSE) {
                TRACKMOUSEEVENT tme;
                memset(&tme, 0, sizeof(tme));
                tme.cbSize = (DWORD)sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                (void)TrackMouseEvent(&tme);
                g_tool_hex_mouse_tracking = TOOL_HEX_TRUE;
            }
            break;
        }
        case WM_MOUSELEAVE:
        {
            tool_hex_invalidate_btn(hwnd, g_tool_hex_hover_id);
            g_tool_hex_hover_id = 0u;
            g_tool_hex_mouse_tracking = TOOL_HEX_FALSE;
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            POINT pt;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            g_tool_hex_press_id = tool_hex_hit_btn(pt);
            if (g_tool_hex_press_id != 0u) {
                tool_hex_invalidate_btn(hwnd, g_tool_hex_press_id);
                SetCapture(hwnd);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        {
            if (g_tool_hex_press_id != 0u) {
                POINT pt;
                uint32_t id = 0u;

                ReleaseCapture();
                pt.x = (int32_t)(int16_t)LOWORD(lp);
                pt.y = (int32_t)(int16_t)HIWORD(lp);
                id = tool_hex_hit_btn(pt);
                if (id == g_tool_hex_press_id) {
                    tool_hex_on_button(hwnd, id);
                }
                tool_hex_invalidate_btn(hwnd, g_tool_hex_press_id);
                g_tool_hex_press_id = 0u;
            }
            return 0;
        }
        case WM_SETCURSOR:
        {
            if ((LOWORD(wp) == HTCLIENT) && (g_tool_hex_hover_id != 0u)) {
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND));
                return (LRESULT)1;
            }
            break;
        }
        case WM_DESTROY:
        {
            /* 独立运行模式: 本窗口以无owner方式创建, 销毁即整个进程退出, 通知结束消息循环
               集合运行模式: 窗口带owner(主界面), 生命周期由主界面统一管理, 此处不做处理 */
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
 * @brief   打开 Hex-ASCII 转换工具窗口(可重复打开多个实例)
 * @param   h_inst  应用实例句柄
 * @param   h_owner 所有者窗口句柄
 * @return  无
 */
void tool_hexascii_open(HINSTANCE h_inst, HWND h_owner)
{
    if (g_tool_hex_class_ready == TOOL_HEX_FALSE) {
        WNDCLASSW wc;
        memset(&wc, 0, sizeof(wc));
        wc.style = (UINT)(CS_HREDRAW | CS_VREDRAW);
        wc.lpfnWndProc = tool_hex_wnd_proc;
        wc.hInstance = h_inst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = TOOL_HEX_CLASS_NAME;
        (void)RegisterClassW(&wc);
        g_tool_hex_class_ready = TOOL_HEX_TRUE;
    }

    g_tool_hex_instance = h_inst;

    {
        HWND h_wnd = CreateWindowW(TOOL_HEX_CLASS_NAME, TOOL_HEX_WND_TITLE,
                                   WS_OVERLAPPEDWINDOW,
                                   CW_USEDEFAULT, CW_USEDEFAULT,
                                   TOOL_HEX_WND_WIDTH, TOOL_HEX_WND_HEIGHT,
                                   h_owner, NULL, h_inst, NULL);
        if (h_wnd != NULL) {
            ShowWindow(h_wnd, SW_SHOWNORMAL);
            UpdateWindow(h_wnd);
        }
    }
}
