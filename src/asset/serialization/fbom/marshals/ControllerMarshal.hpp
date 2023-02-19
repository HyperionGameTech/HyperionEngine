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
        String controller_name;

        if (auto err = in.GetProperty("controller_name").ReadString(controller_name)) {
            return err;
        }

        if (!g_engine->GetComponents().IsRegistered(controller_name.Data())) {
            DebugLog(
                LogType::Error,
                "Controller with name %s is not registered, cannot continue loading the controller\n",
                controller_name.Data()
            );

            return { FBOMResult::FBOM_ERR, "Invalid controller - not registered" };
        }

        TypeID type_id = g_engine->GetComponents().GetControllerTypeID(controller_name.Data());

        if (!type_id) {
            return { FBOMResult::FBOM_ERR, "Invalid controller type ID" };
        }

        auto controller_ptr = g_engine->GetComponents().CreateByName(controller_name.Data());

        if (!controller_ptr) {
            return { FBOMResult::FBOM_ERR, "Failed to construct controller" };
        }

        if (auto err = controller_ptr->Deserialize(in)) {
            return err;
        }

        out_object = UniquePtr<ControllerSerializationWrapper>::Construct(ControllerSerializationWrapper {
            type_id,
            std::move(controller_ptr)
        });

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif