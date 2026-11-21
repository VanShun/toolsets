/**
 * @file    dbg_source.h
 * @author  herongshun
 * @date    2026-11-06
 * @brief   调试终端数据源统一接口与公共接收环形缓冲
 *          ADB/串口等数据源实现同一接口; 字节流写入公共环形缓冲并节流通知UI
 */

#ifndef DBG_SOURCE_H
#define DBG_SOURCE_H

#include <windows.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define DBG_SOURCE_RX_BUF_SIZE   (4u * 1024u * 1024u)  /**< 接收环形缓冲容量(字节; 大容量支撑选择保护期的数据积压与洪峰突发) */
#define DBG_SOURCE_THROTTLE_MS   30u           /**< UI通知节流间隔(毫秒) */
#define DBG_SOURCE_STATUS_LEN    256u          /**< 状态/错误消息最大宽字符数 */
#define DBG_SOURCE_NOTIFY_NONE   0u            /**< 推入不投递通知 */
#define DBG_SOURCE_NOTIFY_NOW    1u            /**< 推入并按节流投递通知 */

/* -------------------------------------------------------------------------- */
/*  类型定义                                                                    */
/* -------------------------------------------------------------------------- */

/** 数据源类型 */
typedef enum DBG_SRC_TYPE_TAG
{
    DBG_SRC_ADB = 0,      /**< ADB子进程数据源 */
    DBG_SRC_SERIAL,       /**< 串口数据源(第二阶段) */
    DBG_SRC_TYPE_COUNT    /**< 类型总数 */
} DBG_SRC_TYPE_E;

/** 数据源状态事件 */
typedef enum DBG_SOURCE_EVT_TAG
{
    DBG_SOURCE_EVT_OPENED = 0,   /**< 连接建立 */
    DBG_SOURCE_EVT_CLOSED,       /**< 连接断开 */
    DBG_SOURCE_EVT_ERROR,        /**< 连接过程出错 */
    DBG_SOURCE_EVT_RELOGIN       /**< 异常断开, 自动重登已调度(ADB) */
} DBG_SOURCE_EVT_E;

/**
 * 状态回调: 由数据源内部线程上下文调用,
 * 实现方须保证线程安全(只做PostMessage类投递, 不直接操作控件)
 */
typedef void (*DBG_SOURCE_STATUS_FN)(uint32_t event, const wchar_t *p_msg, void *p_user);

/** 接收环形缓冲(UI线程初始化持有, 数据源读线程写入) */
typedef struct DBG_SOURCE_RX_TAG
{
    CRITICAL_SECTION cs;                   /**< 互斥保护 */
    uint8_t  buf[DBG_SOURCE_RX_BUF_SIZE];  /**< 环形存储区 */
    uint32_t rd;                           /**< 读位置索引 */
    uint32_t wr;                           /**< 写位置索引 */
    uint32_t used;                         /**< 已用字节数 */
    uint32_t overflow;                     /**< 溢出丢弃累计(取走后清零) */
    HWND     h_notify;                     /**< 节流通知目标窗口 */
    uint32_t notify_msg;                   /**< 通知消息号(建议WM_APP+x) */
    uint32_t last_notify_tick;             /**< 上次通知时刻(GetTickCount) */
} DBG_SOURCE_RX_T;

/** 数据源统一接口(单实例连接, 实现为模块级单例) */
typedef struct DBG_SOURCE_TAG
{
    int32_t (*pfn_open)(const void *p_cfg, DBG_SOURCE_STATUS_FN pfn_status, void *p_user); /**< 建立连接, 0=成功 */
    void    (*pfn_close)(void);                                                            /**< 断开连接(幂等) */
    int32_t (*pfn_write)(const uint8_t *p_data, uint32_t len);                             /**< 发送数据, 0=成功 */
    uint8_t (*pfn_is_open)(void);                                                          /**< 连接状态查询 */
} DBG_SOURCE_T;

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   初始化接收环形缓冲(进程内一次性, UI线程调用)
 * @param   p_rx         环形缓冲对象
 * @param   h_notify     节流通知目标窗口
 * @param   notify_msg   通知消息号(建议WM_APP+x)
 * @return  无
 */
void dbg_source_rx_init(DBG_SOURCE_RX_T *p_rx, HWND h_notify, uint32_t notify_msg);

/**
 * @brief   推收数据到环形缓冲(任意线程; 缓冲不足时丢弃新数据并累计溢出)
 * @param   p_rx     环形缓冲对象
 * @param   p_data   数据
 * @param   len      数据长度(字节)
 * @param   notify   1=按节流间隔投递通知消息
 * @return  无
 */
void dbg_source_rx_push(DBG_SOURCE_RX_T *p_rx, const uint8_t *p_data, uint32_t len, uint8_t notify);

/**
 * @brief   取走环形缓冲内全部新数据(UI线程)
 * @param   p_rx      环形缓冲对象
 * @param   p_out     输出缓冲
 * @param   max_bytes 输出缓冲容量
 * @return  实际取走的字节数
 */
uint32_t dbg_source_rx_drain(DBG_SOURCE_RX_T *p_rx, uint8_t *p_out, uint32_t max_bytes);

/**
 * @brief   取走溢出丢弃累计值(读后清零)
 * @param   p_rx 环形缓冲对象
 * @return  自上次取走以来溢出丢弃的字节数
 */
uint32_t dbg_source_rx_take_overflow(DBG_SOURCE_RX_T *p_rx);

/**
 * @brief   查询环形缓冲是否有待取数据(线程安全, 无副作用; 供UI节拍早退判断)
 * @param   p_rx 环形缓冲对象
 * @return  1: 有数据; 0: 空
 */
uint8_t dbg_source_rx_has_data(const DBG_SOURCE_RX_T *p_rx);

/**
 * @brief   等待新到数据中出现指定ASCII标记(不消费数据, 供UI正常显示)
 *          从调用时刻的写入位置起扫描, 旧数据不参与匹配(重登场景防误触发)
 * @param   p_rx       环形缓冲对象
 * @param   p_needle   [in] 标记串(小写ASCII, 如"assword")
 * @param   timeout_ms 超时(毫秒)
 * @return  1: 匹配到标记; 0: 超时
 */
uint8_t dbg_source_rx_wait_mark(DBG_SOURCE_RX_T *p_rx, const char *p_needle, uint32_t timeout_ms);

/**
 * @brief   获取指定类型数据源的接口实现
 * @param   type 数据源类型(DBG_SRC_TYPE_E)
 * @return  接口表首地址; 类型未实现/非法时返回NULL
 */
const DBG_SOURCE_T *dbg_source_get(uint32_t type);

#endif /* DBG_SOURCE_H */
