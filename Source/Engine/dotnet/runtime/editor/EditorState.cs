using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EditorState")]
    public class EditorState : ObjectBase
    {
        private static EditorState? _instance = null;

        public static EditorState Instance
        {
            get
            {
                if (_instance == null)
                {
                    using (BoxedValueInternal resultData = ObjectBase.GetMethod(Class.GetClass(typeof(EditorState)), new Name("GetInstance", weak: true)).InvokeNative())
                    {
                        _instance = (EditorState?)resultData.GetValue();

                        if (_instance == null)
                        {
                            throw new Exception("Failed to get EditorState instance");
                        }
                    }
                }

                return _instance;
            }
        }

        public EditorState()
        {    
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

        public Node? ClipboardNode
        {
            get => this.GetClipboardNode();
        }
    }
}