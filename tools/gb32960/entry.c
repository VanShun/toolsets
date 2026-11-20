/**
 * @file    entry.c
 * @author  herongshun
 * @date    2026-11-20
 * @brief   GB32960 解析工具独立运行入口(薄壳)
 *          仅独立构建(tools/gb32960/build.bat)编译本文件, 集合版不编译
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "tool_gb32960.h"

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   独立运行入口: 以无owner方式打开工具窗口并进入消息循环
 * @param   h_inst      当前应用实例句柄
 * @param   h_prev      前一实例句柄(Win32下恒为NULL, 未使用)
 * @param   p_cmd_line  命令行参数字符串(未使用)
 * @param   cmd_show    初始显示状态(未使用)
 * @return  消息循环退出码
 */
int WINAPI WinMain(HINSTANCE h_inst, HINSTANCE h_prev, char *p_cmd_line, int cmd_show)
{
    MSG msg;

    (void)h_prev;
    (void)p_cmd_line;
    (void)cmd_show;

    memset(&msg, 0, sizeof(msg));

    tool_gb32960_open(h_inst, NULL);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
