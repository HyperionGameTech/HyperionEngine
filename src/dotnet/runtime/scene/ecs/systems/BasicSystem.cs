namespace Hyperion
{
    public class BasicSystem : SystemBase
    {
        public BasicSystem() : base(new ComponentDescriptor[]
        {
            new ComponentDescriptor
            {
                nativeTypeId = TypeID.ForType<TransformComponent>(),
                rwFlags = ComponentRWFlags.ReadWrite
            }
        })
        {
        }

        public override void Process(EntityManager entityManager, float deltaTime)
        {
        }
    }
}