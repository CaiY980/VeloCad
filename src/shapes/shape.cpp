
#include "shape.h"
#include <BRep_Builder.hxx>
#include <StlAPI_Reader.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Quaternion.hxx>
#include <STEPControl_Writer.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TDocStd_Application.hxx>
#include <BinXCAFDrivers.hxx>
#include<OSD_File.hxx>
#include <OSD_Path.hxx>
#include <TCollection_ExtendedString.hxx> 
#include <XCAFDoc_DocumentTool.hxx>
#include <TDF_Label.hxx>
#include <XCAFDoc_ShapeTool.hxx>

Shape& Shape::open_stl( std::string _filename) {
    if (_filename.empty()) { throw std::runtime_error("Filename is empty!"); }
    const auto ends_with = [&_filename](const std::string &suffix) {
        if (suffix.length() > _filename.length()) { return false; }
        return (_filename.rfind(suffix) == (_filename.length() - suffix.length()));
    };
    if (ends_with(".step") || ends_with(".STEP")) {
        STEPControl_Reader reader;
        const auto &res = reader.ReadFile(_filename.c_str());// 加载文件只是记忆数据，不转换
        if (res != IFSelect_RetDone) { throw std::runtime_error("Failed Import STEP file!"); }
        reader.PrintCheckLoad(Standard_False, IFSelect_ItemsByEntity);// 检查加载的文件(不是强制性)
        //加载step文件
        Standard_Integer NbRoots = reader.NbRootsForTransfer();
        Standard_Integer num = reader.TransferRoots();
        s_ = reader.OneShape();
        return *this;
    } else if (ends_with(".stl") || ends_with(".STL")) {
        TopoDS_Shape topology_shape;
        StlAPI_Reader reader;
        if (!reader.Read(topology_shape, _filename.c_str())) { throw std::runtime_error("Failed Import STL file!"); }
        s_ = topology_shape;
        return *this;
    } else {
        throw std::runtime_error("Not support file!");
    }
   
}

Handle(TDocStd_Document) Shape::read_xde_document(const std::string &filename) {
    // 1. 基础校验：文件是否存在、是否可读
    OSD_Path xde_path(filename.c_str());
    OSD_File xde_file(xde_path);
    if (!xde_file.Exists()) {
        std::cout << "[ERROR] XDE file not found: " << filename << std::endl;
        return nullptr;
    }
    // 2. 初始化 XDE 核心应用
    Handle(TDocStd_Application) app = new TDocStd_Application;


    // 3. 注册 BinXCAF 驱动（关键：让应用识别 XDE 格式）
    BinXCAFDrivers::DefineFormat(app);

    // 4. 打开 XDE 文档（带异常捕获）
    Handle(TDocStd_Document) xde_doc;
    TCollection_ExtendedString occt_path(filename.c_str());
        // 打开 BinXCAF 格式文档
    app->Open(occt_path, xde_doc);
    // 成功读取
    std::cout << "[INFO] Successfully read XDE document: " << filename << std::endl;
    return xde_doc;
}




Shape &Shape::export_step(const std::string &_filename) {
    STEPControl_Writer writer;
    writer.Transfer(s_, STEPControl_AsIs);
    if (!writer.Write(_filename.c_str())) { throw std::runtime_error("Failed to export STEP!"); }
    return *this;
}
#include <XCAFDoc_ColorTool.hxx>

Shape &Shape::export_xde(const std::string &_filename) {

    // 1. 校验形状有效性
    if (s_.IsNull()) {
        throw std::runtime_error("Export XBF failed: TopoDS_Shape is null!");
    }

    // 2. 校验输出路径
    OSD_Path output_path(_filename.c_str());
    OSD_File output_file(output_path);


    // 3. 创建 XDE 应用和文档
    Handle(TDocStd_Application) app = new TDocStd_Application;

    BinXCAFDrivers::DefineFormat(app);

    Handle(TDocStd_Document) xde_doc;
    app->NewDocument("BinXCAF", xde_doc);

    try {
        Standard_ErrorHandler eh;

        // 4. 添加形状到 XDE 文档
        TDF_Label main_label = xde_doc->Main();
        Handle(XCAFDoc_ShapeTool) shape_tool = XCAFDoc_DocumentTool::ShapeTool(main_label);
        if (shape_tool.IsNull()) {
            throw std::runtime_error("Export XBF failed: Cannot get XCAFDoc_ShapeTool!");
        }

        TDF_Label shape_label = shape_tool->AddShape(
                s_,
                Standard_False,
                Standard_True);

        // 5. 绑定颜色到形状（核心：颜色导出关键步骤）

        Handle(XCAFDoc_ColorTool) color_tool = XCAFDoc_DocumentTool::ColorTool(main_label);
        if (color_tool.IsNull()) {
            throw std::runtime_error("Export XBF failed: Cannot get XCAFDoc_ColorTool!");
        }

        // 将颜色绑定到形状标签（表面颜色，最常用）
        // 第二个参数：XCAFDoc_ColorSurf = 表面颜色；XCAFDoc_ColorCurv = 曲线颜色
        color_tool->SetColor(shape_label, color_, XCAFDoc_ColorSurf);

        // 可选：验证颜色是否绑定成功
        Quantity_Color check_color;
        if (!color_tool->GetColor(shape_label, XCAFDoc_ColorSurf, check_color)) {
            throw std::runtime_error("Export XBF failed: Failed to bind color to shape!");
        }


        // 6. 保存文档（含颜色信息）
        TCollection_ExtendedString occt_filename(_filename.c_str());
        app->SaveAs(
                xde_doc,
                occt_filename,
                Message_ProgressRange());


        // 7. 关闭文档释放资源
        app->Close(xde_doc);
        std::cout << "[INFO] Export XBF with color success: " << _filename << std::endl;

    } catch (const Standard_Failure &e) {
        app->Close(xde_doc);
        throw std::runtime_error("Export XBF failed: " + std::string(e.GetMessageString()) + " - " + _filename);
    } catch (...) {
        app->Close(xde_doc);
        throw std::runtime_error("Export XBF failed: Unknown error - " + _filename);
    }

    return *this;
}




Shape Shape::make_compound(const std::vector<Shape> &_shapes) {
    if (_shapes.empty()) { return Shape(); }
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    bool is_all_null = true;
    for (const auto &shape: _shapes) {
        if (!shape.s_.IsNull()) {
            builder.Add(compound, shape.s_);
            is_all_null = false;
        }
    }
    if (is_all_null) { return Shape(); }
    Shape compound_shape = _shapes[0];
    compound_shape.s_ = compound;
    return compound_shape;
}
