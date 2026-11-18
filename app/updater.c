/**
 * @file    updater.c
 * @author  herongshun
 * @date    2026-11-18
 * @brief   应用内更新检查与自升级实现
 *          检查: 后台线程 GET Releases latest -> 解析tag/body/下载URL -> 投递结果
 *          升级: 下载 toolsets_update.exe(校验MZ头) -> 写update.bat
 *                (等待本进程退出->替换->重启->自删) -> 启动bat并退出
 *          配置: exe同目录dbgterm.ini [Update] Mirror(镜像前缀) / AutoCheck
 */

#include <windows.h>
#include <wininet.h>
#include <process.h>
#include <stdint.h>
#include <string.h>
#include "version.h"
#include "updater.h"

/* -------------------------------------------------------------------------- */
/*  宏定义                                                                     */
/* -------------------------------------------------------------------------- */
#define UPD_WND_CLASS      L"TS_UPD_HWND"        /**< 隐藏回调窗口类名 */
#define UPD_MSG_RESULT     (WM_APP + 0x10u)      /**< 检查结果投递消息 */
#define UPD_HTTP_TIMEOUT   12000u                /**< 网络超时(毫秒) */
#define UPD_JSON_MAX       (128u * 1024u)        /**< API响应缓冲上限(字节) */
#define UPD_TAG_MAX        16u                   /**< 版本字符串容量 */
#define UPD_URL_MAX        512u                  /**< 下载URL容量 */
#define UPD_BODY_MAX       2048u                 /**< 更新说明展示上限(宽字符) */
#define UPD_MIRROR_MAX     256u                  /**< 镜像前缀容量 */
#define UPD_ASSET_MIN      (100u * 1024u)        /**< 升级包最小体积(字节, 防错误页) */

/** 检查结果码 */
typedef enum UPD_CODE_TAG
{
    UPD_CODE_UPDATE = 0,   /**< 有新版本 */
    UPD_CODE_LATEST,       /**< 已是最新 */
    UPD_CODE_ERROR         /**< 检查失败 */
} UPD_CODE_E;

/* -------------------------------------------------------------------------- */
/*  内部类型                                                                    */
/* -------------------------------------------------------------------------- */

/** 检查结果(堆分配, UI线程消费后释放) */
typedef struct UPD_RESULT_TAG
{
    uint8_t manual;                          /**< 手动触发标志 */
    uint32_t code;                           /**< UPD_CODE_E */
    wchar_t ver[UPD_TAG_MAX];                /**< 最新版本号(如1.0.1) */
    wchar_t url[UPD_URL_MAX];                /**< 升级exe下载直链 */
    wchar_t body[UPD_BODY_MAX];              /**< 更新说明(已反转义) */
    wchar_t err[128u];                       /**< 错误消息 */
} UPD_RESULT_T;

/** 检查线程参数 */
typedef struct UPD_THREAD_ARG_TAG
{
    HWND h_notify;                            /**< 结果投递窗口 */
    uint8_t manual;                           /**< 手动触发标志 */
    wchar_t mirror[UPD_MIRROR_MAX];           /**< 镜像前缀(可为空) */
} UPD_THREAD_ARG_T;

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static uint8_t  g_upd_class_ready = 0u;        /**< 窗口类注册标志 */
static HWND     g_upd_hwnd = NULL;             /**< 隐藏回调窗口 */
static HWND     g_upd_howner = NULL;           /**< 所有者窗口(主界面) */

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 声明                                                         */
/* -------------------------------------------------------------------------- */
static void upd_download_and_swap(const UPD_RESULT_T *p_res);

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 配置与工具                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   获取ini路径并读取[Update]字符串键
 * @param   p_key 键名
 * @param   p_def 缺省值
 * @param   p_out 输出缓冲
 * @param   max   缓冲容量
 * @return  无
 */
static void upd_ini_get(const wchar_t *p_key, const wchar_t *p_def,
                        wchar_t *p_out, uint32_t max)
{
    wchar_t ini[MAX_PATH + 16u];
    uint32_t i = 0u;

    memset(&ini[0], 0, sizeof(ini));
    memset(p_out, 0, max * sizeof(wchar_t));
    if (GetModuleFileNameW(NULL, &ini[0], MAX_PATH) != 0u) {
        for (i = (uint32_t)lstrlenW(&ini[0]); i > 0u; i--) {
            if ((ini[i - 1u] == L'\\') || (ini[i - 1u] == L'/')) {
                ini[i] = L'\0';
                break;
            }
        }
        (void)lstrcatW(&ini[0], L"dbgterm.ini");
    }
    (void)GetPrivateProfileStringW(L"Update", p_key, p_def, p_out, (DWORD)max, &ini[0]);
}

