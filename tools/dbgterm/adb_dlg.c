/**
 * @file    adb_dlg.c
 * @author  herongshun
 * @date    2026-11-16
 * @brief   ADB 连接配置对话框实现(纯代码创建, 无资源模板)
 *          控件: adb路径+浏览 / 设备下拉+刷新 / 自动命令x3 / 连接+取消
 *          模态实现: 禁用owner + IsDialogMessage消息循环
 */

#include <windows.h>
#include <commdlg.h>
#include <stdint.h>
#include <string.h>
#include "dbg_source.h"
#include "adb_core.h"
#include "adb_dlg.h"

/* -------------------------------------------------------------------------- */
/*  宏定义: 控件ID                                                             */
/* -------------------------------------------------------------------------- */
#define ADB_DLG_IDC_ST_PATH     4001u  /**< 路径标签 */
#define ADB_DLG_IDC_ED_PATH     4002u  /**< 路径输入框 */
#define ADB_DLG_IDC_BTN_BROWSE  4003u  /**< 浏览按钮 */
#define ADB_DLG_IDC_ST_DEV      4004u  /**< 设备标签 */
#define ADB_DLG_IDC_CB_DEV      4005u  /**< 设备下拉框 */
#define ADB_DLG_IDC_BTN_REFRESH 4006u  /**< 刷新按钮 */
#define ADB_DLG_IDC_ST_CMD      4007u  /**< 自动命令标签 */
#define ADB_DLG_IDC_ED_CMD1     4008u  /**< 自动命令1 */
#define ADB_DLG_IDC_ED_CMD2     4009u  /**< 自动命令2 */
#define ADB_DLG_IDC_ED_CMD3     4010u  /**< 自动命令3 */
#define ADB_DLG_IDC_BTN_CONNECT 4011u  /**< 连接按钮 */
#define ADB_DLG_IDC_BTN_CANCEL  4012u  /**< 取消按钮 */
#define ADB_DLG_IDC_ST_HINT     4013u  /**< 提示文本 */
#define ADB_DLG_IDC_ST_USER     4014u  /**< 账号标签 */
#define ADB_DLG_IDC_ED_USER     4015u  /**< 账号输入框 */
#define ADB_DLG_IDC_ST_PWD      4016u  /**< 密码标签 */
#define ADB_DLG_IDC_ED_PWD      4017u  /**< 密码输入框(掩码) */
#define ADB_DLG_IDC_CK_RELOGIN  4018u  /**< 失败自动重登开关 */
#define ADB_DLG_IDC_ST_IVL      4019u  /**< 重登间隔标签 */
#define ADB_DLG_IDC_ED_IVL      4020u  /**< 重登间隔输入框(秒) */
#define ADB_DLG_IDC_ST_LDELAY   4021u  /**< 登录延迟标签 */
#define ADB_DLG_IDC_ED_LDELAY   4022u  /**< 登录延迟输入框(秒) */

/* -------------------------------------------------------------------------- */
/*  宏定义: 窗口与布局                                                         */
/* -------------------------------------------------------------------------- */
#define ADB_DLG_CLASS_NAME   L"TS_ADB_DLG_WND"  /**< 窗口类名 */
#define ADB_DLG_WND_TITLE    L"ADB 连接"         /**< 窗口标题 */
#define ADB_DLG_WND_WIDTH    560u                /**< 窗口宽度 */
#define ADB_DLG_WND_HEIGHT   448u                /**< 窗口高度 */
#define ADB_DLG_MARGIN       16u                 /**< 四周边距 */
#define ADB_DLG_LABEL_W      90u                 /**< 标签列宽 */
#define ADB_DLG_CTRL_H       26u                 /**< 控件高度 */
#define ADB_DLG_ROW_GAP      12u                 /**< 行距 */
#define ADB_DLG_ROW_PATH_Y   18u                 /**< 路径行Y */
#define ADB_DLG_ROW_DEV_Y    54u                 /**< 设备行Y */
#define ADB_DLG_ROW_USER_Y   90u                 /**< 账号行Y */
#define ADB_DLG_ROW_PWD_Y    126u                /**< 密码行Y */
#define ADB_DLG_ROW_CMD_Y    166u                /**< 自动命令首行Y */
#define ADB_DLG_CMD_STEP     34u                 /**< 命令行步进 */
#define ADB_DLG_ROW_RELOGIN_Y 268u               /**< 重登行Y */
#define ADB_DLG_ROW_LDELAY_Y  306u               /**< 登录延迟行Y */
#define ADB_DLG_ROW_BTN_Y    344u                /**< 按钮行Y */
#define ADB_DLG_ROW_HINT_Y   384u                /**< 提示行Y */
#define ADB_DLG_BTN_W        100u                /**< 按钮宽度 */
#define ADB_DLG_BTN_SMALL_W  72u                 /**< 浏览/刷新按钮宽 */
#define ADB_DLG_COMBO_H      200u                /**< 下拉展开高度 */
#define ADB_DLG_IVL_W        48u                 /**< 重登间隔输入框宽 */
#define ADB_DLG_CMD_INTERVAL 200u                 /**< 自动命令下发间隔(毫秒) */
#define ADB_DLG_INI_NAME     L"dbgterm.ini"       /**< 配置文件名(exe同目录) */
#define ADB_DLG_INI_SECTION  L"Adb"               /**< 配置节名 */
#define ADB_DLG_DEF_IVL_S    10u                  /**< 重登间隔默认值(秒) */
#define ADB_DLG_DEF_LDELAY_S 3u                   /**< 登录延迟默认值(秒) */
#define ADB_DLG_TIMER_LOAD   1u                   /**< 延迟回填定时器ID */
#define ADB_DLG_TIMER_LOAD_MS 300u                /**< 延迟回填间隔(毫秒) */

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static uint8_t            g_dlg_class_ready = 0u;            /**< 窗口类注册标志 */
static ADB_DEVICE_T       g_dlg_devs[ADB_DEV_LIST_MAX];      /**< 设备枚举缓存 */
static uint32_t           g_dlg_dev_count = 0u;              /**< 设备数量 */
static ADB_DLG_CTX_T      g_dlg_ctx;                         /**< 当前对话框上下文 */

