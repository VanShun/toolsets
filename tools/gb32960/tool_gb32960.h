/**
 * @file    tool_gb32960.h
 * @author  herongshun
 * @date    2026-11-20
 * @brief   GB32960 协议解析工具界面接口
 */

#ifndef TOOL_GB32960_H
#define TOOL_GB32960_H

#include <windows.h>

/**
 * @brief   打开 GB32960 解析工具窗口(可重复打开多个实例)
 * @param   h_inst  应用实例句柄
 * @param   h_owner 所有者窗口句柄
 * @return  无
 */
void tool_gb32960_open(HINSTANCE h_inst, HWND h_owner);

#endif /* TOOL_GB32960_H */
