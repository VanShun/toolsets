/**
 * @file    test_proto_cap.c
 * @author  herongshun
 * @date    2026-11-21
 * @brief   proto_cap 模块单元测试(控制台)
 *          覆盖: 关键字去空格/大小写匹配, 命中行从命中点提取, 后续行剥
 *          [时间戳]前缀提取, 行数窗口收口, 捕获中再命中先收口, 行数0
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include "proto_cap.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define TEST_PASS  0
#define TEST_FAIL  1

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   断言组包字节等于期望
 * @param   p_cap    捕获上下文(输出侧)
 * @param   p_out    组包输出
 * @param   out_len  组包长度
 * @param   p_expect [in] 期望字节
 * @param   expect_len 期望长度
 * @param   p_name   用例名
 * @return  0: 通过; 1: 失败
 */
static int32_t check_bytes(const uint8_t *p_out, uint32_t out_len,
                           const uint8_t *p_expect, uint32_t expect_len,
                           const char *p_name)
{
    uint32_t i = 0u;

    if (out_len != expect_len) {
        printf("[FAIL] %s: len %u != %u\n", p_name, (unsigned int)out_len,
               (unsigned int)expect_len);
        return TEST_FAIL;
    }
    for (i = 0u; i < expect_len; i++) {
        if (p_out[i] != p_expect[i]) {
            printf("[FAIL] %s: byte[%u] 0x%02X != 0x%02X\n", p_name, (unsigned int)i,
                   (unsigned int)p_out[i], (unsigned int)p_expect[i]);
            return TEST_FAIL;
        }
    }

    return TEST_PASS;
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 用例                                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief   用例: 带时间戳行 + 2行后续, 关键字带空格
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_basic_window(void)
{
    PROTO_CAP_T cap;
    PROTO_CAP_CFG_T cfg;
    uint8_t out[PROTO_CAP_BUF_MAX];
    uint32_t out_len = 0u;
    static const uint8_t expect[] =
    {
        0x23, 0x23, 0x02, 0xFE, 0x4C, 0x01, 0x02, 0xAB, 0xCD, 0xEF, 0x12, 0x34
    };

    memset(&cfg, 0, sizeof(cfg));
    lstrcpynW(&cfg.keyword[0], L"23 23 02 FE", PROTO_CAP_KW_WCHAR);
    cfg.lines = 2u;
    proto_cap_init(&cap);

    if (proto_cap_feed(&cap, &cfg, L"[12:33:44.123] 00 hello", &out[0], &out_len) != 0u) {
        printf("[FAIL] test_basic_window: false hit\n");
        return TEST_FAIL;
    }
    if (proto_cap_feed(&cap, &cfg, L"[12:33:44.124] 23 23 02 FE 4C 01", &out[0], &out_len) != 0u) {
        printf("[FAIL] test_basic_window: hit not started\n");
        return TEST_FAIL;
    }
    if (proto_cap_feed(&cap, &cfg, L"[12:33:44.125] 02 AB CD", &out[0], &out_len) != 0u) {
        printf("[FAIL] test_basic_window: flushed early\n");
        return TEST_FAIL;
    }
    if (proto_cap_feed(&cap, &cfg, L"[12:33:44.126] EF 12 34", &out[0], &out_len) != 1u) {
        printf("[FAIL] test_basic_window: not flushed at window end\n");
        return TEST_FAIL;
    }

    return check_bytes(&out[0], out_len, &expect[0], (uint32_t)sizeof(expect),
                       "test_basic_window");
}

/**
 * @brief   用例: 无空格小写hex命中 + 行数0(仅命中行)
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_nospace_zero_lines(void)
{
    PROTO_CAP_T cap;
    PROTO_CAP_CFG_T cfg;
    uint8_t out[PROTO_CAP_BUF_MAX];
    uint32_t out_len = 0u;
    static const uint8_t expect[] = {0x24, 0x24, 0x01, 0xFE, 0x99};

    memset(&cfg, 0, sizeof(cfg));
    lstrcpynW(&cfg.keyword[0], L"2424 01fe", PROTO_CAP_KW_WCHAR);
    cfg.lines = 0u;
    proto_cap_init(&cap);

    if (proto_cap_feed(&cap, &cfg, L"noise 242401fe99 tail", &out[0], &out_len) != 1u) {
        printf("[FAIL] test_nospace_zero_lines: not flushed\n");
        return TEST_FAIL;
    }

    return check_bytes(&out[0], out_len, &expect[0], (uint32_t)sizeof(expect),
                       "test_nospace_zero_lines");
}

/**
 * @brief   用例: 捕获中再次命中 -> 先收口前一组再开新组
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_rehit_flush(void)
{
    PROTO_CAP_T cap;
    PROTO_CAP_CFG_T cfg;
    uint8_t out[PROTO_CAP_BUF_MAX];
    uint32_t out_len = 0u;
    static const uint8_t expect1[] = {0x23, 0x23, 0x02, 0xFE, 0xAA};
    static const uint8_t expect2[] = {0x23, 0x23, 0x02, 0xFE, 0xBB, 0x00, 0x00, 0x00};

    memset(&cfg, 0, sizeof(cfg));
    lstrcpynW(&cfg.keyword[0], L"2323 02 FE", PROTO_CAP_KW_WCHAR);
    cfg.lines = 3u;
    proto_cap_init(&cap);

    (void)proto_cap_feed(&cap, &cfg, L"[1] 23 23 02 FE AA", &out[0], &out_len);
    /* 第二次命中: 应立即输出第一组 */
    if (proto_cap_feed(&cap, &cfg, L"[2] 23 23 02 FE BB", &out[0], &out_len) != 1u) {
        printf("[FAIL] test_rehit_flush: group1 not flushed\n");
        return TEST_FAIL;
    }
    if (check_bytes(&out[0], out_len, &expect1[0], (uint32_t)sizeof(expect1),
                    "test_rehit_flush#1") != TEST_PASS) {
        return TEST_FAIL;
    }
    /* 第三行普通行收满窗口(1+3行? collected=命中1+普通1 <4 需再喂) */
    (void)proto_cap_feed(&cap, &cfg, L"[3] 00", &out[0], &out_len);
    (void)proto_cap_feed(&cap, &cfg, L"[4] 00", &out[0], &out_len);
    if (proto_cap_feed(&cap, &cfg, L"[5] 00", &out[0], &out_len) != 1u) {
        printf("[FAIL] test_rehit_flush: group2 not flushed\n");
        return TEST_FAIL;
    }

    return check_bytes(&out[0], out_len, &expect2[0], (uint32_t)sizeof(expect2),
                       "test_rehit_flush#2");
}

