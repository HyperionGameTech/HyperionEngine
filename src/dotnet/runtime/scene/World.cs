using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "WorldFlags")]
    [Flags]
    public enum WorldFlags : uint
    {
        None = 0x0,

        IsEditorWorld = 0x1,

        HasPhysics = 0x2,
        HasStreaming = 0x4,

        HasSceneStreamingLayer = 0x100,
        AllStreamingLayerFlags = HasSceneStreamingLayer,

        Default = HasPhysics | HasStreaming | AllStreamingLayerFlags
    }


    [ClassBinding(Name = "World")]
    public class World : ObjectBase
    {
        public World()
        {
        }

        public T? GetSubsystem<T>() where T : Subsystem
        {
            Class? cls = Class.TryGetClass<T>();

            if (cls == null)
            {
                throw new InvalidOperationException($"Type {typeof(T).Name} has no associated Class.");
            }

            return this.GetSubsystemByName(cls.Value.Name) as T;
        }
    }
}