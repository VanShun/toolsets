/**
 * @file    updater.h
 * @author  herongshun
 * @date    2026-11-18
 * @brief   应用内更新检查与自升级接口
 *          数据源: GitHub Releases latest API; 支持ini配置镜像加速前缀
 */

#ifndef UPDATER_H
#define UPDATER_H

#include <windows.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define UPD_REPO_API  L"https://api.github.com/repos/VanShun/toolsets/releases/latest" /**< Releases API */

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   异步检查更新(后台线程联网, 结果经隐藏窗口回到UI线程呈现)
 * @param   h_owner 所有者窗口(主界面)
 * @param   manual  1=手动触发(无更新/出错也提示); 0=启动静默(仅发现新版才提示)
 * @return  无
 */
void updater_check_async(HWND h_owner, uint8_t manual);

#endif /* UPDATER_H */