/**
 * @brief   从JSON文本提取指定字符串键的原始值(字节, 含转义还原; 不做编码转换)
 * @param   p_json JSON文本(窄字符)
 * @param   p_key  键名(如"tag_name")
 * @param   p_out  [out] 值输出(窄字符缓冲)
 * @param   max    输出容量(字节)
 * @return  1: 成功; 0: 未找到
 */
static uint8_t upd_json_get_raw(const char *p_json, const char *p_key,
                                char *p_out, uint32_t max)
{
    char pattern[64u];
    const char *p_hit = NULL;
    const char *p = NULL;
    uint32_t out_pos = 0u;

    memset(p_out, 0, max);
    memset(&pattern[0], 0, sizeof(pattern));
    (void)lstrcpyA(&pattern[0], "\"");
    (void)lstrcatA(&pattern[0], p_key);
    (void)lstrcatA(&pattern[0], "\":");

    p_hit = strstr(p_json, &pattern[0]);
    if (p_hit == NULL) {
        return 0u;
    }
    p = p_hit + lstrlenA(&pattern[0]);
    while (*p == ' ') {
        p++;
    }
    if (*p != '"') {
        return 0u;
    }
    p++;   /* 跳过开引号 */
    while ((*p != '\0') && (*p != '"') && (out_pos < (max - 1u))) {
        char ch = *p;

        if (ch == '\\') {
            p++;
            ch = *p;
            if (ch == 'n') {
                ch = '\n';
            } else if (ch == 'r') {
                ch = '\r';
            } else if (ch == 't') {
                ch = '\t';
            } else {
                /* \" 与 \\ 保留字符本身 */
            }
        }
        p_out[out_pos] = ch;
        out_pos++;
        p++;
    }
    if (*p != '"') {
        return 0u;
    }

    return 1u;
}

/**
 * @brief   从JSON文本提取字符串键并按UTF-8解码为宽字符(中文更新说明不乱码)
 * @param   p_json JSON文本
 * @param   p_key  键名
 * @param   p_out  [out] 宽字符输出
 * @param   max    输出容量(宽字符)
 * @return  1: 成功; 0: 未找到
 */
static uint8_t upd_json_get_str(const char *p_json, const char *p_key,
                                wchar_t *p_out, uint32_t max)
{
    char raw[UPD_BODY_MAX];

    if (upd_json_get_raw(p_json, p_key, &raw[0], (uint32_t)sizeof(raw)) == 0u) {
        memset(p_out, 0, max * sizeof(wchar_t));
        return 0u;
    }
    if (MultiByteToWideChar(CP_UTF8, 0u, &raw[0], -1, p_out, (int32_t)max) <= 0) {
        memset(p_out, 0, max * sizeof(wchar_t));
        return 0u;
    }

    return 1u;
}

/**
 * @brief   提取升级exe下载直链(assets中名称含toolsets.exe的首个URL)
 * @param   p_json JSON文本
 * @param   p_out  [out] URL输出
 * @param   max    容量
 * @return  1: 成功; 0: 未找到
 */
static uint8_t upd_json_get_asset_url(const char *p_json, wchar_t *p_out, uint32_t max)
{
    const char *p = strstr(p_json, "\"browser_download_url\"");

    while (p != NULL) {
        if (upd_json_get_str(p, "browser_download_url", p_out, max) != 0u) {
            if (wcsstr(p_out, L"toolsets.exe") != NULL) {
                return 1u;
            }
        }
        p = strstr(p + 24, "\"browser_download_url\"");
    }

    return 0u;
}

/**
 * @brief   比较版本号(点分三段), 判断远端是否更新
 * @param   p_remote 远端版本字符串(可带v前缀)
 * @param   p_local  本地版本字符串
 * @return  1: 远端更新; 0: 不更新
 */
static uint8_t upd_version_newer(const wchar_t *p_remote, const wchar_t *p_local)
{
    uint32_t r[3] = {0u, 0u, 0u};
    uint32_t l[3] = {0u, 0u, 0u};
    const wchar_t *pr = p_remote;
    const wchar_t *pl = p_local;
    uint32_t i = 0u;

    if (*pr == L'v') {
        pr++;
    }
    for (i = 0u; i < 3u; i++) {
        while ((*pr >= L'0') && (*pr <= L'9')) {
            r[i] = (r[i] * 10u) + (uint32_t)(*pr - L'0');
            pr++;
        }
        if (*pr == L'.') {
            pr++;
        }
    }
    for (i = 0u; i < 3u; i++) {
        while ((*pl >= L'0') && (*pl <= L'9')) {
            l[i] = (l[i] * 10u) + (uint32_t)(*pl - L'0');
            pl++;
        }
        if (*pl == L'.') {
            pl++;
        }
    }
    for (i = 0u; i < 3u; i++) {
        if (r[i] != l[i]) {
            return (r[i] > l[i]) ? 1u : 0u;
        }
    }

    return 0u;
}

/**
 * @brief   WinINet下载URL内容到内存或文件
 * @param   p_url     完整URL
 * @param   p_file    [in] 目标文件路径(非NULL则落盘); NULL时p_buf/p_cap用于读内存
 * @param   p_buf     [out] 内存缓冲(仅p_file为NULL时使用)
 * @param   cap       内存缓冲容量
 * @param   p_got     [out] 实际字节数
 * @return  0: 成功; -1: 失败
 */
