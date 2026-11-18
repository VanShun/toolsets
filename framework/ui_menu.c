/**
 * @file    ui_menu.c
 * @author  herongshun
 * @date    2026-11-13
 * @brief   卡片式菜单主界面组件实现
 *          渐变背景/头部(图标+标题+副标题+页脚)/圆角卡片列表, 使用内存DC防闪烁;
 *          置灰项(engabled=0)不参与命中, 供"先立菜单骨架、后逐项实现"的形态
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "ui_common.h"
#include "ui_menu.h"

/* -------------------------------------------------------------------------- */
/*  宏定义: 主题颜色 (0x00BBGGRR)                                              */
/* -------------------------------------------------------------------------- */
#define UI_MENU_CLR_BG_TOP       0x00FCFDFF   /**< 背景渐变顶部 */
#define UI_MENU_CLR_BG_BOTTOM    0x00E8EDF6   /**< 背景渐变底部 */
#define UI_MENU_CLR_CARD_BG      0x00FFFFFF   /**< 卡片底色 */
#define UI_MENU_CLR_CARD_PRESSED 0x00E9F0FE   /**< 卡片按下底色 */
#define UI_MENU_CLR_CARD_BORDER  0x00E4E8F0   /**< 卡片默认边框 */
#define UI_MENU_CLR_TITLE        0x001B2433   /**< 主标题文字色 */
#define UI_MENU_CLR_SUBTITLE     0x006B7280   /**< 副标题文字色 */
#define UI_MENU_CLR_FOOTER       0x009CA6B5   /**< 页脚文字色 */
#define UI_MENU_CLR_ARROW        0x00AEB6C4   /**< 卡片箭头默认色 */
#define UI_MENU_CLR_DISABLED     0x00C8CDD6   /**< 置灰项底色 */
#define UI_MENU_CLR_DIS_TEXT     0x009AA4B2   /**< 置灰项文字色 */
#define UI_MENU_CLR_MASK_R       0x000000FFu  /**< 颜色R通道掩码 */
#define UI_MENU_CLR_MASK_G       0x0000FF00u  /**< 颜色G通道掩码 */
#define UI_MENU_CLR_MASK_B       0x00FF0000u  /**< 颜色B通道掩码 */
#define UI_MENU_CLR_SHIFT_G      8u           /**< G通道右移位数 */
#define UI_MENU_CLR_SHIFT_B      16u          /**< B通道右移位数 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 窗口与布局                                                         */
/* -------------------------------------------------------------------------- */
#define UI_MENU_CLASS_NAME     L"UI_MENU_CARD_WND"   /**< 窗口类名(共享, 多实例) */
#define UI_MENU_WND_WIDTH      520u                  /**< 窗口宽度 */
#define UI_MENU_HEADER_X       34u                   /**< 头部图标X坐标 */
#define UI_MENU_HEADER_Y       26u                   /**< 头部区域Y坐标 */
#define UI_MENU_ICON_SIZE      36u                   /**< 头部图标尺寸 */
#define UI_MENU_HEADER_GAP     12u                   /**< 头部图标与文字间距 */
#define UI_MENU_TITLE_H        40u                   /**< 主标题高度 */
#define UI_MENU_SUBTITLE_H     20u                   /**< 副标题高度 */
#define UI_MENU_CARD_W         400u                  /**< 卡片宽度 */
#define UI_MENU_CARD_H         66u                   /**< 卡片高度 */
#define UI_MENU_CARD_FIRST_Y   118u                  /**< 首个卡片Y坐标 */
#define UI_MENU_CARD_GAP       14u                   /**< 卡片间距 */
#define UI_MENU_CARD_RADIUS    10u                   /**< 卡片圆角半径 */
#define UI_MENU_BADGE_SIZE     38u                   /**< 图标徽章直径 */
#define UI_MENU_BADGE_MARGIN   14u                   /**< 徽章距卡片左边距 */
#define UI_MENU_TEXT_MARGIN    14u                   /**< 文字距徽章右边距 */
#define UI_MENU_NAME_OFF_Y     9u                    /**< 卡片名称Y偏移 */
#define UI_MENU_NAME_H         24u                   /**< 卡片名称高度 */
#define UI_MENU_DESC_OFF_Y     34u                   /**< 卡片描述Y偏移 */
#define UI_MENU_DESC_H         20u                   /**< 卡片描述高度 */
#define UI_MENU_ARROW_W        24u                   /**< 箭头区域宽度 */
#define UI_MENU_ARROW_MARGIN   16u                   /**< 箭头距卡片右边距 */
#define UI_MENU_BORDER_W       1u                    /**< 默认边框宽度 */
#define UI_MENU_BORDER_W_HV    2u                    /**< 悬停边框宽度 */
#define UI_MENU_FOOTER_W       300u                  /**< 页脚文本区宽度 */
#define UI_MENU_FOOTER_H       18u                   /**< 页脚文本区高度 */
#define UI_MENU_FOOTER_GAP_B   12u                   /**< 页脚距底边间距 */
#define UI_MENU_FOOTER_GAP_R   20u                   /**< 页脚距右边间距 */
#define UI_MENU_FOOTER_AREA_H  48u                   /**< 页脚预留区域高度 */
#define UI_MENU_GRADIENT_BANDS 64u                   /**< 背景渐变色带数 */
#define UI_MENU_HOVER_NONE     (-1)                  /**< 无悬停/按下索引 */

/* -------------------------------------------------------------------------- */
/*  内部类型                                                                    */
/* -------------------------------------------------------------------------- */

/** 菜单窗口实例上下文(经GWLP_USERDATA挂载, 支持多实例) */
typedef struct UI_MENU_CTX_TAG
{
    HINSTANCE      h_inst;                            /**< 应用实例句柄 */
    uint32_t       count;                             /**< 菜单数量 */
    UI_MENU_ITEM_T items[UI_MENU_ITEM_MAX];           /**< 菜单项拷贝 */
    RECT           cards[UI_MENU_ITEM_MAX];           /**< 卡片矩形缓存 */
    int32_t        hover;                             /**< 当前悬停索引 */
    int32_t        press;                             /**< 当前按下索引 */
    uint8_t        tracking;                          /**< 鼠标离开跟踪标志 */
    const wchar_t *p_title;                           /**< 主界面标题 */
    const wchar_t *p_subtitle;                        /**< 副标题 */
    const wchar_t *p_footer;                          /**< 页脚文本 */
} UI_MENU_CTX_T;

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   计算全部卡片矩形(随窗口尺寸居中重排)
 * @param   p_ctx 实例上下文
 * @param   hwnd  窗口句柄
 * @return  无
 */
static void ui_menu_layout(UI_MENU_CTX_T *p_ctx, HWND hwnd)
{
    RECT rc = {0};
    uint32_t cw = 0u;
    uint32_t index = 0u;
    int32_t card_x = 0u;

    GetClientRect(hwnd, &rc);
    cw = (uint32_t)(rc.right - rc.left);

    for (index = 0u; index < p_ctx->count; index++) {
        card_x = (int32_t)((cw - UI_MENU_CARD_W) / 2u);
        if (card_x < (int32_t)UI_MENU_HEADER_X) {
            card_x = (int32_t)UI_MENU_HEADER_X;
        }
        p_ctx->cards[index].left = card_x;
        p_ctx->cards[index].top = (LONG)(UI_MENU_CARD_FIRST_Y +
                                         (index * (UI_MENU_CARD_H + UI_MENU_CARD_GAP)));
        p_ctx->cards[index].right = card_x + (LONG)UI_MENU_CARD_W;
        p_ctx->cards[index].bottom = p_ctx->cards[index].top + (LONG)UI_MENU_CARD_H;
    }
}

/**
 * @brief   在指定矩形内绘制垂直渐变背景
 * @param   hdc  目标设备上下文
 * @param   p_rc 目标矩形
 * @return  无
 */
