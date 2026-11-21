/**
 * @file    tool_gb32960.c
 * @author  herongshun
 * @date    2026-11-20
 * @brief   GB32960 协议解析工具界面实现(单窗体)
 *          布局: hex输入区(等宽/软换行) + 操作行[解析|打开|清空|复制|保存|切换显示]
 *          + 左侧解析记录列表(点击重显) + 右侧结果区(树形/文本双模式)
 *          输入/结果框支持 Ctrl+A 全选; 2016/2025 版本自适应解析
 */

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdint.h>
#include <string.h>
#include "ui_common.h"
#include "gb32960.h"
#include "gb_records.h"
#include "tool_gb32960.h"

/* -------------------------------------------------------------------------- */
/*  宏定义: 控件ID                                                             */
/* -------------------------------------------------------------------------- */
#define GBW_IDC_ED_IN     7001u  /**< hex输入区 */
#define GBW_IDC_ED_OUT    7002u  /**< 解析结果区(文本模式, 只读) */
#define GBW_IDC_TREE_OUT  7003u  /**< 解析结果区(树形模式) */
#define GBW_IDC_LIST_REC  7004u  /**< 解析记录列表 */

/* 自绘按钮逻辑ID */
#define GBW_IDC_BTN_PARSE 7101u  /**< 解析 */
#define GBW_IDC_BTN_OPEN  7102u  /**< 打开文件… */
#define GBW_IDC_BTN_CLEAR 7103u  /**< 清空 */
#define GBW_IDC_BTN_COPY  7104u  /**< 复制结果 */
#define GBW_IDC_BTN_SAVE  7105u  /**< 保存结果… */
#define GBW_IDC_BTN_MODE  7106u  /**< 切换显示(树形/文本) */
#define GBW_IDC_BTN_RECCLR 7107u /**< 清空记录 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 主题颜色 (0x00BBGGRR)                                              */
/* -------------------------------------------------------------------------- */
#define GBW_CLR_ACCENT         0x005EC522  /**< 主题绿 #22C55E */
#define GBW_CLR_ACCENT_HOVER   0x0085AB18  /**< 主题绿悬停 */
#define GBW_CLR_ACCENT_PRESS   0x00848D15  /**< 主题绿按下 */
#define GBW_CLR_BTN_NEUTRAL_BG 0x00F3F4F8  /**< 中性按钮底色 */
#define GBW_CLR_BTN_NEUTRAL_HV 0x00FFFFFF  /**< 中性按钮悬停底色 */
#define GBW_CLR_BTN_NEUTRAL_PR 0x00E7EAF1  /**< 中性按钮按下底色 */
#define GBW_CLR_BTN_BORDER     0x00D5DAE4  /**< 中性按钮边框 */
#define GBW_CLR_TEXT_DARK      0x00283040  /**< 深色文字 */

/* -------------------------------------------------------------------------- */
/*  宏定义: 窗口与布局                                                         */
/* -------------------------------------------------------------------------- */
#define GBW_CLASS_NAME    L"TS_GBW_WND"      /**< 窗口类名 */
#define GBW_WND_TITLE     L"GB32960 协议解析"  /**< 窗口标题 */
#define GBW_WND_WIDTH     980u                /**< 初始宽度 */
#define GBW_WND_HEIGHT    680u                /**< 初始高度 */
#define GBW_MIN_TRACK_W   720u                /**< 最小轨宽 */
#define GBW_MIN_TRACK_H   460u                /**< 最小轨高 */

#define GBW_MARGIN        14u    /**< 页边距 */
#define GBW_IN_Y          14u    /**< 输入区Y */
#define GBW_IN_H          140u   /**< 输入区高 */
#define GBW_BTN_ROW_Y     164u   /**< 按钮行Y(输入区下) */
#define GBW_BTN_H         32u    /**< 按钮高 */
#define GBW_BTN_GAP       10u    /**< 按钮间距 */
#define GBW_BTN_W_MAIN    100u   /**< 主按钮(解析)宽 */
#define GBW_BTN_W_SUB     84u    /**< 次按钮宽 */
#define GBW_BTN_W_SMALL   76u    /**< 小按钮(清空记录)宽 */
#define GBW_BTN_H_SMALL   22u    /**< 小按钮高 */
#define GBW_OUT_Y         214u   /**< 记录/结果区Y */
#define GBW_REC_W         250u   /**< 解析记录面板宽 */
#define GBW_REC_HEAD_H    26u    /**< 记录面板头高 */
#define GBW_REC_GAP       10u    /**< 记录面板与结果区间距 */

#define GBW_BTN_COUNT     7u     /**< 自绘按钮数量 */
#define GBW_TREE_DEPTH_MAX 8u    /**< 树深度上限(超过并入末层) */

/* 树视图消息/常量(commctrl数值, 本地定义避免旧头文件A/W宏歧义);
   注意 TVM_INSERTITEM 的 A/W 是不同消息号(0x1100=A, 0x1132=W), 必须用W版 */
#define GBW_TVM_INSERTITEMW (0x1132u)              /**< 插入树节点(Unicode) */
#define GBW_TVM_DELETEITEM  (0x1101u)              /**< 删除树节点 */
#define GBW_TVM_EXPAND      (0x1102u)              /**< 展开/折叠树节点 */
#define GBW_TVIF_TEXT       0x0001u                /**< 文本有效掩码 */
#define GBW_TVE_EXPAND      0x0002u                /**< 展开 */
#define GBW_TVI_ROOT_VAL    ((ULONG_PTR)0xFFFF0000u) /**< 根位置 */
#define GBW_TVI_LAST_VAL    ((ULONG_PTR)0xFFFF0002u) /**< 尾部插入位置 */

