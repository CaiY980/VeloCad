#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDebug>
#include <QDockWidget>
#include <QFileDialog>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>


#include "shape.h"

class DrawWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    // --- 初始化 UI 的私有辅助函数 ---
    void createMenuBar();   // 创建顶部菜单栏
    void createToolBars();  // 创建工具栏
    void createShapeMenus();// 创建形状菜单动作
    void connectActions();  // 连接信号与槽

    // --- 核心 UI 组件 ---
    DrawWidget *m_drawWidget;// 中心绘图部件

    // --- 动作 (Actions) ---

    // 1. 文件操作
    QAction *actionOpenStl;
    QAction *actionExportStl;
    QAction *actionOpenXde;
    QAction *actionExportXde;
    QAction *actionExit;

    // 2. 形状菜单动作 (对应 Shape1D, Shape2D, Shape3D 中的按钮)

    // 1D Shapes
    QMenu *menuShape1D;
    QActionGroup *grpShape1D;// 用于统一管理信号

    // 2D Shapes
    QMenu *menuShape2D;
    QActionGroup *grpShape2D;

    // 3D Shapes
    QMenu *menuShape3D;
    QActionGroup *grpShape3D;

    // 3. 工具操作 (布尔运算等)
    QAction *actionFuseShapes;
    QAction *actionCutShapes;
    QAction *actionCommonShapes;
    QAction *actionVisualizePoints;
    QAction *actionHlrPrecise;
    QAction *actionHlrDiscrete;
    QAction *actionFillet;
    QAction *actionChamfer;
private slots:
 

    void slot_shape1d_triggered(QAction *action);
    void slot_shape2d_triggered(QAction *action);
    void slot_shape3d_triggered(QAction *action);

};

#endif// MAINWINDOW_H