static void ui_menu_draw_background(HDC hdc, const RECT *p_rc)
{
    RECT band = {0};
    uint32_t i = 0u;
    uint32_t total_h = 0u;
    uint32_t r_top = (UI_MENU_CLR_BG_TOP & UI_MENU_CLR_MASK_R);
    uint32_t g_top = ((UI_MENU_CLR_BG_TOP & UI_MENU_CLR_MASK_G) >> UI_MENU_CLR_SHIFT_G);
    uint32_t b_top = ((UI_MENU_CLR_BG_TOP & UI_MENU_CLR_MASK_B) >> UI_MENU_CLR_SHIFT_B);
    uint32_t r_bot = (UI_MENU_CLR_BG_BOTTOM & UI_MENU_CLR_MASK_R);
    uint32_t g_bot = ((UI_MENU_CLR_BG_BOTTOM & UI_MENU_CLR_MASK_G) >> UI_MENU_CLR_SHIFT_G);
    uint32_t b_bot = ((UI_MENU_CLR_BG_BOTTOM & UI_MENU_CLR_MASK_B) >> UI_MENU_CLR_SHIFT_B);
    uint32_t divisor = UI_MENU_GRADIENT_BANDS - 1u;

    band.left = p_rc->left;
    band.right = p_rc->right;
    total_h = (uint32_t)(p_rc->bottom - p_rc->top);

    for (i = 0u; i < UI_MENU_GRADIENT_BANDS; i++) {
        uint32_t r = ((r_top * (divisor - i)) + (r_bot * i)) / divisor;
        uint32_t g = ((g_top * (divisor - i)) + (g_bot * i)) / divisor;
        uint32_t b = ((b_top * (divisor - i)) + (b_bot * i)) / divisor;
        uint32_t clr = ((b << UI_MENU_CLR_SHIFT_B) | (g << UI_MENU_CLR_SHIFT_G) | r);
        HBRUSH h_brush = CreateSolidBrush((COLORREF)clr);

        band.top = (LONG)(((total_h * i) / UI_MENU_GRADIENT_BANDS) + (uint32_t)p_rc->top);
        band.bottom = (LONG)(((total_h * (i + 1u)) / UI_MENU_GRADIENT_BANDS) + (uint32_t)p_rc->top);
        FillRect(hdc, &band, h_brush);
        DeleteObject(h_brush);
    }
}

/**
 * @brief   绘制头部(图标/标题/副标题)与页脚
 * @param   p_ctx     实例上下文
 * @param   hdc       目标设备上下文
 * @param   client_w  客户区宽
 * @param   client_h  客户区高
 * @return  无
 */
static void ui_menu_draw_header(const UI_MENU_CTX_T *p_ctx, HDC hdc,
                                uint32_t client_w, uint32_t client_h)
{
    RECT rc = {0};
    HICON h_icon = ui_common_load_app_icon(p_ctx->h_inst);
    HFONT h_font_old = NULL;
    uint32_t text_x = UI_MENU_HEADER_X + UI_MENU_ICON_SIZE + UI_MENU_HEADER_GAP;

    if (h_icon != NULL) {
        DrawIconEx(hdc, (int32_t)UI_MENU_HEADER_X, (int32_t)UI_MENU_HEADER_Y, h_icon,
                   (int32_t)UI_MENU_ICON_SIZE, (int32_t)UI_MENU_ICON_SIZE, 0, NULL, DI_NORMAL);
    }

    SetBkMode(hdc, TRANSPARENT);
    h_font_old = (HFONT)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_TITLE));

    rc.left = (LONG)text_x;
    rc.top = (LONG)UI_MENU_HEADER_Y;
    rc.right = (LONG)client_w;
    rc.bottom = (LONG)(UI_MENU_HEADER_Y + UI_MENU_TITLE_H);
    SetTextColor(hdc, (COLORREF)UI_MENU_CLR_TITLE);
    if (p_ctx->p_title != NULL) {
        DrawTextW(hdc, p_ctx->p_title, -1, &rc, (DT_LEFT | DT_VCENTER | DT_SINGLELINE));
    }

    (void)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_SUBTITLE));
    rc.top = (LONG)(UI_MENU_HEADER_Y + UI_MENU_TITLE_H);
    rc.bottom = (LONG)(UI_MENU_HEADER_Y + UI_MENU_TITLE_H + UI_MENU_SUBTITLE_H);
    SetTextColor(hdc, (COLORREF)UI_MENU_CLR_SUBTITLE);
    if (p_ctx->p_subtitle != NULL) {
        DrawTextW(hdc, p_ctx->p_subtitle, -1, &rc,
                  (DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS));
    }

    (void)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_CARD_DESC));
    rc.left = (LONG)((client_w > (UI_MENU_FOOTER_W + UI_MENU_FOOTER_GAP_R))
                     ? (client_w - UI_MENU_FOOTER_W - UI_MENU_FOOTER_GAP_R) : 0u);
    rc.right = (LONG)(client_w - UI_MENU_FOOTER_GAP_R);
    rc.top = (LONG)((client_h > (UI_MENU_FOOTER_H + UI_MENU_FOOTER_GAP_B))
                    ? (client_h - UI_MENU_FOOTER_H - UI_MENU_FOOTER_GAP_B) : 0u);
    rc.bottom = (LONG)((client_h > UI_MENU_FOOTER_GAP_B) ? (client_h - UI_MENU_FOOTER_GAP_B) : 0u);
    SetTextColor(hdc, (COLORREF)UI_MENU_CLR_FOOTER);
    if (p_ctx->p_footer != NULL) {
        DrawTextW(hdc, p_ctx->p_footer, -1, &rc, (DT_RIGHT | DT_VCENTER | DT_SINGLELINE));
    }

    (void)SelectObject(hdc, h_font_old);
}