/* -------------------------------------------------------------------------- */
/*  内部类型                                                                    */
/* -------------------------------------------------------------------------- */

/** 树节点项(TVITEMW布局, 本地定义) */
typedef struct GBW_TVITEM_TAG
{
    UINT       mask;              /**< 有效字段掩码 */
    HTREEITEM  hItem;             /**< 项句柄 */
    UINT       state;             /**< 状态 */
    UINT       stateMask;         /**< 状态掩码 */
    wchar_t   *pszText;           /**< 项文本 */
    int32_t    cchTextMax;        /**< 文本容量 */
    int32_t    iImage;            /**< 图像索引 */
    int32_t    iSelectedImage;    /**< 选中图像索引 */
    int32_t    cChildren;         /**< 子项数(0=无) */
    LPARAM     lParam;            /**< 附加参数 */
} GBW_TVITEM_T;

/** 树节点插入结构(TVINSERTSTRUCTW布局, 本地定义) */
typedef struct GBW_TVIS_TAG
{
    HTREEITEM   hParent;          /**< 父项(TVI_ROOT=根) */
    HTREEITEM   hInsertAfter;     /**< 插入位置(TVI_LAST=尾) */
    GBW_TVITEM_T item;            /**< 项内容 */
} GBW_TVIS_T;

/* -------------------------------------------------------------------------- */
/*  模块内部全局变量                                                           */
/* -------------------------------------------------------------------------- */

static uint8_t   g_gbw_class_ready = 0u;                 /**< 窗口类注册标志 */
static HINSTANCE g_gbw_instance = NULL;                  /**< 应用实例句柄 */
static uint32_t  g_gbw_hover_id = 0u;                    /**< 悬停按钮ID(0=无) */
static uint32_t  g_gbw_press_id = 0u;                    /**< 按下按钮ID(0=无) */
static uint8_t   g_gbw_mouse_tracking = 0u;              /**< 鼠标离开跟踪标志 */
static uint8_t   g_gbw_tree_mode = 1u;                    /**< 1=树形显示; 0=文本显示 */
static WNDPROC   g_gbw_edit_orig_proc = NULL;             /**< EDIT原窗口过程(Ctrl+A子类化) */

static GB32960_NODE_T g_gbw_nodes[GB32960_NODE_MAX];     /**< 当前展示节点缓冲 */
static uint32_t  g_gbw_node_count = 0u;                  /**< 当前展示节点数 */
static uint8_t   g_gbw_bytes[GB32960_FRAME_MAX];         /**< 解码字节缓冲 */
static uint32_t  g_gbw_offs[GB32960_FRAME_LIST_MAX];          /**< 各帧起始偏移 */
static uint32_t  g_gbw_lens[GB32960_FRAME_LIST_MAX];          /**< 各帧长度 */
static wchar_t   g_gbw_text[GB32960_OUT_MAX_WCHAR];      /**< 文本模式物化缓冲 */

static RECT g_gbw_btn_rects[GBW_BTN_COUNT] = {0};        /**< 自绘按钮矩形缓存 */
static RECT g_gbw_rec_title_rect = {0};                  /**< 记录面板标题矩形 */

static const uint32_t g_gbw_btn_ids[GBW_BTN_COUNT] =
{
    GBW_IDC_BTN_PARSE, GBW_IDC_BTN_OPEN, GBW_IDC_BTN_CLEAR,
    GBW_IDC_BTN_COPY, GBW_IDC_BTN_SAVE, GBW_IDC_BTN_MODE, GBW_IDC_BTN_RECCLR
};

static const wchar_t *const g_gbw_btn_labels[GBW_BTN_COUNT] =
{
    L"解 析", L"打开文件…", L"清 空", L"复制结果", L"保存结果", L"切换显示", L"清空记录"
};

/* -------------------------------------------------------------------------- */
/*  内部私有函数: Ctrl+A 全选子类化                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief   EDIT子类化过程: 拦截 Ctrl+A(WM_CHAR 0x01) 转为全选
 * @param   h   控件句柄
 * @param   msg 消息
 * @param   wp  参数1
 * @param   lp  参数2
 * @return  处理结果
 */
static LRESULT CALLBACK gbw_edit_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if ((msg == WM_CHAR) && (wp == (WPARAM)1u)) {
        (void)SendMessageW(h, EM_SETSEL, 0u, (LPARAM)-1);
        return 0;
    }

    return CallWindowProcW(g_gbw_edit_orig_proc, h, msg, wp, lp);
}

/**
 * @brief   给EDIT控件安装 Ctrl+A 全选子类化
 * @param   h_edit 控件句柄
 * @return  无
 */
