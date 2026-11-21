/**
 * @file    test_dbg_frame.c
 * @author  herongshun
 * @date    2026-11-06
 * @brief   dbg_frame 模块单元测试(控制台)
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include "dbg_frame.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define TEST_OUT_MAX      DBG_FRAME_OUT_MAX   /**< 输出缓冲宽字符容量(与模块输出容量同步) */
#define TEST_PASS         0       /**< 单项测试通过 */
#define TEST_FAIL         1       /**< 单项测试失败 */

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   默认装配选项(无时间戳, UTF-8, RX方向)
 * @param   p_opt 选项输出
 * @return  无
 */
static void make_opt(DBG_FRAME_OPT_T *p_opt)
{
    memset(p_opt, 0, sizeof(DBG_FRAME_OPT_T));
    p_opt->ts_enable = 0u;
    p_opt->p_dir_tag = L"RX";
    p_opt->enc = (uint32_t)DBG_FRAME_ENC_UTF8;
}

/**
 * @brief   测试分片到达的行装配(多次feed续写同一输出缓冲)
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_feed_fragmented(void)
{
    DBG_FRAME_T frame;
    DBG_FRAME_OPT_T opt;
    wchar_t out[TEST_OUT_MAX];
    uint32_t pos = 0u;
    int32_t ret = TEST_PASS;

    memset(&out[0], 0, sizeof(out));
    dbg_frame_init(&frame);
    make_opt(&opt);

    (void)dbg_frame_feed(&frame, (const uint8_t *)"he", 2u, &opt, &out[0], TEST_OUT_MAX, &pos);
    (void)dbg_frame_feed(&frame, (const uint8_t *)"llo\nwo", 6u, &opt, &out[0], TEST_OUT_MAX, &pos);
    (void)dbg_frame_feed(&frame, (const uint8_t *)"rld\r\n", 5u, &opt, &out[0], TEST_OUT_MAX, &pos);

    if ((pos == 0u) || (lstrcmpW(&out[0], L"hello\r\nworld\r\n") != 0)) {
        printf("[FAIL] test_feed_fragmented\n");
        ret = TEST_FAIL;
    }

    return ret;
}

/**
 * @brief   测试三种行尾归一(CR/ LF/ CRLF)
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_line_endings(void)
{
    DBG_FRAME_T frame;
    DBG_FRAME_OPT_T opt;
    wchar_t out[TEST_OUT_MAX];
    uint32_t pos = 0u;
    int32_t ret = TEST_PASS;

    memset(&out[0], 0, sizeof(out));
    dbg_frame_init(&frame);
    make_opt(&opt);

    (void)dbg_frame_feed(&frame, (const uint8_t *)"a\rb\r\n", 5u, &opt, &out[0], TEST_OUT_MAX, &pos);
    (void)dbg_frame_feed(&frame, (const uint8_t *)"c\n", 2u, &opt, &out[0], TEST_OUT_MAX, &pos);

    if (lstrcmpW(&out[0], L"a\r\nb\r\nc\r\n") != 0) {
        printf("[FAIL] test_line_endings\n");
        ret = TEST_FAIL;
    }

    return ret;
}

/**
 * @brief   测试UTF-8解码与空行输出
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_utf8_and_empty(void)
{
    DBG_FRAME_T frame;
    DBG_FRAME_OPT_T opt;
    wchar_t out[TEST_OUT_MAX];
    uint32_t pos = 0u;
    int32_t ret = TEST_PASS;
    /* "你好" 的UTF-8编码 */
    const uint8_t utf8_line[] = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};

    memset(&out[0], 0, sizeof(out));
    dbg_frame_init(&frame);
    make_opt(&opt);

    (void)dbg_frame_feed(&frame, &utf8_line[0], 6u, &opt, &out[0], TEST_OUT_MAX, &pos);
    (void)dbg_frame_feed(&frame, (const uint8_t *)"\n\nx\n", 4u, &opt, &out[0], TEST_OUT_MAX, &pos);

    if (lstrcmpW(&out[0], L"你好\r\n\r\nx\r\n") != 0) {
        printf("[FAIL] test_utf8_and_empty\n");
        ret = TEST_FAIL;
    }

    return ret;
}

/**
 * @brief   测试时间戳前缀
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_timestamp_prefix(void)
{
    DBG_FRAME_T frame;
    DBG_FRAME_OPT_T opt;
    wchar_t out[TEST_OUT_MAX];
    uint32_t pos = 0u;
    int32_t ret = TEST_PASS;

    memset(&out[0], 0, sizeof(out));
    dbg_frame_init(&frame);
    make_opt(&opt);
    opt.ts_enable = 1u;

    (void)dbg_frame_feed(&frame, (const uint8_t *)"hi\n", 3u, &opt, &out[0], TEST_OUT_MAX, &pos);

    /* 形如 [12:34:56.789][RX] hi\r\n */
    if ((out[0] != L'[') || (wcsstr(&out[0], L"][RX] hi\r\n") == NULL)) {
        printf("[FAIL] test_timestamp_prefix\n");
        ret = TEST_FAIL;
    }

    return ret;
}

