# VeloCAD - Qt & OpenCASCADE CAD Assistant

**VeloCAD** 是一个基于 **Qt (C++)** 和 **OpenCASCADE Technology (OCCT)** 开发的轻量级 CAD 建模与可视化工具。

本项目旨在展示如何构建一个现代化的 CAD 应用程序框架，集成了 3D 交互视图、参数化几何创建、布尔运算、特征建模（圆角/倒角）以及高级数据交换（XDE）功能。

## ✨ 主要功能

### 1. 几何形状创建 (Geometry Creation)

- - 通过工具栏快速创建参数化的 B-Rep 几何体：
    - **⛔ 1D 曲线 (Curves):**
      - Line (直线), Bezier (贝塞尔曲线), Spline (B样条), Circle (圆), Ellipse (椭圆), Hyperbola (双曲线), Parabola (抛物线)。
    - **♎ 2D 曲面 (Surfaces):**
      - Plane (平面), Cylindrical Face (圆柱面), Conical Face (圆锥面)。
    - **⚽ 3D 实体 (Solids/Topologies):**
      - Box (长方体), Cylinder (圆柱体), Cone (圆锥体), Sphere (球体), Torus (圆环体), Wedge (楔体)。

### 2. 建模操作 (Modeling Operations)

支持基于 B-Rep (边界表示法) 的高级建模操作：

- **🔗 合并 (Fuse/Union):** 将两个形状合并为一个。
- **✂️ 裁剪 (Cut/Difference):** 从一个形状中减去另一个形状。
- **🔍 交集 (Common/Intersection):** 保留两个形状重叠的部分。

### 3. 可视化与分析** (Visualization & Analysis)

- **📍 可视化点:** 显示形状内部的几何点信息。
- **🛠️ 精确 HLR:** 运行精确的消隐线算法 (基于 B-Rep)。
- **⚡ 快速 HLR:** 运行离散的消隐线算法 (基于 Mesh)。

### **4.特征造型(Feature Modeling)**

- **🔘 倒圆角 (Fillet):** 交互式选择边并应用恒定半径圆角。
- **📐 倒角 (Chamfer):** 交互式选择边并应用倒角。

### 5. 文件 I/O (File Exchange)

- **STEL:** 支持网格模型的导入与导出。
- **XDE (XBF):** 支持带有装配结构和颜色信息的XBF格式 CAD 数据交换。

## 🏗️ 项目结构

```
VeloCAD/
├── main.cpp                 # 应用程序入口，样式加载
├── main_window.h/cpp        # 主窗口控制器
│   ├── 菜单栏 (QMenuBar) 与工具栏 (QToolBar) 管理
│   └── 信号槽连接与形状创建指令分发
├── draw_widget.h/cpp        # 核心 3D 绘图部件 (继承自 QWidget)
│   ├── 集成 AIS_InteractiveContext 与 V3d_Viewer
│   ├── 处理鼠标事件 (平移/旋转/拖拽/选择)
│   └── 实现布尔运算、HLR、圆角等核心算法
├── make_shapes.h            # 几何工厂类
│   └── 封装 BRepPrimAPI 和 BRepBuilderAPI 用于生成 TopoDS_Shape
├── shape.h/cpp              # 形状数据包装类
│   ├── 封装 TopoDS_Shape
│   ├── 管理颜色与透明度
│   └── 实现文件 I/O (STL/STEP/XDE)
├── DisplayScene.h/cpp       # XDE 文档渲染器
│   └── 负责解析 OCAF 文档结构并将其转换为 AIS 对象显示
└── resources/               # 资源文件 (图标、QSS 样式表)
```

## 🛠️ 构建与依赖

### 前置要求

- **C++ 编译器**: 支持 C++17 标准 (MSVC 2022).
- **Qt**: 版本 6.x.
- **OpenCASCADE (OCCT)**: 版本 7.6.0 或更高.
- **CMake**: 版本 3.16+.

## 🕹️ 操作指南 (Controls)

### 鼠标操作

- **左键单击:** 选择对象（按住 `Ctrl` 多选）。
- **左键拖拽 (背景):** 框选。
- **左键拖拽 (物体):** 在平面上移动选中的物体。
- **中键滚动:** 缩放视图。
- **中键按住:** 平移视图 (Pan)。
- **右键按住 (背景):** 旋转视图 (Rotate Camera)。
- **右键按住 (物体):** 旋转选中的物体 (Rotate Object)。
- **右键单击:** 弹出上下文菜单（执行布尔运算、切换选择模式等）。

### 快捷功能

**倒圆角/倒角:**

- 在右键菜单中切换选择模式为 **"Select Edges"**。
- 选中一条边，界面右上角会自动弹出参数设置面板。
- 输入半径/距离，点击应用。

<img width="1708" height="999" alt="image" src="https://github.com/user-attachments/assets/4e0e4006-b250-4951-89ab-51cf8d399fd2" style="zoom:25%; />



**布尔运算:**

- 选中两个实体（按住 Ctrl）。
-  **🔗 (合并)** 或 **✂️ (裁剪)** 或🔍 **(交集)** 进行布尔运算。

<img width="1708" height="999" alt="image" src="https://github.com/user-attachments/assets/d89cb923-8ad0-4522-8014-65e5d4fa53cb" style="zoom:25%; />


**其他操作:**

- 选中一个实体。
- 可点击 **📍 (可视化点**) 或 **🛠️ (精确 HLR)** 或⚡ **(快速 HLR)** 进行操作。

<img width="1708" height="999" alt="image" src="https://github.com/user-attachments/assets/3b697088-3a52-45cb-b044-d6b9f2e503d4" style="zoom:25%; />

