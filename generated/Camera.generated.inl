#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region CameraProjectionMode Reflection Data

HYP_BEGIN_ENUM(CameraProjectionMode, 368, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), CameraProjectionMode::NONE),
    HypConstant(NAME(HYP_STR(PERSPECTIVE)), CameraProjectionMode::PERSPECTIVE),
    HypConstant(NAME(HYP_STR(ORTHOGRAPHIC)), CameraProjectionMode::ORTHOGRAPHIC)
HYP_END_ENUM

#pragma endregion CameraProjectionMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region CameraFlags Reflection Data

HYP_BEGIN_ENUM(CameraFlags, 369, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), CameraFlags::NONE),
    HypConstant(NAME(HYP_STR(MATCH_WINDOW_SIZE)), CameraFlags::MATCH_WINDOW_SIZE)
HYP_END_ENUM

#pragma endregion CameraFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region CameraController Reflection Data

HYP_BEGIN_CLASS(CameraController, 167, 7, NAME("HypObjectBase"), HypClassAttribute("abstract", true))
    HypMethod(NAME(HYP_STR(GetInputHandler)), &CameraController::GetInputHandler, Span<const HypClassAttribute> { {HypClassAttribute("property", "InputHandler") } }),
    HypMethod(NAME(HYP_STR(SetInputHandler)), &CameraController::SetInputHandler, Span<const HypClassAttribute> { {HypClassAttribute("property", "InputHandler") } }),
    HypMethod(NAME(HYP_STR(GetCamera)), &CameraController::GetCamera, Span<const HypClassAttribute> { {HypClassAttribute("property", "Camera") } }),
    HypMethod(NAME(HYP_STR(GetProjectionMode)), &CameraController::GetProjectionMode, Span<const HypClassAttribute> { {HypClassAttribute("property", "ProjectionMode") } }),
    HypMethod(NAME(HYP_STR(IsMouseLockAllowed)), &CameraController::IsMouseLockAllowed),
    HypMethod(NAME(HYP_STR(IsMouseLockRequested)), &CameraController::IsMouseLockRequested),
    HypMethod(NAME(HYP_STR(SetTranslation)), &CameraController::SetTranslation),
    HypMethod(NAME(HYP_STR(SetNextTranslation)), &CameraController::SetNextTranslation),
    HypMethod(NAME(HYP_STR(SetDirection)), &CameraController::SetDirection),
    HypMethod(NAME(HYP_STR(SetUpVector)), &CameraController::SetUpVector),
    HypField(NAME(HYP_STR(Camera)), &CameraController::m_camera, offsetof(CameraController, m_camera), Span<const HypClassAttribute> { {HypClassAttribute("property", "Camera"), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(InputHandler)), &CameraController::m_inputHandler, offsetof(CameraController, m_inputHandler), Span<const HypClassAttribute> { {HypClassAttribute("property", "InputHandler"), HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(ProjectionMode)), &CameraController::m_projectionMode, offsetof(CameraController, m_projectionMode), Span<const HypClassAttribute> { {HypClassAttribute("property", "ProjectionMode"), HypClassAttribute("editor", true) } })
HYP_END_CLASS

#pragma endregion CameraController Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

#pragma region Camera Reflection Data

HYP_BEGIN_CLASS(Camera, 143, 0, NAME("Entity"))
    HypMethod(NAME(HYP_STR(GetCameraFlags)), &Camera::GetCameraFlags, Span<const HypClassAttribute> { {HypClassAttribute("property", "Flags"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetCameraFlags)), &Camera::SetCameraFlags, Span<const HypClassAttribute> { {HypClassAttribute("property", "Flags"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetCameraControllers)), &Camera::GetCameraControllers, Span<const HypClassAttribute> { {HypClassAttribute("property", "CameraControllers") } }),
    HypMethod(NAME(HYP_STR(GetCameraController)), &Camera::GetCameraController),
    HypMethod(NAME(HYP_STR(HasActiveCameraController)), &Camera::HasActiveCameraController),
    HypMethod(NAME(HYP_STR(AddCameraController)), &Camera::AddCameraController),
    HypMethod(NAME(HYP_STR(RemoveCameraController)), &Camera::RemoveCameraController),
    HypMethod(NAME(HYP_STR(GetWidth)), &Camera::GetWidth, Span<const HypClassAttribute> { {HypClassAttribute("property", "Width"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetWidth)), &Camera::SetWidth, Span<const HypClassAttribute> { {HypClassAttribute("property", "Width"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetHeight)), &Camera::GetHeight, Span<const HypClassAttribute> { {HypClassAttribute("property", "Height"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetHeight)), &Camera::SetHeight, Span<const HypClassAttribute> { {HypClassAttribute("property", "Height"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetDimensions)), &Camera::GetDimensions, Span<const HypClassAttribute> { {HypClassAttribute("property", "Dimensions") } }),
    HypMethod(NAME(HYP_STR(SetDimensions)), &Camera::SetDimensions, Span<const HypClassAttribute> { {HypClassAttribute("property", "Dimensions") } }),
    HypMethod(NAME(HYP_STR(GetNear)), &Camera::GetNear, Span<const HypClassAttribute> { {HypClassAttribute("property", "Near"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetNear)), &Camera::SetNear, Span<const HypClassAttribute> { {HypClassAttribute("property", "Near"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetFar)), &Camera::GetFar, Span<const HypClassAttribute> { {HypClassAttribute("property", "Far"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetFar)), &Camera::SetFar, Span<const HypClassAttribute> { {HypClassAttribute("property", "Far"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetFOV)), &Camera::GetFOV, Span<const HypClassAttribute> { {HypClassAttribute("property", "FOV"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetFOV)), &Camera::SetFOV, Span<const HypClassAttribute> { {HypClassAttribute("property", "FOV"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetLeft)), &Camera::GetLeft, Span<const HypClassAttribute> { {HypClassAttribute("property", "Left"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetLeft)), &Camera::SetLeft, Span<const HypClassAttribute> { {HypClassAttribute("property", "Left"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetRight)), &Camera::GetRight, Span<const HypClassAttribute> { {HypClassAttribute("property", "Right"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetRight)), &Camera::SetRight, Span<const HypClassAttribute> { {HypClassAttribute("property", "Right"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetBottom)), &Camera::GetBottom, Span<const HypClassAttribute> { {HypClassAttribute("property", "Bottom"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetBottom)), &Camera::SetBottom, Span<const HypClassAttribute> { {HypClassAttribute("property", "Bottom"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetTop)), &Camera::GetTop, Span<const HypClassAttribute> { {HypClassAttribute("property", "Top"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetTop)), &Camera::SetTop, Span<const HypClassAttribute> { {HypClassAttribute("property", "Top"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetTranslation)), &Camera::GetTranslation, Span<const HypClassAttribute> { {HypClassAttribute("property", "Translation"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetTranslation)), &Camera::SetTranslation, Span<const HypClassAttribute> { {HypClassAttribute("property", "Translation"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetDirection)), &Camera::GetDirection, Span<const HypClassAttribute> { {HypClassAttribute("property", "Direction"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetDirection)), &Camera::SetDirection, Span<const HypClassAttribute> { {HypClassAttribute("property", "Direction"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetUpVector)), &Camera::GetUpVector, Span<const HypClassAttribute> { {HypClassAttribute("property", "Up"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetUpVector)), &Camera::SetUpVector, Span<const HypClassAttribute> { {HypClassAttribute("property", "Up"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetSideVector)), &Camera::GetSideVector),
    HypMethod(NAME(HYP_STR(GetTarget)), &Camera::GetTarget),
    HypMethod(NAME(HYP_STR(SetTarget)), &Camera::SetTarget),
    HypMethod(NAME(HYP_STR(Rotate)), &Camera::Rotate),
    HypMethod(NAME(HYP_STR(GetFrustum)), &Camera::GetFrustum, Span<const HypClassAttribute> { {HypClassAttribute("property", "Frustum"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetFrustum)), &Camera::SetFrustum, Span<const HypClassAttribute> { {HypClassAttribute("property", "Frustum"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetViewMatrix)), &Camera::GetViewMatrix, Span<const HypClassAttribute> { {HypClassAttribute("property", "ViewMatrix"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetViewMatrix)), &Camera::SetViewMatrix, Span<const HypClassAttribute> { {HypClassAttribute("property", "ViewMatrix"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetProjectionMatrix)), &Camera::GetProjectionMatrix, Span<const HypClassAttribute> { {HypClassAttribute("property", "ViewMatrix"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetProjectionMatrix)), &Camera::SetProjectionMatrix, Span<const HypClassAttribute> { {HypClassAttribute("property", "ViewMatrix"), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetViewProjectionMatrix)), &Camera::GetViewProjectionMatrix),
    HypMethod(NAME(HYP_STR(SetViewProjectionMatrix)), &Camera::SetViewProjectionMatrix),
    HypMethod(NAME(HYP_STR(GetPreviousViewMatrix)), &Camera::GetPreviousViewMatrix),
    HypMethod(NAME(HYP_STR(TransformScreenToNDC)), &Camera::TransformScreenToNDC),
    HypMethod(NAME(HYP_STR(TransformNDCToWorld)), &Camera::TransformNDCToWorld),
    HypMethod(NAME(HYP_STR(TransformWorldToNDC)), &Camera::TransformWorldToNDC),
    HypMethod(NAME(HYP_STR(TransformWorldToScreen)), &Camera::TransformWorldToScreen),
    HypMethod(NAME(HYP_STR(TransformNDCToScreen)), &Camera::TransformNDCToScreen),
    HypMethod(NAME(HYP_STR(TransformScreenToWorld)), &Camera::TransformScreenToWorld),
    HypMethod(NAME(HYP_STR(GetPixelSize)), &Camera::GetPixelSize),
    HypField(NAME(HYP_STR(MatchWindowSizeRatio)), &Camera::m_matchWindowSizeRatio, offsetof(Camera, m_matchWindowSizeRatio), Span<const HypClassAttribute> { {HypClassAttribute("property", "MatchWindowSizeRatio"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(CameraControllers)), &Camera::m_cameraControllers, offsetof(Camera, m_cameraControllers), Span<const HypClassAttribute> { {HypClassAttribute("property", "CameraControllers") } }),
    HypMethod(NAME(HYP_STR(SetCameraControllers)), &Camera::SetCameraControllers, Span<const HypClassAttribute> { {HypClassAttribute("property", "CameraControllers") } })
HYP_END_CLASS

#pragma endregion Camera Reflection Data

HYP_REGISTER_ENTITY_TYPE(Camera);
} // namespace hyperion


namespace hyperion {

#pragma region NullCameraController Reflection Data

HYP_BEGIN_CLASS(NullCameraController, 168, 0, NAME("CameraController"))
HYP_END_CLASS

#pragma endregion NullCameraController Reflection Data

} // namespace hyperion

