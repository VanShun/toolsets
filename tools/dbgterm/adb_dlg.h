/**
 * @file    adb_dlg.h
 * @author  herongshun
 * @date    2026-11-16
 * @brief   ADB 连接配置对话框接口
 *          从主窗口"连接"菜单或工具栏打开, 模态收集 adb路径/设备/自动命令并建立连接
 */

#ifndef ADB_DLG_H
#define ADB_DLG_H

#include <windows.h>
#include <stdint.h>
#include "dbg_source.h"

/* -------------------------------------------------------------------------- */
/*  类型定义                                                                    */
/* -------------------------------------------------------------------------- */

/** 对话框上下文(主窗口提供, 连接动作经数据源接口执行) */
typedef struct ADB_DLG_CTX_TAG
{
    DBG_SOURCE_RX_T      *p_rx;         /**< 主窗口接收环形缓冲 */
    DBG_SOURCE_STATUS_FN pfn_status;    /**< 状态回调(主窗口) */
    void                 *p_user;       /**< 状态回调用户指针 */
} ADB_DLG_CTX_T;

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   打开 ADB 连接配置对话框(模态)
 *          用户点[连接]且数据源连接成功后对话框关闭; 取消/关闭直接返回
 * @param   h_inst  应用实例句柄
 * @param   h_owner 所有者窗口(主窗口)
 * @param   p_ctx   [in] 连接上下文(须存活至对话框关闭)
 * @return  无
 */
void adb_dlg_open(HINSTANCE h_inst, HWND h_owner, const ADB_DLG_CTX_T *p_ctx);

#endif /* ADB_DLG_H */
