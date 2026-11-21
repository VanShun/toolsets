/**
 * @file    dbg_frame.h
 * @author  herongshun
 * @date    2026-11-06
 * @brief   接收流处理接口(纯C, 无界面依赖)
 *          原始字节流 -> 行装配(跨分片/三种行尾归一/超长截断/空闲断行)
 *          -> 可选时间戳与方向前缀 -> 宽字符行文本输出(CRLF行尾)
 */

#ifndef DBG_FRAME_H
#define DBG_FRAME_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define DBG_FRAME_LINE_MAX      8192u    /**< 单行字节容量(超出强制断行; 覆盖logcat长报文/JSON) */
#define DBG_FRAME_OUT_MAX       32768u   /**< 单次输出宽字符容量(>=LINE_MAX解码展开+安全余量) */
#define DBG_FRAME_IDLE_MS       100u     /**< 空闲断行阈值(毫秒) */
#define DBG_FRAME_OUT_MARGIN    64u      /**< 输出缓冲安全余量(宽字符) */

/* -------------------------------------------------------------------------- */
/*  类型定义                                                                    */
/* -------------------------------------------------------------------------- */

/** 解码编码 */
typedef enum DBG_FRAME_ENC_TAG
{
    DBG_FRAME_ENC_ACP = 0,   /**< 本地ANSI(GBK), 串口默认 */
    DBG_FRAME_ENC_UTF8,      /**< UTF-8, ADB默认 */
    DBG_FRAME_ENC_COUNT      /**< 编码种类数 */
} DBG_FRAME_ENC_E;

/** 装配选项 */
typedef struct DBG_FRAME_OPT_TAG
{
    uint8_t         ts_enable;      /**< 1=行首加[HH:MM:SS.mmm]前缀(带方向时附[方向]) */
    const wchar_t  *p_dir_tag;      /**< 方向标签(NULL=不加, 如仅RX场景省去冗余) */
    uint32_t        enc;            /**< 解码编码(DBG_FRAME_ENC_E) */
    uint8_t         strip_dev_ts;   /**< 1=剥除行首设备侧时间戳(logcat -v time的"MM-DD HH:MM:SS.mmm ") */
} DBG_FRAME_OPT_T;

/** 行装配上下文(UI线程私有, 无需加锁) */
typedef struct DBG_FRAME_TAG
{
    uint8_t  line[DBG_FRAME_LINE_MAX];   /**< 半行字节缓冲 */
    uint32_t line_len;                   /**< 半行当前长度 */
    uint8_t  cr_pending;                 /**< 已见CR, 吞掉紧随的LF */
    uint32_t last_byte_tick;             /**< 最近字节到达时刻(GetTickCount) */
} DBG_FRAME_T;

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   初始化装配上下文
 * @param   p_frame 装配上下文
 * @return  无
 */
void dbg_frame_init(DBG_FRAME_T *p_frame);

/**
 * @brief   送入原始字节并输出装配完成的行文本
 * @param   p_frame   装配上下文
 * @param   p_data    [in]     原始字节
 * @param   len       [in]     字节数
 * @param   p_opt     [in]     装配选项
 * @param   p_out     [in/out] 输出缓冲(行以CRLF结尾拼接, 多次调用续写)
 * @param   out_max   [in]     输出容量(宽字符)
 * @param   p_out_pos [in/out] 当前写位置(调用方维护, 进入时为已写长度)
 * @return  实际消费的字节数(输出缓冲将满时可能小于len, 余量下轮再喂)
 */
uint32_t dbg_frame_feed(DBG_FRAME_T *p_frame, const uint8_t *p_data, uint32_t len,
                        const DBG_FRAME_OPT_T *p_opt,
                        wchar_t *p_out, uint32_t out_max, uint32_t *p_out_pos);

/**
 * @brief   空闲断行: 半行非空且距末字节超过阈值时强制成行
 * @param   p_frame   装配上下文
 * @param   p_opt     [in]     装配选项
 * @param   p_out     [in/out] 输出缓冲
 * @param   out_max   [in]     输出容量(宽字符)
 * @param   p_out_pos [in/out] 当前写位置
 * @return  1: 有行输出; 0: 无
 */
uint32_t dbg_frame_flush_idle(DBG_FRAME_T *p_frame, const DBG_FRAME_OPT_T *p_opt,
                              wchar_t *p_out, uint32_t out_max, uint32_t *p_out_pos);

/**
 * @brief   查询是否有到达空闲断行条件的半行(无副作用, 供节拍早退判断)
 * @param   p_frame 装配上下文
 * @return  1: 半行非空且已空闲超过阈值; 0: 无
 */
uint8_t dbg_frame_idle_ready(const DBG_FRAME_T *p_frame);

#endif /* DBG_FRAME_H */
