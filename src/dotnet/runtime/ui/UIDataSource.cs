using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="UIDataSourceBase")]
    public abstract class UIDataSourceBase : HypObject
    {
        public UIDataSourceBase()
        {
        }

        public void Push(UUID uuid, object value, UUID? parentUuid = null)
        {
            HypData hypData = new HypData(value);

            Push(uuid, ref hypData.Buffer, parentUuid);

            hypData.Dispose();
        }

        public void Push(UUID uuid, ref HypDataBuffer buffer, UUID? parentUuid = null)
        {
            UUID parentUuidOrDefault = parentUuid ?? UUID.Invalid;

            UIDataSourceBase_Push(NativeAddress, ref uuid, ref buffer, ref parentUuidOrDefault);
        }

        [DllImport("hyperion", EntryPoint="UIDataSourceBase_Push")]
        private static extern void UIDataSourceBase_Push([In] IntPtr uiDataSource, [In] ref UUID uuid, [In] ref HypDataBuffer data, [In] ref UUID parentUUID);
    }

    [HypClassBinding(Name="UIDataSource")]
    public class UIDataSource : UIDataSourceBase
    {
        public UIDataSource()
        {
        }
        
        public UIDataSource(TypeID elementTypeId, UIElementFactoryBase factory)
        {
            UIDataSource_SetElementTypeIDAndFactory(NativeAddress, ref elementTypeId, factory.NativeAddress);
        }

        [DllImport("hyperion", EntryPoint="UIDataSource_SetElementTypeIDAndFactory")]
        private static extern void UIDataSource_SetElementTypeIDAndFactory([In] IntPtr uiDataSource, [In] ref TypeID elementTypeId, [In] IntPtr elementFactory);
    }

    [HypClassBinding(Name="UIElementFactoryBase")]
    public abstract class UIElementFactoryBase : HypObject
    {
        public UIElementFactoryBase()
        {
        }

        public abstract UIObject CreateUIObject(UIObject parent, object value, object context);
        public abstract void UpdateUIObject(UIObject uiObject, object value, object context);
    }
}