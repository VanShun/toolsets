/**
 * @file    tool_manifest.h
 * @author  herongshun
 * @date    2026-11-05
 * @brief   工具登记表接口(主界面据此生成工具卡片入口)
 *          本模块是合集版与各工具包之间唯一的粘合点, 工具包自身不感知合集
 */

#ifndef TOOL_MANIFEST_H
#define TOOL_MANIFEST_H

#include <windows.h>
#include <stdint.h>

/** 工具打开回调函数类型 */
typedef void (*TOOL_OPEN_FN)(HINSTANCE h_inst, HWND h_owner);

/** 工具注册信息 */
typedef struct TOOL_INFO_TAG
{
    const wchar_t *p_icon;      /**< 工具图标字形(Segoe UI Symbol 符号) */
    const wchar_t *p_name;      /**< 工具显示名称 */
    const wchar_t *p_desc;      /**< 工具简要描述 */
    uint32_t      accent;       /**< 工具主题色(0x00BBGGRR) */
    TOOL_OPEN_FN  pfn_open;     /**< 打开工具窗口的回调 */
} TOOL_INFO_T;

/**
 * @brief   获取工具登记表
 * @param   p_count [out] 登记表内工具数量
 * @return  工具信息表首地址(只读); 入参非法时返回NULL
 */
const TOOL_INFO_T *tool_manifest_get_table(uint32_t *p_count);

#endif /* TOOL_MANIFEST_H */
