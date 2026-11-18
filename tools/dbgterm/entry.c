/**
 * @file    entry.c
 * @author  herongshun
 * @date    2026-11-06
 * @brief   调试终端工具独立运行入口(薄壳)
 *          仅独立构建(tools/dbgterm/build.bat)编译本文件, 集合版不编译
 *          窗口类注册/窗口创建/消息处理均在 tool_dbgterm 模块内实现,
 *          集合版与独立版编译的模块源码完全一致, 仅入口文件不同
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "tool_dbgterm.h"

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   独立运行入口: 以无owner方式打开工具窗口并进入消息循环
 *          工具窗口销毁时(WM_DESTROY, 无owner分支)自行PostQuitMessage结束循环
 * @param   h_inst      当前应用实例句柄
 * @param   h_prev      前一实例句柄(Win32下恒为NULL, 未使用)
 * @param   p_cmd_line  命令行参数字符串(未使用)
 * @param   cmd_show    初始显示状态(工具窗口内部固定走SW_SHOWNORMAL, 未使用)
 * @return  消息循环退出码
 */
int WINAPI WinMain(HINSTANCE h_inst, HINSTANCE h_prev, char *p_cmd_line, int cmd_show)
{
    MSG msg;

    (void)h_prev;
    (void)p_cmd_line;
    (void)cmd_show;

    memset(&msg, 0, sizeof(msg));

    tool_dbgterm_open(h_inst, NULL);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
