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
        private MessageBoxType _type;
        private string _title;
        private string _message;
        private MessageBoxButton[] _buttons;

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
            _type = type;
            _title = "";
            _message = "";
            _buttons = Array.Empty<MessageBoxButton>();
        }

        public MessageBox(MessageBoxType type, string title, string message, MessageBoxButton[]? buttons = null)
        {
            _type = type;
            _title = title;
            _message = message;
            _buttons = buttons ?? Array.Empty<MessageBoxButton>();
        }

        public MessageBox Title(string title)
        {
            _title = title;

            return this;
        }

        public MessageBox Text(string text)
        {
            _message = text;

            return this;
        }

        public MessageBox Button(string text, MessageBoxButton.OnClick onClick)
        {
            if (_buttons.Length >= 3)
            {
                throw new InvalidOperationException("MessageBox can only have up to 3 buttons.");
            }

            MessageBoxButton button = new MessageBoxButton();
            button.text = text;
            button.onClick = onClick;

            MessageBoxButton[] newButtons = new MessageBoxButton[_buttons.Length + 1];

            for (int i = 0; i < _buttons.Length; i++)
            {
                newButtons[i] = _buttons[i];
            }

            newButtons[_buttons.Length] = button;

            _buttons = newButtons;

            return this;
        }

        public void Show()
        {
            IntPtr buttonTexts = Marshal.AllocHGlobal(_buttons.Length * IntPtr.Size);
            IntPtr buttonFunctionPointers = Marshal.AllocHGlobal(_buttons.Length * IntPtr.Size);

            for (int i = 0; i < _buttons.Length; i++)
            {
                IntPtr buttonText = Marshal.StringToHGlobalAnsi(_buttons[i].text);
                IntPtr buttonFunctionPointer = Marshal.GetFunctionPointerForDelegate(_buttons[i].onClick);

                Marshal.WriteIntPtr(buttonTexts, i * IntPtr.Size, buttonText);
                Marshal.WriteIntPtr(buttonFunctionPointers, i * IntPtr.Size, buttonFunctionPointer);
            }

            MessageBox_Show((int)_type, _title, _message, _buttons.Length, buttonTexts, buttonFunctionPointers);

            for (int i = 0; i < _buttons.Length; i++)
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