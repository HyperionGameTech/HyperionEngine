using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EnvGrid")]
    public class EnvGrid : Entity
    {
        public EnvGrid()
        {
        }
    }

    [ClassBinding(Name = "LegacyEnvGrid")]
    public class LegacyEnvGrid : Entity
    {
        public LegacyEnvGrid()
        {
        }
    }
}