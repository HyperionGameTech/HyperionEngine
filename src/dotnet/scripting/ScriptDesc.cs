using System;
using System.IO;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "ScriptCompileStatus")]
    [Flags]
    public enum ScriptCompileStatus : uint
    {
        Uninitialized = 0x0,
        Compiled = 0x1,
        Dirty = 0x2,
        Processing = 0x4,
        Errored = 0x8
    }


    [ClassBinding(Name = "ScriptLanguage")]
    public enum ScriptLanguage : uint
    {
        HypScript = 0,
        CSharp = 1
    }

    [ClassBinding(Name = "ScriptDesc")]
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct ScriptDesc
    {
        private Guid guid;

        private ScriptLanguage language;

        private fixed byte path[1024];

        private fixed byte assemblyPath[1024];

        private fixed byte className[1024];

        [MarshalAs(UnmanagedType.U4)]
        private uint compileStatus;

        [MarshalAs(UnmanagedType.U4)]
        public int hotReloadVersion;

        [MarshalAs(UnmanagedType.U8)]
        public ulong lastModifiedTimestamp;

        public Guid Guid
        {
            get
            {
                return guid;
            }
            set
            {
                guid = value;
            }
        }

        public string Path
        {
            get
            {
                fixed (byte* p = path)
                {
                    return Marshal.PtrToStringAnsi((IntPtr)p);
                }
            }
            set
            {
                fixed (byte* p = path)
                {
                    byte[] bytes = System.Text.Encoding.ASCII.GetBytes(value);
                    Marshal.Copy(bytes, 0, (IntPtr)p, bytes.Length);
                }
            }
        }

        public string AssemblyPath
        {
            get
            {
                fixed (byte* p = assemblyPath)
                {
                    return Marshal.PtrToStringAnsi((IntPtr)p);
                }
            }
            set
            {
                fixed (byte* p = assemblyPath)
                {
                    byte[] bytes = System.Text.Encoding.ASCII.GetBytes(value);
                    Marshal.Copy(bytes, 0, (IntPtr)p, bytes.Length);
                }
            }
        }

        public string ClassName
        {
            get
            {
                fixed (byte* p = className)
                {
                    return Marshal.PtrToStringAnsi((IntPtr)p);
                }
            }
            set
            {
                fixed (byte* p = className)
                {
                    byte[] bytes = System.Text.Encoding.ASCII.GetBytes(value);
                    Marshal.Copy(bytes, 0, (IntPtr)p, bytes.Length);
                }
            }
        }

        public ScriptCompileStatus CompileStatus
        {
            get
            {
                return (ScriptCompileStatus)compileStatus;
            }
            set
            {
                compileStatus = (uint)value;
            }
        }

        public int HotReloadVersion
        {
            get
            {
                return hotReloadVersion;
            }
            set
            {
                hotReloadVersion = value;
            }
        }

        public ulong LastModifiedTimestamp
        {
            get
            {
                return lastModifiedTimestamp;
            }
            set
            {
                lastModifiedTimestamp = value;
            }
        }
    }

    public class ScriptInstance
    {
        private IntPtr ptr;

        public ScriptInstance(ScriptDesc script)
        {
            this.ptr = ScriptData_AllocateNativeObject(ref script);
        }

        public ScriptInstance(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        ~ScriptInstance()
        {
            if (IsValid)
            {
                ScriptData_FreeNativeObject(ref Get());
            }
        }

        public IntPtr Address
        {
            get
            {
                return ptr;
            }
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public bool IsErrored
        {
            get
            {
                if (!IsValid)
                {
                    return false;
                }

                return (Get().CompileStatus & ScriptCompileStatus.Errored) != 0;
            }
        }

        public bool IsDirty
        {
            get
            {
                if (!IsValid)
                {
                    return false;
                }

                return (Get().CompileStatus & ScriptCompileStatus.Dirty) != 0;
            }
        }

        public bool IsProcessing
        {
            get
            {
                if (!IsValid)
                {
                    return false;
                }

                return (Get().CompileStatus & ScriptCompileStatus.Processing) != 0;
            }
        }

        public unsafe ref ScriptDesc Get()
        {
            if (!IsValid)
            {
                throw new InvalidOperationException("ScriptInstance is not initialized");
            }

            return ref System.Runtime.CompilerServices.Unsafe.AsRef<ScriptDesc>(ptr.ToPointer());
        }

        public void UpdateState()
        {
            if (!IsValid || IsErrored)
            {
                return;
            }

            ref ScriptDesc scriptData = ref Get();

            if (!File.Exists(scriptData.Path))
            {
                scriptData.CompileStatus |= ScriptCompileStatus.Errored;

                return;
            }

            ulong lastModifiedTimestamp = (ulong)(new FileInfo(scriptData.Path).LastWriteTimeUtc - new DateTime(1970, 1, 1)).TotalSeconds;

            if (lastModifiedTimestamp > scriptData.LastModifiedTimestamp)
            {
                scriptData.CompileStatus |= ScriptCompileStatus.Dirty;
                scriptData.LastModifiedTimestamp = lastModifiedTimestamp;
            }
        }

        [DllImport("hyperion", EntryPoint = "ScriptData_AllocateNativeObject")]
        private static extern IntPtr ScriptData_AllocateNativeObject([In] ref ScriptDesc inScriptData);

        [DllImport("hyperion", EntryPoint = "ScriptData_FreeNativeObject")]
        private static extern void ScriptData_FreeNativeObject([In] ref ScriptDesc inScriptData);
    }
}