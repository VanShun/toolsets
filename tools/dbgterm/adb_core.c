/**
 * @file    adb_core.c
 * @author  herongshun
 * @date    2026-11-06
 * @brief   ADB 数据源实现
 *          以管道子进程方式运行"adb.exe [-s 序列号] shell":
 *          stdout/stderr 独立读线程灌入公共环形缓冲, stdin 承担发送与自动命令;
 *          关闭时序: Ctrl+C -> exit -> 兜底 TerminateProcess, 保证设备侧 logcat 收尾
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <process.h>
#include "dbg_source.h"
#include "adb_core.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define ADB_STATE_IDLE          0u     /**< 空闲(未连接) */
#define ADB_STATE_CONNECTED     1u     /**< 已连接 */
#define ADB_STATE_RELOGIN       2u     /**< 异常断开, 自动重登等待中 */
#define ADB_READ_CHUNK          4096u  /**< 管道单次读取字节量 */
#define ADB_CMDLINE_MAX         512u   /**< 命令行缓冲宽字符容量 */
#define ADB_ERR_MSG_MAX         256u   /**< 错误消息宽字符容量 */
#define ADB_ENUM_OUT_MAX        4096u  /**< devices 输出缓冲字节容量 */
#define ADB_ENUM_WAIT_MS        8000u  /**< devices 进程收尸超时 */
#define ADB_CLOSE_PRE_WAIT_MS   100u   /**< 关闭: 优雅退出等待时长 */
#define ADB_CLOSE_KILL_WAIT_MS  2000u  /**< 关闭: 兜底杀进程等待时长 */
#define ADB_THREAD_WAIT_MS      2000u  /**< 关闭: 线程收尸等待时长 */
#define ADB_EXIT_CTRL           0x03u  /**< Ctrl+C 控制字节 */
#define ADB_SYS_PREFIX          "[SYS] "  /**< stderr 行前缀(ASCII, 两编码同形) */
#define ADB_AUTO_INTERVAL_DEF   200u   /**< 自动命令默认间隔(毫秒) */
#define ADB_LOGIN_WAIT_MS       300u   /**< 登录步骤就绪等待(毫秒) */
#define ADB_LOGIN_READY_MS      3000u  /**< 等待"login:"提示超时(毫秒, 超时兜底直发) */
#define ADB_LOGIN_PWD_DEF_S     3u     /**< 账号->密码默认延迟(秒, 设备shell密码提示慢) */
#define ADB_LOGIN_PWD_MIN_MS    500u   /**< 账号->密码延迟下限(毫秒) */
#define ADB_LOGIN_PWD_WAIT_MIN_MS 10000u  /**< 等待"assword:"提示的超时下限(毫秒) */
#define ADB_RELOGIN_TICK_MS     200u   /**< 重登等待检查步进(毫秒) */
#define ADB_RELOGIN_MIN_S       1u     /**< 重登间隔下限(秒) */

/* -------------------------------------------------------------------------- */
/*  类型定义                                                                    */
/* -------------------------------------------------------------------------- */

/** 读线程参数 */
typedef struct ADB_THREAD_ARG_TAG
{
    uint8_t is_sys;    /**< 1=stderr线程(带[SYS]前缀); 0=stdout线程 */
} ADB_THREAD_ARG_T;

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static volatile uint32_t   g_adb_state = ADB_STATE_IDLE;         /**< 连接状态 */
static uint8_t             g_adb_cs_ready = 0u;                  /**< 临界区初始化标志 */
static CRITICAL_SECTION    g_adb_cs;                             /**< 句柄/写操作互斥 */
static HANDLE              g_adb_proc = NULL;                    /**< 子进程句柄 */
static HANDLE              g_adb_stdin_wr = NULL;                /**< 子进程stdin写端 */
static HANDLE              g_adb_stdout_rd = NULL;               /**< 子进程stdout读端 */
static HANDLE              g_adb_stderr_rd = NULL;               /**< 子进程stderr读端 */
static HANDLE              g_adb_hthread_out = NULL;             /**< stdout读线程句柄 */
static HANDLE              g_adb_hthread_err = NULL;             /**< stderr读线程句柄 */
static DBG_SOURCE_RX_T    *g_adb_p_rx = NULL;                    /**< 接收环形缓冲(调用方持有) */
static DBG_SOURCE_STATUS_FN g_adb_pfn_status = NULL;             /**< 状态回调 */
static void                *g_adb_p_user = NULL;                 /**< 状态回调用户指针 */
static wchar_t             g_adb_last_err[ADB_ERR_MSG_MAX] = {0};/**< 最近错误消息(CS保护) */

