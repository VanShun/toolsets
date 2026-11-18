/**
 * @file    test_adb_core.c
 * @author  herongshun
 * @date    2026-11-06
 * @brief   adb_core 模块单元测试(控制台, 不依赖adb/真机)
 *          以 "ping -n 2" 作为流式输出stub子进程, 验证管道读写线程与关闭时序
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include "dbg_source.h"
#include "adb_core.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define TEST_PASS          0        /**< 单项测试通过 */
#define TEST_FAIL          1        /**< 单项测试失败 */
#define TEST_STREAM_MS     1500u    /**< 等待stub输出的时长(毫秒) */
#define TEST_DRAIN_MAX     4096u    /**< 单次取走缓冲容量 */
#define TEST_CMDLINE_MAX   128u     /**< stub命令行宽字符容量 */

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static volatile uint32_t g_test_evt_opened = 0u;   /**< OPENED事件计数 */
static volatile uint32_t g_test_evt_closed = 0u;   /**< CLOSED事件计数 */
static volatile uint32_t g_test_evt_relogin = 0u;  /**< RELOGIN事件计数 */
static wchar_t g_test_cmdline[TEST_CMDLINE_MAX] = {0}; /**< stub命令行 */

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   状态回调(记录事件计数, 仅投递语义)
 * @param   event 事件码
 * @param   p_msg 消息(未使用)
 * @param   p_user 用户指针(未使用)
 * @return  无
 */
static void test_status_cb(uint32_t event, const wchar_t *p_msg, void *p_user)
{
    (void)p_msg;
    (void)p_user;
    if (event == (uint32_t)DBG_SOURCE_EVT_OPENED) {
        g_test_evt_opened++;
    } else if (event == (uint32_t)DBG_SOURCE_EVT_CLOSED) {
        g_test_evt_closed++;
    } else if (event == (uint32_t)DBG_SOURCE_EVT_RELOGIN) {
        g_test_evt_relogin++;
    } else {
        /* 其余事件不计数 */
    }
}

/**
 * @brief   测试管道流式接收与关闭时序
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_pipe_stream_and_close(void)
{
    const DBG_SOURCE_T *p_src = NULL;
    DBG_SOURCE_RX_T rx;
    ADB_CFG_T cfg;
    uint8_t buf[TEST_DRAIN_MAX];
    uint32_t got = 0u;
    uint32_t total = 0u;
    uint32_t iter = 0u;
    int32_t ret = 0;
    int32_t result = TEST_PASS;

    memset(&rx, 0, sizeof(rx));
    memset(&cfg, 0, sizeof(cfg));
    dbg_source_rx_init(&rx, NULL, (WM_APP + 1u));

    (void)lstrcpynW(&g_test_cmdline[0], L"cmd /c ping -n 2 -w 300 127.0.0.1",
                    (int)TEST_CMDLINE_MAX);
    cfg.p_rx = &rx;
    cfg.p_test_cmdline = &g_test_cmdline[0];

    p_src = dbg_source_get((uint32_t)DBG_SRC_ADB);
    if (p_src == NULL) {
        printf("[FAIL] test_pipe_stream: source null\n");
        return TEST_FAIL;
    }

    ret = p_src->pfn_open(&cfg, &test_status_cb, NULL);
    if ((ret != 0) || (p_src->pfn_is_open() == 0u)) {
        printf("[FAIL] test_pipe_stream: open failed (%d)\n", ret);
        return TEST_FAIL;
    }
    if (g_test_evt_opened == 0u) {
        printf("[FAIL] test_pipe_stream: no OPENED event\n");
        result = TEST_FAIL;
    }

    /* 写入应成功(管道可写; stub不读也不影响写返回) */
    ret = p_src->pfn_write((const uint8_t *)"dir\n", 4u);
    if (ret != 0) {
        printf("[FAIL] test_pipe_stream: write failed\n");
        result = TEST_FAIL;
    }

    Sleep(TEST_STREAM_MS);

    for (iter = 0u; iter < 8u; iter++) {
        got = dbg_source_rx_drain(&rx, &buf[0], TEST_DRAIN_MAX);
        total += got;
        if (got < TEST_DRAIN_MAX) {
            break;
        }
    }
    if (total == 0u) {
        printf("[FAIL] test_pipe_stream: no stream data\n");
        result = TEST_FAIL;
    }

    p_src->pfn_close();
    if (p_src->pfn_is_open() != 0u) {
        printf("[FAIL] test_pipe_stream: still open after close\n");
        result = TEST_FAIL;
    }
    if (g_test_evt_closed == 0u) {
        printf("[FAIL] test_pipe_stream: no CLOSED event\n");
        result = TEST_FAIL;
    }

    /* 关闭后应可重新打开一次(句柄/线程复位正确) */
    g_test_evt_opened = 0u;
    ret = p_src->pfn_open(&cfg, &test_status_cb, NULL);
    if ((ret != 0) || (p_src->pfn_is_open() == 0u)) {
        printf("[FAIL] test_pipe_stream: reopen failed\n");
        result = TEST_FAIL;
    }
    p_src->pfn_close();

    if (result == TEST_PASS) {
        printf("[INFO] stream bytes received: %u\n", (unsigned int)total);
    }

    return result;
}