/**
 * @brief   测试空闲断行
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_idle_flush(void)
{
    DBG_FRAME_T frame;
    DBG_FRAME_OPT_T opt;
    wchar_t out[TEST_OUT_MAX];
    uint32_t pos = 0u;
    uint32_t flushed = 0u;
    int32_t ret = TEST_PASS;

    memset(&out[0], 0, sizeof(out));
    dbg_frame_init(&frame);
    make_opt(&opt);

    (void)dbg_frame_feed(&frame, (const uint8_t *)"abc", 3u, &opt, &out[0], TEST_OUT_MAX, &pos);

    /* 阈值内不输出 */
    flushed = dbg_frame_flush_idle(&frame, &opt, &out[0], TEST_OUT_MAX, &pos);
    if (flushed != 0u) {
        printf("[FAIL] test_idle_flush: early flush\n");
        return TEST_FAIL;
    }

    /* 模拟时间流逝后输出 */
    frame.last_byte_tick = GetTickCount() - (DBG_FRAME_IDLE_MS + 50u);
    flushed = dbg_frame_flush_idle(&frame, &opt, &out[0], TEST_OUT_MAX, &pos);
    if ((flushed != 1u) || (lstrcmpW(&out[0], L"abc\r\n") != 0)) {
        printf("[FAIL] test_idle_flush\n");
        ret = TEST_FAIL;
    }

    return ret;
}

/**
 * @brief   测试超长行强制断行
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_force_split(void)
{
    DBG_FRAME_T frame;
    DBG_FRAME_OPT_T opt;
    wchar_t out[TEST_OUT_MAX];
    uint8_t big[DBG_FRAME_LINE_MAX + 8u];
    uint32_t pos = 0u;
    uint32_t i = 0u;
    int32_t ret = TEST_PASS;

    memset(&out[0], 0, sizeof(out));
    memset(&big[0], (uint8_t)'x', sizeof(big));
    dbg_frame_init(&frame);
    make_opt(&opt);

    (void)dbg_frame_feed(&frame, &big[0], (uint32_t)sizeof(big), &opt, &out[0], TEST_OUT_MAX, &pos);

    /* LINE_MAX+8 字节 -> 首行LINE_MAX字节强制断行, 余8字节停在半行 */
    for (i = 0u; i < TEST_OUT_MAX; i++) {
        if (out[i] == L'\r') {
            break;
        }
    }
    if ((i != DBG_FRAME_LINE_MAX) || (out[i + 1u] != L'\n')) {
        printf("[FAIL] test_force_split\n");
        ret = TEST_FAIL;
    }

    return ret;
}

/**
 * @brief   测试精简设备时间戳与无方向标记前缀
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_strip_dev_ts(void)
{
    DBG_FRAME_T frame;
    DBG_FRAME_OPT_T opt;
    wchar_t out[TEST_OUT_MAX];
    uint32_t pos = 0u;
    int32_t ret = TEST_PASS;

    memset(&out[0], 0, sizeof(out));
    dbg_frame_init(&frame);
    make_opt(&opt);
    opt.ts_enable = 1u;
    opt.p_dir_tag = NULL;
    opt.strip_dev_ts = 1u;

    /* logcat -v time 行首时间被剥除; 本地时间戳无[RX]标记 */
    (void)dbg_frame_feed(&frame,
                         (const uint8_t *)"11-17 15:18:21.149 I/yaxon ( 2035): hello\r\n",
                         47u, &opt, &out[0], TEST_OUT_MAX, &pos);
    if (wcsstr(&out[0], L"11-17 15:18:21.149") != NULL) {
        printf("[FAIL] test_strip_dev_ts: device ts not stripped\n");
        ret = TEST_FAIL;
    }
    if (wcsstr(&out[0], L"[RX]") != NULL) {
        printf("[FAIL] test_strip_dev_ts: RX tag shall be absent\n");
        ret = TEST_FAIL;
    }
    if (wcsstr(&out[0], L"I/yaxon ( 2035): hello") == NULL) {
        printf("[FAIL] test_strip_dev_ts: content lost\n");
        ret = TEST_FAIL;
    }

    /* 关闭开关: 设备时间保留 */
    memset(&out[0], 0, sizeof(out));
    dbg_frame_init(&frame);
    opt.strip_dev_ts = 0u;
    pos = 0u;
    (void)dbg_frame_feed(&frame,
                         (const uint8_t *)"11-17 15:18:21.149 I/yaxon ( 2035): hello\r\n",
                         47u, &opt, &out[0], TEST_OUT_MAX, &pos);
    if (wcsstr(&out[0], L"11-17 15:18:21.149") == NULL) {
        printf("[FAIL] test_strip_dev_ts: keep mode lost device ts\n");
        ret = TEST_FAIL;
    }

    /* 非时间开头行不受影响 */
    memset(&out[0], 0, sizeof(out));
    dbg_frame_init(&frame);
    opt.strip_dev_ts = 1u;
    pos = 0u;
    (void)dbg_frame_feed(&frame, (const uint8_t *)"plain line\r\n", 12u,
                         &opt, &out[0], TEST_OUT_MAX, &pos);
    if (wcsstr(&out[0], L"plain line") == NULL) {
        printf("[FAIL] test_strip_dev_ts: plain line damaged\n");
        ret = TEST_FAIL;
    }

    return ret;
}

/**
 * @brief   测试入口
 * @param   无
 * @return  0: 全部通过; 1: 存在失败
 */
int main(void)
{
    int32_t failed = 0;

    failed += test_feed_fragmented();
    failed += test_line_endings();
    failed += test_utf8_and_empty();
    failed += test_timestamp_prefix();
    failed += test_idle_flush();
    failed += test_force_split();
    failed += test_strip_dev_ts();

    if (failed == 0) {
        printf("[PASS] all dbg_frame tests passed\n");
        return 0;
    }
    printf("[FAIL] %d dbg_frame test(s) failed\n", failed);

    return 1;
}