static const ADB_THREAD_ARG_T g_adb_arg_out = {0u};              /**< stdout线程参数 */
static const ADB_THREAD_ARG_T g_adb_arg_err = {1u};              /**< stderr线程参数 */

static volatile uint8_t g_adb_user_close = 0u;                   /**< 用户主动关闭标志(重登线程据此退出) */
static HANDLE           g_adb_hthread_relogin = NULL;            /**< 自动重登线程句柄 */
static ADB_CFG_T        g_adb_cfg_saved;                         /**< 连接配置拷贝(重登复用, 含回调与缓冲指针) */

/* 内部私有函数前向声明 */
static int32_t    adb_spawn_locked(void);
static void       adb_send_login_and_autocmds(const ADB_CFG_T *p_cfg);
static uint32_t   __stdcall adb_relogin_thread(void *p_arg);

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   确保模块临界区已初始化(open入口单线程调用)
 * @param   无
 * @return  无
 */
static void adb_ensure_cs(void)
{
    if (g_adb_cs_ready == 0u) {
        InitializeCriticalSection(&g_adb_cs);
        g_adb_cs_ready = 1u;
    }
}

/**
 * @brief   记录错误消息并投递状态回调(EVT_ERROR)
 * @param   p_msg 错误消息(宽字符)
 * @return  无
 */
static void adb_report_error(const wchar_t *p_msg)
{
    if (p_msg != NULL) {
        EnterCriticalSection(&g_adb_cs);
        lstrcpynW(g_adb_last_err, p_msg, (int)ADB_ERR_MSG_MAX);
        LeaveCriticalSection(&g_adb_cs);
    }
    if (g_adb_pfn_status != NULL) {
        g_adb_pfn_status((uint32_t)DBG_SOURCE_EVT_ERROR, p_msg, g_adb_p_user);
    }
}

/**
 * @brief   投递状态回调
 * @param   event 事件(DBG_SOURCE_EVT_E)
 * @param   p_msg 伴随消息(可为NULL)
 * @return  无
 */
static void adb_notify_status(uint32_t event, const wchar_t *p_msg)
{
    if (g_adb_pfn_status != NULL) {
        g_adb_pfn_status(event, p_msg, g_adb_p_user);
    }
}

/**
 * @brief   向子进程stdin写入原始字节(须持 g_adb_cs)
 * @param   p_data 数据
 * @param   len    长度(字节)
 * @return  0: 成功; -1: 失败(未连接/写失败)
 */
static int32_t adb_write_stdin_locked(const uint8_t *p_data, uint32_t len)
{
    DWORD written = 0u;

    if ((g_adb_stdin_wr == NULL) || (g_adb_state != ADB_STATE_CONNECTED)) {
        return -1;
    }
    if (WriteFile(g_adb_stdin_wr, p_data, (DWORD)len, &written, NULL) == 0) {
        return -1;
    }

    return ((written == (DWORD)len) ? 0 : -1);
}

/**
 * @brief   将宽字符串转为UTF-8并写入stdin(自动命令下发用)
 * @param   p_cmd 命令(宽字符)
 * @return  0: 成功; -1: 失败
 */
static int32_t adb_send_command(const wchar_t *p_cmd)
{
    uint8_t  utf8[ADB_CFG_CMD_WCHAR * 2u] = {0};
    uint32_t cmd_len = 0u;
    uint32_t pos = 0u;
    int32_t  conv_len = 0;
    int32_t  ret = 0;

    cmd_len = (uint32_t)lstrlenW(p_cmd);
    if ((cmd_len == 0u) || (cmd_len >= ADB_CFG_CMD_WCHAR)) {
        return -1;
    }
    conv_len = WideCharToMultiByte(CP_UTF8, 0u, p_cmd, (int32_t)cmd_len,
                                   (char *)&utf8[0], (int32_t)(sizeof(utf8) - 2u), NULL, NULL);
    if (conv_len <= 0) {
        return -1;
    }
    pos = (uint32_t)conv_len;
    utf8[pos] = (uint8_t)'\n';   /* 仅LF: 管道模式下CR会进入登录字符串比对导致失败 */
    pos += 1u;

    EnterCriticalSection(&g_adb_cs);
    ret = adb_write_stdin_locked(&utf8[0], pos);
    LeaveCriticalSection(&g_adb_cs);

    return ret;
}

/**
 * @brief   管道读线程体: 阻塞读至EOF, 数据灌入环形缓冲
 * @param   p_arg 线程参数(ADB_THREAD_ARG_T)
 * @return  线程退出码(0)
 */
