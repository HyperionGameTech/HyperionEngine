#ifndef BUILDABLE_VISITOR_HPP
#define BUILDABLE_VISITOR_HPP

#include <script/compiler/emit/Buildable.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

namespace hyperion::compiler {

class BuildableVisitor
{
public:
    virtual ~BuildableVisitor() = default;

    void Visit(Buildable *);

    virtual void Visit(BytecodeChunk *) = 0;
    virtual void Visit(LabelMarker *) = 0;
    virtual void Visit(Jump *) = 0;
    virtual void Visit(Comparison *) = 0;
    virtual void Visit(FunctionCall *) = 0;
    virtual void Visit(Return *) = 0;
    virtual void Visit(StoreLocal *) = 0;
    virtual void Visit(PopLocal *) = 0;
    virtual void Visit(LoadRef *) = 0;
    virtual void Visit(LoadDeref *) = 0;
    virtual void Visit(ConstI32 *) = 0;
    virtual void Visit(ConstI64 *) = 0;
    virtual void Visit(ConstU32 *) = 0;
    virtual void Visit(ConstU64 *) = 0;
    virtual void Visit(ConstF32 *) = 0;
    virtual void Visit(ConstF64 *) = 0;
    virtual void Visit(ConstBool *) = 0;
    virtual void Visit(ConstNull *) = 0;
    virtual void Visit(BuildableTryCatch *) = 0;
    virtual void Visit(BuildableFunction *) = 0;
    virtual void Visit(BuildableType *) = 0;
    virtual void Visit(BuildableString *) = 0;
    virtual void Visit(StorageOperation *) = 0;
    virtual void Visit(Comment *) = 0;
    virtual void Visit(SymbolExport *) = 0;
    virtual void Visit(CastOperation *) = 0;
    virtual void Visit(RawOperation<> *) = 0;
};

} // namespace hyperion::compiler

#endif
