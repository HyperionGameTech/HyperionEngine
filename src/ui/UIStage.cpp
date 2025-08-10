/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIStage.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIText.hpp>

#include <asset/Assets.hpp>

#include <util/MeshBuilder.hpp>

#include <scene/World.hpp>

#include <scene/camera/OrthoCamera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>

#include <scene/EntityManager.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <rendering/Texture.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <input/InputManager.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

static constexpr float g_minHoldTimeToDrag = 0.05f;

static HYP_FORCE_INLINE float GetTimeToNextKeyDownEvent(const UIObjectKeyState& keyState)
{
    if (keyState.count <= 1)
    {
        return 0.25f;
    }

    return 0.05f;
}

static HYP_FORCE_INLINE bool ShouldTriggerKeyDownEvent(const UIObjectKeyState& keyState)
{
    return GetTimeToNextKeyDownEvent(keyState) < keyState.heldTime;
}

HYP_DECLARE_LOG_CHANNEL(UI);

UIStage::UIStage()
    : UIStage(ThreadId::Current())
{
}

UIStage::UIStage(ThreadId ownerThreadId)
    : UIObject(ownerThreadId),
      m_surfaceSize { 1000, 1000 }
{
    SetName(NAME("Stage"));
    SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::PERCENT }));

    m_camera = CreateObject<Camera>();
    m_camera->SetName(NAME_FMT("{}_Camera", GetName()));
    m_camera->AddCameraController(CreateObject<OrthoCameraController>(
        0.0f, -float(m_surfaceSize.x),
        0.0f, float(m_surfaceSize.y),
        float(g_minDepth), float(g_maxDepth)));

    InitObject(m_camera);
    AddChild(m_camera);
}

UIStage::~UIStage()
{
    //    if (m_scene.IsValid())
    //    {
    //        if (Threads::IsOnThread(m_scene->GetOwnerThreadId()))
    //        {
    //            m_scene->RemoveFromWorld();
    //        }
    //        else
    //        {
    //            Threads::GetThread(m_scene->GetOwnerThreadId())->GetScheduler().Enqueue([scene = m_scene]()
    //                {
    //                    scene->RemoveFromWorld();
    //                },
    //                TaskEnqueueFlags::FIRE_AND_FORGET);
    //        }
    //    }
}

void UIStage::SetSurfaceSize(Vec2i surfaceSize)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_surfaceSize = surfaceSize;

    // if (m_camera.IsValid())
    //{
    //     m_camera->SetWidth(surfaceSize.x);
    //     m_camera->SetHeight(surfaceSize.y);

    //    // @FIXME: needs to remove and re-add the camera controller

    //    m_camera->AddCameraController(CreateObject<OrthoCameraController>(
    //        0.0f, -float(surfaceSize.x),
    //        0.0f, float(surfaceSize.y),
    //        float(g_minDepth), float(g_maxDepth)));
    //}

    UpdateSize(true);
    UpdatePosition(true);

    SetNeedsRepaintFlag();
}

const RC<FontAtlas>& UIStage::GetDefaultFontAtlas() const
{
    HYP_SCOPE;

    if (m_defaultFontAtlas != nullptr)
    {
        return m_defaultFontAtlas;
    }

    // Parent stage
    if (m_stage != nullptr)
    {
        return m_stage->GetDefaultFontAtlas();
    }

    return m_defaultFontAtlas;
}

void UIStage::SetDefaultFontAtlas(RC<FontAtlas> fontAtlas)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_defaultFontAtlas = std::move(fontAtlas);

    OnFontAtlasUpdate();
    // OnTextSizeUpdate();
}

