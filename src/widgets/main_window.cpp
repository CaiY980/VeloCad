#include "draw_widget.h"
#include "main_window.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {

    setWindowTitle("CAD 工具主窗口");
    setMinimumSize(1024, 768);

    m_drawWidget = new DrawWidget(this);
    setCentralWidget(m_drawWidget);

    createShapeMenus();


    createMenuBar(); // 顶部菜单
    createToolBars();// 工具栏 (将形状菜单加入工具栏)
    connectActions();// 绑定逻辑

    statusBar()->show();
    statusBar()->showMessage("就绪");
}

MainWindow::~MainWindow() {
}


void MainWindow::createShapeMenus() {
    // --- 1D Shapes ---
    menuShape1D = new QMenu("1D Shapes", this);
    grpShape1D = new QActionGroup(this);// 使用 ActionGroup 方便统一处理信号

    // 辅助 Lambda：创建 Action 并设置 ID
    auto add1DAction = [&](const QString &text, int id, const QString &tooltip) {
        QAction *act = new QAction(text, this);
        act->setToolTip(tooltip);
        act->setData(id);// 存储原有 ID
        menuShape1D->addAction(act);
        grpShape1D->addAction(act);
    };

    // 根据 shape_1d.cpp 中的定义添加
    add1DAction("Line", 0, "Line");
    add1DAction("Bezier", 1, "Bezier");
    add1DAction("Spline", 2, "Spline");
    add1DAction("Circle", 3, "Circle");
    add1DAction("Ellipse", 4, "Ellipse");
    add1DAction("Hyperbola", 5, "Hyperbola");
    add1DAction("Parabola", 6, "Parabola");

    // --- 2D Shapes ---
    menuShape2D = new QMenu("2D Shapes", this);
    grpShape2D = new QActionGroup(this);

    auto add2DAction = [&](const QString &text, int id, const QString &tooltip) {
        QAction *act = new QAction(text, this);
        act->setToolTip(tooltip);
        act->setData(id);
        menuShape2D->addAction(act);
        grpShape2D->addAction(act);
    };

    // 根据 shape_2d.cpp 中的定义添加
    add2DAction("Plane", 0, "Plane");
    add2DAction("Cylindrical", 1, "Cylindrical");
    add2DAction("Conical", 2, "Conical");

    // --- 3D Shapes ---
    menuShape3D = new QMenu("3D Shapes", this);
    grpShape3D = new QActionGroup(this);

    auto add3DAction = [&](const QString &text, int id, const QString &tooltip) {
        QAction *act = new QAction(text, this);
        act->setToolTip(tooltip);
        act->setData(id);
        menuShape3D->addAction(act);
        grpShape3D->addAction(act);
    };

    // 根据 shape_3d.cpp 中的定义添加
    add3DAction("Box", 0, "Box (ShapeBox)");
    add3DAction("Cylinder", 1, "Cylinder (Cylinder)");
    add3DAction("Cone", 2, "Cone (Cone)");
    add3DAction("Sphere", 3, "Sphere (Sphere)");
    add3DAction("Torus", 4, "Torus (Torus)");
    add3DAction("Wedge", 5, "Wedge (Wedge)");
    add3DAction("Vertex", 6, "Vertex (Vertex)");
    add3DAction("Wire", 7, "Wire (Wire)");
    add3DAction("Polygon", 8, "Polygon (Polygon)");
    add3DAction("Plane", 9, "Plane (Plane)");
    add3DAction("Cylindrical", 10, "Cylindrical (Cylindrical)");
    add3DAction("Conical", 11, "Conical (Conical)");
}

void MainWindow::createMenuBar() {
    QMenuBar *menuBar = this->menuBar();

    // ---- 文件菜单 ----
    QMenu *fileMenu = menuBar->addMenu("文件(&F)");

    actionOpenStl = new QAction("📂 打开 STL", this);
    actionExportStl = new QAction("⬇️ 导出 STL", this);
    actionOpenXde = new QAction("📄 打开 XDE", this);
    actionExportXde = new QAction("📤 导出 XDE", this);
    actionExit = new QAction("退出(&X)", this);
    actionExit->setShortcut(QKeySequence::Quit);

    fileMenu->addAction(actionOpenStl);
    fileMenu->addAction(actionOpenXde);
    fileMenu->addSeparator();
    fileMenu->addAction(actionExportStl);
    fileMenu->addAction(actionExportXde);
    fileMenu->addSeparator();
    fileMenu->addAction(actionExit);

    // ---- 插入形状菜单到菜单栏 (可选，如果只想在工具栏显示可注释掉) ----
    menuBar->addMenu(menuShape1D);
    menuBar->addMenu(menuShape2D);
    menuBar->addMenu(menuShape3D);

    // ---- 工具菜单 ----
    QMenu *toolsMenu = menuBar->addMenu("工具(&T)");

    actionFuseShapes = new QAction("🔗 合并 (Union)", this);
    actionCutShapes = new QAction("✂️ 裁剪 (Difference)", this);
    actionCommonShapes = new QAction("🔍 交集 (Common)", this);
    actionVisualizePoints = new QAction("📍 可视化点", this);
    actionHlrPrecise = new QAction("🛠️ 精确 HLR", this);
    actionHlrDiscrete = new QAction("⚡ 快速 HLR", this);
    actionFillet = new QAction("🔘 倒圆角 (Fillet)", this);
    actionChamfer = new QAction("📐 倒角 (Chamfer)", this);

    toolsMenu->addAction(actionFuseShapes);
    toolsMenu->addAction(actionCutShapes);
    toolsMenu->addAction(actionCommonShapes);
    toolsMenu->addSeparator();
    toolsMenu->addAction(actionVisualizePoints);
    toolsMenu->addSeparator();
    toolsMenu->addAction(actionHlrPrecise);
    toolsMenu->addAction(actionHlrDiscrete);
    toolsMenu->addSeparator();
    toolsMenu->addAction(actionFillet);
    toolsMenu->addAction(actionChamfer);


}