static int32_t upd_http_fetch(const wchar_t *p_url, const wchar_t *p_file,
                              char *p_buf, uint32_t cap, uint32_t *p_got)
{
    HINTERNET h_net = NULL;
    HINTERNET h_url = NULL;
    DWORD timeout = UPD_HTTP_TIMEOUT;
    DWORD total = 0u;
    int32_t ret = -1;

    h_net = InternetOpenW(L"toolsets-updater", INTERNET_OPEN_TYPE_PRECONFIG,
                          NULL, NULL, 0u);
    if (h_net == NULL) {
        return -1;
    }
    (void)InternetSetOptionW(h_net, INTERNET_OPTION_RECEIVE_TIMEOUT,
                             &timeout, (DWORD)sizeof(timeout));
    (void)InternetSetOptionW(h_net, INTERNET_OPTION_CONNECT_TIMEOUT,
                             &timeout, (DWORD)sizeof(timeout));

    h_url = InternetOpenUrlW(h_net, p_url, NULL, 0u,
                             (INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                              INTERNET_FLAG_SECURE), 0u);
    if (h_url == NULL) {
        (void)InternetCloseHandle(h_net);
        return -1;
    }

    if (p_file != NULL) {
        HANDLE h_out = CreateFileW(p_file, GENERIC_WRITE, 0u, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (h_out == INVALID_HANDLE_VALUE) {
            goto done;
        }
        for (;;) {
            uint8_t chunk[8192u];
            DWORD got = 0u;

            if (InternetReadFile(h_url, &chunk[0], (DWORD)sizeof(chunk), &got) == 0) {
                (void)CloseHandle(h_out);
                goto done;
            }
            if (got == 0u) {
                break;
            }
            if (WriteFile(h_out, &chunk[0], got, &got, NULL) == 0) {
                (void)CloseHandle(h_out);
                goto done;
            }
            total += got;
        }
        (void)CloseHandle(h_out);
        *p_got = total;
        ret = 0;
    } else {
        for (;;) {
            DWORD got = 0u;

            if (total >= cap) {
                break;
            }
            if (InternetReadFile(h_url, &p_buf[total], (DWORD)(cap - total), &got) == 0) {
                goto done;
            }
            if (got == 0u) {
                break;
            }
            total += got;
        }
        *p_got = total;
        ret = 0;
    }

done:
    (void)InternetCloseHandle(h_url);
    (void)InternetCloseHandle(h_net);
    return ret;
}

/**
 * @brief   拼接"镜像前缀+原始URL"到输出缓冲
 * @param   p_mirror 镜像前缀(可为空)
 * @param   p_url    原始URL
 * @param   p_out    输出缓冲
 * @param   max      容量
 * @return  无
 */
static void upd_with_mirror(const wchar_t *p_mirror, const wchar_t *p_url,
                            wchar_t *p_out, uint32_t max)
{
    memset(p_out, 0, max * sizeof(wchar_t));
    (void)lstrcpynW(p_out, p_mirror, (int)max);
    (void)lstrcpynW(p_out + lstrlenW(p_out), p_url, (int)(max - lstrlenW(p_out)));
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 检查线程与UI                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief   检查线程体: 拉取latest并解析, 结果堆分配后投递到UI
 * @param   p_arg UPD_THREAD_ARG_T指针(线程自行释放)
 * @return  线程退出码(0)
 */
static uint32_t __stdcall upd_check_thread(void *p_arg)
{
    UPD_THREAD_ARG_T *p_cfg = (UPD_THREAD_ARG_T *)p_arg;
    UPD_RESULT_T *p_res = NULL;
    char *p_json = NULL;
    wchar_t url[UPD_MIRROR_MAX + UPD_URL_MAX];
    wchar_t tag[UPD_TAG_MAX];
    uint32_t got = 0u;

    p_res = (UPD_RESULT_T *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(UPD_RESULT_T));
    p_json = (char *)HeapAlloc(GetProcessHeap(), 0u, UPD_JSON_MAX);
    if ((p_res == NULL) || (p_json == NULL)) {
        if (p_res != NULL) {
            p_res->manual = p_cfg->manual;
            p_res->code = (uint32_t)UPD_CODE_ERROR;
            (void)lstrcpynW(&p_res->err[0], L"内存不足", 128u);
            (void)PostMessageW(g_upd_hwnd, UPD_MSG_RESULT, 0u, (LPARAM)p_res);
        }
        if (p_json != NULL) {
            (void)HeapFree(GetProcessHeap(), 0u, p_json);
        }
        (void)HeapFree(GetProcessHeap(), 0u, p_cfg);
        return 0u;
    }
    p_res->manual = p_cfg->manual;

    upd_with_mirror(&p_cfg->mirror[0], UPD_REPO_API, &url[0],
                    (uint32_t)(sizeof(url) / sizeof(wchar_t)));
    if (upd_http_fetch(&url[0], NULL, p_json, UPD_JSON_MAX, &got) != 0) {
        p_res->code = (uint32_t)UPD_CODE_ERROR;
        (void)lstrcpynW(&p_res->err[0], L"网络请求失败(检查网络或配置镜像)", 128u);
        (void)PostMessageW(g_upd_hwnd, UPD_MSG_RESULT, 0u, (LPARAM)p_res);
        (void)HeapFree(GetProcessHeap(), 0u, p_json);
        (void)HeapFree(GetProcessHeap(), 0u, p_cfg);
        return 0u;
    }
    p_json[(got < UPD_JSON_MAX) ? got : (UPD_JSON_MAX - 1u)] = '\0';

    if ((upd_json_get_str(p_json, "tag_name", &tag[0], UPD_TAG_MAX) == 0u) ||
        (upd_json_get_asset_url(p_json, &p_res->url[0], UPD_URL_MAX) == 0u)) {
        p_res->code = (uint32_t)UPD_CODE_ERROR;
        (void)lstrcpynW(&p_res->err[0], L"响应解析失败(非Release内容)", 128u);
    } else {
        const wchar_t *p_tag = &tag[0];

        (void)upd_json_get_str(p_json, "body", &p_res->body[0], UPD_BODY_MAX);
        if (*p_tag == L'v') {
            p_tag++;   /* 去除tag的v前缀, 避免展示"vv1.0.0" */
        }
        (void)lstrcpynW(&p_res->ver[0], p_tag, UPD_TAG_MAX);
        if (upd_version_newer(&tag[0], VERSION_WSTR) != 0u) {
            p_res->code = (uint32_t)UPD_CODE_UPDATE;
        } else {
            p_res->code = (uint32_t)UPD_CODE_LATEST;
        }
    }

    (void)PostMessageW(g_upd_hwnd, UPD_MSG_RESULT, 0u, (LPARAM)p_res);
    (void)HeapFree(GetProcessHeap(), 0u, p_json);
    (void)HeapFree(GetProcessHeap(), 0u, p_cfg);
    return 0u;
}

/**
 * @brief   隐藏窗口消息处理: 检查结果呈现与升级流程
 * @param   hwnd  窗口句柄
 * @param   msg   消息类型
 * @param   wp    消息参数1
 * @param   lp    消息参数2(UPD_RESULT_T*)
 * @return  消息处理结果
 */
static LRESULT CALLBACK upd_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == UPD_MSG_RESULT) {
        UPD_RESULT_T *p_res = (UPD_RESULT_T *)lp;

        if (p_res != NULL) {
            switch (p_res->code)
            {
                case UPD_CODE_LATEST:
                {
                    if (p_res->manual != 0u) {
                        (void)MessageBoxW(g_upd_howner,
                                          L"已是最新版本(v" VERSION_WSTR L")。",
                                          L"检查更新", (MB_OK | MB_ICONINFORMATION));
                    }
                    break;
                }
                case UPD_CODE_UPDATE:
                {
                    wchar_t msgbox[UPD_BODY_MAX + 128u];

                    memset(&msgbox[0], 0, sizeof(msgbox));
                    (void)wsprintfW(&msgbox[0],
                                    L"发现新版本 v%s(当前 v" VERSION_WSTR L")\n\n%s\n\n是否下载并安装?",
                                    &p_res->ver[0], &p_res->body[0]);
                    if (MessageBoxW(g_upd_howner, &msgbox[0], L"检查更新",
                                    (MB_YESNO | MB_ICONQUESTION)) == IDYES) {
                        upd_download_and_swap(p_res);
                    }
                    break;
                }
                case UPD_CODE_ERROR:
                {
                    if (p_res->manual != 0u) {
                        (void)MessageBoxW(g_upd_howner, &p_res->err[0],
                                          L"检查更新", (MB_OK | MB_ICONWARNING));
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }
            (void)HeapFree(GetProcessHeap(), 0u, p_res);
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

/**
 * @brief   下载升级包并生成替换脚本, 提示后启动脚本退出程序
 * @param   p_res 检查结果(含下载URL)
 * @return  无
 */
static void upd_download_and_swap(const UPD_RESULT_T *p_res)
{
    wchar_t exe_dir[MAX_PATH];
    wchar_t dl_path[MAX_PATH + 32u];
    wchar_t bat_path[MAX_PATH + 32u];
    wchar_t url[UPD_MIRROR_MAX + UPD_URL_MAX];
    wchar_t mirror[UPD_MIRROR_MAX];
    HANDLE h_chk = NULL;
    DWORD got = 0u;
    uint32_t i = 0u;
    WORD mz = 0u;
    DWORD read_back = 0u;

    memset(&exe_dir[0], 0, sizeof(exe_dir));
    if (GetModuleFileNameW(NULL, &exe_dir[0], MAX_PATH) == 0u) {
        return;
    }
    for (i = (uint32_t)lstrlenW(&exe_dir[0]); i > 0u; i--) {
        if ((exe_dir[i - 1u] == L'\\') || (exe_dir[i - 1u] == L'/')) {
            exe_dir[i] = L'\0';
            break;
        }
    }
    (void)wsprintfW(&dl_path[0], L"%stoolsets_update.exe", &exe_dir[0]);
    (void)wsprintfW(&bat_path[0], L"%supdate.bat", &exe_dir[0]);

    /* 下载(镜像前缀改写直链) */
    (void)SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_WAIT));
    upd_ini_get(L"Mirror", L"", &mirror[0], UPD_MIRROR_MAX);
    upd_with_mirror(&mirror[0], &p_res->url[0], &url[0],
                    (uint32_t)(sizeof(url) / sizeof(wchar_t)));
    if (upd_http_fetch(&url[0], &dl_path[0], NULL, 0u, &got) != 0) {
        (void)SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
        (void)MessageBoxW(g_upd_howner, L"下载失败(检查网络或配置镜像)",
                          L"检查更新", (MB_OK | MB_ICONWARNING));
        return;
    }
    (void)SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));

    /* 校验: 体积下限 + MZ文件头 */
    if (got < UPD_ASSET_MIN) {
        (void)MessageBoxW(g_upd_howner, L"下载内容异常(体积过小), 已取消",
                          L"检查更新", (MB_OK | MB_ICONWARNING));
        return;
    }
    h_chk = CreateFileW(&dl_path[0], GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, 0u, NULL);
    if (h_chk == INVALID_HANDLE_VALUE) {
        return;
    }
    (void)ReadFile(h_chk, &mz, 2u, &read_back, NULL);
    (void)CloseHandle(h_chk);
    if ((read_back != 2u) || (mz != 0x5A4Du)) {
        (void)MessageBoxW(g_upd_howner, L"下载内容异常(非可执行文件), 已取消",
                          L"检查更新", (MB_OK | MB_ICONWARNING));
        return;
    }

    /* 替换脚本: 等本进程退出 -> 重试替换 -> 重启 -> 自删 */
    {
        HANDLE h_bat = CreateFileW(&bat_path[0], GENERIC_WRITE, 0u, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        static const char bat_text[] =
            "@echo off\r\n"
            "cd /d \"%~dp0\"\r\n"
            ":W\r\n"
            "tasklist /fi \"IMAGENAME eq toolsets.exe\" 2>nul | find /i \"toolsets.exe\" >nul\r\n"
            "if %errorlevel%==0 (\r\n"
            "    timeout /t 1 /nobreak >nul\r\n"
            "    goto W\r\n"
            ")\r\n"
            "set /a tries=0\r\n"
            ":M\r\n"
            "move /y toolsets_update.exe toolsets.exe >nul\r\n"
            "if not %errorlevel%==0 (\r\n"
            "    set /a tries+=1\r\n"
            "    if %tries% lss 10 (\r\n"
            "        timeout /t 1 /nobreak >nul\r\n"
            "        goto M\r\n"
            "    )\r\n"
            "    exit /b 1\r\n"
            ")\r\n"
            "start \"\" toolsets.exe\r\n"
            "(goto) 2>nul\r\n"
            "del \"%~f0\" 2>nul\r\n";
        DWORD written = 0u;

        if (h_bat == INVALID_HANDLE_VALUE) {
            return;
        }
        (void)WriteFile(h_bat, bat_text, (DWORD)(sizeof(bat_text) - 1u), &written, NULL);
        (void)CloseHandle(h_bat);
    }

    (void)MessageBoxW(g_upd_howner, L"下载完成, 程序即将重启完成升级。",
                      L"检查更新", (MB_OK | MB_ICONINFORMATION));
    {
        char bat_ansi[MAX_PATH + 32u];

        memset(&bat_ansi[0], 0, sizeof(bat_ansi));
        (void)WideCharToMultiByte(CP_ACP, 0u, &bat_path[0], -1,
                                  &bat_ansi[0], (int32_t)sizeof(bat_ansi), NULL, NULL);
        (void)WinExec(&bat_ansi[0], SW_HIDE);
    }
    (void)DestroyWindow(g_upd_howner);   /* 主界面退出(无owner)即进程结束, bat接管替换 */
    (void)PostQuitMessage(0);            /* 退出双保险 */
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void updater_check_async(HWND h_owner, uint8_t manual)
{
    UPD_THREAD_ARG_T *p_arg = NULL;
    uintptr_t t = 0u;
    wchar_t auto_chk[8u];

    g_upd_howner = h_owner;

    if (g_upd_class_ready == 0u) {
        WNDCLASSW wc;

        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = upd_wnd_proc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = UPD_WND_CLASS;
        if (RegisterClassW(&wc) == 0) {
            return;
        }
        g_upd_class_ready = 1u;
    }
    if (g_upd_hwnd == NULL) {
        g_upd_hwnd = CreateWindowExW(0u, UPD_WND_CLASS, L"", 0u,
                                     0, 0, 0, 0, HWND_MESSAGE, NULL,
                                     GetModuleHandleW(NULL), NULL);
        if (g_upd_hwnd == NULL) {
            return;
        }
    }

    if (manual == 0u) {
        upd_ini_get(L"AutoCheck", L"1", &auto_chk[0], 8u);
        if (auto_chk[0] == L'0') {
            return;   /* 启动静默检查已被关闭 */
        }
    }

    p_arg = (UPD_THREAD_ARG_T *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                          sizeof(UPD_THREAD_ARG_T));
    if (p_arg == NULL) {
        return;
    }
    p_arg->h_notify = g_upd_hwnd;
    p_arg->manual = manual;
    upd_ini_get(L"Mirror", L"", &p_arg->mirror[0], UPD_MIRROR_MAX);

    t = _beginthreadex(NULL, 0u, &upd_check_thread, p_arg, 0u, NULL);
    if (t != 0u) {
        (void)CloseHandle((HANDLE)t);
    } else {
        (void)HeapFree(GetProcessHeap(), 0u, p_arg);
    }
}
