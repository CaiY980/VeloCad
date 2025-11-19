#include "draw_widget.h"
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
#include <TopoDS.hxx>
#include <V3d_View.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Trsf.hxx>

#include <BRepBndLib.hxx>
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
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
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
#ifdef _WIN32

#include <WNT_Window.hxx>

#elif defined(__APPLE__)
#include <Cocoa_Window.hxx>
#else
#include <Xw_Window.hxx>
#endif

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
}

// 根据ID创建1D形状（线、圆、曲线等），计算包围盒并显示
void DrawWidget::create_shape_1d(int id) {
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
            std::array<double, 3> p1 = {-1.0, 0.0, 0.0};
            std::array<double, 3> p2 = {1.0, 0.0, 0.0};
            m_make_shapes.makeLine(p1, p2);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 1) {
            std::vector<std::array<double, 3>> bezier_poles = {
                    {-1.5, 0.0, 0.0},
                    {-0.5, 1.0, 0.0},
                    {0.5, -1.0, 0.0},
                    {1.5, 0.0, 0.0}};
            m_make_shapes.makeBezier(bezier_poles);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 2) {
            std::vector<std::array<double, 3>> spline_poles = {
                    {-1.2, 0.0, 0.0},
                    {-0.8, 0.8, 0.0},
                    {0.8, -0.8, 0.0},
                    {1.2, 0.0, 0.0}};
            std::vector<double> spline_knots = {0.0, 1.0};
            std::vector<int> spline_mults = {4, 4};
            int spline_degree = 3;
            m_make_shapes.makeBSpline(spline_poles, spline_knots, spline_mults, spline_degree);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
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
        } else if (id == 5) {
            std::array<double, 3> center = {0.0, 0.0, 0.0};
            std::array<double, 3> normal = {0.0, 0.0, 1.0};
            double radius1 = 1.2;
            double radius2 = 0.6;
            double p1 = -2.0;
            double p2 = 2.0;
            m_make_shapes.makeHyperbola(center, normal, radius1, radius2, p1, p2);
            shape = m_make_shapes.s_;
            color = default_colors.at(id);
        } else if (id == 6) {
            std::array<double, 3> center = {0.0, 0.0, 0.0};
            std::array<double, 3> normal = {0.0, 0.0, 1.0};
            double radius = 1.0;
            double p1 = -2.0;
            double p2 = 2.0;
            m_make_shapes.makeParabola(center, normal, radius, p1, p2);
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
#if 1
    AIS_ListOfInteractive objects;
    m_context->DisplayedObjects(objects);
    for (const auto &x: objects) {
        if ((x == view_cube) || (x == origin_coord)) { continue; }
        m_context->Remove(x, Standard_True);
    }
#else
    m_context->RemoveAll(Standard_True);
    m_context->Display(view_cube, Standard_True);
    m_context->Display(origin_coord, Standard_True);
#endif
    update();
}

