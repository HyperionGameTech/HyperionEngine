using System;
using System.Runtime.InteropServices;

using Hyperion;

public delegate void MyDelegate();

namespace MyNamespace
{
    public class MyClass
    {
        public static MyDelegate GetMyMethodDelegate()
        {
            return MyMethod;
        }

        public static void MyMethod()
        {
            Scene scene = new Scene();
            var sceneID = scene.ID;

            Logger.Log(LogType.Info, "Hello from C#, scene ID: " + sceneID);

            // print address of engine instance to test
            Logger.Log(LogType.Info, "Hello from C#, world ID: " + Engine.Instance.World.ID);

            Node root = scene.Root;

            Node myNode = root.AddChild();
            myNode.LocalTranslation += new Vec3f(1, 2, 3);
            myNode.LocalTranslation += new Vec3f(1, 2, 3);
            myNode.LocalTranslation += new Vec3f(1, 2, 3);
            myNode.Name = "Foo bar baggins";

            Logger.Log(LogType.Info, "Hello from C#, myNode name: " + myNode.Name);
            Logger.Log(LogType.Info, "Hello from C#, myNode local transform: " + myNode.LocalTransform);
            Logger.Log(LogType.Info, "Hello from C#, myNode world transform: " + myNode.WorldTransform);


            // test EntityManager
            EntityManager entityManager = scene.EntityManager;

            Entity entity = entityManager.AddEntity();

            // add transform component
            var transformComponentId = scene.EntityManager.AddComponent<TransformComponent>(entity, new TransformComponent
            {
                transform = new Transform
                {
                    Translation = new Vec3f(1, 2, 3),
                    Scale = new Vec3f(1, 1, 1),
                    Rotation = new Quaternion(0, 0, 0, 1)
                }
            });

            var transformComponent0 = entityManager.GetComponent<TransformComponent>(entity);
            transformComponent0.transform.Translation += new Vec3f(10, 10, 10);

            Logger.Log(LogType.Info, "Hello from C#, entity ID: {0}", entity.ID);
            Logger.Log(LogType.Info, "Hello from C#, entity has transform component: {0}", entityManager.HasComponent<TransformComponent>(entity));
            Logger.Log(LogType.Info, "Hello from C#, transformComponentId: {0}", transformComponentId);     

            // try getting the transform component
            var transformComponent1 = entityManager.GetComponent<TransformComponent>(entity);
            Logger.Log(LogType.Info, "Hello from C#, transformComponent: {0}", transformComponent1.transform);
            myNode.Entity = entity;

            // move node; should move entity
            myNode.LocalTranslation += new Vec3f(20, 20, 20);

            // test entity translation after moving node
            var transformComponent2 = entityManager.GetComponent<TransformComponent>(entity);
            Logger.Log(LogType.Info, "Hello from C#, transformComponent: {0}", transformComponent2.transform);

            List<Vertex> vertices = new List<Vertex>
            {
                new Vertex(new Vec3f(0, 0, 0), new Vec2f(0, 0), new Vec3f(0, 0, 1)),
                new Vertex(new Vec3f(1, 0, 0), new Vec2f(1, 0), new Vec3f(0, 0, 1)),
                new Vertex(new Vec3f(1, 1, 0), new Vec2f(1, 1), new Vec3f(0, 0, 1)),
                new Vertex(new Vec3f(0, 1, 0), new Vec2f(0, 1), new Vec3f(0, 0, 1))
            };

            List<uint> indices = new List<uint>
            {
                0, 1, 2,
                2, 3, 0
            };

            var meshComponentId = scene.EntityManager.AddComponent<MeshComponent>(entity, new MeshComponent
            {
                Mesh = new Mesh(vertices, indices),
                Material = new Material()
            });

            var meshComponent = entityManager.GetComponent<MeshComponent>(entity);
            Logger.Log(LogType.Info, "Hello from C#, meshComponentId: {0}", meshComponentId);
            Logger.Log(LogType.Info, "Hello from C#, mesh component: {0}", meshComponent);
            meshComponent.Mesh.Init();
            meshComponent.Material[MaterialKey.Albedo] = new MaterialParameter(1.0f, 0.0f, 0.0f, 1.0f);
            meshComponent.Material[MaterialKey.Roughness] = new MaterialParameter(0.5f);
            Logger.Log(LogType.Info, "Hello from C#, material param albedo type: {0}", meshComponent.Material[MaterialKey.Albedo].Type);
            Logger.Log(LogType.Info, "Hello from C#, material param albedo: {0}", meshComponent.Material[MaterialKey.Albedo]);

            meshComponent.Material.Textures[TextureKey.AlbedoMap] = new Texture();
            meshComponent.Material.Textures[TextureKey.AlbedoMap].Init();
            Logger.Log(LogType.Info, "Hello from C#, material texture albedo map: {0}", meshComponent.Material.Textures[TextureKey.AlbedoMap].ID);
            
            bool childRemoved = root.RemoveChild(myNode);
            Logger.Log(LogType.Info, "Hello from C#, childRemoved: {0}", childRemoved);
            // see if this breaks:
            Logger.Log(LogType.Info, "Hello from C#, myNode name: {0}", myNode.Name);
        }
    }
}