/* 内部私有函数前向声明(定义位于 dlg_parse_number 之后) */
static void dlg_ini_path(wchar_t *p_out, uint32_t max);

/* -------------------------------------------------------------------------- */
/*  内部私有函数                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief   创建对话框子控件(占位尺寸, 固定坐标布局)
 * @param   hwnd 对话框窗口句柄
 * @return  无
 */
static void dlg_create_controls(HWND hwnd)
{
    (void)CreateWindowExW(0u, L"STATIC", L"adb.exe 路径:",
                          (WS_CHILD | WS_VISIBLE | SS_LEFT),
                          (int)ADB_DLG_MARGIN, (int)ADB_DLG_ROW_PATH_Y,
                          (int)ADB_DLG_LABEL_W, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ST_PATH, NULL, NULL);
    (void)CreateWindowExW(0u, L"EDIT", L"",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER),
                          (int)(ADB_DLG_MARGIN + ADB_DLG_LABEL_W), (int)ADB_DLG_ROW_PATH_Y,
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN * 2u - ADB_DLG_LABEL_W - ADB_DLG_BTN_SMALL_W - 8u),
                          (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ED_PATH, NULL, NULL);
    (void)CreateWindowExW(0u, L"BUTTON", L"浏览…",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON),
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN - ADB_DLG_BTN_SMALL_W),
                          (int)(ADB_DLG_ROW_PATH_Y - 1u),
                          (int)ADB_DLG_BTN_SMALL_W, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_BTN_BROWSE, NULL, NULL);

    (void)CreateWindowExW(0u, L"STATIC", L"设备:",
                          (WS_CHILD | WS_VISIBLE | SS_LEFT),
                          (int)ADB_DLG_MARGIN, (int)ADB_DLG_ROW_DEV_Y,
                          (int)ADB_DLG_LABEL_W, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ST_DEV, NULL, NULL);
    (void)CreateWindowExW(0u, L"COMBOBOX", L"",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL),
                          (int)(ADB_DLG_MARGIN + ADB_DLG_LABEL_W), (int)ADB_DLG_ROW_DEV_Y,
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN * 2u - ADB_DLG_LABEL_W - ADB_DLG_BTN_SMALL_W - 8u),
                          (int)ADB_DLG_COMBO_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_CB_DEV, NULL, NULL);
    (void)CreateWindowExW(0u, L"BUTTON", L"刷新",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON),
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN - ADB_DLG_BTN_SMALL_W),
                          (int)(ADB_DLG_ROW_DEV_Y - 1u),
                          (int)ADB_DLG_BTN_SMALL_W, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_BTN_REFRESH, NULL, NULL);

    (void)CreateWindowExW(0u, L"STATIC", L"账号:",
                          (WS_CHILD | WS_VISIBLE | SS_LEFT),
                          (int)ADB_DLG_MARGIN, (int)ADB_DLG_ROW_USER_Y,
                          (int)ADB_DLG_LABEL_W, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ST_USER, NULL, NULL);
    (void)CreateWindowExW(0u, L"EDIT", L"",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER),
                          (int)(ADB_DLG_MARGIN + ADB_DLG_LABEL_W), (int)ADB_DLG_ROW_USER_Y,
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN * 2u - ADB_DLG_LABEL_W),
                          (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ED_USER, NULL, NULL);
    (void)CreateWindowExW(0u, L"STATIC", L"密码:",
                          (WS_CHILD | WS_VISIBLE | SS_LEFT),
                          (int)ADB_DLG_MARGIN, (int)ADB_DLG_ROW_PWD_Y,
                          (int)ADB_DLG_LABEL_W, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ST_PWD, NULL, NULL);
    (void)CreateWindowExW(0u, L"EDIT", L"",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD | WS_BORDER),
                          (int)(ADB_DLG_MARGIN + ADB_DLG_LABEL_W), (int)ADB_DLG_ROW_PWD_Y,
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN * 2u - ADB_DLG_LABEL_W),
                          (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ED_PWD, NULL, NULL);

    (void)CreateWindowExW(0u, L"STATIC", L"自动命令:",
                          (WS_CHILD | WS_VISIBLE | SS_LEFT),
                          (int)ADB_DLG_MARGIN, (int)ADB_DLG_ROW_CMD_Y,
                          (int)ADB_DLG_LABEL_W, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ST_CMD, NULL, NULL);
    (void)CreateWindowExW(0u, L"EDIT", L"logcat -v time",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER),
                          (int)(ADB_DLG_MARGIN + ADB_DLG_LABEL_W), (int)ADB_DLG_ROW_CMD_Y,
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN * 2u - ADB_DLG_LABEL_W),
                          (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ED_CMD1, NULL, NULL);
    (void)CreateWindowExW(0u, L"EDIT", L"",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER),
                          (int)(ADB_DLG_MARGIN + ADB_DLG_LABEL_W),
                          (int)(ADB_DLG_ROW_CMD_Y + ADB_DLG_CMD_STEP),
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN * 2u - ADB_DLG_LABEL_W),
                          (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ED_CMD2, NULL, NULL);
    (void)CreateWindowExW(0u, L"EDIT", L"",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER),
                          (int)(ADB_DLG_MARGIN + ADB_DLG_LABEL_W),
                          (int)(ADB_DLG_ROW_CMD_Y + ADB_DLG_CMD_STEP * 2u),
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN * 2u - ADB_DLG_LABEL_W),
                          (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ED_CMD3, NULL, NULL);

    /* 重登行: [失败自动重登] [重登间隔(秒): N] */
    (void)CreateWindowExW(0u, L"BUTTON", L"失败自动重登",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX),
                          (int)ADB_DLG_MARGIN, (int)(ADB_DLG_ROW_RELOGIN_Y + 2u),
                          (int)130u, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_CK_RELOGIN, NULL, NULL);
    (void)CreateWindowExW(0u, L"STATIC", L"重登间隔(秒):",
                          (WS_CHILD | WS_VISIBLE | SS_LEFT),
                          (int)(ADB_DLG_MARGIN + 140u), (int)(ADB_DLG_ROW_RELOGIN_Y + 2u),
                          (int)110u, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ST_IVL, NULL, NULL);
    (void)CreateWindowExW(0u, L"EDIT", L"10",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | WS_BORDER),
                          (int)(ADB_DLG_MARGIN + 258u), (int)ADB_DLG_ROW_RELOGIN_Y,
                          (int)ADB_DLG_IVL_W, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ED_IVL, NULL, NULL);

    /* 登录延迟行: [登录延迟(秒): N] (账号发出后到密码发出的等待) */
    (void)CreateWindowExW(0u, L"STATIC", L"登录延迟(秒):",
                          (WS_CHILD | WS_VISIBLE | SS_LEFT),
                          (int)ADB_DLG_MARGIN, (int)(ADB_DLG_ROW_LDELAY_Y + 2u),
                          (int)120u, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ST_LDELAY, NULL, NULL);
    (void)CreateWindowExW(0u, L"EDIT", L"3",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | WS_BORDER),
                          (int)(ADB_DLG_MARGIN + 138u), (int)ADB_DLG_ROW_LDELAY_Y,
                          (int)ADB_DLG_IVL_W, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ED_LDELAY, NULL, NULL);

    (void)CreateWindowExW(0u, L"BUTTON", L"连接",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON),
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN - ADB_DLG_BTN_W * 2u - 8u),
                          (int)ADB_DLG_ROW_BTN_Y,
                          (int)ADB_DLG_BTN_W, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_BTN_CONNECT, NULL, NULL);
    (void)CreateWindowExW(0u, L"BUTTON", L"取消",
                          (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON),
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN - ADB_DLG_BTN_W),
                          (int)ADB_DLG_ROW_BTN_Y,
                          (int)ADB_DLG_BTN_W, (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_BTN_CANCEL, NULL, NULL);

    (void)CreateWindowExW(0u, L"STATIC", L"提示: 首次运行 adb 可能需数秒启动服务",
                          (WS_CHILD | WS_VISIBLE | SS_LEFT),
                          (int)ADB_DLG_MARGIN, (int)ADB_DLG_ROW_HINT_Y,
                          (int)(ADB_DLG_WND_WIDTH - ADB_DLG_MARGIN * 2u), (int)ADB_DLG_CTRL_H,
                          hwnd, (HMENU)(uintptr_t)ADB_DLG_IDC_ST_HINT, NULL, NULL);
}

/**
 * @brief   更新提示文本
 * @param   hwnd   对话框窗口句柄
 * @param   p_text 提示文本
 * @return  无
 */
static void dlg_set_hint(HWND hwnd, const wchar_t *p_text)
{
    SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ST_HINT), p_text);
}

/**
 * @brief   浏览选择 adb.exe 并回填路径
 * @param   hwnd 对话框窗口句柄
 * @return  无
 */
static void dlg_on_browse(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t file[ADB_CFG_PATH_WCHAR];

    memset(&ofn, 0, sizeof(ofn));
    memset(&file[0], 0, sizeof(file));
    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_PATH),
                         &file[0], (int32_t)ADB_CFG_PATH_WCHAR);
    ofn.lStructSize = (DWORD)sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"adb.exe\0adb.exe\0EXE\0*.exe\0所有文件\0*.*\0";
    ofn.lpstrFile = &file[0];
    ofn.nMaxFile = (DWORD)ADB_CFG_PATH_WCHAR;
    ofn.Flags = (DWORD)(OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);

    if (GetOpenFileNameW(&ofn) != 0) {
        SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_PATH), &file[0]);
    }
}

/**
 * @brief   枚举设备并填充下拉框
 * @param   hwnd 对话框窗口句柄
 * @return  无
 */
static void dlg_on_refresh(HWND hwnd)
{
    wchar_t path[ADB_CFG_PATH_WCHAR];
    wchar_t item[ADB_DEV_SERIAL_WCHAR + ADB_DEV_STATE_WCHAR + 8u];
    HWND h_combo = NULL;
    uint32_t i = 0u;

    memset(&path[0], 0, sizeof(path));
    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_PATH),
                         &path[0], (int32_t)ADB_CFG_PATH_WCHAR);
    if (path[0] == L'\0') {
        if (SearchPathW(NULL, L"adb.exe", NULL, ADB_CFG_PATH_WCHAR, &path[0], NULL) == 0) {
            dlg_set_hint(hwnd, L"未找到 adb.exe: 请填写路径或点击浏览");
            return;
        }
        SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_PATH), &path[0]);
    }

    dlg_set_hint(hwnd, L"正在枚举设备(首次运行 adb 可能需数秒)…");
    UpdateWindow(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ST_HINT));

    g_dlg_dev_count = adb_core_enum_devices(&path[0], &g_dlg_devs[0], ADB_DEV_LIST_MAX);
    h_combo = GetDlgItem(hwnd, (int)ADB_DLG_IDC_CB_DEV);
    SendMessageW(h_combo, CB_RESETCONTENT, 0u, 0u);
    for (i = 0u; i < g_dlg_dev_count; i++) {
        (void)wsprintfW(&item[0], L"%s · %s",
                        &g_dlg_devs[i].serial[0], &g_dlg_devs[i].state[0]);
        SendMessageW(h_combo, CB_ADDSTRING, 0u, (LPARAM)&item[0]);
        SendMessageW(h_combo, CB_SETITEMDATA, (WPARAM)i, (LPARAM)i);
    }
    if (g_dlg_dev_count > 0u) {
        /* 优先选中 ini 记录的上次设备, 无记录则选首项 */
        wchar_t last[ADB_DEV_SERIAL_WCHAR];
        uint32_t picked = 0u;
        uint32_t d = 0u;

        memset(&last[0], 0, sizeof(last));
        {
            wchar_t ini[ADB_CFG_PATH_WCHAR + 16u];
            uint32_t t = 0u;

            memset(&ini[0], 0, sizeof(ini));
            dlg_ini_path(&ini[0], (uint32_t)(sizeof(ini) / sizeof(wchar_t)));
            (void)GetPrivateProfileStringW(ADB_DLG_INI_SECTION, L"Device", L"",
                                           &last[0], (DWORD)ADB_DEV_SERIAL_WCHAR, &ini[0]);
            for (t = 0u; last[t] != L'\0'; t++) {
                if (last[t] == L' ') {
                    last[t] = L'\0';   /* 兼容历史显示文本 */
                    break;
                }
            }
        }
        for (d = 0u; d < g_dlg_dev_count; d++) {
            if ((last[0] != L'\0') && (lstrcmpW(&g_dlg_devs[d].serial[0], &last[0]) == 0)) {
                picked = d;
                break;
            }
        }
        SendMessageW(h_combo, CB_SETCURSEL, (WPARAM)picked, (LPARAM)0);
        dlg_set_hint(hwnd, L"枚举完成");
    } else {
        dlg_set_hint(hwnd, L"未发现设备(检查连接与授权)");
    }
}

/**
 * @brief   获取纯设备序列号(三处共用: 连接/保存/回填)
 *          列表有选中项 -> 原始序列号; 否则取编辑文本并截去首个空格后的
 *          显示后缀(如"serial · device", 序列号不含空格)
 * @param   hwnd  对话框窗口句柄
 * @param   p_out 序列号输出缓冲
 * @param   max   缓冲容量(宽字符)
 * @return  无
 */
static void dlg_get_serial(HWND hwnd, wchar_t *p_out, uint32_t max)
{
    LRESULT sel = -1;
    uint32_t i = 0u;

    memset(p_out, 0, max * sizeof(wchar_t));
    sel = SendMessageW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_CB_DEV), CB_GETCURSEL, 0u, 0u);
    if (sel >= 0) {
        uint32_t idx = (uint32_t)SendMessageW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_CB_DEV),
                                              CB_GETITEMDATA, (WPARAM)sel, 0);

        if (idx < g_dlg_dev_count) {
            lstrcpynW(p_out, &g_dlg_devs[idx].serial[0], (int)max);
            return;
        }
    }
    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_CB_DEV), p_out, (int32_t)max);
    for (i = 0u; p_out[i] != L'\0'; i++) {
        if (p_out[i] == L' ') {
            p_out[i] = L'\0';   /* 截去" · 状态"显示后缀 */
            break;
        }
    }
}
static uint32_t dlg_parse_number(const wchar_t *p_text, uint32_t def_val)
{
    uint32_t value = 0u;
    uint32_t i = 0u;
    uint8_t any_digit = 0u;

    if (p_text == NULL) {
        return def_val;
    }
    for (i = 0u; (p_text[i] >= L'0') && (p_text[i] <= L'9') && (i < 8u); i++) {
        value = (value * 10u) + (uint32_t)(p_text[i] - L'0');
        any_digit = 1u;
    }

    return ((any_digit != 0u) && (value > 0u)) ? value : def_val;
}

/**
 * @brief   获取配置文件完整路径(exe同目录 dbgterm.ini)
 * @param   p_out 路径输出缓冲
 * @param   max   缓冲容量(宽字符)
 * @return  无
 */
static void dlg_ini_path(wchar_t *p_out, uint32_t max)
{
    uint32_t i = 0u;

    memset(p_out, 0, max * sizeof(wchar_t));
    if (GetModuleFileNameW(NULL, p_out, (DWORD)max) == 0u) {
        return;
    }
    /* 截去exe文件名, 拼接ini名 */
    for (i = (uint32_t)lstrlenW(p_out); i > 0u; i--) {
        if ((p_out[i - 1u] == L'\\') || (p_out[i - 1u] == L'/')) {
            p_out[i] = L'\0';
            break;
        }
    }
    (void)lstrcatW(p_out, ADB_DLG_INI_NAME);
}

/**
 * @brief   从 ini 加载上一次连接配置并回填控件(WM_CREATE 调用)
 * @param   hwnd 对话框窗口句柄
 * @return  无
 */
