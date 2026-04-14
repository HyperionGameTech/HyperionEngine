using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AssetPath")]
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct AssetPath
    {
        public static readonly AssetPath Invalid = new AssetPath();

        private Name* chain;

        public AssetPath()
        {
            chain = null;
        }

        public bool Valid => chain != null;

        public override string ToString()
        {
            /// Same impl as native AssetPath::ToString().
            /// @TODO: if/when AssetPath replaces using Name array chain with offset + length into string table,
            /// Replace this with calling the native extension method.

            if (chain == null)
                return string.Empty;

            string str = "";

            Name* curr = chain;

            while (curr->HashCode != 0)
            {
                if (curr != chain)
                {
                    str += '/';
                }

                str += curr->ToString();

                ++curr;
            }

            return str;
        }
    }
}