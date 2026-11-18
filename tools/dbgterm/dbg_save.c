/**
 * @file    dbg_save.c
 * @author  herongshun
 * @date    2026-11-18
 * @brief   日志保存实现(双通道: 主窗口/过滤窗口)
 *          目录缺省 exe同目录\Record(不存在自动创建, 两通道共享);
 *          自动保存为持续追加句柄, 快照为一次性完整落盘; 均带UTF-8 BOM
 */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <shlobj.h>
#include "dbg_conf.h"
#include "dbg_save.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define DBG_SAVE_SEC       L"Save"            /**< ini节名 */
#define DBG_SAVE_DEF_SUB   L"Record"          /**< 缺省子目录名 */
#define DBG_SAVE_UTF8_BOM  {0xEFu, 0xBBu, 0xBFu}  /**< UTF-8 BOM */
#define DBG_SAVE_CH_COUNT  2u                 /**< 通道数 */

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static uint8_t  g_save_inited = 0u;                       /**< 初始化标志 */
static uint8_t  g_save_on[DBG_SAVE_CH_COUNT] = {0};       /**< 各通道自动保存开关 */
static HANDLE   g_save_file[DBG_SAVE_CH_COUNT] = {NULL};  /**< 各通道记录文件句柄 */
static wchar_t  g_save_dir[DBG_SAVE_DIR_MAX] = {0};       /**< 保存目录(共享) */

static const wchar_t *const g_prefix_auto[DBG_SAVE_CH_COUNT] =
{
    L"dbgterm_auto", L"dbgterm_flt_auto"
};
static const wchar_t *const g_prefix_snap[DBG_SAVE_CH_COUNT] =
{
    L"dbgterm", L"dbgterm_flt"
};
static const wchar_t *const g_key_auto[DBG_SAVE_CH_COUNT] =
{
    L"AutoSave", L"FltAutoSave"
};

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   确保保存目录存在(不存在则逐级创建)
 * @param   p_dir 目录路径
 * @return  1: 就绪; 0: 创建失败
 */
static uint8_t save_ensure_dir(const wchar_t *p_dir)
{
    uint32_t attr = GetFileAttributesW(p_dir);

    if (attr != INVALID_FILE_ATTRIBUTES) {
        return ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0u) ? 1u : 0u;
    }
    return (CreateDirectoryW(p_dir, NULL) != 0) ? 1u : 0u;
}

/**
 * @brief   生成带时间戳的保存文件完整路径
 * @param   p_out    路径输出缓冲
 * @param   max      缓冲容量
 * @param   p_prefix 文件名前缀
 * @return  无
 */
static void save_make_path(wchar_t *p_out, uint32_t max, const wchar_t *p_prefix)
{
    SYSTEMTIME st;

    memset(&st, 0, sizeof(st));
    GetLocalTime(&st);
    memset(p_out, 0, max * sizeof(wchar_t));
    (void)wsprintfW(p_out, L"%s\\%s_%04u%02u%02u_%02u%02u%02u.txt",
                    &g_save_dir[0], p_prefix,
                    (unsigned int)st.wYear, (unsigned int)st.wMonth, (unsigned int)st.wDay,
                    (unsigned int)st.wHour, (unsigned int)st.wMinute, (unsigned int)st.wSecond);
}

/**
 * @brief   宽文本转UTF-8堆缓冲(调用方负责free)
 * @param   p_text  宽文本
 * @param   p_bytes [out] UTF-8字节数(不含BOM)
 * @return  堆缓冲地址; 失败NULL
 */
static uint8_t *save_to_utf8(const wchar_t *p_text, uint32_t *p_bytes)
{
    uint32_t wide_len = (uint32_t)lstrlenW(p_text);
    int32_t need = WideCharToMultiByte(CP_UTF8, 0u, p_text, (int32_t)wide_len,
                                       NULL, 0, NULL, NULL);
    uint8_t *p_buf = NULL;

    if (need <= 0) {
        return NULL;
    }
    p_buf = (uint8_t *)HeapAlloc(GetProcessHeap(), 0u, (SIZE_T)need);
    if (p_buf == NULL) {
        return NULL;
    }
    if (WideCharToMultiByte(CP_UTF8, 0u, p_text, (int32_t)wide_len,
                            (char *)p_buf, need, NULL, NULL) != need) {
        (void)HeapFree(GetProcessHeap(), 0u, p_buf);
        return NULL;
    }
    *p_bytes = (uint32_t)need;
    return p_buf;
}

