#include <script/compiler/emit/BuildableVisitor.hpp>

namespace hyperion::compiler {

void BuildableVisitor::Visit(Buildable *buildable)
{
    if (auto *node = dynamic_cast<BytecodeChunk*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<LabelMarker*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<Jump*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<Comparison*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<FunctionCall*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<Return*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<StoreLocal*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<PopLocal*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<LoadRef*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<LoadDeref*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<ConstI32*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<ConstI64*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<ConstU32*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<ConstU64*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<ConstF32*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<ConstF64*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<ConstBool*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<ConstNull*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<BuildableTryCatch*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<BuildableFunction*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<BuildableType*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<BuildableString*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<StorageOperation*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<Comment*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<SymbolExport*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<CastOperation*>(buildable)) {
        Visit(node);
    } else if (auto *node = dynamic_cast<RawOperation<>*>(buildable)) {
        Visit(node);
    } else {
        throw "Not implemented";
    }
}

} // namespace hyperion::compiler