static uint32_t __stdcall adb_read_thread(void *p_arg)
{
    const ADB_THREAD_ARG_T *p_cfg = (const ADB_THREAD_ARG_T *)p_arg;
    HANDLE  h_rd = NULL;
    uint8_t buf[ADB_READ_CHUNK];
    DWORD   got = 0u;
    uint8_t at_bol = 1u;   /* 行首标志: [SYS]前缀仅加在行首, 避免按块碎片化 */

    memset(&buf[0], 0, sizeof(buf));
    h_rd = (p_cfg->is_sys != 0u) ? g_adb_stderr_rd : g_adb_stdout_rd;

    for (;;) {
        if (ReadFile(h_rd, &buf[0], (DWORD)ADB_READ_CHUNK, &got, NULL) == 0) {
            break;
        }
        if (got == 0u) {
            break;
        }
        if ((p_cfg->is_sys != 0u) && (at_bol != 0u)) {
            dbg_source_rx_push(g_adb_p_rx, (const uint8_t *)ADB_SYS_PREFIX,
                               (uint32_t)(sizeof(ADB_SYS_PREFIX) - 1u), DBG_SOURCE_NOTIFY_NOW);
        }
        dbg_source_rx_push(g_adb_p_rx, &buf[0], (uint32_t)got, DBG_SOURCE_NOTIFY_NOW);

        /* 下一块是否处于行首: 本块末字节为换行 */
        at_bol = (uint8_t)((buf[got - 1u] == (uint8_t)'\n') ? 1u : 0u);
    }

    /* 仅stdout线程负责断开路径处理: 置RELOGIN态由看门狗重连, 或普通断开通知 */
    if (p_cfg->is_sys == 0u) {
        uint8_t notify_closed = 0u;
        uint8_t notify_relogin = 0u;

        EnterCriticalSection(&g_adb_cs);
        if ((g_adb_state == ADB_STATE_CONNECTED) && (g_adb_user_close == 0u)) {
            if ((g_adb_cfg_saved.auto_relogin != 0u) && (g_adb_hthread_relogin != NULL)) {
                g_adb_state = ADB_STATE_RELOGIN;   /* 看门狗线程将接管重连 */
                notify_relogin = 1u;
            } else {
                notify_closed = 1u;   /* 未启用重登或看门狗缺失: 普通断开 */
            }
        }
        LeaveCriticalSection(&g_adb_cs);

        if (notify_relogin != 0u) {
            adb_notify_status((uint32_t)DBG_SOURCE_EVT_RELOGIN, L"连接断开, 自动重登已调度");
        } else if (notify_closed != 0u) {
            adb_notify_status((uint32_t)DBG_SOURCE_EVT_CLOSED, L"连接已断开");
        } else {
            /* 用户主动关闭路径: 断开通知由 adb_close 统一发出 */
        }
    }

    return 0u;
}

/**
 * @brief   关闭子进程与全部句柄并复位状态(须持 g_adb_cs 或 open/close单线程入口)
 * @param   无
 * @return  无
 */
static void adb_cleanup(void)
{
    if (g_adb_stdin_wr != NULL) {
        (void)CloseHandle(g_adb_stdin_wr);
        g_adb_stdin_wr = NULL;
    }
    if (g_adb_proc != NULL) {
        if (WaitForSingleObject(g_adb_proc, ADB_CLOSE_PRE_WAIT_MS) == WAIT_TIMEOUT) {
            (void)TerminateProcess(g_adb_proc, (UINT)1);
            (void)WaitForSingleObject(g_adb_proc, ADB_CLOSE_KILL_WAIT_MS);
        }
        (void)CloseHandle(g_adb_proc);
        g_adb_proc = NULL;
    }
    /* 子进程已终止, 管道写端关闭 -> 读线程ReadFile返回EOF自然退出 */
    if (g_adb_hthread_out != NULL) {
        (void)WaitForSingleObject(g_adb_hthread_out, ADB_THREAD_WAIT_MS);
        (void)CloseHandle(g_adb_hthread_out);
        g_adb_hthread_out = NULL;
    }
    if (g_adb_hthread_err != NULL) {
        (void)WaitForSingleObject(g_adb_hthread_err, ADB_THREAD_WAIT_MS);
        (void)CloseHandle(g_adb_hthread_err);
        g_adb_hthread_err = NULL;
    }
    if (g_adb_stdout_rd != NULL) {
        (void)CloseHandle(g_adb_stdout_rd);
        g_adb_stdout_rd = NULL;
    }
    if (g_adb_stderr_rd != NULL) {
        (void)CloseHandle(g_adb_stderr_rd);
        g_adb_stderr_rd = NULL;
    }
    g_adb_p_rx = NULL;
    g_adb_state = ADB_STATE_IDLE;
}

/**
 * @brief   拼接"adb [-s 序列号] shell"命令行
 * @param   p_cfg  连接配置
 * @param   p_out  输出缓冲
 * @param   max    输出容量(宽字符)
 * @return  0: 成功; -1: 失败
 */
