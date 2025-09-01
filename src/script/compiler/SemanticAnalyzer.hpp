#pragma once

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/Enums.hpp>
#include <script/compiler/ast/AstArgument.hpp>

#include <core/utilities/Optional.hpp>

#include <memory>
#include <utility>

namespace hyperion::compiler {

// forward declaration
class Module;

struct SubstitutionResult
{
    Variant<SymbolTypeRef, RC<AstArgument>> value;
    SizeType index = SizeType(-1);
};

struct ArgInfo
{
    bool isNamed;
    String name;
    SymbolTypeRef type;
};

class SemanticAnalyzer : public AstVisitor
{
public:
    struct Helpers
    {
    private:
        static SizeType FindFreeSlot(
            SizeType currentIndex,
            const FlatSet<SizeType>& usedIndices,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            bool isVariadic,
            SizeType numSuppliedArgs);

        static SizeType ArgIndex(
            SizeType currentIndex,
            const ArgInfo& argInfo,
            const FlatSet<SizeType>& usedIndices,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            bool isVariadic = false,
            SizeType numSuppliedArgs = -1);

    public:
        static SymbolTypeRef GetVarArgType(
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs);

        static SymbolTypeRef SubstituteGenericParameters(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypeRef& inputType,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            const SourceLocation& location);

        static SymbolTypeRef ResolvePlaceholderType(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypeRef& inputType,
            const SourceLocation& location);

        static void CheckArgTypeCompatible(
            AstVisitor* visitor,
            Module* mod,
            const SourceLocation& location,
            const SymbolTypeRef& argType,
            const SymbolTypeRef& paramType);

        static bool SubstituteFunctionArgs(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypeRef& symbolType,
            const Array<RC<AstArgument>>& args,
            const SourceLocation& location,
            SymbolTypeRef& outReturnType,
            Array<RC<AstArgument>>& outArgs);

        static void EnsureFunctionArgCompatibility(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypeRef& symbolType,
            const Array<RC<AstArgument>>& args,
            const SourceLocation& location);

        static void EnsureLooseTypeAssignmentCompatibility(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypeRef& symbolType,
            const SymbolTypeRef& assignmentType,
            const SourceLocation& location);

        static void EnsureTypeAssignmentCompatibility(
            AstVisitor* visitor,
            Module* mod,
            const SymbolTypeRef& symbolType,
            const SymbolTypeRef& assignmentType,
            const SourceLocation& location);
    };

public:
    SemanticAnalyzer(AstIterator* astIterator, CompilationUnit* compilationUnit);
    SemanticAnalyzer(const SemanticAnalyzer& other);

    /** Generates the compilation unit structure from the given statement iterator */
    void Analyze(bool expectModuleDecl = true);
};

} // namespace hyperion::compiler
