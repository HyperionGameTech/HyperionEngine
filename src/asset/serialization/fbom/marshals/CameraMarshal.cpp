/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <scene/camera/Camera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Camera> : public FBOMObjectMarshalerBase<Camera>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Camera &in_object, FBOMObject &out) const override
    {
        out.SetProperty(NAME("translation"), FBOMData::FromVec3f(in_object.GetTranslation()));
        out.SetProperty(NAME("direction"), FBOMData::FromVec3f(in_object.GetDirection()));
        out.SetProperty(NAME("up"), FBOMData::FromVec3f(in_object.GetUpVector()));
        out.SetProperty(NAME("view_matrix"), FBOMData::FromMat4f(in_object.GetViewMatrix()));
        out.SetProperty(NAME("projection_matrix"), FBOMData::FromMat4f(in_object.GetProjectionMatrix()));
        out.SetProperty(NAME("view_projection_matrix"), FBOMData::FromMat4f(in_object.GetViewProjectionMatrix()));
        out.SetProperty(NAME("width"), FBOMData::FromUnsignedInt(uint32(in_object.GetWidth())));
        out.SetProperty(NAME("height"), FBOMData::FromUnsignedInt(uint32(in_object.GetHeight())));
        out.SetProperty(NAME("near"), FBOMData::FromFloat(in_object.GetNear()));
        out.SetProperty(NAME("far"), FBOMData::FromFloat(in_object.GetFar()));
        out.SetProperty(NAME("frustum"), FBOMSequence(FBOMVec4f(), in_object.GetFrustum().GetPlanes().Size()), ByteBuffer(in_object.GetFrustum().GetPlanes().ByteSize(), in_object.GetFrustum().GetPlanes().Data()));
        out.SetProperty(NAME("fov"), FBOMData::FromFloat(in_object.GetFOV()));
        out.SetProperty(NAME("left"), FBOMData::FromFloat(in_object.GetLeft()));
        out.SetProperty(NAME("right"), FBOMData::FromFloat(in_object.GetRight()));
        out.SetProperty(NAME("bottom"), FBOMData::FromFloat(in_object.GetBottom()));
        out.SetProperty(NAME("top"), FBOMData::FromFloat(in_object.GetTop()));

        // TODO: Save camera controller!

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {

        struct CameraParams
        {
            float _near, _far, fov;
            float left, right, bottom, top;
            uint32 width, height;
        } camera_params;

        Memory::MemSet(&camera_params, 0, sizeof(camera_params));

        in.GetProperty("width").ReadUnsignedInt(&camera_params.width);
        in.GetProperty("height").ReadUnsignedInt(&camera_params.height);
        in.GetProperty("near").ReadFloat(&camera_params._near);
        in.GetProperty("far").ReadFloat(&camera_params._far);
        in.GetProperty("fov").ReadFloat(&camera_params.fov);
        in.GetProperty("left").ReadFloat(&camera_params.left);
        in.GetProperty("right").ReadFloat(&camera_params.right);
        in.GetProperty("bottom").ReadFloat(&camera_params.bottom);
        in.GetProperty("top").ReadFloat(&camera_params.top);

        Handle<Camera> camera_handle;

        if (camera_params.fov > MathUtil::epsilon_f) {
            camera_handle = CreateObject<Camera>(
                camera_params.fov,
                camera_params.width, camera_params.height,
                camera_params._near, camera_params._far
            );
        } else {
            camera_handle = CreateObject<Camera>(
                camera_params.width, camera_params.height,
                camera_params.left, camera_params.right,
                camera_params.bottom, camera_params.top,
                camera_params._near, camera_params._far
            );
        }
        
        {
            Vec3f translation, direction, up_vector;
            in.GetProperty("translation").ReadVec3f(&translation);
            in.GetProperty("direction").ReadVec3f(&direction);
            in.GetProperty("up").ReadVec3f(&up_vector);

            camera_handle->SetTranslation(translation);
            camera_handle->SetDirection(direction);
            camera_handle->SetUpVector(up_vector);
            
            in.GetProperty("view_matrix").ReadMat4f(&camera_handle->GetViewMatrix());
            in.GetProperty("projection_matrix").ReadMat4f(&camera_handle->GetProjectionMatrix());
            in.GetProperty("view_projection_matrix").ReadMat4f(&camera_handle->GetViewProjectionMatrix());
        }

        { // frustum
            Vec4f planes[6];

            if (FBOMResult err = in.GetProperty("frustum").ReadElements(FBOMVec4f(), 6, &planes[0])) {
                return err;
            }

            for (uint32 i = 0; i < ArraySize(planes); i++) {
                camera_handle->GetFrustum().GetPlane(i) = planes[i];
            }
        }

        out_object = std::move(camera_handle);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Camera, FBOMMarshaler<Camera>);

} // namespace hyperion::fbom