static void gbw_install_ctrl_a(HWND h_edit)
{
    if ((h_edit == NULL) || (g_gbw_edit_orig_proc != NULL)) {
        return;
    }
    g_gbw_edit_orig_proc = (WNDPROC)GetWindowLongW(h_edit, GWL_WNDPROC);
    (void)SetWindowLongW(h_edit, GWL_WNDPROC, (LONG)gbw_edit_proc);
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 控件创建与布局                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief   创建子控件通用封装(位置尺寸为占位值, 实际由 gbw_layout 统一排布)
 * @param   h_parent   父窗口句柄
 * @param   p_class    控件窗口类名
 * @param   style      控件扩展样式
 * @param   ctrl_id    控件ID
 * @param   font_kind  字体种类
 * @return  控件窗口句柄
 */
static HWND gbw_create_child(HWND h_parent, const wchar_t *p_class,
                             uint32_t style, uint32_t ctrl_id, uint32_t font_kind)
{
    HWND h_ctrl = CreateWindowExW(0u, p_class, L"",
                                  (WS_CHILD | WS_VISIBLE | style),
                                  0, 0, 10, 10,
                                  h_parent, (HMENU)(uintptr_t)ctrl_id, NULL, NULL);

    if (h_ctrl != NULL) {
        ui_common_apply_font_kind(h_ctrl, font_kind);
    }

    return h_ctrl;
}

/**
 * @brief   创建全部子控件
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_create_controls(HWND hwnd)
{
    HWND h_in = NULL;
    HWND h_out = NULL;

    /* 输入区: 不带WS_HSCROLL -> 多行EDIT按窗宽软换行(长hex串显示友好) */
    h_in = gbw_create_child(hwnd, L"EDIT",
                            (uint32_t)(ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN |
                                       WS_VSCROLL | WS_BORDER),
                            GBW_IDC_ED_IN, UI_FONT_KIND_EDIT);
    /* 结果区(文本模式): 横向滚动保表格对齐 */
    h_out = gbw_create_child(hwnd, L"EDIT",
                             (uint32_t)(ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
                                        WS_VSCROLL | WS_HSCROLL | WS_BORDER),
                             GBW_IDC_ED_OUT, UI_FONT_KIND_EDIT);
    gbw_install_ctrl_a(h_in);
    gbw_install_ctrl_a(h_out);
    (void)SendMessageW(h_in, EM_SETLIMITTEXT, (WPARAM)GB32960_IN_MAX_WCHAR, (LPARAM)0);

    /* 结果区(树形模式): 带展开按钮/连线/选中高亮 */
    (void)gbw_create_child(hwnd, WC_TREEVIEWW,
                           (uint32_t)(WS_BORDER | WS_VSCROLL |
                                      TVS_HASBUTTONS | TVS_HASLINES |
                                      TVS_LINESATROOT | TVS_SHOWSELALWAYS),
                           GBW_IDC_TREE_OUT, UI_FONT_KIND_EDIT);

    /* 解析记录列表 */
    (void)gbw_create_child(hwnd, L"LISTBOX",
                           (uint32_t)(WS_BORDER | WS_VSCROLL |
                                      LBS_NOTIFY | LBS_NOINTEGRALHEIGHT),
                           GBW_IDC_LIST_REC, UI_FONT_KIND_EDIT);
}

/**
 * @brief   按客户区尺寸排布子控件并缓存按钮矩形
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_layout(HWND hwnd)
{
    RECT rc = {0};
    uint32_t cw = 0u;
    uint32_t ch = 0u;
    uint32_t out_y = 0u;
    uint32_t res_x = 0u;
    uint32_t res_w = 0u;
    uint32_t body_h = 0u;

    GetClientRect(hwnd, &rc);
    cw = (uint32_t)(rc.right - rc.left);
    ch = (uint32_t)(rc.bottom - rc.top);
    if (cw < 300u) {
        return;
    }

    (void)MoveWindow(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN),
                     (int)GBW_MARGIN, (int)GBW_IN_Y,
                     (int)(cw - GBW_MARGIN * 2u), (int)GBW_IN_H, TRUE);

    {
        uint32_t x = GBW_MARGIN;
        uint32_t i = 0u;
        const uint32_t widths[GBW_BTN_COUNT] =
        {
            GBW_BTN_W_MAIN, GBW_BTN_W_SUB, GBW_BTN_W_SUB,
            GBW_BTN_W_SUB, GBW_BTN_W_SUB, GBW_BTN_W_SUB, GBW_BTN_W_SMALL
        };

        /* 主行6个: 解析|打开|清空|复制|保存|切换显示 */
        for (i = 0u; i < 6u; i++) {
            g_gbw_btn_rects[i].left = (LONG)x;
            g_gbw_btn_rects[i].top = (LONG)GBW_BTN_ROW_Y;
            g_gbw_btn_rects[i].right = (LONG)(x + widths[i]);
            g_gbw_btn_rects[i].bottom = (LONG)(GBW_BTN_ROW_Y + GBW_BTN_H);
            x += widths[i] + GBW_BTN_GAP;
        }
        /* 记录面板头内小按钮: 清空记录 */
        g_gbw_btn_rects[6].left = (LONG)(GBW_MARGIN + GBW_REC_W -
                                         GBW_BTN_W_SMALL - 6u);
        g_gbw_btn_rects[6].top = (LONG)(GBW_OUT_Y + 2u);
        g_gbw_btn_rects[6].right = (LONG)(GBW_MARGIN + GBW_REC_W - 6u);
        g_gbw_btn_rects[6].bottom = (LONG)(GBW_OUT_Y + 2u + GBW_BTN_H_SMALL);
    }

    /* 记录面板标题矩形(自绘文本) */
    g_gbw_rec_title_rect.left = (LONG)(GBW_MARGIN + 6u);
    g_gbw_rec_title_rect.top = (LONG)(GBW_OUT_Y + 2u);
    g_gbw_rec_title_rect.right = (LONG)(GBW_MARGIN + GBW_REC_W - GBW_BTN_W_SMALL - 12u);
    g_gbw_rec_title_rect.bottom = (LONG)(GBW_OUT_Y + GBW_REC_HEAD_H);

    out_y = GBW_OUT_Y + GBW_REC_HEAD_H;
    if (out_y >= ch) {
        return;
    }
    body_h = (ch - out_y - GBW_MARGIN);

    /* 解析记录列表(左) */
    (void)MoveWindow(GetDlgItem(hwnd, (int)GBW_IDC_LIST_REC),
                     (int)GBW_MARGIN, (int)out_y,
                     (int)GBW_REC_W, (int)body_h, TRUE);

    /* 结果区(右): 文本与树同矩形, 按模式显隐 */
    res_x = (GBW_MARGIN + GBW_REC_W + GBW_REC_GAP);
    res_w = (cw - res_x - GBW_MARGIN);
    (void)MoveWindow(GetDlgItem(hwnd, (int)GBW_IDC_ED_OUT),
                     (int)res_x, (int)GBW_OUT_Y, (int)res_w, (int)(ch - GBW_OUT_Y - GBW_MARGIN),
                     TRUE);
    (void)MoveWindow(GetDlgItem(hwnd, (int)GBW_IDC_TREE_OUT),
                     (int)res_x, (int)GBW_OUT_Y, (int)res_w, (int)(ch - GBW_OUT_Y - GBW_MARGIN),
                     TRUE);

    InvalidateRect(hwnd, NULL, FALSE);
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 自绘按钮                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief   绘制单个自绘按钮(解析=主按钮, 其余中性)
 * @param   hdc   目标设备上下文
 * @param   index 按钮索引
 * @return  无
 */
