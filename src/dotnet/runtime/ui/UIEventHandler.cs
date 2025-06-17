using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [AttributeUsage(AttributeTargets.Method, Inherited = true)]
    public class UIEvent : Attribute
    {
        public bool AllowNested { get; set; } = false;
    }

    public abstract class UIEventHandler : Script
    {
        private UIObject? uiObject;

        public override void Init(Entity entity)
        {
            base.Init(entity);

            uiObject = Scene.GetEntityManager().GetComponent<UIComponent>(entity).UIObject;

            if (uiObject == null)
            {
                throw new InvalidOperationException("UIObject is null");
            }
        }
        
        public UIObject UIObject
        {
            get
            {
                Assert.Throw(uiObject != null, "UIObject is null");

                return uiObject!;
            }
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnInit()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnAttached()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnRemoved()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnChildAttached(UIObject child)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnChildRemoved(UIObject child)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnMouseDown(MouseEvent mouseEvent)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnMouseUp(MouseEvent mouseEvent)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnMouseHover(MouseEvent mouseEvent)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnMouseLeave(MouseEvent mouseEvent)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnMouseDrag(MouseEvent mouseEvent)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnMouseMove(MouseEvent mouseEvent)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnGainFocus(MouseEvent mouseEvent)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnLoseFocus(MouseEvent mouseEvent)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnScroll(MouseEvent mouseEvent)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnClick(MouseEvent mouseEvent)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnKeyDown(KeyboardEvent keyboardEvent)
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnKeyUp(KeyboardEvent keyboardEvent)
        {
            return UIEventHandlerResult.Ok;
        }
    }
}