/**
 * @file    dbg_filter.c
 * @author  herongshun
 * @date    2026-11-06
 * @brief   接收关键字过滤实现: 组管理 + 大小写不敏感子串匹配
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "dbg_filter.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define DBG_FILTER_TRUE    1u   /**< 布尔真 */
#define DBG_FILTER_FALSE   0u   /**< 布尔假 */

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static DBG_FILTER_GROUP_T g_filter_groups[DBG_FILTER_GROUP_MAX];          /**< 过滤组表(静态零初始化) */
static uint8_t            g_filter_enable = DBG_FILTER_FALSE;           /**< 总开关 */

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   大小写不敏感子串匹配
 * @param   p_line 行文本
 * @param   p_kw   关键字
 * @return  1: 命中; 0: 未命中
 */
static uint8_t filter_contains_ci(const wchar_t *p_line, const wchar_t *p_kw)
{
    uint32_t line_len = (uint32_t)lstrlenW(p_line);
    uint32_t kw_len = (uint32_t)lstrlenW(p_kw);
    uint32_t i = 0u;
    uint32_t j = 0u;
    wchar_t cl = 0u;
    wchar_t ck = 0u;
    uint8_t hit = DBG_FILTER_TRUE;

    if ((kw_len == 0u) || (kw_len > line_len)) {
        return DBG_FILTER_FALSE;
    }

    for (i = 0u; (i + kw_len) <= line_len; i++) {
        hit = DBG_FILTER_TRUE;
        for (j = 0u; j < kw_len; j++) {
            cl = towlower(p_line[i + j]);
            ck = towlower(p_kw[j]);
            if (cl != ck) {
                hit = DBG_FILTER_FALSE;
                break;
            }
        }
        if (hit == DBG_FILTER_TRUE) {
            return DBG_FILTER_TRUE;
        }
    }

    return DBG_FILTER_FALSE;
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void dbg_filter_set_group(uint32_t group_idx, const DBG_FILTER_GROUP_T *p_group)
{
    if ((group_idx >= DBG_FILTER_GROUP_MAX) || (p_group == NULL)) {
        return;
    }
    g_filter_groups[group_idx] = *p_group;
    g_filter_groups[group_idx].name[DBG_FILTER_NAME_WCHAR - 1u] = L'\0';
}

void dbg_filter_get_group(uint32_t group_idx, DBG_FILTER_GROUP_T *p_group)
{
    if ((group_idx >= DBG_FILTER_GROUP_MAX) || (p_group == NULL)) {
        return;
    }
    *p_group = g_filter_groups[group_idx];
}

void dbg_filter_set_enable(uint8_t enabled)
{
    g_filter_enable = (enabled != DBG_FILTER_FALSE) ? DBG_FILTER_TRUE : DBG_FILTER_FALSE;
}

uint32_t dbg_filter_match(const wchar_t *p_line, uint32_t *p_group_idx)
{
    uint32_t group = 0u;
    uint32_t kw = 0u;
    const DBG_FILTER_GROUP_T *p_grp = NULL;
    uint8_t any_valid = DBG_FILTER_FALSE;

    if (p_line == NULL) {
        return (uint32_t)DBG_FILTER_RET_BYPASS;
    }
    if (g_filter_enable == DBG_FILTER_FALSE) {
        return (uint32_t)DBG_FILTER_RET_BYPASS;
    }

    for (group = 0u; group < DBG_FILTER_GROUP_MAX; group++) {
        p_grp = &g_filter_groups[group];
        if ((p_grp->enabled == DBG_FILTER_FALSE) || (p_grp->keyword_count == 0u)) {
            continue;
        }
        any_valid = DBG_FILTER_TRUE;
        for (kw = 0u; kw < p_grp->keyword_count; kw++) {
            if (p_grp->keyword[kw][0] == L'\0') {
                continue;
            }
            if (filter_contains_ci(p_line, &p_grp->keyword[kw][0]) == DBG_FILTER_TRUE) {
                if (p_group_idx != NULL) {
                    *p_group_idx = group;
                }
                return (uint32_t)DBG_FILTER_RET_HIT;
            }
        }
    }

    return (any_valid != DBG_FILTER_FALSE) ? (uint32_t)DBG_FILTER_RET_MISS
                                           : (uint32_t)DBG_FILTER_RET_BYPASS;
}

uint32_t dbg_filter_get_group_color(uint32_t group_idx)
{
    if (group_idx >= DBG_FILTER_GROUP_MAX) {
        return 0x00000000u;   /* 黑色 */
    }
    return g_filter_groups[group_idx].color;
}