/**
 * @brief   打开指定通道自动保存记录文件(带BOM)
 * @param   ch 通道
 * @return  0: 成功; -1: 失败
 */
static int32_t save_open_record(uint32_t ch)
{
    static const uint8_t bom[3u] = DBG_SAVE_UTF8_BOM;
    wchar_t path[DBG_SAVE_DIR_MAX + 64u];
    DWORD written = 0u;

    if (ch >= DBG_SAVE_CH_COUNT) {
        return -1;
    }
    if (g_save_file[ch] != NULL) {
        return 0;
    }
    if (save_ensure_dir(&g_save_dir[0]) == 0u) {
        return -1;
    }
    save_make_path(&path[0], (uint32_t)(sizeof(path) / sizeof(wchar_t)), g_prefix_auto[ch]);
    g_save_file[ch] = CreateFileW(&path[0], (GENERIC_WRITE), 0u, NULL,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_save_file[ch] == INVALID_HANDLE_VALUE) {
        g_save_file[ch] = NULL;
        return -1;
    }
    (void)WriteFile(g_save_file[ch], &bom[0], 3u, &written, NULL);
    return 0;
}

/**
 * @brief   关闭指定通道记录文件
 * @param   ch 通道
 * @return  无
 */
static void save_close_record(uint32_t ch)
{
    if ((ch < DBG_SAVE_CH_COUNT) && (g_save_file[ch] != NULL)) {
        (void)CloseHandle(g_save_file[ch]);
        g_save_file[ch] = NULL;
    }
}

/**
 * @brief   计算缺省目录(exe目录\Record)并确保存在
 * @return  无
 */
