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

            
            bool childRemoved = root.RemoveChild(myNode);
            Logger.Log(LogType.Info, "Hello from C#, childRemoved: {0}", childRemoved);
            // see if this breaks:
            Logger.Log(LogType.Info, "Hello from C#, myNode name: {0}", myNode.Name);
        }
    }
}