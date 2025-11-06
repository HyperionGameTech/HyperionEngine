using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
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