/**
 * @brief   绘制单个菜单卡片(置灰项使用灰色徽章与文字)
 * @param   p_ctx 实例上下文
 * @param   hdc   目标设备上下文
 * @param   index 菜单索引
 * @return  无
 */
static void ui_menu_draw_card(const UI_MENU_CTX_T *p_ctx, HDC hdc, uint32_t index)
{
    const UI_MENU_ITEM_T *p_item = &p_ctx->items[index];
    RECT rc = p_ctx->cards[index];
    RECT rc_badge = {0};
    RECT rc_text = {0};
    RECT rc_arrow = {0};
    uint8_t enabled = p_item->enabled;
    uint8_t hovered = ((p_ctx->hover == (int32_t)index) ? 1u : 0u);
    uint8_t pressed = ((p_ctx->press == (int32_t)index) ? 1u : 0u);
    uint32_t fill_clr = UI_MENU_CLR_CARD_BG;
    uint32_t border_clr = UI_MENU_CLR_CARD_BORDER;
    uint32_t name_clr = UI_MENU_CLR_TITLE;
    uint32_t desc_clr = UI_MENU_CLR_SUBTITLE;
    uint32_t arrow_clr = UI_MENU_CLR_ARROW;
    uint32_t badge_clr = p_item->accent;
    uint32_t border_w = UI_MENU_BORDER_W;
    HFONT h_font_old = NULL;

    if (enabled == 0u) {
        fill_clr = UI_MENU_CLR_CARD_BG;
        border_clr = UI_MENU_CLR_DISABLED;
        name_clr = UI_MENU_CLR_DIS_TEXT;
        desc_clr = UI_MENU_CLR_DIS_TEXT;
        arrow_clr = UI_MENU_CLR_DISABLED;
        badge_clr = UI_MENU_CLR_DISABLED;
    } else {
        if (pressed != 0u) {
            fill_clr = UI_MENU_CLR_CARD_PRESSED;
        }
        if (hovered != 0u) {
            border_clr = p_item->accent;
            border_w = UI_MENU_BORDER_W_HV;
            arrow_clr = p_item->accent;
        }
    }

    SetBkMode(hdc, TRANSPARENT);
    ui_common_fill_round_rect(hdc, &rc, UI_MENU_CARD_RADIUS, fill_clr, border_clr, border_w);

    /* 左侧徽章与图标字形 */
    rc_badge.left = rc.left + (LONG)UI_MENU_BADGE_MARGIN;
    rc_badge.top = rc.top + (LONG)((UI_MENU_CARD_H - UI_MENU_BADGE_SIZE) / 2u);
    rc_badge.right = rc_badge.left + (LONG)UI_MENU_BADGE_SIZE;
    rc_badge.bottom = rc_badge.top + (LONG)UI_MENU_BADGE_SIZE;
    {
        HBRUSH h_brush = CreateSolidBrush((COLORREF)badge_clr);
        HBRUSH h_brush_old = (HBRUSH)SelectObject(hdc, h_brush);

        Ellipse(hdc, rc_badge.left, rc_badge.top, rc_badge.right, rc_badge.bottom);
        (void)SelectObject(hdc, h_brush_old);
        DeleteObject(h_brush);
    }

    h_font_old = (HFONT)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_ICON));
    SetTextColor(hdc, (COLORREF)UI_MENU_CLR_CARD_BG);
    if (p_item->p_icon != NULL) {
        DrawTextW(hdc, p_item->p_icon, -1, &rc_badge, (DT_CENTER | DT_VCENTER | DT_SINGLELINE));
    }

    /* 名称与描述 */
    rc_text.left = rc_badge.right + (LONG)UI_MENU_TEXT_MARGIN;
    rc_text.right = rc.right - (LONG)(UI_MENU_ARROW_W + UI_MENU_ARROW_MARGIN);
    rc_text.top = rc.top + (LONG)UI_MENU_NAME_OFF_Y;
    rc_text.bottom = rc_text.top + (LONG)UI_MENU_NAME_H;
    SetTextColor(hdc, (COLORREF)name_clr);
    (void)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_CARD_NAME));
    if (p_item->p_name != NULL) {
        DrawTextW(hdc, p_item->p_name, -1, &rc_text,
                  (DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS));
    }

    rc_text.top = rc.top + (LONG)UI_MENU_DESC_OFF_Y;
    rc_text.bottom = rc_text.top + (LONG)UI_MENU_DESC_H;
    SetTextColor(hdc, (COLORREF)desc_clr);
    (void)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_CARD_DESC));
    if (p_item->p_desc != NULL) {
        DrawTextW(hdc, p_item->p_desc, -1, &rc_text,
                  (DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS));
    }

    /* 右侧箭头(置灰项不带箭头) */
    rc_arrow.left = rc.right - (LONG)(UI_MENU_ARROW_W + UI_MENU_ARROW_MARGIN);
    rc_arrow.top = rc.top;
    rc_arrow.right = rc_arrow.left + (LONG)UI_MENU_ARROW_W;
    rc_arrow.bottom = rc.bottom;
    SetTextColor(hdc, (COLORREF)arrow_clr);
    (void)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_ICON));
    if (enabled != 0u) {
        DrawTextW(hdc, L"›", -1, &rc_arrow, (DT_CENTER | DT_VCENTER | DT_SINGLELINE));
    }

    (void)SelectObject(hdc, h_font_old);
}

