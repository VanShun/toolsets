/**
 * @file    dbg_conf.c
 * @author  herongshun
 * @date    2026-11-17
 * @brief   dbgterm 配置文件读写实现(kernel32 Profile系, 无新依赖)
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "dbg_conf.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define DBG_CONF_INI_NAME  L"dbgterm.ini"   /**< 配置文件名 */
#define DBG_CONF_PATH_MAX  280u             /**< ini路径缓冲容量 */

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void dbg_conf_path(wchar_t *p_out, uint32_t max)
{
    uint32_t i = 0u;

    memset(p_out, 0, max * sizeof(wchar_t));
    if (GetModuleFileNameW(NULL, p_out, (DWORD)max) == 0u) {
        return;
    }
    for (i = (uint32_t)lstrlenW(p_out); i > 0u; i--) {
        if ((p_out[i - 1u] == L'\\') || (p_out[i - 1u] == L'/')) {
            p_out[i] = L'\0';
            break;
        }
    }
    (void)lstrcatW(p_out, DBG_CONF_INI_NAME);
}

uint32_t dbg_conf_get_uint(const wchar_t *p_sec, const wchar_t *p_key, uint32_t def)
{
    wchar_t ini[DBG_CONF_PATH_MAX];

    memset(&ini[0], 0, sizeof(ini));
    dbg_conf_path(&ini[0], (uint32_t)(sizeof(ini) / sizeof(wchar_t)));
    return GetPrivateProfileIntW(p_sec, p_key, def, &ini[0]);
}

void dbg_conf_set_uint(const wchar_t *p_sec, const wchar_t *p_key, uint32_t val)
{
    wchar_t ini[DBG_CONF_PATH_MAX];
    wchar_t num[16u];

    memset(&ini[0], 0, sizeof(ini));
    memset(&num[0], 0, sizeof(num));
    dbg_conf_path(&ini[0], (uint32_t)(sizeof(ini) / sizeof(wchar_t)));
    (void)wsprintfW(&num[0], L"%u", (unsigned int)val);
    (void)WritePrivateProfileStringW(p_sec, p_key, &num[0], &ini[0]);
}

void dbg_conf_get_str(const wchar_t *p_sec, const wchar_t *p_key, const wchar_t *p_def,
                      wchar_t *p_out, uint32_t max)
{
    wchar_t ini[DBG_CONF_PATH_MAX];

    memset(&ini[0], 0, sizeof(ini));
    dbg_conf_path(&ini[0], (uint32_t)(sizeof(ini) / sizeof(wchar_t)));
    (void)GetPrivateProfileStringW(p_sec, p_key, (p_def != NULL) ? p_def : L"",
                                   p_out, (DWORD)max, &ini[0]);
}

void dbg_conf_set_str(const wchar_t *p_sec, const wchar_t *p_key, const wchar_t *p_val)
{
    wchar_t ini[DBG_CONF_PATH_MAX];

    memset(&ini[0], 0, sizeof(ini));
    dbg_conf_path(&ini[0], (uint32_t)(sizeof(ini) / sizeof(wchar_t)));
    (void)WritePrivateProfileStringW(p_sec, p_key, (p_val != NULL) ? p_val : L"", &ini[0]);
}
