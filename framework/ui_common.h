/**
 * @file    ui_common.h
 * @author  herongshun
 * @date    2026-10-26
 * @brief   UI 公共组件接口(主题字体/应用图标/圆角矩形绘制/剪贴板)
 */

#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <windows.h>

/** 应用图标资源ID(须与 res/app.rc 中保持一致) */
#define UI_COMMON_APP_ICON_ID   1u

/** 共享字体种类 */
typedef enum UI_FONT_KIND_TAG
{
    UI_FONT_KIND_TEXT = 0,      /**< 常规文本(18px 雅黑) */
    UI_FONT_KIND_EDIT,          /**< 编辑框(16px Consolas等宽, 利于十六进制对齐) */
    UI_FONT_KIND_BTN,           /**< 浅底按钮文本(15px 雅黑, ClearType) */
    UI_FONT_KIND_BTN_COLOR,     /**< 彩底按钮文本(15px 雅黑, 灰度抗锯齿防彩边) */
    UI_FONT_KIND_TITLE,         /**< 主界面标题(26px 雅黑半粗) */
    UI_FONT_KIND_SUBTITLE,      /**< 副标题(13px 雅黑) */
    UI_FONT_KIND_CARD_NAME,     /**< 卡片名称(16px 雅黑半粗) */
    UI_FONT_KIND_CARD_DESC,     /**< 卡片描述(12px 雅黑) */
    UI_FONT_KIND_ICON,          /**< 图标字形(20px Segoe UI Symbol) */
    UI_FONT_KIND_COUNT          /**< 字体种类数(内部使用, 保持末位) */
} UI_FONT_KIND_E;

/**
 * @brief   为控件应用指定种类的共享字体
 * @param   h_ctrl      目标控件句柄
 * @param   font_kind   字体种类(UI_FONT_KIND_E)
 * @return  无
 */
void ui_common_apply_font_kind(HWND h_ctrl, uint32_t font_kind);

/**
 * @brief   获取指定种类的共享字体(懒创建)
 * @param   font_kind   字体种类(UI_FONT_KIND_E)
 * @return  字体句柄; 种类非法时返回NULL
 */
HFONT ui_common_get_font(uint32_t font_kind);

/**
 * @brief   加载应用图标资源
 * @param   h_inst  应用实例句柄
 * @return  图标句柄; 资源缺失时返回NULL
 */
HICON ui_common_load_app_icon(HINSTANCE h_inst);

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
                               uint32_t fill_color, uint32_t border_color, uint32_t border_width);

/**
 * @brief   将宽字符串文本复制到系统剪贴板
 * @param   h_owner 剪贴板打开所有者窗口
 * @param   p_text  待复制文本(以'\0'结尾)
 * @return  0: 复制成功; -1: 复制失败
 */
int32_t ui_common_copy_to_clipboard(HWND h_owner, const wchar_t *p_text);

#endif /* UI_COMMON_H */