/**
 * @brief   绘制菜单窗口全部内容(内存DC防闪烁)
 * @param   p_ctx 实例上下文
 * @param   hwnd  窗口句柄
 * @param   hdc   窗口DC
 * @return  无
 */
static void ui_menu_paint_all(const UI_MENU_CTX_T *p_ctx, HWND hwnd, HDC hdc)
{
    RECT rc_client = {0};
    PAINTSTRUCT ps;
    BITMAPINFO bmi;
    void *p_bits = NULL;
    HDC hdc_mem = NULL;
    HBITMAP h_bmp = NULL;
    HBITMAP h_bmp_old = NULL;
    uint32_t index = 0u;
    uint32_t client_w = 0u;
    uint32_t client_h = 0u;

    memset(&ps, 0, sizeof(ps));
    memset(&bmi, 0, sizeof(bmi));
    GetClientRect(hwnd, &rc_client);
    client_w = (uint32_t)(rc_client.right - rc_client.left);
    client_h = (uint32_t)(rc_client.bottom - rc_client.top);
    if ((client_w == 0u) || (client_h == 0u)) {
        return;
    }

    hdc_mem = CreateCompatibleDC(hdc);
    bmi.bmiHeader.biSize = (DWORD)sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)client_w;
    bmi.bmiHeader.biHeight = (LONG)client_h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    h_bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &p_bits, NULL, 0);
    if (h_bmp == NULL) {
        h_bmp = CreateCompatibleBitmap(hdc, (int32_t)client_w, (int32_t)client_h);
    }
    h_bmp_old = (HBITMAP)SelectObject(hdc_mem, h_bmp);

    ui_menu_draw_background(hdc_mem, &rc_client);
    ui_menu_draw_header(p_ctx, hdc_mem, client_w, client_h);
    for (index = 0u; index < p_ctx->count; index++) {
        ui_menu_draw_card(p_ctx, hdc_mem, index);
    }

    (void)BitBlt(hdc, 0, 0, (int32_t)client_w, (int32_t)client_h, hdc_mem, 0, 0, SRCCOPY);
    (void)SelectObject(hdc_mem, h_bmp_old);
    DeleteObject(h_bmp);
    DeleteDC(hdc_mem);
}

