/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <templates/Templates.hpp>

#include <analyzer/Definitions.hpp>

namespace hyperion {
namespace buildtool {

String MemberDummyClass(const HypMemberDefinition &definition)
{
    String str = "class Dummy\n";
    str += "{\n";
    str += "\t" + definition.source;
    str += "\n};\n";

    return str;
}

} // namespace buildtool
} // namespace hyperion