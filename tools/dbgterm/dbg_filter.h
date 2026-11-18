/**
 * @file    dbg_filter.h
 * @author  herongshun
 * @date    2026-11-06
 * @brief   接收关键字过滤接口(纯C, 无界面依赖)
 *          多组关键字(对标getsscom多组过滤形态), 组内任一关键字子串命中即该组命中
 */

#ifndef DBG_FILTER_H
#define DBG_FILTER_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define DBG_FILTER_GROUP_MAX     12u   /**< 过滤组数量(过滤窗口行数上限) */
#define DBG_FILTER_NAME_WCHAR    32u   /**< 组名宽字符容量 */
#define DBG_FILTER_KEYWORD_MAX   8u    /**< 组内关键字最大条数 */
#define DBG_FILTER_KEYWORD_WCHAR 64u   /**< 关键字宽字符容量 */

/* -------------------------------------------------------------------------- */
/*  类型定义                                                                    */
/* -------------------------------------------------------------------------- */

/** 行判定结果 */
typedef enum DBG_FILTER_RET_TAG
{
    DBG_FILTER_RET_BYPASS = 0,   /**< 过滤未启用/无有效组: 全部放行 */
    DBG_FILTER_RET_MISS,         /**< 未命中任何组 */
    DBG_FILTER_RET_HIT           /**< 命中(p_group_idx返回组号) */
} DBG_FILTER_RET_E;

/** 过滤组 */
typedef struct DBG_FILTER_GROUP_TAG
{
    wchar_t  name[DBG_FILTER_NAME_WCHAR];                                /**< 组名 */
    wchar_t  keyword[DBG_FILTER_KEYWORD_MAX][DBG_FILTER_KEYWORD_WCHAR];  /**< 组内关键字 */
    uint32_t keyword_count;                                              /**< 关键字条数 */
    uint32_t color;                                                      /**< 命中行显示色(0x00BBGGRR) */
    uint8_t  enabled;                                                    /**< 组启用标志 */
} DBG_FILTER_GROUP_T;

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   设置一个过滤组(整体替换)
 * @param   group_idx 组号[0, DBG_FILTER_GROUP_MAX)
 * @param   p_group   [in] 组内容(不可为NULL)
 * @return  无
 */
void dbg_filter_set_group(uint32_t group_idx, const DBG_FILTER_GROUP_T *p_group);

/**
 * @brief   读取一个过滤组
 * @param   group_idx 组号
 * @param   p_group   [out] 组内容输出
 * @return  无
 */
void dbg_filter_get_group(uint32_t group_idx, DBG_FILTER_GROUP_T *p_group);

/**
 * @brief   设置过滤总开关
 * @param   enabled 1=启用过滤判定; 0=全部放行
 * @return  无
 */
void dbg_filter_set_enable(uint8_t enabled);

/**
 * @brief   对单行文本做过滤判定
 * @param   p_line      [in]  行文本(宽字符)
 * @param   p_group_idx [out] 命中组号(HIT时有效, 可为NULL)
 * @return  DBG_FILTER_RET_E 判定结果
 */
uint32_t dbg_filter_match(const wchar_t *p_line, uint32_t *p_group_idx);

/**
 * @brief   获取指定组的显示颜色
 * @param   group_idx 组号
 * @return  颜色(0x00BBGGRR); 组号非法返回黑色
 */
uint32_t dbg_filter_get_group_color(uint32_t group_idx);

#endif /* DBG_FILTER_H */
