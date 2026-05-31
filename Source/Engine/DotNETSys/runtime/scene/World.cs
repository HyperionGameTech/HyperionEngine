using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "WorldFlags")]
    [Flags]
    public enum WorldFlags : uint
    {
        None = 0x0,

        EditorWorld = 0x1,

        HasPhysics = 0x2,
        HasStreaming = 0x4,

        HasSceneStreamingLayer = 0x100,
        AllStreamingLayerFlags = HasSceneStreamingLayer,

        Default = HasPhysics | HasStreaming | AllStreamingLayerFlags
    }


    [ClassBinding(Name = "World")]
    public class World : AssetObject
    {
        public World()
        {
        }

        public Name Name
        {
            get => this.GetName();
            set => this.SetName(value);
        }

        public WorldGrid WorldGrid => this.GetWorldGrid(); // extension method

        public WorldFlags WorldFlags
        {
            get => this.GetWorldFlags();
            set => this.SetWorldFlags(value);
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

        public void AddSubsystem<T>(T subsystem) where T : Subsystem
        {
            if (!this.TryAddSubsystem(subsystem))
            {
                throw new InvalidOperationException($"Failed to add subsystem of type {typeof(T).Name} to World.");
            }
        }
    }
}