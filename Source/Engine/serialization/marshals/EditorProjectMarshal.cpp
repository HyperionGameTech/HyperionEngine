/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <Core/serialization/fbom/FBOMMarshaler.hpp>
#include <Core/serialization/fbom/FBOMData.hpp>
#include <Core/serialization/fbom/FBOMObject.hpp>
#include <Core/serialization/fbom/marshals/ObjectMarshal.hpp>

#include <editor/EditorProject.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

namespace Hyperion::serialization {

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

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const override
    {
        Handle<EditorProject> editorProject = MakeHandle<EditorProject>();
        out = BoxedValue(editorProject);

        if (FBOMResult err = ObjectMarshal::Deserialize_Internal(context, in, EditorProject::StaticClass(), out))
        {
            return err;
        }

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(EditorProject, EditorProjectMarshal);

} // namespace Hyperion::serialization
