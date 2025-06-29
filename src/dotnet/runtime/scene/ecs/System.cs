using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    /// <summary>
    /// Represents a C++ system in the ECS (Entity Component System) architecture. To create a custom system, derive from ScriptableSystem instead of this class.
    /// </summary>
    [HypClassBinding(Name = "SystemBase")]
    public class SystemBase : HypObject
    {
        internal SystemBase()
        {
        }
    }

    [HypClassBinding(Name = "ScriptableSystem")]
    public abstract class ScriptableSystem : SystemBase
    {
        public ScriptableSystem()
        {
        }

        protected abstract ComponentInfo[] GetComponentInfos();

        /// <summary>
        /// Determines whether this system allows parallel execution. By default, managed systems do not allow parallel execution.
        /// Override this method in derived classes to enable parallel execution.
        /// </summary>
        public virtual bool AllowParallelExecution()
        {
            return false;
        }

        /// <summary>
        /// Determines whether this system requires execution on the game thread. By default, managed systems require the game thread.
        /// Override this method in derived classes to change this behavior.
        /// </summary>
        public virtual bool RequiresGameThread()
        {
            return true;
        }

        /// <summary>
        /// Determines whether this system has its Process() method called on each tick.
        /// Override this method in derived classes to turn off this behavior, to reduce overhead for systems that do not need to run every tick.
        /// </summary>
        public virtual bool AllowUpdate()
        {
            return true;
        }

        /// <summary>
        /// Called when an Entity is added to the EntityManager that matches the system's component descriptors, or when all necessary components for the system are added to an existing entity.
        /// </summary>
        /// <param name="entity">The Entity that was added.</param>
        public virtual void OnEntityAdded(Entity entity)
        {
            InvokeNativeMethod(new Name("OnEntityAdded_Impl", weak: true), new object[] { entity });
        }

        /// <summary>
        /// Called when an Entity is removed from the EntityManager that matches the system's component descriptors, or when any of the components required by the system are removed from the entity.
        /// </summary>
        /// <param name="entityId">The Id of the Entity that was removed.</param>
        public virtual void OnEntityRemoved(ObjIdBase entityId)
        {
            InvokeNativeMethod(new Name("OnEntityRemoved_Impl", weak: true), new object[] { entityId });
        }

        /// <summary>
        /// Called by the EntityManager when the system is added. Ensure the base class implementation is called to properly initialize the system.
        /// </summary>
        public virtual void Init()
        {
            InvokeNativeMethod(new Name("Init_Impl", weak: true));
        }

        /// <summary>
        /// Called by the EntityManager when the system is removed. Ensure the base class implementation is called to properly clean up the system.
        /// </summary>
        public virtual void Shutdown()
        {
            InvokeNativeMethod(new Name("Shutdown_Impl", weak: true));
        }

        /// <summary>
        /// Processes the system logic for the current tick. This method is called on each tick if AllowUpdate() returns true.
        /// </summary>
        /// <param name="delta">The time elapsed since the last tick, in seconds.</param>
        public abstract void Process(float delta);
    }
}
