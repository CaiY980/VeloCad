#ifndef MAKE_SHAPES_H
#define MAKE_SHAPES_H

#include "shape.h"
#include <array>

#include <string>
#include <vector>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepPrimAPI_MakeWedge.hxx>
#include <Font_BRepTextBuilder.hxx>
#include <Geom_Line.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BezierCurve.hxx>
#include <gp_Circ.hxx>
#include <gp_Elips.hxx>
#include <gp_Hypr.hxx>
#include <gp_Parab.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>
class MakeShapes : public Shape {
public:
    // 构造函数
    MakeShapes() = default;

    // ==================== 三维基本形状 ====================
    // 长方体
    void makeBox(const double &_x = 1, const double &_y = 1, const double &_z = 1) {
        BRepPrimAPI_MakeBox make_box(_x, _y, _z);
        if (make_box.Wedge().IsDegeneratedShape()) {
            throw std::runtime_error("Is Degenerated Shape!");
        }
        s_ = make_box;
    }

    // 圆柱体
    void makeCylinder(const double &_r = 1, const double &_h = 1) {
        BRepPrimAPI_MakeCylinder make_cylinder(_r, _h);
        s_ = make_cylinder;
    }

    // 圆锥体
    void makeCone(const double &R1 = 1, const double &R2 = 0, const double &H = 1) {
        if (std::abs(R1 - R2) < 1e-4) {
            throw std::runtime_error("R1==R2");
        }
        BRepPrimAPI_MakeCone make_cone(R1, R2, H);
        s_ = make_cone;
    }

    // 球体
    void makeSphere(const double &_r = 1) {
        BRepPrimAPI_MakeSphere make_sphere(_r);
        s_ = make_sphere;
    }

    // 圆环
    void makeTorus(const double &R1 = 2, const double &R2 = 1, const double &angle = 360) {
        BRepPrimAPI_MakeTorus make_torus(R1, R2, angle * M_PI / 180);
        s_ = make_torus;
    }

    // 楔形（重载1）
    void makeWedge(const double &dx = 1, const double &dy = 1, const double &dz = 1, const double &ltx = 0) {
        BRepPrimAPI_MakeWedge make_wedge(dx, dy, dz, ltx);
        s_ = make_wedge;
    }

    // 楔形（重载2）
    void makeWedge(const double &dx, const double &dy, const double &dz,
                   const double &xmin, const double &zmin, const double &xmax, const double &zmax) {
        BRepPrimAPI_MakeWedge make_wedge(dx, dy, dz, xmin, zmin, xmax, zmax);
        s_ = make_wedge;
    }

    // 顶点
    void makeVertex(const double &x, const double &y, const double &z) {
        BRepBuilderAPI_MakeVertex make_vertex(gp_Pnt(x, y, z));
        s_ = make_vertex;
    }

    // ==================== 线框和多边形 ====================


    // 线框（从参数表）
    void makeWire() {
        BRepBuilderAPI_MakeWire make_wire;

        // 定义x0y平面（Z=0）的4个顶点（矩形，中心在原点，边长2）
        gp_Pnt p1(-1.0, -1.0, 0.0);  // 左下
        gp_Pnt p2(1.0, -1.0, 0.0);   // 右下
        gp_Pnt p3(1.0, 1.0, 0.0);    // 右上
        gp_Pnt p4(-1.0, 1.0, 0.0);   // 左上

        // 生成4条边（x0y平面内，Z=0不变）
        TopoDS_Edge edge1 = BRepBuilderAPI_MakeEdge(p1, p2);  // 下边缘（x从-1到1，y=-1）
        TopoDS_Edge edge2 = BRepBuilderAPI_MakeEdge(p2, p3);  // 右边缘（y从-1到1，x=1）
        TopoDS_Edge edge3 = BRepBuilderAPI_MakeEdge(p3, p4);  // 上边缘（x从1到-1，y=1）
        TopoDS_Edge edge4 = BRepBuilderAPI_MakeEdge(p4, p1);  // 左边缘（y从1到-1，x=-1）

        // 将边添加到Wire构建器
        make_wire.Add(edge1);
        make_wire.Add(edge2);
        make_wire.Add(edge3);
        make_wire.Add(edge4);

        // 构建Wire并赋值给s_
        if (make_wire.IsDone()) {
            s_ = make_wire.Wire();  // 明确指定为Wire类型（更规范）
            std::cout << "生成x0y平面矩形Wire成功（中心在原点）" << std::endl;
        }
        else {
            s_.Nullify();
            std::cerr << "Wire生成失败！" << std::endl;
        }
    }

    // 线框（从边）
    void makeWire(const MakeShapes &edge) {
        if (edge.s_.IsNull() || edge.s_.ShapeType() != TopAbs_EDGE) {
            s_.Nullify();
            return;
        }
        BRepBuilderAPI_MakeWire make_wire(TopoDS::Edge(edge.s_));
        s_ = make_wire;
    }