// 初始化交互上下文、3D查看器和光源设置
void DrawWidget::initialize_context() {
    Handle(Aspect_DisplayConnection) display_connection = new Aspect_DisplayConnection();
    WId window_handle = (WId) winId();
    Handle(WNT_Window) wind = new WNT_Window((Aspect_Handle) window_handle);

    m_viewer = new V3d_Viewer(new OpenGl_GraphicDriver(display_connection));
    m_view = m_viewer->CreateView();
    m_view->SetWindow(wind);
    if (!wind->IsMapped()) {
        wind->Map();
    }
    m_context = new AIS_InteractiveContext(m_viewer);
    light_direction = new V3d_Light(Graphic3d_TypeOfLightSource_Directional);
    light_direction->SetDirection(m_view->Camera()->Direction());
    m_viewer->AddLight(new V3d_Light(Graphic3d_TypeOfLightSource_Ambient));
    m_viewer->AddLight(light_direction);
    m_viewer->SetLightOn();
    Quantity_Color background_color;
    Quantity_Color::ColorFromHex("#F0F8FF", background_color);
    m_view->SetBackgroundColor(background_color);
    m_view->MustBeResized();

    create_view_cube();
    create_origin_coord();

    m_context->SetDisplayMode(AIS_Shaded, Standard_True);
    Handle(Prs3d_Drawer) highlight_style = m_context->HighlightStyle();
    highlight_style->SetMethod(Aspect_TOHM_COLOR);
    highlight_style->SetColor(Quantity_NOC_LIGHTYELLOW);
    highlight_style->SetDisplayMode(1);
    highlight_style->SetTransparency(0.2f);
    Handle(Prs3d_Drawer) t_select_style = m_context->SelectionStyle();
    t_select_style->SetMethod(Aspect_TOHM_COLOR);
    t_select_style->SetColor(Quantity_NOC_LIGHTSEAGREEN);
    t_select_style->SetDisplayMode(1);
    t_select_style->SetTransparency(0.4f);
    m_view->SetZoom(100);
    m_viewer->SetRectangularGridValues(0, 0, 1, 1, 0);
    m_viewer->SetRectangularGridGraphicValues(2.01, 2.01, 0);
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

// 鼠标按下事件：处理对象选择、相机平移初始化和拖拽初始化
void DrawWidget::mousePressEvent(QMouseEvent *event) {

    const qreal ratio = devicePixelRatioF();
    m_isDraggingObject = false;
    m_isPanningCamera = false;
    m_isRotatingCamera = false;
    m_isRotatingObject = false;
    m_isMenu = false;
    if (event->button() & Qt::LeftButton) {
        m_x_max = event->x();
        m_y_max = event->y();
        m_context->MoveTo(event->pos().x() * ratio, event->pos().y() * ratio, m_view, Standard_True);
        if (QApplication::keyboardModifiers() == Qt::ControlModifier) {
            m_context->SelectDetected(AIS_SelectionScheme_Add);
        } else {
            const auto pick_status = m_context->SelectDetected();
            if (pick_status == AIS_SOP_OneSelected) {
                Handle(SelectMgr_EntityOwner) owner = m_context->DetectedOwner();
                if (owner) {
                    Handle(StdSelect_BRepOwner) brepOwner = Handle(StdSelect_BRepOwner)::DownCast(owner);
                    if (!brepOwner.IsNull()) {
                        TopoDS_Shape shape = brepOwner->Shape();
                        handleSelectedShape(shape);
                    }
                }
            }
        }
        if (m_context->HasDetected()) {
            m_isDraggingObject = true;

            m_draggedObject = m_context->DetectedInteractive();


            m_dragStartTrsf = m_draggedObject->Transformation();


            Standard_Real x, y, z;

            m_view->Convert(event->pos().x() * ratio, event->pos().y() * ratio, x, y, z);

            m_dragStartPos3d.SetCoord(x, y, z);

            Handle(SelectMgr_EntityOwner) owner = m_context->DetectedOwner();
            if (owner) {
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

            m_x_max = event->x();
            m_y_max = event->y();
            showFilletUI(false);
        }
        m_view->Update();
    } else if (event->button() & Qt::RightButton) {

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
        if (m_isDraggingObject) {

            emit shapeMoved(m_draggedObject->Transformation());
        }
    } else if (event->button() == Qt::RightButton) {

        if (m_isRotatingObject) {
            emit shapeMoved(m_rotatedObject->Transformation());
        }
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

        gp_Trsf deltaTrsf;
        deltaTrsf.SetTranslation(deltaPos);

        gp_Trsf finalTrsf = deltaTrsf * m_dragStartTrsf;

        m_context->SetLocation(m_draggedObject, TopLoc_Location(finalTrsf));
        m_context->Update(m_draggedObject, Standard_True);

    }

    else if (m_isPanningCamera) {
        m_view->Pan((event->pos().x() - m_x_max) * ratio, (m_y_max - event->pos().y()) * ratio);
        m_x_max = event->x();
        m_y_max = event->y();

    } else if (m_isRotatingObject) {
        m_isMenu = false;
        const double rotationSpeed = 0.01;

        gp_Pnt2d currentPos2d(event->pos().x(), event->pos().y());
        double deltaX = currentPos2d.X() - m_dragStartPos2d.X();
        double deltaY = currentPos2d.Y() - m_dragStartPos2d.Y();

        Handle(Graphic3d_Camera) aCamera = m_view->Camera();

        gp_Dir aCamUp = aCamera->Up();

        gp_Dir aCamDir = aCamera->Direction();

        gp_Dir aCamRight = aCamDir.Crossed(aCamUp);

        gp_Trsf deltaRotY;
        deltaRotY.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), aCamUp), deltaX * rotationSpeed);

        gp_Trsf deltaRotX;
        deltaRotX.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), aCamRight), deltaY * rotationSpeed);

        gp_Trsf totalDeltaRotation = deltaRotY * deltaRotX;

        gp_Trsf finalTrsf = m_dragStartTrsf * totalDeltaRotation;

        m_context->SetLocation(m_rotatedObject, TopLoc_Location(finalTrsf));
        m_context->Update(m_rotatedObject, Standard_True);

    } else if (m_isRotatingCamera) {
        m_isMenu = false;
        m_isPanningCamera = false;
        m_isDraggingObject = false;
        m_view->Rotation(event->x() * ratio, event->pos().y() * ratio);
        light_direction->SetDirection(m_view->Camera()->Direction());
    }

    else {
        m_context->MoveTo(event->pos().x() * ratio, event->pos().y() * ratio, m_view, Standard_True);
    }
}

// 滚轮事件：处理视图缩放
void DrawWidget::wheelEvent(QWheelEvent *event) {
    const qreal ratio = devicePixelRatioF();
    m_view->StartZoomAtPoint((int) (event->position().x() * ratio), (int) (event->position().y() * ratio));
    m_view->ZoomAtPoint(0, 0, event->angleDelta().y(), 0);
}