static void gbw_draw_button(HDC hdc, uint32_t index)
{
    uint32_t ctrl_id = g_gbw_btn_ids[index];
    uint8_t primary = ((index == 0u) ? 1u : 0u);
    uint8_t hovered = ((g_gbw_hover_id == ctrl_id) ? 1u : 0u);
    uint8_t pressed = ((g_gbw_press_id == ctrl_id) ? 1u : 0u);
    uint32_t fill_clr = 0u;
    uint32_t border_clr = 0u;
    uint32_t text_clr = 0u;
    RECT rc = g_gbw_btn_rects[index];
    HFONT h_font_old = NULL;

    if (primary != 0u) {
        if (pressed != 0u) {
            fill_clr = GBW_CLR_ACCENT_PRESS;
        } else if (hovered != 0u) {
            fill_clr = GBW_CLR_ACCENT_HOVER;
        } else {
            fill_clr = GBW_CLR_ACCENT;
        }
        border_clr = fill_clr;
        text_clr = 0x00FFFFFFu;
    } else {
        if (pressed != 0u) {
            fill_clr = GBW_CLR_BTN_NEUTRAL_PR;
        } else if (hovered != 0u) {
            fill_clr = GBW_CLR_BTN_NEUTRAL_HV;
        } else {
            fill_clr = GBW_CLR_BTN_NEUTRAL_BG;
        }
        border_clr = GBW_CLR_BTN_BORDER;
        text_clr = GBW_CLR_TEXT_DARK;
    }

    SetBkMode(hdc, TRANSPARENT);
    ui_common_fill_round_rect(hdc, &rc, 8u, fill_clr, border_clr, 1u);

    InflateRect(&rc, -6, -4);
    h_font_old = (HFONT)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_BTN));
    SetTextColor(hdc, (COLORREF)text_clr);
    ui_common_text_crisp(hdc, &rc, g_gbw_btn_labels[index],
                         (DT_CENTER | DT_VCENTER | DT_SINGLELINE), fill_clr);
    (void)SelectObject(hdc, h_font_old);
}

/**
 * @brief   命中测试
 * @param   pt 客户区坐标
 * @return  按钮逻辑ID; 无命中返回0
 */
static uint32_t gbw_hit_btn(POINT pt)
{
    uint32_t index = 0u;

    for (index = 0u; index < GBW_BTN_COUNT; index++) {
        if (PtInRect(&g_gbw_btn_rects[index], pt) != 0) {
            return g_gbw_btn_ids[index];
        }
    }

    return 0u;
}

/**
 * @brief   使指定按钮矩形区域重绘
 * @param   hwnd    主窗口句柄
 * @param   ctrl_id 按钮逻辑ID(0忽略)
 * @return  无
 */
