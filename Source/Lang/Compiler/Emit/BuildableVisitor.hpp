#pragma once

#include <Lang/Compiler/Emit/Buildable.hpp>
#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/StorageOperation.hpp>

namespace Hyperion {

class BuildableVisitor
{
public:
    virtual ~BuildableVisitor() = default;

    void Visit(Buildable*);

    virtual void Visit(BytecodeChunk*) = 0;
    virtual void Visit(LabelMarker*) = 0;
    virtual void Visit(Jump*) = 0;
    virtual void Visit(Comparison*) = 0;
    virtual void Visit(FunctionCall*) = 0;
    virtual void Visit(Return*) = 0;
    virtual void Visit(StoreLocal*) = 0;
    virtual void Visit(PopLocal*) = 0;
    virtual void Visit(LoadRef*) = 0;
    virtual void Visit(LoadDeref*) = 0;
    virtual void Visit(ConstI32*) = 0;
    virtual void Visit(ConstI64*) = 0;
    virtual void Visit(ConstU32*) = 0;
    virtual void Visit(ConstU64*) = 0;
    virtual void Visit(ConstF32*) = 0;
    virtual void Visit(ConstF64*) = 0;
    virtual void Visit(ConstBool*) = 0;
    virtual void Visit(ConstNull*) = 0;
    virtual void Visit(LoadClass*) = 0;
    virtual void Visit(TryCatchInfo*) = 0;
    virtual void Visit(ScriptFunction*) = 0;
    virtual void Visit(ClassTable*) = 0;
    virtual void Visit(ConstString*) = 0;
    virtual void Visit(StorageOperation*) = 0;
    virtual void Visit(Comment*) = 0;
    virtual void Visit(SymbolExport*) = 0;
    virtual void Visit(CastOperation*) = 0;
    virtual void Visit(RawOperation<>*) = 0;
};

} // namespace Hyperion
