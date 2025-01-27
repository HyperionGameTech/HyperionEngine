namespace Hyperion
{
    public class Assert
    {
        public static void Throw(bool condition, string message, params object[] args)
        {
            if (!condition)
            {
                string formattedMessage = string.Format(message, args);

                Logger.Log(LogType.Error, "Assertion failed: {0}", formattedMessage);

                throw new Exception(formattedMessage);
            }
        }
    }
}