// 创建右键菜单，包含选择模式、布尔运算及HLR功能
void DrawWidget::createContextMenu() {
    m_contextMenu = new QMenu(this);
    m_selectionModeGroup = new QActionGroup(this);
    const auto selectShapes = m_selectionModeGroup->addAction("Select Shapes");
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

    connect(m_hlrPreciseAction, &QAction::triggered, this, &DrawWidget::onRunPreciseHLR);
    connect(m_hlrDiscreteAction, &QAction::triggered, this, &DrawWidget::onRunDiscreteHLR);

    m_contextMenu->addAction(m_hlrPreciseAction);
    m_contextMenu->addAction(m_hlrDiscreteAction);

    m_hlrPreciseAction->setEnabled(false);
    m_hlrDiscreteAction->setEnabled(false);
}

// 设置交互上下文的选择模式（如点、边、面、实体）
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
    if (shape.ShapeType() == TopAbs_EDGE) {
        m_selectedEdge = TopoDS::Edge(shape);
        Handle(AIS_InteractiveObject) selectedInteractive = m_context->DetectedInteractive();
        m_selectedAISShape = Handle(AIS_Shape)::DownCast(selectedInteractive);
        if (!m_selectedEdge.IsNull() && !m_selectedAISShape.IsNull()) {
            showFilletUI(true);
        } else {
            showFilletUI(false);
        }
        static const std::unordered_map<int, std::string> type_map = {
                {GeomAbs_Line, "line"},
                {GeomAbs_Circle, "circle"},
                {GeomAbs_Ellipse, "ellipse"},
                {GeomAbs_Hyperbola, "hyperbola"},
                {GeomAbs_Parabola, "parabola"},
                {GeomAbs_BezierCurve, "bezier_curve"},
                {GeomAbs_BSplineCurve, "bspline_curve"},
                {GeomAbs_OffsetCurve, "offset_curve"},
                {GeomAbs_OtherCurve, "other_curve"}};
        const TopoDS_Edge edge = TopoDS::Edge(shape);
        BRepAdaptor_Curve C(edge);
        const std::string type_str = type_map.find(C.GetType()) != type_map.end() ? type_map.at(C.GetType()) : "";
        QString type = QString::fromStdString(type_str);
        const gp_Pnt &start_point = BRep_Tool::Pnt(TopExp::FirstVertex(edge));
        const gp_Pnt &end_point = BRep_Tool::Pnt(TopExp::LastVertex(edge));
        QJsonObject jsonObj{{
                {"edge", QJsonObject({
                                 {"type", type},
                                 {"first", QJsonObject({
                                                   {"x", start_point.X()},
                                                   {"y", start_point.Y()},
                                                   {"z", start_point.Z()},
                                           })},
                                 {"last", QJsonObject({
                                                  {"x", end_point.X()},
                                                  {"y", end_point.Y()},
                                                  {"z", end_point.Z()},
                                          })},
                         })},
        }};
        emit selectedShapeInfo(QJsonDocument(jsonObj));
    } else if (shape.ShapeType() == TopAbs_VERTEX) {
        m_selectedEdge.Nullify();
        showFilletUI(false);
        const TopoDS_Vertex vertex = TopoDS::Vertex(shape);
        gp_Pnt point = BRep_Tool::Pnt(vertex);
        QJsonObject jsonObj{
                {"vertex", QJsonObject({{"x", point.X()}, {"y", point.Y()}, {"z", point.Z()}})},
        };
        emit selectedShapeInfo(QJsonDocument(jsonObj));
    } else {

        m_selectedEdge.Nullify();
        showFilletUI(false);
        std::stringstream ss;
        shape.DumpJson(ss);
        std::string json_str = "{" + ss.str() + "}";
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json_str), &error);
        if (error.error == QJsonParseError::NoError) {
            emit selectedShapeInfo(doc);
        }
    }
}

// 控制圆角/倒角参数设置面板的显示与隐藏
void DrawWidget::showFilletUI(bool show) {
    if (show) {
        if (m_filletWidget) {
            QSize uiSize = m_filletWidget->sizeHint();
            int x = width() - uiSize.width() - 10;
            int y = 10;
            m_filletWidget->move(qMax(0, x), y);
            m_filletWidget->show();
        }
    } else {
        if (m_filletWidget) {
            m_filletWidget->hide();
        }
        m_selectedAISShape.Nullify();
        m_selectedEdge.Nullify();
    }
}

