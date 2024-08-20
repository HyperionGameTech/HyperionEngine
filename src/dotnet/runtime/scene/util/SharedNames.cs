using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class PropertyNames
    {
        public static Name ID { get; } = Name.FromString("ID", weak: true);
        public static Name Name { get; } = Name.FromString("Name", weak: true);
        public static Name World { get; } = Name.FromString("World", weak: true);
        public static Name GameTime { get; } = Name.FromString("GameTime", weak: true);
        public static Name NumIndices { get; } = Name.FromString("NumIndices", weak: true);
        public static Name AABB { get; } = Name.FromString("AABB", weak: true);
        public static Name Root { get; } = Name.FromString("Root", weak: true);
        public static Name EntityManager { get; } = Name.FromString("EntityManager", weak: true);
        public static Name Entity { get; } = Name.FromString("Entity", weak: true);
        public static Name EntityAABB { get; } = Name.FromString("EntityAABB", weak: true);
        public static Name Camera { get; } = Name.FromString("Camera", weak: true);
        public static Name Mesh { get; } = Name.FromString("Mesh", weak: true);
        public static Name Scene { get; } = Name.FromString("Scene", weak: true);
        public static Name Transform { get; } = Name.FromString("Transform", weak: true);
        public static Name LocalTransform { get; } = Name.FromString("LocalTransform", weak: true);
        public static Name WorldTransform { get; } = Name.FromString("WorldTransform", weak: true);
        public static Name BasePath { get; } = Name.FromString("BasePath", weak: true);
        public static Name OriginAlignment { get; } = Name.FromString("OriginAlignment", weak: true);
        public static Name ParentAlignment { get; } = Name.FromString("ParentAlignment", weak: true);
        public static Name Position { get; } = Name.FromString("Position", weak: true);
        public static Name ActualSize { get; } = Name.FromString("ActualSize", weak: true);
        public static Name Size { get; } = Name.FromString("Size", weak: true);
    }

    public class MethodNames
    {
        public static Name AddEntity { get; } = Name.FromString("AddEntity", weak: true);
        public static Name RemoveEntity { get; } = Name.FromString("RemoveEntity", weak: true);
        public static Name HasEntity { get; } = Name.FromString("HasEntity", weak: true);
        public static Name GetSubsystem { get; } = Name.FromString("GetSubsystem", weak: true);
        public static Name GetInstance { get; } = Name.FromString("GetInstance", weak: true);
    }
}