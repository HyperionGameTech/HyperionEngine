namespace Hyperion
{
    public enum ComponentRWFlags : uint
    {
        None = 0x0,
        Read = 0x1,
        Write = 0x2,
        ReadWrite = 0x3
    }

    public struct ComponentDescriptor
    {
        public TypeID nativeTypeId;
        public ComponentRWFlags rwFlags;
    }

    public abstract class SystemBase
    {
        private ComponentDescriptor[] componentDescriptors;
        public SystemBase(ComponentDescriptor[] componentDescriptors)
        {
            this.componentDescriptors = componentDescriptors;
        }

        public ComponentDescriptor[] ComponentDescriptors
        {
            get
            {
                return componentDescriptors;
            }
        }

        public abstract void Process(EntityManager entityManager, float deltaTime);
    }
}