static void gbw_invalidate_btn(HWND hwnd, uint32_t ctrl_id)
{
    uint32_t index = 0u;

    if (ctrl_id == 0u) {
        return;
    }
    for (index = 0u; index < GBW_BTN_COUNT; index++) {
        if (g_gbw_btn_ids[index] == ctrl_id) {
            InvalidateRect(hwnd, &g_gbw_btn_rects[index], FALSE);
            return;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 结果展示                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief   将当前节点缓冲填充到树视图(按深度栈挂接父子)
 * @param   h_tree 树视图句柄
 * @return  无
 */
static void gbw_fill_tree(HWND h_tree)
{
    HTREEITEM parents[GBW_TREE_DEPTH_MAX] = {0};
    HTREEITEM roots[GB32960_FRAME_LIST_MAX] = {0};
    uint32_t root_cnt = 0u;
    uint32_t i = 0u;
    uint32_t r = 0u;

    (void)SendMessageW(h_tree, WM_SETREDRAW, (WPARAM)FALSE, (LPARAM)0);
    (void)SendMessageW(h_tree, GBW_TVM_DELETEITEM, 0u, (LPARAM)GBW_TVI_ROOT_VAL);

    for (i = 0u; i < g_gbw_node_count; i++) {
        GBW_TVIS_T vis;
        HTREEITEM h_item = NULL;
        uint32_t depth = (uint32_t)g_gbw_nodes[i].depth;
        uint32_t d = 0u;

        if (depth >= GBW_TREE_DEPTH_MAX) {
            depth = (GBW_TREE_DEPTH_MAX - 1u);
        }
        memset(&vis, 0, sizeof(vis));
        vis.hParent = (depth == 0u) ? (HTREEITEM)GBW_TVI_ROOT_VAL
                                    : ((parents[depth - 1u] != NULL)
                                       ? parents[depth - 1u]
                                       : (HTREEITEM)GBW_TVI_ROOT_VAL);
        vis.hInsertAfter = (HTREEITEM)GBW_TVI_LAST_VAL;
        vis.item.mask = GBW_TVIF_TEXT;
        vis.item.pszText = &g_gbw_nodes[i].text[0];
        h_item = (HTREEITEM)SendMessageW(h_tree, GBW_TVM_INSERTITEMW, 0u, (LPARAM)&vis);
        if (h_item == NULL) {
            continue;
        }
        if ((depth == 0u) && (root_cnt < GB32960_FRAME_LIST_MAX)) {
            roots[root_cnt] = h_item;
            root_cnt++;
        }
        parents[depth] = h_item;
        for (d = (depth + 1u); d < GBW_TREE_DEPTH_MAX; d++) {
            parents[d] = NULL;   /* 深度回跳: 清空更深层父槽 */
        }
    }

    /* 展开全部根节点(帧级)便于概览 */
    for (r = 0u; r < root_cnt; r++) {
        (void)SendMessageW(h_tree, GBW_TVM_EXPAND, (WPARAM)GBW_TVE_EXPAND,
                           (LPARAM)roots[r]);
    }

    (void)SendMessageW(h_tree, WM_SETREDRAW, (WPARAM)TRUE, (LPARAM)0);
    InvalidateRect(h_tree, NULL, FALSE);
}

/**
 * @brief   按当前模式展示结果(树形/文本)
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_show_result(HWND hwnd)
{
    HWND h_tree = GetDlgItem(hwnd, (int)GBW_IDC_TREE_OUT);
    HWND h_ed = GetDlgItem(hwnd, (int)GBW_IDC_ED_OUT);

    if (g_gbw_tree_mode != 0u) {
        ShowWindow(h_ed, SW_HIDE);
        gbw_fill_tree(h_tree);
        ShowWindow(h_tree, SW_SHOW);
    } else {
        ShowWindow(h_tree, SW_HIDE);
        gb32960_nodes_to_text(&g_gbw_nodes[0], g_gbw_node_count,
                              &g_gbw_text[0], GB32960_OUT_MAX_WCHAR);
        SetWindowTextW(h_ed, &g_gbw_text[0]);
        ShowWindow(h_ed, SW_SHOW);
    }
}

/**
 * @brief   刷新解析记录列表(全量重建, 上限100条)
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_refresh_rec_list(HWND hwnd)
{
    HWND h_list = GetDlgItem(hwnd, (int)GBW_IDC_LIST_REC);
    uint32_t count = gb_records_count();
    uint32_t i = 0u;

    (void)SendMessageW(h_list, LB_RESETCONTENT, 0u, 0u);
    for (i = 0u; i < count; i++) {
        wchar_t sum[GB_REC_SUM_WCHAR];

        memset(&sum[0], 0, sizeof(sum));
        gb_records_summary(i, &sum[0], GB_REC_SUM_WCHAR);
        (void)SendMessageW(h_list, LB_ADDSTRING, 0u, (LPARAM)&sum[0]);
    }
    if (count > 0u) {
        (void)SendMessageW(h_list, LB_SETCURSEL, (WPARAM)(count - 1u), 0u);
    }
    InvalidateRect(hwnd, NULL, FALSE);   /* 重绘记录面板标题计数 */
}

/**
 * @brief   追加单条提示节点(错误/提示信息)
 * @param   p_text 提示文本
 * @return  无
 */
static void gbw_set_msg_nodes(const wchar_t *p_text)
{
    memset(&g_gbw_nodes[0], 0, sizeof(GB32960_NODE_T));
    g_gbw_nodes[0].depth = 0u;
    g_gbw_nodes[0].flags = 0u;
    (void)lstrcpynW(&g_gbw_nodes[0].text[0], p_text, (int)GB32960_NODE_TEXT_WCHAR);
    g_gbw_node_count = 1u;
}

/* -------------------------------------------------------------------------- */
/*  内部私有函数: 动作                                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief   执行解析: 输入区hex -> 切帧 -> 逐帧解析(入库+节点) -> 展示
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_on_parse(HWND hwnd)
{
    HWND h_in = GetDlgItem(hwnd, (int)GBW_IDC_ED_IN);
    uint32_t in_len = (uint32_t)GetWindowTextLengthW(h_in);
    wchar_t *p_in = NULL;
    uint32_t frames = 0u;
    uint32_t count = 0u;
    uint32_t i = 0u;
    wchar_t err[160];

    if (in_len == 0u) {
        gbw_set_msg_nodes(L"提示: 请先粘贴或加载 hex 报文");
        gbw_show_result(hwnd);
        return;
    }
    p_in = (wchar_t *)HeapAlloc(GetProcessHeap(), 0u,
                               ((SIZE_T)in_len + 2u) * sizeof(wchar_t));
    if (p_in == NULL) {
        gbw_set_msg_nodes(L"错误: 内存不足");
        gbw_show_result(hwnd);
        return;
    }
    (void)GetWindowTextW(h_in, p_in, (int32_t)(in_len + 1u));
    memset(&err[0], 0, sizeof(err));
    frames = gb32960_decode_split(p_in, &g_gbw_bytes[0], GB32960_FRAME_MAX,
                                  &g_gbw_offs[0], &g_gbw_lens[0], GB32960_FRAME_LIST_MAX,
                                  &err[0], (uint32_t)(sizeof(err) / sizeof(err[0])));
    (void)HeapFree(GetProcessHeap(), 0u, p_in);

    if (frames == 0u) {
        gbw_set_msg_nodes((err[0] != L'\0') ? &err[0] : L"✗ 解析失败");
        gbw_show_result(hwnd);
        return;
    }

    for (i = 0u; i < frames; i++) {
        if (gb32960_parse_frame_nodes(&g_gbw_bytes[g_gbw_offs[i]], g_gbw_lens[i],
                                      (i + 1u), &g_gbw_nodes[0], &count,
                                      GB32960_NODE_MAX) != 0u) {
            (void)gb_records_add(&g_gbw_bytes[g_gbw_offs[i]], g_gbw_lens[i]);
        }
    }
    g_gbw_node_count = count;
    gbw_refresh_rec_list(hwnd);
    gbw_show_result(hwnd);
}

/**
 * @brief   打开报文文件(读取全部内容到输入区并解析)
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_on_open(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t path[MAX_PATH];
    HANDLE h_file = NULL;
    DWORD size = 0u;
    DWORD read_back = 0u;
    char *p_raw = NULL;
    int32_t wide_len = 0u;
    wchar_t *p_wide = NULL;

    memset(&ofn, 0, sizeof(ofn));
    memset(&path[0], 0, sizeof(path));
    ofn.lStructSize = (DWORD)sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"报文文本\0*.txt;*.log;*.hex\0所有文件\0*.*\0";
    ofn.lpstrFile = &path[0];
    ofn.nMaxFile = (DWORD)MAX_PATH;
    ofn.Flags = (DWORD)(OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);

    if (GetOpenFileNameW(&ofn) == 0) {
        return;
    }
    h_file = CreateFileW(&path[0], GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, 0u, NULL);
    if (h_file == INVALID_HANDLE_VALUE) {
        return;
    }
    size = GetFileSize(h_file, NULL);
    if ((size == 0u) || (size > (GB32960_IN_MAX_WCHAR * 2u))) {
        (void)CloseHandle(h_file);
        gbw_set_msg_nodes(L"错误: 文件为空或过大(上限64KB)");
        gbw_show_result(hwnd);
        return;
    }
    p_raw = (char *)HeapAlloc(GetProcessHeap(), 0u, (SIZE_T)size + 2u);
    p_wide = (wchar_t *)HeapAlloc(GetProcessHeap(), 0u,
                                  ((SIZE_T)GB32960_IN_MAX_WCHAR + 2u) * sizeof(wchar_t));
    if ((p_raw == NULL) || (p_wide == NULL)) {
        if (p_raw != NULL) {
            (void)HeapFree(GetProcessHeap(), 0u, p_raw);
        }
        if (p_wide != NULL) {
            (void)HeapFree(GetProcessHeap(), 0u, p_wide);
        }
        (void)CloseHandle(h_file);
        return;
    }
    (void)ReadFile(h_file, p_raw, size, &read_back, NULL);
    (void)CloseHandle(h_file);
    p_raw[read_back] = '\0';
    p_raw[read_back + 1u] = '\0';

    wide_len = MultiByteToWideChar(CP_ACP, 0u, p_raw, -1, p_wide,
                                   (int32_t)GB32960_IN_MAX_WCHAR);
    if (wide_len <= 0) {
        wide_len = MultiByteToWideChar(CP_UTF8, 0u, p_raw, -1, p_wide,
                                       (int32_t)GB32960_IN_MAX_WCHAR);
    }
    if (wide_len > 0) {
        SetWindowTextW(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN), p_wide);
        gbw_on_parse(hwnd);   /* 加载后立即解析 */
    }
    (void)HeapFree(GetProcessHeap(), 0u, p_raw);
    (void)HeapFree(GetProcessHeap(), 0u, p_wide);
}

