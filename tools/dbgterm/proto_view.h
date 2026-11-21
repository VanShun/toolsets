/**
 * @file    proto_view.h
 * @author  herongshun
 * @date    2026-11-21
 * @brief   协议解析窗口接口(调试终端扩展功能)
 *          从调试终端显示行流捕获报文(起始关键字+向下行数), 调用注册的
 *          协议解析器实时解析, 记录列表/详情查看/自动落盘;
 *          解析器经 proto_view_set_parser 注入(合集版由app层接gb32960,
 *          独立dbgterm未注册时仅提示无解析器)
 */

#ifndef PROTO_VIEW_H
#define PROTO_VIEW_H

#include <stdint.h>
#include <windows.h>

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define PROTO_VIEW_NAME_WCHAR  48u    /**< 解析器名称容量 */
#define PROTO_VIEW_PARSER_MAX  4u     /**< 可注册解析器上限 */

/* -------------------------------------------------------------------------- */
/*  类型定义                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief 协议解析回调: 字节流 -> 文本解析报告
 * @param   p_data [in]  报文字节
 * @param   len    [in]  字节数
 * @param   p_out  [out] 报告输出缓冲
 * @param   out_cap [in] 输出容量(宽字符)
 * @return  成功解析的帧数(0=失败, 报告含原因)
 */
typedef uint32_t (*PROTO_VIEW_PARSE_FN)(const uint8_t *p_data, uint32_t len,
                                        wchar_t *p_out, uint32_t out_cap);

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   注册/更新协议解析器(合集装配层调用)
 * @param   p_name 解析器显示名(如 L"GB/T 32960")
 * @param   p_fn   解析回调
 * @return  无
 */
void proto_view_set_parser(const wchar_t *p_name, PROTO_VIEW_PARSE_FN p_fn);

/**
 * @brief   查询协议解析窗口是否打开
 * @param   无
 * @return  1: 打开; 0: 关闭
 */
uint8_t proto_view_is_open(void);

/**
 * @brief   喂入一行显示文本(主窗口每行落地时调用, 窗口关闭时由调用方跳过)
 * @param   p_line 行文本(可带[时间戳]前缀)
 * @return  无
 */
void proto_view_feed(const wchar_t *p_line);

/**
 * @brief   打开协议解析窗口(单实例, 已打开则前置)
 * @param   h_inst  应用实例句柄
 * @param   h_owner 所有者窗口句柄
 * @return  无
 */
void proto_view_open(HINSTANCE h_inst, HWND h_owner);

#endif /* PROTO_VIEW_H */
