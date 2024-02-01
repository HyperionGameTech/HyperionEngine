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

            Console.WriteLine("Hello from C#, scene ID: " + sceneID);

            // print address of engine instance to test
            Console.WriteLine("Hello from C#, world ID: " + Engine.Instance.World.ID);

            Node root = scene.Root;

            Node myNode = root.AddChild();
            myNode.LocalTranslation += new Vec3f(1, 2, 3);
            myNode.LocalTranslation += new Vec3f(1, 2, 3);
            myNode.LocalTranslation += new Vec3f(1, 2, 3);

            Console.WriteLine("Hello from C#, myNode name: " + myNode.Name);
            Console.WriteLine("Hello from C#, myNode local transform: " + myNode.LocalTransform);
            Console.WriteLine("Hello from C#, myNode world transform: " + myNode.WorldTransform);
        }
    }
}