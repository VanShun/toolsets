/**
 * @file    ui_common.c
 * @author  herongshun
 * @date    2026-10-26
 * @brief   UI 公共组件实现(主题字体/应用图标/圆角矩形绘制/剪贴板)
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "ui_common.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define UI_COMMON_FONT_FACE   L"Microsoft YaHei UI"  /**< 常规界面字体族 */
#define UI_COMMON_FONT_MONO   L"Consolas"            /**< 等宽字体族(十六进制显示) */
#define UI_COMMON_FONT_SYMBOL L"Segoe UI Symbol"     /**< 图标字形字体族 */
#define UI_COMMON_RET_OK      0                      /**< 操作成功返回值 */
#define UI_COMMON_RET_FAIL    (-1)                   /**< 操作失败返回值 */
#define UI_COMMON_EDGE_FIX    1                      /**< 圆角矩形右/下边缘收缩像素 */

/* -------------------------------------------------------------------------- */
/*  内部类型与字体规格表                                                        */
/* -------------------------------------------------------------------------- */

/** 字体创建规格 */
typedef struct UI_FONT_SPEC_TAG
{
    int32_t       height;      /**< 字体高度(像素) */
    uint32_t      weight;      /**< 字重(FW_xxx) */
    uint32_t      quality;     /**< 渲染质量(浅底用ClearType, 彩色底用灰度抗锯齿防彩边) */
    const wchar_t *p_face;     /**< 字体族名 */
} UI_FONT_SPEC_T;

static const UI_FONT_SPEC_T g_ui_common_font_specs[UI_FONT_KIND_COUNT] =
{
    { 18, FW_NORMAL,   CLEARTYPE_QUALITY,   UI_COMMON_FONT_FACE   },  /**< UI_FONT_KIND_TEXT */
    { 16, FW_NORMAL,   CLEARTYPE_QUALITY,   UI_COMMON_FONT_MONO   },  /**< UI_FONT_KIND_EDIT */
    { 15, FW_NORMAL,   CLEARTYPE_QUALITY,   UI_COMMON_FONT_FACE   },  /**< UI_FONT_KIND_BTN */
    { 15, FW_NORMAL,   ANTIALIASED_QUALITY, UI_COMMON_FONT_FACE   },  /**< UI_FONT_KIND_BTN_COLOR */
    { 26, FW_SEMIBOLD, CLEARTYPE_QUALITY,   UI_COMMON_FONT_FACE   },  /**< UI_FONT_KIND_TITLE */
    { 13, FW_NORMAL,   CLEARTYPE_QUALITY,   UI_COMMON_FONT_FACE   },  /**< UI_FONT_KIND_SUBTITLE */
    { 16, FW_SEMIBOLD, CLEARTYPE_QUALITY,   UI_COMMON_FONT_FACE   },  /**< UI_FONT_KIND_CARD_NAME */
    { 12, FW_NORMAL,   CLEARTYPE_QUALITY,   UI_COMMON_FONT_FACE   },  /**< UI_FONT_KIND_CARD_DESC */
    { 20, FW_NORMAL,   ANTIALIASED_QUALITY, UI_COMMON_FONT_SYMBOL },  /**< UI_FONT_KIND_ICON */
};

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static HFONT g_ui_common_font_table[UI_FONT_KIND_COUNT] = {0};   /**< 共享字体句柄表(懒创建) */

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   获取指定种类的共享字体(懒创建)
 * @param   font_kind   字体种类(UI_FONT_KIND_E)
 * @return  字体句柄; 种类非法时返回NULL
 */
HFONT ui_common_get_font(uint32_t font_kind)
{
    const UI_FONT_SPEC_T *p_spec = NULL;

    if (font_kind >= UI_FONT_KIND_COUNT) {
        return NULL;
    }

    if (g_ui_common_font_table[font_kind] == NULL) {
        p_spec = &g_ui_common_font_specs[font_kind];
        g_ui_common_font_table[font_kind] = CreateFontW(p_spec->height, 0, 0, 0,
                                                        p_spec->weight, 0, 0, 0,
                                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                                        CLIP_DEFAULT_PRECIS, p_spec->quality,
                                                        DEFAULT_PITCH, p_spec->p_face);
    }

    return g_ui_common_font_table[font_kind];
}

/**
 * @brief   为控件应用指定种类的共享字体
 * @param   h_ctrl      目标控件句柄
 * @param   font_kind   字体种类(UI_FONT_KIND_E)
 * @return  无
 */
