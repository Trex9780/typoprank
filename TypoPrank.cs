using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

internal sealed class TypoPrank : ApplicationContext
{
    private const int WhKeyboardLl = 13;
    private const int WmKeyDown = 0x100;
    private const int WmKeyUp = 0x101;
    private const int WmSysKeyDown = 0x104;
    private const int WmSysKeyUp = 0x105;
    private const uint Injected = 0x10;
    private const uint KeyUp = 0x02;
    private const byte Released = 254;
    private const byte Passed = 255;

    // shortcut: uses QWERTY neighbors, detect the active layout if another keyboard layout matters.
    private static readonly string[] Rows = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
    private readonly Random random = new Random();
    private readonly byte[] heldReplacement = new byte[256];
    private readonly HookProc callback;
    private readonly NotifyIcon tray;
    private readonly ToolStripMenuItem pauseItem;
    private IntPtr hook;
    private bool paused;
    private int lettersUntilTypo;
    private int previousLetter;
    private int variedLetters;

    [STAThread]
    private static void Main()
    {
        bool first;
        using (var instance = new Mutex(true, "Local\\TypoPrank-8B2D7795", out first))
        {
            if (!first) return;
            Application.EnableVisualStyles();
            Application.Run(new TypoPrank());
        }
    }

    private TypoPrank()
    {
        lettersUntilTypo = random.Next(12, 36);
        pauseItem = new ToolStripMenuItem("Pause", null, delegate { TogglePause(); });
        var menu = new ContextMenuStrip();
        menu.Items.Add(pauseItem);
        menu.Items.Add("Exit", null, delegate { ExitThread(); });
        tray = new NotifyIcon
        {
            Icon = SystemIcons.Information,
            Text = "Typo Prank - occasional letter swaps",
            ContextMenuStrip = menu,
            Visible = true
        };
        callback = OnKey;
        hook = SetWindowsHookEx(WhKeyboardLl, callback, GetModuleHandle(null), 0);
        if (hook == IntPtr.Zero)
        {
            MessageBox.Show("Could not start the keyboard prank. Windows error " + Marshal.GetLastWin32Error(),
                "Typo Prank", MessageBoxButtons.OK, MessageBoxIcon.Error);
            ExitThread();
        }
    }

    private void TogglePause()
    {
        paused = !paused;
        previousLetter = 0;
        variedLetters = 0;
        pauseItem.Text = paused ? "Resume" : "Pause";
        tray.Text = paused ? "Typo Prank - paused" : "Typo Prank - occasional letter swaps";
        if (paused)
        {
            for (int key = 0; key < heldReplacement.Length; key++)
            {
                if (heldReplacement[key] > 0 && heldReplacement[key] < Released)
                {
                    InjectKey(heldReplacement[key], true);
                    heldReplacement[key] = Released;
                }
            }
        }
    }

    private IntPtr OnKey(int code, IntPtr message, IntPtr data)
    {
        if (code < 0) return CallNextHookEx(hook, code, message, data);
        var key = (KeyboardEvent)Marshal.PtrToStructure(data, typeof(KeyboardEvent));
        if ((key.flags & Injected) != 0) return CallNextHookEx(hook, code, message, data);
        int vk = (int)key.vkCode;
        bool down = message == (IntPtr)WmKeyDown || message == (IntPtr)WmSysKeyDown;
        bool up = message == (IntPtr)WmKeyUp || message == (IntPtr)WmSysKeyUp;
        if (vk < 0 || vk >= heldReplacement.Length || (!down && !up))
            return CallNextHookEx(hook, code, message, data);

        if (up)
        {
            byte replacement = heldReplacement[vk];
            heldReplacement[vk] = 0;
            if (replacement > 0 && replacement < Released)
            {
                InjectKey(replacement, true);
                return (IntPtr)1;
            }
            if (replacement == Released) return (IntPtr)1;
        }
        else if (down)
        {
            byte replacement = heldReplacement[vk];
            if (replacement == Passed) return CallNextHookEx(hook, code, message, data);
            if (replacement == Released) return (IntPtr)1;
            if (replacement != 0)
            {
                if (!ModifierDown()) InjectKey(replacement, false);
                return (IntPtr)1;
            }
            if (vk < 'A' || vk > 'Z' || ModifierDown())
            {
                if (vk != 0x10 && vk != 0xA0 && vk != 0xA1)
                {
                    previousLetter = 0;
                    variedLetters = 0;
                }
            }
            else
            {
                variedLetters = vk == previousLetter ? 1 : Math.Min(3, variedLetters + 1);
                previousLetter = vk;
                if (!paused && variedLetters >= 3 && --lettersUntilTypo == 0)
                {
                    lettersUntilTypo = random.Next(12, 36);
                    replacement = ReplacementFor(vk, random.Next(2) == 0);
                    if (!Down(replacement) && InjectKey(replacement, false))
                    {
                        heldReplacement[vk] = replacement;
                        return (IntPtr)1;
                    }
                }
            }
            heldReplacement[vk] = Passed;
        }
        return CallNextHookEx(hook, code, message, data);
    }

    private static byte ReplacementFor(int key, bool right)
    {
        foreach (string row in Rows)
        {
            int index = row.IndexOf((char)key);
            if (index < 0) continue;
            if (index == 0) return (byte)row[1];
            if (index == row.Length - 1) return (byte)row[index - 1];
            return (byte)row[index + (right ? 1 : -1)];
        }
        return 0;
    }

    private static bool ModifierDown()
    {
        return Down(0x11) || Down(0x12) || Down(0x5B) || Down(0x5C);
    }

    private static bool Down(int key)
    {
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    }

    private static bool InjectKey(byte key, bool up)
    {
        var input = new Input[1];
        input[0].type = 1;
        input[0].data.keyboard.vk = key;
        input[0].data.keyboard.flags = up ? KeyUp : 0;
        return SendInput(1, input, Marshal.SizeOf(typeof(Input))) == 1;
    }

    protected override void ExitThreadCore()
    {
        for (int key = 0; key < heldReplacement.Length; key++)
            if (heldReplacement[key] > 0 && heldReplacement[key] < Released) InjectKey(heldReplacement[key], true);
        if (hook != IntPtr.Zero) UnhookWindowsHookEx(hook);
        tray.Visible = false;
        tray.Dispose();
        base.ExitThreadCore();
    }

    private delegate IntPtr HookProc(int code, IntPtr message, IntPtr data);

    [StructLayout(LayoutKind.Sequential)]
    private struct KeyboardEvent
    {
        public uint vkCode;
        public uint scanCode;
        public uint flags;
        public uint time;
        public IntPtr extraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KeyboardInput
    {
        public ushort vk;
        public ushort scan;
        public uint flags;
        public uint time;
        public IntPtr extraInfo;
    }

    [StructLayout(LayoutKind.Explicit, Size = 32)]
    private struct InputData
    {
        [FieldOffset(0)] public KeyboardInput keyboard;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Input
    {
        public uint type;
        public InputData data;
    }

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr SetWindowsHookEx(int id, HookProc callback, IntPtr module, uint thread);
    [DllImport("user32.dll")]
    private static extern bool UnhookWindowsHookEx(IntPtr hook);
    [DllImport("user32.dll")]
    private static extern IntPtr CallNextHookEx(IntPtr hook, int code, IntPtr message, IntPtr data);
    [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
    private static extern IntPtr GetModuleHandle(string name);
    [DllImport("user32.dll")]
    private static extern short GetAsyncKeyState(int key);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint SendInput(uint count, Input[] inputs, int size);
}