static void dlg_config_load(HWND hwnd)
{
    wchar_t ini[ADB_CFG_PATH_WCHAR + 16u];
    wchar_t text[ADB_CFG_CMD_WCHAR];
    wchar_t num[16u];
    uint32_t value = 0u;

    memset(&ini[0], 0, sizeof(ini));
    dlg_ini_path(&ini[0], (uint32_t)(sizeof(ini) / sizeof(wchar_t)));

    (void)GetPrivateProfileStringW(ADB_DLG_INI_SECTION, L"Path", L"",
                                   &text[0], (DWORD)ADB_CFG_PATH_WCHAR, &ini[0]);
    SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_PATH), &text[0]);

    (void)GetPrivateProfileStringW(ADB_DLG_INI_SECTION, L"Device", L"",
                                   &text[0], (DWORD)ADB_DEV_SERIAL_WCHAR, &ini[0]);
    {
        uint32_t d = 0u;

        for (d = 0u; text[d] != L'\0'; d++) {
            if (text[d] == L' ') {
                text[d] = L'\0';   /* 兼容历史保存的"serial · state"显示文本 */
                break;
            }
        }
    }
    SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_CB_DEV), &text[0]);

    (void)GetPrivateProfileStringW(ADB_DLG_INI_SECTION, L"User", L"",
                                   &text[0], (DWORD)ADB_CFG_LOGIN_WCHAR, &ini[0]);
    SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_USER), &text[0]);

    (void)GetPrivateProfileStringW(ADB_DLG_INI_SECTION, L"Password", L"",
                                   &text[0], (DWORD)ADB_CFG_LOGIN_WCHAR, &ini[0]);
    SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_PWD), &text[0]);

    (void)GetPrivateProfileStringW(ADB_DLG_INI_SECTION, L"AutoCmd1", L"logcat -v time",
                                   &text[0], (DWORD)ADB_CFG_CMD_WCHAR, &ini[0]);
    SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_CMD1), &text[0]);

    (void)GetPrivateProfileStringW(ADB_DLG_INI_SECTION, L"AutoCmd2", L"",
                                   &text[0], (DWORD)ADB_CFG_CMD_WCHAR, &ini[0]);
    SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_CMD2), &text[0]);

    (void)GetPrivateProfileStringW(ADB_DLG_INI_SECTION, L"AutoCmd3", L"",
                                   &text[0], (DWORD)ADB_CFG_CMD_WCHAR, &ini[0]);
    SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_CMD3), &text[0]);

    value = GetPrivateProfileIntW(ADB_DLG_INI_SECTION, L"AutoRelogin", 0u, &ini[0]);
    (void)SendMessageW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_CK_RELOGIN), BM_SETCHECK,
                       (WPARAM)((value != 0u) ? BST_CHECKED : BST_UNCHECKED), 0u);

    value = GetPrivateProfileIntW(ADB_DLG_INI_SECTION, L"ReloginInterval",
                                  ADB_DLG_DEF_IVL_S, &ini[0]);
    (void)wsprintfW(&num[0], L"%u", (unsigned int)value);
    SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_IVL), &num[0]);

    value = GetPrivateProfileIntW(ADB_DLG_INI_SECTION, L"LoginDelay",
                                  ADB_DLG_DEF_LDELAY_S, &ini[0]);
    (void)wsprintfW(&num[0], L"%u", (unsigned int)value);
    SetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_LDELAY), &num[0]);
}

/**
 * @brief   将当前对话框配置保存到 ini(连接成功时调用)
 * @param   hwnd 对话框窗口句柄
 * @return  无
 */
static void dlg_config_save(HWND hwnd)
{
    wchar_t ini[ADB_CFG_PATH_WCHAR + 16u];
    wchar_t text[ADB_CFG_CMD_WCHAR];
    wchar_t num[16u];

    memset(&ini[0], 0, sizeof(ini));
    dlg_ini_path(&ini[0], (uint32_t)(sizeof(ini) / sizeof(wchar_t)));

    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_PATH),
                         &text[0], (int32_t)ADB_CFG_PATH_WCHAR);
    (void)WritePrivateProfileStringW(ADB_DLG_INI_SECTION, L"Path", &text[0], &ini[0]);

    dlg_get_serial(hwnd, &text[0], ADB_DEV_SERIAL_WCHAR);
    (void)WritePrivateProfileStringW(ADB_DLG_INI_SECTION, L"Device", &text[0], &ini[0]);

    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_USER),
                         &text[0], (int32_t)ADB_CFG_LOGIN_WCHAR);
    (void)WritePrivateProfileStringW(ADB_DLG_INI_SECTION, L"User", &text[0], &ini[0]);

    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_PWD),
                         &text[0], (int32_t)ADB_CFG_LOGIN_WCHAR);
    (void)WritePrivateProfileStringW(ADB_DLG_INI_SECTION, L"Password", &text[0], &ini[0]);

    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_CMD1),
                         &text[0], (int32_t)ADB_CFG_CMD_WCHAR);
    (void)WritePrivateProfileStringW(ADB_DLG_INI_SECTION, L"AutoCmd1", &text[0], &ini[0]);

    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_CMD2),
                         &text[0], (int32_t)ADB_CFG_CMD_WCHAR);
    (void)WritePrivateProfileStringW(ADB_DLG_INI_SECTION, L"AutoCmd2", &text[0], &ini[0]);

    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_CMD3),
                         &text[0], (int32_t)ADB_CFG_CMD_WCHAR);
    (void)WritePrivateProfileStringW(ADB_DLG_INI_SECTION, L"AutoCmd3", &text[0], &ini[0]);

    (void)wsprintfW(&num[0], L"%u",
                    (unsigned int)((SendMessageW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_CK_RELOGIN),
                                                 BM_GETCHECK, 0u, 0u) == BST_CHECKED) ? 1u : 0u));
    (void)WritePrivateProfileStringW(ADB_DLG_INI_SECTION, L"AutoRelogin", &num[0], &ini[0]);

    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_IVL),
                         &num[0], (int32_t)16);
    (void)WritePrivateProfileStringW(ADB_DLG_INI_SECTION, L"ReloginInterval", &num[0], &ini[0]);

    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_LDELAY),
                         &num[0], (int32_t)16);
    (void)WritePrivateProfileStringW(ADB_DLG_INI_SECTION, L"LoginDelay", &num[0], &ini[0]);
}

