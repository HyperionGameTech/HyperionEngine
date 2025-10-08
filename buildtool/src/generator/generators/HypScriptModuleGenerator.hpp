/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_HYPSCRIPT_MODULE_GENERATOR_HPP
#define HYPERION_BUILDTOOL_HYPSCRIPT_MODULE_GENERATOR_HPP

#include <generator/Generator.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>

namespace hyperion {
namespace buildtool {

struct HypClassDefinition;

class HypScriptModuleGenerator : public GeneratorBase
{
public:
    virtual ~HypScriptModuleGenerator() override = default;

    HYP_FORCE_INLINE Result Generate(const Analyzer& analyzer, const Module& mod) const
    {
        return GeneratorBase::Generate(analyzer, mod);
    }

    virtual Result Generate(const Analyzer& analyzer, const Module& mod, ByteWriter& writer) const override;
    virtual FilePath GetOutputFilePath(const Analyzer& analyzer, const Module& mod) const override;

protected:
    Array<const HypClassDefinition*> SortClassesTopologically(const Analyzer& analyzer, const Array<const HypClassDefinition*>& classes, const HashMap<String, SizeType>& classNameToIndex) const;
};

} // namespace buildtool
} // namespace hyperion

#endif
