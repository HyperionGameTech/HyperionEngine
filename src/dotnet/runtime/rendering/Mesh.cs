using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class Mesh : IDisposable
    {
        private ManagedHandle handle;

        public Mesh(List<Vertex> vertices, List<uint> indices)
        {
            var verticesArray = vertices.ToArray();
            var indicesArray = indices.ToArray();

            handle = new ManagedHandle();
            
            Mesh_Create(verticesArray, (uint)vertices.Count, indicesArray, (uint)indices.Count, out handle);
        }

        public Mesh(ManagedHandle handle)
        {
            this.handle = handle;
            this.handle.IncRef(Mesh_GetTypeID());
        }

        public void Dispose()
        {
            handle.DecRef(Mesh_GetTypeID());
        }

        public void Init()
        {
            Mesh_Init(handle);
        }

        public ManagedHandle Handle
        {
            get
            {
                return handle;
            }
        }

        public uint ID
        {
            get
            {
                return handle.id;
            }
        }

        public BoundingBox AABB
        {
            get
            {
                BoundingBox aabb = new BoundingBox();
                Mesh_GetAABB(handle, out aabb);
                return aabb;
            }
        }

        [DllImport("hyperion", EntryPoint = "Mesh_GetTypeID")]
        [return: MarshalAs(UnmanagedType.Struct, SizeConst = 4)]
        private static extern TypeID Mesh_GetTypeID();

        [DllImport("hyperion", EntryPoint = "Mesh_Create")]
        private static extern void Mesh_Create(Vertex[] vertices, uint vertexCount, uint[] indices, uint indexCount, [Out] out ManagedHandle handle);

        [DllImport("hyperion", EntryPoint = "Mesh_Init")]
        private static extern void Mesh_Init(ManagedHandle mesh);

        [DllImport("hyperion", EntryPoint = "Mesh_GetAABB")]
        private static extern void Mesh_GetAABB(ManagedHandle mesh, [Out] out BoundingBox aabb);
    }
}