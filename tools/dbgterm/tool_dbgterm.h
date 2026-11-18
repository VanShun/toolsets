/**
 * @file    tool_dbgterm.h
 * @author  herongshun
 * @date    2026-11-13
 * @brief   调试终端工具主界面(功能菜单)接口
 */

#ifndef TOOL_DBGTERM_H
#define TOOL_DBGTERM_H

#include <windows.h>

/**
 * @brief   打开调试终端功能菜单主界面(可重复打开多个实例)
 * @param   h_inst  应用实例句柄
 * @param   h_owner 所有者窗口句柄
 * @return  无
 */
void tool_dbgterm_open(HINSTANCE h_inst, HWND h_owner);

#endif /* TOOL_DBGTERM_H */
