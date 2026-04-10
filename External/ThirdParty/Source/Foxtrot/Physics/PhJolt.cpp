#include "PhJolt.hpp"

#include <ThirdParty/Jolt/Jolt.h>

#include <Core/FxLog.hpp>
#include <Core/FxTypes.hpp>

/* Additional Jolt includes */
#include <ThirdParty/Jolt/Core/Factory.h>
#include <ThirdParty/Jolt/Core/JobSystemThreadPool.h>
#include <ThirdParty/Jolt/Core/TempAllocator.h>
#include <ThirdParty/Jolt/Physics/Body/BodyActivationListener.h>
#include <ThirdParty/Jolt/Physics/Body/BodyCreationSettings.h>
#include <ThirdParty/Jolt/Physics/Collision/Shape/BoxShape.h>
#include <ThirdParty/Jolt/Physics/Collision/Shape/SphereShape.h>
#include <ThirdParty/Jolt/Physics/PhysicsSettings.h>
#include <ThirdParty/Jolt/Physics/PhysicsSystem.h>
#include <ThirdParty/Jolt/RegisterTypes.h>

#include <cstdarg>

// Callback for traces, connect this to your own trace function if you have one
static void JoltTrace(const char* fmt, ...)
{
    // Format the message
    va_list list;
    va_start(list, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, list);
    va_end(list);

    // Print to the TTY
    FxLogInfo("{}", buffer);
}


using namespace JPH::literals;


void PhJolt::Update()
{
    if (bPhysicsPaused) {
        return;
    }

    // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the
    // simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
    const int collision_steps = 1;

    PhysicsSystem.Update(cTimeStep, collision_steps, pTempAllocator.pPtr, pJobSystem.pPtr);
}

void PhJolt::OptimizeBroadPhase() { PhysicsSystem.OptimizeBroadPhase(); }

void PhJolt::Create()
{
    if (mbIsInited) {
        FxLogWarning("PhJolt is already initialized!");
        return;
    }

    JPH::RegisterDefaultAllocator();
    JPH::Trace = JoltTrace;

    JPH::Factory::sInstance = new JPH::Factory();

    JPH::RegisterTypes();

    pTempAllocator.InitRef(10 * FxUnitMebibyte);

    pJobSystem.InitRef(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, JPH::thread::hardware_concurrency() - 1);

    const uint32 max_bodies = 1024;
    const uint32 num_body_mutexes = 0;
    const uint32 max_body_pairs = 1024;

    const uint32 max_contact_constraints = 1024;

    // Now we can create the actual physics system.
    PhysicsSystem.Init(max_bodies, num_body_mutexes, max_body_pairs, max_contact_constraints, mBroadPhaseInterface,
                       mObjectVsBPLayerFilter, mObjectLayerPairFilter);


    // A body activation listener gets notified when bodies activate and go to sleep
    // Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    PhysicsSystem.SetBodyActivationListener(&mBodyActivationListener);

    // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
    // Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    PhysicsSystem.SetContactListener(&mContactListener);

    // The main way to interact with the bodies in the physics system is through the body interface. There is a locking
    // and a non-locking variant of this. We're going to use the locking version (even though we're not planning to
    // access bodies from multiple threads)
    // JPH::BodyInterface& body_interface = PhysicsSystem.GetBodyInterface();

    // // Next we can create a rigid body to serve as the floor, we make a large box
    // // Create the settings for the collision volume (the shape).
    // // Note that for simple shapes (like boxes) you can also directly construct a BoxShape.
    // JPH::BoxShapeSettings floor_shape_settings(JPH::Vec3(100.0f, 1.0f, 100.0f));
    // floor_shape_settings.SetEmbedded(); // A ref counted object on the stack (base class RefTarget) should be marked
    // as
    //                                     // such to prevent it from being freed when its reference count goes to 0.

    // // Create the shape
    // JPH::ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
    // JPH::ShapeRefC floor_shape = floor_shape_result.Get(); // We don't expect an error here, but you can check
    //                                                        // floor_shape_result for HasError() / GetError()

    // // Create the settings for the body itself. Note that here you can also set other properties like the restitution
    // /
    // // friction.
    // JPH::BodyCreationSettings floor_settings(floor_shape, JPH::RVec3(0.0_r, -1.0_r, 0.0_r), JPH::Quat::sIdentity(),
    //                                          JPH::EMotionType::Static, PhLayer::Static);

    // // Create the actual rigid body
    // JPH::Body* floor = body_interface.CreateBody(
    //     floor_settings); // Note that if we run out of bodies this can return nullptr

    // // Add it to the world
    // body_interface.AddBody(floor->GetID(), JPH::EActivation::DontActivate);

    // // Now create a dynamic body to bounce on the floor
    // // Note that this uses the shorthand version of creating and adding a body to the world
    // JPH::BodyCreationSettings sphere_settings(new JPH::SphereShape(0.5f), JPH::RVec3(0.0_r, 2.0_r, 0.0_r),
    //                                           JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
    //                                           PhLayer::Dynamic);
    // JPH::BodyID sphere_id = body_interface.CreateAndAddBody(sphere_settings, JPH::EActivation::Activate);

    // Now you can interact with the dynamic body, in this case we're going to give it a velocity.
    // (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to
    // the physics system)
    // body_interface.SetLinearVelocity(sphere_id, JPH::Vec3(0.0f, -5.0f, 0.0f));

    // Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision
    // detection performance (it's pointless here because we only have 2 bodies). You should definitely not call this
    // every frame or when e.g. streaming in a new level section as it is an expensive operation. Instead insert all new
    // objects in batches instead of 1 at a time to keep the broad phase efficient.
    // PhysicsSystem.OptimizeBroadPhase();

    mbIsInited = true;
}

void PhJolt::Destroy()
{
    if (!mbIsInited) {
        return;
    }

    JPH::UnregisterTypes();

    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

PhJolt::PhJolt() {}

PhJolt::~PhJolt()
{
    Destroy();

    mbIsInited = false;
}
