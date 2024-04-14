using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public abstract class UIEventHandler : Script
    {
        private UIObject? uiObject;

        public override void Init(Entity entity)
        {
            base.Init(entity);

            uiObject = Scene.EntityManager.GetComponent<UIComponent>(Entity).UIObject;
        }
        
        public UIObject UIObject
        {
            get
            {
                return uiObject!;
            }
        }

        [Hyperion.ScriptMethodStub]
        public virtual bool OnMouseDown()
        {
            return false;
        }

        [Hyperion.ScriptMethodStub]
        public virtual bool OnMouseUp()
        {
            return false;
        }

        [Hyperion.ScriptMethodStub]
        public virtual bool OnMouseHover()
        {
            return false;
        }

        [Hyperion.ScriptMethodStub]
        public virtual bool OnMouseLeave()
        {
            return false;
        }

        [Hyperion.ScriptMethodStub]
        public virtual bool OnMouseDrag()
        {
            return false;
        }

        [Hyperion.ScriptMethodStub]
        public virtual bool OnClick()
        {
            return false;
        }
    }
}