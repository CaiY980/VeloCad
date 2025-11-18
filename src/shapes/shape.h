
#ifndef SHAPE_H
#define SHAPE_H
#include <TDocStd_Document.hxx>
#include <AIS_Shape.hxx>
#include <TopoDS_Edge.hxx>
#include <BRepBuilderAPI_Transform.hxx> 
class TopoShape : public TopoDS_Shape {
public:

    TopoShape() {}


    TopoShape(const TopoDS_Shape &shape) : TopoDS_Shape(shape) {}

    TopoShape(const TopoShape &other) : TopoDS_Shape(other) {}

    TopoShape &operator=(const TopoDS_Shape &shape) {
        if (this != &shape) {             
            TopoDS_Shape::operator=(shape);
        }
        return *this;
    }

    TopoShape &operator=(const TopoShape &other) {
        if (this != &other) {              
            TopoDS_Shape::operator=(other);
        }
        return *this;
    }

  
};


class Shape {
public:

    TopoShape data() const { return s_; }

    TopoShape s_;
    
    Quantity_Color color_{Quantity_NOC_ORANGE3};//!< 形状颜色，默认为橙色
    Standard_Real transparency_{0};             //!< 透明度，0为不透明，1为完全透明
    Standard_Real density_{1.0};                //!< 密度，用于计算质量，质量=体积*密度
                      
public:

    explicit Shape() = default;




    ~Shape() = default;

   

    
    Handle(TDocStd_Document) read_xde_document(const std::string &filename);
 

    Shape &export_step(const std::string &_filename);

    Shape &export_xde(const std::string &_filename);

    std::array<double, 4> rgba() const;

    Shape &open_stl(std::string _filename);
    Shape make_compound(const std::vector<Shape> &_shapes);

private:
    
   

};

#endif//SHAPE_H
