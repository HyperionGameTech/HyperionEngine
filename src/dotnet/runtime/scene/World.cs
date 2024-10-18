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
            HypClassBinding? hypClassBindingAttribute = HypClassBinding.ForType(typeof(T));

            if (hypClassBindingAttribute == null)
            {
                throw new InvalidOperationException($"Type {typeof(T).Name} is not a HypObject");
            }

            Name name = new Name(hypClassBindingAttribute.Name, weak: true);

            return this.GetSubsystemByName(name) as T;
        }
    }
}