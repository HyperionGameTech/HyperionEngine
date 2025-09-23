/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/object/HypData.hpp>
#include <core/object/HypClass.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <editor/EditorProject.hpp>
#include <editor/EditorSubsystem.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion::serialization {

class EditorProjectMarshal : public HypClassInstanceMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        const EditorProject& editorProject = in.Get<EditorProject>();

        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out))
        {
            return err;
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        Handle<EditorProject> editorProject = CreateObject<EditorProject>();

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, EditorProject::Class(), AnyRef(*editorProject)))
        {
            return err;
        }

        out = HypData(std::move(editorProject));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(EditorProject, EditorProjectMarshal);

} // namespace hyperion::serialization