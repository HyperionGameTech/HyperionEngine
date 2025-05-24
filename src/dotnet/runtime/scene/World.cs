using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="World")]
    public class World : HypObject
    {
        public World()
        {
        }

        public T? GetSubsystem<T>() where T : Subsystem
        {
            HypClass? hypClass = HypClass.TryGetClass<T>();

            if (hypClass == null)
            {
                throw new InvalidOperationException($"Type {typeof(T).Name} has no associated HypClass.");
            }

            return this.GetSubsystemByName(hypClass.Value.Name) as T;
        }
    }
}