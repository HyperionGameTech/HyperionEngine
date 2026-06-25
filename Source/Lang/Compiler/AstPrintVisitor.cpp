#include <Lang/Compiler/AstPrintVisitor.hpp>
#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBinaryExpression.hpp>
#include <Lang/Compiler/Ast/AstUnaryExpression.hpp>
#include <Lang/Compiler/Ast/AstCallExpression.hpp>
#include <Lang/Compiler/Ast/AstMemberCallExpression.hpp>
#include <Lang/Compiler/Ast/AstIdentifier.hpp>
#include <Lang/Compiler/Ast/AstConstant.hpp>
#include <Lang/Compiler/Ast/AstInteger.hpp>
#include <Lang/Compiler/Ast/AstFloat.hpp>
#include <Lang/Compiler/Ast/AstString.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>
#include <Lang/Compiler/Ast/AstIfStatement.hpp>
#include <Lang/Compiler/Ast/AstForLoop.hpp>
#include <Lang/Compiler/Ast/AstForEachLoop.hpp>
#include <Lang/Compiler/Ast/AstWhileLoop.hpp>
#include <Lang/Compiler/Ast/AstFunctionExpression.hpp>
#include <Lang/Compiler/Ast/AstClass.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstReturnStatement.hpp>
#include <Lang/Compiler/Ast/AstMember.hpp>
#include <Lang/Compiler/Ast/AstArrayExpression.hpp>
#include <Lang/Compiler/Ast/AstArrayAccess.hpp>
#include <Lang/Compiler/Ast/AstHashMap.hpp>
#include <Lang/Compiler/Ast/AstDirective.hpp>
#include <Lang/Compiler/Ast/AstImport.hpp>
#include <Lang/Compiler/Ast/AstModuleDeclaration.hpp>
#include <Lang/Compiler/Ast/AstBreakStatement.hpp>
#include <Lang/Compiler/Ast/AstContinueStatement.hpp>
#include <Lang/Compiler/Ast/AstTernaryExpression.hpp>