/**
 * @brief   命中测试: 坐标落在哪个可用卡片上(置灰项不命中)
 * @param   p_ctx 实例上下文
 * @param   pt    客户区坐标
 * @return  卡片索引; 无命中返回UI_MENU_HOVER_NONE
 */
static int32_t ui_menu_hit_test(const UI_MENU_CTX_T *p_ctx, POINT pt)
{
    uint32_t index = 0u;

    for (index = 0u; index < p_ctx->count; index++) {
        if ((p_ctx->items[index].enabled != 0u) &&
            (PtInRect(&p_ctx->cards[index], pt) != 0)) {
            return (int32_t)index;
        }
    }

    return UI_MENU_HOVER_NONE;
}

/**
 * @brief   使指定卡片区域重绘
 * @param   hwnd  窗口句柄
 * @param   p_ctx 实例上下文
 * @param   index 索引(UI_MENU_HOVER_NONE忽略)
 * @return  无
 */
static void ui_menu_invalidate_card(HWND hwnd, const UI_MENU_CTX_T *p_ctx, int32_t index)
{
    if ((index >= 0) && (index < (int32_t)p_ctx->count)) {
        InvalidateRect(hwnd, &p_ctx->cards[index], FALSE);
    }
}

/**
 * @brief   菜单窗口消息处理过程
 * @param   hwnd  窗口句柄
 * @param   msg   消息类型
 * @param   wp    消息参数1
 * @param   lp    消息参数2
 * @return  消息处理结果
 */