void MainWindow::createToolBars() {
    // 1. 主文件工具栏
    QToolBar *mainToolBar = new QToolBar("标准", this);
    mainToolBar->setIconSize(QSize(24, 24));
    mainToolBar->addAction(actionOpenStl);
    mainToolBar->addAction(actionExportStl);
    mainToolBar->addSeparator();
    mainToolBar->addAction(actionOpenXde);
    mainToolBar->addAction(actionExportXde);
    addToolBar(Qt::TopToolBarArea, mainToolBar);


    QToolBar *shapeToolBar = new QToolBar("形状绘制", this);
    shapeToolBar->setIconSize(QSize(24, 24));
    shapeToolBar->setMovable(false);

   
    // 1D 按钮
    QToolButton *btn1D = new QToolButton(this);
    btn1D->setText("⛔ 1D 形状");// 设置显示的文本/图标
    btn1D->setToolTip("1D Shapes Menu");
    btn1D->setMenu(menuShape1D);                   // 设置之前创建的菜单
    btn1D->setPopupMode(QToolButton::InstantPopup);// 设置为点击即弹出
    shapeToolBar->addWidget(btn1D);                // 添加到工具栏
    // 2D 按钮
    QToolButton *btn2D = new QToolButton(this);
    btn2D->setText("♎ 2D 形状");
    btn2D->setToolTip("2D Shapes Menu");
    btn2D->setMenu(menuShape2D);
    btn2D->setPopupMode(QToolButton::InstantPopup);
    shapeToolBar->addWidget(btn2D);
    // 3D 按钮
    QToolButton *btn3D = new QToolButton(this);
    btn3D->setText("⚽ 3D 形状");
    btn3D->setToolTip("3D Shapes Menu");
    btn3D->setMenu(menuShape3D);
    btn3D->setPopupMode(QToolButton::InstantPopup);
    shapeToolBar->addWidget(btn3D);
    addToolBar(Qt::TopToolBarArea, shapeToolBar);

    // 3. 建模操作工具栏
    QToolBar *opsToolBar = new QToolBar("建模操作", this);
    opsToolBar->setIconSize(QSize(24, 24));
    opsToolBar->addAction(actionFuseShapes);
    opsToolBar->addAction(actionCutShapes);
    opsToolBar->addAction(actionCommonShapes);
    opsToolBar->addSeparator();
    opsToolBar->addAction(actionVisualizePoints);
    opsToolBar->addAction(actionHlrPrecise);
    opsToolBar->addAction(actionHlrDiscrete);
    opsToolBar->addSeparator();
    opsToolBar->addAction(actionFillet);
    opsToolBar->addAction(actionChamfer);
    addToolBar(Qt::TopToolBarArea, opsToolBar);
}

void MainWindow::connectActions() {


    connect(grpShape1D, &QActionGroup::triggered, this, &MainWindow::slot_shape1d_triggered);
    connect(grpShape2D, &QActionGroup::triggered, this, &MainWindow::slot_shape2d_triggered);
    connect(grpShape3D, &QActionGroup::triggered, this, &MainWindow::slot_shape3d_triggered);

    // 文件逻辑
    connect(actionOpenStl, &QAction::triggered, m_drawWidget, &DrawWidget::readStl);
    connect(actionExportStl, &QAction::triggered, m_drawWidget, &DrawWidget::exportStl);
    connect(actionOpenXde, &QAction::triggered, m_drawWidget, &DrawWidget::readXde);
    connect(actionExportXde, &QAction::triggered, m_drawWidget, &DrawWidget::exportXde);
    connect(actionExit, &QAction::triggered, qApp, &QApplication::quit);

    // 工具逻辑
    connect(actionFuseShapes, &QAction::triggered, m_drawWidget, &DrawWidget::onFuseShapes);
    connect(actionCutShapes, &QAction::triggered, m_drawWidget, &DrawWidget::onCutShapes);
    connect(actionCommonShapes, &QAction::triggered, m_drawWidget, &DrawWidget::onCommonShapes);
    connect(actionVisualizePoints, &QAction::triggered, m_drawWidget, &DrawWidget::onVisualizeInternalPoints);
    connect(actionHlrPrecise, &QAction::triggered, m_drawWidget, &DrawWidget::onRunPreciseHLR);
    connect(actionHlrDiscrete, &QAction::triggered, m_drawWidget, &DrawWidget::onRunDiscreteHLR);
    connect(actionFillet, &QAction::triggered, m_drawWidget, &DrawWidget::onApplyFillet);
    connect(actionChamfer, &QAction::triggered, m_drawWidget, &DrawWidget::onApplyChamfer);
}

void MainWindow::slot_shape1d_triggered(QAction *action) {
    if (!action) return;

    int index = action->data().toInt();

    // 将索引传给 DrawWidget 处理
    if (m_drawWidget) {
        m_drawWidget->create_shape_1d(index);
    }
}


void MainWindow::slot_shape2d_triggered(QAction *action) {
    if (!action) return;

     int index = action->data().toInt();

    // 将索引传给 DrawWidget 处理
    if (m_drawWidget) {
        m_drawWidget->create_shape_2d(index);
    }
}


void MainWindow::slot_shape3d_triggered(QAction *action) {
    if (!action) return;

     int index = action->data().toInt();

    // 将索引传给 DrawWidget 处理
    if (m_drawWidget) {
        m_drawWidget->create_shape_3d(index);
    }
}