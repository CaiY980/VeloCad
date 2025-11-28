#include "draw_widget.h"
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <AIS_PointCloud.hxx>
#include <Graphic3d_ArrayOfPoints.hxx>
#include <omp.h> 
#include <TopoDS_Shape.hxx>   
#include <BRepGProp.hxx>       
#include <GProp_GProps.hxx>    
#include <AIS_InteractiveObject.hxx>
#include <AIS_ListOfInteractive.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Axis2Placement.hxx>
#include <Geom_Line.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <QApplication>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <StdSelect_BRepOwner.hxx>
#include <TopExp.hxx>
#include <V3d_View.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Trsf.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <DisplayScene.h>
#include <ElCLib.hxx>
#include <ElSLib.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <HLRBRep_PolyAlgo.hxx>
#include <HLRBRep_PolyHLRToShape.hxx>
#include <NCollection_DataMap.hxx>
#include <Poly_CoherentTriangulation.hxx>
#include <QGridLayout>
#include <QLabel>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax1.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <unordered_map>

#include <WNT_Window.hxx>
#include <future>
#include <vector>
#include <thread>
#include <algorithm>


// 构造函数：初始化窗口设置、UI控件和右键菜单
DrawWidget::DrawWidget(QWidget *parent) : QWidget(parent) {
    setBackgroundRole(QPalette::NoRole);
    setMouseTracking(true);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    createContextMenu();

    m_filletWidget = new QWidget(this);
    m_filletWidget->setObjectName("filletWidget");

    m_filletWidget->setStyleSheet(
            "QWidget#filletWidget { background-color: rgba(45, 45, 45, 0.8); border-radius: 5px; }"
            "QLabel, QPushButton, QDoubleSpinBox { color: white; background-color: rgba(70, 70, 70, 0.9); }");

    auto *layout = new QGridLayout(m_filletWidget);

    auto *filletLabel = new QLabel(tr("Fillet Radius:"));
    m_filletSpinBox = new QDoubleSpinBox();
    m_filletSpinBox->setDecimals(2);
    m_filletSpinBox->setSingleStep(0.1);
    m_filletSpinBox->setValue(1.0);
    m_filletButton = new QPushButton(tr("Apply Fillet"));

    auto *chamferLabel = new QLabel(tr("Chamfer Dist:"));
    m_chamferSpinBox = new QDoubleSpinBox();
    m_chamferSpinBox->setDecimals(2);
    m_chamferSpinBox->setSingleStep(0.1);
    m_chamferSpinBox->setValue(1.0);
    m_chamferButton = new QPushButton(tr("Apply Chamfer"));

    layout->addWidget(filletLabel, 0, 0);
    layout->addWidget(m_filletSpinBox, 0, 1);
    layout->addWidget(m_filletButton, 0, 2);
    layout->addWidget(chamferLabel, 1, 0);
    layout->addWidget(m_chamferSpinBox, 1, 1);
    layout->addWidget(m_chamferButton, 1, 2);

    m_filletWidget->setLayout(layout);
    m_filletWidget->hide();

    connect(m_filletButton, &QPushButton::clicked, this, &DrawWidget::onApplyFillet);
    connect(m_chamferButton, &QPushButton::clicked, this, &DrawWidget::onApplyChamfer);

    setFocusPolicy(Qt::StrongFocus);
}

