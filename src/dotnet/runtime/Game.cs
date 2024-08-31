using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public abstract class Game
    {
        private Scene? scene;
        private InputManager? inputManager;
        private AssetManager? assetManager;
        private UIStage? uiStage;

        protected Scene Scene
        {
            get
            {
                return scene!;
            }
        }

        protected InputManager InputManager
        {
            get
            {
                return inputManager!;
            }
        }

        protected AssetManager AssetManager
        {
            get
            {
                return assetManager!;
            }
        }

        protected UIStage UIStage
        {
            get
            {
                return uiStage;
            }
        }

        /// <summary>
        /// Invoked from native code before the Init() is called.
        /// Sets up handles used by the Game instance.
        internal void BeforeInit(Scene scene, InputManager inputManager, AssetManager assetManager, UIStage uiStage)
        {
            this.scene = scene;
            this.inputManager = inputManager;
            this.assetManager = assetManager;
            this.uiStage = uiStage;

            Console.WriteLine("Game BeforeInit: this.scene = {0}, this.inputManager = {1}", this.scene, this.inputManager);

            HypClass boundingBoxClass = (HypClass)HypClass.GetClass("BoundingBox");
            // object boundingBoxTest = HypStructRegistry.CreateInstance(boundingBoxClass);
            // Console.WriteLine("BoundingBox Test = {0} {1}", boundingBoxTest, boundingBoxTest.GetType().Name);

            foreach (var field in boundingBoxClass.Fields)
            {
                Console.WriteLine("Field Name = {0}, Type = {1}, Offset = {2}", field.Name, field.TypeID, field.Offset);
            }

            Console.WriteLine("uiStage.Position = {0}", uiStage.GetPosition());
            Console.WriteLine("uiStage.ActualSize = {0}", uiStage.GetActualSize());

            Entity newEntity = this.scene.EntityManager.AddEntity();
            Console.WriteLine("New Entity = {0}", newEntity.ID);

            Console.WriteLine("this.scene.ID = {0}", this.scene.ID);
            Console.WriteLine("this.scene.World = {0}", this.scene.World);
            Console.WriteLine("this.scene.World.ID = {0}", this.scene.World.ID);
            Console.WriteLine("this.scene.World.GameTime = {0}", this.scene.World.GetGameTime());

            HypData testArray = new HypData(new float[] { 1.0f, 2.0f, 3.0f, 4.0f });
            Console.WriteLine("TestArray = {0}", testArray.GetValue());

            var mesh = new Mesh();
            Console.WriteLine("Mesh NumIndices = {0}", mesh.NumIndices);
            Console.WriteLine("Mesh AABB = {0}", mesh.AABB);
            mesh.AABB = new BoundingBox(new Vec3f(1.0f, 2.0f, 3.0f), new Vec3f(4.0f, 5.0f, 6.0f));
            Console.WriteLine("Mesh AABB = {0}", mesh.AABB);

            // HypMethod? testMethod = mesh.HypClass.GetMethod(Name.FromString("InvertNormals"));

            // if (testMethod != null)
            // {
            //     HypData testMethodResult = ((HypMethod)testMethod).Invoke(mesh);
            //     Console.WriteLine("TestMethod result = {0}", testMethodResult.GetValue());
            // }

            // // iterate over methods
            // foreach (var method in mesh.HypClass.Methods)
            // {
            //     Console.WriteLine("Game BeforeInit: mesh.HypClass.Method = {0}", method.Name);

            //     // iterate over parameters
            //     foreach (var parameter in method.Parameters)
            //     {
            //         Console.WriteLine("Game BeforeInit: mesh.HypClass.Method = {0}, parameter = {1}", method.Name, parameter.TypeID);
            //     }
            // }

            foreach (var property in this.scene.HypClass.Properties)
            {
                var result = property.InvokeGetter(this.scene);
                var camera = result.GetValue();

                if (camera != null)
                {
                    Console.WriteLine("Camera = {0}, type = {1}", camera, camera.GetType().Name);
                    // Console.WriteLine("Game BeforeInit: mesh.HypClass.Property = {0}, result = {1}, type = {2} ({3}), size = {4}  {5}", property.Name, result, result.Type, result.Type.TypeName, result.TotalSize, result.Type.HypClass);
                }
                // Console.WriteLine("{0} = {1} Value (Camera) = {2}", property.Name, result, camera);
                // Console.WriteLine("Game BeforeInit: mesh.HypClass.Property = {0}, result = {1}, type = {2} ({3}), size = {4}  {5}", property.Name, result, result.Type, result.Type.TypeName, result.TotalSize, result.Type.HypClass);
            }

            Console.WriteLine("this.inputManager.HypClass = {0} {1}", this.inputManager.HypClass, this.inputManager.HypClass.Name);
            Console.WriteLine("this.assetManager.HypClass = {0} {1}", this.assetManager.HypClass, this.assetManager.HypClass.Name);

            Console.WriteLine("Game BeforeInit: this.scene = {0}, this.inputManager = {1}", this.scene, this.inputManager);
        }

        public abstract void Init();
        public abstract void Update(float deltaTime);
    }
}