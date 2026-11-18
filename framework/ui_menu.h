/**
 * @file    ui_menu.h
 * @author  herongshun
 * @date    2026-11-13
 * @brief   卡片式菜单主界面公共组件(框架层)
 *          渐变背景+圆角卡片列表, 点击可用项回调打开子界面; 供合集主界面与
 *          各工具包的功能菜单复用, 每窗口实例状态独立(支持多实例并存)
 */

#ifndef UI_MENU_H
#define UI_MENU_H

#include <windows.h>
#include <stdint.h>

/** 菜单项数量上限(单窗口) */
#define UI_MENU_ITEM_MAX   32u

/** 菜单项 */
typedef struct UI_MENU_ITEM_TAG
{
    const wchar_t *p_icon;    /**< 徽章字形(Segoe UI Symbol 符号) */
    const wchar_t *p_name;    /**< 显示名称 */
    const wchar_t *p_desc;    /**< 简要描述 */
    uint32_t      accent;     /**< 主题色(0x00BBGGRR) */
    uint8_t       enabled;    /**< 1=可点击; 0=置灰占位(不可点击) */
    void (*pfn_open)(HINSTANCE h_inst, HWND h_owner);  /**< 点击回调(enabled=1时必填) */
} UI_MENU_ITEM_T;

/** 菜单栏命令回调(窗口收到WM_COMMAND时转交应用层处理) */
typedef void (*UI_MENU_CMD_FN)(HINSTANCE h_inst, HWND h_menu_wnd, uint32_t cmd_id);

/**
 * @brief   创建并显示一个卡片式菜单窗口
 *          窗口高度随菜单项数量自适应; 无owner时销毁窗口自行PostQuitMessage;
 *          可选挂接菜单栏(h_menu非NULL时SetMenu, 命令经pfn_cmd回调应用层)
 * @param   h_inst     应用实例句柄
 * @param   h_owner    所有者窗口(可为NULL)
 * @param   p_title    主界面标题
 * @param   p_subtitle 副标题(可为NULL)
 * @param   p_footer   页脚文本(可为NULL)
 * @param   p_items    [in] 菜单项数组(调用方须保证数组内容存活期覆盖窗口生命周期,
 *                          建议使用static const表)
 * @param   count      菜单数量(上限UI_MENU_ITEM_MAX)
 * @param   h_menu     菜单栏句柄(可为NULL=无菜单栏)
 * @param   pfn_cmd    菜单命令回调(可为NULL)
 * @return  窗口句柄; 失败返回NULL
 */
HWND ui_menu_window_open(HINSTANCE h_inst, HWND h_owner,
                         const wchar_t *p_title,
                         const wchar_t *p_subtitle,
                         const wchar_t *p_footer,
                         const UI_MENU_ITEM_T *p_items, uint32_t count,
                         HMENU h_menu, UI_MENU_CMD_FN pfn_cmd);

#endif /* UI_MENU_H */
