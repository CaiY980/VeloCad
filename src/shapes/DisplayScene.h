#ifndef DisplayScene_h
#define DisplayScene_h

#include <Standard_Transient.hxx>
#include <TDocStd_Document.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <NCollection_DataMap.hxx>
#include <NCollection_List.hxx>
#include <NCollection_DefaultHasher.hxx> 
#include <TDF_Label.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFPrs_Style.hxx>

// 使用智能指针别名
typedef NCollection_List<Handle(AIS_InteractiveObject)> AisList;

typedef NCollection_DataMap<TDF_Label, AisList, NCollection_DefaultHasher<TDF_Label>> LabelPrsMap;

// 读取 TDocStd_Document（XCAF 格式），遍历其装配体结构（Assembly Structure），并将几何体显示在 AIS_InteractiveContext 中
class DisplayScene : public Standard_Transient
{
    DEFINE_STANDARD_RTTI_INLINE(DisplayScene, Standard_Transient)

public:
    DisplayScene(const Handle(TDocStd_Document)& doc,const Handle(AIS_InteractiveContext)& ctx): m_doc(doc), m_ctx(ctx) {}
    // 触发整个加载流程
    virtual bool Execute();

    TopoDS_Shape GetLastShape() const { return m_shape; }

protected:

    // 负责处理父子层级关系、坐标变换矩阵累加、样式传递
    void processLabel(const TDF_Label& label,
        const TopLoc_Location& parentTrsf,
        const XCAFPrs_Style& parentStyle,
        const TCollection_AsciiString& parentId,
        LabelPrsMap& mapOfOriginals);

    // 叶子节点处理：建具体的 AIS 对象并添加到场景中
    void displayComponent(const TDF_Label& refLabel,
        const TopLoc_Location& location,
        const TCollection_AsciiString& itemId,
        LabelPrsMap& mapOfOriginals);

    // 辅助：用于查找 XCAF 文档中的颜色属性
    XCAFPrs_Style resolveStyle(const TDF_Label& label,
        const XCAFPrs_Style& parentStyle);

protected:
    Handle(TDocStd_Document)       m_doc;
    Handle(AIS_InteractiveContext) m_ctx;
    TopoDS_Shape                   m_shape;
};

#endif