// 对选中的边应用圆角操作，并更新模型显示
void DrawWidget::onApplyFillet() {
    if (m_selectedAISShape.IsNull() || m_selectedEdge.IsNull()) {
        qWarning("onApplyFillet: No valid edge or parent shape selected.");
        return;
    }
    if (!m_filletSpinBox) {
        qWarning("onApplyFillet: Fillet spin box not initialized.");
        return;
    }

    double radius = m_filletSpinBox->value();
    if (radius <= 0) {
        qWarning("onApplyFillet: Radius must be greater than 0.");
        return;
    }

    TopoDS_Shape parentShape = m_selectedAISShape->Shape();
    if (parentShape.ShapeType() != TopAbs_SOLID && parentShape.ShapeType() != TopAbs_SHELL) {
        qWarning("onApplyFillet: Only solid/shell supports fillet.");
        return;
    }

    try {
        BRepFilletAPI_MakeFillet filletMaker(parentShape);
        filletMaker.Add(radius, m_selectedEdge);

        filletMaker.Build();
        if (!filletMaker.IsDone()) {
            qWarning("onApplyFillet: Fillet operation failed. Possible reasons: radius too large / edge invalid.");
            return;
        }

        TopoDS_Shape newShape = filletMaker.Shape();
        if (newShape.IsNull()) {
            qWarning("onApplyFillet: Result shape is null.");
            return;
        }

        TopoDS_Shape newSolidShape;
        TopExp_Explorer solidExplorer;

        for (solidExplorer.Init(newShape, TopAbs_SOLID);
             solidExplorer.More();
             solidExplorer.Next()) {
            newSolidShape = solidExplorer.Current();
            break;
        }

        if (newSolidShape.IsNull()) {
            qWarning("onApplyFillet: Fillet operation succeeded but no Solid was returned.");
            qWarning("Possible reasons: invalid input shape, incorrect fillet parameters, or non-solid input.");
            return;
        }

        Quantity_Color color;
        m_selectedAISShape->Color(color);
        Standard_Real transparency = m_selectedAISShape->Transparency();

        m_context->Remove(m_selectedAISShape, Standard_False);
        m_shape.s_ = newSolidShape;
        Handle(AIS_Shape) newAisShape = new AIS_Shape(newSolidShape);
        m_shape.color_ = color;
        newAisShape->SetColor(color);
        newAisShape->SetTransparency(transparency);
        newAisShape->SetDisplayMode(AIS_Shaded);

        m_context->Display(newAisShape, Standard_True);
        m_context->SetCurrentObject(newAisShape, Standard_True);

        showFilletUI(false);

    } catch (const std::exception &e) {
        qCritical() << "onApplyFillet: Exception occurred:" << e.what();
        showFilletUI(false);
    }
}

// 对选中的边应用倒角操作，并更新模型显示
void DrawWidget::onApplyChamfer() {
    if (m_selectedAISShape.IsNull() || m_selectedEdge.IsNull()) {
        qWarning("onApplyChamfer: No valid edge or parent shape selected.");
        return;
    }
    if (!m_chamferSpinBox) {
        qWarning("onApplyChamfer: Chamfer spin box not initialized.");
        return;
    }

    double distance = m_chamferSpinBox->value();
    if (distance <= 0) {
        qWarning("onApplyChamfer: Distance must be greater than 0.");
        return;
    }

    TopoDS_Shape parentShape = m_selectedAISShape->Shape();
    if (parentShape.ShapeType() != TopAbs_SOLID && parentShape.ShapeType() != TopAbs_SHELL) {
        qWarning("onApplyChamfer: Only solid/shell supports chamfer.");
        return;
    }

    try {
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
        TopExp::MapShapesAndAncestors(parentShape, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);

        if (!edgeFaceMap.Contains(m_selectedEdge)) {
            qWarning("onApplyChamfer: No adjacent faces found for selected edge.");
            return;
        }

        const TopTools_ListOfShape &adjacentFaces = edgeFaceMap.FindFromKey(m_selectedEdge);
        if (adjacentFaces.IsEmpty()) {
            qWarning("onApplyChamfer: Adjacent faces list is empty.");
            return;
        }

        TopoDS_Face refFace = TopoDS::Face(adjacentFaces.First());

        BRepFilletAPI_MakeChamfer chamferMaker(parentShape);
        chamferMaker.Add(distance, distance, m_selectedEdge, refFace);

        chamferMaker.Build();
        if (!chamferMaker.IsDone()) {
            qWarning("onApplyChamfer: Chamfer operation failed. Possible reasons: distance too large / edge invalid.");
            return;
        }

        TopoDS_Shape newShape = chamferMaker.Shape();
        if (newShape.IsNull()) {
            qWarning("onApplyChamfer: Result shape is null.");
            return;
        }

        TopoDS_Shape newSolidShape;
        TopExp_Explorer solidExplorer;

        for (solidExplorer.Init(newShape, TopAbs_SOLID);
             solidExplorer.More();
             solidExplorer.Next()) {
            newSolidShape = solidExplorer.Current();
            break;
        }

        if (newSolidShape.IsNull()) {
            qWarning("onApplyFillet: Fillet operation succeeded but no Solid was returned.");
            qWarning("Possible reasons: invalid input shape, incorrect fillet parameters, or non-solid input.");
            return;
        }

        Quantity_Color color;
        m_selectedAISShape->Color(color);
        m_shape.color_ = color;
        Standard_Real transparency = m_selectedAISShape->Transparency();

        m_context->Remove(m_selectedAISShape, Standard_False);
        m_shape.s_ = newSolidShape;
        Handle(AIS_Shape) newAisShape = new AIS_Shape(newSolidShape);
        newAisShape->SetColor(color);
        newAisShape->SetTransparency(transparency);
        newAisShape->SetDisplayMode(AIS_Shaded);

        m_context->Display(newAisShape, Standard_True);
        m_context->SetCurrentObject(newAisShape, Standard_True);

        showFilletUI(false);

    } catch (const std::exception &e) {
        qCritical() << "onApplyChamfer: Exception occurred:" << e.what();
        showFilletUI(false);
    }
}

