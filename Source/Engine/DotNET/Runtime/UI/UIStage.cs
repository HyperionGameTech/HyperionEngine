using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="UIStage")]
    public class UIStage : UIObject
    {
        public UIStage() : base()
        {
            
        }

        public Vec2i SurfaceSize
        {
            get
            {
                return (Vec2i)this.GetSurfaceSize();
            }
            set
            {
                this.SetSurfaceSize(value);
            }
        }

        public T CreateUIObject<T>(Name name, Vec2i position, Vec2i size, bool attachToRoot) where T : UIObject, new()
        {
            throw new NotImplementedException();
            // if (!createUIObjectMethods.TryGetValue(typeof(T), out Func<SharedPtr, Name, Vec2i, Vec2i, bool, SharedPtr> method))
            // {
            //     throw new Exception("Failed to create UI object");
            // }

            // SharedPtr result = method(refCountedPtr, name, position, size, attachToRoot);

            // if (!result.IsValid)
            // {
            //     throw new Exception("Failed to create UI object");
            // }

            // return UIObjectHelpers.MarshalUIObject(result) as T;
        }
    }
}