/**
 * @brief   复制当前结果(节点物化为文本, 树形/文本模式一致)
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_on_copy(HWND hwnd)
{
    if (g_gbw_node_count == 0u) {
        return;
    }
    gb32960_nodes_to_text(&g_gbw_nodes[0], g_gbw_node_count,
                          &g_gbw_text[0], GB32960_OUT_MAX_WCHAR);
    (void)ui_common_copy_to_clipboard(hwnd, &g_gbw_text[0]);
}

/**
 * @brief   保存当前结果到文件(UTF-8 BOM 文本)
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_on_save(HWND hwnd)
{
    OPENFILENAMEW ofn;
    wchar_t path[MAX_PATH];
    HANDLE h_file = NULL;
    DWORD written = 0u;
    int32_t utf8_len = 0u;
    char *p_utf8 = NULL;
    static const char UTF8_BOM[3u] = {(char)0xEF, (char)0xBB, (char)0xBF};

    if (g_gbw_node_count == 0u) {
        return;
    }
    memset(&ofn, 0, sizeof(ofn));
    memset(&path[0], 0, sizeof(path));
    lstrcpynW(&path[0], L"gb32960_result.txt", MAX_PATH);
    ofn.lStructSize = (DWORD)sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"文本文件\0*.txt\0所有文件\0*.*\0";
    ofn.lpstrFile = &path[0];
    ofn.nMaxFile = (DWORD)MAX_PATH;
    ofn.Flags = (DWORD)(OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);

    if (GetSaveFileNameW(&ofn) == 0) {
        return;
    }
    gb32960_nodes_to_text(&g_gbw_nodes[0], g_gbw_node_count,
                          &g_gbw_text[0], GB32960_OUT_MAX_WCHAR);
    utf8_len = WideCharToMultiByte(CP_UTF8, 0u, &g_gbw_text[0], -1, NULL, 0u,
                                   NULL, NULL);
    if (utf8_len <= 0) {
        return;
    }
    p_utf8 = (char *)HeapAlloc(GetProcessHeap(), 0u, (SIZE_T)utf8_len + 4u);
    if (p_utf8 == NULL) {
        return;
    }
    (void)WideCharToMultiByte(CP_UTF8, 0u, &g_gbw_text[0], -1,
                              &p_utf8[3u], utf8_len, NULL, NULL);
    memcpy(&p_utf8[0], &UTF8_BOM[0], 3u);

    h_file = CreateFileW(&path[0], GENERIC_WRITE, 0u, NULL,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h_file != INVALID_HANDLE_VALUE) {
        (void)WriteFile(h_file, p_utf8, (DWORD)(utf8_len + 2u), &written, NULL);
        (void)CloseHandle(h_file);
    }
    (void)HeapFree(GetProcessHeap(), 0u, p_utf8);
}

/**
 * @brief   记录列表选中: 重显该帧解析结果
 * @param   hwnd 主窗口句柄
 * @return  无
 */
