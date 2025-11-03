using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="Time")]
    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public struct Time
    {
        // Value in milliseconds
        private ulong value;

        public Time()
        {
            value = 0;
        }

        public Time(ulong value)
        {
            this.value = value;
        }

        public static Time Now()
        {
            return new Time(Time_Now());
        }

        public static implicit operator ulong(Time time)
        {
            return time.value;
        }

        public static implicit operator Time(ulong value)
        {
            return new Time(value);
        }

        // Converts from milliseconds since unix epoch to a DateTime
        public static implicit operator DateTime(Time time)
        {
            return new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc).AddMilliseconds(time.value);
        }

        [DllImport("hyperion", EntryPoint = "Time_Now")]
        private static extern ulong Time_Now();
    }
}