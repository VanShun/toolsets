/**
 * @file    version.h
 * @author  herongshun
 * @date    2026-11-18
 * @brief   工程版本号单一真源(C代码与exe版本资源共用)
 *          发版流程: 同步修改下方三个数字与两个字符串 -> 重建
 *                    -> git tag vx.y.z -> push --tags
 */

#ifndef VERSION_H
#define VERSION_H

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define VERSION_MAJOR  1        /**< 主版本(不兼容的架构变更) */
#define VERSION_MINOR  4        /**< 次版本(向下兼容的功能新增) */
#define VERSION_PATCH  0        /**< 修订号(缺陷修复) */

/* 发版时与上方三个数字同步修改(刻意不做宏拼接, 避免预处理器陷阱) */
#define VERSION_STR   "1.4.0"   /**< 版本字符串(窄字符, .rc资源用) */
#define VERSION_WSTR  L"1.4.0"  /**< 版本字符串(宽字符, 界面显示用) */

#endif /* VERSION_H */
