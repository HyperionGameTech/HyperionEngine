using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum MessageBoxType : int
    {
        Info = 0,
        Warning = 1,
        Critical = 2
    }

    public struct MessageBoxButton
    {
        public delegate void OnClick();

        public string text;
        public OnClick onClick;
    }

    public class MessageBox
    {
        private MessageBoxType type;
        private string title;
        private string message;
        private MessageBoxButton[] buttons;

        public static MessageBox Info(string title = "", string message = "")
        {
            return new MessageBox(MessageBoxType.Info, title, message);
        }

        public static MessageBox Warning(string title = "", string message = "")
        {
            return new MessageBox(MessageBoxType.Warning, title, message);
        }

        public static MessageBox Critical(string title = "", string message = "")
        {
            return new MessageBox(MessageBoxType.Critical, title, message);
        }

        public MessageBox(MessageBoxType type)
        {
            this.type = type;
            this.title = "";
            this.message = "";
            this.buttons = new MessageBoxButton[0];
        }

        public MessageBox(MessageBoxType type, string title, string message, MessageBoxButton[] buttons = null)
        {
            this.type = type;
            this.title = title;
            this.message = message;
            this.buttons = buttons ?? new MessageBoxButton[0];
        }

        public MessageBox Title(string title)
        {
            this.title = title;

            return this;
        }

        public MessageBox Text(string text)
        {
            this.message = text;

            return this;
        }

        public MessageBox Button(string text, MessageBoxButton.OnClick onClick)
        {
            if (buttons.Length >= 3)
            {
                throw new InvalidOperationException("MessageBox can only have up to 3 buttons.");
            }

            MessageBoxButton button = new MessageBoxButton();
            button.text = text;
            button.onClick = onClick;

            MessageBoxButton[] newButtons = new MessageBoxButton[buttons.Length + 1];

            for (int i = 0; i < buttons.Length; i++)
            {
                newButtons[i] = buttons[i];
            }

            newButtons[buttons.Length] = button;

            buttons = newButtons;

            return this;
        }

        public void Show()
        {
            IntPtr buttonTexts = Marshal.AllocHGlobal(buttons.Length * IntPtr.Size);
            IntPtr buttonFunctionPointers = Marshal.AllocHGlobal(buttons.Length * IntPtr.Size);

            for (int i = 0; i < buttons.Length; i++)
            {
                IntPtr buttonText = Marshal.StringToHGlobalAnsi(buttons[i].text);
                IntPtr buttonFunctionPointer = Marshal.GetFunctionPointerForDelegate(buttons[i].onClick);

                Marshal.WriteIntPtr(buttonTexts, i * IntPtr.Size, buttonText);
                Marshal.WriteIntPtr(buttonFunctionPointers, i * IntPtr.Size, buttonFunctionPointer);
            }

            MessageBox_Show((int)type, title, message, buttons.Length, buttonTexts, buttonFunctionPointers);

            for (int i = 0; i < buttons.Length; i++)
            {
                IntPtr buttonText = Marshal.ReadIntPtr(buttonTexts, i * IntPtr.Size);

                Marshal.FreeHGlobal(buttonText);
            }

            Marshal.FreeHGlobal(buttonTexts);
            Marshal.FreeHGlobal(buttonFunctionPointers);
        }

        [DllImport("hyperion", EntryPoint = "MessageBox_Show")]
        private static extern void MessageBox_Show(int type, [MarshalAs(UnmanagedType.LPStr)] string title, [MarshalAs(UnmanagedType.LPStr)] string message, int buttons, IntPtr buttonTexts, IntPtr buttonFunctionPointers);
    }
}