using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class UIStage : UIObject
    {
        // This dictionary is used to map the generic type of the UI object to the corresponding method that creates the UI object
        private static readonly Dictionary<Type, Func<RefCountedPtr, Name, Vec2i, Vec2i, bool, RefCountedPtr>> createUIObjectMethods = new Dictionary<Type, Func<RefCountedPtr, Name, Vec2i, Vec2i, bool, RefCountedPtr>>
        {
            {
                typeof(UIButton), (RefCountedPtr rc, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIButton(rc, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIText), (RefCountedPtr rc, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIText(rc, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIPanel), (RefCountedPtr rc, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIPanel(rc, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIImage), (RefCountedPtr rc, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIImage(rc, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UITabView), (RefCountedPtr rc, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UITabView(rc, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UITab), (RefCountedPtr rc, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UITab(rc, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIGrid), (RefCountedPtr rc, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIGrid(rc, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIMenuBar), (RefCountedPtr rc, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIMenuBar(rc, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIMenuItem), (RefCountedPtr rc, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIMenuItem(rc, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIDockableItem), (RefCountedPtr rc, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIDockableItem(rc, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIDockableContainer), (RefCountedPtr rc, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIDockableContainer(rc, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            }
        };

        public UIStage() : base()
        {
                
        }

        public UIStage(RefCountedPtr refCountedPtr) : base(refCountedPtr)
        {
        }

        public Scene Scene
        {
            get
            {
                ManagedHandle sceneHandle = new ManagedHandle();
                UIStage_GetScene(refCountedPtr, out sceneHandle);
                return new Scene(sceneHandle);
            }
        }

        public Vec2i SurfaceSize
        {
            get
            {
                Vec2i size = new Vec2i();
                UIStage_GetSurfaceSize(refCountedPtr, out size);
                return size;
            }
        }

        public T CreateUIObject<T>(Name name, Vec2i position, Vec2i size, bool attachToRoot) where T : UIObject, new()
        {
            if (!createUIObjectMethods.TryGetValue(typeof(T), out Func<RefCountedPtr, Name, Vec2i, Vec2i, bool, RefCountedPtr> method))
            {
                throw new Exception("Failed to create UI object");
            }

            RefCountedPtr result = method(refCountedPtr, name, position, size, attachToRoot);

            if (!result.IsValid)
            {
                throw new Exception("Failed to create UI object");
            }

            return UIObjectHelpers.MarshalUIObject(result) as T;
        }

        [DllImport("hyperion", EntryPoint = "UIStage_GetScene")]
        private static extern void UIStage_GetScene(RefCountedPtr rc, [Out] out ManagedHandle sceneHandle);

        [DllImport("hyperion", EntryPoint = "UIStage_GetSurfaceSize")]
        private static extern void UIStage_GetSurfaceSize(RefCountedPtr rc, [Out] out Vec2i size);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIButton")]
        private static extern void UIStage_CreateUIObject_UIButton(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIText")]
        private static extern void UIStage_CreateUIObject_UIText(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIPanel")]
        private static extern void UIStage_CreateUIObject_UIPanel(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIImage")]
        private static extern void UIStage_CreateUIObject_UIImage(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UITabView")]
        private static extern void UIStage_CreateUIObject_UITabView(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UITab")]
        private static extern void UIStage_CreateUIObject_UITab(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIGrid")]
        private static extern void UIStage_CreateUIObject_UIGrid(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIGridRow")]
        private static extern void UIStage_CreateUIObject_UIGridRow(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIGridColumn")]
        private static extern void UIStage_CreateUIObject_UIGridColumn(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIMenuBar")]
        private static extern void UIStage_CreateUIObject_UIMenuBar(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIMenuItem")]
        private static extern void UIStage_CreateUIObject_UIMenuItem(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIDockableItem")]
        private static extern void UIStage_CreateUIObject_UIDockableItem(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIDockableContainer")]
        private static extern void UIStage_CreateUIObject_UIDockableContainer(RefCountedPtr rc, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);
    }
}