// 根据ID创建1D形状（线、圆、曲线等），计算包围盒并显示
void DrawWidget::create_shape_1d(int id) {

    // 每次点击按钮时，先重置所有绘制状态
    m_drawLineStep = 0;
    m_drawMode = 0;
    m_clickedPoints.clear();
    clearTempObjects();

    qDebug() << "id 槽函数触发" << id;
    TopoDS_Shape shape;
    m_shape.s_ = shape;
    Quantity_Color color;

    const std::map<int, Quantity_Color> default_colors = {
            {0, Quantity_Color(Quantity_NOC_RED)},
            {1, Quantity_Color(Quantity_NOC_GREEN)},
            {2, Quantity_Color(Quantity_NOC_BLUE)},
            {3, Quantity_Color(Quantity_NOC_YELLOW)},
            {4, Quantity_Color(Quantity_NOC_MAGENTA)},
            {5, Quantity_Color(Quantity_NOC_CYAN)},
            {6, Quantity_Color(Quantity_NOC_ORANGE)}};

    try {
        if (id == 0) {
            m_drawLineStep = 1;
            // qDebug() << "进入画线模式：请在视图中点击鼠标左键确定起点。";
            return; // 直接返回，等待鼠标事件
        } else if (id == 1) {
            m_drawMode = 1;
            // qDebug() << "进入贝塞尔曲线绘制模式：左键添加控制点，右键结束绘制。";
            return; // 等待鼠标操作
        } else if (id == 2) {
            m_drawMode = 2;
            // qDebug() << "进入 B 样条曲线绘制模式：左键添加控制点，右键结束绘制。";
            return; // 等待鼠标操作
        } else if (id == 3) {
            std::array<double, 3> center = {0.0, 0.0, 0.0};
            std::array<double, 3> normal = {0.0, 0.0, 1.0};
            double radius = 1.0;
            m_make_shapes.makeCircle(center, normal, radius);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 4) {
            std::array<double, 3> center = {0.0, 0.0, 0.0};
            std::array<double, 3> normal = {0.0, 0.0, 1.0};
            double radius1 = 1.5;
            double radius2 = 0.8;
            m_make_shapes.makeEllipse(center, normal, radius1, radius2);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else {
            qWarning() << "[1D Shape] 无效ID：" << id << "，未创建任何形状";
            return;
        }

        if (shape.IsNull()) {
            throw std::runtime_error("形状创建失败：空形状");
        }
    } catch (const std::exception &e) {
        qCritical() << "[1D Shape] ID=" << id << " 创建失败：" << e.what();
        shape = TopoDS_Shape();
        return;
    }

    Bnd_Box bounding;
    BRepBndLib::Add(shape, bounding);
    if (!bounding.IsOpen()) {
        std::array<Standard_Real, 4> bndXY;
        Standard_Real bndMinZ, bndMaxinZ;
        bounding.Get(bndXY[0], bndXY[1], bndMinZ, bndXY[2], bndXY[3], bndMaxinZ);
        std::transform(bndXY.begin(), bndXY.end(), bndXY.begin(), [](double x) { return std::abs(x); });
        const auto max = *std::max_element(bndXY.begin(), bndXY.end());
        Standard_Real x_size_old, y_size_old, offset_old;
        m_viewer->RectangularGridGraphicValues(x_size_old, y_size_old, offset_old);
        if (max > x_size_old) {
            const double logValue = std::log10(std::abs(max));
            const auto step = std::pow(10.0, std::floor(logValue));
            const auto x = std::ceil(max / step) * step + 1e-3;
            m_viewer->SetRectangularGridValues(0, 0, step, step, 0);
            m_viewer->SetRectangularGridGraphicValues(x, x, 0);
        }
    } else {
        qDebug() << "Warning: bounding is open";
    }

    const auto ais_shape = new AIS_Shape(shape);
    ais_shape->SetColor(color);
    m_shape.s_ = shape;
    m_shape.color_ = color;
    ais_shape->SetMaterial(Graphic3d_NameOfMaterial_Stone);
    m_context->Display(ais_shape, Standard_True);

    m_view->FitAll();
}

// 根据ID创建2D形状（平面、圆柱面等），计算包围盒并显示
void DrawWidget::create_shape_2d(int id) {
    qDebug() << "id 槽函数触发" << id;
    TopoDS_Shape shape_face;
    Quantity_Color color;
    Standard_Real transparency;

    const std::map<int, std::pair<Quantity_Color, Standard_Real>> face_params = {
            {0, {Quantity_Color(Quantity_NOC_LIGHTBLUE), 0.8f}},
            {1, {Quantity_Color(Quantity_NOC_GREEN), 0.7f}},
            {2, {Quantity_Color(Quantity_NOC_LIGHTPINK), 0.7f}}};

    try {
        if (id == 0) {
            std::array<double, 3> plane_pos = {0.0, 0.0, 0.0};
            std::array<double, 3> plane_dir = {0.0, 0.0, 1.0};
            std::array<double, 4> plane_uv = {-5.0, 5.0, -5.0, 5.0};
            m_make_shapes.makePlane(plane_pos, plane_dir, plane_uv);
            shape_face = m_make_shapes.s_;
            color = face_params.at(id).first;
            transparency = face_params.at(id).second;
        } else if (id == 1) {
            std::array<double, 3> cyl_pos = {0.0, 0.0, 0.0};
            std::array<double, 3> cyl_dir = {0.0, 0.0, 1.0};
            double cyl_radius = 1.5;
            std::array<double, 4> cyl_uv = {0.0, 360.0, -2.0, 2.0};
            m_make_shapes.makeCylindricalFace(cyl_pos, cyl_dir, cyl_radius, cyl_uv);
            shape_face = m_make_shapes.s_;
            color = face_params.at(id).first;
            transparency = face_params.at(id).second;
        } else if (id == 2) {
            std::array<double, 3> cone_pos = {0.0, 0.0, 0.0};
            std::array<double, 3> cone_dir = {0.0, 0.0, 1.0};
            double cone_angle = 30.0;
            double cone_radius = 1.2;
            std::array<double, 4> cone_uv = {0.0, 360.0, 0.0, 3.0};
            m_make_shapes.makeConicalFace(cone_pos, cone_dir, cone_angle, cone_radius, cone_uv);
            shape_face = m_make_shapes.s_;
            color = face_params.at(id).first;
            transparency = face_params.at(id).second;
        } else {
            qWarning() << "[2D/3D Face] 无效ID：" << id << "，未创建任何面";
            return;
        }

        if (shape_face.IsNull()) {
            throw std::runtime_error("面形状创建失败：空形状");
        }
    } catch (const std::exception &e) {
        qCritical() << "[2D Shape] ID=" << id << " 创建失败：" << e.what();
        shape_face = TopoDS_Shape();
        return;
    }

    Bnd_Box bounding;
    BRepBndLib::Add(shape_face, bounding);
    if (!bounding.IsOpen()) {
        std::array<Standard_Real, 4> bndXY;
        Standard_Real bndMinZ, bndMaxinZ;
        bounding.Get(bndXY[0], bndXY[1], bndMinZ, bndXY[2], bndXY[3], bndMaxinZ);
        std::transform(bndXY.begin(), bndXY.end(), bndXY.begin(), [](double x) { return std::abs(x); });
        const auto max = *std::max_element(bndXY.begin(), bndXY.end());
        Standard_Real x_size_old, y_size_old, offset_old;
        m_viewer->RectangularGridGraphicValues(x_size_old, y_size_old, offset_old);
        if (max > x_size_old) {
            const double logValue = std::log10(std::abs(max));
            const auto step = std::pow(10.0, std::floor(logValue));
            const auto x = std::ceil(max / step) * step + 1e-3;
            m_viewer->SetRectangularGridValues(0, 0, step, step, 0);
            m_viewer->SetRectangularGridGraphicValues(x, x, 0);
        }
    } else {
        qDebug() << "Warning: bounding is open";
    }

    const auto ais_shape = new AIS_Shape(shape_face);
    ais_shape->SetColor(color);
    m_shape.color_ = color;
    m_shape.s_ = shape_face;
    ais_shape->SetMaterial(Graphic3d_NameOfMaterial_Stone);
    m_context->Display(ais_shape, Standard_True);

    m_view->FitAll();
}

// 根据ID创建3D形状（立方体、圆锥、圆环等），计算包围盒并显示
void DrawWidget::create_shape_3d(int id) {
    qDebug() << "id 槽函数触发" << id;
    TopoDS_Shape shape;
    Quantity_Color color;

    const std::map<int, Quantity_Color> default_colors = {
            {0, Quantity_Color(Quantity_NOC_BLUE)},
            {1, Quantity_Color(Quantity_NOC_GREEN)},
            {2, Quantity_Color(Quantity_NOC_YELLOW)},
            {3, Quantity_Color(Quantity_NOC_RED)},
            {4, Quantity_Color(Quantity_NOC_PURPLE)},
            {5, Quantity_Color(Quantity_NOC_ORANGE)},
            {6, Quantity_Color(Quantity_NOC_BLACK)},
            {7, Quantity_Color(Quantity_NOC_GRAY)},
            {8, Quantity_Color(Quantity_NOC_CYAN)},
            {9, Quantity_Color(Quantity_NOC_MAGENTA)}};

    try {
        if (id == 0) {
            double box_x = 2.0, box_y = 2.0, box_z = 2.0;
            m_make_shapes.makeBox(box_x, box_y, box_z);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 1) {
            double cyl_r = 1.0;
            double cyl_h = 3.0;
            m_make_shapes.makeCylinder(cyl_r, cyl_h);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 2) {
            double cone_R1 = 1.0;
            double cone_R2 = 0.3;
            double cone_H = 3.0;
            m_make_shapes.makeCone(cone_R1, cone_R2, cone_H);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 3) {
            double sphere_r = 1.5;
            m_make_shapes.makeSphere(sphere_r);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 4) {
            double torus_R1 = 2.0;
            double torus_R2 = 0.5;
            double torus_angle = 360.0;
            m_make_shapes.makeTorus(torus_R1, torus_R2, torus_angle);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 5) {
            double wedge_dx = 2.0, wedge_dy = 2.0, wedge_dz = 2.0;
            double wedge_ltx = 1.0;
            m_make_shapes.makeWedge(wedge_dx, wedge_dy, wedge_dz, wedge_ltx);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 6) {
            double vx = 0.0, vy = 0.0, vz = 0.0;
            m_make_shapes.makeVertex(vx, vy, vz);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 7) {
            m_make_shapes.makeWire();
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 8) {
            std::vector<std::array<double, 3>> poly_vertices = {
                    {0.0, 1.5, 0.0},
                    {-1.2, 0.5, 0.0},
                    {-0.8, -1.0, 0.0},
                    {0.8, -1.0, 0.0},
                    {1.2, 0.5, 0.0}};
            m_make_shapes.makePolygon(poly_vertices);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 9) {
            double dx = 2.0, dy = 2.0, dz = 2.0;
            double xmin = -1.0, zmin = -1.0, xmax = 1.0, zmax = 1.0;
            m_make_shapes.makeWedge(dx, dy, dz, xmin, zmin, xmax, zmax);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else {
            qWarning() << "[3D Shape] 无效ID：" << id << "，未创建任何形状";
            return;
        }

        if (shape.IsNull()) {
            throw std::runtime_error("3D形状创建失败：空形状");
        }
    } catch (const std::exception &e) {
        qCritical() << "[3D Shape] ID=" << id << " 创建失败：" << e.what();
        shape = TopoDS_Shape();
        return;
    }

    Bnd_Box bounding;
    BRepBndLib::Add(shape, bounding);
    if (!bounding.IsOpen()) {
        std::array<Standard_Real, 4> bndXY;
        Standard_Real bndMinZ, bndMaxinZ;
        bounding.Get(bndXY[0], bndXY[1], bndMinZ, bndXY[2], bndXY[3], bndMaxinZ);
        std::transform(bndXY.begin(), bndXY.end(), bndXY.begin(), [](double x) { return std::abs(x); });
        const auto max = *std::max_element(bndXY.begin(), bndXY.end());
        Standard_Real x_size_old, y_size_old, offset_old;
        m_viewer->RectangularGridGraphicValues(x_size_old, y_size_old, offset_old);
        if (max > x_size_old) {
            const double logValue = std::log10(std::abs(max));
            const auto step = std::pow(10.0, std::floor(logValue));
            const auto x = std::ceil(max / step) * step + 1e-3;
            m_viewer->SetRectangularGridValues(0, 0, step, step, 0);
            m_viewer->SetRectangularGridGraphicValues(x, x, 0);
        }
    } else {
        qDebug() << "Warning: bounding is open";
    }

    const auto ais_shape = new AIS_Shape(shape);
    ais_shape->SetColor(color);
    m_shape.color_ = color;
    m_shape.s_ = shape;
    ais_shape->SetMaterial(Graphic3d_NameOfMaterial_Stone);
    m_context->Display(ais_shape, Standard_True);

    m_view->FitAll();
}

// 显示指定的Shape对象，并自动调整视图网格和视角
void DrawWidget::onDisplayShape(const Shape &theIObj) {
    m_shape = theIObj;
    if (theIObj.data().IsNull()) { return; }
    const auto shape = theIObj.data();
    Bnd_Box bounding;
    BRepBndLib::Add(shape, bounding);
    if (!bounding.IsOpen()) {
        std::array<Standard_Real, 4> bndXY;
        Standard_Real bndMinZ, bndMaxinZ;
        bounding.Get(bndXY[0], bndXY[1], bndMinZ, bndXY[2], bndXY[3], bndMaxinZ);
        std::transform(bndXY.begin(), bndXY.end(), bndXY.begin(), [](double x) { return std::abs(x); });
        const auto max = *std::max_element(bndXY.begin(), bndXY.end());
        Standard_Real x_size_old, y_size_old, offset_old;
        m_viewer->RectangularGridGraphicValues(x_size_old, y_size_old, offset_old);
        if (max > x_size_old) {
            const double logValue = std::log10(std::abs(max));
            const auto step = std::pow(10.0, std::floor(logValue));
            const auto x = std::ceil(max / step) * step + 1e-3;
            m_viewer->SetRectangularGridValues(0, 0, step, step, 0);
            m_viewer->SetRectangularGridGraphicValues(x, x, 0);
        }
    } else {
        qDebug() << "Warning: bounding is open";
    }
    const auto ais_shape = new AIS_Shape(shape);
    ais_shape->SetColor(theIObj.color_);
    ais_shape->SetTransparency(theIObj.transparency_);
    ais_shape->SetMaterial(Graphic3d_NameOfMaterial_Stone);
    m_context->Display(ais_shape, Standard_True);

    m_view->FitAll();
}

// 清空当前视图中的所有交互对象（除坐标轴和ViewCube外）
void DrawWidget::remove_all() {
    m_viewer->SetRectangularGridValues(0, 0, 1, 1, 0);
    m_viewer->SetRectangularGridGraphicValues(2.01, 2.01, 0);

    AIS_ListOfInteractive objects;
    m_context->DisplayedObjects(objects);
    for (const auto &x: objects) {
        if ((x == view_cube) || (x == origin_coord)) { continue; }
        m_context->Remove(x, Standard_True);
    }

    update();
}

// 初始化交互上下文、3D查看器和光源设置
void DrawWidget::initialize_context() {

    
    Handle(Aspect_DisplayConnection) display_connection = new Aspect_DisplayConnection();
    WId window_handle = (WId)winId();
    
    Handle(WNT_Window) wind = new WNT_Window((Aspect_Handle)window_handle);

    // ------------------------------------------------------------------------
    //  创建 3D 查看器 (Viewer) 和 视图 (View)
    // ------------------------------------------------------------------------
    m_viewer = new V3d_Viewer(new OpenGl_GraphicDriver(display_connection));
    m_view = m_viewer->CreateView();
    m_view->SetWindow(wind);

 
    if (!wind->IsMapped()) {
        wind->Map();
    }
    // ------------------------------------------------------------------------
    //  创建交互上下文 (AIS Context)
    // ------------------------------------------------------------------------
    m_context = new AIS_InteractiveContext(m_viewer);

    // ------------------------------------------------------------------------
    //  配置光源 (Lighting)
    // ------------------------------------------------------------------------
    light_direction = new V3d_Light(Graphic3d_TypeOfLightSource_Directional);
    light_direction->SetDirection(m_view->Camera()->Direction());
    m_viewer->AddLight(new V3d_Light(Graphic3d_TypeOfLightSource_Ambient));
    m_viewer->AddLight(light_direction);
    m_viewer->SetLightOn();

    // ------------------------------------------------------------------------
    // 设置背景与视图参数
    // ------------------------------------------------------------------------
    Quantity_Color background_color;
    Quantity_Color::ColorFromHex("#505050", background_color);
    m_view->SetBackgroundColor(background_color);
    m_view->MustBeResized();
    create_view_cube();
    create_origin_coord();

    // ------------------------------------------------------------------------
    //  配置交互样式 (Highligthing & Selection)
    // ------------------------------------------------------------------------
    m_context->SetDisplayMode(AIS_Shaded, Standard_True);

    // --- 配置鼠标悬停（Detected）的高亮样式 ---
    Handle(Prs3d_Drawer) highlight_style = m_context->HighlightStyle();
    highlight_style->SetMethod(Aspect_TOHM_COLOR);       // 使用纯色覆盖方式
    highlight_style->SetColor(Quantity_NOC_LIGHTYELLOW); // 悬停时变为淡黄色
    highlight_style->SetDisplayMode(1);                  // 强制使用着色模式显示高亮
    highlight_style->SetTransparency(0.2f);              // 设置 20% 透明度，透过高亮能看到物体纹理

    // --- 配置鼠标选中（Selected）的样式 ---
    Handle(Prs3d_Drawer) t_select_style = m_context->SelectionStyle();
    t_select_style->SetMethod(Aspect_TOHM_COLOR);
    t_select_style->SetColor(Quantity_NOC_LIGHTSEAGREEN); // 选中时变为海绿色
    t_select_style->SetDisplayMode(1);
    t_select_style->SetTransparency(0.4f);                // 设置 40% 透明度

    // ------------------------------------------------------------------------
    // 初始化网格 (Grid)
    // ------------------------------------------------------------------------
    m_view->SetZoom(100);
    m_viewer->SetRectangularGridValues(0, 0, 1, 1, 0);
    m_viewer->SetRectangularGridGraphicValues(2.01, 2.01, 0);

    // 激活矩形网格，并以线条模式显示。
    m_viewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
}

// 创建并显示视图立方体（View Cube）
void DrawWidget::create_view_cube() {
    view_cube = new AIS_ViewCube();
    const auto &vc_attributes = view_cube->Attributes();
    vc_attributes->SetDatumAspect(new Prs3d_DatumAspect());
    const Handle(Prs3d_DatumAspect) &datumAspect = view_cube->Attributes()->DatumAspect();
    datumAspect->ShadingAspect(Prs3d_DatumParts_XAxis)->SetColor(Quantity_NOC_RED);
    datumAspect->ShadingAspect(Prs3d_DatumParts_YAxis)->SetColor(Quantity_NOC_GREEN);
    datumAspect->ShadingAspect(Prs3d_DatumParts_ZAxis)->SetColor(Quantity_NOC_BLUE);
    datumAspect->TextAspect(Prs3d_DatumParts_XAxis)->SetColor(Quantity_NOC_RED);
    datumAspect->TextAspect(Prs3d_DatumParts_YAxis)->SetColor(Quantity_NOC_GREEN);
    datumAspect->TextAspect(Prs3d_DatumParts_ZAxis)->SetColor(Quantity_NOC_BLUE);
    Handle(Graphic3d_TransformPers) transform_pers = new Graphic3d_TransformPers(Graphic3d_TMF_TriedronPers, Aspect_TOTP_LEFT_LOWER, Graphic3d_Vec2i(85, 85));
    view_cube->SetTransformPersistence(transform_pers);
    view_cube->SetSize(50);
    view_cube->SetAxesPadding(3);
    view_cube->SetBoxColor(Quantity_Color(Quantity_NOC_MATRAGRAY));
    view_cube->SetFontHeight(12);
    m_context->Display(view_cube, Standard_True);
}

// 创建并显示原点坐标系
void DrawWidget::create_origin_coord() {
    Handle(Geom_Axis2Placement) axis = new Geom_Axis2Placement(gp::XOY());
    origin_coord = new AIS_Trihedron(axis);
    origin_coord->SetDatumDisplayMode(Prs3d_DM_WireFrame);
    origin_coord->SetDrawArrows(false);
    origin_coord->Attributes()->DatumAspect()->LineAspect(Prs3d_DatumParts_XAxis)->SetWidth(2.5);
    origin_coord->Attributes()->DatumAspect()->LineAspect(Prs3d_DatumParts_YAxis)->SetWidth(2.5);
    origin_coord->Attributes()->DatumAspect()->LineAspect(Prs3d_DatumParts_ZAxis)->SetWidth(2.5);
    origin_coord->SetDatumPartColor(Prs3d_DatumParts_XAxis, Quantity_NOC_RED2);
    origin_coord->SetDatumPartColor(Prs3d_DatumParts_YAxis, Quantity_NOC_GREEN2);
    origin_coord->SetDatumPartColor(Prs3d_DatumParts_ZAxis, Quantity_NOC_BLUE2);
    origin_coord->SetLabel(Prs3d_DatumParts_XAxis, "");
    origin_coord->SetLabel(Prs3d_DatumParts_YAxis, "");
    origin_coord->SetLabel(Prs3d_DatumParts_ZAxis, "");
    origin_coord->SetSize(60);
    origin_coord->SetTransformPersistence(new Graphic3d_TransformPers(Graphic3d_TMF_ZoomPers, axis->Ax2().Location()));
    origin_coord->Attributes()->SetZLayer(Graphic3d_ZLayerId_Topmost);
    origin_coord->SetInfiniteState(true);
    m_context->Display(origin_coord, AIS_WireFrame, -1, Standard_True);
}

// 绘图事件：重绘3D视图
void DrawWidget::paintEvent(QPaintEvent *event) {
    m_view->Redraw();
    QWidget::paintEvent(event);
}

// 调整大小事件：确保视图尺寸适应，并更新UI控件位置
void DrawWidget::resizeEvent(QResizeEvent *event) {
    if (!m_context) { initialize_context(); }
    if (!m_view.IsNull()) { m_view->MustBeResized(); }

    if (m_filletWidget->isVisible()) {
        QSize uiSize = m_filletWidget->sizeHint();
        m_filletWidget->move(width() - uiSize.width() - 10, 10);
    }

    QWidget::resizeEvent(event);
}

void DrawWidget::clearTempObjects() {
    if (m_context) {
        m_context->ClearSelected(Standard_False);
        m_context->ClearDetected();
    }
    for (const auto& obj : m_tempAISObjects) {
        m_context->Remove(obj, Standard_False); // False 表示不立即刷新屏幕
    }
    m_tempAISObjects.clear();
    m_view->Redraw(); // 统一刷新
}

void DrawWidget::finishDrawing()
{
    if (m_clickedPoints.size() >= 2) {
        TopoDS_Shape shape;

        if (m_drawMode == 1) { // Bezier
            std::vector<std::array<double, 3>> poles;
            for (const auto& pt : m_clickedPoints) {
                poles.push_back({ pt.X(), pt.Y(), pt.Z() });
            }
            try {
                m_make_shapes.makeBezier(poles);
                shape = m_make_shapes.s_;
            }
            catch (const std::exception& e) {
                qWarning() << "Bezier 创建失败:" << e.what();
            }
        }
        else if (m_drawMode == 2) { // BSpline
            int n = m_clickedPoints.size();
            int degree = 3;
            if (n <= degree) degree = n - 1;
            if (degree < 1) degree = 1;

            std::vector<std::array<double, 3>> poles;
            for (const auto& pt : m_clickedPoints) {
                poles.push_back({ pt.X(), pt.Y(), pt.Z() });
            }

            int num_distinct_knots = n - degree + 1;
            std::vector<double> knots(num_distinct_knots);
            std::vector<int> mults(num_distinct_knots);

            for (int i = 0; i < num_distinct_knots; ++i) {
                if (num_distinct_knots > 1)
                    knots[i] = double(i) / (num_distinct_knots - 1);
                else
                    knots[i] = 0.0;
            }

            for (int i = 0; i < num_distinct_knots; ++i) {
                if (i == 0 || i == num_distinct_knots - 1)
                    mults[i] = degree + 1;
                else
                    mults[i] = 1;
            }

            try {
                m_make_shapes.makeBSpline(poles, knots, mults, degree);
                shape = m_make_shapes.s_;
            }
            catch (const std::exception& e) {
                qWarning() << "BSpline 创建失败:" << e.what();
            }
        }

        if (!shape.IsNull()) {
            Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
            aisShape->SetColor(Quantity_NOC_YELLOW);
            aisShape->SetMaterial(Graphic3d_NameOfMaterial_Stone);
            m_shape.s_ = shape;
            m_context->Display(aisShape, Standard_True);
            qDebug() << "形状绘制完成。";
        }
    }

    m_drawMode = 0;
    m_clickedPoints.clear();
    clearTempObjects();
}


// 鼠标按下事件：处理对象选择、相机平移初始化和拖拽初始化
void DrawWidget::mousePressEvent(QMouseEvent *event) {

    const qreal ratio = devicePixelRatioF();

    if (m_drawMode != 0 || m_drawLineStep > 0) {
        // 将鼠标在 2D 屏幕上的点击点 (x, y) 转换为 3D 空间中的一条射线（Ray）
        //vx, vy, vz: 射线的起点（通常是摄像机位置）。
        //vdx, vdy, vdz: 射线的方向向量。
        Standard_Real vx, vy, vz, vdx, vdy, vdz;
        m_view->ConvertWithProj(
            event->pos().x() * ratio, event->pos().y() * ratio, // 输入：修正后的 2D 屏幕坐标
            vx, vy, vz,   // 输出：射线的起点 (Eye/Camera Position)
            vdx, vdy, vdz // 输出：射线的方向 (Ray Direction)
        );
        gp_Pnt eyePnt(vx, vy, vz);
        gp_Dir rayDir(vdx, vdy, vdz);

        // 射线与平面求交
        Standard_Real x, y, z;
        // 检查视线是否平行于地面 (避免除以零)  Precision::Confusion() 是一个 接近于0 的 极小值
        if (Abs(rayDir.Z()) > Precision::Confusion()) {
            Standard_Real t = -eyePnt.Z() / rayDir.Z();
            // 计算交点  eyePnt点 向 gp_Vec(rayDir) 方向 移动了 t 个单位 
            gp_Pnt intersectPnt = eyePnt.Translated(gp_Vec(rayDir).Multiplied(t));

            x = intersectPnt.X();
            y = intersectPnt.Y();
            z = 0.0; // 强制锁定在 Z=0 平面，与坐标轴底面一致
        }
        else {
            x = vx; y = vy; z = vz;
        }



        gp_Pnt clickPnt(x, y, z);

        qDebug() << x <<"  " << y << "  " << z;
       
        if (m_drawLineStep > 0) {
            if (event->button() == Qt::LeftButton) {
                if (m_drawLineStep == 1) {
                    m_lineStartPoint = clickPnt;
                    m_drawLineStep = 2;
                    qDebug() << "起点已设定，请点击终点。";
                }
                else if (m_drawLineStep == 2) {
                    std::array<double, 3> p1 = { m_lineStartPoint.X(), m_lineStartPoint.Y(), m_lineStartPoint.Z() };
                    std::array<double, 3> p2 = { x, y, z };
                    m_make_shapes.makeLine(p1, p2);

                    if (!m_make_shapes.s_.IsNull()) {
                        Handle(AIS_Shape) aisShape = new AIS_Shape(m_make_shapes.s_);
                        aisShape->SetColor(Quantity_NOC_RED);
                        m_context->Display(aisShape, Standard_True);
                        
                    }
                    m_drawLineStep = 0; // 结束直线绘制
                }
            }

            return; 
        }
        if (m_drawMode != 0) {
            if (event->button() == Qt::LeftButton) {
                // 1. 记录点
                m_clickedPoints.push_back(clickPnt);

                // 2. 显示临时红点
                TopoDS_Vertex v = BRepBuilderAPI_MakeVertex(clickPnt);
                Handle(AIS_Shape) aisPoint = new AIS_Shape(v);
                aisPoint->SetColor(Quantity_NOC_RED);
                m_context->Display(aisPoint, Standard_True);
                m_tempAISObjects.push_back(aisPoint);

            }
            return;
        }
    }


    m_isDraggingObject = false;
    m_isPanningCamera = false;
    m_isRotatingCamera = false;
    m_isRotatingObject = false;
    m_isMenu = false;
  
    if ( event->button() & Qt::LeftButton) {
        m_x_max = event->x();
        m_y_max = event->y();
       
        m_context->MoveTo(event->pos().x() * ratio, event->pos().y() * ratio, m_view, Standard_True);
        if (QApplication::keyboardModifiers() == Qt::ControlModifier) {
            /*
            // 【多选模式】如果按住了 Ctrl 键
            // 使用 AIS_SelectionScheme_Add：将检测到的物体“添加”到当前选择集中
            // 允许用户同时选中多个零件进行布尔运算
            */
            m_context->SelectDetected(AIS_SelectionScheme_Add);
        } else {
            // 【单选模式】如果没有按 Ctrl
            // SelectDetected() 默认会清除之前的选择，只选中当前这一个
            const auto pick_status = m_context->SelectDetected();
            // 如果成功选中了且仅选中了一个对象
            if (pick_status == AIS_SOP_OneSelected) {
        
                Handle(SelectMgr_EntityOwner) owner = m_context->DetectedOwner();
                if (owner) {
                    Handle(StdSelect_BRepOwner) brepOwner = Handle(StdSelect_BRepOwner)::DownCast(owner);
                    if (!brepOwner.IsNull()) {
                        TopoDS_Shape shape = brepOwner->Shape();
                        // HandleSelectedShape 会根据选中的是点、边还是面，在界面上显示坐标或长度
                        handleSelectedShape(shape);
                    }
                }
            }
        }
        // 鼠标下方有物体（HasDetected）  准备拖拽
        if (m_context->HasDetected()) {
            m_isDraggingObject = true;

            m_draggedObject = m_context->DetectedInteractive();

            // 记录初始状态：记录物体移动前的“原始位置矩阵”
            // 这是为了计算相对位移：新位置 = 偏移量 * 原始位置
            m_dragStartTrsf = m_draggedObject->Transformation();


            Standard_Real x, y, z;
            // 使用 m_view->Convert 将屏幕坐标转为 3D 坐标
            // 注意：这里的转换通常基于投影平面，用于计算鼠标移动的 Delta 值
            m_view->Convert(event->pos().x() * ratio, event->pos().y() * ratio, x, y, z);

            m_dragStartPos3d.SetCoord(x, y, z);

            Handle(SelectMgr_EntityOwner) owner = m_context->DetectedOwner();
            if (owner) {
                // StdSelect_BRepOwner：这是 Open CASCADE 中专门用来管理 BRep (边界表示法) 拓扑结构的所有者类。只有它才包含具体的 点(Vertex)、边(Edge)、面(Face) 信息
                Handle(StdSelect_BRepOwner) brepOwner = Handle(StdSelect_BRepOwner)::DownCast(owner);
                if (!brepOwner.IsNull()) {
                    TopoDS_Shape detectedSubShape = brepOwner->Shape();
                    handleSelectedShape(detectedSubShape);
                } else {
                    showFilletUI(false);
                }
            } else {
                showFilletUI(false);
            }
        } else {

            m_isPanningCamera = true;
            // 记录起点：记录鼠标点击的 2D 屏幕坐标
            // 平移算法是基于 2D 屏幕像素差值的 (dx, dy)
            m_x_max = event->x();
            m_y_max = event->y();
            showFilletUI(false);
        }
        m_view->Update();
    } else if ( event->button() & Qt::RightButton) {
        // 在执行任何判断前，强制让交互上下文 (m_context) 刷新一次检测结果。确保系统准确知道鼠标当前是否悬停在某个物体上。这是后续分支判断的基础
        m_context->MoveTo(event->pos().x() * ratio, event->pos().y() * ratio, m_view, Standard_True);
        m_isMenu = true;
        if (m_context->HasDetected()) {

            m_isRotatingObject = true;
            m_rotatedObject = m_context->DetectedInteractive();
            m_dragStartTrsf = m_rotatedObject->Transformation();

            m_dragStartPos2d.SetX(event->pos().x());
            m_dragStartPos2d.SetY(event->pos().y());
        } else {

            m_isRotatingCamera = true;

            m_view->StartRotation(event->pos().x() * ratio, event->pos().y() * ratio);
        }
    }
}

// 鼠标释放事件：结束拖拽、旋转操作并发出信号，弹出右键菜单
void DrawWidget::mouseReleaseEvent(QMouseEvent *event) {
    const qreal ratio = devicePixelRatioF();
    m_context->MoveTo(event->pos().x() * ratio, event->pos().y() * ratio, m_view, Standard_True);
    if (event->button() == Qt::LeftButton) {
        //if (m_isDraggingObject) {
        //    emit shapeMoved(m_draggedObject->Transformation());
        //}
    } else if (event->button() == Qt::RightButton) {
        //if (m_isRotatingObject) {
        //    emit shapeMoved(m_rotatedObject->Transformation());
        //}
        if (m_isMenu) {
            const int aNbSelected = m_context->NbSelected();

            m_fuseAction->setEnabled(aNbSelected > 1);
            m_cutAction->setEnabled(aNbSelected > 1);
            m_commonAction->setEnabled(aNbSelected > 1);
            m_visualizePointsAction->setEnabled(aNbSelected == 1);
            m_hlrPreciseAction->setEnabled(aNbSelected == 1);
            m_hlrDiscreteAction->setEnabled(aNbSelected == 1);
            m_contextMenu->exec(event->globalPos());
        }
    }
    const int aNbSelected = m_context->NbSelected();
    emit selectObject(aNbSelected);

    m_isDraggingObject = false;
    m_isPanningCamera = false;
    m_isRotatingCamera = false;
    m_isRotatingObject = false;
    m_isMenu = false;
    m_draggedObject.Nullify();
    m_rotatedObject.Nullify();
}

// 鼠标移动事件：处理物体拖拽、相机平移、物体旋转和相机旋转逻辑
void DrawWidget::mouseMoveEvent(QMouseEvent *event) {

    const qreal ratio = devicePixelRatioF();
    if (m_isDraggingObject) {
        gp_Pnt currentPos3d;
        Standard_Real x, y, z;
        m_view->Convert(event->pos().x() * ratio, event->pos().y() * ratio, x, y, z);
        currentPos3d.SetCoord(x, y, z);
       
        gp_Vec deltaPos = currentPos3d.XYZ() - m_dragStartPos3d.XYZ();

        // 构建平移变换矩阵
        gp_Trsf deltaTrsf;
        deltaTrsf.SetTranslation(deltaPos);
        // 应用变换 (原始位置 * 偏移)
        gp_Trsf finalTrsf = deltaTrsf * m_dragStartTrsf;
        // 更新物体位置
        m_context->SetLocation(m_draggedObject, TopLoc_Location(finalTrsf));
        m_context->Update(m_draggedObject, Standard_True);
    }

    else if (m_isPanningCamera) {
        m_view->Pan((event->pos().x() - m_x_max) * ratio, (m_y_max - event->pos().y()) * ratio);
        m_x_max = event->x();
        m_y_max = event->y();

    } 
    else if (m_isRotatingObject) {
        m_isMenu = false;
        const double rotationSpeed = 0.005;

        gp_Pnt2d currentPos2d(event->pos().x(), event->pos().y());
        double deltaX = currentPos2d.X() - m_dragStartPos2d.X();
        double deltaY = currentPos2d.Y() - m_dragStartPos2d.Y();

        Handle(Graphic3d_Camera) aCamera = m_view->Camera();

        gp_Dir aCamUp = aCamera->Up();

        gp_Dir aCamDir = aCamera->Direction();

        gp_Dir aCamRight = aCamDir.Crossed(aCamUp);

       
        /*
        鼠标左右移动 (deltaX) -> 绕着 竖直轴 (aCamUp) 旋转
        这里的 (0,0,0) 指的是旋转轴心。
        由于这个旋转矩阵 totalDeltaRotation 是被右乘到物体原始矩阵上的 (m_dragStartTrsf * totalDeltaRotation)。
        这意味着旋转是发生在物体自身的局部坐标系中的。
        在物体的局部坐标系里，(0,0,0) 就是物体自己的中心（或者说建模原点）。
        */
        gp_Trsf deltaRotY;
        deltaRotY.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), aCamUp), deltaX * rotationSpeed);

        gp_Trsf deltaRotX;
        deltaRotX.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), aCamRight), deltaY * rotationSpeed);

        gp_Trsf totalDeltaRotation = deltaRotY * deltaRotX;

        gp_Trsf finalTrsf = totalDeltaRotation *  m_dragStartTrsf ;

        m_context->SetLocation(m_rotatedObject, TopLoc_Location(finalTrsf));
        m_context->Update(m_rotatedObject, Standard_True);

    } else if (m_isRotatingCamera) {
        m_isMenu = false;
        m_isPanningCamera = false;
        m_isDraggingObject = false;
        // 它根据鼠标的起点（在 StartRotation 中记录）和当前点，计算摄像机应该绕着目标中心旋转多少度
        m_view->Rotation(event->x() * ratio, event->pos().y() * ratio);
        light_direction->SetDirection(m_view->Camera()->Direction());
    }

    else {
        m_context->MoveTo(event->pos().x() * ratio, event->pos().y() * ratio, m_view, Standard_True);
    }
}

void DrawWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_E) {
        qDebug() << "按 E 键结束绘制";
        finishDrawing();
        return;
    }

    QWidget::keyPressEvent(event);
}

// 滚轮事件：处理视图缩放
void DrawWidget::wheelEvent(QWheelEvent *event) {
    const qreal ratio = devicePixelRatioF();
    // 以鼠标光标为中心的缩放
    m_view->StartZoomAtPoint((int) (event->position().x() * ratio), (int) (event->position().y() * ratio));
    m_view->ZoomAtPoint(0, 0, event->angleDelta().y(), 0);
}

// 创建右键菜单，包含选择模式、布尔运算及HLR功能
void DrawWidget::createContextMenu() {
    m_contextMenu = new QMenu(this);
    m_selectionModeGroup = new QActionGroup(this);

    const auto selectShapes = m_selectionModeGroup->addAction("Select Shapes");
    // 将一个整数值隐藏存储在菜单项中
    selectShapes->setData(static_cast<int>(TopAbs_SHAPE));
    const auto selectVector = m_selectionModeGroup->addAction("Select Vertices");
    selectVector->setData(static_cast<int>(TopAbs_VERTEX));
    m_selectionModeGroup->addAction("Select Edges")->setData(static_cast<int>(TopAbs_EDGE));
    m_selectionModeGroup->addAction("Select Wires")->setData(static_cast<int>(TopAbs_WIRE));
    m_selectionModeGroup->addAction("Select Faces")->setData(static_cast<int>(TopAbs_FACE));
    m_selectionModeGroup->addAction("Select Shells")->setData(static_cast<int>(TopAbs_SHELL));
    m_selectionModeGroup->addAction("Select Solids")->setData(static_cast<int>(TopAbs_SOLID));
    m_selectionModeGroup->addAction("Select CompSolids")->setData(static_cast<int>(TopAbs_COMPSOLID));
    m_selectionModeGroup->addAction("Select Compounds")->setData(static_cast<int>(TopAbs_COMPOUND));

    const auto action_list = m_selectionModeGroup->actions();
    for (const auto &action: action_list) {
        action->setCheckable(true);
    }
    selectShapes->setChecked(true);
    m_contextMenu->addActions(action_list);
    // 在 selectVector ("Select Vertices") 之前插入一条横线
    m_contextMenu->insertSeparator(selectVector);
    connect(m_contextMenu, &QMenu::triggered, this, &DrawWidget::selectActionTriggered);

    m_contextMenu->addSeparator();

    m_fuseAction = new QAction(tr("Fuse Shapes (Union)"), this);
    m_cutAction = new QAction(tr("Cut Shapes (Difference)"), this);
    m_commonAction = new QAction(tr("Common Shapes (Intersection)"), this);

    connect(m_fuseAction, &QAction::triggered, this, &DrawWidget::onFuseShapes);
    connect(m_cutAction, &QAction::triggered, this, &DrawWidget::onCutShapes);
    connect(m_commonAction, &QAction::triggered, this, &DrawWidget::onCommonShapes);

    m_contextMenu->addAction(m_fuseAction);
    m_contextMenu->addAction(m_cutAction);
    m_contextMenu->addAction(m_commonAction);

    m_fuseAction->setEnabled(false);
    m_cutAction->setEnabled(false);
    m_commonAction->setEnabled(false);

    m_contextMenu->addSeparator();

    m_visualizePointsAction = new QAction(tr("Visualize Internal Points"), this);

    connect(m_visualizePointsAction, &QAction::triggered, this, &DrawWidget::onVisualizeInternalPoints);

    m_contextMenu->addAction(m_visualizePointsAction);

    m_visualizePointsAction->setEnabled(false);

    m_hlrPreciseAction = new QAction(tr("Run Precise HLR (Slow, B-Rep)"), this);
    m_hlrDiscreteAction = new QAction(tr("Run Discrete HLR (Fast, Mesh)"), this);



    m_contextMenu->addAction(m_hlrPreciseAction);
    m_contextMenu->addAction(m_hlrDiscreteAction);

    m_hlrPreciseAction->setEnabled(false);
    m_hlrDiscreteAction->setEnabled(false);
}

