#ifndef DisplayScene_h
#define DisplayScene_h

#include <Standard_Transient.hxx>
#include <TDocStd_Document.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <NCollection_DataMap.hxx>
#include <NCollection_List.hxx>
#include <NCollection_DefaultHasher.hxx> // 恢复这个
#include <TDF_Label.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFPrs_Style.hxx>

// 使用智能指针别名，代码更简洁
typedef NCollection_List<Handle(AIS_InteractiveObject)> AisList;

// 恢复原来的 Hasher 定义
typedef NCollection_DataMap<TDF_Label, AisList, NCollection_DefaultHasher<TDF_Label>> LabelPrsMap;

class DisplayScene : public Standard_Transient
{
    DEFINE_STANDARD_RTTI_INLINE(DisplayScene, Standard_Transient)

public:
    DisplayScene(const Handle(TDocStd_Document)& doc,
        const Handle(AIS_InteractiveContext)& ctx)
        : m_doc(doc), m_ctx(ctx) {}

    virtual bool Execute();

    TopoDS_Shape GetLastShape() const { return m_shape; }

protected:

    // 递归入口：处理层级、样式传递
    void processLabel(const TDF_Label& label,
        const TopLoc_Location& parentTrsf,
        const XCAFPrs_Style& parentStyle,
        const TCollection_AsciiString& parentId,
        LabelPrsMap& mapOfOriginals);

    // 叶子节点处理：显示具体的几何
    void displayComponent(const TDF_Label& refLabel,
        const TopLoc_Location& location,
        const TCollection_AsciiString& itemId,
        LabelPrsMap& mapOfOriginals);

    // 辅助：解析当前节点的样式
    XCAFPrs_Style resolveStyle(const TDF_Label& label,
        const XCAFPrs_Style& parentStyle);

protected:
    Handle(TDocStd_Document)       m_doc;
    Handle(AIS_InteractiveContext) m_ctx;
    TopoDS_Shape                   m_shape;
};

#endif