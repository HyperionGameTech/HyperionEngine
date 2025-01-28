namespace Hyperion
{
    public class Assert
    {
        public static void Throw(bool condition, string? message = null, params object[] args)
        {
            if (!condition)
            {
                if (message == null || ((string)message).Length == 0)
                {
                    throw new Exception("Assertion failed");
                }

                string formattedMessage = string.Format((string)message, args ?? Array.Empty<object>());

                throw new Exception(formattedMessage);
            }
        }
    }
}