// 决定了当你在模型上点击鼠标时，选中的是整个物体，还是物体上的一个点、一条边或一个面。
void DrawWidget::setSelectionMode(TopAbs_ShapeEnum mode) {
    if (!m_context.IsNull()) {
        m_context->ClearSelected(Standard_False);
        m_context->Deactivate();
        m_context->Activate(AIS_Shape::SelectionMode(mode));
        m_view->Redraw();
        qDebug() << "选择模式已切换到:" << static_cast<int>(mode);
    }
}

// 处理选中的形状信息，若是边则显示圆角UI，并发送选中信息信号
void DrawWidget::handleSelectedShape(const TopoDS_Shape &shape) {

    // 1. 生成信息文本
    std::string infoText;
    if (shape.ShapeType() == TopAbs_VERTEX) {



        // 如果是点，显示坐标
        TopoDS_Vertex v = TopoDS::Vertex(shape);
        gp_Pnt p = BRep_Tool::Pnt(v);
        // 保留2位小数
        char buf[100];
        snprintf(buf, sizeof(buf), "(%.2f, %.2f, %.2f)", p.X(), p.Y(), p.Z());
        infoText = std::string(buf);
    }

    else if (shape.ShapeType() == TopAbs_EDGE) {
        showFilletUI(true);
        // 1. 保存选中的边
        m_selectedEdge = TopoDS::Edge(shape);

        // 2. 保存选中的父物体 (AIS_Shape)
        // 我们需要知道这条边属于哪个大的 3D 物体，因为倒角操作是针对父物体的。
        // DetectedInteractive() 返回当前鼠标下的整个交互对象。
        Handle(AIS_InteractiveObject) detectedObj = m_context->DetectedInteractive();
        m_selectedAISShape = Handle(AIS_Shape)::DownCast(detectedObj);


        // 如果是边，显示长度
        GProp_GProps props;
        BRepGProp::LinearProperties(shape, props);
        char buf[100];
        snprintf(buf, sizeof(buf), "L: %.2f", props.Mass());
        infoText = std::string(buf);
    }
    else {
        // 其他形状，显示类型
        infoText = "Shape";
    }

    // 2. 计算显示位置 (包围盒中心)
    Bnd_Box bbox;
    BRepBndLib::Add(shape, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    /*
    求几何中心（平均值）：
    (xmin + xmax) / 2.0：算出 X 轴的中点。
    (ymin + ymax) / 2.0：算出 Y 轴的中点。
    (zmin + zmax) / 2.0：算出 Z 轴（高度）的中点。
    如果不加偏移，文字就会正好显示在物体的肚子里（正中心）。
    应用 Z 轴偏移 (+ 0.2)：
    */
    gp_Pnt textPos((xmin + xmax) / 2.0, (ymin + ymax) / 2.0, (zmin + zmax) / 2.0 + 0.2);

    // 3. 生成 3D 文字形状
    try {
        // 移除旧标签
        if (!m_infoLabel.IsNull()) {
            m_context->Remove(m_infoLabel, Standard_True);
            m_infoLabel.Nullify();
        }

        // 调用 make_shapes 的 makeText 函数
        // 注意：字体大小设为 0.2 (根据您的场景单位可能需要调整)
        m_make_shapes.makeText(infoText, 0.2);
        TopoDS_Shape textTopo = m_make_shapes.s_;

        if (!textTopo.IsNull()) {
            // 4. 将文字移动到目标位置 (默认生成在原点)
            gp_Trsf transform;
            transform.SetTranslation(gp_Vec(gp_Pnt(0, 0, 0), textPos));
            BRepBuilderAPI_Transform xform(textTopo, transform);

            // 5. 显示
            m_infoLabel = new AIS_Shape(xform.Shape());
            m_infoLabel->SetColor(Quantity_NOC_YELLOW); // 黄色文字
            m_infoLabel->SetZLayer(Graphic3d_ZLayerId_Topmost); // 确保显示在最上层
            m_context->Display(m_infoLabel, Standard_True);
        }
    }
    catch (const std::exception& e) {
        qWarning() << "Text generation failed:" << e.what();
    }

}

// 控制圆角/倒角参数设置面板的显示与隐藏
void DrawWidget::showFilletUI(bool show) {
    if (show) {
        if (m_filletWidget) {
            QSize uiSize = m_filletWidget->sizeHint(); // 获取面板的推荐大小
            int x = width() - uiSize.width() - 10;  // 计算 X 坐标：窗口总宽度 - 面板宽度 - 10像素边距
            int y = 10;  // 计算 Y 坐标：距离顶部 10 像素
            m_filletWidget->move(qMax(0, x), y);
            m_filletWidget->show();
        }
    } else {
        if (m_filletWidget) {
            m_filletWidget->hide();
        }
        // 【关键状态管理】
        // 当 UI 隐藏时，意味着圆角操作流程结束或取消。
        // 必须把之前记录的“选中的物体”和“选中的边”指针置空 (Nullify)。
        // 防止后续在没有 UI 的情况下误操作旧的对象，或者导致内存泄露/野指针引用。
        m_selectedAISShape.Nullify();
        m_selectedEdge.Nullify();
    }

    if (!m_infoLabel.IsNull()) {
        m_context->Remove(m_infoLabel, Standard_True);
        m_infoLabel.Nullify();
    }
}

// -----------------------------------------------------------------------------
// 公共辅助函数：封装圆角与倒角通用部分
// -----------------------------------------------------------------------------
void DrawWidget::applyEdgeOperation(const QString& opName, double paramValue,
    std::function<TopoDS_Shape(const TopoDS_Shape&, const TopoDS_Edge&, double)> operation)
{
    // 基础校验
    if (m_selectedAISShape.IsNull() || m_selectedEdge.IsNull()) {
        qWarning() << opName << ": No valid edge or parent shape selected.";
        return;
    }
    if (paramValue <= 0) {
        qWarning() << opName << ": Parameter value must be greater than 0.";
        return;
    }

    TopoDS_Shape parentShape = m_selectedAISShape->Shape();
    if (parentShape.ShapeType() != TopAbs_SOLID && parentShape.ShapeType() != TopAbs_SHELL) {
        qWarning() << opName << ": Only solid/shell supports this operation.";
        return;
    }

    try {
        // 执行具体的几何算法 (回调函数)
        // 注意：如果算法失败，回调内部应该抛出异常或返回空形状
        TopoDS_Shape newShape = operation(parentShape, m_selectedEdge, paramValue);

        if (newShape.IsNull()) {
            qWarning() << opName << ": Operation returned null shape.";
            return;
        }

        // 提取结果中的 Solid
        TopoDS_Shape newSolidShape;
        TopExp_Explorer solidExplorer;
        for (solidExplorer.Init(newShape, TopAbs_SOLID); solidExplorer.More(); solidExplorer.Next()) {
            newSolidShape = solidExplorer.Current();
            break;
        }

        if (newSolidShape.IsNull()) {
            qWarning() << opName << ": Operation succeeded but no Solid was returned.";
            return;
        }

        // 更新 3D 显示 (保持颜色和透明度)
        Quantity_Color color;
        m_selectedAISShape->Color(color);
        Standard_Real transparency = m_selectedAISShape->Transparency();

        // 移除旧物体 (不立即刷新)
        m_context->Remove(m_selectedAISShape, Standard_False);

        // 更新内部数据
        m_shape.s_ = newSolidShape;
        m_shape.color_ = color;

        // 创建新显示对象
        Handle(AIS_Shape) newAisShape = new AIS_Shape(newSolidShape);
        newAisShape->SetColor(color);
        newAisShape->SetTransparency(transparency);
        newAisShape->SetDisplayMode(AIS_Shaded);

        // 显示并选中
        m_context->Display(newAisShape, Standard_True);
        m_context->SetCurrentObject(newAisShape, Standard_True);

        // 关闭 UI
        showFilletUI(false);

    }
    catch (const std::exception& e) {
        qCritical() << opName << ": Exception occurred:" << e.what();
        showFilletUI(false);
    }
    catch (...) {
        qCritical() << opName << ": Unknown exception occurred.";
        showFilletUI(false);
    }
}


void DrawWidget::onApplyFillet() {
    if (!m_filletSpinBox) return;

    // 调用公共函数
    applyEdgeOperation("onApplyFillet", m_filletSpinBox->value(),
        [](const TopoDS_Shape& parent, const TopoDS_Edge& edge, double radius) -> TopoDS_Shape {

            BRepFilletAPI_MakeFillet filletMaker(parent);
            filletMaker.Add(radius, edge);
            filletMaker.Build();

            if (!filletMaker.IsDone()) {
                throw std::runtime_error("Fillet algorithm failed (radius too large?)");
            }
            return filletMaker.Shape();
        });
}



void DrawWidget::onApplyChamfer() {
    if (!m_chamferSpinBox) return;

   
    applyEdgeOperation("onApplyChamfer", m_chamferSpinBox->value(),
        [](const TopoDS_Shape& parent, const TopoDS_Edge& edge, double dist) -> TopoDS_Shape {

            // 倒角特有的逻辑：查找相邻面
            TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
            TopExp::MapShapesAndAncestors(parent, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);

            if (!edgeFaceMap.Contains(edge)) {
                throw std::runtime_error("No adjacent faces found for selected edge.");
            }

            const TopTools_ListOfShape& adjacentFaces = edgeFaceMap.FindFromKey(edge);
            if (adjacentFaces.IsEmpty()) {
                throw std::runtime_error("Adjacent faces list is empty.");
            }

            // 通常取第一个面作为参考面
            TopoDS_Face refFace = TopoDS::Face(adjacentFaces.First());

            BRepFilletAPI_MakeChamfer chamferMaker(parent);
            // 这里假设是对称倒角 (dist, dist)
            chamferMaker.Add(dist, dist, edge, refFace);
            chamferMaker.Build();

            if (!chamferMaker.IsDone()) {
                throw std::runtime_error("Chamfer algorithm failed (distance too large?)");
            }
            return chamferMaker.Shape();
        });
}

// -----------------------------------------------------------------------------
// 公共辅助函数：封装布尔运算通用流程
// -----------------------------------------------------------------------------
void DrawWidget::applyBooleanOperation(const QString& opName,
    std::function<TopoDS_Shape(const TopoDS_Shape&, const TopoDS_Shape&)> booleanOp)
{
    // 检查选中数量
    if (m_context->NbSelected() < 2) {
        qWarning() << opName << ": Need at least 2 objects selected.";
        return;
    }

    // 收集选中物体
    AIS_ListOfInteractive selectedList;
    m_context->InitSelected();
    while (m_context->MoreSelected()) {
        selectedList.Append(m_context->SelectedInteractive());
        m_context->NextSelected();
    }

    // 准备基础形状 (Base Shape)
    Handle(AIS_Shape) baseAisShape = Handle(AIS_Shape)::DownCast(selectedList.First());
    if (baseAisShape.IsNull()) {
        qWarning() << opName << ": Selected object 1 is not a shape.";
        return;
    }

    // 应用变换矩阵：将物体从局部坐标系转到世界坐标系
    // 如果不这样做，布尔运算会假设所有物体都在生成时的原点位置
    TopoDS_Shape resultShape = BRepBuilderAPI_Transform(
        baseAisShape->Shape(),
        baseAisShape->Transformation(),
        Standard_True // Copy geometry
    ).Shape();

 
    // 已经把位移应用到顶点上了，现在请把你的‘相对位移记录’清零
    resultShape.Location(TopLoc_Location());

    // 遍历并执行运算
    AIS_ListOfInteractive::Iterator anIter(selectedList);
    anIter.Next(); // 跳过第一个（Base）

    for (; anIter.More(); anIter.Next()) {
        Handle(AIS_Shape) toolAisShape = Handle(AIS_Shape)::DownCast(anIter.Value());
        if (toolAisShape.IsNull()) { continue; }

        // 应用变换矩阵到工具形状 (Tool Shape)
        TopoDS_Shape transformedToolShape = BRepBuilderAPI_Transform(
            toolAisShape->Shape(),
            toolAisShape->Transformation(),
            Standard_True
        ).Shape();
        transformedToolShape.Location(TopLoc_Location());

        try {
            // --- 调用回调函数执行具体的算法 ---
            TopoDS_Shape nextShape = booleanOp(resultShape, transformedToolShape);

            if (!nextShape.IsNull()) {
                resultShape = nextShape;
                resultShape.Location(TopLoc_Location());
            }
            else {
                qWarning() << opName << ": Algorithm failed (null result).";
            }
        }
        catch (const std::exception& e) {
            qCritical() << opName << ": Operation failed:" << e.what();
        }
    }

    // 移除旧物体
    for (AIS_ListOfInteractive::Iterator it(selectedList); it.More(); it.Next()) {
        m_context->Remove(it.Value(), Standard_False);
    }

    // 显示新物体 (继承第一个物体的外观属性)
    m_shape.s_ = resultShape;
    Handle(AIS_Shape) newAisShape = new AIS_Shape(resultShape);

    Quantity_Color color;
    baseAisShape->Color(color);
    m_shape.color_ = color;

    newAisShape->SetColor(color);
    newAisShape->SetTransparency(baseAisShape->Transparency());
    newAisShape->SetMaterial(Graphic3d_NameOfMaterial_Stone);

    m_context->Display(newAisShape, Standard_True);
    m_context->SetCurrentObject(newAisShape, Standard_True);
}

void DrawWidget::onFuseShapes() {
    applyBooleanOperation("Fuse", [](const TopoDS_Shape& s1, const TopoDS_Shape& s2) -> TopoDS_Shape {
        BRepAlgoAPI_Fuse algo(s1, s2);
        if (algo.IsDone()) {
            return algo.Shape();
        }
        throw std::runtime_error("Shapes do not intersect or invalid topology");
        });
}
void DrawWidget::onCutShapes() {
    applyBooleanOperation("Cut", [](const TopoDS_Shape& s1, const TopoDS_Shape& s2) -> TopoDS_Shape {
        BRepAlgoAPI_Cut algo(s1, s2);
        if (algo.IsDone()) {
            return algo.Shape();
        }
        throw std::runtime_error("Cut operation failed");
        });
}
void DrawWidget::onCommonShapes() {
    applyBooleanOperation("Common", [](const TopoDS_Shape& s1, const TopoDS_Shape& s2) -> TopoDS_Shape {
        BRepAlgoAPI_Common algo(s1, s2);
        if (algo.IsDone()) {
            return algo.Shape();
        }
        throw std::runtime_error("Common operation failed");
        });
}



std::vector<gp_XYZ> GetPointsInsideSolid(
    const TopoDS_Shape& solidShape,
    const int density = 20)
{
    std::vector<gp_XYZ> innerPoints;
    if (solidShape.IsNull() || density <= 0) return innerPoints;

    // 计算包围盒
    Bnd_Box aabb;
    BRepBndLib::Add(solidShape, aabb, true);
    if (aabb.IsVoid()) return innerPoints;

    gp_XYZ Pmin = aabb.CornerMin().XYZ();
    gp_XYZ Pmax = aabb.CornerMax().XYZ();
    gp_XYZ D = Pmax - Pmin;

    if (D.X() < Precision::Confusion()) D.SetX(1.0);
    if (D.Y() < Precision::Confusion()) D.SetY(1.0);
    if (D.Z() < Precision::Confusion()) D.SetZ(1.0);

    // 计算步长与网格数量
    double dims[3] = { D.X(), D.Y(), D.Z() };
    const double mind = Min(dims[0], Min(dims[1], dims[2]));
    const double d = mind / density;

    if (d < Precision::Confusion()) return innerPoints;

    const int nx = static_cast<int>(ceil(dims[0] / d));
    const int ny = static_cast<int>(ceil(dims[1] / d));
    const int nz = static_cast<int>(ceil(dims[2] / d));

    // 预估内存
    long long estimatedCount = (long long)nx * ny * nz * 0.5;
    if (estimatedCount > 0) innerPoints.reserve(estimatedCount);

    // 准备求交器 (单线程只需要一个实例)
    IntCurvesFace_ShapeIntersector intersector;
    intersector.Load(solidShape, Precision::Confusion());

    // 循环 (Y 和 Z)
    for (int j = 0; j <= ny; ++j) {
        for (int k = 0; k <= nz; ++k) {

            double y = Pmin.Y() + j * d;
            double z = Pmin.Z() + k * d;

            if (y > Pmax.Y() || z > Pmax.Z()) continue;

            // 构建射线
            gp_Lin ray(gp_Pnt(Pmin.X() - d, y, z), gp::DX());

            // 执行求交
            intersector.Perform(ray, 0, D.X() + 3.0 * d);

            if (intersector.NbPnt() == 0) continue;

            // 收集交点
            std::vector<double> intersectionParams;
            intersectionParams.reserve(intersector.NbPnt());
            for (int p = 1; p <= intersector.NbPnt(); ++p) {
                intersectionParams.push_back(intersector.WParameter(p));
            }
            std::sort(intersectionParams.begin(), intersectionParams.end());

            // 奇偶规则填充
            for (size_t idx = 0; idx + 1 < intersectionParams.size(); idx += 2) {
                double enterDist = intersectionParams[idx];
                double exitDist = intersectionParams[idx + 1];

                int i_start = static_cast<int>(ceil((enterDist / d) - 1.0));
                int i_end = static_cast<int>(floor((exitDist / d) - 1.0));

                if (i_start < 0) i_start = 0;
                if (i_end > nx) i_end = nx;

                for (int i = i_start; i <= i_end; ++i) {
                    // 存入结果向量
                    innerPoints.emplace_back(
                        Pmin.X() + i * d,
                        y,
                        z
                    );
                }
            }
        }
    }

    return innerPoints;
}



//std::vector<gp_XYZ> GetPointsInsideSolid(
//    const TopoDS_Shape& solidShape,
//    const int density = 20) 
//{
//    std::vector<gp_XYZ> innerPoints;
//    if (solidShape.IsNull() || density <= 0) return innerPoints;
//
//    Bnd_Box aabb;
//    BRepBndLib::Add(solidShape, aabb, true);
//    if (aabb.IsVoid()) return innerPoints;
//
//    gp_XYZ Pmin = aabb.CornerMin().XYZ();
//    gp_XYZ Pmax = aabb.CornerMax().XYZ();
//    gp_XYZ D = Pmax - Pmin;
//
//    // 防止极小包围盒导致除零
//    if (D.X() < Precision::Confusion()) D.SetX(1.0);
//    if (D.Y() < Precision::Confusion()) D.SetY(1.0);
//    if (D.Z() < Precision::Confusion()) D.SetZ(1.0);
//
//    // 步长计算逻辑优化建议：防止某一维过细导致点数爆炸
//    // 这里保持你的逻辑，但建议生产环境增加步长下限
//    double dims[3] = { D.X(), D.Y(), D.Z() };
//    const double mind = Min(dims[0], Min(dims[1], dims[2]));
//    const double d = mind / density;
//
//    if (d < Precision::Confusion()) return innerPoints;
//
//    const int nslice[3] = {
//            static_cast<int>(ceil(dims[0] / d)),
//            static_cast<int>(ceil(dims[1] / d)),
//            static_cast<int>(ceil(dims[2] / d)) };
//
//    // 预估总点数用于 reserve，减少 vector 扩容开销
//    long long estimatedCount = (long long)nslice[0] * nslice[1] * nslice[2];
//    // 安全检查：如果点数过大（如超过100万），可能需要报警或截断
//    if (estimatedCount > 1000000) {
//        qWarning() << "Estimated points too high:" << estimatedCount;
//        // 可以在此强制调整 d
//    }
//
//    // 线程安全的容器 (OpenMP 下向 vector push_back 不安全)
//    // 这里简单处理：每个线程私有 vector，最后合并
//
//#pragma omp parallel
//    {
//        // 每个线程必须拥有独立的 Classifier，因为它是非线程安全的且包含状态
//        BRepClass3d_SolidClassifier classifier(solidShape);
//        std::vector<gp_XYZ> threadPoints;
//
//#pragma omp for collapse(3) schedule(dynamic)
//        for (int i = 0; i <= nslice[0]; ++i) {
//            for (int j = 0; j <= nslice[1]; ++j) {
//                for (int k = 0; k <= nslice[2]; ++k) {
//                    gp_XYZ currentPoint = Pmin + gp_XYZ(d * i, d * j, d * k);
//                    // 快速包围盒预检查（虽然外层循环基于包围盒，但这是为了后续可能的扩展）
//                    if (currentPoint.X() > Pmax.X() || currentPoint.Y() > Pmax.Y() || currentPoint.Z() > Pmax.Z())
//                        continue;
//                    classifier.Perform(currentPoint, Precision::Confusion());
//
//                    if (classifier.State() == TopAbs_IN) {
//                        threadPoints.push_back(currentPoint);
//                    }
//                }
//            }
//        }
//
//        // 合并结果到主 vector (临界区)
//#pragma omp critical
//        {
//            innerPoints.insert(innerPoints.end(), threadPoints.begin(), threadPoints.end());
//        }
//    }
//
//    return innerPoints;
//}

// 可视化选中实体的内部采样点
void DrawWidget::onVisualizeInternalPoints() {
    if (m_context->NbSelected() != 1) {
        qWarning("Please select exactly one shape to visualize its internal points.");
        return;
    }
    m_context->InitSelected();
    Handle(AIS_InteractiveObject) selectedAIS = m_context->SelectedInteractive();
    Handle(AIS_Shape) selectedAisShape = Handle(AIS_Shape)::DownCast(selectedAIS);
    if (selectedAisShape.IsNull()) return;

    TopoDS_Shape rawShape = selectedAisShape->Shape();
    gp_Trsf shapeTrsf = selectedAisShape->Transformation();
    BRepBuilderAPI_Transform transform(rawShape, shapeTrsf, Standard_True);
    TopoDS_Shape shapeInWorld = transform.Shape();
    shapeInWorld.Location(TopLoc_Location());  // 清除位置属性，防止双重变换

    // 计算点
    const int density = 30;
    
    std::vector<gp_XYZ> innerPoints = GetPointsInsideSolid(shapeInWorld, density);

    if (innerPoints.empty()) {
        qWarning("No internal points found.");
        return;
    }

    qDebug() << "Generated" << innerPoints.size() << "internal points.";

    // 创建图形数据数组  innerPoints: 这是之前算法算出来的 std::vector<gp_XYZ>，它只是普通的 C++ 内存数组，显卡（GPU）无法直接读取。
    // Graphic3d_ArrayOfPoints 本质上是VBO
    Handle(Graphic3d_ArrayOfPoints) aPoints = new Graphic3d_ArrayOfPoints(static_cast<Standard_Integer>(innerPoints.size()));

    for (const gp_XYZ& pt : innerPoints) {
        aPoints->AddVertex(pt);
    }

    // 创建点云对象
    Handle(AIS_PointCloud) aisPointCloud = new AIS_PointCloud();
    aisPointCloud->SetPoints(aPoints);
    
    // AIS_PointCloud 的样式设置与 AIS_Shape 不同
    Handle(Prs3d_Drawer) drawer = aisPointCloud->Attributes();
    drawer->SetPointAspect(new Prs3d_PointAspect(Aspect_TOM_POINT, Quantity_NOC_BLACK, 5.0));

    // 显示
    m_context->Display(aisPointCloud, Standard_True);
}



void DrawWidget::onMakeCompound() {
    // 检查选中数量
    if (m_context->NbSelected() < 2) {
        qWarning() << "MakeCompound: Please select at least 2 objects.";
        return;
    }

    // 收集选中物体
    std::vector<Shape> shapesToCompound;
    AIS_ListOfInteractive selectedList; // 用于后续删除
    m_context->InitSelected();

    while (m_context->MoreSelected()) {
        Handle(AIS_InteractiveObject) obj = m_context->SelectedInteractive();
        selectedList.Append(obj);

        Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(obj);
        if (!aisShape.IsNull()) {
        

            TopoDS_Shape rawShape = aisShape->Shape();
            gp_Trsf trsf = aisShape->Transformation();

            // 应用变换
            BRepBuilderAPI_Transform transformer(rawShape, trsf, Standard_True);
            TopoDS_Shape transformedShape = transformer.Shape();

            // 封装到 Shape 对象
            Shape s;
            s.s_ = transformedShape;

            // 尝试保留颜色 (取第一个物体的颜色作为复合体的参考，或保留各自颜色结构)
            Quantity_Color col;
            aisShape->Color(col);
            s.color_ = col;

            shapesToCompound.push_back(s);
        }
        m_context->NextSelected();
    }

    if (shapesToCompound.empty()) return;

    try {
        Shape compoundResult = m_make_shapes.make_compound(shapesToCompound);

        if (compoundResult.s_.IsNull()) {
            qWarning() << "MakeCompound: Result is null.";
            return;
        }

        for (AIS_ListOfInteractive::Iterator it(selectedList); it.More(); it.Next()) {
            m_context->Remove(it.Value(), Standard_False);
        }
        Handle(AIS_Shape) newAis = new AIS_Shape(compoundResult.s_);

        // 设置颜色 (使用默认或第一个物体的颜色)
        if (!shapesToCompound.empty()) {
            newAis->SetColor(shapesToCompound[0].color_);
        }
        else {
            newAis->SetColor(Quantity_NOC_YELLOW);
        }

        newAis->SetMaterial(Graphic3d_NameOfMaterial_Stone);

        // 更新 m_shape 记录 (用于导出等)
        m_shape = compoundResult;

        m_context->Display(newAis, Standard_True);
        m_context->SetCurrentObject(newAis, Standard_True); // 选中新生成的物体

        qDebug() << "Compound created successfully from" << shapesToCompound.size() << "shapes.";

    }
    catch (const std::exception& e) {
        qCritical() << "MakeCompound Failed:" << e.what();
    }
}


// 打开文件对话框读取STL文件
void DrawWidget::readStl() {
    QString filePath = QFileDialog::getOpenFileName(this, "Open File", "", "All Files (*.*)");
    if (!filePath.isEmpty()) {
        qDebug() << "Selected file:" << filePath;
        Shape s;
        auto shape = s.open_stl(filePath.toStdString());
        onDisplayShape(shape);
    }
}

// 打开文件对话框将当前模型导出为STL/STEP
void DrawWidget::exportStl() {
    QString filePath = QFileDialog::getSaveFileName(this,
                                                    "Export File",
                                                    "",
                                                    "All Files (*.*)");
    if (!filePath.isEmpty()) {
        qDebug() << "Export path:" << filePath;
        std::string exportPath = filePath.toStdString();
        m_shape.export_step(exportPath);
    }
}

// 读取XDE文档（STEP/IGES等）并显示
void DrawWidget::readXde() {
    QString filePath = QFileDialog::getOpenFileName(this, "Open File", "", "All Files (*.*)");
    if (!filePath.isEmpty()) {
        qDebug() << "Selected file:" << filePath;
        Shape s;
        auto doc = s.read_xde_document(filePath.toStdString());
        DisplayScene cmd(doc, m_context);
        if (!cmd.Execute()) {
            std::cout << "Failed to visualize CAD model with `DisplayScene` command." << std::endl;
        }
        m_shape.s_ = cmd.GetLastShape();
    }
}

// 将当前模型导出为XDE支持的格式
void DrawWidget::exportXde() {
    QString filePath = QFileDialog::getSaveFileName(this,
                                                    "Export File",
                                                    "",
                                                    "All Files (*.*)");
    if (!filePath.isEmpty()) {
        qDebug() << "Export path:" << filePath;
        std::string exportPath = filePath.toStdString();
        m_shape.export_xde(exportPath);
    }
}
