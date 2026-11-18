/**
 * @file    dbg_conf.h
 * @author  herongshun
 * @date    2026-11-17
 * @brief   dbgterm 配置文件读写接口(exe同目录 dbgterm.ini, 各模块分节使用)
 */

#ifndef DBG_CONF_H
#define DBG_CONF_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   获取配置文件完整路径(exe同目录 dbgterm.ini)
 * @param   p_out 路径输出缓冲
 * @param   max   缓冲容量(宽字符)
 * @return  无
 */
void dbg_conf_path(wchar_t *p_out, uint32_t max);

/**
 * @brief   读取无符号整型配置
 * @param   p_sec 节名
 * @param   p_key 键名
 * @param   def   缺省值
 * @return  配置值
 */
uint32_t dbg_conf_get_uint(const wchar_t *p_sec, const wchar_t *p_key, uint32_t def);

/**
 * @brief   写入无符号整型配置
 * @param   p_sec  节名
 * @param   p_key  键名
 * @param   val    值
 * @return  无
 */
void dbg_conf_set_uint(const wchar_t *p_sec, const wchar_t *p_key, uint32_t val);

/**
 * @brief   读取字符串配置
 * @param   p_sec 节名
 * @param   p_key 键名
 * @param   p_def 缺省值
 * @param   p_out 输出缓冲
 * @param   max   缓冲容量(宽字符)
 * @return  无
 */
void dbg_conf_get_str(const wchar_t *p_sec, const wchar_t *p_key, const wchar_t *p_def,
                      wchar_t *p_out, uint32_t max);

/**
 * @brief   写入字符串配置
 * @param   p_sec 节名
 * @param   p_key 键名
 * @param   p_val 值
 * @return  无
 */
void dbg_conf_set_str(const wchar_t *p_sec, const wchar_t *p_key, const wchar_t *p_val);

#endif /* DBG_CONF_H */