void ui_common_apply_font_kind(HWND h_ctrl, uint32_t font_kind)
{
    HFONT h_font = NULL;

    if (h_ctrl == NULL) {
        return;
    }

    h_font = ui_common_get_font(font_kind);
    if (h_font != NULL) {
        SendMessageW(h_ctrl, WM_SETFONT, (WPARAM)h_font, (LPARAM)1);
    }
}

/**
 * @brief   加载应用图标资源
 * @param   h_inst  应用实例句柄
 * @return  图标句柄; 资源缺失时返回NULL
 */
HICON ui_common_load_app_icon(HINSTANCE h_inst)
{
    return LoadIconW(h_inst, MAKEINTRESOURCEW(UI_COMMON_APP_ICON_ID));
}

/**
 * @brief   绘制带边框的圆角填充矩形
 * @param   hdc           目标设备上下文
 * @param   p_rc          目标矩形
 * @param   radius        圆角半径(像素)
 * @param   fill_color    填充色(0x00BBGGRR)
 * @param   border_color  边框色(0x00BBGGRR)
 * @param   border_width  边框宽度(像素)
 * @return  无
 */
void ui_common_fill_round_rect(HDC hdc, const RECT *p_rc, uint32_t radius,
                               uint32_t fill_color, uint32_t border_color, uint32_t border_width)
{
    HBRUSH h_brush = CreateSolidBrush((COLORREF)fill_color);
    HPEN   h_pen = CreatePen(PS_SOLID, (int32_t)border_width, (COLORREF)border_color);
    HBRUSH h_brush_old = (HBRUSH)SelectObject(hdc, h_brush);
    HPEN   h_pen_old = (HPEN)SelectObject(hdc, h_pen);

    RoundRect(hdc, p_rc->left, p_rc->top,
              (p_rc->right - UI_COMMON_EDGE_FIX), (p_rc->bottom - UI_COMMON_EDGE_FIX),
              (int32_t)radius, (int32_t)radius);

    (void)SelectObject(hdc, h_brush_old);
    (void)SelectObject(hdc, h_pen_old);
    DeleteObject(h_brush);
    DeleteObject(h_pen);
}

void ui_common_text_crisp(HDC hdc, const RECT *p_rc, const wchar_t *p_text,
                          UINT format, uint32_t bk)
{
    if (p_text == NULL) {
        return;
    }
    (void)SetBkColor(hdc, (COLORREF)bk);
    (void)SetBkMode(hdc, OPAQUE);
    (void)DrawTextW(hdc, p_text, -1, (RECT *)p_rc, format);
    (void)SetBkMode(hdc, TRANSPARENT);   /* 恢复, 不影响后续绘制 */
}

/**
 * @brief   将宽字符串文本复制到系统剪贴板
 * @param   h_owner 剪贴板打开所有者窗口
 * @param   p_text  待复制文本(以'\0'结尾)
 * @return  0: 复制成功; -1: 复制失败
 */
int32_t ui_common_copy_to_clipboard(HWND h_owner, const wchar_t *p_text)
{
    uint32_t text_len  = 0u;
    uint32_t buf_bytes = 0u;
    HGLOBAL  h_global  = NULL;
    void    *p_lock    = NULL;
    HANDLE   h_set_ret = NULL;

    if (p_text == NULL) {
        return UI_COMMON_RET_FAIL;
    }

    text_len = (uint32_t)lstrlenW(p_text);
    if (text_len == 0u) {
        return UI_COMMON_RET_FAIL;
    }

    buf_bytes = (text_len + 1u) * (uint32_t)sizeof(wchar_t);
    h_global = GlobalAlloc(GMEM_MOVEABLE, buf_bytes);
    if (h_global == NULL) {
        return UI_COMMON_RET_FAIL;
    }

    p_lock = GlobalLock(h_global);
    if (p_lock == NULL) {
        GlobalFree(h_global);
        return UI_COMMON_RET_FAIL;
    }
    memcpy(p_lock, p_text, buf_bytes);
    GlobalUnlock(h_global);

    if (OpenClipboard(h_owner) == 0) {
        GlobalFree(h_global);
        return UI_COMMON_RET_FAIL;
    }

    EmptyClipboard();
    h_set_ret = SetClipboardData(CF_UNICODETEXT, h_global);
    CloseClipboard();

    if (h_set_ret == NULL) {
        GlobalFree(h_global);
        return UI_COMMON_RET_FAIL;
    }

    return UI_COMMON_RET_OK;
}