static void gbw_on_rec_sel(HWND hwnd)
{
    HWND h_list = GetDlgItem(hwnd, (int)GBW_IDC_LIST_REC);
    LRESULT sel = SendMessageW(h_list, LB_GETCURSEL, 0u, 0u);
    const uint8_t *p_frame = NULL;
    uint32_t len = 0u;
    uint32_t count = 0u;

    if ((sel < 0) || ((uint32_t)sel >= gb_records_count())) {
        return;
    }
    p_frame = gb_records_get((uint32_t)sel, &len);
    if (p_frame == NULL) {
        return;
    }
    (void)gb32960_parse_frame_nodes(p_frame, len, ((uint32_t)sel + 1u),
                                    &g_gbw_nodes[0], &count, GB32960_NODE_MAX);
    g_gbw_node_count = count;
    gbw_show_result(hwnd);
}

/**
 * @brief   处理自绘按钮动作
 * @param   hwnd    主窗口句柄
 * @param   ctrl_id 按钮逻辑ID
 * @return  无
 */
static void gbw_on_button(HWND hwnd, uint32_t ctrl_id)
{
    switch (ctrl_id)
    {
        case GBW_IDC_BTN_PARSE:
        {
            gbw_on_parse(hwnd);
            break;
        }
        case GBW_IDC_BTN_OPEN:
        {
            gbw_on_open(hwnd);
            break;
        }
        case GBW_IDC_BTN_CLEAR:
        {
            SetWindowTextW(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN), L"");
            gbw_set_msg_nodes(L"");
            g_gbw_node_count = 0u;
            gbw_show_result(hwnd);
            SetFocus(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN));
            break;
        }
        case GBW_IDC_BTN_COPY:
        {
            gbw_on_copy(hwnd);
            break;
        }
        case GBW_IDC_BTN_SAVE:
        {
            gbw_on_save(hwnd);
            break;
        }
        case GBW_IDC_BTN_MODE:
        {
            g_gbw_tree_mode = (g_gbw_tree_mode != 0u) ? 0u : 1u;
            gbw_show_result(hwnd);
            break;
        }
        case GBW_IDC_BTN_RECCLR:
        {
            gb_records_clear();
            gbw_refresh_rec_list(hwnd);
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
 * @brief   主窗口消息处理过程
 * @param   hwnd  窗口句柄
 * @param   msg   消息类型
 * @param   wp    消息参数1
 * @param   lp    消息参数2
 * @return  消息处理结果
 */
static LRESULT CALLBACK gbw_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            gbw_create_controls(hwnd);
            gbw_show_result(hwnd);   /* 初始按模式显隐结果控件 */
            SetFocus(GetDlgItem(hwnd, (int)GBW_IDC_ED_IN));
            return 0;
        }
        case WM_SIZE:
        {
            gbw_layout(hwnd);
            return 0;
        }
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO *p_mmi = (MINMAXINFO *)lp;

            p_mmi->ptMinTrackSize.x = GBW_MIN_TRACK_W;
            p_mmi->ptMinTrackSize.y = GBW_MIN_TRACK_H;
            return 0;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc;
            uint32_t index = 0u;
            wchar_t title[48];
            HFONT h_font_old = NULL;

            memset(&ps, 0, sizeof(ps));
            hdc = BeginPaint(hwnd, &ps);
            SetBkMode(hdc, TRANSPARENT);
            for (index = 0u; index < GBW_BTN_COUNT; index++) {
                gbw_draw_button(hdc, index);
            }
            /* 记录面板标题(含计数) */
            (void)wsprintfW(&title[0], L"解析记录 (%u)",
                            (unsigned int)gb_records_count());
            h_font_old = (HFONT)SelectObject(hdc, ui_common_get_font(UI_FONT_KIND_BTN));
            SetTextColor(hdc, (COLORREF)GBW_CLR_TEXT_DARK);
            ui_common_text_crisp(hdc, &g_gbw_rec_title_rect, &title[0],
                                 (DT_LEFT | DT_VCENTER | DT_SINGLELINE),
                                 (uint32_t)GetSysColor(COLOR_BTNFACE));
            (void)SelectObject(hdc, h_font_old);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_COMMAND:
        {
            uint32_t id = (uint32_t)LOWORD(wp);
            uint32_t code = (uint32_t)HIWORD(wp);

            if ((id == (uint32_t)GBW_IDC_LIST_REC) && (code == (uint32_t)LBN_SELCHANGE)) {
                gbw_on_rec_sel(hwnd);
            }
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            POINT pt;
            uint32_t id = 0u;
            uint32_t old_id = g_gbw_hover_id;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            id = gbw_hit_btn(pt);
            if (id != old_id) {
                g_gbw_hover_id = id;
                gbw_invalidate_btn(hwnd, old_id);
                gbw_invalidate_btn(hwnd, id);
            }
            if (g_gbw_mouse_tracking == 0u) {
                TRACKMOUSEEVENT tme;
                memset(&tme, 0, sizeof(tme));
                tme.cbSize = (DWORD)sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                (void)TrackMouseEvent(&tme);
                g_gbw_mouse_tracking = 1u;
            }
            break;
        }
        case WM_MOUSELEAVE:
        {
            gbw_invalidate_btn(hwnd, g_gbw_hover_id);
            g_gbw_hover_id = 0u;
            g_gbw_mouse_tracking = 0u;
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            POINT pt;

            pt.x = (int32_t)(int16_t)LOWORD(lp);
            pt.y = (int32_t)(int16_t)HIWORD(lp);
            g_gbw_press_id = gbw_hit_btn(pt);
            if (g_gbw_press_id != 0u) {
                gbw_invalidate_btn(hwnd, g_gbw_press_id);
                SetCapture(hwnd);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        {
            if (g_gbw_press_id != 0u) {
                POINT pt;
                uint32_t id = 0u;

                ReleaseCapture();
                pt.x = (int32_t)(int16_t)LOWORD(lp);
                pt.y = (int32_t)(int16_t)HIWORD(lp);
                id = gbw_hit_btn(pt);
                if (id == g_gbw_press_id) {
                    gbw_on_button(hwnd, id);
                }
                gbw_invalidate_btn(hwnd, g_gbw_press_id);
                g_gbw_press_id = 0u;
            }
            return 0;
        }
        case WM_SETCURSOR:
        {
            if ((LOWORD(wp) == HTCLIENT) && (g_gbw_hover_id != 0u)) {
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND));
                return (LRESULT)1u;
            }
            break;
        }
        case WM_DESTROY:
        {
            if (GetWindow(hwnd, GW_OWNER) == NULL) {
                PostQuitMessage(0);
            }
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

/**
 * @brief   打开 GB32960 解析工具窗口(可重复打开多个实例)
 * @param   h_inst  应用实例句柄
 * @param   h_owner 所有者窗口句柄
 * @return  无
 */
void tool_gb32960_open(HINSTANCE h_inst, HWND h_owner)
{
    WNDCLASSW wc;
    HWND h_wnd = NULL;

    if (g_gbw_class_ready == 0u) {
        InitCommonControls();   /* 树视图/列表公共控件注册 */
        memset(&wc, 0, sizeof(wc));
        wc.style = (UINT)(CS_HREDRAW | CS_VREDRAW);
        wc.lpfnWndProc = gbw_wnd_proc;
        wc.hInstance = h_inst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = GBW_CLASS_NAME;
        (void)RegisterClassW(&wc);
        g_gbw_class_ready = 1u;
    }

    g_gbw_instance = h_inst;

    h_wnd = CreateWindowW(GBW_CLASS_NAME, GBW_WND_TITLE,
                          (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN),
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          GBW_WND_WIDTH, GBW_WND_HEIGHT,
                          h_owner, NULL, h_inst, NULL);
    if (h_wnd != NULL) {
        ShowWindow(h_wnd, SW_SHOWNORMAL);
        UpdateWindow(h_wnd);
    }
}
