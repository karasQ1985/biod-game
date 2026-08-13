#!/usr/bin/env python3
"""
Biod 参数调节器 - 可视化修改项目中硬编码的上限数值
无需手动编辑源文件，修改后自动应用到所有相关代码。
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import re
import os
import shutil
import subprocess
import threading
from dataclasses import dataclass, field
from typing import List, Callable

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ============================================================
# 参数定义：每个参数包含中文标签、描述、定位模式、替换逻辑
# ============================================================

@dataclass
class ParamDef:
    param_id: str                # 唯一标识
    group: str                   # 分组名
    label: str                   # 中文标签
    desc: str                    # 参数说明（悬停提示）
    file_path: str               # 相对项目根路径
    # 查找锚点：此行或上下文在文件中唯一出现
    anchor: str                  # 锚点字符串（用于定位行）
    # 正则：从锚点所在行中提取当前数值
    extract_re: str              # 正则，第一个捕获组=当前值
    # 替换：{value} 将被替换为实际数值
    replace_template: str        # 替换模板，{value} 占位
    default: int                 # 默认值
    min_val: int                 # 最小值
    max_val: int                 # 最大值
    step: int = 1                # 步进
    unit: str = ""               # 单位
    sync_group: str = ""         # 同步组ID（同组参数必须值相同）


# ============================================================
# 参数注册表
# ============================================================

PARAMS: List[ParamDef] = [
    # ── 全局 Boid 总数 ──
    ParamDef(
        "global_cap_init", "全局粒子上限",
        "Simulation 初始化上限 (m_maxBoids)",
        "全局粒子容量，控制模拟世界中最多可同时存在的粒子数。此值写入 m_maxBoids，所有 spawn 逻辑以此为硬边界。",
        "src/ui/GLWidget.cpp",
        "m_sim.init(worldW, worldH,",
        r"m_sim\.init\(worldW,\s*worldH,\s*(\d+)\)",
        "    m_sim.init(worldW, worldH, {value});\n",
        default=10000, min_val=100, max_val=100000, step=100,
        sync_group="global_cap"
    ),
    ParamDef(
        "global_cap_renderer_vbo", "全局粒子上限",
        "Renderer GPU VBO 缓冲区大小",
        "OpenGL 实例化渲染的顶点缓冲区预分配。必须 ≥ global_cap_init，否则超出部分不渲染且可能崩溃。",
        "src/renderer/Renderer.cpp",
        "glBufferData(GL_ARRAY_BUFFER, 10000 * INSTANCE_STRIDE * sizeof(float)",
        r"glBufferData\(GL_ARRAY_BUFFER,\s*(\d+)\s*\*\s*INSTANCE_STRIDE\s*\*\s*sizeof\(float\)",
        "    glBufferData(GL_ARRAY_BUFFER, {value} * INSTANCE_STRIDE * sizeof(float), nullptr, GL_DYNAMIC_DRAW);\n",
        default=10000, min_val=100, max_val=100000, step=100,
        sync_group="global_cap"
    ),
    ParamDef(
        "global_cap_renderer_cpu", "全局粒子上限",
        "Renderer CPU 实例缓冲区预分配",
        "CPU 端实例数据 vector 的 reserve 容量。必须与 GPU VBO 一致，否则 resize 可能触发多余分配。",
        "src/renderer/Renderer.cpp",
        "m_instanceData.reserve(10000 * INSTANCE_STRIDE)",
        r"m_instanceData\.reserve\((\d+)\s*\*\s*INSTANCE_STRIDE\)",
        "    m_instanceData.reserve({value} * INSTANCE_STRIDE);\n",
        default=10000, min_val=100, max_val=100000, step=100,
        sync_group="global_cap"
    ),
    ParamDef(
        "global_cap_slider", "全局粒子上限",
        "UI 全局上限滑块最大值",
        "主窗口滑块 m_globalCapSlider 的上限，决定用户可通过滑块调到的最大值。",
        "src/ui/MainWindow.cpp",
        "m_globalCapSlider->setRange(100,",
        r"m_globalCapSlider->setRange\(100,\s*(\d+)\)",
        "        m_globalCapSlider->setRange(100, {value});\n",
        default=10000, min_val=100, max_val=100000, step=100,
        sync_group="global_cap"
    ),

    # ── 单群体内粒子上限 ──
    ParamDef(
        "per_flock_default", "单群体粒子上限",
        "ReproductionParams::maxFlockSize 默认值",
        "每个群体内部可容纳粒子的默认上限。新建群体继承此值，单独修改不影响已有群体。",
        "src/simulation/FlockData.h",
        "int   maxFlockSize = 2000;",
        r"int\s+maxFlockSize\s*=\s*(\d+)\s*;",
        "    int   maxFlockSize = {value};\n",
        default=2000, min_val=50, max_val=50000, step=100,
        sync_group="per_flock"
    ),
    ParamDef(
        "per_flock_globalcap_clamp", "单群体粒子上限",
        "setGlobalFlockCap() 硬钳制值",
        "全局滑块改变时此函数会将所有群体的 maxFlockSize 同步为此值，但当前硬编码钳制为旧默认值，是一处设计缺陷。",
        "src/simulation/Simulation.cpp",
        "if (cap > 2000) cap = 2000;",
        r"if\s*\(cap\s*>\s*(\d+)\)\s*cap\s*=\s*(\d+)\s*;",
        "    if (cap > {value}) cap = {value};\n",
        default=2000, min_val=50, max_val=50000, step=100,
        sync_group="per_flock"
    ),
    ParamDef(
        "per_flock_slider", "单群体粒子上限",
        "单群体上限滑块最大值",
        "调整单个群体参数的面板中「群体上限 (Max Flock Size)」滑块的最大值。",
        "src/ui/MainWindow.cpp",
        'maxFlockSize),             50, 5000, ScaleMode::OneToOne',
        r'maxFlockSize\),\s*50,\s*(\d+),\s*ScaleMode::OneToOne',
        '    reg.reg({{"\\u7fa4\\u4f53\\u4e0a\\u9650 (Max Flock Size)", "repro", HO_N(reproduction, maxFlockSize),             50, {value}, ScaleMode::OneToOne,  p.reproduction.maxFlockSize,                              BASE_FLOCK}});\n',
        default=5000, min_val=500, max_val=100000, step=500,
        sync_group="per_flock_slider"
    ),

    # ── 群体种类上限 ──
    ParamDef(
        "max_flocks", "群体种类上限",
        "MAX_FLOCKS 最大群体种类数",
        "模拟中可同时存在的群体种类数量上限。增大需确认 UI 布局和 FlockParams 数组是否正确适配。",
        "src/simulation/Simulation.h",
        "constexpr int MAX_FLOCKS = 12;",
        r"constexpr\s+int\s+MAX_FLOCKS\s*=\s*(\d+)\s*;",
        "inline constexpr int MAX_FLOCKS = {value};\n",
        default=12, min_val=2, max_val=64, step=1,
    ),

    # ── 植物数量上限 ──
    ParamDef(
        "max_plants_default", "植物数量上限",
        "PlantParams::maxPlants 默认值",
        "世界中最多可同时存在的植物实体数量。植物被吃光后可再生，此值禁止 total count 超过数量。",
        "src/simulation/PlantData.h",
        "float maxPlants = 200.0f;",
        r"float\s+maxPlants\s*=\s*([\d.]+)f\s*;",
        "    float maxPlants = {value}.0f;\n",
        default=200, min_val=10, max_val=50000, step=10,
        sync_group="max_plants"
    ),
    ParamDef(
        "max_plants_slider", "植物数量上限",
        "植物滑块最大值",
        "植物生态面板中「最大数量 (Max Plants)」滑块的最大可选值。",
        "src/ui/MainWindow.cpp",
        'maxPlants),       10,  500, ScaleMode::OneToOne',
        r'maxPlants\),\s*10,\s*(\d+),\s*ScaleMode::OneToOne',
        '    reg.reg({{"\\u6700\\u5927\\u6570\\u91cf (Max Plants)",          "plants", PO(maxPlants),       10,  {value}, ScaleMode::OneToOne,  static_cast<int>(pp.maxPlants),  BASE_PLANT}});\n',
        default=500, min_val=100, max_val=100000, step=100,
        sync_group="max_plants_slider"
    ),
    ParamDef(
        "initial_plants", "植物数量上限",
        "初始植物数量",
        "世界初始化时随机生成的植物数量。必须 ≤ maxPlants，否则初始化时会被截断。",
        "src/simulation/PlantData.h",
        "float initialPlants = 80.0f;",
        r"float\s+initialPlants\s*=\s*([\d.]+)f\s*;",
        "    float initialPlants = {value}.0f;\n",
        default=80, min_val=0, max_val=50000, step=10,
    ),
]


# ============================================================
# 同步组处理
# ============================================================

def _group_members() -> dict:
    """返回 {sync_group: [param_ids]} 的映射"""
    g: dict = {}
    for p in PARAMS:
        if p.sync_group:
            g.setdefault(p.sync_group, []).append(p.param_id)
    return g

SYNC_GROUPS = _group_members()


# ============================================================
# 文件修改引擎
# ============================================================

def _read_file(rel_path: str) -> str:
    """读取项目中的源文件"""
    full = os.path.join(PROJECT_ROOT, rel_path)
    with open(full, "r", encoding="utf-8") as f:
        return f.read()


def _write_file(rel_path: str, content: str):
    """写入项目中的源文件，自动创建 .bak 备份"""
    full = os.path.join(PROJECT_ROOT, rel_path)
    bak = full + ".bak"
    if not os.path.exists(bak):
        shutil.copy2(full, bak)
    with open(full, "w", encoding="utf-8") as f:
        f.write(content)


def get_current_value(param: ParamDef) -> int | None:
    """从源文件中读取当前参数值"""
    try:
        content = _read_file(param.file_path)
    except FileNotFoundError:
        return None
    match = re.search(param.extract_re, content)
    if match:
        val = match.group(1)
        # 处理 C++ float 字面量后缀
        if val.endswith("f"):
            val = val[:-1]
        try:
            if "." in val:
                return int(float(val))
            return int(val)
        except ValueError:
            return None
    return None


def apply_param(param: ParamDef, new_value: int) -> bool:
    """将单个参数的新值写回源文件。

    使用 extract_re 正则定位目标行，通过每个捕获组的起止位置
    精确替换数值，保留行内所有其他内容不变。
    支持多捕获组（如 if (cap > X) cap = Y; 两处数值需同步替换）。"""
    try:
        content = _read_file(param.file_path)
    except FileNotFoundError:
        return False

    match = re.search(param.extract_re, content)
    if not match or match.lastindex is None or match.lastindex < 1:
        return False

    matched_text = match.group(0)
    new_text = matched_text

    # 从右到左替换所有捕获组，避免前序替换影响后续偏移
    for gi in range(match.lastindex, 0, -1):
        old_val = match.group(gi)
        g_start = match.start(gi) - match.start(0)
        g_end   = match.end(gi)   - match.start(0)

        # 如果原始值是 float 格式（含小数点或后缀 f），保持 .0f 格式
        is_float = ("." in old_val) or old_val.endswith("f")
        if is_float:
            formatted = f"{new_value}.0f"
            # 如果 group 没捕获到 f，那么后面紧跟的就是 f，跳过它
            if not old_val.endswith("f") and g_end < len(new_text) and new_text[g_end] == "f":
                g_end += 1
        else:
            formatted = str(new_value)

        new_text = new_text[:g_start] + formatted + new_text[g_end:]

    # 用 re.sub 仅替换第一次出现
    escaped = re.escape(matched_text)
    new_content = re.sub(escaped, lambda _: new_text, content, count=1)

    _write_file(param.file_path, new_content)
    return True


def restore_defaults(param_ids: list[str] | None = None) -> int:
    """恢复参数为默认值。如不指定 param_ids 则恢复全部。返回成功数量。"""
    targets = PARAMS
    if param_ids:
        id_set = set(param_ids)
        targets = [p for p in PARAMS if p.param_id in id_set]

    success = 0
    for p in targets:
        try:
            if apply_param(p, p.default):
                success += 1
        except Exception:
            pass
    return success


# ============================================================
# GUI 窗口
# ============================================================

class ParamTunerApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Biod 参数调节器 — Param Tuner")
        self.root.geometry("820x680")
        self.root.resizable(True, True)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        # ── 顶部提示 ──
        top = ttk.Frame(root, padding=10)
        top.pack(fill=tk.X)
        ttk.Label(top, text="Biod 参数调节器",
                  font=("Microsoft YaHei", 16, "bold")).pack(anchor=tk.CENTER)
        tip = (
            "修改下方参数后点击 [应用修改] 自动更新所有相关源文件。\n"
            "首次应用自动创建 .bak 备份文件；[复位全部] 恢复所有参数的出厂默认值。"
        )
        ttk.Label(top, text=tip, foreground="#555",
                  font=("Microsoft YaHei", 9)).pack(anchor=tk.CENTER, pady=(5, 0))

        # ── 主区域：Canvas + 滚动条 ──
        main = ttk.Frame(root)
        main.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        self.canvas = tk.Canvas(main, borderwidth=0, highlightthickness=0)
        self.scrollbar = ttk.Scrollbar(main, orient=tk.VERTICAL, command=self.canvas.yview)
        self.scroll_frame = ttk.Frame(self.canvas)
        self.scroll_frame.bind("<Configure>",
                               lambda e: self.canvas.configure(scrollregion=self.canvas.bbox("all")))
        self.canvas.create_window((0, 0), window=self.scroll_frame, anchor=tk.NW)
        self.canvas.configure(yscrollcommand=self.scrollbar.set)
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.canvas.bind_all("<MouseWheel>", self._on_mousewheel)

        # ── 按分组渲染参数卡片 ──
        self.param_widgets: dict[str, dict] = {}  # param_id -> {"var": IntVar, "label": Label, ...}

        groups_order = ["全局粒子上限", "单群体粒子上限", "群体种类上限", "植物数量上限", "渲染性能"]
        group_params: dict[str, list[ParamDef]] = {}
        for p in PARAMS:
            group_params.setdefault(p.group, []).append(p)

        row = 0
        for gname in groups_order:
            plist = group_params.get(gname, [])
            if not plist:
                continue
            ttk.Label(self.scroll_frame, text=f"▎ {gname}",
                      font=("Microsoft YaHei", 12, "bold"),
                      foreground="#2a6e9e").grid(row=row, column=0, columnspan=3,
                                                  sticky=tk.W, padx=10, pady=(15, 5))
            row += 1

            for p in plist:
                current = get_current_value(p)
                if current is None:
                    current = p.default

                frame = ttk.Frame(self.scroll_frame, relief=tk.GROOVE, padding=8)
                frame.grid(row=row, column=0, columnspan=3, sticky=tk.EW, padx=10, pady=3)

                # 标签行（参数名）
                lbl = ttk.Label(frame, text=p.label,
                                font=("Microsoft YaHei", 10, "bold"))
                lbl.grid(row=0, column=0, sticky=tk.W)

                # 说明文字（灰色小字）
                desc_lbl = ttk.Label(frame, text=p.desc, wraplength=550,
                                     foreground="#777",
                                     font=("Microsoft YaHei", 8))
                desc_lbl.grid(row=1, column=0, columnspan=2, sticky=tk.W, pady=(2, 6))

                # 滑块 + 输入框
                var = tk.IntVar(value=current)
                scale = ttk.Scale(frame, from_=p.min_val, to=p.max_val,
                                  variable=var, orient=tk.HORIZONTAL, length=400,
                                  command=lambda v, pid=p.param_id: self._on_scale_change(pid))
                scale.grid(row=2, column=0, sticky=tk.W, padx=(0, 10))

                entry = ttk.Entry(frame, textvariable=var, width=8,
                                  font=("Consolas", 11))
                entry.grid(row=2, column=1, sticky=tk.W, padx=(0, 10))
                entry.bind("<Return>", lambda e, pid=p.param_id: self._on_entry_confirm(pid))
                entry.bind("<FocusOut>", lambda e, pid=p.param_id: self._on_entry_confirm(pid))

                # 单位标签
                if p.unit:
                    ttk.Label(frame, text=p.unit, foreground="#888").grid(row=2, column=1,
                                                                           sticky=tk.E, padx=(65, 10))

                # 复位按钮（单个参数）
                reset_btn = ttk.Button(frame, text="↺ 复位",
                                       command=lambda pid=p.param_id: self._reset_one(pid))
                reset_btn.grid(row=2, column=2, sticky=tk.E, padx=(5, 5))

                # 当前文件提示
                file_lbl = ttk.Label(frame, text=f"文件: {p.file_path}",
                                     foreground="#aaa", font=("Consolas", 7))
                file_lbl.grid(row=3, column=0, columnspan=3, sticky=tk.W, pady=(2, 0))

                # 存储引用
                self.param_widgets[p.param_id] = {
                    "var": var,
                    "scale": scale,
                    "entry": entry,
                    "label": lbl,
                    "param": p,
                }

                row += 1

            # 同组复位全部按钮
            btn_frame = ttk.Frame(self.scroll_frame)
            btn_frame.grid(row=row, column=0, columnspan=3, sticky=tk.E, padx=10, pady=(0, 8))
            ttk.Button(btn_frame, text=f"复位「{gname}」组全部参数",
                       command=lambda g=gname: self._reset_group(g)).pack(side=tk.RIGHT)
            row += 1

        # ── 底部操作栏 ──
        bottom = ttk.Frame(root, padding=10)
        bottom.pack(fill=tk.X, side=tk.BOTTOM)

        ttk.Button(bottom, text="↻ 复位全部参数为默认值",
                   command=self._reset_all).pack(side=tk.LEFT, padx=(0, 20))

        self.status_var = tk.StringVar(value="就绪 — 尚未应用修改")
        ttk.Label(bottom, textvariable=self.status_var, foreground="#555").pack(
            side=tk.LEFT, expand=True)

        ttk.Button(bottom, text="▶ 编译并运行", command=self._build_and_run,
                   style="Accent.TButton").pack(side=tk.RIGHT, padx=(20, 5))
        ttk.Button(bottom, text="✔ 应用修改", command=self._apply_all,
                   style="Accent.TButton").pack(side=tk.RIGHT, padx=(5, 0))
        ttk.Button(bottom, text="刷新当前值", command=self._refresh_all).pack(side=tk.RIGHT, padx=(0, 10))

        # ── 构建输出面板（默认折叠） ──
        self.build_frame = ttk.Frame(root)
        # 不立即 pack，在 _build_and_run 中显示

        self.build_output = scrolledtext.ScrolledText(
            self.build_frame, height=8, wrap=tk.WORD,
            font=("Consolas", 9), background="#1e1e1e", foreground="#d4d4d4",
            insertbackground="#d4d4d4", state=tk.DISABLED)
        self.build_output.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 5))

        # 运行按钮（编译成功后显示）
        run_bar = ttk.Frame(self.build_frame)
        run_bar.pack(fill=tk.X, padx=10, pady=(0, 5))
        self.build_status_label = ttk.Label(run_bar, text="", foreground="#888")
        self.build_status_label.pack(side=tk.LEFT)
        self.run_btn = ttk.Button(run_bar, text="▷ 运行 Biod.exe",
                                  command=self._run_binary, state=tk.DISABLED)
        self.run_btn.pack(side=tk.RIGHT)
        self.close_build_btn = ttk.Button(run_bar, text="✕ 关闭面板",
                                          command=self._hide_build_panel)
        self.close_build_btn.pack(side=tk.RIGHT, padx=(0, 10))

    # ── 事件处理 ──

    def _on_mousewheel(self, event):
        self.canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

    def _on_scale_change(self, param_id: str):
        """滑块拖拽时更新输入框数值并高亮变化"""
        w = self.param_widgets.get(param_id)
        if w and w["var"].get() != w["param"].default:
            w["label"].configure(foreground="#d4641c")
        elif w:
            w["label"].configure(foreground="#000")

    def _on_entry_confirm(self, param_id: str):
        """输入框回车/失焦时钳制数值范围"""
        w = self.param_widgets.get(param_id)
        if not w:
            return
        val = w["var"].get()
        p = w["param"]
        val = max(p.min_val, min(p.max_val, val))
        w["var"].set(val)
        if val != p.default:
            w["label"].configure(foreground="#d4641c")
        else:
            w["label"].configure(foreground="#000")

    def _reset_one(self, param_id: str):
        """复位单个参数"""
        w = self.param_widgets.get(param_id)
        if not w:
            return
        w["var"].set(w["param"].default)
        w["label"].configure(foreground="#000")

    def _reset_group(self, group_name: str):
        """复位一个分组的所有参数"""
        count = 0
        for p in PARAMS:
            if p.group == group_name:
                w = self.param_widgets.get(p.param_id)
                if w:
                    w["var"].set(p.default)
                    w["label"].configure(foreground="#000")
                    count += 1
        self.status_var.set(f"已复位「{group_name}」组 {count} 个参数 — 未应用")

    def _reset_all(self):
        """复位所有参数到默认值"""
        for p in PARAMS:
            w = self.param_widgets.get(p.param_id)
            if w:
                w["var"].set(p.default)
                w["label"].configure(foreground="#000")
        self.status_var.set(f"已复位全部 {len(PARAMS)} 个参数 — 未应用")

    def _refresh_all(self):
        """从文件重新读取当前值并更新控件"""
        for p in PARAMS:
            w = self.param_widgets.get(p.param_id)
            if not w:
                continue
            val = get_current_value(p)
            if val is not None:
                w["var"].set(val)
                if val != p.default:
                    w["label"].configure(foreground="#d4641c")
                else:
                    w["label"].configure(foreground="#000")
        self.status_var.set("已从文件刷新当前值")

    def _apply_all(self):
        """将所有参数的当前值写回源文件"""
        # 同步组校验：同组参数必须值相同
        for gname, ids in SYNC_GROUPS.items():
            ref_val = None
            for pid in ids:
                w = self.param_widgets.get(pid)
                if not w:
                    continue
                if ref_val is None:
                    ref_val = w["var"].get()
                elif w["var"].get() != ref_val:
                    messagebox.showwarning(
                        "同步组不一致",
                        f"同组的「{w['param'].label}」({w['var'].get()}) 与同组其他参数 ({ref_val}) 不一致。\n"
                        f"请将它们设为同一数值后再应用。",
                    )
                    return

        # 逐个应用
        success = 0
        fail = 0
        for p in PARAMS:
            w = self.param_widgets.get(p.param_id)
            if not w:
                continue
            new_val = w["var"].get()
            try:
                if apply_param(p, new_val):
                    w["label"].configure(
                        foreground="#000" if new_val == p.default else "#2a6e2a")
                    success += 1
                else:
                    fail += 1
            except Exception as e:
                fail += 1
                messagebox.showerror("写入失败", f"写入 {p.file_path} 时出错:\n{e}")

        self.status_var.set(
            f"应用完成 — 成功 {success} 项，失败 {fail} 项。重新编译以生效。")

        if success > 0:
            messagebox.showinfo(
                "应用成功",
                f"已修改 {success} 个参数（{fail} 个失败）。\n"
                f"首次修改的文件已自动创建 .bak 备份。\n"
                f"请运行 cmake --build 重新编译后生效。",
            )

    # ── 编译与运行 ──

    def _find_build_tools(self) -> dict:
        """Auto-detect build toolchain (cmake, ninja, mingw, qt).
        Priority: QT_DIR env -> C:/Qt -> D:/Qt -> cmake from PATH fallback."""
        import glob as _glob

        def _first_exists(*candidates: str) -> str:
            for c in candidates:
                if os.path.exists(c):
                    return c
            return candidates[0]  # return first so error message is clear

        # Locate Qt root directory
        qt_root = ""
        for base in [os.environ.get("QT_DIR", ""), "C:/Qt", "D:/Qt", "E:/Qt"]:
            if base and os.path.isdir(base):
                qt_root = base
                break

        # Auto-detect Qt version directory (e.g. 6.11.1/mingw_64)
        qt_dir = ""
        if qt_root:
            versions = sorted(
                [d for d in os.listdir(qt_root) if os.path.isdir(os.path.join(qt_root, d)) and d[0].isdigit()],
                reverse=True
            )
            for ver in versions:
                candidate = os.path.join(qt_root, ver, "mingw_64")
                if os.path.isdir(candidate):
                    qt_dir = candidate
                    break

        # Tools directory (cmake / ninja / mingw)
        tools_base = os.path.join(qt_root, "Tools") if qt_root else ""

        cmake_exe  = _first_exists(
            os.path.join(tools_base, "CMake_64", "bin", "cmake.exe"),
            "cmake",  # fallback: hope it is on PATH
        )
        ninja_exe  = os.path.join(tools_base, "Ninja", "ninja.exe") if tools_base else "ninja"
        gxx_exe    = os.path.join(tools_base, "mingw1310_64", "bin", "g++.exe") if tools_base else "g++"

        return {
            "cmake": cmake_exe,
            "ninja": ninja_exe,
            "gxx": gxx_exe,
            "qt_dir": qt_dir,
            "tools_dir": tools_base,
        }

    def _show_build_panel(self):
        self.build_frame.pack(fill=tk.X, side=tk.BOTTOM)
        self.build_output.configure(state=tk.NORMAL)
        self.build_output.delete("1.0", tk.END)
        self.build_output.configure(state=tk.DISABLED)
        self.run_btn.configure(state=tk.DISABLED)
        self.build_status_label.configure(text="", foreground="#888")

    def _hide_build_panel(self):
        self.build_frame.pack_forget()

    def _append_build_log(self, text: str):
        self.build_output.configure(state=tk.NORMAL)
        self.build_output.insert(tk.END, text)
        self.build_output.see(tk.END)
        self.build_output.configure(state=tk.DISABLED)

    def _run_binary(self):
        exe = os.path.join(PROJECT_ROOT, "build", "Biod.exe")
        if not os.path.exists(exe):
            messagebox.showerror("找不到可执行文件", f"未找到:\n{exe}")
            return
        try:
            subprocess.Popen([exe], cwd=os.path.join(PROJECT_ROOT, "build"))
            self._append_build_log("\n>>> Biod.exe 已启动运行\n")
        except Exception as e:
            messagebox.showerror("启动失败", str(e))

    def _build_and_run(self):
        """一键闭环：应用参数 → 编译 → 可运行"""
        # 先应用所有修改
        self._apply_all()

        # 检查源文件是否真的有改动（通过对比 param values 和 defaults）
        has_changes = False
        for p in PARAMS:
            w = self.param_widgets.get(p.param_id)
            if w and w["var"].get() != p.default:
                has_changes = True
                break
        if not has_changes:
            self._append_build_log("所有参数均为默认值，无需重新编译。\n")

        # 定位工具
        tools = self._find_build_tools()
        for key, path in tools.items():
            if not os.path.exists(path):
                self._append_build_log(f"错误: 未找到 {key}: {path}\n")
                return

        # 构建 PATH
        path_extra = (
            os.path.dirname(tools["cmake"]) + os.pathsep +
            os.path.dirname(tools["ninja"]) + os.pathsep +
            os.path.dirname(tools["gxx"]) + os.pathsep +
            os.path.join(tools["qt_dir"], "bin")
        )

        self._show_build_panel()
        self._append_build_log("=" * 50 + "\n")
        self._append_build_log("编译构建中...\n")
        self._append_build_log(f"CMake:  {tools['cmake']}\n")
        self._append_build_log(f"编译器: {tools['gxx']}\n")
        self._append_build_log("=" * 50 + "\n\n")
        self.build_status_label.configure(text="编译中...", foreground="#d4a017")
        self.root.update()

        build_dir = os.path.join(PROJECT_ROOT, "build")
        cmd = (
            f'"{tools["cmake"]}" --build "{build_dir}" --config Release'
        )

        def run_build():
            try:
                env = os.environ.copy()
                env["PATH"] = path_extra + os.pathsep + env.get("PATH", "")
                proc = subprocess.Popen(
                    cmd,
                    shell=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    env=env,
                    cwd=PROJECT_ROOT,
                )
                for line in proc.stdout:
                    self.root.after(0, self._append_build_log, line)

                proc.wait()
                exit_code = proc.returncode

                if exit_code == 0:
                    self.root.after(0, lambda: self._build_status_label.configure(
                        text="编译成功 — 可以运行", foreground="#2a9e2a"))
                    self.root.after(0, lambda: self.run_btn.configure(state=tk.NORMAL))
                    self.root.after(0, self._append_build_log,
                                    "\n" + "=" * 50 + "\n编译成功！点击「▷ 运行 Biod.exe」启动程序。\n")
                    self.root.after(0, lambda: self.status_var.set(
                        "编译成功 — 参数已应用，可运行"))
                    # 部署 Qt DLL
                    windeploy = os.path.join(tools["qt_dir"], "bin", "windeployqt.exe")
                    if os.path.exists(windeploy):
                        deploy_cmd = f'"{windeploy}" "{build_dir}\\Biod.exe" --no-translations'
                        dep_proc = subprocess.run(deploy_cmd, shell=True,
                                                  capture_output=True, text=True,
                                                  env=env, cwd=PROJECT_ROOT)
                        self.root.after(0, self._append_build_log, "\n--- windeployqt ---\n")
                        if dep_proc.stdout:
                            self.root.after(0, self._append_build_log, dep_proc.stdout[-500:])
                else:
                    self.root.after(0, lambda: self._build_status_label.configure(
                        text=f"编译失败 (exit {exit_code})", foreground="#d42a2a"))
                    self.root.after(0, lambda: self.run_btn.configure(state=tk.DISABLED))
                    self.root.after(0, self._append_build_log,
                                    f"\n编译失败，退出码: {exit_code}\n")
                    self.root.after(0, lambda: self.status_var.set(
                        f"编译失败 — 请检查输出日志"))

            except Exception as e:
                self.root.after(0, self._append_build_log, f"\n构建异常: {e}\n")
                self.root.after(0, lambda: self._build_status_label.configure(
                    text="构建异常", foreground="#d42a2a"))

        threading.Thread(target=run_build, daemon=True).start()

    def _on_close(self):
        self.root.destroy()


# ============================================================
# 入口
# ============================================================

def main():
    root = tk.Tk()
    # 设置主题样式
    style = ttk.Style(root)
    try:
        style.theme_use("clam")
    except tk.TclError:
        pass
    # 应用按钮强调色
    style.configure("Accent.TButton",
                    font=("Microsoft YaHei", 11, "bold"),
                    padding=8)
    app = ParamTunerApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