static LRESULT CALLBACK ui_menu_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    UI_MENU_CTX_T *p_ctx = (UI_MENU_CTX_T *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (msg == WM_NCCREATE) {
        /* 窗口上下文经CREATESTRUCT挂载(早于WM_CREATE) */
        LPCREATESTRUCTW p_cs = (LPCREATESTRUCTW)lp;
        (void)SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)p_cs->lpCreateParams);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    if (p_ctx == NULL) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    switch (msg)
    {
        case WM_CREATE:
        {
            HICON h_icon = ui_common_load_app_icon(p_ctx->h_inst);

            SendMessageW(hwnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)h_icon);
            SendMessageW(hwnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)h_icon);
            return 0;
        }
        case WM_SIZE:
        {
            ui_menu_layout(p_ctx, hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_ERASEBKGND:
        {
            return (LRESULT)1;   /* WM_PAINT经内存DC全幅自绘 */
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc;

            memset(&ps, 0, sizeof(ps));
            hdc = BeginPaint(hwnd, &ps);
            ui_menu_paint_all(p_ctx, hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            POINT pt;
            int32_t hover = UI_MENU_HOVER_NONE;
            int32_t old_hover = p_ctx->hover;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            hover = ui_menu_hit_test(p_ctx, pt);
            if (hover != old_hover) {
                p_ctx->hover = hover;
                ui_menu_invalidate_card(hwnd, p_ctx, old_hover);
                ui_menu_invalidate_card(hwnd, p_ctx, hover);
            }
            if (p_ctx->tracking == 0u) {
                TRACKMOUSEEVENT tme;
                memset(&tme, 0, sizeof(tme));
                tme.cbSize = (DWORD)sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                (void)TrackMouseEvent(&tme);
                p_ctx->tracking = 1u;
            }
            break;
        }
        case WM_MOUSELEAVE:
        {
            ui_menu_invalidate_card(hwnd, p_ctx, p_ctx->hover);
            p_ctx->hover = UI_MENU_HOVER_NONE;
            p_ctx->tracking = 0u;
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            POINT pt;
            int32_t hit = UI_MENU_HOVER_NONE;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            hit = ui_menu_hit_test(p_ctx, pt);
            if (hit != UI_MENU_HOVER_NONE) {
                p_ctx->press = hit;
                ui_menu_invalidate_card(hwnd, p_ctx, hit);
                SetCapture(hwnd);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        {
            if (p_ctx->press != UI_MENU_HOVER_NONE) {
                POINT pt;
                int32_t hit = UI_MENU_HOVER_NONE;

                ReleaseCapture();
                pt.x = (int32_t)(int16_t)LOWORD(lp);
                pt.y = (int32_t)(int16_t)HIWORD(lp);
                hit = ui_menu_hit_test(p_ctx, pt);
                if ((hit == p_ctx->press) && (p_ctx->items[hit].pfn_open != NULL)) {
                    p_ctx->items[hit].pfn_open(p_ctx->h_inst, hwnd);
                }
                ui_menu_invalidate_card(hwnd, p_ctx, p_ctx->press);
                p_ctx->press = UI_MENU_HOVER_NONE;
            }
            return 0;
        }
        case WM_SETCURSOR:
        {
            if ((LOWORD(wp) == HTCLIENT) && (p_ctx->hover != UI_MENU_HOVER_NONE)) {
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND));
                return (LRESULT)1;
            }
            break;
        }
        case WM_DESTROY:
        {
            (void)HeapFree(GetProcessHeap(), 0u, p_ctx);
            (void)SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)0);
            /* 独立运行模式(无owner): 菜单窗口销毁即进程退出 */
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

HWND ui_menu_window_open(HINSTANCE h_inst, HWND h_owner,
                         const wchar_t *p_title,
                         const wchar_t *p_subtitle,
                         const wchar_t *p_footer,
                         const UI_MENU_ITEM_T *p_items, uint32_t count)
{
    static uint8_t s_class_ready = 0u;
    WNDCLASSW wc;
    UI_MENU_CTX_T *p_ctx = NULL;
    HWND h_wnd = NULL;
    uint32_t i = 0u;
    uint32_t wnd_h = 0u;

    if ((h_inst == NULL) || (p_items == NULL) || (count == 0u)) {
        return NULL;
    }
    if (count > UI_MENU_ITEM_MAX) {
        count = UI_MENU_ITEM_MAX;
    }

    if (s_class_ready == 0u) {
        memset(&wc, 0, sizeof(wc));
        wc.style = (UINT)(CS_HREDRAW | CS_VREDRAW);
        wc.lpfnWndProc = ui_menu_wnd_proc;
        wc.hInstance = h_inst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = UI_MENU_CLASS_NAME;
        if (RegisterClassW(&wc) == 0) {
            return NULL;
        }
        s_class_ready = 1u;
    }

    p_ctx = (UI_MENU_CTX_T *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(UI_MENU_CTX_T));
    if (p_ctx == NULL) {
        return NULL;
    }
    p_ctx->h_inst = h_inst;
    p_ctx->count = count;
    p_ctx->hover = UI_MENU_HOVER_NONE;
    p_ctx->press = UI_MENU_HOVER_NONE;
    p_ctx->p_title = p_title;
    p_ctx->p_subtitle = p_subtitle;
    p_ctx->p_footer = p_footer;
    for (i = 0u; i < count; i++) {
        p_ctx->items[i] = p_items[i];
    }

    wnd_h = UI_MENU_CARD_FIRST_Y + (count * (UI_MENU_CARD_H + UI_MENU_CARD_GAP)) +
            UI_MENU_FOOTER_AREA_H;
    if (wnd_h < 320u) {
        wnd_h = 320u;
    }

    h_wnd = CreateWindowW(UI_MENU_CLASS_NAME, (p_title != NULL) ? p_title : L"",
                          WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          UI_MENU_WND_WIDTH, (int32_t)wnd_h,
                          h_owner, NULL, h_inst, p_ctx);
    if (h_wnd == NULL) {
        (void)HeapFree(GetProcessHeap(), 0u, p_ctx);
        return NULL;
    }

    ShowWindow(h_wnd, SW_SHOWNORMAL);
    UpdateWindow(h_wnd);

    return h_wnd;
}
