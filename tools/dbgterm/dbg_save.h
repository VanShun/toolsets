/**
 * @file    dbg_save.h
 * @author  herongshun
 * @date    2026-11-18
 * @brief   日志保存接口(双通道: 主窗口全量日志 / 过滤窗口命中日志)
 *          自动保存(持续追加)与立即快照, 输出到可配置文件夹(两通道共享);
 *          文件为UTF-8(带BOM), 主通道 dbgterm[_auto]_*.txt, 过滤通道
 *          dbgterm_flt[_auto]_*.txt
 */

#ifndef DBG_SAVE_H
#define DBG_SAVE_H

#include <windows.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define DBG_SAVE_DIR_MAX   260u   /**< 保存目录宽字符容量 */
#define DBG_SAVE_MSG_MAX   260u   /**< 结果消息宽字符容量 */
#define DBG_SAVE_CH_MAIN   0u     /**< 主窗口(全量日志)通道 */
#define DBG_SAVE_CH_FILTER 1u     /**< 过滤窗口(命中日志)通道 */

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   初始化(读取ini中的目录与两通道自动保存开关; 主窗口WM_CREATE调用)
 * @return  无
 */
void dbg_save_init(void);

/**
 * @brief   打开文件夹选择对话框并持久化保存目录(两通道共享)
 * @param   h_owner 所有者窗口
 * @return  1: 目录已更换; 0: 取消
 */
uint8_t dbg_save_pick_dir(HWND h_owner);

/**
 * @brief   当前保存目录
 * @param   p_out 目录输出缓冲
 * @param   max   缓冲容量
 * @return  无
 */
void dbg_save_get_dir(wchar_t *p_out, uint32_t max);

/**
 * @brief   设置指定通道自动保存开关(持久化; 开启即建文件, 关闭即封盘)
 * @param   ch  通道(DBG_SAVE_CH_MAIN / DBG_SAVE_CH_FILTER)
 * @param   on  1=开启; 0=关闭
 * @return  无
 */
void dbg_save_set_autosave_ch(uint32_t ch, uint8_t on);

/**
 * @brief   指定通道自动保存是否开启
 * @param   ch 通道
 * @return  1: 开启; 0: 关闭
 */
uint8_t dbg_save_autosave_on_ch(uint32_t ch);

/**
 * @brief   指定通道数据喂入(自动保存开启时持续追加; UI线程调用)
 * @param   ch    通道
 * @param   p_text 文本(CRLF行尾)
 * @return  无
 */
void dbg_save_feed_ch(uint32_t ch, const wchar_t *p_text);

/**
 * @brief   指定通道立即保存: 文本快照写入新文件(单次完整落盘)
 * @param   ch    通道
 * @param   p_text 文本(宽字符)
 * @param   p_msg  [out] 结果消息缓冲(成功时含文件路径)
 * @return  0: 成功; -1: 失败(消息含原因)
 */
int32_t dbg_save_snapshot_ch(uint32_t ch, const wchar_t *p_text, wchar_t *p_msg);

/**
 * @brief   收尾(关闭全部通道记录文件; 主窗口销毁时调用)
 * @param   无
 * @return  无
 */
void dbg_save_shutdown(void);

/* ---- 主通道便捷接口(与旧接口兼容, 等价于 ch=DBG_SAVE_CH_MAIN) ---- */

void     dbg_save_set_autosave(uint8_t on);                             /**< 主通道开关 */
uint8_t  dbg_save_autosave_on(void);                                    /**< 主通道状态 */
void     dbg_save_feed(const wchar_t *p_text);                          /**< 主通道喂入 */
int32_t  dbg_save_snapshot(const wchar_t *p_text, wchar_t *p_msg);      /**< 主通道快照 */

#endif /* DBG_SAVE_H */