/**
 * @brief   用例: 命中点前的时间戳数字不混入(从命中位置起提取)
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_ts_not_leaked(void)
{
    PROTO_CAP_T cap;
    PROTO_CAP_CFG_T cfg;
    uint8_t out[PROTO_CAP_BUF_MAX];
    uint32_t out_len = 0u;
    static const uint8_t expect[] = {0x23, 0x23, 0x02, 0xFE, 0x10};

    memset(&cfg, 0, sizeof(cfg));
    lstrcpynW(&cfg.keyword[0], L"23 23 02 FE", PROTO_CAP_KW_WCHAR);
    cfg.lines = 0u;
    proto_cap_init(&cap);

    /* 时间戳含大量hex字符(12:33:44 -> 123344), 不应混入 */
    (void)proto_cap_feed(&cap, &cfg, L"[12:33:44.567] 23 23 02 FE 10",
                         &out[0], &out_len);

    return check_bytes(&out[0], out_len, &expect[0], (uint32_t)sizeof(expect),
                       "test_ts_not_leaked");
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   测试入口
 * @param   无
 * @return  0: 全部通过; 1: 存在失败
 */
int main(void)
{
    int32_t failed = 0;

    failed += test_basic_window();
    failed += test_nospace_zero_lines();
    failed += test_rehit_flush();
    failed += test_ts_not_leaked();

    if (failed == 0) {
        printf("[PASS] all proto_cap tests passed\n");
        return 0;
    }
    printf("[FAIL] %d proto_cap test(s) failed\n", failed);

    return 1;
}
