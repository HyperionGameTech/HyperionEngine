#include <Lang/Compiler/Emit/BuildableVisitor.hpp>

namespace Hyperion {

#define HYP_VISIT_CASE(Type) \
    case BuildableType::Type: \
        Visit(static_cast<Type*>(buildable)); \
        break;

#define HYP_VISIT_CASE_TYPE(EnumName, Type) \
    case BuildableType::EnumName: \
        Visit(static_cast<Type*>(buildable)); \
        break;

void BuildableVisitor::Visit(Buildable* buildable)
{
    switch (buildable->GetBuildableType())
    {
    HYP_VISIT_CASE(BytecodeChunk)
    HYP_VISIT_CASE(LabelMarker)
    HYP_VISIT_CASE(Jump)
    HYP_VISIT_CASE(Comparison)
    HYP_VISIT_CASE(FunctionCall)
    HYP_VISIT_CASE(Return)
    HYP_VISIT_CASE(StoreLocal)
    HYP_VISIT_CASE(PopLocal)
    HYP_VISIT_CASE(LoadRef)
    HYP_VISIT_CASE(LoadDeref)
    HYP_VISIT_CASE(ConstI32)
    HYP_VISIT_CASE(ConstI64)
    HYP_VISIT_CASE(ConstU32)
    HYP_VISIT_CASE(ConstU64)
    HYP_VISIT_CASE(ConstF32)
    HYP_VISIT_CASE(ConstF64)
    HYP_VISIT_CASE(ConstBool)
    HYP_VISIT_CASE(ConstNull)
    HYP_VISIT_CASE(LoadClass)
    HYP_VISIT_CASE(TryCatchInfo)
    HYP_VISIT_CASE(ScriptFunction)
    HYP_VISIT_CASE(ClassTable)
    HYP_VISIT_CASE(ConstString)
    HYP_VISIT_CASE(StorageOperation)
    HYP_VISIT_CASE(Comment)
    HYP_VISIT_CASE(SymbolExport)
    HYP_VISIT_CASE(CastOperation)
    HYP_VISIT_CASE(IsInstanceComp)
    HYP_VISIT_CASE_TYPE(RawOperation, RawOperation<>)

    default:
        HYP_NOT_IMPLEMENTED();
    }
}

#undef HYP_VISIT_CASE
#undef HYP_VISIT_CASE_TYPE

} // namespace Hyperion