static void save_default_dir(void)
{
    uint32_t i = 0u;

    memset(&g_save_dir[0], 0, sizeof(g_save_dir));
    if (GetModuleFileNameW(NULL, &g_save_dir[0], DBG_SAVE_DIR_MAX) == 0u) {
        return;
    }
    for (i = (uint32_t)lstrlenW(&g_save_dir[0]); i > 0u; i--) {
        if ((g_save_dir[i - 1u] == L'\\') || (g_save_dir[i - 1u] == L'/')) {
            g_save_dir[i] = L'\0';
            break;
        }
    }
    (void)lstrcatW(&g_save_dir[0], DBG_SAVE_DEF_SUB);
    (void)save_ensure_dir(&g_save_dir[0]);
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void dbg_save_init(void)
{
    uint32_t ch = 0u;

    if (g_save_inited != 0u) {
        return;
    }
    g_save_inited = 1u;

    dbg_conf_get_str(DBG_SAVE_SEC, L"Dir", L"", &g_save_dir[0], DBG_SAVE_DIR_MAX);
    if (g_save_dir[0] == L'\0') {
        save_default_dir();
    }
    for (ch = 0u; ch < DBG_SAVE_CH_COUNT; ch++) {
        if (dbg_conf_get_uint(DBG_SAVE_SEC, g_key_auto[ch], 0u) != 0u) {
            g_save_on[ch] = 1u;
            (void)save_open_record(ch);
        }
    }
}

uint8_t dbg_save_pick_dir(HWND h_owner)
{
    BROWSEINFOW bi;
    LPITEMIDLIST pidl = NULL;
    wchar_t path[MAX_PATH];
    uint8_t changed = 0u;

    memset(&bi, 0, sizeof(bi));
    memset(&path[0], 0, sizeof(path));
    bi.hwndOwner = h_owner;
    bi.lpszTitle = L"选择日志保存文件夹";
    bi.ulFlags = (UINT)BIF_RETURNONLYFSDIRS;

    pidl = SHBrowseForFolderW(&bi);
    if (pidl != NULL) {
        if (SHGetPathFromIDListW(pidl, &path[0]) != 0) {
            uint32_t ch = 0u;

            (void)lstrcpynW(&g_save_dir[0], &path[0], (int)DBG_SAVE_DIR_MAX);
            dbg_conf_set_str(DBG_SAVE_SEC, L"Dir", &g_save_dir[0]);
            changed = 1u;
            /* 目录变更: 开启中的通道文件切换到新目录 */
            for (ch = 0u; ch < DBG_SAVE_CH_COUNT; ch++) {
                if (g_save_on[ch] != 0u) {
                    save_close_record(ch);
                    (void)save_open_record(ch);
                }
            }
        }
        (void)CoTaskMemFree(pidl);
    }

    return changed;
}

void dbg_save_get_dir(wchar_t *p_out, uint32_t max)
{
    if ((p_out == NULL) || (max == 0u)) {
        return;
    }
    dbg_save_init();
    (void)lstrcpynW(p_out, &g_save_dir[0], (int)max);
}

void dbg_save_set_autosave_ch(uint32_t ch, uint8_t on)
{
    if (ch >= DBG_SAVE_CH_COUNT) {
        return;
    }
    dbg_save_init();
    g_save_on[ch] = (on != 0u) ? 1u : 0u;
    dbg_conf_set_uint(DBG_SAVE_SEC, g_key_auto[ch], (uint32_t)g_save_on[ch]);
    if (g_save_on[ch] != 0u) {
        (void)save_open_record(ch);
    } else {
        save_close_record(ch);
    }
}

uint8_t dbg_save_autosave_on_ch(uint32_t ch)
{
    dbg_save_init();
    return ((ch < DBG_SAVE_CH_COUNT) && (g_save_on[ch] != 0u)) ? 1u : 0u;
}

void dbg_save_feed_ch(uint32_t ch, const wchar_t *p_text)
{
    uint32_t bytes = 0u;
    uint8_t *p_utf8 = NULL;
    DWORD written = 0u;

    if ((ch >= DBG_SAVE_CH_COUNT) || (g_save_on[ch] == 0u) ||
        (g_save_file[ch] == NULL) || (p_text == NULL)) {
        return;
    }
    p_utf8 = save_to_utf8(p_text, &bytes);
    if (p_utf8 == NULL) {
        return;
    }
    (void)WriteFile(g_save_file[ch], p_utf8, (DWORD)bytes, &written, NULL);
    (void)HeapFree(GetProcessHeap(), 0u, p_utf8);
}

int32_t dbg_save_snapshot_ch(uint32_t ch, const wchar_t *p_text, wchar_t *p_msg)
{
    static const uint8_t bom[3u] = DBG_SAVE_UTF8_BOM;
    wchar_t path[DBG_SAVE_DIR_MAX + 64u];
    uint32_t bytes = 0u;
    uint8_t *p_utf8 = NULL;
    HANDLE h_file = NULL;
    DWORD written = 0u;

    if ((ch >= DBG_SAVE_CH_COUNT) || (p_text == NULL) || (p_msg == NULL)) {
        return -1;
    }
    dbg_save_init();
    p_msg[0] = L'\0';
    if (save_ensure_dir(&g_save_dir[0]) == 0u) {
        (void)lstrcpynW(p_msg, L"错误: 保存目录创建失败", DBG_SAVE_MSG_MAX);
        return -1;
    }
    save_make_path(&path[0], (uint32_t)(sizeof(path) / sizeof(wchar_t)), g_prefix_snap[ch]);

    h_file = CreateFileW(&path[0], GENERIC_WRITE, 0u, NULL,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h_file == INVALID_HANDLE_VALUE) {
        (void)wsprintfW(p_msg, L"错误: 文件创建失败(%s)", &path[0]);
        return -1;
    }
    (void)WriteFile(h_file, &bom[0], 3u, &written, NULL);

    p_utf8 = save_to_utf8(p_text, &bytes);
    if (p_utf8 != NULL) {
        (void)WriteFile(h_file, p_utf8, (DWORD)bytes, &written, NULL);
        (void)HeapFree(GetProcessHeap(), 0u, p_utf8);
    }
    (void)CloseHandle(h_file);

    (void)wsprintfW(p_msg, L"已保存: %s", &path[0]);
    return 0;
}

void dbg_save_shutdown(void)
{
    uint32_t ch = 0u;

    for (ch = 0u; ch < DBG_SAVE_CH_COUNT; ch++) {
        save_close_record(ch);
        g_save_on[ch] = 0u;
    }
    g_save_inited = 0u;
}

/* ---- 主通道便捷接口 ---- */

void dbg_save_set_autosave(uint8_t on)
{
    dbg_save_set_autosave_ch(DBG_SAVE_CH_MAIN, on);
}

uint8_t dbg_save_autosave_on(void)
{
    return dbg_save_autosave_on_ch(DBG_SAVE_CH_MAIN);
}

void dbg_save_feed(const wchar_t *p_text)
{
    dbg_save_feed_ch(DBG_SAVE_CH_MAIN, p_text);
}

int32_t dbg_save_snapshot(const wchar_t *p_text, wchar_t *p_msg)
{
    return dbg_save_snapshot_ch(DBG_SAVE_CH_MAIN, p_text, p_msg);
}