/**
 * @brief   收集配置并建立连接, 成功则关闭对话框
 * @param   hwnd 对话框窗口句柄
 * @return  无
 */
static void dlg_on_connect(HWND hwnd)
{
    const DBG_SOURCE_T *p_src = NULL;
    ADB_CFG_T cfg;
    wchar_t path[ADB_CFG_PATH_WCHAR];
    wchar_t cmd[ADB_CFG_CMD_WCHAR];
    wchar_t hint_buf[128];
    LRESULT sel = -1;
    uint32_t dev_idx = 0u;

    memset(&cfg, 0, sizeof(cfg));
    memset(&path[0], 0, sizeof(path));
    memset(&hint_buf[0], 0, sizeof(hint_buf));

    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_PATH),
                         &path[0], (int32_t)ADB_CFG_PATH_WCHAR);
    if (path[0] == L'\0') {
        dlg_set_hint(hwnd, L"请填写 adb.exe 路径");
        return;
    }
    if (GetFileAttributesW(&path[0]) == INVALID_FILE_ATTRIBUTES) {
        dlg_set_hint(hwnd, L"adb.exe 不存在, 请检查路径");
        return;
    }
    sel = SendMessageW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_CB_DEV), CB_GETCURSEL, 0u, 0u);
    if (sel >= 0) {
        dev_idx = (uint32_t)SendMessageW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_CB_DEV),
                                         CB_GETITEMDATA, (WPARAM)sel, 0);
        if (dev_idx < g_dlg_dev_count) {
            if (lstrcmpW(&g_dlg_devs[dev_idx].state[0], L"device") != 0) {
                (void)wsprintfW(&hint_buf[0], L"设备状态为 %s: 请重插USB/重启设备/完成授权后刷新",
                                &g_dlg_devs[dev_idx].state[0]);
                dlg_set_hint(hwnd, &hint_buf[0]);
                return;
            }
        }
    }
    /* 纯序列号解析: 列表选中取原始值, 手输文本截去显示后缀 */
    dlg_get_serial(hwnd, &cfg.device_serial[0], ADB_CFG_SERIAL_WCHAR);
    if (cfg.device_serial[0] == L'\0') {
        dlg_set_hint(hwnd, L"请先点击[刷新]选择设备, 或直接输入序列号");
        return;
    }

    p_src = dbg_source_get((uint32_t)DBG_SRC_ADB);
    if (p_src == NULL) {
        dlg_set_hint(hwnd, L"数据源不可用");
        return;
    }

    lstrcpynW(&cfg.adb_path[0], &path[0], (int)ADB_CFG_PATH_WCHAR);
    cfg.p_rx = g_dlg_ctx.p_rx;
    cfg.p_test_cmdline = NULL;
    cfg.auto_cmd_interval_ms = ADB_DLG_CMD_INTERVAL;

    /* 登录账号/密码与自动重登配置 */
    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_USER),
                         &cfg.login_user[0], (int32_t)ADB_CFG_LOGIN_WCHAR);
    (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_PWD),
                         &cfg.login_password[0], (int32_t)ADB_CFG_LOGIN_WCHAR);
    cfg.auto_relogin = (SendMessageW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_CK_RELOGIN),
                                     BM_GETCHECK, 0u, 0u) == BST_CHECKED) ? 1u : 0u;
    {
        wchar_t ivl_text[16];

        memset(&ivl_text[0], 0, sizeof(ivl_text));
        (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_IVL),
                             &ivl_text[0], (int32_t)16);
        cfg.relogin_interval_s = dlg_parse_number(&ivl_text[0], ADB_DLG_DEF_IVL_S);
    }
    {
        wchar_t delay_text[16];

        memset(&delay_text[0], 0, sizeof(delay_text));
        (void)GetWindowTextW(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ED_LDELAY),
                             &delay_text[0], (int32_t)16);
        cfg.login_pwd_delay_s = dlg_parse_number(&delay_text[0], ADB_DLG_DEF_LDELAY_S);
    }

    if (GetDlgItemTextW(hwnd, (int)ADB_DLG_IDC_ED_CMD1, &cmd[0],
                        (int32_t)ADB_CFG_CMD_WCHAR) > 0) {
        lstrcpynW(&cfg.auto_cmd[0][0], &cmd[0], (int)ADB_CFG_CMD_WCHAR);
        cfg.auto_cmd_count++;
    }
    if (GetDlgItemTextW(hwnd, (int)ADB_DLG_IDC_ED_CMD2, &cmd[0],
                        (int32_t)ADB_CFG_CMD_WCHAR) > 0) {
        lstrcpynW(&cfg.auto_cmd[1][0], &cmd[0], (int)ADB_CFG_CMD_WCHAR);
        cfg.auto_cmd_count++;
    }
    if (GetDlgItemTextW(hwnd, (int)ADB_DLG_IDC_ED_CMD3, &cmd[0],
                        (int32_t)ADB_CFG_CMD_WCHAR) > 0) {
        lstrcpynW(&cfg.auto_cmd[2][0], &cmd[0], (int)ADB_CFG_CMD_WCHAR);
        cfg.auto_cmd_count++;
    }

    dlg_set_hint(hwnd, L"连接中(含自动命令下发, 可能短暂无响应)…");
    UpdateWindow(GetDlgItem(hwnd, (int)ADB_DLG_IDC_ST_HINT));

    if (p_src->pfn_open(&cfg, g_dlg_ctx.pfn_status, g_dlg_ctx.p_user) != 0) {
        wchar_t err[DBG_SOURCE_STATUS_LEN];
        memset(&err[0], 0, sizeof(err));
        adb_core_get_last_error(&err[0], DBG_SOURCE_STATUS_LEN);
        dlg_set_hint(hwnd, &err[0]);
        return;
    }

    dlg_config_save(hwnd);   /* 连接成功: 持久化本次配置 */
    DestroyWindow(hwnd);   /* 关闭对话框 */
}

