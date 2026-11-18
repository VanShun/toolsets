/**
 * @file    test_dbg_filter.c
 * @author  herongshun
 * @date    2026-11-06
 * @brief   dbg_filter 模块单元测试(控制台)
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include "dbg_filter.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define TEST_PASS    0    /**< 单项测试通过 */
#define TEST_FAIL    1    /**< 单项测试失败 */

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   测试组配置与子串命中判定
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_match_basic(void)
{
    DBG_FILTER_GROUP_T grp;
    uint32_t idx = 0xFFFFFFFFu;
    uint32_t ret = 0u;
    int32_t result = TEST_PASS;

    memset(&grp, 0, sizeof(grp));
    lstrcpyW(&grp.name[0], L"LOG");
    lstrcpyW(&grp.keyword[0][0], L"log");
    lstrcpyW(&grp.keyword[1][0], L"tag");
    grp.keyword_count = 2u;
    grp.enabled = 1u;
    dbg_filter_set_group(0u, &grp);

    memset(&grp, 0, sizeof(grp));
    lstrcpyW(&grp.name[0], L"MCU");
    lstrcpyW(&grp.keyword[0][0], L"MCU_drv");
    grp.keyword_count = 1u;
    grp.enabled = 1u;
    dbg_filter_set_group(1u, &grp);

    dbg_filter_set_enable(1u);

    ret = dbg_filter_match(L"11-06 10:00:01.123  D/LOG ( 1234): boot ok", &idx);
    if ((ret != (uint32_t)DBG_FILTER_RET_HIT) || (idx != 0u)) {
        printf("[FAIL] test_match_basic: expect hit group0 (case-insensitive)\n");
        result = TEST_FAIL;
    }

    ret = dbg_filter_match(L"[mcu_drv] init done", &idx);
    if ((ret != (uint32_t)DBG_FILTER_RET_HIT) || (idx != 1u)) {
        printf("[FAIL] test_match_basic: expect hit group1\n");
        result = TEST_FAIL;
    }

    ret = dbg_filter_match(L"nothing special here", &idx);
    if (ret != (uint32_t)DBG_FILTER_RET_MISS) {
        printf("[FAIL] test_match_basic: expect miss\n");
        result = TEST_FAIL;
    }

    return result;
}

/**
 * @brief   测试开关与BYPASS路径
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_enable_bypass(void)
{
    DBG_FILTER_GROUP_T grp;
    uint32_t ret = 0u;
    int32_t result = TEST_PASS;

    dbg_filter_set_enable(0u);
    ret = dbg_filter_match(L"anything log", NULL);
    if (ret != (uint32_t)DBG_FILTER_RET_BYPASS) {
        printf("[FAIL] test_enable_bypass: disabled shall bypass\n");
        result = TEST_FAIL;
    }

    dbg_filter_set_enable(1u);
    /* 停用组: 仅剩空组 -> BYPASS */
    memset(&grp, 0, sizeof(grp));
    grp.enabled = 0u;
    grp.keyword_count = 1u;
    lstrcpyW(&grp.keyword[0][0], L"log");
    dbg_filter_set_group(0u, &grp);
    dbg_filter_set_group(1u, &grp);
    ret = dbg_filter_match(L"log line", NULL);
    if (ret != (uint32_t)DBG_FILTER_RET_BYPASS) {
        printf("[FAIL] test_enable_bypass: no valid group shall bypass\n");
        result = TEST_FAIL;
    }

    /* 关键字长于行: 不命中 */
    memset(&grp, 0, sizeof(grp));
    grp.enabled = 1u;
    grp.keyword_count = 1u;
    lstrcpyW(&grp.keyword[0][0], L"this_is_a_very_long_keyword_string");
    dbg_filter_set_group(0u, &grp);
    ret = dbg_filter_match(L"short", NULL);
    if (ret != (uint32_t)DBG_FILTER_RET_MISS) {
        printf("[FAIL] test_enable_bypass: long keyword shall miss\n");
        result = TEST_FAIL;
    }

    return result;
}

/**
 * @brief   测试入口
 * @param   无
 * @return  0: 全部通过; 1: 存在失败
 */
int main(void)
{
    int32_t failed = 0;

    failed += test_match_basic();
    failed += test_enable_bypass();

    if (failed == 0) {
        printf("[PASS] all dbg_filter tests passed\n");
        return 0;
    }
    printf("[FAIL] %d dbg_filter test(s) failed\n", failed);

    return 1;
}
