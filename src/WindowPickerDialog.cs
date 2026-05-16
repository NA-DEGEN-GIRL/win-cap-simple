using System;
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;

namespace WinCapSimple
{
    internal sealed class WindowPickerDialog : Form
    {
        private readonly ListBox _listBox = new ListBox();
        private readonly Button _captureButton = new Button();
        private readonly Button _cancelButton = new Button();

        public WindowInfo SelectedWindow;

        public WindowPickerDialog(List<WindowInfo> windows)
        {
            Text = "Window Capture";
            StartPosition = FormStartPosition.CenterParent;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MinimizeBox = false;
            MaximizeBox = false;
            ShowInTaskbar = false;
            ClientSize = new Size(560, 390);

            Label label = new Label();
            label.Text = "Select a window";
            label.AutoSize = true;
            label.Location = new Point(12, 12);

            _listBox.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            _listBox.Location = new Point(12, 38);
            _listBox.Size = new Size(536, 295);
            _listBox.IntegralHeight = false;
            _listBox.DoubleClick += delegate { AcceptSelection(); };

            if (windows != null)
            {
                for (int i = 0; i < windows.Count; i++)
                {
                    _listBox.Items.Add(windows[i]);
                }
            }

            if (_listBox.Items.Count > 0)
            {
                _listBox.SelectedIndex = 0;
            }

            _captureButton.Text = "Capture";
            _captureButton.Anchor = AnchorStyles.Bottom | AnchorStyles.Right;
            _captureButton.Location = new Point(376, 350);
            _captureButton.Size = new Size(82, 28);
            _captureButton.Click += delegate { AcceptSelection(); };

            _cancelButton.Text = "Cancel";
            _cancelButton.Anchor = AnchorStyles.Bottom | AnchorStyles.Right;
            _cancelButton.Location = new Point(466, 350);
            _cancelButton.Size = new Size(82, 28);
            _cancelButton.DialogResult = DialogResult.Cancel;

            Controls.Add(label);
            Controls.Add(_listBox);
            Controls.Add(_captureButton);
            Controls.Add(_cancelButton);

            AcceptButton = _captureButton;
            CancelButton = _cancelButton;
        }

        private void AcceptSelection()
        {
            WindowInfo selected = _listBox.SelectedItem as WindowInfo;
            if (selected == null)
            {
                DialogResult = DialogResult.Cancel;
                Close();
                return;
            }

            SelectedWindow = selected;
            DialogResult = DialogResult.OK;
            Close();
        }
    }
}
