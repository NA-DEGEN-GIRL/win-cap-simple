using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace WinCapSimple
{
    internal sealed class SelectionOverlay : Form
    {
        private readonly CaptureMode _mode;
        private readonly Rectangle _virtualBounds;
        private readonly List<Point> _freeformPoints = new List<Point>();
        private bool _dragging;
        private Point _startScreenPoint;
        private Point _currentScreenPoint;

        public Rectangle SelectedBounds;
        public List<Point> SelectedFreeformPoints;

        public SelectionOverlay(CaptureMode mode)
        {
            _mode = mode;
            _virtualBounds = SystemInformation.VirtualScreen;

            StartPosition = FormStartPosition.Manual;
            Bounds = _virtualBounds;
            FormBorderStyle = FormBorderStyle.None;
            TopMost = true;
            ShowInTaskbar = false;
            BackColor = Color.Black;
            Opacity = 0.28;
            Cursor = Cursors.Cross;
            KeyPreview = true;
            DoubleBuffered = true;

            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            Activate();
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Escape)
            {
                DialogResult = DialogResult.Cancel;
                Close();
            }

            base.OnKeyDown(e);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Left)
            {
                return;
            }

            _dragging = true;
            _startScreenPoint = ToScreenPoint(e.Location);
            _currentScreenPoint = _startScreenPoint;
            _freeformPoints.Clear();
            _freeformPoints.Add(_startScreenPoint);
            Invalidate();
            base.OnMouseDown(e);
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            if (!_dragging)
            {
                return;
            }

            _currentScreenPoint = ToScreenPoint(e.Location);
            if (_mode == CaptureMode.Freeform)
            {
                Point last = _freeformPoints[_freeformPoints.Count - 1];
                if (Math.Abs(last.X - _currentScreenPoint.X) + Math.Abs(last.Y - _currentScreenPoint.Y) >= 3)
                {
                    _freeformPoints.Add(_currentScreenPoint);
                }
            }

            Invalidate();
            base.OnMouseMove(e);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            if (!_dragging || e.Button != MouseButtons.Left)
            {
                return;
            }

            _dragging = false;
            _currentScreenPoint = ToScreenPoint(e.Location);

            if (_mode == CaptureMode.Freeform)
            {
                _freeformPoints.Add(_currentScreenPoint);
                if (_freeformPoints.Count >= 3)
                {
                    SelectedFreeformPoints = new List<Point>(_freeformPoints);
                    SelectedBounds = CaptureService.BoundsFromPoints(SelectedFreeformPoints);
                    DialogResult = DialogResult.OK;
                    Close();
                }
            }
            else
            {
                Rectangle selected = Normalize(_startScreenPoint, _currentScreenPoint);
                if (selected.Width >= 2 && selected.Height >= 2)
                {
                    SelectedBounds = selected;
                    DialogResult = DialogResult.OK;
                    Close();
                }
            }

            Invalidate();
            base.OnMouseUp(e);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

            using (Brush textBrush = new SolidBrush(Color.White))
            using (Font font = new Font(Font.FontFamily, 13.0f, FontStyle.Bold))
            {
                string message = _mode == CaptureMode.Freeform ? "Drag freeform area, Esc cancels" : "Drag rectangle, Esc cancels";
                e.Graphics.DrawString(message, font, textBrush, 18, 18);
            }

            if (!_dragging)
            {
                return;
            }

            using (Pen border = new Pen(Color.White, 2.0f))
            using (Brush fill = new SolidBrush(Color.FromArgb(70, Color.DodgerBlue)))
            {
                if (_mode == CaptureMode.Freeform)
                {
                    Point[] points = ToClientPoints(_freeformPoints);
                    if (points.Length > 1)
                    {
                        e.Graphics.DrawLines(border, points);
                    }
                }
                else
                {
                    Rectangle selected = ToClientRectangle(Normalize(_startScreenPoint, _currentScreenPoint));
                    e.Graphics.FillRectangle(fill, selected);
                    e.Graphics.DrawRectangle(border, selected);
                }
            }
        }

        private Point ToScreenPoint(Point clientPoint)
        {
            return new Point(_virtualBounds.Left + clientPoint.X, _virtualBounds.Top + clientPoint.Y);
        }

        private Point[] ToClientPoints(IList<Point> screenPoints)
        {
            Point[] points = new Point[screenPoints.Count];
            for (int i = 0; i < screenPoints.Count; i++)
            {
                points[i] = new Point(screenPoints[i].X - _virtualBounds.Left, screenPoints[i].Y - _virtualBounds.Top);
            }
            return points;
        }

        private Rectangle ToClientRectangle(Rectangle screenRectangle)
        {
            return new Rectangle(
                screenRectangle.Left - _virtualBounds.Left,
                screenRectangle.Top - _virtualBounds.Top,
                screenRectangle.Width,
                screenRectangle.Height);
        }

        private static Rectangle Normalize(Point first, Point second)
        {
            int left = Math.Min(first.X, second.X);
            int top = Math.Min(first.Y, second.Y);
            int right = Math.Max(first.X, second.X);
            int bottom = Math.Max(first.Y, second.Y);
            return Rectangle.FromLTRB(left, top, right, bottom);
        }
    }
}
