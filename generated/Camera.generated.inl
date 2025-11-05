#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region CameraProjectionMode Reflection Data

HYP_BEGIN_ENUM(CameraProjectionMode, 386, 0, {})
    StaticField(NAME(HYP_STR(NONE)), CameraProjectionMode::NONE),
    StaticField(NAME(HYP_STR(PERSPECTIVE)), CameraProjectionMode::PERSPECTIVE),
    StaticField(NAME(HYP_STR(ORTHOGRAPHIC)), CameraProjectionMode::ORTHOGRAPHIC)
HYP_END_ENUM

#pragma endregion CameraProjectionMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region CameraFlags Reflection Data

HYP_BEGIN_ENUM(CameraFlags, 387, 0, {})
    StaticField(NAME(HYP_STR(NONE)), CameraFlags::NONE),
    StaticField(NAME(HYP_STR(MATCH_WINDOW_SIZE)), CameraFlags::MATCH_WINDOW_SIZE)
HYP_END_ENUM

#pragma endregion CameraFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region CameraController Reflection Data

HYP_BEGIN_CLASS(CameraController, 182, 8, NAME("HypObjectBase"), ClassAttribute("abstract", true))
    Method(NAME(HYP_STR(GetInputHandler)), &CameraController::GetInputHandler, Span<const ClassAttribute> { {ClassAttribute("property", "InputHandler") } }),
    Method(NAME(HYP_STR(SetInputHandler)), &CameraController::SetInputHandler, Span<const ClassAttribute> { {ClassAttribute("property", "InputHandler") } }),
    Method(NAME(HYP_STR(GetCamera)), &CameraController::GetCamera, Span<const ClassAttribute> { {ClassAttribute("property", "Camera") } }),
    Method(NAME(HYP_STR(GetProjectionMode)), &CameraController::GetProjectionMode, Span<const ClassAttribute> { {ClassAttribute("property", "ProjectionMode") } }),
    Method(NAME(HYP_STR(IsMouseLockAllowed)), &CameraController::IsMouseLockAllowed),
    Method(NAME(HYP_STR(IsMouseLockRequested)), &CameraController::IsMouseLockRequested),
    Method(NAME(HYP_STR(SetTranslation)), &CameraController::SetTranslation),
    Method(NAME(HYP_STR(SetNextTranslation)), &CameraController::SetNextTranslation),
    Method(NAME(HYP_STR(SetDirection)), &CameraController::SetDirection),
    Method(NAME(HYP_STR(SetUpVector)), &CameraController::SetUpVector),
    Field(NAME(HYP_STR(Camera)), &CameraController::m_camera, offsetof(CameraController, m_camera), Span<const ClassAttribute> { {ClassAttribute("property", "Camera"), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(InputHandler)), &CameraController::m_inputHandler, offsetof(CameraController, m_inputHandler), Span<const ClassAttribute> { {ClassAttribute("property", "InputHandler"), ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(ProjectionMode)), &CameraController::m_projectionMode, offsetof(CameraController, m_projectionMode), Span<const ClassAttribute> { {ClassAttribute("property", "ProjectionMode"), ClassAttribute("editor", true) } })
HYP_END_CLASS

#pragma endregion CameraController Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region Camera Reflection Data

HYP_BEGIN_CLASS(Camera, 177, 0, NAME("Entity"))
    Method(NAME(HYP_STR(GetCameraFlags)), &Camera::GetCameraFlags, Span<const ClassAttribute> { {ClassAttribute("property", "Flags"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetCameraFlags)), &Camera::SetCameraFlags, Span<const ClassAttribute> { {ClassAttribute("property", "Flags"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetCameraControllers)), &Camera::GetCameraControllers, Span<const ClassAttribute> { {ClassAttribute("property", "CameraControllers") } }),
    Method(NAME(HYP_STR(GetCameraController)), &Camera::GetCameraController),
    Method(NAME(HYP_STR(HasActiveCameraController)), &Camera::HasActiveCameraController),
    Method(NAME(HYP_STR(AddCameraController)), &Camera::AddCameraController),
    Method(NAME(HYP_STR(RemoveCameraController)), &Camera::RemoveCameraController),
    Method(NAME(HYP_STR(GetDimensions)), &Camera::GetDimensions, Span<const ClassAttribute> { {ClassAttribute("property", "Dimensions") } }),
    Method(NAME(HYP_STR(SetDimensions)), &Camera::SetDimensions, Span<const ClassAttribute> { {ClassAttribute("property", "Dimensions") } }),
    Method(NAME(HYP_STR(GetNear)), &Camera::GetNear, Span<const ClassAttribute> { {ClassAttribute("property", "Near"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetNear)), &Camera::SetNear, Span<const ClassAttribute> { {ClassAttribute("property", "Near"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetFar)), &Camera::GetFar, Span<const ClassAttribute> { {ClassAttribute("property", "Far"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetFar)), &Camera::SetFar, Span<const ClassAttribute> { {ClassAttribute("property", "Far"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetFOV)), &Camera::GetFOV, Span<const ClassAttribute> { {ClassAttribute("property", "FOV"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetFOV)), &Camera::SetFOV, Span<const ClassAttribute> { {ClassAttribute("property", "FOV"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetLeft)), &Camera::GetLeft, Span<const ClassAttribute> { {ClassAttribute("property", "Left"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetLeft)), &Camera::SetLeft, Span<const ClassAttribute> { {ClassAttribute("property", "Left"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetRight)), &Camera::GetRight, Span<const ClassAttribute> { {ClassAttribute("property", "Right"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetRight)), &Camera::SetRight, Span<const ClassAttribute> { {ClassAttribute("property", "Right"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetBottom)), &Camera::GetBottom, Span<const ClassAttribute> { {ClassAttribute("property", "Bottom"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetBottom)), &Camera::SetBottom, Span<const ClassAttribute> { {ClassAttribute("property", "Bottom"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetTop)), &Camera::GetTop, Span<const ClassAttribute> { {ClassAttribute("property", "Top"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetTop)), &Camera::SetTop, Span<const ClassAttribute> { {ClassAttribute("property", "Top"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetTranslation)), &Camera::GetTranslation, Span<const ClassAttribute> { {ClassAttribute("property", "Translation"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetTranslation)), &Camera::SetTranslation, Span<const ClassAttribute> { {ClassAttribute("property", "Translation"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetDirection)), &Camera::GetDirection, Span<const ClassAttribute> { {ClassAttribute("property", "Direction"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetDirection)), &Camera::SetDirection, Span<const ClassAttribute> { {ClassAttribute("property", "Direction"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetUpVector)), &Camera::GetUpVector, Span<const ClassAttribute> { {ClassAttribute("property", "Up"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetUpVector)), &Camera::SetUpVector, Span<const ClassAttribute> { {ClassAttribute("property", "Up"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetSideVector)), &Camera::GetSideVector),
    Method(NAME(HYP_STR(GetTarget)), &Camera::GetTarget),
    Method(NAME(HYP_STR(SetTarget)), &Camera::SetTarget),
    Method(NAME(HYP_STR(Rotate)), &Camera::Rotate),
    Method(NAME(HYP_STR(GetFrustum)), &Camera::GetFrustum, Span<const ClassAttribute> { {ClassAttribute("property", "Frustum"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetFrustum)), &Camera::SetFrustum, Span<const ClassAttribute> { {ClassAttribute("property", "Frustum"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetViewMatrix)), &Camera::GetViewMatrix, Span<const ClassAttribute> { {ClassAttribute("property", "ViewMatrix"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetViewMatrix)), &Camera::SetViewMatrix, Span<const ClassAttribute> { {ClassAttribute("property", "ViewMatrix"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetProjectionMatrix)), &Camera::GetProjectionMatrix, Span<const ClassAttribute> { {ClassAttribute("property", "ViewMatrix"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetProjectionMatrix)), &Camera::SetProjectionMatrix, Span<const ClassAttribute> { {ClassAttribute("property", "ViewMatrix"), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetViewProjectionMatrix)), &Camera::GetViewProjectionMatrix),
    Method(NAME(HYP_STR(SetViewProjectionMatrix)), &Camera::SetViewProjectionMatrix),
    Method(NAME(HYP_STR(GetPreviousViewMatrix)), &Camera::GetPreviousViewMatrix),
    Method(NAME(HYP_STR(TransformScreenToNDC)), &Camera::TransformScreenToNDC),
    Method(NAME(HYP_STR(TransformNDCToWorld)), &Camera::TransformNDCToWorld),
    Method(NAME(HYP_STR(TransformWorldToNDC)), &Camera::TransformWorldToNDC),
    Method(NAME(HYP_STR(TransformWorldToScreen)), &Camera::TransformWorldToScreen),
    Method(NAME(HYP_STR(TransformNDCToScreen)), &Camera::TransformNDCToScreen),
    Method(NAME(HYP_STR(TransformScreenToWorld)), &Camera::TransformScreenToWorld),
    Method(NAME(HYP_STR(GetPixelSize)), &Camera::GetPixelSize),
    Field(NAME(HYP_STR(MatchWindowSizeRatio)), &Camera::m_matchWindowSizeRatio, offsetof(Camera, m_matchWindowSizeRatio), Span<const ClassAttribute> { {ClassAttribute("property", "MatchWindowSizeRatio"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(CameraControllers)), &Camera::m_cameraControllers, offsetof(Camera, m_cameraControllers), Span<const ClassAttribute> { {ClassAttribute("property", "CameraControllers") } }),
    Method(NAME(HYP_STR(SetCameraControllers)), &Camera::SetCameraControllers, Span<const ClassAttribute> { {ClassAttribute("property", "CameraControllers") } })
HYP_END_CLASS

#pragma endregion Camera Reflection Data

HYP_REGISTER_ENTITY_TYPE(Camera);
} // namespace hyperion


namespace hyperion {

#pragma region NullCameraController Reflection Data

HYP_BEGIN_CLASS(NullCameraController, 190, 0, NAME("CameraController"))
HYP_END_CLASS

#pragma endregion NullCameraController Reflection Data

} // namespace hyperion