static int32_t adb_build_cmdline(const ADB_CFG_T *p_cfg, wchar_t *p_out, uint32_t max)
{
    uint32_t len = 0u;

    p_out[0] = L'\0';
    (void)lstrcpynW(p_out, L"\"", (int)max);
    (void)lstrcatW(p_out, p_cfg->adb_path);
    (void)lstrcatW(p_out, L"\"");
    if (p_cfg->device_serial[0] != L'\0') {
        (void)lstrcatW(p_out, L" -s ");
        (void)lstrcatW(p_out, p_cfg->device_serial);
    }
    (void)lstrcatW(p_out, L" shell");

    len = (uint32_t)lstrlenW(p_out);
    if ((len == 0u) || (len >= max)) {
        return -1;
    }

    return 0;
}

/**
 * @brief   创建子进程与读线程(须持 g_adb_cs; 失败时自行清理并保持IDLE态)
 *          使用 g_adb_cfg_saved 中的路径/序列号/测试命令行
 * @param   无
 * @return  0: 成功; -1: 失败(错误消息已写入 g_adb_last_err)
 */
static int32_t adb_spawn_locked(void)
{
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    HANDLE h_out_wr = NULL;
    HANDLE h_in_rd = NULL;
    HANDLE h_err_wr = NULL;
    wchar_t cmdline[ADB_CMDLINE_MAX];
    uintptr_t thread_ret = 0u;
    int32_t ret = -1;

    memset(&sa, 0, sizeof(sa));
    sa.nLength = (DWORD)sizeof(sa);
    sa.bInheritHandle = TRUE;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));

    do {
        if (CreatePipe(&g_adb_stdout_rd, &h_out_wr, &sa, 0) == 0) {
            lstrcpynW(g_adb_last_err, L"创建stdout管道失败", (int)ADB_ERR_MSG_MAX);
            break;
        }
        if (CreatePipe(&h_in_rd, &g_adb_stdin_wr, &sa, 0) == 0) {
            lstrcpynW(g_adb_last_err, L"创建stdin管道失败", (int)ADB_ERR_MSG_MAX);
            break;
        }
        if (CreatePipe(&g_adb_stderr_rd, &h_err_wr, &sa, 0) == 0) {
            lstrcpynW(g_adb_last_err, L"创建stderr管道失败", (int)ADB_ERR_MSG_MAX);
            break;
        }
        /* 本端句柄不可被子进程继承 */
        (void)SetHandleInformation(g_adb_stdout_rd, HANDLE_FLAG_INHERIT, 0u);
        (void)SetHandleInformation(g_adb_stdin_wr, HANDLE_FLAG_INHERIT, 0u);
        (void)SetHandleInformation(g_adb_stderr_rd, HANDLE_FLAG_INHERIT, 0u);

        si.cb = (DWORD)sizeof(si);
        si.dwFlags = (DWORD)STARTF_USESTDHANDLES;
        si.hStdInput = h_in_rd;
        si.hStdOutput = h_out_wr;
        si.hStdError = h_err_wr;

        if (g_adb_cfg_saved.p_test_cmdline != NULL) {
            (void)lstrcpynW(&cmdline[0], g_adb_cfg_saved.p_test_cmdline, (int)ADB_CMDLINE_MAX);
        } else {
            if (adb_build_cmdline(&g_adb_cfg_saved, &cmdline[0], ADB_CMDLINE_MAX) != 0) {
                lstrcpynW(g_adb_last_err, L"命令行构造失败", (int)ADB_ERR_MSG_MAX);
                break;
            }
        }

        if (CreateProcessW(NULL, &cmdline[0], NULL, NULL, TRUE,
                           (DWORD)CREATE_NO_WINDOW, NULL, NULL, &si, &pi) == 0) {
            lstrcpynW(g_adb_last_err, L"adb 进程创建失败(检查路径)", (int)ADB_ERR_MSG_MAX);
            break;
        }
        (void)CloseHandle(pi.hThread);
        g_adb_proc = pi.hProcess;

        /* 父进程侧关闭子进程端句柄 */
        (void)CloseHandle(h_out_wr);
        h_out_wr = NULL;
        (void)CloseHandle(h_in_rd);
        h_in_rd = NULL;
        (void)CloseHandle(h_err_wr);
        h_err_wr = NULL;

        g_adb_state = ADB_STATE_CONNECTED;

        thread_ret = _beginthreadex(NULL, 0u, &adb_read_thread,
                                    (void *)&g_adb_arg_out, 0u, NULL);
        if (thread_ret == 0u) {
            lstrcpynW(g_adb_last_err, L"读线程创建失败", (int)ADB_ERR_MSG_MAX);
            break;
        }
        g_adb_hthread_out = (HANDLE)thread_ret;

        thread_ret = _beginthreadex(NULL, 0u, &adb_read_thread,
                                    (void *)&g_adb_arg_err, 0u, NULL);
        if (thread_ret == 0u) {
            lstrcpynW(g_adb_last_err, L"错误流线程创建失败", (int)ADB_ERR_MSG_MAX);
            break;
        }
        g_adb_hthread_err = (HANDLE)thread_ret;

        ret = 0;
    } while (0);

    if (h_out_wr != NULL) {
        (void)CloseHandle(h_out_wr);
    }
    if (h_in_rd != NULL) {
        (void)CloseHandle(h_in_rd);
    }
    if (h_err_wr != NULL) {
        (void)CloseHandle(h_err_wr);
    }
    if (ret != 0) {
        adb_cleanup();
    }

    return ret;
}

