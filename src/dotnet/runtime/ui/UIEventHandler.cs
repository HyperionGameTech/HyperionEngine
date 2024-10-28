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
        private UIStage? uiStage;
        private UIObject? uiObject;

        public override void Init(IDBase entity)
        {
            base.Init(entity);

            uiObject = Scene.GetEntityManager().GetComponent<UIComponent>(Entity).UIObject;

            if (uiObject == null)
            {
                throw new InvalidOperationException("UIObject is null");
            }
        }

        protected UIStage UIStage
        {
            get
            {
                return uiStage!;
            }
        }
        
        public UIObject UIObject
        {
            get
            {
                throw new NotImplementedException();
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
        public virtual UIEventHandlerResult OnMouseDown()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnMouseUp()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnMouseHover()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnMouseLeave()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnMouseDrag()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnMouseMove()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnGainFocus()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnLoseFocus()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnScroll()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnClick()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnKeyDown()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        [UIEvent(AllowNested = false)]
        public virtual UIEventHandlerResult OnKeyUp()
        {
            return UIEventHandlerResult.Ok;
        }
    }
}