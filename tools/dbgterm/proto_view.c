/**
 * @file    proto_view.c
 * @author  herongshun
 * @date    2026-11-21
 * @brief   协议解析窗口实现(调试终端 扩展→协议解析)
 *          布局: 配置区[启用/解析器勾选 | 关键字/向下行数 | 自绘按钮]
 *          + 记录列表 + 详情(只读) + 状态行;
 *          捕获: proto_cap状态机; 解析: 注册回调; 记录: 环形100条(heap);
 *          自动保存: [Proto]Dir目录 proto_auto_*.txt 持续追加(UTF-8 BOM)
 */

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdint.h>
#include <string.h>
#include "ui_common.h"
#include "dbg_conf.h"
#include "proto_cap.h"
#include "proto_view.h"

/* -------------------------------------------------------------------------- */
/*  宏定义: 控件ID                                                             */
/* -------------------------------------------------------------------------- */
#define PV_IDC_CK_ENABLE      6001u  /**< 启用实时解析 */
#define PV_IDC_ED_KW          6002u  /**< 命中关键字输入 */
#define PV_IDC_ED_LINES       6003u  /**< 向下行数输入 */
#define PV_IDC_LIST_REC       6004u  /**< 记录列表 */
#define PV_IDC_ED_DETAIL      6005u  /**< 详情(只读) */
#define PV_IDC_CK_PARSER_BASE 6010u  /**< 解析器勾选基址(6010+idx) */
#define PV_IDC_ST_STATUS      6050u  /**< 状态文本 */

/* 自绘按钮逻辑ID */
#define PV_BTN_CLR            6101u  /**< 清空记录 */
#define PV_BTN_SAVE           6102u  /**< 保存详情 */
#define PV_BTN_DIR            6103u  /**< 打开目录 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 布局与行为                                                         */
/* -------------------------------------------------------------------------- */
#define PV_CLASS_NAME     L"TS_PROTO_VIEW_WND"  /**< 窗口类名 */
#define PV_WND_TITLE      L"协议解析"            /**< 窗口标题 */
#define PV_WND_WIDTH      880u                  /**< 初始宽度 */
#define PV_WND_HEIGHT     620u                  /**< 初始高度 */
#define PV_MIN_TRACK_W    640u                  /**< 最小轨宽 */
#define PV_MIN_TRACK_H    420u                  /**< 最小轨高 */

#define PV_MARGIN         12u     /**< 页边距 */
#define PV_ROW1_Y         12u     /**< 配置行1Y */
#define PV_ROW2_Y         44u     /**< 配置行2Y */
#define PV_CTRL_H         24u     /**< 复选框高 */
#define PV_ED_H           26u     /**< 输入框高 */
#define PV_LIST_Y         80u     /**< 记录列表Y */
#define PV_LIST_H         190u    /**< 记录列表高 */
#define PV_DETAIL_GAP     8u      /**< 列表与详情间距 */
#define PV_STATUS_H       20u     /**< 状态行高 */

#define PV_BTN_W          92u     /**< 自绘按钮宽 */
#define PV_BTN_H          28u     /**< 自绘按钮高 */
#define PV_BTN_GAP        8u      /**< 按钮间距 */
#define PV_BTN_COUNT      3u      /**< 自绘按钮数量 */

#define PV_REC_MAX        100u    /**< 记录上限(最旧淘汰) */
#define PV_REPORT_WCHAR   (64u * 1024u) /**< 解析报告缓冲(宽字符) */
#define PV_DIR_MAX        260u    /**< 保存目录容量 */
#define PV_TEXT_MAX       160u    /**< 通用文本缓冲 */
#define PV_IDLE_FLUSH_MS  600u    /**< 空闲收口阈值(毫秒, 报文一次打完后收口) */

/* ListView 消息/常量(注意A/W为不同消息号, 须用W版; 本地定义避免宏歧义) */
#define PV_LVM_FIRST            0x1000u                       /**< ListView消息基址 */
#define PV_LVM_INSERTCOLUMNW    (PV_LVM_FIRST + 97u)          /**< 插入列(Unicode) */
#define PV_LVM_INSERTITEMW      (PV_LVM_FIRST + 77u)          /**< 插入行(Unicode) */
#define PV_LVM_SETITEMW         (PV_LVM_FIRST + 76u)          /**< 设置子项(Unicode) */
#define PV_LVM_DELETEALLITEMS   (PV_LVM_FIRST + 9u)           /**< 清空全部行 */
#define PV_LVM_GETNEXTITEM      (PV_LVM_FIRST + 12u)          /**< 查选中行 */
#define PV_LVM_SETITEMSTATE     (PV_LVM_FIRST + 43u)          /**< 设置行状态 */
#define PV_LVM_SETEXTLISTSTYLE  (PV_LVM_FIRST + 54u)          /**< 设置扩展样式 */
#define PV_LVS_REPORT           0x0001u                       /**< 报表模式 */
#define PV_LVS_SINGLESEL        0x0004u                       /**< 单选 */
#define PV_LVS_SHOWSELALWAYS    0x0008u                       /**< 始终显示选中 */
#define PV_LVCF_TEXT            0x0004u                       /**< 列文本有效 */
#define PV_LVCF_WIDTH           0x0002u                       /**< 列宽有效 */
#define PV_LVIF_TEXT            0x0001u                       /**< 项文本有效 */
#define PV_LVIF_STATE           0x0008u                       /**< 状态有效 */
#define PV_LVIS_FOCUSED         0x0001u                       /**< 焦点态 */
#define PV_LVIS_SELECTED        0x0002u                       /**< 选中态 */
#define PV_LVNI_SELECTED        0x0002u                       /**< 查找选中条件 */
#define PV_LVS_EX_FULLROWSELECT 0x00000020u                   /**< 整行选中 */
#define PV_LVN_ITEMCHANGED      (0u - 99u)                    /**< 选中行变化通知 */

#define PV_CLR_BTN_BG     0x00F3F4F8  /**< 中性按钮底色 */
#define PV_CLR_BTN_HV     0x00FFFFFF  /**< 中性按钮悬停底色 */
#define PV_CLR_BTN_PR     0x00E7EAF1  /**< 中性按钮按下底色 */
#define PV_CLR_BTN_BORDER 0x00D5DAE4  /**< 中性按钮边框 */
#define PV_CLR_TEXT_DARK  0x00283040  /**< 深色文字 */

/* -------------------------------------------------------------------------- */
/*  内部类型                                                                    */
/* -------------------------------------------------------------------------- */

/** 已注册解析器 */
typedef struct PV_PARSER_TAG
{
    wchar_t               name[PROTO_VIEW_NAME_WCHAR];  /**< 显示名 */
    PROTO_VIEW_PARSE_FN   p_fn;                         /**< 解析回调 */
    uint8_t               checked;                      /**< 勾选状态 */
} PV_PARSER_T;

