using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="InputHandlerBase")]
    public abstract class InputHandlerBase : HypObject
    {
        public InputHandlerBase()
        {
        }

        public virtual bool OnKeyDown(KeyboardEvent evt)
        {
            return this.OnKeyDown_Impl(evt);
        }

        public virtual bool OnKeyUp(KeyboardEvent evt)
        {
            return this.OnKeyUp_Impl(evt);
        }

        public virtual bool OnMouseDown(MouseEvent evt)
        {
            return this.OnMouseDown_Impl(evt);
        }

        public virtual bool OnMouseUp(MouseEvent evt)
        {
            return this.OnMouseUp_Impl(evt);
        }

        public virtual bool OnMouseMove(MouseEvent evt)
        {
            return this.OnMouseMove_Impl(evt);
        }

        public virtual bool OnMouseDrag(MouseEvent evt)
        {
            return this.OnMouseDrag_Impl(evt);
        }

        public virtual bool OnClick(MouseEvent evt)
        {
            return this.OnClick_Impl(evt);
        }
    }

    [HypClassBinding(Name="NullInputHandler")]
    public class NullInputHandler : InputHandlerBase
    {
        public NullInputHandler()
        {
        }

        public override bool OnKeyDown(KeyboardEvent evt)
        {
            return false;
        }

        public override bool OnKeyUp(KeyboardEvent evt)
        {
            return false;
        }

        public override bool OnMouseDown(MouseEvent evt)
        {
            return false;
        }

        public override bool OnMouseUp(MouseEvent evt)
        {
            return false;
        }

        public override bool OnMouseMove(MouseEvent evt)
        {
            return false;
        }

        public override bool OnMouseDrag(MouseEvent evt)
        {
            return false;
        }

        public override bool OnClick(MouseEvent evt)
        {
            return false;
        }
    }
}