void UIStage::Init()
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (const Handle<AppContextBase>& appContext = g_engineDriver->GetAppContext())
    {
        const auto updateSurfaceSize = [this](ApplicationWindow* window)
        {
            if (window == nullptr)
            {
                return;
            }

            const Vec2i size = Vec2i(window->GetDimensions());

            m_surfaceSize = Vec2i(size);

            if (m_camera.IsValid())
            {
                m_camera->AddCameraController(CreateObject<OrthoCameraController>(
                    0.0f, -float(m_surfaceSize.x),
                    0.0f, float(m_surfaceSize.y),
                    float(g_minDepth), float(g_maxDepth)));
            }
        };

        updateSurfaceSize(appContext->GetMainWindow());
        m_onCurrentWindowChangedHandler = appContext->OnCurrentWindowChanged.Bind(updateSurfaceSize);
    }

    if (!m_defaultFontAtlas)
    {
        auto fontAtlasAsset = g_assetManager->Load<RC<FontAtlas>>("fonts/default.json");

        if (fontAtlasAsset.HasValue())
        {
            m_defaultFontAtlas = fontAtlasAsset->Result();
        }
        else
        {
            HYP_LOG(UI, Error, "Failed to load default font atlas! Error was: {}", fontAtlasAsset.GetError().GetMessage());
        }
    }

    // create a new Scene
    if (GetScene() != nullptr)
    {
        Handle<Scene> newScene = CreateObject<Scene>(nullptr, Threads::CurrentThreadId(), SceneFlags::FOREGROUND | SceneFlags::UI);
        newScene->SetName(Name::Unique(HYP_FORMAT("UIStage_{}_Scene", GetName()).Data()));
        newScene->SetRoot(HandleFromThis()); // strong ref
        InitObject(newScene);

        g_engineDriver->GetDefaultWorld()->AddScene(newScene);

        SetScene(newScene);
    }

    UIObject::Init();
}

void UIStage::AddChildUIObject(const Handle<UIObject>& uiObject)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (!uiObject)
    {
        return;
    }

    UIObject::AddChildUIObject(uiObject);

    // Check if no parent stage
    if (m_stage == nullptr)
    {
        // Set child object stage to this
        uiObject->SetStage(this);
    }
}

void UIStage::Update_Internal(float delta)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    UIObject::Update_Internal(delta);

    for (auto& it : m_mouseButtonPressedStates)
    {
        it.second.heldTime += delta;
    }
    for (auto& it : m_keyedDownObjects)
    {
        for (auto& jt : it)
        {
            jt.second.heldTime += delta;
        }
    }
}

void UIStage::OnAttached_Internal(UIObject* parent)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    OnAttached();
}

void UIStage::OnRemoved_Internal()
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    Handle<Scene> newScene = CreateObject<Scene>(nullptr, Threads::CurrentThreadId(), SceneFlags::FOREGROUND | SceneFlags::UI);
    newScene->SetName(Name::Unique(HYP_FORMAT("UIStage_{}_Scene", GetName()).Data()));
    newScene->SetRoot(HandleFromThis()); // strong ref

    g_engineDriver->GetDefaultWorld()->AddScene(newScene);

    SetScene(newScene);

    OnRemoved();
}

void UIStage::SetStage_Internal(UIStage* stage)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    m_stage = stage;

    SetNeedsRepaintFlag();

    // Do not update children
}

bool UIStage::TestRay(const Vec2f& position, Array<Handle<UIObject>>& outObjects, EnumFlags<UIRayTestFlags> flags)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    const Vec4f worldPosition = Vec4f(position.x * float(GetSurfaceSize().x), position.y * float(GetSurfaceSize().y), 0.0f, 1.0f);
    const Vec3f direction { worldPosition.x / worldPosition.w, worldPosition.y / worldPosition.w, 0.0f };

    Ray ray;
    ray.position = worldPosition.GetXYZ() / worldPosition.w;
    ray.direction = direction;

    RayTestResults rayTestResults;

    for (auto [entity, transformComponent, boundingBoxComponent, _] : GetScene()->GetEntityManager()->GetEntitySet<TransformComponent, BoundingBoxComponent, EntityType<UIObject>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        UIObject* uiObject = static_cast<UIObject*>(entity);

        if ((flags & UIRayTestFlags::ONLY_VISIBLE) && !uiObject->GetComputedVisibility())
        {
            continue;
        }

        BoundingBox aabb = uiObject->GetAABBClamped();
        aabb.min.z = -1.0f;
        aabb.max.z = 1.0f;

        if (aabb.ContainsPoint(direction))
        {
            RayHit hit {};
            hit.hitpoint = Vec3f { position.x, position.y, 0.0f };
            hit.distance = -float(uiObject->GetComputedDepth());
            hit.id = entity->Id().Value();
            hit.userData = uiObject;

            rayTestResults.AddHit(hit);
        }
    }

    outObjects.Reserve(rayTestResults.Size());

    for (const RayHit& hit : rayTestResults)
    {
        if (Handle<UIObject> uiObject = static_cast<const UIObject*>(hit.userData)->HandleFromThis())
        {
            outObjects.PushBack(std::move(uiObject));
        }
    }

    return outObjects.Any();
}

