#ifndef HYPERION_V2_FBOM_MARSHALS_CAMERA_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_CAMERA_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/EntityMarshal.hpp>
#include <asset/serialization/fbom/marshals/NodeMarshal.hpp>
#include <scene/camera/Camera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Camera> : public FBOMObjectMarshalerBase<Camera>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType(Camera::GetClass().GetName());
    }

    virtual FBOMResult Serialize(const Camera &in_object, FBOMObject &out) const override
    {
        out.SetProperty("translation", FBOMData::FromVec3f(in_object.GetTranslation()));
        out.SetProperty("direction", FBOMData::FromVec3f(in_object.GetDirection()));
        out.SetProperty("up",FBOMData::FromVec3f(in_object.GetUpVector()));
        out.SetProperty("view_matrix", FBOMData::FromMat4(in_object.GetViewMatrix()));
        out.SetProperty("projection_matrix", FBOMData::FromMat4(in_object.GetProjectionMatrix()));
        out.SetProperty("view_projection_matrix", FBOMData::FromMat4(in_object.GetViewProjectionMatrix()));
        out.SetProperty("width", FBOMData::FromUnsignedInt(uint32(in_object.GetWidth())));
        out.SetProperty("height", FBOMData::FromUnsignedInt(uint32(in_object.GetHeight())));
        out.SetProperty("near", FBOMData::FromFloat(in_object.GetNear()));
        out.SetProperty("far", FBOMData::FromFloat(in_object.GetFar()));
        out.SetProperty("frustum", FBOMData::FromArray(in_object.GetFrustum().GetPlanes()));
        out.SetProperty("fov", FBOMData::FromFloat(in_object.GetFOV()));
        out.SetProperty("left", FBOMData::FromFloat(in_object.GetLeft()));
        out.SetProperty("right", FBOMData::FromFloat(in_object.GetRight()));
        out.SetProperty("bottom", FBOMData::FromFloat(in_object.GetBottom()));
        out.SetProperty("top", FBOMData::FromFloat(in_object.GetTop()));

        // TODO: Save camera controller!

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
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

        UniquePtr<Handle<Camera>> camera_handle;

        if (camera_params.fov > MathUtil::epsilon_f) {
            camera_handle = UniquePtr<Handle<Camera>>::Construct(
                CreateObject<Camera>(
                    camera_params.fov,
                    camera_params.width, camera_params.height,
                    camera_params._near, camera_params._far
                )
            );
        } else {
            camera_handle = UniquePtr<Handle<Camera>>::Construct(
                CreateObject<Camera>(
                    camera_params.width, camera_params.height,
                    camera_params.left, camera_params.right,
                    camera_params.bottom, camera_params.top,
                    camera_params._near, camera_params._far
                )
            );
        }
        
        {
            Vector3 translation, direction, up_vector;
            in.GetProperty("translation").ReadArrayElements(FBOMFloat(), 3, &translation);
            in.GetProperty("direction").ReadArrayElements(FBOMFloat(), 3, &direction);
            in.GetProperty("up").ReadArrayElements(FBOMFloat(), 3, &up_vector);

            (*camera_handle)->SetTranslation(translation);
            (*camera_handle)->SetDirection(direction);
            (*camera_handle)->SetUpVector(up_vector);
            
            in.GetProperty("view_matrix").ReadMat4(&(*camera_handle)->GetViewMatrix());
            in.GetProperty("projection_matrix").ReadMat4(&(*camera_handle)->GetProjectionMatrix());
            in.GetProperty("view_projection_matrix").ReadMat4(&(*camera_handle)->GetViewProjectionMatrix());
        }

        { // frustum
            Vector4 planes[6];

            in.GetProperty("frustum").ReadArrayElements(FBOMVec4f(), 6, &planes[0]);

            for (uint i = 0; i < std::size(planes); i++) {
                (*camera_handle)->GetFrustum().GetPlane(i) = planes[i];
            }
        }

        out_object = std::move(camera_handle);

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif