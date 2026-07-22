/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

 #pragma once

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class World;
class View;
class EnvProbe;
class ProbeVolume;
class Light;
class VolumeBase;
class PassData;

struct Viewport;

/*! \brief Describes the setup for rendering a frame.  */
struct RenderSetup
{
    friend const RenderSetup& NullRenderSetup();

    World* world = nullptr;
    View* view = nullptr;
    EnvProbe* envProbe = nullptr;
    ProbeVolume* probeVolume = nullptr;
    Light* light = nullptr;
    VolumeBase* volume = nullptr;

    Swapchain* swapchain = nullptr;

    // override render target to use this framebuffer, even if View has an OutputTarget.
    Framebuffer* framebuffer = nullptr;
    Viewport viewport;

    PassData* passData = nullptr;

    RenderSetup* prev = nullptr;

public:
    RenderSetup() = default;

    explicit RenderSetup(World* world)
        : world(world)
    {
    }

    RenderSetup(World* world, View* view)
        : world(world),
          view(view)
    {
    }

    RenderSetup(const RenderSetup& other) = default;
    RenderSetup& operator=(const RenderSetup& other) = default;

    RenderSetup(RenderSetup&& other) noexcept = default;
    RenderSetup& operator=(RenderSetup&& other) noexcept = default;

    ~RenderSetup() = default;

    /*! \brief Returns true if this RenderSetup has a valid World set. */
    HYP_FORCE_INLINE bool HasWorld() const
    {
        return world != nullptr;
    }

    /*! \brief Returns true if this RenderSetup has a valid View set. */
    HYP_FORCE_INLINE bool HasView() const
    {
        return view != nullptr;
    }

    /*! \brief Creates a forked RenderSetup that has this RenderSetup as its previous setup.
     *  This is useful for creating nested RenderSetups that can refer back to their parent setup if needed. */
    RenderSetup Fork() const
    {
        RenderSetup forked = *this;
        forked.prev = const_cast<RenderSetup*>(this);
        return forked;
    }
};

/*! \brief Special null RenderSetup that can be used for simple rendering tasks that don't make sense to use a World, such as rendering texture mipmaps.
 *  \internal Use sparingly as most rendering tasks should have a valid World and using this will cause the IsValid() check to return false */
extern const RenderSetup& NullRenderSetup();


} // namespace Hyperion