void UIStage::SetFocusedObject(const Handle<UIObject>& uiObject)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    if (uiObject == m_focusedObject)
    {
        return;
    }

    Handle<UIObject> currentFocusedUiObject = m_focusedObject.Lock();

    // Be sure to set the focused object to nullptr before calling Blur() to prevent infinite recursion
    // due to Blur() calling SetFocusedObject() again.
    m_focusedObject.Reset();

    if (currentFocusedUiObject != nullptr)
    {
        // Only blur children if
        const bool shouldBlurChildren = uiObject == nullptr || !uiObject->IsOrHasParent(currentFocusedUiObject);

        currentFocusedUiObject->Blur(shouldBlurChildren);
    }

    m_focusedObject = uiObject;

    if (Handle<UIStage> parentStage = GetClosestParentUIObject<UIStage>())
    {
        parentStage->SetFocusedObject(uiObject);
    }
}

void UIStage::ComputeActualSize(const UIObjectSize& inSize, Vec2i& outActualSize, UpdateSizePhase phase, bool isInner)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    // stage with a parent stage: treat self like a normal UIObject
    if (m_stage != nullptr)
    {
        UIObject::ComputeActualSize(inSize, outActualSize, phase, isInner);

        return;
    }

    // inner calculation is the same
    if (isInner)
    {
        UIObject::ComputeActualSize(inSize, outActualSize, phase, isInner);

        return;
    }

    outActualSize = m_surfaceSize;
}

