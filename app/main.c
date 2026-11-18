/**
 * @file    main.c
 * @author  herongshun
 * @date    2026-11-13
 * @brief   ToolSets 程序入口与主界面
 *          经 framework/ui_menu 卡片式主界面组件呈现工具登记表,
 *          绘制逻辑统一收敛于框架组件, 本文件只负责装配与消息循环
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "tool_manifest.h"
#include "ui_menu.h"
#include "version.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define MAIN_WND_TITLE    L"ToolSets 工具合集"                      /**< 主界面标题 */
#define MAIN_WND_SUBTITLE L"轻量级 Windows 工具合集 · 点击卡片打开工具" /**< 副标题 */
#define MAIN_WND_FOOTER   L"v" VERSION_WSTR L" · 纯C Win32 · 零依赖"  /**< 页脚文本(含版本号) */

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   程序入口: 装配工具登记表为菜单项, 呈现主界面并进入消息循环
 * @param   h_inst      当前应用实例句柄
 * @param   h_prev      前一实例句柄(Win32下恒为NULL, 未使用)
 * @param   p_cmd_line  命令行参数字符串(未使用)
 * @param   cmd_show    初始显示状态(主界面组件固定SW_SHOWNORMAL, 未使用)
 * @return  消息循环退出码
 */
int WINAPI WinMain(HINSTANCE h_inst, HINSTANCE h_prev, char *p_cmd_line, int cmd_show)
{
    const TOOL_INFO_T *p_table = NULL;
    UI_MENU_ITEM_T items[UI_MENU_ITEM_MAX];
    HWND h_main = NULL;
    MSG msg;
    uint32_t count = 0u;
    uint32_t i = 0u;

    (void)h_prev;
    (void)p_cmd_line;
    (void)cmd_show;

    memset(&items[0], 0, sizeof(items));
    memset(&msg, 0, sizeof(msg));

    p_table = tool_manifest_get_table(&count);
    if ((p_table == NULL) || (count == 0u)) {
        return 1;
    }
    if (count > UI_MENU_ITEM_MAX) {
        count = UI_MENU_ITEM_MAX;
    }

    /* 登记表 -> 菜单项(合集内工具全部可用) */
    for (i = 0u; i < count; i++) {
        items[i].p_icon = p_table[i].p_icon;
        items[i].p_name = p_table[i].p_name;
        items[i].p_desc = p_table[i].p_desc;
        items[i].accent = p_table[i].accent;
        items[i].enabled = 1u;
        items[i].pfn_open = p_table[i].pfn_open;
    }

    h_main = ui_menu_window_open(h_inst, NULL,
                                 MAIN_WND_TITLE, MAIN_WND_SUBTITLE, MAIN_WND_FOOTER,
                                 &items[0], count);
    if (h_main == NULL) {
        return 1;
    }

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
