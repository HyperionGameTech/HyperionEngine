/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <generator/generators/CXXModuleGenerator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <asset/ByteWriter.hpp>

#include <core/containers/HashSet.hpp>

#include <core/utilities/Format.hpp>

#include <util/StringUtil.hpp>

namespace hyperion {
namespace buildtool {

static const HashMap<HypClassDefinitionType, String> g_start_macro_names = {
    { HypClassDefinitionType::CLASS, "HYP_BEGIN_CLASS" },
    { HypClassDefinitionType::STRUCT, "HYP_BEGIN_STRUCT" },
    { HypClassDefinitionType::ENUM, "HYP_BEGIN_ENUM" }
};

static const HashMap<HypClassDefinitionType, String> g_end_macro_names = {
    { HypClassDefinitionType::CLASS, "HYP_END_CLASS" },
    { HypClassDefinitionType::STRUCT, "HYP_END_STRUCT" },
    { HypClassDefinitionType::ENUM, "HYP_END_ENUM" }
};

FilePath CXXModuleGenerator::GetOutputFilePath(const Analyzer &analyzer, const Module &mod) const
{
    FilePath relative_path = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    return analyzer.GetCXXOutputDirectory() / relative_path.BasePath() / StringUtil::StripExtension(relative_path.Basename()) + ".generated.cpp";
}

Result<void> CXXModuleGenerator::Generate_Internal(const Analyzer &analyzer, const Module &mod, ByteWriter &writer) const
{
    FilePath relative_path = FilePath(FileSystem::RelativePath(mod.GetPath().Data(), analyzer.GetSourceDirectory().Data()).c_str());

    writer.WriteString(HYP_FORMAT("/* Generated from: {} */\n\n", relative_path));

    writer.WriteString("#include <core/object/HypClassUtils.hpp>\n");
    writer.WriteString(HYP_FORMAT("#include <{}>\n\n", relative_path));

    for (const Pair<String, HypClassDefinition> &pair : mod.GetHypClasses()) {
        const HypClassDefinition &hyp_class = pair.second;
        
        const bool is_component = hyp_class.HasAttribute("component");
        const HypClassAttributeValue &struct_size_attribute_value = hyp_class.GetAttribute("size");
        const bool has_scriptable_methods = hyp_class.HasScriptableMethods();

        if (is_component) {
            writer.WriteString("#include <scene/ecs/ComponentInterface.hpp>\n");
        }

        if (has_scriptable_methods) {
            writer.WriteString("#include <dotnet/Object.hpp>\n");
            writer.WriteString("#include <dotnet/Class.hpp>\n");
            writer.WriteString("#include <dotnet/Method.hpp>\n");
        }

        writer.WriteString("\nnamespace hyperion {\n\n");

        writer.WriteString(HYP_FORMAT("#pragma region {} Reflection Data\n\n", hyp_class.name));

        Array<String> class_attributes;

        for (const Pair<String, HypClassAttributeValue> &attribute_pair : hyp_class.attributes) {
            const String &name = attribute_pair.first;
            const HypClassAttributeValue &value = attribute_pair.second;

            class_attributes.PushBack(HYP_FORMAT("HypClassAttribute(\"{}\", {})", name.ToLower(), value.ToString()));
        }

        writer.WriteString(HYP_FORMAT("{}({}", g_start_macro_names.At(hyp_class.type), hyp_class.name));

        HashSet<const HypClassDefinition *> base_class_definitions;

        if (hyp_class.base_class_names.Any()) {
            for (const String &base_class_name : hyp_class.base_class_names) {
                const HypClassDefinition *base_class_definition = analyzer.FindHypClassDefinition(base_class_name);

                if (base_class_definition) {
                    base_class_definitions.Insert(base_class_definition);
                }
            }
        }
        
        if (base_class_definitions.Any()) {
            if (base_class_definitions.Size() > 1) {
                return HYP_MAKE_ERROR(Error, "Multiple base classes not supported");
            }

            const HypClassDefinition *base_class_definition = *base_class_definitions.Begin();

            writer.WriteString(HYP_FORMAT(", NAME(\"{}\")", base_class_definition->name));
        } else {
            writer.WriteString(", {}");
        }

        if (class_attributes.Any()) {
            writer.WriteString(", " + String::Join(class_attributes, ','));
        }

        writer.WriteString(")\n");

        for (SizeType i = 0; i < hyp_class.members.Size(); ++i) {
            const HypMemberDefinition &member = hyp_class.members[i];

            if (member.type == HypMemberType::TYPE_METHOD && member.HasAttribute("Scriptable")) {
                continue;
            }

            String attributes_string;

            if (member.attributes.Any()) {
                attributes_string = "Span<const HypClassAttribute> { {";

                for (SizeType i = 0; i < member.attributes.Size(); i++) {
                    const String &name = member.attributes[i].first;
                    const HypClassAttributeValue &value = member.attributes[i].second;

                    attributes_string += HYP_FORMAT("HypClassAttribute(\"{}\", {})", name.ToLower(), value.ToString());

                    if (i != member.attributes.Size() - 1) {
                        attributes_string += ", ";
                    }
                }

                attributes_string += " } }";
            }

            if (member.type == HypMemberType::TYPE_FIELD) {
                writer.WriteString(HYP_FORMAT("    HypField(NAME(HYP_STR({})), &Type::{}, offsetof(Type, {})", member.name, member.name, member.name));

                if (attributes_string.Any()) {
                    writer.WriteString(", " + attributes_string);
                }

                writer.WriteString(")");
            } else if (member.type == HypMemberType::TYPE_METHOD) {
                writer.WriteString(HYP_FORMAT("    HypMethod(NAME(HYP_STR({})), &Type::{}", member.name, member.name));

                if (attributes_string.Any()) {
                    writer.WriteString(", " + attributes_string);
                }

                writer.WriteString(")");
            } else if (member.type == HypMemberType::TYPE_PROPERTY) {
                String property_args_string;

                if (member.attributes.Any()) {
                    for (SizeType i = 0; i < member.attributes.Size(); i++) {
                        property_args_string += member.attributes[i].first;

                        if (i != member.attributes.Size() - 1) {
                            property_args_string += ", ";
                        }
                    }
                }

                writer.WriteString(HYP_FORMAT("    HypProperty(NAME(HYP_STR({})){})", member.name, property_args_string.Any() ? ", " + property_args_string : ""));
            } else if (member.type == HypMemberType::TYPE_CONSTANT) {
                writer.WriteString(HYP_FORMAT("    HypConstant(NAME(HYP_STR({})), Type::{})", StringUtil::ToPascalCase(member.name), member.name));
            }

            if (i != hyp_class.members.Size() - 1) {
                writer.WriteString(",");
            }

            writer.WriteString("\n");
        }

        writer.WriteString(HYP_FORMAT("{}\n\n", g_end_macro_names.At(hyp_class.type)));

        writer.WriteString(HYP_FORMAT("#pragma endregion {} Reflection Data\n\n", hyp_class.name));

        if (has_scriptable_methods) {
            writer.WriteString(HYP_FORMAT("#pragma region {} Scriptable Methods\n\n", hyp_class.name));

            for (const HypMemberDefinition &member : hyp_class.members) {
                if (member.type == HypMemberType::TYPE_METHOD && member.HasAttribute("Scriptable")) {
                    if (!member.cxx_type) {
                        return HYP_MAKE_ERROR(Error, "Missing C++ type for member; Parsing failed");
                    }

                    if (!member.cxx_type->is_function || member.cxx_type->is_static) {
                        return HYP_MAKE_ERROR(Error, "Scriptable attribute can only be applied to instance methods");
                    }

                    const ASTFunctionType *function_type = dynamic_cast<const ASTFunctionType *>(member.cxx_type.Get());

                    if (!function_type) {
                        return HYP_MAKE_ERROR(Error, "Internal error: failed cast to ASTFunctionType");
                    }
                    
                    String method_args_string_sig;
                    String method_args_string_call;

                    for (SizeType i = 0; i < function_type->parameters.Size(); ++i) {
                        method_args_string_sig += function_type->parameters[i]->type->FormatDecl(function_type->parameters[i]->name);
                        method_args_string_call += function_type->parameters[i]->name;

                        if (i != function_type->parameters.Size() - 1) {
                            method_args_string_sig += ", ";
                            method_args_string_call += ", ";
                        }
                    }
                    
                    String return_type_string = function_type->return_type->Format();

                    if (function_type->return_type->IsVoid()) {
                        writer.WriteString(HYP_FORMAT("void {}::{}({}){}", hyp_class.name, member.name, method_args_string_sig, function_type->is_const_method ? " const" : ""));
                        writer.WriteString("\n");
                        writer.WriteString("{\n");
                        writer.WriteString("    if (dotnet::Object *managed_object = GetManagedObject()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::Method *method_ptr = managed_object->GetClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString(HYP_FORMAT("            managed_object->InvokeMethod<void>(method_ptr{});\n", method_args_string_call.Any() ? ", " + method_args_string_call : ""));
                        writer.WriteString("            return;\n");
                        writer.WriteString("        }\n");
                        writer.WriteString("    }\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("    {}_Impl({});\n", member.name, method_args_string_call));
                        writer.WriteString("}\n");
                    } else {
                        writer.WriteString(HYP_FORMAT("{} {}::{}({}){}", return_type_string, hyp_class.name, member.name, method_args_string_sig, function_type->is_const_method ? " const" : ""));
                        writer.WriteString("\n");
                        writer.WriteString("{\n");
                        writer.WriteString("    if (dotnet::Object *managed_object = GetManagedObject()) {\n");
                        writer.WriteString(HYP_FORMAT("        constexpr HashCode hash_code = HashCode::GetHashCode(\"{}\");\n", member.name));
                        writer.WriteString("        if (dotnet::Method *method_ptr = managed_object->GetClass()->GetMethodByHash(hash_code)) {\n");
                        writer.WriteString(HYP_FORMAT("            return managed_object->InvokeMethod<{}>(method_ptr{});\n", return_type_string, method_args_string_call.Any() ? ", " + method_args_string_call : ""));
                        writer.WriteString("        }\n");
                        writer.WriteString("    }\n");
                        writer.WriteString("\n");
                        writer.WriteString(HYP_FORMAT("    return {}_Impl({});\n", member.name, method_args_string_call));
                        writer.WriteString("}\n");
                    }
                }
            }
        
            writer.WriteString(HYP_FORMAT("#pragma endregion {} Scriptable Methods\n", hyp_class.name));
        }

        if (is_component) {
            writer.WriteString(HYP_FORMAT("HYP_REGISTER_COMPONENT({});\n", hyp_class.name));
        }

        if (struct_size_attribute_value.IsValid()) {
            writer.WriteString(HYP_FORMAT("static_assert(sizeof({}) == {}, \"Expected sizeof({}) to be {} bytes\");\n", hyp_class.name, struct_size_attribute_value.ToString(), hyp_class.name, struct_size_attribute_value.ToString()));
        }

        writer.WriteString("} // namespace hyperion\n\n");
    }

    return { };
}

} // namespace buildtool
} // namespace hyperion