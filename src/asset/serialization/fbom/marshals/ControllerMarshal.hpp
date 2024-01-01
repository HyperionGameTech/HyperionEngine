#ifndef HYPERION_V2_FBOM_MARSHALS_CONTROLLER_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_CONTROLLER_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <scene/Controller.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Controller> : public FBOMObjectMarshalerBase<Controller>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType("Controller");
    }

    virtual FBOMResult Serialize(const Controller &in_object, FBOMObject &out) const override
    {
        in_object.Serialize(out);

        AssertThrowMsg(out.HasProperty("controller_name"), "Controller Serialize() method must set controller_name property");

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        Name component_name;

        if (auto err = in.GetProperty("component_name").ReadName(&component_name)) {
            return err;
        }

        if (!g_engine->GetComponents().IsRegistered(component_name)) {
            DebugLog(
                LogType::Error,
                "Component with name %s is not registered, cannot continue loading the controller\n",
                component_name.LookupString().Data()
            );

            return { FBOMResult::FBOM_ERR, "Invalid component - not registered" };
        }

        TypeID type_id = g_engine->GetComponents().GetControllerTypeID(component_name);

        if (!type_id) {
            return { FBOMResult::FBOM_ERR, "Invalid component type ID" };
        }

        auto component = g_engine->GetComponents().CreateByName(component_name);

        if (!component) {
            return { FBOMResult::FBOM_ERR, "Failed to construct component" };
        }

        if (auto err = component->Deserialize(in)) {
            return err;
        }

        out_object = UniquePtr<ControllerSerializationWrapper>::Construct(ControllerSerializationWrapper {
            type_id,
            std::move(component)
        });

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif