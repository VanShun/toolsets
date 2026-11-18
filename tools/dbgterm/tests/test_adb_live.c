/**
 * @file    test_adb_live.c
 * @author  herongshun
 * @date    2026-11-17
 * @brief   adb_core 真机验证工具(控制台, 不纳入build.bat自动运行)
 *          用法: test_adb_live <adb_path> <serial> [user] [password] [seconds]
 *          实测: 连接 -> 等提示登录 -> 自动命令 -> 原始流打印 -> 关闭
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
#define LIVE_DRAIN_MAX   4096u    /**< 单次取走缓冲容量 */
#define LIVE_CMD_MAX     256u     /**< 命令缓冲 */

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static uint32_t g_live_opened = 0u;    /**< OPENED事件计数 */
static uint32_t g_live_relogin = 0u;   /**< RELOGIN事件计数 */
static uint32_t g_live_closed = 0u;    /**< CLOSED事件计数 */

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   状态回调: 事件打印到控制台
 * @param   event 事件码
 * @param   p_msg 伴随消息
 * @param   p_user 未使用
 * @return  无
 */
static void live_status_cb(uint32_t event, const wchar_t *p_msg, void *p_user)
{
    (void)p_user;
    if (event == (uint32_t)DBG_SOURCE_EVT_OPENED) {
        g_live_opened++;
    } else if (event == (uint32_t)DBG_SOURCE_EVT_RELOGIN) {
        g_live_relogin++;
    } else if (event == (uint32_t)DBG_SOURCE_EVT_CLOSED) {
        g_live_closed++;
    } else {
        /* ERROR事件走last_error */
    }
    if (p_msg != NULL) {
        char utf8[256];
        int32_t n = WideCharToMultiByte(CP_UTF8, 0u, p_msg, -1, &utf8[0], 256, NULL, NULL);
        if (n > 0) {
            printf("[EVT] %u: %s\n", (unsigned int)event, &utf8[0]);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   入口: 实测连接/登录/流接收/关闭
 * @param   argc 参数数
 * @param   argv [1]adb路径 [2]序列号 [3]账号(可省) [4]密码(可省) [5]时长秒(可省)
 * @return  0: 正常结束
 */
int main(int argc, char *argv[])
{
    const DBG_SOURCE_T *p_src = NULL;
    DBG_SOURCE_RX_T rx;
    ADB_CFG_T cfg;
    uint8_t buf[LIVE_DRAIN_MAX];
    uint32_t seconds = 20u;
    uint32_t elapsed = 0u;
    int32_t ret = 0;

    if (argc < 3) {
        printf("usage: test_adb_live <adb_path> <serial> [user] [password] [seconds]\n");
        return 1;
    }
    if (argc >= 6) {
        seconds = (uint32_t)atoi(argv[5]);
        if (seconds == 0u) {
            seconds = 20u;
        }
    }

    memset(&rx, 0, sizeof(rx));
    memset(&cfg, 0, sizeof(cfg));
    dbg_source_rx_init(&rx, NULL, (WM_APP + 1u));

    MultiByteToWideChar(CP_UTF8, 0u, argv[1], -1, &cfg.adb_path[0], ADB_CFG_PATH_WCHAR);
    MultiByteToWideChar(CP_UTF8, 0u, argv[2], -1, &cfg.device_serial[0], ADB_CFG_SERIAL_WCHAR);
    if (argc >= 4) {
        MultiByteToWideChar(CP_UTF8, 0u, argv[3], -1, &cfg.login_user[0], ADB_CFG_LOGIN_WCHAR);
    }
    if (argc >= 5) {
        MultiByteToWideChar(CP_UTF8, 0u, argv[4], -1, &cfg.login_password[0], ADB_CFG_LOGIN_WCHAR);
    }
    cfg.login_pwd_delay_s = 3u;
    cfg.auto_relogin = 1u;
    cfg.relogin_interval_s = 5u;
    cfg.p_rx = &rx;
    MultiByteToWideChar(CP_UTF8, 0u, "echo __LIVE_LOGIN_OK__", -1,
                        &cfg.auto_cmd[0][0], ADB_CFG_CMD_WCHAR);
    MultiByteToWideChar(CP_UTF8, 0u, "id", -1, &cfg.auto_cmd[1][0], ADB_CFG_CMD_WCHAR);
    cfg.auto_cmd_count = 2u;
    cfg.auto_cmd_interval_ms = 500u;

    p_src = dbg_source_get((uint32_t)DBG_SRC_ADB);
    if (p_src == NULL) {
        printf("[FAIL] source null\n");
        return 1;
    }

    printf("[LIVE] opening %s -s %s ...\n", argv[1], argv[2]);
    ret = p_src->pfn_open(&cfg, &live_status_cb, NULL);
    printf("[LIVE] open ret=%d is_open=%u\n", ret, (unsigned int)p_src->pfn_is_open());

    while (elapsed < seconds) {
        uint32_t got = dbg_source_rx_drain(&rx, &buf[0], LIVE_DRAIN_MAX);

        if (got > 0u) {
            fwrite(&buf[0], 1u, got, stdout);
            fflush(stdout);
        }
        Sleep(200u);
        elapsed += (200u + 999u) / 1000u;
        if (elapsed % 5u == 0u) {
            printf("\n[LIVE] %us: opened=%u relogin=%u closed=%u\n",
                   (unsigned int)elapsed, (unsigned int)g_live_opened,
                   (unsigned int)g_live_relogin, (unsigned int)g_live_closed);
        }
    }

    printf("[LIVE] closing...\n");
    p_src->pfn_close();
    printf("[LIVE] done: opened=%u relogin=%u closed=%u\n",
           (unsigned int)g_live_opened, (unsigned int)g_live_relogin,
           (unsigned int)g_live_closed);

    return 0;
}
