/**
 * @file    main.c
 * @author  herongshun
 * @date    2026-11-18
 * @brief   ToolSets 程序入口与主界面
 *          经 framework/ui_menu 卡片式主界面组件呈现工具登记表;
 *          菜单栏(文件/帮助)与应用内更新检查(updater)在本文件装配
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "tool_manifest.h"
#include "ui_menu.h"
#include "version.h"
#include "updater.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define MAIN_WND_TITLE    L"ToolSets 工具合集"                      /**< 主界面标题 */
#define MAIN_WND_SUBTITLE L"轻量级 Windows 工具合集 · 点击卡片打开工具" /**< 副标题 */
#define MAIN_WND_FOOTER   L"v" VERSION_WSTR L" · 纯C Win32 · 零依赖"  /**< 页脚文本(含版本号) */

#define MAIN_MID_EXIT      4001u  /**< 文件-退出 */
#define MAIN_MID_CHECKUPD  4002u  /**< 帮助-检查更新 */
#define MAIN_MID_ABOUT     4003u  /**< 帮助-关于 */

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   创建主界面菜单栏(文件/帮助)
 * @return  菜单栏句柄
 */
static HMENU main_create_menubar(void)
{
    HMENU h_menu = CreateMenu();
    HMENU h_file = CreatePopupMenu();
    HMENU h_help = CreatePopupMenu();

    (void)AppendMenuW(h_file, MF_STRING, (UINT_PTR)MAIN_MID_EXIT, L"退出");
    (void)AppendMenuW(h_help, MF_STRING, (UINT_PTR)MAIN_MID_CHECKUPD, L"检查更新…");
    (void)AppendMenuW(h_help, MF_STRING, (UINT_PTR)MAIN_MID_ABOUT, L"关于 ToolSets…");

    (void)AppendMenuW(h_menu, MF_POPUP, (UINT_PTR)h_file, L"文件(&F)");
    (void)AppendMenuW(h_menu, MF_POPUP, (UINT_PTR)h_help, L"帮助(&H)");

    return h_menu;
}

/**
 * @brief   菜单栏命令回调(ui_menu转发WM_COMMAND)
 * @param   h_inst  应用实例句柄
 * @param   h_wnd   主界面窗口句柄
 * @param   cmd_id  命令ID
 * @return  无
 */
static void main_on_menu_cmd(HINSTANCE h_inst, HWND h_wnd, uint32_t cmd_id)
{
    switch (cmd_id)
    {
        case MAIN_MID_EXIT:
        {
            (void)DestroyWindow(h_wnd);
            break;
        }
        case MAIN_MID_CHECKUPD:
        {
            updater_check_async(h_wnd, 1u);
            break;
        }
        case MAIN_MID_ABOUT:
        {
            wchar_t msg[160];

            memset(&msg[0], 0, sizeof(msg));
            (void)wsprintfW(&msg[0],
                            L"ToolSets v" VERSION_WSTR L"\n纯C Win32 工具合集\n\n单EXE · 零依赖 · GitHub发布更新");
            (void)MessageBoxW(h_wnd, &msg[0], L"关于 ToolSets",
                              (MB_OK | MB_ICONINFORMATION));
            break;
        }
        default:
        {
            break;
        }
    }
    (void)h_inst;
}

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
                                 &items[0], count,
                                 main_create_menubar(), &main_on_menu_cmd);
    if (h_main == NULL) {
        return 1;
    }

    updater_check_async(h_main, 0u);   /* 启动静默检查(发现新版才提示; ini可关) */

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
