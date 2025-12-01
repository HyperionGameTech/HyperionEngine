using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EditorState")]
    public class EditorState : ObjectBase
    {
        private static EditorState? instance = null;

        public EditorState()
        {    
        }

        public static EditorState Instance
        {
            get
            {
                if (instance == null)
                {
                    using (HypDataBuffer resultData = ObjectBase.GetMethod(Class.GetClass(typeof(EditorState)), new Name("GetInstance", weak: true)).InvokeNative())
                    {
                        instance = (EditorState)resultData.GetValue();
                    }
                }

                return (EditorState)instance;
            }
        }

        public EditorProject? CurrentProject
        {
            get
            {
                return this.GetCurrentProject();
            }
            set
            {
                this.SetCurrentProject(value);
            }
        }
    }
}