using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "UIDataSourceBase")]
    public abstract class UIDataSourceBase : ObjectBase
    {
        public UIDataSourceBase()
        {
        }

        public void Push(UUID uuid, object value, UUID? parentUuid = null)
        {
            BoxedValueInternal boxedInternal = new BoxedValueInternal();
            boxedInternal.SetValue(value);

            Push(uuid, ref boxedInternal, parentUuid);

            boxedInternal.Dispose();
        }

        public void Push(UUID uuid, ref BoxedValueInternal buffer, UUID? parentUuid = null)
        {
            UUID parentUuidOrDefault = parentUuid ?? UUID.Invalid;

            UIDataSourceBase_Push(NativeAddress, ref uuid, ref buffer, ref parentUuidOrDefault);
        }

        [DllImport("hyperion", EntryPoint = "UIDataSourceBase_Push")]
        private static extern void UIDataSourceBase_Push([In] IntPtr uiDataSource, [In] ref UUID uuid, [In] ref BoxedValueInternal data, [In] ref UUID parentUUID);
    }

    [ClassBinding(Name = "UIDataSource")]
    public class UIDataSource : UIDataSourceBase
    {
        public UIDataSource()
        {
        }

        public UIDataSource(TypeId elementTypeId, UIElementFactoryBase factory)
        {
            UIDataSource_SetElementFactory(NativeAddress, ref elementTypeId, factory.NativeAddress);
        }

        [DllImport("hyperion", EntryPoint = "UIDataSource_SetElementFactory")]
        private static extern void UIDataSource_SetElementFactory([In] IntPtr uiDataSource, [In] ref TypeId elementTypeId, [In] IntPtr elementFactory);
    }

    [ClassBinding(Name = "UIElementFactoryBase")]
    public abstract class UIElementFactoryBase : ObjectBase
    {
        public UIElementFactoryBase()
        {
        }

        public abstract UIObject CreateUIObject(UIObject parent, object value, object context);
        public abstract void UpdateUIObject(UIObject uiObject, object value, object context);
    }
}