namespace Hyperion {

static const WideString s_colorReset = L"\033[0m";
static const WideString s_colorNodeName = L"\033[1;96m";
static const WideString s_colorValue = L"\033[1;92m";
static const WideString s_colorType = L"\033[1;95m";
static const WideString s_colorLocation = L"\033[38;5;240m";
static const WideString s_colorKeyword = L"\033[1;94m";
static const WideString s_colorComment = L"\033[38;5;245m";

AstPrintVisitor::AstPrintVisitor(
    AstIterator* astIterator,
    CompilationUnit* compilationUnit,
    size_t indentSize)
    : AstVisitor(astIterator, compilationUnit),
      m_indentSize(indentSize),
      m_showTypes(true),
      m_showLocations(false),
      m_useColors(true),
      m_showDetails(false)
{
}

WideString AstPrintVisitor::PrintNode(AstStatement* node, size_t depth) const
{
    if (!node)
    {
        return GetIndentation(depth) + Colorize(L"├─ <null>", s_colorComment) + L"\n";
    }

    WideString result;
    const WideString indentation = GetIndentation(depth);

    WideString connector = depth == 0 ? L"┌─ " : L"├─ ";

    // Start with node name
    WideString nodeName = GetNodeDescription(node);
    result += indentation + Colorize(connector, s_colorComment) + FormatNodeName(nodeName);

    WideString nodeInfo = GetBasicNodeInfo(node);

    if (!nodeInfo.Empty())
    {
        result += L" " + nodeInfo;
    }

    // Add type information if enabled
    if (m_showTypes)
    {
        WideString typeInfo = GetTypeInfo(node);
        if (!typeInfo.Empty())
        {
            result += L" " + FormatType(typeInfo);
        }
    }

    // Add location information if enabled
    if (m_showLocations)
    {
        result += L" " + FormatLocation(node->GetLocation());
    }

    result += L"\n";

    // Add detailed information if enabled
    if (m_showDetails)
    {
        WideString details = GetDetailedInfo(node, depth);
        if (!details.Empty())
        {
            result += details;
        }
    }

    result += PrintChildren(node, depth);

    return result;
}

WideString AstPrintVisitor::PrintTree(AstStatement* root) const
{
    if (!root)
    {
        return Colorize(L"(empty tree)", s_colorComment) + L"\n";
    }

    WideString result;

    result += PrintNode(root, 0);
    result += L"\n" + Colorize(L"└─ " + Colorize(L"End of tree", s_colorComment), s_colorComment) + L"\n";

    return result;
}

WideString AstPrintVisitor::GetIndentation(size_t depth) const
{
    WideString result;

    for (size_t i = 0; i < depth * m_indentSize; ++i)
    {
        result += L" ";
    }

    return result;
}

WideString AstPrintVisitor::FormatNodeName(const WideString& nodeName) const
{
    return Colorize(nodeName, s_colorNodeName);
}

WideString AstPrintVisitor::FormatValue(const WideString& value) const
{
    return Colorize(value, s_colorValue);
}

WideString AstPrintVisitor::FormatType(const WideString& type) const
{
    return Colorize(L"[" + type + L"]", s_colorType);
}

WideString AstPrintVisitor::FormatLocation(const SourceLocation& location) const
{
    return Colorize(L"@" + WideString::ToString(location.GetLine()) + L":" + WideString::ToString(location.GetColumn()), s_colorLocation);
}

WideString AstPrintVisitor::Colorize(const WideString& text, const WideString& colorCode) const
{
    if (!m_useColors)
    {
        return text;
    }
    return colorCode + text + s_colorReset;
}

WideString AstPrintVisitor::GetNodeDescription(AstStatement* node) const
{
    if ((node)->IsA<AstBinaryExpression>())
    {
        return L"BinaryExpression";
    }

    if ((node)->IsA<AstUnaryExpression>())
    {
        return L"UnaryExpression";
    }

    if ((node)->IsA<AstCallExpression>())
    {
        return L"CallExpression";
    }

    if ((node)->IsA<AstMemberCallExpression>())
    {
        return L"MemberCallExpression";
    }

    if ((node)->IsA<AstIdentifier>())
    {
        return L"Identifier";
    }

    if ((node)->IsA<AstInteger>())
    {
        return L"Integer";
    }

    if ((node)->IsA<AstFloat>())
    {
        return L"Float";
    }

    if ((node)->IsA<AstString>())
    {
        return L"WideString";
    }

    if ((node)->IsA<AstBlock>())
    {
        return L"Block";
    }

    if ((node)->IsA<AstIfStatement>())
    {
        return L"IfStatement";
    }

    if ((node)->IsA<AstForLoop>())
    {
        return L"ForLoop";
    }

    if ((node)->IsA<AstForEachLoop>())
    {
        return L"ForEachLoop";
    }

    if ((node)->IsA<AstWhileLoop>())
    {
        return L"WhileLoop";
    }

    if ((node)->IsA<AstFunctionExpression>())
    {
        return L"FunctionExpression";
    }

    if ((node)->IsA<AstClass>())
    {
        return L"Class";
    }

    if ((node)->IsA<AstVariableDeclaration>())
    {
        return L"VariableDeclaration";
    }

    if ((node)->IsA<AstReturnStatement>())
    {
        return L"ReturnStatement";
    }

    if ((node)->IsA<AstMember>())
    {
        return L"Member";
    }

    if ((node)->IsA<AstArrayExpression>())
    {
        return L"ArrayExpression";
    }

    if ((node)->IsA<AstArrayAccess>())
    {
        return L"ArrayAccess";
    }

    if ((node)->IsA<AstHashMap>())
    {
        return L"TMap";
    }

    if ((node)->IsA<AstDirective>())
    {
        return L"Directive";
    }

    if ((node)->IsA<AstImport>())
    {
        return L"Import";
    }

    if ((node)->IsA<AstModuleDeclaration>())
    {
        return L"ModuleDeclaration";
    }

    if ((node)->IsA<AstBreakStatement>())
    {
        return L"BreakStatement";
    }

    if ((node)->IsA<AstContinueStatement>())
    {
        return L"ContinueStatement";
    }

    if ((node)->IsA<AstTernaryExpression>())
    {
        return L"TernaryExpression";
    }

    return L"AstNode";
}

WideString AstPrintVisitor::GetBasicNodeInfo(AstStatement* node) const
{
    WideString result;

    if (auto* identifier = DynamicCast<AstIdentifier>(node))
    {
        result += FormatValue(L"'" + identifier->GetName().ToWide() + L"'");
    }
    else if (auto* integer = DynamicCast<AstInteger>(node))
    {
        result += FormatValue(integer->ToString().ToWide());
    }
    else if (auto* floatNode = DynamicCast<AstFloat>(node))
    {
        result += FormatValue(floatNode->ToString().ToWide());
    }
    else if (auto* stringNode = DynamicCast<AstString>(node))
    {
        result += FormatValue(stringNode->ToString().ToWide());
    }
    else if (auto* varDecl = DynamicCast<AstVariableDeclaration>(node))
    {
        result += FormatValue(L"'" + varDecl->GetName().ToWide() + L"'");
    }
    else
    {
        // For other nodes, try to get a basic string representation
        WideString nodeString = node->ToString().ToWide();
        if (!nodeString.Empty() && nodeString.Size() < 100)
        {
            result += FormatValue(L"(" + nodeString + L")");
        }
    }

    return result;
}

WideString AstPrintVisitor::GetTypeInfo(AstStatement* node) const
{
    if (AstExpression* expr = DynamicCast<AstExpression>(node))
    {
        const SymbolType* symbolType = expr->GetExprType();

        if (symbolType)
        {
            return symbolType->ToString(true).ToWide();
        }
    }

    return WideString::empty;
}

WideString AstPrintVisitor::GetDetailedInfo(AstStatement* node, size_t depth) const
{
    WideString result;
    const WideString indentation = GetIndentation(depth + 1);

    if (auto* callExpr = DynamicCast<AstCallExpression>(node))
    {
        result += indentation
            + Colorize(L"│ ", s_colorComment)
            + Colorize(L"Arguments: ", s_colorComment)
            + FormatValue(WideString::ToString(callExpr->GetArguments().Size())) + L"\n";
    }
    else if (auto* block = DynamicCast<AstBlock>(node))
    {
        result += indentation
            + Colorize(L"│ ", s_colorComment)
            + Colorize(L"Statements: ", s_colorComment)
            + FormatValue(WideString::ToString(block->GetChildren().Size())) + L"\n";
    }
    else if (auto* varDecl = DynamicCast<AstVariableDeclaration>(node))
    {
        result += indentation
            + Colorize(L"│ ", s_colorComment)
            + Colorize(L"Variable: ", s_colorComment)
            + FormatValue(L"'" + varDecl->GetName().ToWide() + L"'") + L"\n";
    }

    return result;
}

WideString AstPrintVisitor::PrintChildren(AstStatement* node, size_t depth) const
{
    WideString result;
    Array<AstStatement*> children = GetChildNodes(node);

    for (size_t i = 0; i < children.Size(); ++i)
    {
        AstStatement* child = children[i];
        const bool isLast = (i == children.Size() - 1);

        if (child)
        {
            WideString childResult = PrintNode(child, depth + 1);

            if (isLast && depth > 0)
            {
                size_t pos = childResult.FindFirstIndex(L"├─");

                if (pos != WideString::NotFound)
                {
                    childResult = childResult.Substr(0, pos) + L"└─" + childResult.Substr(pos + 2);
                }
            }

            result += childResult;
        }
        else
        {
            const WideString connector = isLast ? L"└─ " : L"├─ ";
            result += GetIndentation(depth + 1) + Colorize(connector, s_colorComment) + Colorize(L"<null child>", s_colorComment) + L"\n";
        }
    }

    return result;
}

Array<AstStatement*> AstPrintVisitor::GetChildNodes(AstStatement* node) const
{
    Array<AstStatement*> children;

    if (auto* moduleDecl = DynamicCast<AstModuleDeclaration>(node))
    {
        for (const auto& child : moduleDecl->GetChildren())
        {
            children.PushBack(child.Get());
        }
    }
    else if (auto* binExpr = DynamicCast<AstBinaryExpression>(node))
    {
        children.PushBack(binExpr->GetLeft().Get());
        children.PushBack(binExpr->GetRight().Get());
    }
    else if (auto* callExpr = DynamicCast<AstCallExpression>(node))
    {
        for (const auto& arg : callExpr->GetArguments())
        {
            if (arg && arg->GetExpr())
            {
                children.PushBack(arg->GetExpr().Get());
            }
        }
    }
    else if (auto* memberCall = DynamicCast<AstMemberCallExpression>(node))
    {
        if (memberCall->GetTarget())
        {
            children.PushBack(memberCall->GetTarget());
        }

        if (const RC<AstArgumentList>& argumentList = memberCall->GetArguments())
        {
            for (const auto& arg : argumentList->GetArguments())
            {
                if (arg && arg->GetExpr())
                {
                    children.PushBack(arg->GetExpr().Get());
                }
            }
        }
    }
    else if (auto* block = DynamicCast<AstBlock>(node))
    {
        for (const auto& child : block->GetChildren())
        {
            children.PushBack(child.Get());
        }
    }
    else if (auto* ifStmt = DynamicCast<AstIfStatement>(node))
    {
        children.PushBack(ifStmt->GetConditional().Get());
        children.PushBack(ifStmt->GetBlock().Get());
        if (ifStmt->GetElseBlock())
        {
            children.PushBack(ifStmt->GetElseBlock().Get());
        }
    }
    else if (auto* varDecl = DynamicCast<AstVariableDeclaration>(node))
    {
        if (varDecl->GetAssignment())
        {
            children.PushBack(varDecl->GetAssignment().Get());
        }
    }
    else if (auto* returnStmt = DynamicCast<AstReturnStatement>(node))
    {
        if (returnStmt->GetExpression())
        {
            children.PushBack(returnStmt->GetExpression().Get());
        }
    }
    else if (auto* member = DynamicCast<AstMember>(node))
    {
        if (member->GetTarget())
        {
            children.PushBack(member->GetTarget());
        }
    }
    else if (auto* arrayExpr = DynamicCast<AstArrayExpression>(node))
    {
        for (const auto& element : arrayExpr->GetMembers())
        {
            children.PushBack(element.Get());
        }
    }
    else if (auto* arrayAccess = DynamicCast<AstArrayAccess>(node))
    {
        children.PushBack(arrayAccess->GetTarget());
        children.PushBack(arrayAccess->GetIndex());
    }
    else if (auto* forEachLoop = DynamicCast<AstForEachLoop>(node))
    {
        // AstForEachLoop doesn't expose getters, but we still list it as a leaf
    }

    return children;
}

} // namespace Hyperion