// 执行布尔并集运算：合并选中的多个形状
void DrawWidget::onFuseShapes() {
    if (m_context->NbSelected() < 2) { return; }

    AIS_ListOfInteractive selectedList;
    m_context->InitSelected();
    while (m_context->MoreSelected()) {
        selectedList.Append(m_context->SelectedInteractive());
        m_context->NextSelected();
    }

    Handle(AIS_InteractiveObject) baseAIS = selectedList.First();
    Handle(AIS_Shape) baseAisShape = Handle(AIS_Shape)::DownCast(baseAIS);
    if (baseAisShape.IsNull()) {
        qWarning("Fuse: Selected object 1 is not a shape.");
        return;
    }

    TopoDS_Shape baseRawShape = baseAisShape->Shape();
    BRepBuilderAPI_Transform baseTransform(baseRawShape, baseAisShape->Transformation(), Standard_True);
    TopoDS_Shape resultShape = baseTransform.Shape();
    resultShape.Location(TopLoc_Location());

    AIS_ListOfInteractive::Iterator anIter(selectedList);
    anIter.Next();

    for (; anIter.More(); anIter.Next()) {
        Handle(AIS_Shape) toolAisShape = Handle(AIS_Shape)::DownCast(anIter.Value());
        if (toolAisShape.IsNull()) { continue; }

        TopoDS_Shape toolRawShape = toolAisShape->Shape();
        TopLoc_Location toolLocation = toolAisShape->LocalTransformation();
        BRepBuilderAPI_Transform toolTransform(toolRawShape, toolLocation.Transformation(), Standard_True);
        TopoDS_Shape transformedToolShape = toolTransform.Shape();
        transformedToolShape.Location(TopLoc_Location());

        try {
            BRepAlgoAPI_Fuse algo(resultShape, transformedToolShape);
            if (algo.IsDone()) {
                resultShape = algo.Shape();
                resultShape.Location(TopLoc_Location());
            } else {
                qWarning("Fuse: BRepAlgoAPI_Fuse failed. Possible reasons: shapes do not intersect / invalid topology.");
            }
        } catch (const std::exception &e) {
            qDebug() << "Fuse operation failed:" << e.what();
        }
    }

    for (AIS_ListOfInteractive::Iterator it(selectedList); it.More(); it.Next()) {
        m_context->Remove(it.Value(), Standard_False);
    }
    m_shape.s_ = resultShape;
    Handle(AIS_Shape) newAisShape = new AIS_Shape(resultShape);
    Quantity_Color color;
    baseAisShape->Color(color);
    newAisShape->SetColor(color);
    m_shape.color_ = color;
    newAisShape->SetTransparency(baseAisShape->Transparency());
    newAisShape->SetMaterial(Graphic3d_NameOfMaterial_Stone);
    m_context->Display(newAisShape, Standard_True);
    m_context->SetCurrentObject(newAisShape, Standard_True);
}