/**
 * @brief   下发登录序列与自动命令(不持CS; adb_send_command内部自行加锁)
 *          形态对标getsscom: shell就绪后先账号/密码登录, 再执行自动命令;
 *          发送时机: 等"login:"提示(3s兜底)发账号 -> 等"assword:"提示
 *          (2x登录延迟, 至少10s, 兜底)发密码 -> 执行自动命令
 * @param   p_cfg 连接配置
 * @return  无
 */
static void adb_send_login_and_autocmds(const ADB_CFG_T *p_cfg)
{
    uint32_t interval = (p_cfg->auto_cmd_interval_ms != 0u) ? p_cfg->auto_cmd_interval_ms
                                                            : ADB_AUTO_INTERVAL_DEF;
    uint32_t pwd_delay_ms = ((p_cfg->login_pwd_delay_s != 0u) ? p_cfg->login_pwd_delay_s
                                                              : ADB_LOGIN_PWD_DEF_S) * 1000u;
    uint32_t index = 0u;

    if (pwd_delay_ms < ADB_LOGIN_PWD_MIN_MS) {
        pwd_delay_ms = ADB_LOGIN_PWD_MIN_MS;
    }
    if (pwd_delay_ms < ADB_LOGIN_PWD_WAIT_MIN_MS) {
        pwd_delay_ms = ADB_LOGIN_PWD_WAIT_MIN_MS;   /* 等"assword:"提示的超时 */
    }

    if (p_cfg->login_user[0] != L'\0') {
        /* 等设备侧"login:"提示(超时兜底直发, 兼容无提示设备) */
        (void)dbg_source_rx_wait_mark(g_adb_p_rx, "ogin:", ADB_LOGIN_READY_MS);
        if (adb_send_command(&p_cfg->login_user[0]) != 0) {
            return;
        }
        if (p_cfg->login_password[0] != L'\0') {
            /* 等设备侧"assword:"提示: 出现即发, 超时兜底直发 */
            (void)dbg_source_rx_wait_mark(g_adb_p_rx, "assword", pwd_delay_ms);
            if (adb_send_command(&p_cfg->login_password[0]) != 0) {
                return;
            }
        }
        Sleep(ADB_LOGIN_WAIT_MS);
    }

    for (index = 0u; index < p_cfg->auto_cmd_count; index++) {
        if (p_cfg->auto_cmd[index][0] == L'\0') {
            continue;
        }
        Sleep(interval);
        if (adb_send_command(&p_cfg->auto_cmd[index][0]) != 0) {
            break;
        }
    }
}

/**
 * @brief   自动重登看门狗线程(会话期常驻)
 *          监控状态机: CONNECTED=连接正常仅巡检 / RELOGIN或IDLE(拉起失败)=按间隔重连;
 *          重连 = 清理残余句柄 -> 重新拉起 -> 登录序列与自动命令 -> 存活才通知OPENED;
 *          用户断开(g_adb_user_close=1)时退出; 登录期间再次断线由读线程置RELOGIN,
 *          看门狗下一轮巡检接管(无槽位竞争, 修复重登循环卡死问题)
 * @param   p_arg 未使用
 * @return  线程退出码(0)
 */
static uint32_t __stdcall adb_relogin_thread(void *p_arg)
{
    HANDLE h_self = g_adb_hthread_relogin;
    uint32_t target_ms = 0u;

    (void)p_arg;
    target_ms = g_adb_cfg_saved.relogin_interval_s * 1000u;
    if (target_ms < (ADB_RELOGIN_MIN_S * 1000u)) {
        target_ms = ADB_RELOGIN_MIN_S * 1000u;
    }

    while (g_adb_user_close == 0u) {
        uint32_t waited = 0u;
        uint8_t need_reconnect = 0u;

        Sleep(ADB_RELOGIN_TICK_MS);
        EnterCriticalSection(&g_adb_cs);
        if (g_adb_user_close != 0u) {
            LeaveCriticalSection(&g_adb_cs);
            goto done;
        }
        need_reconnect = (uint8_t)((g_adb_state != ADB_STATE_CONNECTED) ? 1u : 0u);
        LeaveCriticalSection(&g_adb_cs);
        if (need_reconnect == 0u) {
            continue;   /* 连接正常: 继续巡检 */
        }

        /* 按间隔等待(期间用户可随时断开取消) */
        while (waited < target_ms) {
            if (g_adb_user_close != 0u) {
                goto done;
            }
            Sleep(ADB_RELOGIN_TICK_MS);
            waited += ADB_RELOGIN_TICK_MS;
        }

        EnterCriticalSection(&g_adb_cs);
        if ((g_adb_user_close != 0u) || (g_adb_state == ADB_STATE_CONNECTED)) {
            LeaveCriticalSection(&g_adb_cs);
            continue;   /* 等待期间被恢复/取消: 重新巡检 */
        }
        (void)adb_cleanup();   /* 清理残余句柄(状态回IDLE) */
        if (adb_spawn_locked() == 0) {
            uint8_t alive = 0u;

            LeaveCriticalSection(&g_adb_cs);
            adb_send_login_and_autocmds(&g_adb_cfg_saved);
            EnterCriticalSection(&g_adb_cs);
            alive = (uint8_t)((g_adb_state == ADB_STATE_CONNECTED) ? 1u : 0u);
            LeaveCriticalSection(&g_adb_cs);
            if (alive != 0u) {
                adb_notify_status((uint32_t)DBG_SOURCE_EVT_OPENED, L"自动重登成功");
            }
            /* 登录期间又断线: 读线程已置RELOGIN, 继续巡检接管 */
        } else {
            LeaveCriticalSection(&g_adb_cs);
            adb_notify_status((uint32_t)DBG_SOURCE_EVT_ERROR, L"重登失败, 间隔后将再次尝试");
        }
    }

done:
    EnterCriticalSection(&g_adb_cs);
    if (g_adb_hthread_relogin == h_self) {
        g_adb_hthread_relogin = NULL;
        (void)CloseHandle(h_self);
    }
    LeaveCriticalSection(&g_adb_cs);
    return 0u;
}

/**
 * @brief   接口: 建立连接(子进程+读线程+登录序列+自动命令)
 * @param   p_cfg      ADB_CFG_T 配置(内部保存拷贝供重登复用)
 * @param   pfn_status 状态回调(线程上下文调用, 须线程安全)
 * @param   p_user     回调用户指针
 * @return  0: 成功; -1: 失败
 */
static int32_t adb_open(const void *p_cfg, DBG_SOURCE_STATUS_FN pfn_status, void *p_user)
{
    const ADB_CFG_T *p_adb = (const ADB_CFG_T *)p_cfg;
    uint8_t relogin_wait = 0u;
    int32_t ret = -1;

    if ((p_adb == NULL) || (p_adb->p_rx == NULL) || (pfn_status == NULL)) {
        return -1;
    }
    if (p_adb->auto_cmd_count > ADB_CFG_MAX_AUTO_CMD) {
        return -1;
    }

    adb_ensure_cs();
    EnterCriticalSection(&g_adb_cs);
    if (g_adb_state == ADB_STATE_RELOGIN) {
        relogin_wait = 1u;
    }
    if (g_adb_state != ADB_STATE_IDLE) {
        LeaveCriticalSection(&g_adb_cs);
        adb_report_error((relogin_wait != 0u) ? L"自动重登进行中, 请先断开" : L"已有连接存在");
        return -1;
    }
    /* 回收上一轮残留的重登线程句柄(已退出未关闭的情形) */
    if (g_adb_hthread_relogin != NULL) {
        (void)WaitForSingleObject(g_adb_hthread_relogin, ADB_THREAD_WAIT_MS);
        (void)CloseHandle(g_adb_hthread_relogin);
        g_adb_hthread_relogin = NULL;
    }
    g_adb_cfg_saved = *p_adb;
    g_adb_p_rx = p_adb->p_rx;
    g_adb_pfn_status = pfn_status;
    g_adb_p_user = p_user;
    g_adb_last_err[0] = L'\0';
    g_adb_user_close = 0u;
    ret = adb_spawn_locked();
    LeaveCriticalSection(&g_adb_cs);

    if (ret != 0) {
        adb_report_error(&g_adb_last_err[0]);
        return -1;
    }

    /* 自动重登会话: 登录序列前启动常驻看门狗(登录期间断线亦被覆盖);
       看门狗缺失时读线程按普通断开处理(降级) */
    if (g_adb_cfg_saved.auto_relogin != 0u) {
        uintptr_t t = _beginthreadex(NULL, 0u, &adb_relogin_thread, NULL, 0u, NULL);

        if (t != 0u) {
            g_adb_hthread_relogin = (HANDLE)t;
        }
    }

    adb_send_login_and_autocmds(&g_adb_cfg_saved);

    /* 进程在登录期间秒退(如设备offline): 状态已转RELOGIN, 不误报已连接 */
    EnterCriticalSection(&g_adb_cs);
    if (g_adb_state != ADB_STATE_CONNECTED) {
        LeaveCriticalSection(&g_adb_cs);
        return 0;
    }
    LeaveCriticalSection(&g_adb_cs);
    adb_notify_status((uint32_t)DBG_SOURCE_EVT_OPENED, L"已连接 adb shell");

    return 0;
}

/**
 * @brief   接口: 断开连接(用户主动; 取消进行中的自动重登)
 *           Ctrl+C -> exit -> 兜底杀进程 -> 线程收尸
 * @param   无
 * @return  无
 */
static void adb_close(void)
{
    uint8_t notify = 0u;

    if (g_adb_cs_ready == 0u) {
        return;
    }

    g_adb_user_close = 1u;
    EnterCriticalSection(&g_adb_cs);
    if (g_adb_state == ADB_STATE_CONNECTED) {
        /* 优雅收尾: 中断设备侧持续输出(如logcat)并退出shell */
        if (g_adb_stdin_wr != NULL) {
            const uint8_t ctrl = (uint8_t)ADB_EXIT_CTRL;
            (void)adb_write_stdin_locked(&ctrl, 1u);
            Sleep(ADB_CLOSE_PRE_WAIT_MS);
            (void)adb_write_stdin_locked((const uint8_t *)"exit\r\n", 6u);
        }
        notify = 1u;
    } else if (g_adb_state == ADB_STATE_RELOGIN) {
        notify = 1u;   /* 取消自动重登 */
    } else if (g_adb_hthread_relogin == NULL) {
        LeaveCriticalSection(&g_adb_cs);
        return;
    } else {
        /* IDLE + 残留重登线程: 仅回收 */
    }
    adb_cleanup();
    LeaveCriticalSection(&g_adb_cs);

    /* 重登线程观察到user_close后自行退出并回收句柄, 此处兜底等待回收 */
    if (g_adb_hthread_relogin != NULL) {
        (void)WaitForSingleObject(g_adb_hthread_relogin, ADB_THREAD_WAIT_MS);
        EnterCriticalSection(&g_adb_cs);
        if (g_adb_hthread_relogin != NULL) {
            (void)CloseHandle(g_adb_hthread_relogin);
            g_adb_hthread_relogin = NULL;
        }
        LeaveCriticalSection(&g_adb_cs);
    }

    if (notify != 0u) {
        adb_notify_status((uint32_t)DBG_SOURCE_EVT_CLOSED, L"已断开");
    }
}

/**
 * @brief   接口: 发送数据到子进程stdin
 * @param   p_data 数据
 * @param   len    长度(字节)
 * @return  0: 成功; -1: 失败
 */
static int32_t adb_write(const uint8_t *p_data, uint32_t len)
{
    int32_t ret = 0;

    if ((p_data == NULL) || (len == 0u)) {
        return -1;
    }

    EnterCriticalSection(&g_adb_cs);
    ret = adb_write_stdin_locked(p_data, len);
    LeaveCriticalSection(&g_adb_cs);

    return ret;
}

/**
 * @brief   接口: 连接状态查询(重登等待期间视为占用)
 * @param   无
 * @return  1: 已连接或重登等待中; 0: 空闲
 */
