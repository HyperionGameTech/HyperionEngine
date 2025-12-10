using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Sequential, Size = 4)]
    public struct MethodParameter
    {
        private TypeId _typeId;

        public TypeId TypeId => _typeId;
    }

    [Flags]
    public enum MethodFlags : byte
    {
        None = 0x0,
        Static = 0x1,
        Member = 0x2
    }

    public struct Method
    {
        public static readonly Method Invalid = new Method(IntPtr.Zero);

        internal IntPtr _ptr;

        internal Method(IntPtr ptr)
        {
            _ptr = ptr;
        }

        public Name Name
        {
            get
            {
                Name name;
                Method_GetName(_ptr, out name);
                return name;
            }
        }

        public TypeId ReturnTypeId
        {
            get
            {
                TypeId returnTypeId;
                Method_GetReturnTypeId(_ptr, out returnTypeId);
                return returnTypeId;
            }
        }

        public IEnumerable<MethodParameter> Parameters
        {
            get
            {
                IntPtr pParams;
                uint count = Method_GetParameters(_ptr, out pParams);

                for (int i = 0; i < count; i++)
                {
                    MethodParameter param = Marshal.PtrToStructure<MethodParameter>(pParams + i * Marshal.SizeOf<MethodParameter>());
                    yield return param;
                }
            }
        }

        public MethodFlags Flags
        {
            get
            {
                return (MethodFlags)Method_GetFlags(_ptr);
            }
        }

        public bool IsStatic
        {
            get
            {
                return (Flags & MethodFlags.Static) != 0;
            }
        }

        public bool IsMemberFunction
        {
            get
            {
                return (Flags & MethodFlags.Member) != 0;
            }
        }

        public HypDataBuffer InvokeNativeWithThis(ObjectBase thisObject, object[]? args = null)
        {
            if (_ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke method: Invalid method");
            }

            uint numArgs = args == null ? 1 : (uint)args.Length + 1;

            if (!IsMemberFunction)
            {
                throw new InvalidOperationException("Cannot invoke method: Method is not a member function");
            }

            IntPtr pParams;
            uint numParams = Method_GetParameters(_ptr, out pParams);

            if (numArgs != numParams)
            {
                throw new InvalidOperationException("Cannot invoke method: Invalid number of arguments - expected " + numParams + " but got " + numArgs);
            }

            bool shouldStackAlloc = numArgs * Marshal.SizeOf<HypDataBuffer>() < 1024;

            Span<HypDataBuffer> hypDataArgsBuffers = shouldStackAlloc
                ? stackalloc HypDataBuffer[(int)numArgs]
                : new HypDataBuffer[(int)numArgs];

            int argIndex = 0;

            HypDataBuffer.HypData_Construct(ref hypDataArgsBuffers[argIndex]);
            hypDataArgsBuffers[argIndex].SetValue(thisObject);
            argIndex++;

            if (numArgs > 1)
            {
                for (; argIndex < numArgs; argIndex++)
                {
                    HypDataBuffer.HypData_Construct(ref hypDataArgsBuffers[argIndex]);
                    hypDataArgsBuffers[argIndex].SetValue(args[argIndex - 1]);
                }
            }

            HypDataBuffer resultBuffer;

            // Args is pointer to contiguous HypDataBuffer objects
            unsafe
            {
                fixed (HypDataBuffer* pArgs = hypDataArgsBuffers)
                {
                    bool result = Method_Invoke(_ptr, (IntPtr)pArgs, numArgs, out resultBuffer);

                    for (int i = 0; i < numArgs; i++)
                        hypDataArgsBuffers[i].Dispose();

                    if (!result)
                        throw new InvalidOperationException("Failed to invoke method");
                }
            }

            return resultBuffer;
        }

        public HypDataBuffer InvokeNative(params object[] args)
        {
            if (_ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Cannot invoke method: Invalid method");
            }

            uint numArgs = (uint)args.Length;

            ObjectBase? thisObject = null;

            if (IsMemberFunction)
            {
                if (args.Length == 0)
                {
                    throw new InvalidOperationException("Cannot invoke method: Method is a member function but no thisObject was provided");
                }

                thisObject = args[0] as ObjectBase;

                if (thisObject == null)
                {
                    throw new InvalidOperationException("Cannot invoke method: Invalid thisObject");
                }
            }

            IntPtr pParams;
            uint numParams = Method_GetParameters(_ptr, out pParams);

            if (numArgs != numParams)
            {
                throw new InvalidOperationException("Cannot invoke method: Invalid number of arguments - expected " + numParams + " but got " + numArgs);
            }

            bool shouldStackAlloc = numArgs * Marshal.SizeOf<HypDataBuffer>() < 1024;

            Span<HypDataBuffer> hypDataArgsBuffers = numArgs > 0
                ? (shouldStackAlloc ? stackalloc HypDataBuffer[(int)numArgs] : new HypDataBuffer[(int)numArgs])
                : Span<HypDataBuffer>.Empty;

            if (numArgs > 0)
            {
                int argIndex = 0;

                if (thisObject != null)
                {
                    HypDataBuffer.HypData_Construct(ref hypDataArgsBuffers[argIndex]);
                    hypDataArgsBuffers[argIndex].SetValue(thisObject);
                    argIndex++;
                }

                for (; argIndex < numArgs; argIndex++)
                {
                    HypDataBuffer.HypData_Construct(ref hypDataArgsBuffers[argIndex]);
                    hypDataArgsBuffers[argIndex].SetValue(args[argIndex]);
                }
            }

            HypDataBuffer resultBuffer;

            // Args is pointer to contiguous HypDataBuffer objects
            unsafe
            {
                fixed (HypDataBuffer* pArgs = hypDataArgsBuffers)
                {
                    bool result = Method_Invoke(_ptr, (IntPtr)pArgs, numArgs, out resultBuffer);

                    for (int i = 0; i < numArgs; i++)
                        hypDataArgsBuffers[i].Dispose();

                    if (!result)
                        throw new InvalidOperationException("Failed to invoke method");
                }
            }

            return resultBuffer;
        }

        public HypData Invoke(params object[] args)
        {
            return HypData.FromBuffer(InvokeNative(args));
        }

        [DllImport("hyperion", EntryPoint = "Method_GetName")]
        private static extern void Method_GetName([In] IntPtr methodPtr, [Out] out Name name);

        [DllImport("hyperion", EntryPoint = "Method_GetReturnTypeId")]
        private static extern void Method_GetReturnTypeId([In] IntPtr methodPtr, [Out] out TypeId returnTypeId);

        [DllImport("hyperion", EntryPoint = "Method_GetParameters")]
        private static extern uint Method_GetParameters([In] IntPtr methodPtr, [Out] out IntPtr outParamsPtr);

        [DllImport("hyperion", EntryPoint = "Method_GetFlags")]
        private static extern byte Method_GetFlags([In] IntPtr methodPtr);

        [DllImport("hyperion", EntryPoint = "Method_Invoke")]
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern unsafe bool Method_Invoke([In] IntPtr methodPtr, [In] IntPtr argsPtr, uint numArgs, [Out] out HypDataBuffer outResult);
    }
}