UIEventHandlerResult UIStage::OnInputEvent(
    InputManager* inputManager,
    const SystemEvent& event)
{
    HYP_SCOPE;
    AssertOnOwnerThread();

    UIEventHandlerResult eventHandlerResult = UIEventHandlerResult::OK;

    RayTestResults rayTestResults;

    const Vec2i mousePosition = inputManager->GetMousePosition();
    const Vec2i previousMousePosition = inputManager->GetPreviousMousePosition();
    const Vec2f mouseScreen = Vec2f(mousePosition) / Vec2f(m_surfaceSize);
    const Vec2f invSurfaceSize = Vec2f(1.0f) / Vec2f(m_surfaceSize);

    switch (event.GetType())
    {
    case SystemEventType::EVENT_MOUSEMOTION:
    {
        // check intersects with objects on mouse movement.
        // for any objects that had mouse held on them,
        // if the mouse is on them, signal mouse movement

        // project a ray into the scene and test if it hits any objects

        const EnumFlags<MouseButtonState> mouseButtons = inputManager->GetButtonStates();

        if (mouseButtons != MouseButtonState::NONE)
        { // mouse drag event
            UIEventHandlerResult mouseDragEventHandlerResult = UIEventHandlerResult::OK;

            for (const Pair<WeakHandle<UIObject>, UIObjectMouseState>& it : m_mouseButtonPressedStates)
            {
                if (it.second.mouseButtons & mouseButtons)
                {
                    // signal mouse drag
                    if (Handle<UIObject> uiObject = it.first.Lock())
                    {
                        MouseEvent mouseEvent {
                            .inputManager = inputManager,
                            .position = uiObject->TransformScreenCoordsToRelative(mousePosition),
                            .previousPosition = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                            .absolutePosition = mousePosition,
                            .mouseButtons = mouseButtons
                        };

                        if (MathUtil::Abs(it.second.originalMousePosition - mouseScreen).LengthSquared() < invSurfaceSize.LengthSquared())
                        {
                            // If the mouse position hasn't changed significantly, don't trigger a drag event
                            continue;
                        }

                        UIEventHandlerResult currentResult = uiObject->OnMouseDrag(mouseEvent);

                        HYP_LOG(UI, Debug, "Drag mouse on {} : position: {}, previous position: {}, absolute position: {}",
                            uiObject->GetName(), mouseEvent.position, mouseEvent.previousPosition, mouseEvent.absolutePosition);

                        mouseDragEventHandlerResult |= currentResult;

                        if (mouseDragEventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
                        {
                            break;
                        }
                    }
                }
            }
        }

        Array<Handle<UIObject>> rayTestResults;

        if (TestRay(mouseScreen, rayTestResults))
        {
            UIObject* firstHit = nullptr;

            UIEventHandlerResult mouseHoverEventHandlerResult = UIEventHandlerResult::OK;
            UIEventHandlerResult mouseMoveEventHandlerResult = UIEventHandlerResult::OK;

            for (auto it = rayTestResults.Begin(); it != rayTestResults.End(); ++it)
            {
                if (const Handle<UIObject>& uiObject = *it)
                {
                    if (firstHit != nullptr)
                    {
                        // We don't want to check the current object if it's not a child of the first hit object,
                        // since it would be behind the first hit object.
                        if (!firstHit->IsOrHasParent(uiObject))
                        {
                            continue;
                        }
                    }
                    else
                    {
                        firstHit = uiObject;
                    }

                    if (m_hoveredUiObjects.Contains(uiObject))
                    {
                        // Already hovered, trigger mouse move event instead
                        UIEventHandlerResult currentResult = uiObject->OnMouseMove(MouseEvent {
                            .inputManager = inputManager,
                            .position = uiObject->TransformScreenCoordsToRelative(mousePosition),
                            .previousPosition = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                            .absolutePosition = mousePosition,
                            .mouseButtons = mouseButtons });

                        mouseMoveEventHandlerResult |= currentResult;

                        if (mouseMoveEventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
                        {
                            break;
                        }
                    }
                }
            }

            firstHit = nullptr;

            for (auto it = rayTestResults.Begin(); it != rayTestResults.End(); ++it)
            {
                if (const Handle<UIObject>& uiObject = *it)
                {
                    if (firstHit != nullptr)
                    {
                        // We don't want to check the current object if it's not a child of the first hit object,
                        // since it would be behind the first hit object.
                        if (!firstHit->IsOrHasParent(uiObject))
                        {
                            continue;
                        }
                    }
                    else
                    {
                        firstHit = uiObject;
                    }

                    if (!uiObject->AcceptsFocus() || !uiObject->IsEnabled())
                    {
                        continue;
                    }

                    if (!m_hoveredUiObjects.Insert(uiObject).second)
                    {
                        continue;
                    }

                    uiObject->SetFocusState(uiObject->GetFocusState() | UIObjectFocusState::HOVER);

                    UIEventHandlerResult currentResult = uiObject->OnMouseHover(MouseEvent {
                        .inputManager = inputManager,
                        .position = uiObject->TransformScreenCoordsToRelative(mousePosition),
                        .previousPosition = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                        .absolutePosition = mousePosition,
                        .mouseButtons = mouseButtons });

                    mouseHoverEventHandlerResult |= currentResult;

                    //                    BoundingBoxComponent& boundingBoxComponent = uiObject->GetScene()->GetEntityManager()->GetComponent<BoundingBoxComponent>(uiObject->GetEntity());

                    //                    HYP_LOG(UI, Debug, "Mouse hover on {}: {}, Text: {}, Size: {}, Inner size: {}, Text Size: {}, Node AABB: {}, Has children: {}, Size clamped: {}, Depth: {}",
                    //                        uiObject->InstanceClass()->GetName(),
                    //                        uiObject->GetName(),
                    //                        uiObject->GetText(),
                    //                        uiObject->GetActualSize(),
                    //                        uiObject->GetActualInnerSize(),
                    //                        uiObject->GetTextSize(),
                    //                        uiObject->GetNode()->GetWorldAABB().GetExtent(),
                    //                        uiObject->HasChildUIObjects(),
                    //                        uiObject->GetActualSizeClamped(),
                    //                        uiObject->GetComputedDepth());

                    if (mouseHoverEventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
                    {
                        break;
                    }
                }
            }
        }

        for (auto it = m_hoveredUiObjects.Begin(); it != m_hoveredUiObjects.End();)
        {
            const auto rayTestResultsIt = rayTestResults.FindAs(*it);

            if (rayTestResultsIt == rayTestResults.End())
            {
                if (Handle<UIObject> uiObject = it->Lock())
                {
                    uiObject->SetFocusState(uiObject->GetFocusState() & ~UIObjectFocusState::HOVER);

                    uiObject->OnMouseLeave(MouseEvent {
                        .inputManager = inputManager,
                        .position = uiObject->TransformScreenCoordsToRelative(mousePosition),
                        .previousPosition = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                        .absolutePosition = mousePosition,
                        .mouseButtons = inputManager->GetButtonStates() });
                }

                it = m_hoveredUiObjects.Erase(it);
            }
            else
            {
                ++it;
            }
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_DOWN:
    {
        // project a ray into the scene and test if it hits any objects
        RayHit hit;

        Array<Handle<UIObject>> rayTestResults;

        if (TestRay(mouseScreen, rayTestResults))
        {
            UIObject* firstHit = nullptr;

            for (auto it = rayTestResults.Begin(); it != rayTestResults.End(); ++it)
            {
                const Handle<UIObject>& uiObject = *it;

                if (!firstHit)
                {
                    firstHit = uiObject.Get();
                }

                // if (firstHit != nullptr) {
                //     // We don't want to check the current object if it's not a child of the first hit object,
                //     // since it would be behind the first hit object.
                //     if (!firstHit->IsOrHasParent(uiObject)) {
                //         continue;
                //     }
                // } else {
                //     firstHit = uiObject;
                // }

                auto mouseButtonPressedStatesIt = m_mouseButtonPressedStates.FindAs(uiObject);

                if (mouseButtonPressedStatesIt != m_mouseButtonPressedStates.End())
                {
                    if ((mouseButtonPressedStatesIt->second.mouseButtons & event.GetMouseButtons()) == event.GetMouseButtons())
                    {
                        // already holding buttons, go to next
                        continue;
                    }

                    mouseButtonPressedStatesIt->second.mouseButtons |= event.GetMouseButtons();
                }
                else
                {
                    if (!uiObject->AcceptsFocus() || !uiObject->IsEnabled())
                    {
                        continue;
                    }

                    mouseButtonPressedStatesIt = m_mouseButtonPressedStates.Set(uiObject, { event.GetMouseButtons(), 0.0f }).first;
                }

                mouseButtonPressedStatesIt->second.originalMousePosition = mouseScreen;
                mouseButtonPressedStatesIt->second.heldTime = 0.0f; // reset held time

                if (event.GetMouseButtons() & MouseButtonState::LEFT)
                {
                    uiObject->SetFocusState(uiObject->GetFocusState() | UIObjectFocusState::PRESSED);
                }

                const UIEventHandlerResult onMouseDownResult = uiObject->OnMouseDown(MouseEvent {
                    .inputManager = inputManager,
                    .position = uiObject->TransformScreenCoordsToRelative(mousePosition),
                    .previousPosition = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                    .absolutePosition = mousePosition,
                    .mouseButtons = mouseButtonPressedStatesIt->second.mouseButtons });

                eventHandlerResult |= onMouseDownResult;

                if (eventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
                {
                    break;
                }
            }

            if (firstHit != nullptr && firstHit->AcceptsFocus())
            {
                firstHit->Focus();
            }
        }

        break;
    }
    case SystemEventType::EVENT_MOUSEBUTTON_UP:
    {
        Array<Handle<UIObject>> rayTestResults;
        TestRay(mouseScreen, rayTestResults);

        const EnumFlags<MouseButtonState> buttons = event.GetMouseButtons();

        if (buttons == MouseButtonState::LEFT)
        {
            for (auto it = rayTestResults.Begin(); it != rayTestResults.End(); ++it)
            {
                const Handle<UIObject>& uiObject = *it;

                auto stateIt = m_mouseButtonPressedStates.Find(uiObject);

                if (stateIt == m_mouseButtonPressedStates.End() || !(stateIt->second.mouseButtons & MouseButtonState::LEFT))
                {
                    continue;
                }

                const EnumFlags<MouseButtonState> currentState = stateIt->second.mouseButtons;

                // check if we should trigger a click event
                if (uiObject->IsEnabled())
                {
                    const UIEventHandlerResult result = uiObject->OnClick(MouseEvent {
                        .inputManager = inputManager,
                        .position = uiObject->TransformScreenCoordsToRelative(mousePosition),
                        .previousPosition = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                        .absolutePosition = mousePosition,
                        .mouseButtons = buttons });

                    eventHandlerResult |= result;

                    if (result & UIEventHandlerResult::ERR)
                    {
                        HYP_LOG(UI, Error, "OnClick returned error: {}", result.GetMessage().GetOr("<No message>"));

                        break;
                    }

                    if (result & UIEventHandlerResult::STOP_BUBBLING)
                    {
                        break;
                    }
                }
            }
        }

        for (auto it = m_mouseButtonPressedStates.Begin(); it != m_mouseButtonPressedStates.End();)
        {
            EnumFlags<MouseButtonState>& stateMouseButtons = it->second.mouseButtons;

            if ((stateMouseButtons & buttons) == MouseButtonState::NONE) // no overlap; skip
            {
                ++it;

                continue;
            }

            // trigger mouse up
            if (Handle<UIObject> uiObject = it->first.Lock())
            {
                // No longer pressed if left mouse btn was released
                if ((buttons & MouseButtonState::LEFT) && (stateMouseButtons & MouseButtonState::LEFT))
                {
                    uiObject->SetFocusState(uiObject->GetFocusState() & ~UIObjectFocusState::PRESSED);
                }

                UIEventHandlerResult currentResult = uiObject->OnMouseUp(MouseEvent {
                    .inputManager = inputManager,
                    .position = uiObject->TransformScreenCoordsToRelative(mousePosition),
                    .previousPosition = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                    .absolutePosition = mousePosition,
                    .mouseButtons = (stateMouseButtons & ~buttons) });

                eventHandlerResult |= currentResult;

                stateMouseButtons &= (~buttons);
            }
            else
            {
                stateMouseButtons = MouseButtonState::NONE;
            }

            if (stateMouseButtons == MouseButtonState::NONE) // now empty after update; remove from the map
            {
                it = m_mouseButtonPressedStates.Erase(it);

                continue;
            }

            ++it;
        }

        break;
    }
    case SystemEventType::EVENT_MOUSESCROLL:
    {
        Vec2i wheel = event.GetMouseWheel();

        Array<Handle<UIObject>> rayTestResults;

        if (TestRay(mouseScreen, rayTestResults))
        {
            UIObject* firstHit = nullptr;

            for (auto it = rayTestResults.Begin(); it != rayTestResults.End(); ++it)
            {
                const Handle<UIObject>& uiObject = *it;

                // if (firstHit) {
                //     // We don't want to check the current object if it's not a child of the first hit object,
                //     // since it would be behind the first hit object.
                //     if (!firstHit->IsOrHasParent(uiObject)) {
                //         continue;
                //     }
                // } else {
                //     firstHit = uiObject;
                // }

                UIEventHandlerResult currentResult = uiObject->OnScroll(MouseEvent {
                    .inputManager = inputManager,
                    .position = uiObject->TransformScreenCoordsToRelative(mousePosition),
                    .previousPosition = uiObject->TransformScreenCoordsToRelative(previousMousePosition),
                    .absolutePosition = mousePosition,
                    .mouseButtons = inputManager->GetButtonStates(),
                    .wheel = wheel });

                eventHandlerResult |= currentResult;

                if (eventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
                {
                    break;
                }
            }
        }

        break;
    }
    case SystemEventType::EVENT_KEYDOWN:
    {
        const KeyCode keyCode = event.GetKeyCode();

        if (!m_keyedDownObjects.HasIndex(uint32(keyCode)))
        {
            m_keyedDownObjects.Emplace(uint32(keyCode));
        }

        auto& keyedDown = m_keyedDownObjects[uint32(keyCode)];

        Handle<UIObject> uiObject = m_focusedObject.Lock();

        while (uiObject != nullptr)
        {
            auto it = keyedDown.FindAs(uiObject);

            if (it == keyedDown.End() || ShouldTriggerKeyDownEvent(it->second))
            {
                // newly pressed, or we've been holding long enough that we should trigger another event
                UIObjectKeyState& keyState = keyedDown[uiObject];

                ++keyState.count;
                keyState.heldTime = 0.0f;

                UIEventHandlerResult currentResult = uiObject->OnKeyDown(KeyboardEvent {
                    .inputManager = inputManager,
                    .keyCode = keyCode });

                eventHandlerResult |= currentResult;

                if (eventHandlerResult & UIEventHandlerResult::STOP_BUBBLING)
                {
                    break;
                }
            }

            if (UIObject* parent = uiObject->GetParentUIObject())
            {
                uiObject = parent->HandleFromThis();
            }
            else
            {
                uiObject = nullptr;
            }
        }

        break;
    }
    case SystemEventType::EVENT_KEYUP:
    {
        const KeyCode keyCode = event.GetKeyCode();

        if (!m_keyedDownObjects.HasIndex(uint32(keyCode)))
        {
            break;
        }

        for (auto& it : m_keyedDownObjects[uint32(keyCode)])
        {
            WeakHandle<UIObject>& weakUiObject = it.first;

            if (Handle<UIObject> uiObject = weakUiObject.Lock())
            {
                uiObject->OnKeyUp(KeyboardEvent {
                    .inputManager = inputManager,
                    .keyCode = keyCode });
            }
        }

        m_keyedDownObjects.EraseAt(uint32(keyCode));

        break;
    }
    default:
        break;
    }

    return eventHandlerResult;
}

} // namespace hyperion