static uint8_t adb_is_open(void)
{
    return ((g_adb_state != ADB_STATE_IDLE) ? 1u : 0u);
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

const DBG_SOURCE_T *adb_core_get_source(void)
{
    static const DBG_SOURCE_T g_adb_source =
    {
        adb_open,
        adb_close,
        adb_write,
        adb_is_open,
    };

    return &g_adb_source;
}

void adb_core_get_last_error(wchar_t *p_out, uint32_t max)
{
    if ((p_out == NULL) || (max == 0u)) {
        return;
    }
    p_out[0] = L'\0';
    if (g_adb_cs_ready == 0u) {
        return;
    }
    EnterCriticalSection(&g_adb_cs);
    (void)lstrcpynW(p_out, g_adb_last_err, (int)max);
    LeaveCriticalSection(&g_adb_cs);
}

uint32_t adb_core_enum_devices(const wchar_t *p_adb_path,
                               ADB_DEVICE_T *p_devs, uint32_t max_count)
{
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    HANDLE h_rd = NULL;
    HANDLE h_wr = NULL;
    wchar_t cmdline[ADB_CMDLINE_MAX];
    uint8_t out[ADB_ENUM_OUT_MAX];
    DWORD got = 0u;
    uint32_t total = 0u;
    uint32_t count = 0u;
    uint32_t line_start = 0u;
    uint32_t index = 0u;

    if ((p_adb_path == NULL) || (p_adb_path[0] == L'\0') ||
        (p_devs == NULL) || (max_count == 0u)) {
        return 0u;
    }

    memset(&sa, 0, sizeof(sa));
    sa.nLength = (DWORD)sizeof(sa);
    sa.bInheritHandle = TRUE;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    memset(&out[0], 0, sizeof(out));

    (void)lstrcpynW(&cmdline[0], L"\"", (int)ADB_CMDLINE_MAX);
    (void)lstrcatW(&cmdline[0], p_adb_path);
    (void)lstrcatW(&cmdline[0], L"\" devices");

    if (CreatePipe(&h_rd, &h_wr, &sa, 0) == 0) {
        return 0u;
    }
    (void)SetHandleInformation(h_rd, HANDLE_FLAG_INHERIT, 0u);

    si.cb = (DWORD)sizeof(si);
    si.dwFlags = (DWORD)STARTF_USESTDHANDLES;
    si.hStdInput = NULL;
    si.hStdOutput = h_wr;
    si.hStdError = h_wr;

    if (CreateProcessW(NULL, &cmdline[0], NULL, NULL, TRUE,
                       (DWORD)CREATE_NO_WINDOW, NULL, NULL, &si, &pi) == 0) {
        (void)CloseHandle(h_rd);
        (void)CloseHandle(h_wr);
        return 0u;
    }
    (void)CloseHandle(pi.hThread);
    (void)CloseHandle(h_wr);

    /* 同步读取至EOF(adb devices 必然终止; 超时兜底强杀) */
    while ((total + ADB_READ_CHUNK) <= ADB_ENUM_OUT_MAX) {
        if (ReadFile(h_rd, &out[total], (DWORD)ADB_READ_CHUNK, &got, NULL) == 0) {
            break;
        }
        if (got == 0u) {
            break;
        }
        total += (uint32_t)got;
    }
    if (WaitForSingleObject(pi.hProcess, ADB_ENUM_WAIT_MS) == WAIT_TIMEOUT) {
        (void)TerminateProcess(pi.hProcess, (UINT)1);
        (void)WaitForSingleObject(pi.hProcess, ADB_CLOSE_KILL_WAIT_MS);
    }
    (void)CloseHandle(pi.hProcess);
    (void)CloseHandle(h_rd);

    /* 逐行解析: 跳过表头/daemon行, 序列号\t状态 */
    while ((line_start < total) && (count < max_count)) {
        uint32_t line_end = line_start;
        uint32_t tab_pos = 0xFFFFFFFFu;
        uint32_t serial_len = 0u;
        int32_t conv_len = 0;

        while ((line_end < total) && (out[line_end] != (uint8_t)'\n')) {
            if (out[line_end] == (uint8_t)'\t') {
                tab_pos = line_end;
            }
            line_end++;
        }
        if (line_end >= total) {
            break;
        }
        if ((line_end > 0u) && (out[line_end - 1u] == (uint8_t)'\r')) {
            line_end--;   /* 剔除CRLF行尾的CR, 避免状态字段带\r */
        }
        out[line_end] = (uint8_t)'\0';

        if ((tab_pos != 0xFFFFFFFFu) && (tab_pos > line_start)) {
            serial_len = tab_pos - line_start;
            conv_len = MultiByteToWideChar(CP_UTF8, 0u, (LPCCH)&out[line_start],
                                           (int32_t)serial_len,
                                           &p_devs[count].serial[0], (int32_t)(ADB_DEV_SERIAL_WCHAR - 1u));
            if (conv_len > 0) {
                p_devs[count].serial[conv_len] = L'\0';
                conv_len = MultiByteToWideChar(CP_UTF8, 0u, (LPCCH)&out[tab_pos + 1u],
                                               (int32_t)(line_end - tab_pos - 1u),
                                               &p_devs[count].state[0], (int32_t)(ADB_DEV_STATE_WCHAR - 1u));
                if (conv_len > 0) {
                    p_devs[count].state[conv_len] = L'\0';
                    count++;
                }
            }
        }
        line_start = line_end + 1u;
    }

    /* 剩余容量条目清零, 便于调用方判空 */
    for (index = count; index < max_count; index++) {
        memset(&p_devs[index], 0, sizeof(ADB_DEVICE_T));
    }

    return count;
}
