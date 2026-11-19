/**
 * @file    color_view.h
 * @author  herongshun
 * @date    2026-11-19
 * @brief   多彩关键字配置窗口接口
 *          行式关键字+颜色配置(与过滤窗口风格一致), 作用于主窗口接收区着色;
 *          配置持久化到 ini [Color] 节, 变更经回调通知主窗口重建着色词表
 */

#ifndef COLOR_VIEW_H
#define COLOR_VIEW_H

#include <windows.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   打开多彩关键字配置窗口(已打开则置前; 单实例)
 * @param   h_inst  应用实例句柄
 * @param   h_owner 所有者窗口(调试终端主窗口)
 * @return  无
 */
void color_view_open(HINSTANCE h_inst, HWND h_owner);

/**
 * @brief   配置窗口是否处于打开状态
 * @param   无
 * @return  1: 打开; 0: 关闭
 */
uint8_t color_view_is_open(void);

/**
 * @brief   关闭配置窗口(主窗口销毁时调用)
 * @param   无
 * @return  无
 */
void color_view_close(void);

/**
 * @brief   注册配置变更回调(任一行关键字或颜色变化时调用, 主窗口据此重建词表)
 * @param   pfn 变更回调(可为NULL清除)
 * @return  无
 */
void color_view_set_changed_fn(void (*pfn)(void));

#endif /* COLOR_VIEW_H */
