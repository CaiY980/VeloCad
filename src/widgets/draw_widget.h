
#ifndef DRAW_WIDGET_H
#define DRAW_WIDGET_H

//#include "make_edge.h"
//#include "make_face.h"
#include "make_shapes.h"

#include "shape.h"
#include <AIS_InteractiveContext.hxx>
#include <gp_Pnt.hxx>          
#include <gp_Trsf.hxx>         
#include <AIS_Shape.hxx>
#include <AIS_Trihedron.hxx>
#include <AIS_ViewCube.hxx>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QWidget>
#include <V3d_Light.hxx>

#include <QDoubleSpinBox>
#include <QPushButton>   
#include <TopoDS_Edge.hxx>   

#include <gp_Pnt2d.hxx>
class DrawWidget : public QWidget {
    Q_OBJECT

    Handle(AIS_InteractiveContext) m_context;//!交互式上下文能够管理一个或多个查看器(viewer)中的图形行为和交互式对象的选择
    Handle(V3d_Viewer) m_viewer;             //!定义查看器(viewer)类型对象上的服务
    Handle(V3d_View) m_view;                 //!创建一个视图
    Handle(AIS_ViewCube) view_cube;          //!视方体
    Handle(AIS_Trihedron) origin_coord;      //!基坐标系
    Handle(V3d_Light) light_direction;       //! 定向光源
    Standard_Integer m_x_max{};              //!记录鼠标平移坐标X
    Standard_Integer m_y_max{};              //!记录鼠标平移坐标Y

public:

    // 构造函数主要对 QWidget 的属性进行配置，为 OpenCASCADE (OCC) 的 OpenGL 渲染做好准备
    explicit DrawWidget(QWidget *parent = nullptr);

    void remove_all();

    //!初始化交互环境 搭建了 Qt 和 OCC 之间的“桥梁”
    void initialize_context();

private:
    // 创建并显示一个位于 3D 视图角落的“视图立方体”
    void create_view_cube();
    
    // 创建并显示一个位于世界坐标系原点 (0,0,0) 的三色坐标轴。
    void create_origin_coord();

signals:
    void selectedShapeInfo(const QJsonDocument &info);
    void shapeMoved(const gp_Trsf &newTransform);
    void selectObject(int);
public slots:
    void onMakeCompound();
    void onRunPreciseHLR(); 
    void onRunDiscreteHLR();
    void onVisualizeInternalPoints();
    // 从外部（如 Lua 虚拟机）接收 Shape 并将其显示在场景中的入口
    void onDisplayShape(const Shape &theIObj);

    void create_shape_1d(int id);
    void create_shape_2d(int id);
    void create_shape_3d(int id);
    
    // 为布尔运算添加三个新的槽函数
    void onFuseShapes();
    void onCutShapes();
    void onCommonShapes();
    // 为圆角/倒角按钮添加槽函数
    void onApplyFillet();
    void onApplyChamfer();

    void readStl();
    void exportStl();
    void readXde();
    void exportXde();

protected:
    //!覆写绘图事件
    void paintEvent(QPaintEvent *event) override;

    //!覆写窗口尺寸变化事件
    void resizeEvent(QResizeEvent *event) override;

    //! 返回窗口的绘制引擎
    [[nodiscard]] QPaintEngine *paintEngine() const override { return nullptr; }

    //!覆写鼠标按键按下事件
    void mousePressEvent(QMouseEvent *event) override;

    //!覆写鼠标按键释放事件
    void mouseReleaseEvent(QMouseEvent *event) override;

    //!覆写鼠标移动事件
    void mouseMoveEvent(QMouseEvent *event) override;

    //!覆写鼠标滚轮事件
    void wheelEvent(QWheelEvent *event) override;

    // !覆写键盘事件
    void keyPressEvent(QKeyEvent* event) override;


private:

    Handle(AIS_Shape) m_infoLabel;

    int m_drawLineStep = 0;   // 画线步骤: 0=无, 1=等待起点, 2=等待终点
    gp_Pnt m_lineStartPoint;  // 存储直线的起始点

    int m_drawMode = 0; // 当前绘制模式: 0=无, 1=Bezier, 2=BSpline
    std::vector<gp_Pnt> m_clickedPoints; // 存储用户点击的控制点
    std::vector<Handle(AIS_InteractiveObject)> m_tempAISObjects; // 存储临时显示的控制点对象（绘制完成后清除）

    // 辅助函数：清理临时绘制对象
    void clearTempObjects();
    void finishDrawing();
    MakeShapes m_make_shapes;

