using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="UIDataSourceBase")]
    public abstract class UIDataSourceBase : HypObject
    {
        public UIDataSourceBase()
        {
        }

        public void Push(Uuid uuid, object value, Uuid? parentUuid = null)
        {
            HypDataBuffer hypDataBuffer = new HypDataBuffer();
            hypDataBuffer.SetValue(value);

            Push(uuid, ref hypDataBuffer, parentUuid);

            hypDataBuffer.Dispose();
        }

        public void Push(Uuid uuid, ref HypDataBuffer buffer, Uuid? parentUuid = null)
        {
            Uuid parentUuidOrDefault = parentUuid ?? Uuid.Invalid;

            UIDataSourceBase_Push(NativeAddress, ref uuid, ref buffer, ref parentUuidOrDefault);
        }

        [DllImport("hyperion", EntryPoint="UIDataSourceBase_Push")]
        private static extern void UIDataSourceBase_Push([In] IntPtr uiDataSource, [In] ref Uuid uuid, [In] ref HypDataBuffer data, [In] ref Uuid parentUUID);
    }

    [ClassBinding(Name="UIDataSource")]
    public class UIDataSource : UIDataSourceBase
    {
        public UIDataSource()
        {
        }
        
        public UIDataSource(TypeId elementTypeId, UIElementFactoryBase factory)
        {
            UIDataSource_SetElementFactory(NativeAddress, ref elementTypeId, factory.NativeAddress);
        }

        [DllImport("hyperion", EntryPoint="UIDataSource_SetElementFactory")]
        private static extern void UIDataSource_SetElementFactory([In] IntPtr uiDataSource, [In] ref TypeId elementTypeId, [In] IntPtr elementFactory);
    }

    [ClassBinding(Name="UIElementFactoryBase")]
    public abstract class UIElementFactoryBase : HypObject
    {
        public UIElementFactoryBase()
        {
        }

        public abstract UIObject CreateUIObject(UIObject parent, object value, object context);
        public abstract void UpdateUIObject(UIObject uiObject, object value, object context);
    }
}