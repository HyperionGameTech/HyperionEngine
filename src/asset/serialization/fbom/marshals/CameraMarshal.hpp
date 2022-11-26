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
        out.SetProperty("translation", FBOMVec3f(), in_object.GetTranslation());
        out.SetProperty("direction", FBOMVec3f(), in_object.GetDirection());
        out.SetProperty("up", FBOMVec3f(), in_object.GetUpVector());
        out.SetProperty("view_matrix", FBOMMat4(), in_object.GetViewMatrix());
        out.SetProperty("projection_matrix", FBOMMat4(), in_object.GetProjectionMatrix());
        out.SetProperty("view_projection_matrix", FBOMMat4(), in_object.GetViewProjectionMatrix());
        out.SetProperty("width", FBOMUnsignedInt(), in_object.GetWidth());
        out.SetProperty("height", FBOMUnsignedInt(), in_object.GetHeight());
        out.SetProperty("near", FBOMFloat(), in_object.GetNear());
        out.SetProperty("far", FBOMFloat(), in_object.GetFar());
        out.SetProperty("frustum", FBOMArray(FBOMVec4f(), 6), &in_object.GetFrustum().GetPlanes()[0]);
        out.SetProperty("fov", FBOMFloat(), in_object.GetFov());
        out.SetProperty("left", FBOMFloat(), in_object.GetLeft());
        out.SetProperty("right", FBOMFloat(), in_object.GetRight());
        out.SetProperty("bottom", FBOMFloat(), in_object.GetBottom());
        out.SetProperty("top", FBOMFloat(), in_object.GetTop());

        // TODO: Save camera controller!

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        Float _near, _far, fov;
        Float left, right, bottom, top;
        UInt32 width, height;

        in.GetProperty("width").ReadUnsignedInt(&width);
        in.GetProperty("height").ReadUnsignedInt(&height);
        in.GetProperty("near").ReadFloat(&_near);
        in.GetProperty("far").ReadFloat(&_far);
        in.GetProperty("fov").ReadFloat(&fov);
        in.GetProperty("left").ReadFloat(&left);
        in.GetProperty("right").ReadFloat(&right);
        in.GetProperty("bottom").ReadFloat(&bottom);
        in.GetProperty("top").ReadFloat(&top);

        UniquePtr<OpaqueHandle<Camera>> camera_handle;

        if (fov > MathUtil::epsilon<Float>) {
            camera_handle = UniquePtr<OpaqueHandle<Camera>>::Construct(
                Engine::Get()->CreateObject<Camera>(
                    fov, width, height, _near, _far
                )
            );
        } else {
            camera_handle = UniquePtr<OpaqueHandle<Camera>>::Construct(
                Engine::Get()->CreateObject<Camera>(
                    width, height, left, right, bottom, top, _near, _far
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

            Matrix4 view_matrix, projection_matrix, view_projection_matrix;
            in.GetProperty("view_matrix").ReadAsType(FBOMMat4(), &(*camera_handle)->GetViewMatrix());
            in.GetProperty("projection_matrix").ReadAsType(FBOMMat4(), &(*camera_handle)->GetProjectionMatrix());
            in.GetProperty("view_projection_matrix").ReadAsType(FBOMMat4(), &(*camera_handle)->GetViewProjectionMatrix());
        }

        { // frustum
            Vector4 planes[6];

            in.GetProperty("frustum").ReadArrayElements(FBOMVec4f(), 6, &planes[0]);

            for (UInt i = 0; i < std::size(planes); i++) {
                (*camera_handle)->GetFrustum().GetPlane(i) = planes[i];
            }
        }

        out_object = std::move(camera_handle);

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif