#include "DisplayScene.h"


#include <AIS_ConnectedInteractive.hxx>
#include <AIS_ColoredShape.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDF_Tool.hxx>
#include <TopoDS_Iterator.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFPrs_AISObject.hxx>


namespace
{
    bool IsEmptyShape(const TopoDS_Shape& shape)
    {
        if (shape.IsNull()) return true;
        if (shape.ShapeType() >= TopAbs_FACE) return false;

        TopoDS_Iterator it(shape);
        return !it.More();
    }
}

//-----------------------------------------------------------------------------

bool DisplayScene::Execute()
{
    if (m_doc.IsNull() || m_ctx.IsNull()) return false;
    // 操作 XCAF 文档的入口
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(m_doc->Main());
    TDF_LabelSequence roots;
    // 获取装配体顶层节点
    shapeTool->GetFreeShapes(roots);

    // 默认样式
    XCAFPrs_Style defaultStyle;
    defaultStyle.SetColorSurf(Quantity_NOC_GREEN);
    defaultStyle.SetColorCurv(Quantity_Color(0.0, 0.4, 0.0, Quantity_TOC_sRGB));

    LabelPrsMap mapOfOriginals;

    for (TDF_LabelSequence::Iterator lit(roots); lit.More(); lit.Next())
    {
        // 当前的 OCAF 标签，代表数据结构中的一个节点
        const TDF_Label& label = lit.Value();
        // 获取该根节点自身的变换矩阵（位置/旋转）
        TopLoc_Location parentLoc = shapeTool->GetLocation(label);

        try
        {
            this->processLabel(label, parentLoc, defaultStyle, "", mapOfOriginals);
        }
        catch (const Standard_Failure& e)
        {
            std::cout << "Error displaying label: " << e.GetMessageString() << std::endl;
        }
    }

    // 刷新 Viewer
    m_ctx->UpdateCurrentViewer();
    return true;
}

//-----------------------------------------------------------------------------
// 核心递归逻辑
//-----------------------------------------------------------------------------
void DisplayScene::processLabel(const TDF_Label& label,
    const TopLoc_Location& parentTrsf,
    const XCAFPrs_Style& parentStyle,
    const TCollection_AsciiString& parentId,
    LabelPrsMap& mapOfOriginals)
{
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(m_doc->Main());

    // 1. 处理引用 (Reference)
    /*
    label 是当前遍历到的树节点（可能只是一个“指针”）。
    refLabel 是最终存放几何数据的真实节点。
    */
    TDF_Label refLabel = label;
    if (shapeTool->IsReference(label))
    {
        shapeTool->GetReferredShape(label, refLabel);
    }

    // 2. 构建 ID (Path)
    TCollection_AsciiString itemId;
    TDF_Tool::Entry(label, itemId);
    if (!parentId.IsEmpty())
    {
        itemId.Prepend("/");
        itemId.Prepend(parentId);
    }

    // 3. 如果不是装配体（即它是最底层的零件，是 Component/Solid），则进行显示
    if (!shapeTool->IsAssembly(refLabel))
    {
        displayComponent(refLabel, parentTrsf, itemId, mapOfOriginals);
        return; // 叶子节点，结束递归
    }

    // 4. 如果是装配体，解析样式并递归
    // 父级装配体可能定义了颜色。子零件如果没有定义自己的颜色，通常应该继承父级的颜色
    XCAFPrs_Style currentStyle = resolveStyle(refLabel, parentStyle);

    // refLabel 是已经解析过引用的真实节点，确保我们遍历的是真实的几何结构定义，而不是一个仅仅指向别处的“空壳”
    for (TDF_ChildIterator childIt(refLabel); childIt.More(); childIt.Next())
    {
        TDF_Label childLabel = childIt.Value();
        if (!childLabel.IsNull() && (childLabel.HasAttribute() || childLabel.HasChild()))
        {
            // 累加变换矩阵
            TopLoc_Location trsf = parentTrsf * shapeTool->GetLocation(childLabel);
            this->processLabel(childLabel, trsf, currentStyle, itemId, mapOfOriginals);
        }
    }
}

//-----------------------------------------------------------------------------
// 显示逻辑
//-----------------------------------------------------------------------------
void DisplayScene::displayComponent(const TDF_Label& refLabel,
    const TopLoc_Location& location,
    const TCollection_AsciiString& itemId,
    LabelPrsMap& mapOfOriginals)
{
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(m_doc->Main());

    // 获取几何
    TopoDS_Shape shape = shapeTool->GetShape(refLabel);
    if (::IsEmptyShape(shape)) return;

    m_shape = shape;

    // 检查缓存
    AisList* aisListPtr = mapOfOriginals.ChangeSeek(refLabel);

    if (aisListPtr == nullptr)
    {
        // 创建原始对象，XCAFPrs_AISObject 会自动处理几何和部分属性
        Handle(XCAFPrs_AISObject) originalPrs = new XCAFPrs_AISObject(refLabel);

        AisList newList;
        newList.Append(originalPrs);
        // 存入 Map
        aisListPtr = mapOfOriginals.Bound(refLabel, newList);
    }

    // 创建 Connected Interactive 引用缓存的原始对象
    for (AisList::Iterator it(*aisListPtr); it.More(); it.Next())
    {
        const Handle(AIS_InteractiveObject)& original = it.Value();
        Handle(AIS_ConnectedInteractive) connectedPrs = new AIS_ConnectedInteractive();

        connectedPrs->Connect(original);
        connectedPrs->SetLocalTransformation(location.Transformation());
        connectedPrs->SetDisplayMode(AIS_Shaded);

        try
        {
            // false = 不立即重绘，提高加载速度
            m_ctx->Display(connectedPrs, false);
        }
        catch (...)
        {
            m_ctx->Remove(connectedPrs, false);
        }
    }
}

//-----------------------------------------------------------------------------
// 样式逻辑
//-----------------------------------------------------------------------------
XCAFPrs_Style DisplayScene::resolveStyle(const TDF_Label& label,
    const XCAFPrs_Style& parentStyle)
{
    Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(m_doc->Main());
    XCAFPrs_Style style = parentStyle;
    Quantity_ColorRGBA color;

    if (colorTool->GetColor(label, XCAFDoc_ColorGen, color))
    {
        style.SetColorCurv(color.GetRGB());
        style.SetColorSurf(color);
    }
    if (colorTool->GetColor(label, XCAFDoc_ColorSurf, color))
    {
        style.SetColorSurf(color);
    }
    if (colorTool->GetColor(label, XCAFDoc_ColorCurv, color))
    {
        style.SetColorCurv(color.GetRGB());
    }

    return style;
}