/** 单条解析记录(堆内容, 环形淘汰时释放) */
typedef struct PV_REC_TAG
{
    wchar_t  *p_detail;                  /**< 详情全文(原始hex+报告, heap) */
    wchar_t  time_str[12u];              /**< 列表列: 捕获时刻 HH:MM:SS */
    uint32_t bytes;                      /**< 列表列: 报文字节数 */
    wchar_t  result[PV_TEXT_MAX];        /**< 列表列: 解析结果摘要 */
} PV_REC_T;

/** ListView列定义(LVCOLUMNW布局, 本地定义) */
typedef struct PV_LVCOL_TAG
{
    UINT       mask;          /**< 有效字段掩码 */
    int32_t    fmt;           /**< 对齐 */
    int32_t    cx;            /**< 列宽 */
    wchar_t   *pszText;       /**< 列标题 */
    int32_t    cchTextMax;    /**< 文本容量 */
    int32_t    iSubItem;      /**< 列索引 */
    int32_t    iImage;        /**< 图像 */
    int32_t    iOrder;        /**< 排序序号 */
    int32_t    cxMin;         /**< 最小宽 */
    int32_t    cxDefault;     /**< 缺省宽 */
    int32_t    cxIdeal;       /**< 理想宽 */
} PV_LVCOL_T;

/** ListView项(LVITEMW布局, 本地定义) */
typedef struct PV_LVITEM_TAG
{
    UINT       mask;          /**< 有效字段掩码 */
    int32_t    iItem;         /**< 行索引 */
    int32_t    iSubItem;      /**< 列索引 */
    UINT       state;         /**< 状态 */
    UINT       stateMask;     /**< 状态掩码 */
    wchar_t   *pszText;       /**< 项文本 */
    int32_t    cchTextMax;    /**< 文本容量 */
    int32_t    iImage;        /**< 图像 */
    LPARAM     lParam;        /**< 附加参数 */
    int32_t    iIndent;       /**< 缩进 */
    int32_t    iGroupId;      /**< 组 */
    UINT       cColumns;      /**< 列数 */
    UINT      *puColumns;     /**< 列数组 */
} PV_LVITEM_T;

/** 通用通知头(NMHDR布局, 本地定义) */
typedef struct PV_NMHDR_TAG
{
    HWND       hwndFrom;      /**< 来源控件 */
    UINT_PTR   idFrom;        /**< 来源ID */
    UINT       code;          /**< 通知码 */
} PV_NMHDR_T;

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static uint8_t   g_pv_class_ready = 0u;             /**< 窗口类注册标志 */
static HINSTANCE g_pv_instance = NULL;              /**< 应用实例句柄 */
static HWND      g_pv_hwnd = NULL;                  /**< 协议解析窗口句柄 */

static PV_PARSER_T g_pv_parsers[PROTO_VIEW_PARSER_MAX] = {0};  /**< 解析器表 */
static uint32_t  g_pv_parser_count = 0u;            /**< 已注册解析器数 */

static uint8_t   g_pv_enabled = 0u;                 /**< 启用实时解析 */
static PROTO_CAP_T g_pv_cap;                        /**< 捕获状态机 */
static PROTO_CAP_CFG_T g_pv_cfg;                    /**< 捕获配置 */

static PV_REC_T  g_pv_recs[PV_REC_MAX] = {0};       /**< 记录环形数组 */
static uint32_t  g_pv_rec_count = 0u;               /**< 当前记录数 */
static uint32_t  g_pv_rec_head = 0u;                /**< 最旧记录物理槽位 */
static uint32_t  g_pv_hit_total = 0u;               /**< 累计命中条数 */
static uint32_t  g_pv_last_feed_tick = 0u;          /**< 最近喂行时刻(空闲收口用) */

static wchar_t   g_pv_dir[PV_DIR_MAX] = L"";        /**< 自动保存目录 */
static uint8_t   g_pv_autosave = 1u;                /**< 自动保存开关 */
static wchar_t   g_pv_save_path[PV_DIR_MAX] = L"";  /**< 当前保存文件全路径 */
static uint8_t   g_pv_file_bom_done = 0u;           /**< 当前文件已写BOM */

static uint32_t  g_pv_hover_id = 0u;                /**< 悬停按钮ID */
static uint32_t  g_pv_press_id = 0u;                /**< 按下按钮ID */
static uint8_t   g_pv_mouse_tracking = 0u;          /**< 鼠标离开跟踪标志 */

static RECT g_pv_btn_rects[PV_BTN_COUNT] = {0};     /**< 自绘按钮矩形 */

static const uint32_t g_pv_btn_ids[PV_BTN_COUNT] =
{
    PV_BTN_CLR, PV_BTN_SAVE, PV_BTN_DIR
};

static const wchar_t *const g_pv_btn_labels[PV_BTN_COUNT] =
{
    L"清空记录", L"保存详情", L"打开目录"
};

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 记录与保存                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief   取记录环形数组逻辑槽位
 * @param   idx 逻辑索引(0=最旧)
 * @return  物理槽位
 */
static uint32_t pv_slot(uint32_t idx)
{
    return ((g_pv_rec_head + idx) % PV_REC_MAX);
}

/**
 * @brief   释放记录详情堆内存
 * @param   p_rec 记录
 * @return  无
 */
static void pv_rec_free(PV_REC_T *p_rec)
{
    if (p_rec->p_detail != NULL) {
        (void)HeapFree(GetProcessHeap(), 0u, p_rec->p_detail);
        p_rec->p_detail = NULL;
    }
}

/**
 * @brief   确保自动保存目录存在(缺省 exe目录\proto_log)
 * @return  无
 */
static void pv_ensure_dir(void)
{
    wchar_t exe_dir[PV_DIR_MAX];
    wchar_t def_dir[PV_DIR_MAX + 32u];
    int32_t i = 0u;
    int32_t last_slash = -1;

    memset(&exe_dir[0], 0, sizeof(exe_dir));
    memset(&def_dir[0], 0, sizeof(def_dir));
    dbg_conf_get_str(L"Proto", L"Dir", L"", &g_pv_dir[0], PV_DIR_MAX);
    if (g_pv_dir[0] == L'\0') {
        (void)GetModuleFileNameW(NULL, &exe_dir[0], PV_DIR_MAX);
        for (i = 0u; exe_dir[i] != L'\0'; i++) {
            if (exe_dir[i] == L'\\') {
                last_slash = i;
            }
        }
        if (last_slash >= 0) {
            exe_dir[last_slash] = L'\0';
        }
        (void)wsprintfW(&def_dir[0], L"%s\\proto_log", &exe_dir[0]);
        lstrcpynW(&g_pv_dir[0], &def_dir[0], PV_DIR_MAX);
    }
    CreateDirectoryW(&g_pv_dir[0], NULL);
}