    Shape m_shape;
  

    QWidget *m_filletWidget;
    QDoubleSpinBox *m_filletSpinBox;
    QDoubleSpinBox *m_chamferSpinBox;
    QPushButton *m_filletButton;
    QPushButton *m_chamferButton;
    
    Handle(AIS_Shape) m_selectedAISShape;
    TopoDS_Edge m_selectedEdge;

    Handle(AIS_InteractiveObject) m_draggedObject;// 正在被拖拽的 AIS 对象
    gp_Pnt m_dragStartPos3d; //拖拽开始时，鼠标在 3D 空间中的点
    gp_Trsf m_dragStartTrsf;// 拖拽开始时，对象的原始变换矩阵

    bool m_isDraggingObject{false};
    // 正在拖拽物体
    bool m_isPanningCamera{false};
    // 正在平移摄像机
    bool m_isRotatingCamera{false};

    // 正在旋转摄像机
    bool m_isRotatingObject{false};// 正在(右键)旋转物体
    bool m_isMenu{false}; // 呼出右键菜单栏
    Handle(AIS_InteractiveObject) m_rotatedObject;
    gp_Pnt2d m_dragStartPos2d;                  

    // 辅助函数
    void showFilletUI(bool show);
    // 创建一个 QMenu 和一个 QActionGroup
    void createContextMenu();
    // Qt 菜单组件
    QMenu *m_contextMenu;
    QActionGroup *m_selectionModeGroup;
   
    QAction *m_fuseAction;
    QAction *m_cutAction;
    QAction *m_commonAction;
    QAction *m_visualizePointsAction;
    QAction *m_hlrPreciseAction; 
    QAction *m_hlrDiscreteAction;
    // 右键菜单控制
    bool m_isRotating{false};


    // 激活用户指定的新选择模式。
    void setSelectionMode(TopAbs_ShapeEnum mode);
    // 当用户在3D视图中成功选中一个对象后被调用的
    void handleSelectedShape(const TopoDS_Shape &shape);
private slots:
    // 槽函数实现 当用户点击菜单项时被触发 从 QAction 中取回 data（即 TopAbs_ShapeEnum 值）并设置选择模式。
    void selectActionTriggered(QAction *act) {
        setSelectionMode(static_cast<TopAbs_ShapeEnum>(act->data().toInt()));
    }
};

struct t_link {
    int n[2];// 边的两个节点索引

    t_link() { n[0] = n[1] = 0; }
    t_link(const int _n0, const int _n1) {
        n[0] = _n0;
        n[1] = _n1;
    }
    t_link(const std::initializer_list<int> &init) {
        n[0] = *init.begin();
        n[1] = *(init.end() - 1);
    }

    // Hasher (函数对象)
    struct Hasher {
        // 1-参数: 计算哈希值
        int operator()(const t_link &link) const {
            int key = link.n[0] + link.n[1];// 保证 (a,b) 和 (b,a) 初始值相同
            key += (key << 10);
            key ^= (key >> 6);
            key += (key << 3);
            key ^= (key >> 11);
            return (key & 0x7fffffff);// 确保为正数
        }

        // 2-参数: 检查是否相等
        bool operator()(const t_link &link0, const t_link &link1) const {
            // 检查 (a,b) == (a,b) 或 (a,b) == (b,a)
            return ((link0.n[0] == link1.n[0]) && (link0.n[1] == link1.n[1])) ||
                   ((link0.n[1] == link1.n[0]) && (link0.n[0] == link1.n[1]));
        }
    };
};

struct t_hlrEdges {
    bool OutputVisibleSharpEdges;
    bool OutputVisibleSmoothEdges;
    bool OutputVisibleOutlineEdges;
    bool OutputVisibleSewnEdges;
    bool OutputVisibleIsoLines;
    bool OutputHiddenSharpEdges;
    bool OutputHiddenSmoothEdges;
    bool OutputHiddenOutlineEdges;
    bool OutputHiddenSewnEdges;
    bool OutputHiddenIsoLines;

    t_hlrEdges()
        : OutputVisibleSharpEdges(true),
          OutputVisibleSmoothEdges(true),
          OutputVisibleOutlineEdges(true),
          OutputVisibleSewnEdges(true),
          OutputVisibleIsoLines(true),
          OutputHiddenSharpEdges(false),
          OutputHiddenSmoothEdges(false),
          OutputHiddenOutlineEdges(false),
          OutputHiddenSewnEdges(false),
          OutputHiddenIsoLines(false) {}
};

#endif//3D_WIDGET_H
