/**
 * @file    tool_hexascii.h
 * @author  herongshun
 * @date    2026-10-26
 * @brief   Hex-ASCII 双向转换工具接口
 */

#ifndef TOOL_HEXASCII_H
#define TOOL_HEXASCII_H

#include <windows.h>

/**
 * @brief   打开 Hex-ASCII 转换工具窗口(可重复打开多个实例)
 * @param   h_inst  应用实例句柄
 * @param   h_owner 所有者窗口句柄
 * @return  无
 */
void tool_hexascii_open(HINSTANCE h_inst, HWND h_owner);

#endif /* TOOL_HEXASCII_H */
