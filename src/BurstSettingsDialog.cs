using System.Drawing;
using System.Windows.Forms;

namespace WinCapSimple
{
    internal sealed class BurstSettingsDialog : Form
    {
        private readonly NumericUpDown _duration = new NumericUpDown();
        private readonly NumericUpDown _interval = new NumericUpDown();
        private readonly Button _startButton = new Button();
        private readonly Button _cancelButton = new Button();

        public BurstSettings Settings;

        public BurstSettingsDialog()
        {
            Text = "Burst Capture";
            StartPosition = FormStartPosition.CenterParent;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            ShowInTaskbar = false;
            ClientSize = new Size(320, 150);

            Label durationLabel = new Label();
            durationLabel.Text = "Duration";
            durationLabel.AutoSize = true;
            durationLabel.Location = new Point(14, 18);

            _duration.Minimum = 1;
            _duration.Maximum = 60;
            _duration.Value = 5;
            _duration.Location = new Point(160, 16);
            _duration.Size = new Size(72, 22);

            Label secondsLabel = new Label();
            secondsLabel.Text = "sec";
            secondsLabel.AutoSize = true;
            secondsLabel.Location = new Point(240, 19);

            Label intervalLabel = new Label();
            intervalLabel.Text = "Frame interval";
            intervalLabel.AutoSize = true;
            intervalLabel.Location = new Point(14, 58);

            _interval.Minimum = 100;
            _interval.Maximum = 2000;
            _interval.Increment = 50;
            _interval.Value = 250;
            _interval.Location = new Point(160, 56);
            _interval.Size = new Size(72, 22);

            Label millisecondsLabel = new Label();
            millisecondsLabel.Text = "ms";
            millisecondsLabel.AutoSize = true;
            millisecondsLabel.Location = new Point(240, 59);

            _startButton.Text = "Start";
            _startButton.Anchor = AnchorStyles.Bottom | AnchorStyles.Right;
            _startButton.Location = new Point(136, 108);
            _startButton.Size = new Size(80, 28);
            _startButton.Click += delegate { Accept(); };

            _cancelButton.Text = "Cancel";
            _cancelButton.Anchor = AnchorStyles.Bottom | AnchorStyles.Right;
            _cancelButton.Location = new Point(226, 108);
            _cancelButton.Size = new Size(80, 28);
            _cancelButton.DialogResult = DialogResult.Cancel;

            Controls.Add(durationLabel);
            Controls.Add(_duration);
            Controls.Add(secondsLabel);
            Controls.Add(intervalLabel);
            Controls.Add(_interval);
            Controls.Add(millisecondsLabel);
            Controls.Add(_startButton);
            Controls.Add(_cancelButton);

            AcceptButton = _startButton;
            CancelButton = _cancelButton;
        }

        private void Accept()
        {
            Settings = new BurstSettings();
            Settings.DurationSeconds = (int)_duration.Value;
            Settings.IntervalMilliseconds = (int)_interval.Value;
            DialogResult = DialogResult.OK;
            Close();
        }
    }
}