/**
 * @brief   测试失败自动重登: stub进程立即退出 -> RELOGIN事件 -> 自动重连 -> 用户断开取消
 * @param   无
 * @return  0: 通过; 1: 失败
 */
static int32_t test_auto_relogin(void)
{
    const DBG_SOURCE_T *p_src = NULL;
    DBG_SOURCE_RX_T rx;
    ADB_CFG_T cfg;
    uint32_t waited = 0u;
    int32_t ret = 0;
    int32_t result = TEST_PASS;

    memset(&rx, 0, sizeof(rx));
    memset(&cfg, 0, sizeof(cfg));
    dbg_source_rx_init(&rx, NULL, (WM_APP + 1u));

    (void)lstrcpynW(&g_test_cmdline[0], L"cmd /c exit", (int)TEST_CMDLINE_MAX);
    cfg.p_rx = &rx;
    cfg.p_test_cmdline = &g_test_cmdline[0];
    cfg.auto_relogin = 1u;
    cfg.relogin_interval_s = 1u;
    g_test_evt_opened = 0u;
    g_test_evt_relogin = 0u;
    g_test_evt_closed = 0u;

    p_src = dbg_source_get((uint32_t)DBG_SRC_ADB);
    if (p_src == NULL) {
        printf("[FAIL] test_auto_relogin: source null\n");
        return TEST_FAIL;
    }

    ret = p_src->pfn_open(&cfg, &test_status_cb, NULL);
    if (ret != 0) {
        printf("[FAIL] test_auto_relogin: open failed\n");
        return TEST_FAIL;
    }

    /* 等待: stub立即退出 -> RELOGIN -> 间隔1s -> 重连成功(第二次OPENED) */
    while ((waited < 10000u) && ((g_test_evt_relogin == 0u) || (g_test_evt_opened < 2u))) {
        Sleep(100u);
        waited += 100u;
    }
    if (g_test_evt_relogin == 0u) {
        printf("[FAIL] test_auto_relogin: no RELOGIN event\n");
        result = TEST_FAIL;
    }
    if (g_test_evt_opened < 2u) {
        printf("[FAIL] test_auto_relogin: no reopen (opened=%u)\n",
               (unsigned int)g_test_evt_opened);
        result = TEST_FAIL;
    }

    /* 用户断开取消重登循环 */
    p_src->pfn_close();
    Sleep(500u);
    if (p_src->pfn_is_open() != 0u) {
        printf("[FAIL] test_auto_relogin: still open after user close\n");
        result = TEST_FAIL;
    }

    /* 取消后应可正常再次打开(状态完全复位) */
    g_test_evt_opened = 0u;
    ret = p_src->pfn_open(&cfg, &test_status_cb, NULL);
    if ((ret != 0) || (p_src->pfn_is_open() == 0u)) {
        printf("[FAIL] test_auto_relogin: reopen after cancel failed\n");
        result = TEST_FAIL;
    }
    p_src->pfn_close();

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

    failed += test_pipe_stream_and_close();
    failed += test_auto_relogin();

    if (failed == 0) {
        printf("[PASS] all adb_core tests passed\n");
        return 0;
    }
    printf("[FAIL] %d adb_core test(s) failed\n", failed);

    return 1;
}
