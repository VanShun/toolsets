/**
 * @file    proto_cap.h
 * @author  herongshun
 * @date    2026-11-21
 * @brief   协议行捕获接口(纯C, 无界面依赖)
 *          从调试终端的显示行流中按"起始关键字+向下行数"捕获报文字节;
 *          提取按token: 仅接受"空白分隔的2位纯hex"token——logcat前缀
 *          (I/yaxon ( 2035): [2026/11/21 … INFO]: c:0623]: 等)与空行
 *          时间戳均不满足该形态, 不混入字节流;
 *          关键字忽略空格/大小写转字节序列后按字节对齐查找;
 *          空行(无有效token)不占行数窗口; 组包完成条件=窗口行数满或
 *          调用方强制收口(proto_cap_take, 供空闲超时/再次命中场景)
 */

#ifndef PROTO_CAP_H
#define PROTO_CAP_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define PROTO_CAP_BUF_MAX      8192u    /**< 组包字节缓冲容量 */
#define PROTO_CAP_KW_WCHAR     64u      /**< 关键字宽字符容量(去空格后≤32字节) */
#define PROTO_CAP_LINES_MAX    20u      /**< 向下行数上限 */

/* -------------------------------------------------------------------------- */
/*  类型定义                                                                    */
/* -------------------------------------------------------------------------- */

/** 捕获配置 */
typedef struct PROTO_CAP_CFG_TAG
{
    wchar_t   keyword[PROTO_CAP_KW_WCHAR];  /**< 起始关键字(可含空格, hex字符) */
    uint32_t  lines;                        /**< 命中行向下的追加行数(0=仅命中行) */
} PROTO_CAP_CFG_T;

/** 捕获状态机上下文(UI线程私有) */
typedef struct PROTO_CAP_TAG
{
    uint8_t   buf[PROTO_CAP_BUF_MAX];   /**< 组包字节缓冲 */
    uint32_t  len;                      /**< 已收集字节数 */
    uint32_t  collected;                /**< 已收集行数(含命中行) */
    uint8_t   active;                   /**< 1=捕获进行中 */
} PROTO_CAP_T;

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   初始化捕获上下文
 * @param   p_cap 捕获上下文
 * @return  无
 */
void proto_cap_init(PROTO_CAP_T *p_cap);

/**
 * @brief   喂入一行显示文本, 按配置捕获组包
 * @param   p_cap     [in/out] 捕获上下文
 * @param   p_cfg     [in]     捕获配置
 * @param   p_line    [in]     显示行文本(可带[时间戳]前缀)
 * @param   p_out     [out]    组包结果输出缓冲(容量PROTO_CAP_BUF_MAX)
 * @param   p_out_len [out]    组包字节数
 * @return  1: 本次喂入完成一组组包(p_out有效); 0: 未完成
 */
uint8_t proto_cap_feed(PROTO_CAP_T *p_cap, const PROTO_CAP_CFG_T *p_cfg,
                       const wchar_t *p_line, uint8_t *p_out, uint32_t *p_out_len);

/**
 * @brief   查询捕获是否进行中(有未收口的组)
 * @param   p_cap 捕获上下文
 * @return  1=进行中; 0=空闲
 */
uint8_t proto_cap_active(const PROTO_CAP_T *p_cap);

/**
 * @brief   强制收口当前组(空闲超时等场景; 空闲上下文返回0)
 * @param   p_cap     [in/out] 捕获上下文
 * @param   p_out     [out]    组包结果输出缓冲
 * @param   p_out_len [out]    组包字节数
 * @return  1=有组收口; 0=无
 */
uint8_t proto_cap_take(PROTO_CAP_T *p_cap, uint8_t *p_out, uint32_t *p_out_len);

#endif /* PROTO_CAP_H */