    // 多边形
    void makePolygon(const std::vector<std::array<double, 3>> _vertices = {}) {
        BRepBuilderAPI_MakePolygon make_polygon;
        for (const auto &p: _vertices) {
            make_polygon.Add(gp_Pnt(p[0], p[1], p[2]));
        }
        if (!make_polygon.Added()) {
            throw std::runtime_error("Polygon Last Vertex NOT Added!");
        }
        make_polygon.Close();
        s_ = make_polygon;
    }

    // ==================== 文本 ====================
    void makeText(const std::string &_text = "", const double &_size = 1) {
        StdPrs_BRepFont aFont;
        const NCollection_String aFontName("Arial");
        if (!aFont.Init(aFontName, Font_FontAspect_Regular, _size)) {
            throw std::runtime_error("Failed init font!");
        }
        Font_BRepTextBuilder aTextBuilder;
        NCollection_String aText(_text.c_str());
        s_ = aTextBuilder.Perform(aFont, aText);
    }

    // ==================== 边 ====================
    // 通用边构造
    void makeEdge(const std::string &_type, const std::array<double, 3> _vec1,
                  const std::array<double, 3> _vec2, const double &_r1 = -1, const double &_r2 = -1) {
        const auto pos = gp_Pnt(_vec1[0], _vec1[1], _vec1[2]);
        const auto dir = gp_Dir(_vec2[0], _vec2[1], _vec2[2]);

        if (_type == "lin") {
            const auto end = gp_Pnt(_vec2[0], _vec2[1], _vec2[2]);
            BRepBuilderAPI_MakeEdge make_edge_lin(pos, end);
            s_ = make_edge_lin;
        } else if (_type == "circ") {
            if (_r1 <= 0) throw std::invalid_argument("Invalid circle radius!");
            gp_Circ circ({pos, dir}, _r1);
            BRepBuilderAPI_MakeEdge make_edge_circ(circ);
            s_ = make_edge_circ;
        } else if (_type == "elips") {
            if (_r1 <= 0 || _r2 <= 0 || _r1 < _r2) {
                throw std::invalid_argument("Invalid ellipse parameters! R1 > R2 > 0");
            }
            gp_Elips elips({pos, dir}, _r1, _r2);
            BRepBuilderAPI_MakeEdge make_edge_elips(elips);
            s_ = make_edge_elips;
        } else if (_type == "hypr") {
            gp_Hypr hypr({pos, dir}, _r1, _r2);
            BRepBuilderAPI_MakeEdge make_edge_hypr(hypr);
            s_ = make_edge_hypr;
        } else if (_type == "parab") {
            gp_Parab parab({pos, dir}, _r1);
            BRepBuilderAPI_MakeEdge make_edge_parab(parab);
            s_ = make_edge_parab;
        } else {
            throw std::runtime_error("Edge: wrong type!");
        }
    }

    // 贝塞尔曲线（无权重）
    void makeBezier(const std::vector<std::array<double, 3>> poles) {
        TColgp_Array1OfPnt CurvePoles(1, static_cast<Standard_Integer>(poles.size()));
        for (int i = 0; i < poles.size(); ++i) {
            CurvePoles.SetValue(i + 1, gp_Pnt(poles[i][0], poles[i][1], poles[i][2]));
        }
        Handle(Geom_BezierCurve) bezier_curve = new Geom_BezierCurve(CurvePoles);
        BRepBuilderAPI_MakeEdge make_edge_bezier(bezier_curve);
        s_ = make_edge_bezier;
    }

    // 贝塞尔曲线（有权重）
    void makeBezier(const std::vector<std::array<double, 3>> poles, const std::vector<double> weights) {
        if (poles.size() != weights.size()) {
            throw std::invalid_argument("Invalid bezier weights!");
        }
        TColgp_Array1OfPnt CurvePoles(1, static_cast<Standard_Integer>(poles.size()));
        for (int i = 0; i < poles.size(); ++i) {
            CurvePoles.SetValue(i + 1, gp_Pnt(poles[i][0], poles[i][1], poles[i][2]));
        }
        TColStd_Array1OfReal CurveWeights(1, static_cast<Standard_Integer>(weights.size()));
        for (int i = 0; i < weights.size(); ++i) {
            CurveWeights.SetValue(i + 1, weights[i]);
        }
        Handle(Geom_BezierCurve) bezier_curve = new Geom_BezierCurve(CurvePoles, CurveWeights);
        BRepBuilderAPI_MakeEdge make_edge_bezier(bezier_curve);
        s_ = make_edge_bezier;
    }

    // B样条曲线
    void makeBSpline(const std::vector<std::array<double, 3>> poles,
                     const std::vector<double> knots,
                     const std::vector<int> multiplicities,
                     const int &degree) {
        if (!(degree > 0 && degree <= Geom_BSplineCurve::MaxDegree())) {
            throw std::invalid_argument("Invalid bspline degree!");
        }
        if (!(knots.size() == multiplicities.size() && knots.size() >= 2)) {
            throw std::invalid_argument("Invalid bspline knots!");
        }
        TColgp_Array1OfPnt CurvePoles(1, static_cast<Standard_Integer>(poles.size()));
        for (int i = 0; i < poles.size(); ++i) {
            CurvePoles.SetValue(i + 1, gp_Pnt(poles[i][0], poles[i][1], poles[i][2]));
        }
        TColStd_Array1OfReal CurveKnots(1, static_cast<Standard_Integer>(knots.size()));
        for (int i = 0; i < knots.size(); ++i) {
            CurveKnots.SetValue(i + 1, knots[i]);
        }
        TColStd_Array1OfInteger CurveMults(1, static_cast<Standard_Integer>(multiplicities.size()));
        for (int i = 0; i < multiplicities.size(); ++i) {
            CurveMults.SetValue(i + 1, multiplicities[i]);
        }
        Handle(Geom_BSplineCurve) bspline_curve = new Geom_BSplineCurve(CurvePoles, CurveKnots, CurveMults, degree);
        BRepBuilderAPI_MakeEdge make_edge_bspline(bspline_curve);
        s_ = make_edge_bspline;
    }

    // 直线
    void makeLine(const std::array<double, 3> p1, const std::array<double, 3> p2) {
        const auto pnt1 = gp_Pnt(p1[0], p1[1], p1[2]);
        const auto pnt2 = gp_Pnt(p2[0], p2[1], p2[2]);
        BRepBuilderAPI_MakeEdge make_edge_lin(pnt1, pnt2);
        s_ = make_edge_lin;
    }

    // 圆
    void makeCircle(const std::array<double, 3> center, const std::array<double, 3> normal, const double &radius) {
        const auto pnt = gp_Pnt(center[0], center[1], center[2]);
        const auto dir = gp_Dir(normal[0], normal[1], normal[2]);
        gp_Circ circ({pnt, dir}, radius);
        BRepBuilderAPI_MakeEdge make_edge_circ(circ);
        s_ = make_edge_circ;
    }

    // 椭圆
    void makeEllipse(const std::array<double, 3> center, const std::array<double, 3> normal,
                     const double &radius1, const double &radius2) {
        if (radius1 <= 0 || radius2 <= 0 || radius1 < radius2) {
            throw std::invalid_argument("Invalid ellipse parameters! R1 > R2 > 0");
        }
        const auto pnt = gp_Pnt(center[0], center[1], center[2]);
        const auto dir = gp_Dir(normal[0], normal[1], normal[2]);
        gp_Elips elips({pnt, dir}, radius1, radius2);
        BRepBuilderAPI_MakeEdge make_edge_elips(elips);
        s_ = make_edge_elips;
    }


    // ==================== 面 ====================
    // 从形状构造面
    void makeFace(const MakeShapes &_shape) {
        const auto shape_type = _shape.s_.ShapeType();
        if (shape_type == TopAbs_WIRE) {
            const auto wire = TopoDS::Wire(_shape.s_);
            BRepBuilderAPI_MakeFace make_face(wire);
            s_ = make_face;
        } else if (shape_type == TopAbs_EDGE) {
            const auto edge = TopoDS::Edge(_shape.s_);
            BRepBuilderAPI_MakeWire make_wire;
            make_wire.Add(edge);
            if (!make_wire.IsDone()) {
                throw std::runtime_error("Failed make wire!");
            }
            BRepBuilderAPI_MakeFace make_face(make_wire);
            s_ = make_face;
        } else {
            throw std::runtime_error("Face: Not Support Shape Type!");
        }
    }

    // 平面
    void makePlane(const std::array<double, 3> pos, const std::array<double, 3> dir, const std::array<double, 4> uv) {
        gp_Pln pln(gp_Pnt(pos[0], pos[1], pos[2]), gp_Dir(dir[0], dir[1], dir[2]));
        BRepBuilderAPI_MakeFace make_face(pln, uv[0], uv[1], uv[2], uv[3]);
        s_ = make_face;
    }

    // 圆柱面
    void makeCylindricalFace(const std::array<double, 3> pos, const std::array<double, 3> dir,
                             const double &r, const std::array<double, 4> uv) {
        gp_Cylinder cylinder(gp_Ax2(gp_Pnt(pos[0], pos[1], pos[2]), gp_Dir(dir[0], dir[1], dir[2])), r);
        BRepBuilderAPI_MakeFace make_face(cylinder, uv[0] * M_PI / 180, uv[1] * M_PI / 180, uv[2], uv[3]);
        s_ = make_face;
    }

    // 圆锥面
    void makeConicalFace(const std::array<double, 3> pos, const std::array<double, 3> dir,
                         const double &angle, const double &r, const std::array<double, 4> uv) {
        const auto rad = angle * M_PI / 180;
        if (std::abs(rad) < gp::Resolution() || std::abs(rad) >= M_PI / 2 - gp::Resolution()) {
            throw std::runtime_error("Conical: The absolute value of the angle must be greater than 0 and less than 90 deg!");
        }
        gp_Cone conical(gp_Ax2(gp_Pnt(pos[0], pos[1], pos[2]), gp_Dir(dir[0], dir[1], dir[2])), rad, r);
        BRepBuilderAPI_MakeFace make_face(conical, uv[0] * M_PI / 180, uv[1] * M_PI / 180, uv[2], uv[3]);
        s_ = make_face;
    }

private:
    // 继承自Shape的成员变量s_用于存储几何形状
};

#endif// MAKE_SHAPES_H