/**
 * @brief   生成本次会话的自动保存文件路径并重置BOM标记
 * @return  无
 */
static void pv_reset_save_path(void)
{
    SYSTEMTIME st;

    memset(&st, 0, sizeof(st));
    GetLocalTime(&st);
    (void)wsprintfW(&g_pv_save_path[0], L"%s\\proto_auto_%04u%02u%02u_%02u%02u%02u.txt",
                    &g_pv_dir[0], (unsigned int)st.wYear, (unsigned int)st.wMonth,
                    (unsigned int)st.wDay, (unsigned int)st.wHour,
                    (unsigned int)st.wMinute, (unsigned int)st.wSecond);
    g_pv_file_bom_done = 0u;
}

/**
 * @brief   追加一块UTF-8文本到自动保存文件(首块带BOM)
 * @param   p_text 宽字符文本
 * @return  1: 已写入; 0: 失败
 */
static uint8_t pv_append_file(const wchar_t *p_text)
{
    HANDLE h_file = NULL;
    DWORD written = 0u;
    int32_t need = 0u;
    char *p_utf8 = NULL;
    static const char BOM[3u] = {(char)0xEF, (char)0xBB, (char)0xBF};

    if ((g_pv_autosave == 0u) || (g_pv_save_path[0] == L'\0')) {
        return 0u;
    }
    need = WideCharToMultiByte(CP_UTF8, 0u, p_text, -1, NULL, 0u, NULL, NULL);
    if (need <= 0) {
        return 0u;
    }
    p_utf8 = (char *)HeapAlloc(GetProcessHeap(), 0u, (SIZE_T)need + 4u);
    if (p_utf8 == NULL) {
        return 0u;
    }
    (void)WideCharToMultiByte(CP_UTF8, 0u, p_text, -1, &p_utf8[3u], need,
                              NULL, NULL);
    memcpy(&p_utf8[0], &BOM[0], 3u);

    h_file = CreateFileW(&g_pv_save_path[0], (GENERIC_WRITE), 0u, NULL,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h_file == INVALID_HANDLE_VALUE) {
        (void)HeapFree(GetProcessHeap(), 0u, p_utf8);
        return 0u;
    }
    (void)SetFilePointer(h_file, 0u, NULL, FILE_END);
    if (g_pv_file_bom_done == 0u) {
        (void)WriteFile(h_file, &BOM[0], 3u, &written, NULL);
        g_pv_file_bom_done = 1u;
    }
    (void)WriteFile(h_file, &p_utf8[3u], (DWORD)(need - 1u), &written, NULL);
    (void)CloseHandle(h_file);
    (void)HeapFree(GetProcessHeap(), 0u, p_utf8);

    return 1u;
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: UI 辅助                                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief   创建子控件(字体统一)
 * @param   hwnd      父窗口
 * @param   p_class   类名
 * @param   style     样式
 * @param   ctrl_id   控件ID
 * @param   font_kind 字体
 * @return  控件句柄
 */
static HWND pv_create_child(HWND hwnd, const wchar_t *p_class, uint32_t style,
                            uint32_t ctrl_id, uint32_t font_kind)
{
    HWND h_ctrl = CreateWindowExW(0u, p_class, L"", (WS_CHILD | WS_VISIBLE | style),
                                  0, 0, 10, 10, hwnd, (HMENU)(uintptr_t)ctrl_id,
                                  g_pv_instance, NULL);

    if (h_ctrl != NULL) {
        ui_common_apply_font_kind(h_ctrl, font_kind);
    }

    return h_ctrl;
}

/**
 * @brief   刷新记录表格(全量重建)与状态行
 * @param   hwnd 窗口句柄
 * @return  无
 */
static void pv_refresh_list(HWND hwnd)
{
    HWND h_list = GetDlgItem(hwnd, (int)PV_IDC_LIST_REC);
    uint32_t i = 0u;

    (void)SendMessageW(h_list, PV_LVM_DELETEALLITEMS, 0u, 0u);
    for (i = 0u; i < g_pv_rec_count; i++) {
        const PV_REC_T *p_rec = &g_pv_recs[pv_slot(i)];
        wchar_t cell[PV_TEXT_MAX];
        PV_LVITEM_T item;

        memset(&item, 0u, sizeof(item));
        item.mask = PV_LVIF_TEXT;
        item.iItem = (int32_t)i;
        item.iSubItem = 0;
        (void)wsprintfW(&cell[0], L"#%03u", (unsigned int)(i + 1u));
        item.pszText = &cell[0];
        (void)SendMessageW(h_list, PV_LVM_INSERTITEMW, 0u, (LPARAM)&item);

        item.iSubItem = 1;
        item.pszText = (wchar_t *)&p_rec->time_str[0];
        (void)SendMessageW(h_list, PV_LVM_SETITEMW, 0u, (LPARAM)&item);
        item.iSubItem = 2;
        (void)wsprintfW(&cell[0], L"%uB", (unsigned int)p_rec->bytes);
        item.pszText = &cell[0];
        (void)SendMessageW(h_list, PV_LVM_SETITEMW, 0u, (LPARAM)&item);
        item.iSubItem = 3;
        item.pszText = (wchar_t *)&p_rec->result[0];
        (void)SendMessageW(h_list, PV_LVM_SETITEMW, 0u, (LPARAM)&item);
    }
    if (g_pv_rec_count > 0u) {
        PV_LVITEM_T sel_item;

        memset(&sel_item, 0u, sizeof(sel_item));
        sel_item.mask = PV_LVIF_STATE;
        sel_item.iItem = (int32_t)(g_pv_rec_count - 1u);
        sel_item.state = (PV_LVIS_SELECTED | PV_LVIS_FOCUSED);
        sel_item.stateMask = (PV_LVIS_SELECTED | PV_LVIS_FOCUSED);
        (void)SendMessageW(h_list, PV_LVM_SETITEMSTATE,
                           (WPARAM)(g_pv_rec_count - 1u), (LPARAM)&sel_item);
    }
    {
        wchar_t text[PV_TEXT_MAX];
        HWND h_st = GetDlgItem(hwnd, (int)PV_IDC_ST_STATUS);

        (void)wsprintfW(&text[0], L"命中 %u 条 | 保存: %s",
                        (unsigned int)g_pv_hit_total,
                        (g_pv_autosave != 0u) ? &g_pv_save_path[0] : L"(关闭)");
        SetWindowTextW(h_st, &text[0]);
    }
}

/**
 * @brief   详情写入详情区(选中记录或最新记录)
 * @param   hwnd 窗口句柄
 * @return  无
 */
static void pv_show_detail(HWND hwnd)
{
    HWND h_list = GetDlgItem(hwnd, (int)PV_IDC_LIST_REC);
    LRESULT sel = SendMessageW(h_list, PV_LVM_GETNEXTITEM, (WPARAM)-1,
                               (LPARAM)PV_LVNI_SELECTED);
    const wchar_t *p_text = L"(无记录)";

    if (sel < 0) {
        sel = (LRESULT)((g_pv_rec_count > 0u) ? (g_pv_rec_count - 1u) : 0u);
    }
    if (((uint32_t)sel < g_pv_rec_count) && (g_pv_rec_count > 0u)) {
        const PV_REC_T *p_rec = &g_pv_recs[pv_slot((uint32_t)sel)];

        if ((p_rec->p_detail != NULL) && (p_rec->p_detail[0] != L'\0')) {
            p_text = p_rec->p_detail;
        }
    }
    SetWindowTextW(GetDlgItem(hwnd, (int)PV_IDC_ED_DETAIL), p_text);
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 捕获落地                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief   组包完成: 生成详情文本, 入库, 自动保存, 刷新UI
 * @param   hwnd 窗口句柄(可为NULL, 仅落地不刷UI)
 * @param   p_data 报文字节
 * @param   len    字节数
 * @return  无
 */
static void pv_commit(HWND hwnd, const uint8_t *p_data, uint32_t len)
{
    static wchar_t report[PV_REPORT_WCHAR];
    static wchar_t detail[PV_REPORT_WCHAR + 8192u];
    SYSTEMTIME st;
    uint32_t pos = 0u;
    uint32_t i = 0u;
    uint32_t total_frames = 0u;
    PV_REC_T *p_rec = NULL;
    const wchar_t *p_first_name = L"";

    memset(&report[0], 0, sizeof(report));
    memset(&detail[0], 0, sizeof(detail));
    memset(&st, 0, sizeof(st));
    GetLocalTime(&st);

    /* 详情头 */
    pos += (uint32_t)wsprintfW(&detail[pos], L"==== #%03u %02u:%02u:%02u.%03u (%u字节) ====\r\n",
                               (unsigned int)(g_pv_hit_total + 1u),
                               (unsigned int)st.wHour, (unsigned int)st.wMinute,
                               (unsigned int)st.wSecond, (unsigned int)st.wMilliseconds,
                               (unsigned int)len);
    pos += (uint32_t)wsprintfW(&detail[pos], L"原始: ");
    for (i = 0u; (i < len) && (pos < (PV_REPORT_WCHAR + 4096u)); i++) {
        pos += (uint32_t)wsprintfW(&detail[pos], L"%02X", (unsigned int)p_data[i]);
        if (((i % 64u) == 63u) || ((i + 1u) == len)) {
            pos += (uint32_t)wsprintfW(&detail[pos], L"\r\n");
            if ((i + 1u) != len) {
                pos += (uint32_t)wsprintfW(&detail[pos], L"      ");
            }
        }
    }

    /* 逐个勾选解析器解析 */
    for (i = 0u; i < g_pv_parser_count; i++) {
        if ((g_pv_parsers[i].checked != 0u) && (g_pv_parsers[i].p_fn != NULL)) {
            uint32_t frames = g_pv_parsers[i].p_fn(p_data, len, &report[0],
                                                   PV_REPORT_WCHAR);

            pos += (uint32_t)wsprintfW(&detail[pos], L"---- 解析(%s, %u帧) ----\r\n",
                                       &g_pv_parsers[i].name[0], (unsigned int)frames);
            if ((pos + (uint32_t)lstrlenW(&report[0]) + 4u) <
                (uint32_t)(sizeof(detail) / sizeof(detail[0]))) {
                pos += (uint32_t)wsprintfW(&detail[pos], L"%s", &report[0]);
            }
            total_frames += frames;
            if (p_first_name[0] == L'\0') {
                p_first_name = &g_pv_parsers[i].name[0];
            }
        }
    }
    if (pos == 0u) {
        return;
    }

    /* 入库(满则淘汰最旧) */
    if (g_pv_rec_count >= PV_REC_MAX) {
        pv_rec_free(&g_pv_recs[g_pv_rec_head]);
        g_pv_rec_head = ((g_pv_rec_head + 1u) % PV_REC_MAX);
        g_pv_rec_count--;
    }
    p_rec = &g_pv_recs[pv_slot(g_pv_rec_count)];
    pv_rec_free(p_rec);
    p_rec->p_detail = (wchar_t *)HeapAlloc(GetProcessHeap(), 0u,
                                           ((SIZE_T)pos + 2u) * sizeof(wchar_t));
    if (p_rec->p_detail == NULL) {
        return;
    }
    memcpy(p_rec->p_detail, &detail[0], ((SIZE_T)pos + 1u) * sizeof(wchar_t));
    (void)wsprintfW(&p_rec->time_str[0], L"%02u:%02u:%02u",
                    (unsigned int)st.wHour, (unsigned int)st.wMinute,
                    (unsigned int)st.wSecond);
    p_rec->bytes = len;
    if (p_first_name[0] == L'\0') {
        lstrcpynW(&p_rec->result[0], L"原始(未解析)", PV_TEXT_MAX);
    } else if (total_frames > 0u) {
        (void)wsprintfW(&p_rec->result[0], L"%s: %u帧 通过", p_first_name,
                        (unsigned int)total_frames);
    } else {
        (void)wsprintfW(&p_rec->result[0], L"%s: 失败(见详情)", p_first_name);
    }
    g_pv_rec_count++;
    g_pv_hit_total++;

    /* 自动保存 */
    (void)pv_append_file(&detail[0]);

    if (hwnd != NULL) {
        pv_refresh_list(hwnd);
        pv_show_detail(hwnd);
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 自绘按钮                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief   绘制单个自绘按钮(中性)
 * @param   hdc   目标设备上下文
 * @param   index 按钮索引
 * @return  无
 */
static void pv_draw_button(HDC hdc, uint32_t index)
{
    uint32_t ctrl_id = g_pv_btn_ids[index];
    uint8_t hovered = ((g_pv_hover_id == ctrl_id) ? 1u : 0u);
    uint8_t pressed = ((g_pv_press_id == ctrl_id) ? 1u : 0u);
    uint32_t fill_clr = PV_CLR_BTN_BG;
    RECT rc = g_pv_btn_rects[index];
    HFONT h_font_old = NULL;

    if (pressed != 0u) {
        fill_clr = PV_CLR_BTN_PR;
    } else if (hovered != 0u) {
        fill_clr = PV_CLR_BTN_HV;
    }
    SetBkMode(hdc, TRANSPARENT);
    ui_common_fill_round_rect(hdc, &rc, 8u, fill_clr, PV_CLR_BTN_BORDER, 1u);
    InflateRect(&rc, -6, -4);
    h_font_old = (HFONT)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_BTN));
    SetTextColor(hdc, (COLORREF)PV_CLR_TEXT_DARK);
    ui_common_text_crisp(hdc, &rc, g_pv_btn_labels[index],
                         (DT_CENTER | DT_VCENTER | DT_SINGLELINE), fill_clr);
    (void)SelectObject(hdc, h_font_old);
}

/**
 * @brief   命中测试
 * @param   pt 客户区坐标
 * @return  按钮逻辑ID; 0=无
 */
static uint32_t pv_hit_btn(POINT pt)
{
    uint32_t index = 0u;

    for (index = 0u; index < PV_BTN_COUNT; index++) {
        if (PtInRect(&g_pv_btn_rects[index], pt) != 0) {
            return g_pv_btn_ids[index];
        }
    }

    return 0u;
}

/**
 * @brief   使指定按钮重绘
 * @param   hwnd    窗口句柄
 * @param   ctrl_id 按钮ID
 * @return  无
 */
static void pv_invalidate_btn(HWND hwnd, uint32_t ctrl_id)
{
    uint32_t index = 0u;

    if (ctrl_id == 0u) {
        return;
    }
    for (index = 0u; index < PV_BTN_COUNT; index++) {
        if (g_pv_btn_ids[index] == ctrl_id) {
            InvalidateRect(hwnd, &g_pv_btn_rects[index], FALSE);
            return;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 布局与控件                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief   创建全部子控件
 * @param   hwnd 窗口句柄
 * @return  无
 */
static void pv_create_controls(HWND hwnd)
{
    uint32_t i = 0u;

    (void)pv_create_child(hwnd, L"BUTTON", BS_AUTOCHECKBOX, PV_IDC_CK_ENABLE,
                          UI_FONT_KIND_BTN);
    SetWindowTextW(GetDlgItem(hwnd, (int)PV_IDC_CK_ENABLE), L"启用实时解析");
    for (i = 0u; i < g_pv_parser_count; i++) {
        HWND h_ck = pv_create_child(hwnd, L"BUTTON", BS_AUTOCHECKBOX,
                                    (PV_IDC_CK_PARSER_BASE + i), UI_FONT_KIND_BTN);

        SetWindowTextW(h_ck, &g_pv_parsers[i].name[0]);
        (void)SendMessageW(h_ck, BM_SETCHECK,
                           (g_pv_parsers[i].checked != 0u) ? BST_CHECKED : BST_UNCHECKED,
                           0u);
    }
    if (g_pv_parser_count == 0u) {
        HWND h_st = pv_create_child(hwnd, L"STATIC", SS_LEFT, PV_IDC_ST_STATUS + 1u,
                                    UI_FONT_KIND_BTN);

        SetWindowTextW(h_st, L"(未注册解析器: 独立版dbgterm无协议解析, 合集版可用)");
    }

    (void)pv_create_child(hwnd, L"STATIC", SS_LEFT, 6060u, UI_FONT_KIND_BTN);
    SetWindowTextW(GetDlgItem(hwnd, (int)6060u), L"命中关键字:");
    (void)pv_create_child(hwnd, L"EDIT",
                          (uint32_t)(ES_AUTOHSCROLL | WS_BORDER), PV_IDC_ED_KW,
                          UI_FONT_KIND_EDIT);
    (void)pv_create_child(hwnd, L"STATIC", SS_LEFT, 6061u, UI_FONT_KIND_BTN);
    SetWindowTextW(GetDlgItem(hwnd, (int)6061u), L"向下行数:");
    (void)pv_create_child(hwnd, L"EDIT",
                          (uint32_t)(ES_AUTOHSCROLL | ES_NUMBER | WS_BORDER),
                          PV_IDC_ED_LINES, UI_FONT_KIND_EDIT);

    /* 记录表格(ListView报表模式: 带表头, 序号/时间/字节/结果) */
    {
        HWND h_list = pv_create_child(hwnd, L"SysListView32",
                                      (uint32_t)(WS_BORDER | WS_VSCROLL |
                                                 PV_LVS_REPORT | PV_LVS_SINGLESEL |
                                                 PV_LVS_SHOWSELALWAYS),
                                      PV_IDC_LIST_REC, UI_FONT_KIND_EDIT);
        static const wchar_t *const col_titles[4u] =
        {
            L"序号", L"时间", L"字节", L"结果"
        };
        static const int32_t col_widths[4u] = {64, 90, 72, 460};
        uint32_t c = 0u;

        for (c = 0u; c < 4u; c++) {
            PV_LVCOL_T col;

            memset(&col, 0u, sizeof(col));
            col.mask = (PV_LVCF_TEXT | PV_LVCF_WIDTH);
            col.pszText = (wchar_t *)col_titles[c];
            col.cx = col_widths[c];
            (void)SendMessageW(h_list, PV_LVM_INSERTCOLUMNW, (WPARAM)c, (LPARAM)&col);
        }
        (void)SendMessageW(h_list, PV_LVM_SETEXTLISTSTYLE,
                           (WPARAM)PV_LVS_EX_FULLROWSELECT,
                           (LPARAM)PV_LVS_EX_FULLROWSELECT);
    }
    (void)pv_create_child(hwnd, L"EDIT",
                          (uint32_t)(ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
                                     WS_VSCROLL | WS_HSCROLL | WS_BORDER),
                          PV_IDC_ED_DETAIL, UI_FONT_KIND_EDIT);
    (void)pv_create_child(hwnd, L"STATIC", SS_LEFT, PV_IDC_ST_STATUS,
                          UI_FONT_KIND_BTN);
}

/**
 * @brief   按客户区尺寸排布子控件并缓存按钮矩形
 * @param   hwnd 窗口句柄
 * @return  无
 */
static void pv_layout(HWND hwnd)
{
    RECT rc = {0};
    uint32_t cw = 0u;
    uint32_t ch = 0u;
    uint32_t x = 0u;
    uint32_t i = 0u;
    uint32_t btn_x = 0u;

    GetClientRect(hwnd, &rc);
    cw = (uint32_t)(rc.right - rc.left);
    ch = (uint32_t)(rc.bottom - rc.top);
    if (cw < 400u) {
        return;
    }

    /* 行1: 启用 + 解析器勾选(左) | 按钮(右) */
    (void)MoveWindow(GetDlgItem(hwnd, (int)PV_IDC_CK_ENABLE),
                     (int)PV_MARGIN, (int)PV_ROW1_Y, 120, (int)PV_CTRL_H, TRUE);
    x = (PV_MARGIN + 124u);
    for (i = 0u; i < g_pv_parser_count; i++) {
        (void)MoveWindow(GetDlgItem(hwnd, (int)(PV_IDC_CK_PARSER_BASE + i)),
                         (int)x, (int)PV_ROW1_Y, 150, (int)PV_CTRL_H, TRUE);
        x += 154u;
    }
    btn_x = (cw - PV_MARGIN - (PV_BTN_W * PV_BTN_COUNT) - (PV_BTN_GAP * (PV_BTN_COUNT - 1u)));
    for (i = 0u; i < PV_BTN_COUNT; i++) {
        g_pv_btn_rects[i].left = (LONG)(btn_x + (i * (PV_BTN_W + PV_BTN_GAP)));
        g_pv_btn_rects[i].top = (LONG)PV_ROW1_Y;
        g_pv_btn_rects[i].right = (LONG)(btn_x + (i * (PV_BTN_W + PV_BTN_GAP)) + PV_BTN_W);
        g_pv_btn_rects[i].bottom = (LONG)(PV_ROW1_Y + PV_BTN_H);
    }

    /* 行2: 关键字 + 行数 */
    (void)MoveWindow(GetDlgItem(hwnd, (int)6060u),
                     (int)PV_MARGIN, (int)(PV_ROW2_Y + 3u), 92, (int)PV_CTRL_H, TRUE);
    (void)MoveWindow(GetDlgItem(hwnd, (int)PV_IDC_ED_KW),
                     (int)(PV_MARGIN + 94u), (int)PV_ROW2_Y, 300, (int)PV_ED_H, TRUE);
    (void)MoveWindow(GetDlgItem(hwnd, (int)6061u),
                     (int)(PV_MARGIN + 402u), (int)(PV_ROW2_Y + 3u), 80, (int)PV_CTRL_H, TRUE);
    (void)MoveWindow(GetDlgItem(hwnd, (int)PV_IDC_ED_LINES),
                     (int)(PV_MARGIN + 484u), (int)PV_ROW2_Y, 44, (int)PV_ED_H, TRUE);

    /* 记录列表 + 详情 + 状态行 */
    (void)MoveWindow(GetDlgItem(hwnd, (int)PV_IDC_LIST_REC),
                     (int)PV_MARGIN, (int)PV_LIST_Y,
                     (int)(cw - PV_MARGIN * 2u), (int)PV_LIST_H, TRUE);
    (void)MoveWindow(GetDlgItem(hwnd, (int)PV_IDC_ED_DETAIL),
                     (int)PV_MARGIN, (int)(PV_LIST_Y + PV_LIST_H + PV_DETAIL_GAP),
                     (int)(cw - PV_MARGIN * 2u),
                     (int)(ch - (PV_LIST_Y + PV_LIST_H + PV_DETAIL_GAP) - PV_STATUS_H - PV_MARGIN),
                     TRUE);
    (void)MoveWindow(GetDlgItem(hwnd, (int)PV_IDC_ST_STATUS),
                     (int)PV_MARGIN, (int)(ch - PV_STATUS_H - 2u),
                     (int)(cw - PV_MARGIN * 2u), (int)PV_STATUS_H, TRUE);

    InvalidateRect(hwnd, NULL, FALSE);
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 配置读写                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief   从ini加载配置并同步到控件
 * @param   hwnd 窗口句柄
 * @return  无
 */
static void pv_load_cfg(HWND hwnd)
{
    wchar_t text[PV_TEXT_MAX];
    uint32_t i = 0u;

    g_pv_enabled = (dbg_conf_get_uint(L"Proto", L"Enable", 0u) != 0u) ? 1u : 0u;
    g_pv_autosave = (dbg_conf_get_uint(L"Proto", L"AutoSave", 1u) != 0u) ? 1u : 0u;
    memset(&text[0], 0, sizeof(text));
    dbg_conf_get_str(L"Proto", L"Keyword", L"23 23 02 FE", &g_pv_cfg.keyword[0],
                     PROTO_CAP_KW_WCHAR);
    g_pv_cfg.lines = dbg_conf_get_uint(L"Proto", L"Lines", 5u);
    if (g_pv_cfg.lines > PROTO_CAP_LINES_MAX) {
        g_pv_cfg.lines = PROTO_CAP_LINES_MAX;
    }

    (void)SendMessageW(GetDlgItem(hwnd, (int)PV_IDC_CK_ENABLE), BM_SETCHECK,
                       (g_pv_enabled != 0u) ? BST_CHECKED : BST_UNCHECKED, 0u);
    SetWindowTextW(GetDlgItem(hwnd, (int)PV_IDC_ED_KW), &g_pv_cfg.keyword[0]);
    (void)wsprintfW(&text[0], L"%u", (unsigned int)g_pv_cfg.lines);
    SetWindowTextW(GetDlgItem(hwnd, (int)PV_IDC_ED_LINES), &text[0]);

    for (i = 0u; i < g_pv_parser_count; i++) {
        wchar_t key[PV_TEXT_MAX];

        (void)wsprintfW(&key[0], L"Use%u", (unsigned int)i);
        g_pv_parsers[i].checked =
            (dbg_conf_get_uint(L"Proto", &key[0], 1u) != 0u) ? 1u : 0u;
        (void)SendMessageW(GetDlgItem(hwnd, (int)(PV_IDC_CK_PARSER_BASE + i)), BM_SETCHECK,
                           (g_pv_parsers[i].checked != 0u) ? BST_CHECKED : BST_UNCHECKED,
                           0u);
    }
}

/**
 * @brief   保存配置到ini
 * @return  无
 */
static void pv_save_cfg(void)
{
    uint32_t i = 0u;

    dbg_conf_set_uint(L"Proto", L"Enable", (uint32_t)g_pv_enabled);
    dbg_conf_set_uint(L"Proto", L"AutoSave", (uint32_t)g_pv_autosave);
    dbg_conf_set_str(L"Proto", L"Keyword", &g_pv_cfg.keyword[0]);
    dbg_conf_set_uint(L"Proto", L"Lines", g_pv_cfg.lines);
    for (i = 0u; i < g_pv_parser_count; i++) {
        wchar_t key[PV_TEXT_MAX];

        (void)wsprintfW(&key[0], L"Use%u", (unsigned int)i);
        dbg_conf_set_uint(L"Proto", &key[0], (uint32_t)g_pv_parsers[i].checked);
    }
    dbg_conf_set_str(L"Proto", L"Dir", &g_pv_dir[0]);
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 动作                                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief   清空记录
 * @param   hwnd 窗口句柄
 * @return  无
 */
static void pv_on_clear(HWND hwnd)
{
    uint32_t i = 0u;

    for (i = 0u; i < PV_REC_MAX; i++) {
        pv_rec_free(&g_pv_recs[i]);
    }
    g_pv_rec_count = 0u;
    g_pv_rec_head = 0u;
    g_pv_hit_total = 0u;
    proto_cap_init(&g_pv_cap);
    pv_refresh_list(hwnd);
    pv_show_detail(hwnd);
}

/**
 * @brief   保存当前详情到文件(UTF-8 BOM)
 * @param   hwnd 窗口句柄
 * @return  无
 */
static void pv_on_save(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t path[PV_DIR_MAX];
    HWND h_ed = GetDlgItem(hwnd, (int)PV_IDC_ED_DETAIL);
    uint32_t len = (uint32_t)SendMessageW(h_ed, WM_GETTEXTLENGTH, 0u, 0u);
    wchar_t *p_buf = NULL;
    int32_t utf8_len = 0u;
    char *p_utf8 = NULL;
    HANDLE h_file = NULL;
    DWORD written = 0u;
    static const char BOM[3u] = {(char)0xEF, (char)0xBB, (char)0xBF};

    if (len == 0u) {
        return;
    }
    memset(&ofn, 0, sizeof(ofn));
    memset(&path[0], 0, sizeof(path));
    lstrcpynW(&path[0], L"proto_detail.txt", PV_DIR_MAX);
    ofn.lStructSize = (DWORD)sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"文本文件\0*.txt\0所有文件\0*.*\0";
    ofn.lpstrFile = &path[0];
    ofn.nMaxFile = (DWORD)PV_DIR_MAX;
    ofn.Flags = (DWORD)(OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
    if (GetSaveFileNameW(&ofn) == 0) {
        return;
    }
    p_buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0u,
                                 ((SIZE_T)len + 2u) * sizeof(wchar_t));
    if (p_buf == NULL) {
        return;
    }
    (void)SendMessageW(h_ed, WM_GETTEXT, (WPARAM)(len + 1u), (LPARAM)p_buf);
    utf8_len = WideCharToMultiByte(CP_UTF8, 0u, p_buf, -1, NULL, 0u, NULL, NULL);
    p_utf8 = (char *)HeapAlloc(GetProcessHeap(), 0u, (SIZE_T)utf8_len + 4u);
    if (p_utf8 != NULL) {
        (void)WideCharToMultiByte(CP_UTF8, 0u, p_buf, -1, &p_utf8[3u], utf8_len,
                                  NULL, NULL);
        memcpy(&p_utf8[0], &BOM[0], 3u);
        h_file = CreateFileW(&path[0], GENERIC_WRITE, 0u, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
        if (h_file != INVALID_HANDLE_VALUE) {
            (void)WriteFile(h_file, p_utf8, (DWORD)(utf8_len + 2u), &written, NULL);
            (void)CloseHandle(h_file);
        }
        (void)HeapFree(GetProcessHeap(), 0u, p_utf8);
    }
    (void)HeapFree(GetProcessHeap(), 0u, p_buf);
}

/**
 * @brief   处理自绘按钮动作
 * @param   hwnd    窗口句柄
 * @param   ctrl_id 按钮ID
 * @return  无
 */
static void pv_on_button(HWND hwnd, uint32_t ctrl_id)
{
    switch (ctrl_id)
    {
        case PV_BTN_CLR:
        {
            pv_on_clear(hwnd);
            break;
        }
        case PV_BTN_SAVE:
        {
            pv_on_save(hwnd);
            break;
        }
        case PV_BTN_DIR:
        {
            pv_ensure_dir();
            (void)ShellExecuteW(hwnd, L"open", &g_pv_dir[0], NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        default:
        {
            break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 消息处理                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief   窗口消息处理过程
 * @param   hwnd  窗口句柄
 * @param   msg   消息
 * @param   wp    参数1
 * @param   lp    参数2
 * @return  处理结果
 */
static LRESULT CALLBACK pv_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            pv_ensure_dir();
            pv_reset_save_path();
            proto_cap_init(&g_pv_cap);
            pv_create_controls(hwnd);
            pv_load_cfg(hwnd);
            pv_refresh_list(hwnd);
            return 0;
        }
        case WM_SIZE:
        {
            pv_layout(hwnd);
            return 0;
        }
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO *p_mmi = (MINMAXINFO *)lp;

            p_mmi->ptMinTrackSize.x = PV_MIN_TRACK_W;
            p_mmi->ptMinTrackSize.y = PV_MIN_TRACK_H;
            return 0;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc;
            uint32_t index = 0u;

            memset(&ps, 0, sizeof(ps));
            hdc = BeginPaint(hwnd, &ps);
            SetBkMode(hdc, TRANSPARENT);
            for (index = 0u; index < PV_BTN_COUNT; index++) {
                pv_draw_button(hdc, index);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_NOTIFY:
        {
            const PV_NMHDR_T *p_hdr = (const PV_NMHDR_T *)lp;

            if ((p_hdr != NULL) && (p_hdr->idFrom == (UINT_PTR)PV_IDC_LIST_REC) &&
                (p_hdr->code == PV_LVN_ITEMCHANGED)) {
                pv_show_detail(hwnd);
            }
            return 0;
        }
        case WM_COMMAND:
        {
            uint32_t id = (uint32_t)LOWORD(wp);
            uint32_t code = (uint32_t)HIWORD(wp);

            if (id == (uint32_t)PV_IDC_CK_ENABLE) {
                g_pv_enabled = (SendMessageW((HWND)lp, BM_GETCHECK, 0u, 0u)
                                == BST_CHECKED) ? 1u : 0u;
                proto_cap_init(&g_pv_cap);
                pv_save_cfg();
            } else if ((id >= PV_IDC_CK_PARSER_BASE) &&
                       (id < (PV_IDC_CK_PARSER_BASE + PROTO_VIEW_PARSER_MAX))) {
                uint32_t idx = (id - PV_IDC_CK_PARSER_BASE);

                if (idx < g_pv_parser_count) {
                    g_pv_parsers[idx].checked =
                        (SendMessageW((HWND)lp, BM_GETCHECK, 0u, 0u)
                         == BST_CHECKED) ? 1u : 0u;
                    pv_save_cfg();
                }
            } else if ((id == (uint32_t)PV_IDC_ED_KW) && (code == EN_KILLFOCUS)) {
                wchar_t text[PROTO_CAP_KW_WCHAR];

                memset(&text[0], 0, sizeof(text));
                (void)SendMessageW((HWND)lp, WM_GETTEXT,
                                   (WPARAM)PROTO_CAP_KW_WCHAR, (LPARAM)&text[0]);
                lstrcpynW(&g_pv_cfg.keyword[0], &text[0], PROTO_CAP_KW_WCHAR);
                proto_cap_init(&g_pv_cap);
                pv_save_cfg();
            } else if ((id == (uint32_t)PV_IDC_ED_LINES) && (code == EN_KILLFOCUS)) {
                wchar_t text[16u];
                uint32_t v = 0u;
                uint32_t i = 0u;

                memset(&text[0], 0, sizeof(text));
                (void)SendMessageW((HWND)lp, WM_GETTEXT, 16u, (LPARAM)&text[0]);
                for (i = 0u; text[i] != L'\0'; i++) {
                    if ((text[i] >= L'0') && (text[i] <= L'9')) {
                        v = ((v * 10u) + (uint32_t)(text[i] - L'0')) & 0xFFFFu;
                    }
                }
                g_pv_cfg.lines = (v > PROTO_CAP_LINES_MAX) ? PROTO_CAP_LINES_MAX : v;
                proto_cap_init(&g_pv_cap);
                pv_save_cfg();
            }
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            POINT pt;
            uint32_t id = 0u;
            uint32_t old_id = g_pv_hover_id;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            id = pv_hit_btn(pt);
            if (id != old_id) {
                g_pv_hover_id = id;
                pv_invalidate_btn(hwnd, old_id);
                pv_invalidate_btn(hwnd, id);
            }
            if (g_pv_mouse_tracking == 0u) {
                TRACKMOUSEEVENT tme;

                memset(&tme, 0, sizeof(tme));
                tme.cbSize = (DWORD)sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                (void)TrackMouseEvent(&tme);
                g_pv_mouse_tracking = 1u;
            }
            break;
        }
        case WM_MOUSELEAVE:
        {
            pv_invalidate_btn(hwnd, g_pv_hover_id);
            g_pv_hover_id = 0u;
            g_pv_mouse_tracking = 0u;
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            POINT pt;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            g_pv_press_id = pv_hit_btn(pt);
            if (g_pv_press_id != 0u) {
                pv_invalidate_btn(hwnd, g_pv_press_id);
                SetCapture(hwnd);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        {
            if (g_pv_press_id != 0u) {
                POINT pt;
                uint32_t id = 0u;

                ReleaseCapture();
                pt.x = (int32_t)(int16_t)LOWORD(lp);
                pt.y = (int32_t)(int16_t)HIWORD(lp);
                id = pv_hit_btn(pt);
                if (id == g_pv_press_id) {
                    pv_on_button(hwnd, id);
                }
                pv_invalidate_btn(hwnd, g_pv_press_id);
                g_pv_press_id = 0u;
            }
            return 0;
        }
        case WM_SETCURSOR:
        {
            if ((LOWORD(wp) == HTCLIENT) && (g_pv_hover_id != 0u)) {
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND));
                return (LRESULT)1u;
            }
            break;
        }
        case WM_DESTROY:
        {
            pv_save_cfg();
            g_pv_hwnd = NULL;
            return 0;
        }
        default:
        {
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* -------------------------------------------------------------------------- */
/*  外部函数                                                                   */
/* -------------------------------------------------------------------------- */

void proto_view_set_parser(const wchar_t *p_name, PROTO_VIEW_PARSE_FN p_fn)
{
    uint32_t i = 0u;

    if ((p_name == NULL) || (p_name[0] == L'\0') || (p_fn == NULL)) {
        return;
    }
    for (i = 0u; i < g_pv_parser_count; i++) {
        if (lstrcmpW(&g_pv_parsers[i].name[0], p_name) == 0) {
            g_pv_parsers[i].p_fn = p_fn;   /* 同名更新 */
            return;
        }
    }
    if (g_pv_parser_count >= PROTO_VIEW_PARSER_MAX) {
        return;
    }
    lstrcpynW(&g_pv_parsers[g_pv_parser_count].name[0], p_name, PROTO_VIEW_NAME_WCHAR);
    g_pv_parsers[g_pv_parser_count].p_fn = p_fn;
    g_pv_parsers[g_pv_parser_count].checked = 1u;
    g_pv_parser_count++;
}

uint8_t proto_view_is_open(void)
{
    return (g_pv_hwnd != NULL) ? 1u : 0u;
}

void proto_view_feed(const wchar_t *p_line)
{
    uint8_t out[PROTO_CAP_BUF_MAX];
    uint32_t out_len = 0u;

    if ((g_pv_hwnd == NULL) || (g_pv_enabled == 0u) || (p_line == NULL)) {
        return;
    }
    g_pv_last_feed_tick = GetTickCount();
    if (proto_cap_feed(&g_pv_cap, &g_pv_cfg, p_line, &out[0], &out_len) != 0u) {
        pv_commit(g_pv_hwnd, &out[0], out_len);
    }
}

void proto_view_tick(void)
{
    uint8_t out[PROTO_CAP_BUF_MAX];
    uint32_t out_len = 0u;

    if ((g_pv_hwnd == NULL) || (g_pv_enabled == 0u)) {
        return;
    }
    if (proto_cap_active(&g_pv_cap) == 0u) {
        return;
    }
    /* 空闲收口: 报文一次打完后行流静默(行数窗口配大也不滞留),
       连续报文则由再次命中路径收口 */
    if ((GetTickCount() - g_pv_last_feed_tick) < PV_IDLE_FLUSH_MS) {
        return;
    }
    if (proto_cap_take(&g_pv_cap, &out[0], &out_len) != 0u) {
        pv_commit(g_pv_hwnd, &out[0], out_len);
    }
}

void proto_view_open(HINSTANCE h_inst, HWND h_owner)
{
    WNDCLASSW wc;
    HWND h_wnd = NULL;

    if (g_pv_class_ready == 0u) {
        memset(&wc, 0, sizeof(wc));
        wc.style = (UINT)(CS_HREDRAW | CS_VREDRAW);
        wc.lpfnWndProc = pv_wnd_proc;
        wc.hInstance = h_inst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = PV_CLASS_NAME;
        (void)RegisterClassW(&wc);
        g_pv_class_ready = 1u;
    }
    g_pv_instance = h_inst;

    if (g_pv_hwnd != NULL) {
        ShowWindow(g_pv_hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(g_pv_hwnd);
        return;
    }
    h_wnd = CreateWindowW(PV_CLASS_NAME, PV_WND_TITLE,
                          (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN),
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          PV_WND_WIDTH, PV_WND_HEIGHT,
                          h_owner, NULL, h_inst, NULL);
    if (h_wnd != NULL) {
        g_pv_hwnd = h_wnd;
        ShowWindow(h_wnd, SW_SHOWNORMAL);
        UpdateWindow(h_wnd);
    }
}
