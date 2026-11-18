/**
 * @file    filter_view.h
 * @author  herongshun
 * @date    2026-11-17
 * @brief   过滤日志独立窗口接口(形态对标getsscom: 全量日志与过滤日志分窗显示)
 *          窗口内配置过滤组与关键字, 只显示命中当前组的接收行
 */

#ifndef FILTER_VIEW_H
#define FILTER_VIEW_H

#include <windows.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   打开过滤日志窗口(已打开则置前; 单实例)
 * @param   h_inst  应用实例句柄
 * @param   h_owner 所有者窗口(调试终端主窗口)
 * @return  无
 */
void filter_view_open(HINSTANCE h_inst, HWND h_owner);

/**
 * @brief   过滤窗口是否处于打开状态
 * @param   无
 * @return  1: 打开; 0: 关闭
 */
uint8_t filter_view_is_open(void);

/**
 * @brief   追加一行命中文本到过滤窗口(主窗口UI线程调用; 窗口未打开时忽略)
 *          行文本按默认色追加, 行内命中该组关键字的部分按组颜色高亮;
 *          批量上下文(begin/end_batch)中调用时免于逐行解锁/重绘, 洪峰下自动降级为纯追加
 * @param   p_line     行文本(不含行尾)
 * @param   group_idx  命中过滤组号(取其关键字与颜色)
 * @return  无
 */
void filter_view_append_hit(const wchar_t *p_line, uint32_t group_idx);

/**
 * @brief   开始批量追加(一个刷新节拍一次: 解锁+关重绘; 未开窗时安全空操作)
 * @param   无
 * @return  无
 */
void filter_view_begin_batch(void);

/**
 * @brief   结束批量追加(回锁+恢复重绘+滚动到底)
 * @param   无
 * @return  无
 */
void filter_view_end_batch(void);

/**
 * @brief   关闭过滤窗口(主窗口销毁时调用)
 * @param   无
 * @return  无
 */
void filter_view_close(void);

#endif /* FILTER_VIEW_H */
