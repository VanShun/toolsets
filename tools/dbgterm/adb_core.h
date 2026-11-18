/**
 * @file    adb_core.h
 * @author  herongshun
 * @date    2026-11-06
 * @brief   ADB 数据源接口(adb shell 管道子进程 + 设备枚举)
 *          形态对标 getsscom AdbDataSource: 连接后自动下发命令(如 logcat)
 */

#ifndef ADB_CORE_H
#define ADB_CORE_H

#include <windows.h>
#include <stdint.h>
#include "dbg_source.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define ADB_CFG_PATH_WCHAR     260u   /**< adb.exe 路径宽字符容量 */
#define ADB_CFG_SERIAL_WCHAR   64u    /**< 设备序列号宽字符容量 */
#define ADB_CFG_CMD_WCHAR      256u   /**< 自动命令宽字符容量 */
#define ADB_CFG_LOGIN_WCHAR    64u    /**< 登录账号/密码宽字符容量 */
#define ADB_CFG_MAX_AUTO_CMD   3u     /**< 自动命令最大条数 */
#define ADB_DEV_LIST_MAX       16u    /**< 设备枚举最大条数 */
#define ADB_DEV_SERIAL_WCHAR   64u    /**< 枚举序列号宽字符容量 */
#define ADB_DEV_STATE_WCHAR    16u    /**< 枚举状态宽字符容量 */

/* -------------------------------------------------------------------------- */
/*  类型定义                                                                    */
/* -------------------------------------------------------------------------- */

/** ADB 数据源连接配置 */
typedef struct ADB_CFG_TAG
{
    wchar_t adb_path[ADB_CFG_PATH_WCHAR];                              /**< adb.exe完整路径 */
    wchar_t device_serial[ADB_CFG_SERIAL_WCHAR];                       /**< 设备序列号(空=不指定) */
    wchar_t login_user[ADB_CFG_LOGIN_WCHAR];                           /**< 登录账号(空=不登录, 对标getsscom User) */
    wchar_t login_password[ADB_CFG_LOGIN_WCHAR];                       /**< 登录密码(空=仅发账号) */
    uint32_t login_pwd_delay_s;                                        /**< 账号发出到密码发出的延迟(秒, 设备shell提示慢) */
    wchar_t auto_cmd[ADB_CFG_MAX_AUTO_CMD][ADB_CFG_CMD_WCHAR];         /**< 登录后自动下发命令 */
    uint32_t auto_cmd_count;                                           /**< 自动命令条数 */
    uint32_t auto_cmd_interval_ms;                                     /**< 自动命令下发间隔 */
    uint8_t  auto_relogin;                                             /**< 1=连接断开后按间隔自动重登 */
    uint32_t relogin_interval_s;                                       /**< 自动重登间隔(秒, 1~86400) */
    DBG_SOURCE_RX_T *p_rx;                                             /**< 接收环形缓冲(调用方初始化) */
    const wchar_t *p_test_cmdline;                                     /**< 测试钩子: 非空则作完整命令行(替代adb) */
} ADB_CFG_T;

/** 设备枚举条目 */
typedef struct ADB_DEVICE_TAG
{
    wchar_t serial[ADB_DEV_SERIAL_WCHAR];   /**< 设备序列号 */
    wchar_t state[ADB_DEV_STATE_WCHAR];     /**< 状态: device/offline/unauthorized */
} ADB_DEVICE_T;

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   枚举当前 adb 可见设备(同步执行"adb devices", 首次可能数秒)
 * @param   p_adb_path [in]  adb.exe 完整路径
 * @param   p_devs     [out] 设备条目输出数组
 * @param   max_count  [in]  数组容量
 * @return  实际枚举到的设备数; 失败返回0(调用方检查 GetLastError 可选)
 */
uint32_t adb_core_enum_devices(const wchar_t *p_adb_path,
                               ADB_DEVICE_T *p_devs, uint32_t max_count);

/**
 * @brief   获取最近一次错误的详细消息
 * @param   p_out [out] 消息输出缓冲
 * @param   max   [in]  缓冲容量(宽字符)
 * @return  无
 */
void adb_core_get_last_error(wchar_t *p_out, uint32_t max);

/**
 * @brief   获取 ADB 数据源接口实现
 * @param   无
 * @return  接口表首地址(只读)
 */
const DBG_SOURCE_T *adb_core_get_source(void);

#endif /* ADB_CORE_H */
