#include <script/compiler/ast/AstTypeHolder.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/ast/AstNil.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <util/UTF8.hpp>

#include <iostream>

namespace hyperion::compiler {

AstTypeHolder::AstTypeHolder(const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD)
{
}

} // namespace hyperion::compiler
