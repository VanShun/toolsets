/**
 * @file    test_proto_cap.c
 * @author  herongshun
 * @date    2026-11-21
 * @brief   proto_cap 模块单元测试(控制台)
 *          覆盖: 2位hex token提取(logcat前缀/时间戳不污染), 关键字字节
 *          对齐命中, 行数窗口收口, 空行跳过, 再命中收口, 强制收口
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
 * @param   p_out      组包输出
 * @param   out_len    组包长度
 * @param   p_expect   期望字节
 * @param   expect_len 期望长度
 * @param   p_name     用例名
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
 * @brief   用例: 行数0(仅命中行) + 噪声token过滤
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_zero_lines_noise(void)
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

    if (proto_cap_feed(&cap, &cfg, L"noise 24 24 01 FE 99 tail 2035):",
                       &out[0], &out_len) != 1u) {
        printf("[FAIL] test_zero_lines_noise: not flushed\n");
        return TEST_FAIL;
    }

    return check_bytes(&out[0], out_len, &expect[0], (uint32_t)sizeof(expect),
                       "test_zero_lines_noise");
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
    if (proto_cap_feed(&cap, &cfg, L"[2] 23 23 02 FE BB", &out[0], &out_len) != 1u) {
        printf("[FAIL] test_rehit_flush: group1 not flushed\n");
        return TEST_FAIL;
    }
    if (check_bytes(&out[0], out_len, &expect1[0], (uint32_t)sizeof(expect1),
                    "test_rehit_flush#1") != TEST_PASS) {
        return TEST_FAIL;
    }
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
 * @brief   hex字符串(可含空格)转字节
 * @param   p_hex [in]  hex文本
 * @param   p_out [out] 字节输出
 * @return  字节数
 */
static uint32_t hex_to_bytes(const wchar_t *p_hex, uint8_t *p_out)
{
    uint32_t n = 0u;
    uint8_t hi = 0u;
    uint8_t have = 0u;
    const wchar_t *digits = L"0123456789ABCDEF";
    const wchar_t *p = p_hex;

    while (*p != L'\0') {
        const wchar_t *p_hit = wcschr(digits, *p);

        if (p_hit != NULL) {
            uint8_t v = (uint8_t)(p_hit - digits);

            if (have == 0u) {
                hi = v;
                have = 1u;
            } else {
                p_out[n] = (uint8_t)((hi << 4) | v);
                n++;
                have = 0u;
            }
        }
        p++;
    }

    return n;
}

/**
 * @brief   用例: 真实logcat格式行, 前缀含大量hex状字符不污染;
 *          空行(仅时间戳)跳过; 强制收口
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_logcat_real(void)
{
    PROTO_CAP_T cap;
    PROTO_CAP_CFG_T cfg;
    uint8_t out[PROTO_CAP_BUF_MAX];
    uint8_t expect[PROTO_CAP_BUF_MAX];
    uint32_t expect_len = 0u;
    uint32_t out_len = 0u;
    static const wchar_t line1[] =
        L"[17:07:57.221] I/yaxon   ( 2035): [2026/11/21 17:07:56.717113 INFO]: "
        L"[gps_app.exe yx_tcp_drv.c:0623]: 23 23 02 FE 4C 53 36 41 42 43 31 30 37 53 43 "
        L"30 30 30 35 30 33 01 01 CD 1A 0B 15 11 07 38 01 FE 03 01 8F FF FF FF FF FF FF "
        L"FF FF FF 00 FF FE 0B B8 63 FF 02 01 01 00 FF EC 77 00 00 FF FF F6 00 00 05 01 "
        L"07 0B 51 BD 01 75 A3 C7 06 01 01 0C DF 01 01 0C DF 01 01 41 01 01 41 07 00 00 "
        L"00 00 00 00 00 00 00 08 01 01 FF FF FF FF 00 A8 00 01 A8 0C DF 0C DF";
    static const wchar_t line2[] =
        L"[17:07:57.221] I/yaxon   ( 2035): [2026/11/21 17:07:56.717113 INFO]: "
        L"[gps_app.exe yx_tcp_drv.c:0629]: 0C DF 09 01 01 00 20 41 41 B4";
    /* 期望 = 行1命中点起的全部token + 行2全部token */
    static const wchar_t expect_hex[] =
        L"23 23 02 FE 4C 53 36 41 42 43 31 30 37 53 43 30 30 30 35 30 33 01 01 CD "
        L"1A 0B 15 11 07 38 01 FE 03 01 8F FF FF FF FF FF FF FF FF FF 00 FF FE "
        L"0B B8 63 FF 02 01 01 00 FF EC 77 00 00 FF FF F6 00 00 05 01 07 0B 51 "
        L"BD 01 75 A3 C7 06 01 01 0C DF 01 01 0C DF 01 01 41 01 01 41 07 00 00 "
        L"00 00 00 00 00 00 00 08 01 01 FF FF FF FF 00 A8 00 01 A8 0C DF 0C DF "
        L"0C DF 09 01 01 00 20 41 41 B4";

    memset(&cfg, 0, sizeof(cfg));
    lstrcpynW(&cfg.keyword[0], L"23 23 02 FE", PROTO_CAP_KW_WCHAR);
    cfg.lines = 5u;
    proto_cap_init(&cap);
    expect_len = hex_to_bytes(&expect_hex[0], &expect[0]);

    if (proto_cap_feed(&cap, &cfg, &line1[0], &out[0], &out_len) != 0u) {
        printf("[FAIL] test_logcat_real: unexpected flush\n");
        return TEST_FAIL;
    }
    if (proto_cap_feed(&cap, &cfg, L"[17:07:57.221] ", &out[0], &out_len) != 0u) {
        printf("[FAIL] test_logcat_real: blank line counted\n");
        return TEST_FAIL;
    }
    if (proto_cap_feed(&cap, &cfg, &line2[0], &out[0], &out_len) != 0u) {
        printf("[FAIL] test_logcat_real: flushed early\n");
        return TEST_FAIL;
    }
    if (proto_cap_active(&cap) != 1u) {
        printf("[FAIL] test_logcat_real: not active\n");
        return TEST_FAIL;
    }
    if (proto_cap_take(&cap, &out[0], &out_len) != 1u) {
        printf("[FAIL] test_logcat_real: take failed\n");
        return TEST_FAIL;
    }

    return check_bytes(&out[0], out_len, &expect[0], expect_len, "test_logcat_real");
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
    failed += test_zero_lines_noise();
    failed += test_rehit_flush();
    failed += test_logcat_real();

    if (failed == 0) {
        printf("[PASS] all proto_cap tests passed\n");
        return 0;
    }
    printf("[FAIL] %d proto_cap test(s) failed\n", failed);

    return 1;
}
