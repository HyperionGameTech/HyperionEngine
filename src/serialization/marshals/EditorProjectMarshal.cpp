/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/marshals/ObjectMarshal.hpp>

#include <core/reflection/Class.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <editor/EditorProject.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion::serialization {

class EditorProjectMarshal : public ObjectMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        const EditorProject& editorProject = in.Get<EditorProject>();

        if (FBOMResult err = ObjectMarshal::Serialize(in, out))
        {
            return err;
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        Handle<EditorProject> editorProject = CreateObject<EditorProject>();
        out = HypData(editorProject);

        if (FBOMResult err = ObjectMarshal::Deserialize_Internal(context, in, EditorProject::StaticClass(), out))
        {
            return err;
        }

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(EditorProject, EditorProjectMarshal);

} // namespace hyperion::serialization
