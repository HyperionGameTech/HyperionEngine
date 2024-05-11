using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public abstract class UIEventHandler : Script
    {
        private UIStage? uiStage;
        private UIObject? uiObject;

        public override void Init(Entity entity)
        {
            base.Init(entity);

            uiObject = Scene.EntityManager.GetComponent<UIComponent>(Entity).UIObject;
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
                return uiObject!;
            }
        }

        [Hyperion.ScriptMethodStub]
        public virtual UIEventHandlerResult OnMouseDown()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        public virtual UIEventHandlerResult OnMouseUp()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        public virtual UIEventHandlerResult OnMouseHover()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        public virtual UIEventHandlerResult OnMouseLeave()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        public virtual UIEventHandlerResult OnMouseDrag()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        public virtual UIEventHandlerResult OnMouseMove()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        public virtual UIEventHandlerResult OnGainFocus()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        public virtual UIEventHandlerResult OnLoseFocus()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        public virtual UIEventHandlerResult OnScroll()
        {
            return UIEventHandlerResult.Ok;
        }

        [Hyperion.ScriptMethodStub]
        public virtual UIEventHandlerResult OnClick()
        {
            return UIEventHandlerResult.Ok;
        }
    }
}