// 执行布尔差集运算：用后续形状剪裁第一个形状
void DrawWidget::onCutShapes() {
    if (m_context->NbSelected() < 2) { return; }

    AIS_ListOfInteractive selectedList;
    m_context->InitSelected();
    while (m_context->MoreSelected()) {
        selectedList.Append(m_context->SelectedInteractive());
        m_context->NextSelected();
    }

    Handle(AIS_Shape) baseAisShape = Handle(AIS_Shape)::DownCast(selectedList.First());
    if (baseAisShape.IsNull()) {
        qWarning("Cut: Selected object 1 is not a shape.");
        return;
    }

    TopoDS_Shape resultShape = BRepBuilderAPI_Transform(
            baseAisShape->Shape(),
            baseAisShape->Transformation(),
            Standard_True);
    resultShape.Location(TopLoc_Location());

    AIS_ListOfInteractive::Iterator anIter(selectedList);
    anIter.Next();

    for (; anIter.More(); anIter.Next()) {
        Handle(AIS_Shape) toolAisShape = Handle(AIS_Shape)::DownCast(anIter.Value());
        if (toolAisShape.IsNull()) { continue; }

        TopoDS_Shape transformedToolShape = BRepBuilderAPI_Transform(
                toolAisShape->Shape(),
                toolAisShape->Transformation(),
                Standard_True);
        transformedToolShape.Location(TopLoc_Location());

        try {
            BRepAlgoAPI_Cut algo(resultShape, transformedToolShape);
            if (algo.IsDone()) {
                resultShape = algo.Shape();
                resultShape.Location(TopLoc_Location());
            } else {
                qWarning("Cut: BRepAlgoAPI_Cut failed. Possible reasons: shapes do not intersect / invalid topology.");
            }
        } catch (const std::exception &e) {
            qDebug() << "Cut operation failed:" << e.what();
        }
    }

    for (AIS_ListOfInteractive::Iterator it(selectedList); it.More(); it.Next()) {
        m_context->Remove(it.Value(), Standard_False);
    }
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

// 执行布尔交集运算：保留选中形状的重叠部分
void DrawWidget::onCommonShapes() {
    if (m_context->NbSelected() < 2) { return; }

    AIS_ListOfInteractive selectedList;
    m_context->InitSelected();
    while (m_context->MoreSelected()) {
        selectedList.Append(m_context->SelectedInteractive());
        m_context->NextSelected();
    }

    Handle(AIS_Shape) baseAisShape = Handle(AIS_Shape)::DownCast(selectedList.First());
    if (baseAisShape.IsNull()) {
        qWarning("Common: Selected object 1 is not a shape.");
        return;
    }

    TopoDS_Shape resultShape = BRepBuilderAPI_Transform(
            baseAisShape->Shape(),
            baseAisShape->Transformation(),
            Standard_True);
    resultShape.Location(TopLoc_Location());

    AIS_ListOfInteractive::Iterator anIter(selectedList);
    anIter.Next();

    for (; anIter.More(); anIter.Next()) {
        Handle(AIS_Shape) toolAisShape = Handle(AIS_Shape)::DownCast(anIter.Value());
        if (toolAisShape.IsNull()) { continue; }

        TopoDS_Shape transformedToolShape = BRepBuilderAPI_Transform(
                toolAisShape->Shape(),
                toolAisShape->Transformation(),
                Standard_True);
        transformedToolShape.Location(TopLoc_Location());

        try {
            BRepAlgoAPI_Common algo(resultShape, transformedToolShape);
            if (algo.IsDone()) {
                resultShape = algo.Shape();
                resultShape.Location(TopLoc_Location());
            } else {
                qWarning("Common: BRepAlgoAPI_Common failed. Possible reasons: shapes do not intersect / invalid topology.");
            }
        } catch (const std::exception &e) {
            qDebug() << "Common operation failed:" << e.what();
        }
    }

    for (AIS_ListOfInteractive::Iterator it(selectedList); it.More(); it.Next()) {
        m_context->Remove(it.Value(), Standard_False);
    }
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

// 辅助函数：使用扫描线算法计算实体内部的点
std::vector<gp_XYZ> GetPointsInsideSolid(
        const TopoDS_Shape &solidShape,
        const int density = 20) {
    std::vector<gp_XYZ> innerPoints;

    if (solidShape.IsNull() || density <= 0) {
        return innerPoints;
    }

    Bnd_Box aabb;
    BRepBndLib::Add(solidShape, aabb, true);

    if (aabb.IsVoid()) {
        return innerPoints;
    }

    gp_XYZ Pmin = aabb.CornerMin().XYZ();
    gp_XYZ Pmax = aabb.CornerMax().XYZ();
    gp_XYZ D = Pmax - Pmin;

    if (D.X() < Precision::Confusion()) D.SetX(1.0);
    if (D.Y() < Precision::Confusion()) D.SetY(1.0);
    if (D.Z() < Precision::Confusion()) D.SetZ(1.0);

    double dims[3] = {D.X(), D.Y(), D.Z()};
    const double mind = Min(dims[0], Min(dims[1], dims[2]));
    const double d = mind / density;

    if (d < Precision::Confusion()) {
        return innerPoints;
    }

    const int nslice[3] = {
            static_cast<int>(Round(dims[0] / d)) + 1,
            static_cast<int>(Round(dims[1] / d)) + 1,
            static_cast<int>(Round(dims[2] / d)) + 1};

    BRepClass3d_SolidClassifier classifier(solidShape);
    const double tolerance = Precision::Confusion();

    for (int i = 0; i <= nslice[0]; ++i) {
        for (int j = 0; j <= nslice[1]; ++j) {
            for (int k = 0; k <= nslice[2]; ++k) {
                gp_XYZ currentPoint = Pmin + gp_XYZ(d * i, d * j, d * k);
                classifier.Perform(currentPoint, tolerance);
                if (classifier.State() == TopAbs_IN) {
                    innerPoints.push_back(currentPoint);
                }
            }
        }
    }
    return innerPoints;
}

// 可视化选中实体的内部采样点
void DrawWidget::onVisualizeInternalPoints() {
    if (m_context->NbSelected() != 1) {
        qWarning("Please select exactly one shape to visualize its internal points.");
        return;
    }

    m_context->InitSelected();
    Handle(AIS_InteractiveObject) selectedAIS = m_context->SelectedInteractive();
    Handle(AIS_Shape) selectedAisShape = Handle(AIS_Shape)::DownCast(selectedAIS);

    if (selectedAisShape.IsNull()) {
        qWarning("Selected object is not a shape.");
        return;
    }

    TopoDS_Shape rawShape = selectedAisShape->Shape();
    gp_Trsf shapeTrsf = selectedAisShape->Transformation();

    BRepBuilderAPI_Transform transform(rawShape, shapeTrsf, Standard_True);
    TopoDS_Shape shapeInWorld = transform.Shape();
    shapeInWorld.Location(TopLoc_Location());

    const int density = 30;
    std::vector<gp_XYZ> innerPoints = GetPointsInsideSolid(shapeInWorld, density);

    if (innerPoints.empty()) {
        qWarning("No internal points found or shape is not a solid.");
        return;
    }

    qDebug() << "Generated" << innerPoints.size() << "internal points.";

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);

    for (const gp_XYZ &pt: innerPoints) {
        builder.Add(compound, BRepBuilderAPI_MakeVertex(pt));
    }

    Handle(AIS_Shape) aisPoints = new AIS_Shape(compound);

    aisPoints->SetDisplayMode(0);
    aisPoints->SetColor(Quantity_NOC_RED);

    Handle(Prs3d_Drawer) drawer = aisPoints->Attributes();
    drawer->SetPointAspect(new Prs3d_PointAspect(Aspect_TOM_O_POINT, Quantity_NOC_RED, 2.0));

    m_context->Display(aisPoints, Standard_True);
}

// 辅助函数：构建3D曲线
const TopoDS_Shape &Build3dCurves(const TopoDS_Shape &shape) {
    for (TopExp_Explorer it(shape, TopAbs_EDGE); it.More(); it.Next())
        BRepLib::BuildCurve3d(TopoDS::Edge(it.Current()));

    return shape;
}

// 辅助函数：执行精确隐藏线消除 (HLR) 算法计算
TopoDS_Shape HLR(const TopoDS_Shape &shape,
                 const gp_Dir &direction,
                 const t_hlrEdges visibility) {
    Handle(HLRBRep_Algo) brep_hlr = new HLRBRep_Algo;
    brep_hlr->Add(shape);

    gp_Ax2 transform(gp::Origin(), direction);
    HLRAlgo_Projector projector(transform);
    brep_hlr->Projector(projector);
    brep_hlr->Update();
    brep_hlr->Hide();

    HLRBRep_HLRToShape shapes(brep_hlr);

    TopoDS_Shape V = Build3dCurves(shapes.VCompound());
    TopoDS_Shape V1 = Build3dCurves(shapes.Rg1LineVCompound());
    TopoDS_Shape VN = Build3dCurves(shapes.RgNLineVCompound());
    TopoDS_Shape VO = Build3dCurves(shapes.OutLineVCompound());
    TopoDS_Shape VI = Build3dCurves(shapes.IsoLineVCompound());
    TopoDS_Shape H = Build3dCurves(shapes.HCompound());
    TopoDS_Shape H1 = Build3dCurves(shapes.Rg1LineHCompound());
    TopoDS_Shape HN = Build3dCurves(shapes.RgNLineHCompound());
    TopoDS_Shape HO = Build3dCurves(shapes.OutLineHCompound());
    TopoDS_Shape HI = Build3dCurves(shapes.IsoLineHCompound());

    TopoDS_Compound C;
    BRep_Builder().MakeCompound(C);
    if (!V.IsNull() && visibility.OutputVisibleSharpEdges)
        BRep_Builder().Add(C, V);
    if (!V1.IsNull() && visibility.OutputVisibleSmoothEdges)
        BRep_Builder().Add(C, V1);
    if (!VN.IsNull() && visibility.OutputVisibleOutlineEdges)
        BRep_Builder().Add(C, VN);
    if (!VO.IsNull() && visibility.OutputVisibleSewnEdges)
        BRep_Builder().Add(C, VO);
    if (!VI.IsNull() && visibility.OutputVisibleIsoLines)
        BRep_Builder().Add(C, VI);
    if (!H.IsNull() && visibility.OutputHiddenSharpEdges)
        BRep_Builder().Add(C, H);
    if (!H1.IsNull() && visibility.OutputHiddenSmoothEdges)
        BRep_Builder().Add(C, H1);
    if (!HN.IsNull() && visibility.OutputHiddenOutlineEdges)
        BRep_Builder().Add(C, HN);
    if (!HO.IsNull() && visibility.OutputHiddenSewnEdges)
        BRep_Builder().Add(C, HO);

    if (!HI.IsNull() && visibility.OutputHiddenIsoLines)
        BRep_Builder().Add(C, HI);

    gp_Trsf T;
    T.SetTransformation(gp_Ax3(transform));
    T.Invert();

    return C.Moved(T);
}

// 辅助函数：执行离散（网格化）隐藏线消除 (DHLR) 算法计算
TopoDS_Shape DHLR(const TopoDS_Shape &shape,
                  const gp_Dir &direction,
                  const t_hlrEdges visibility) {
    gp_Ax2 transform(gp::Origin(), direction);

    HLRAlgo_Projector projector(transform);

    Handle(HLRBRep_PolyAlgo) polyAlgo = new HLRBRep_PolyAlgo;
    polyAlgo->Projector(projector);
    polyAlgo->Load(shape);
    polyAlgo->Update();

    HLRBRep_PolyHLRToShape HLRToShape;
    HLRToShape.Update(polyAlgo);

    TopoDS_Compound C;
    BRep_Builder().MakeCompound(C);

    TopoDS_Shape vcompound = HLRToShape.VCompound();
    if (!vcompound.IsNull())
        BRep_Builder().Add(C, vcompound);
    vcompound = HLRToShape.OutLineVCompound();
    if (!vcompound.IsNull())
        BRep_Builder().Add(C, vcompound);

    gp_Trsf T;
    T.SetTransformation(gp_Ax3(transform));
    T.Invert();

    return C.Moved(T);
}


// 运行精确HLR算法并显示结果
void DrawWidget::onRunPreciseHLR() {
    if (m_context->NbSelected() != 1) {
        qWarning("请选择一个形状以运行 HLR。");
        return;
    }
    m_context->InitSelected();
    Handle(AIS_InteractiveObject) selectedAIS = m_context->SelectedInteractive();

    Handle(AIS_Shape) selectedAisShape = Handle(AIS_Shape)::DownCast(selectedAIS);
    if (selectedAisShape.IsNull()) {
        qWarning("选中的对象不是一个形状。");
        return;
    }

    TopoDS_Shape shape = selectedAisShape->Shape();
    gp_Trsf shapeTrsf = selectedAisShape->Transformation();
    BRepBuilderAPI_Transform transform(shape, shapeTrsf, Standard_True);
    shape = transform.Shape();
    shape.Location(TopLoc_Location());

    qDebug() << "开始【精确 HLR】计算 (UI 将冻结)...";

    t_hlrEdges style;
    TopoDS_Shape phlr = HLR(shape, gp::DX(), style);

    if (!phlr.IsNull()) {
        Handle(AIS_Shape) aisHLR = new AIS_Shape(phlr);
        aisHLR->SetColor(Quantity_NOC_BLUE);
        m_context->Display(aisHLR, Standard_True);
    } else {
        qWarning("精确 HLR (HLR) 未返回结果。");
    }
}

// 运行离散HLR算法并显示结果
void DrawWidget::onRunDiscreteHLR() {
    if (m_context->NbSelected() != 1) {
        qWarning("请选择一个形状以运行 HLR。");
        return;
    }
    m_context->InitSelected();
    Handle(AIS_InteractiveObject) selectedAIS = m_context->SelectedInteractive();
    Handle(AIS_Shape) selectedAisShape = Handle(AIS_Shape)::DownCast(selectedAIS);
    if (selectedAisShape.IsNull()) {
        qWarning("选中的对象不是一个形状。");
        return;
    }

    TopoDS_Shape shape = selectedAisShape->Shape();
    gp_Trsf shapeTrsf = selectedAisShape->Transformation();
    BRepBuilderAPI_Transform transform(shape, shapeTrsf, Standard_True);
    shape = transform.Shape();
    shape.Location(TopLoc_Location());

    qDebug() << "开始【离散 HLR】计算 (UI 将短暂冻结)...";

    BRepMesh_IncrementalMesh meshGen(shape, 1.0);

    t_hlrEdges style;
    TopoDS_Shape dhlr = DHLR(shape, gp::DX(), style);

    if (!dhlr.IsNull()) {
        Bnd_Box aabb;
        BRepBndLib::Add(shape, aabb);
        double xDim = 100.0;
        if (!aabb.IsVoid()) {
            double xRange = aabb.CornerMax().X() - aabb.CornerMin().X();
            if (xRange > 1.0) xDim = xRange;
        }
        gp_Trsf T;
        T.SetTranslation(gp_Vec(xDim * 1.1, 0, 0));

        Handle(AIS_Shape) aisDHLR = new AIS_Shape(dhlr.Moved(T));
        aisDHLR->SetColor(Quantity_NOC_RED);
        m_context->Display(aisDHLR, Standard_True);
    } else {
        qWarning("离散 HLR (DHLR) 未返回结果。");
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
        m_shape.s_ = cmd.getShape();
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