/**
 * @brief   对话框消息处理过程
 * @param   hwnd  窗口句柄
 * @param   msg   消息类型
 * @param   wp    消息参数1
 * @param   lp    消息参数2
 * @return  消息处理结果
 */
static LRESULT CALLBACK adb_dlg_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            dlg_create_controls(hwnd);
            return 0;
        }
        case WM_TIMER:
        {
            if ((uint32_t)wp == ADB_DLG_TIMER_LOAD) {
                (void)KillTimer(hwnd, ADB_DLG_TIMER_LOAD);
                dlg_config_load(hwnd);
            }
            return 0;
        }
        case WM_COMMAND:
        {
            uint32_t id = (uint32_t)LOWORD(wp);
            uint32_t code = (uint32_t)HIWORD(wp);

            if (code == BN_CLICKED) {
                switch (id)
                {
                    case ADB_DLG_IDC_BTN_BROWSE:
                    {
                        dlg_on_browse(hwnd);
                        break;
                    }
                    case ADB_DLG_IDC_BTN_REFRESH:
                    {
                        dlg_on_refresh(hwnd);
                        break;
                    }
                    case ADB_DLG_IDC_BTN_CONNECT:
                    {
                        dlg_on_connect(hwnd);
                        break;
                    }
                    case ADB_DLG_IDC_BTN_CANCEL:
                    {
                        DestroyWindow(hwnd);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
            return 0;
        }
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        default:
        {
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void adb_dlg_open(HINSTANCE h_inst, HWND h_owner, const ADB_DLG_CTX_T *p_ctx)
{
    WNDCLASSW wc;
    MSG msg;
    HWND h_dlg = NULL;

    if ((h_inst == NULL) || (p_ctx == NULL)) {
        return;
    }
    g_dlg_ctx = *p_ctx;

    if (g_dlg_class_ready == 0u) {
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = adb_dlg_wnd_proc;
        wc.hInstance = h_inst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = ADB_DLG_CLASS_NAME;
        if (RegisterClassW(&wc) == 0) {
            return;
        }
        g_dlg_class_ready = 1u;
    }

    h_dlg = CreateWindowExW((WS_EX_DLGMODALFRAME), ADB_DLG_CLASS_NAME, ADB_DLG_WND_TITLE,
                            (WS_POPUP | WS_CAPTION | WS_SYSMENU),
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            ADB_DLG_WND_WIDTH, ADB_DLG_WND_HEIGHT,
                            h_owner, NULL, h_inst, NULL);
    if (h_dlg == NULL) {
        return;
    }

    /* 模态: 禁用owner, 独立消息循环至对话框销毁 */
    if (h_owner != NULL) {
        (void)EnableWindow(h_owner, FALSE);
    }
    ShowWindow(h_dlg, SW_SHOWNORMAL);
    UpdateWindow(h_dlg);
    (void)SetForegroundWindow(h_dlg);

    /* 配置回填: 显示后经300ms定时器延迟执行, 确保晚于创建/显示阶段的一切复位 */
    (void)SetTimer(h_dlg, ADB_DLG_TIMER_LOAD, ADB_DLG_TIMER_LOAD_MS, NULL);

    memset(&msg, 0, sizeof(msg));
    while (IsWindow(h_dlg) && (GetMessageW(&msg, NULL, 0, 0) > 0)) {
        if (IsDialogMessageW(h_dlg, &msg) == 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (h_owner != NULL) {
        (void)EnableWindow(h_owner, TRUE);
        (void)SetActiveWindow(h_owner);
    }
}
