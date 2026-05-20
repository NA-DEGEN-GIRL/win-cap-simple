using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Windows.Forms;

namespace WinCapSimple
{
    internal sealed class AnnotatingCanvas : Control
    {
        private Bitmap _image;
        private readonly List<InkStroke> _strokes = new List<InkStroke>();
        private InkStroke _activeStroke;

        public bool MarkerEnabled;
        public Color MarkerColor = Color.FromArgb(232, 43, 43);
        public float MarkerWidth = 6.0f;

        public AnnotatingCanvas()
        {
            BackColor = Color.FromArgb(245, 246, 248);
            Dock = DockStyle.Fill;
            DoubleBuffered = true;
            TabStop = true;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        }

        public bool HasImage
        {
            get { return _image != null; }
        }

        public void SetImage(Bitmap image)
        {
            if (_image != null)
            {
                _image.Dispose();
            }

            _image = image;
            _strokes.Clear();
            Invalidate();
        }

        public void ClearMarks()
        {
            _strokes.Clear();
            Invalidate();
        }

        public bool RotateFlipCurrent(RotateFlipType rotateFlipType)
        {
            if (_image == null)
            {
                return false;
            }

            Bitmap transformed = CreateFlattenedImage();
            transformed.RotateFlip(rotateFlipType);
            SetImage(transformed);
            return true;
        }

        public Bitmap CreateFlattenedImage()
        {
            if (_image == null)
            {
                return null;
            }

            Bitmap output = new Bitmap(_image.Width, _image.Height, PixelFormat.Format32bppArgb);
            using (Graphics graphics = Graphics.FromImage(output))
            {
                graphics.SmoothingMode = SmoothingMode.AntiAlias;
                graphics.InterpolationMode = InterpolationMode.NearestNeighbor;
                graphics.Clear(Color.Transparent);
                graphics.DrawImageUnscaled(_image, 0, 0);
                DrawStrokesOnImage(graphics, 1.0f, 0.0f, 0.0f);
            }
            return output;
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && _image != null)
            {
                _image.Dispose();
                _image = null;
            }

            base.Dispose(disposing);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            Focus();

            if (MarkerEnabled && _image != null && e.Button == MouseButtons.Left)
            {
                PointF imagePoint;
                if (TryClientToImage(e.Location, out imagePoint))
                {
                    _activeStroke = new InkStroke();
                    _activeStroke.Color = MarkerColor;
                    _activeStroke.Width = MarkerWidth;
                    _activeStroke.Points.Add(imagePoint);
                    _strokes.Add(_activeStroke);
                    InvalidateStrokeSegment(imagePoint, imagePoint, _activeStroke.Width);
                }
            }

            base.OnMouseDown(e);
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            if (_activeStroke != null && e.Button == MouseButtons.Left)
            {
                PointF imagePoint;
                if (TryClientToImage(e.Location, out imagePoint))
                {
                    List<PointF> points = _activeStroke.Points;
                    PointF last = points[points.Count - 1];
                    if (Math.Abs(last.X - imagePoint.X) + Math.Abs(last.Y - imagePoint.Y) >= 1.0f)
                    {
                        points.Add(imagePoint);
                        InvalidateStrokeSegment(last, imagePoint, _activeStroke.Width);
                    }
                }
            }

            base.OnMouseMove(e);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            _activeStroke = null;
            base.OnMouseUp(e);
        }

        protected override void OnPaintBackground(PaintEventArgs pevent)
        {
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(BackColor);

            if (_image == null)
            {
                using (Brush brush = new SolidBrush(Color.FromArgb(110, 116, 126)))
                using (Font font = new Font(Font.FontFamily, 14.0f))
                {
                    string text = "New Capture";
                    SizeF size = e.Graphics.MeasureString(text, font);
                    e.Graphics.DrawString(text, font, brush, (Width - size.Width) / 2.0f, (Height - size.Height) / 2.0f);
                }
                return;
            }

            RectangleF imageRect = GetImageRectangle();
            e.Graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
            e.Graphics.DrawImage(_image, imageRect);

            using (Pen border = new Pen(Color.FromArgb(205, 210, 218)))
            {
                e.Graphics.DrawRectangle(border, imageRect.X, imageRect.Y, imageRect.Width, imageRect.Height);
            }

            float scale = imageRect.Width / _image.Width;
            DrawStrokesOnImage(e.Graphics, scale, imageRect.X, imageRect.Y);
        }

        private void DrawStrokesOnImage(Graphics graphics, float scale, float offsetX, float offsetY)
        {
            for (int i = 0; i < _strokes.Count; i++)
            {
                InkStroke stroke = _strokes[i];
                if (stroke.Points.Count == 0)
                {
                    continue;
                }

                using (Pen pen = new Pen(stroke.Color, Math.Max(1.0f, stroke.Width * scale)))
                {
                    pen.StartCap = LineCap.Round;
                    pen.EndCap = LineCap.Round;
                    pen.LineJoin = LineJoin.Round;

                    if (stroke.Points.Count == 1)
                    {
                        PointF p = Transform(stroke.Points[0], scale, offsetX, offsetY);
                        graphics.DrawEllipse(pen, p.X, p.Y, 1.0f, 1.0f);
                    }
                    else
                    {
                        PointF[] points = new PointF[stroke.Points.Count];
                        for (int j = 0; j < stroke.Points.Count; j++)
                        {
                            points[j] = Transform(stroke.Points[j], scale, offsetX, offsetY);
                        }
                        graphics.DrawLines(pen, points);
                    }
                }
            }
        }

        private PointF Transform(PointF point, float scale, float offsetX, float offsetY)
        {
            return new PointF(offsetX + point.X * scale, offsetY + point.Y * scale);
        }

        private bool TryClientToImage(Point clientPoint, out PointF imagePoint)
        {
            RectangleF imageRect = GetImageRectangle();
            if (!imageRect.Contains(clientPoint))
            {
                imagePoint = PointF.Empty;
                return false;
            }

            float scale = imageRect.Width / _image.Width;
            float x = (clientPoint.X - imageRect.X) / scale;
            float y = (clientPoint.Y - imageRect.Y) / scale;
            x = Math.Max(0, Math.Min(_image.Width - 1, x));
            y = Math.Max(0, Math.Min(_image.Height - 1, y));
            imagePoint = new PointF(x, y);
            return true;
        }

        private RectangleF GetImageRectangle()
        {
            if (_image == null || Width <= 0 || Height <= 0)
            {
                return RectangleF.Empty;
            }

            float margin = 24.0f;
            float availableWidth = Math.Max(1.0f, Width - margin * 2.0f);
            float availableHeight = Math.Max(1.0f, Height - margin * 2.0f);
            float scale = Math.Min(availableWidth / _image.Width, availableHeight / _image.Height);
            float width = _image.Width * scale;
            float height = _image.Height * scale;
            return new RectangleF((Width - width) / 2.0f, (Height - height) / 2.0f, width, height);
        }

        private void InvalidateStrokeSegment(PointF a, PointF b, float imageWidth)
        {
            if (_image == null)
            {
                Invalidate();
                return;
            }

            RectangleF imageRect = GetImageRectangle();
            if (imageRect.Width <= 0.0f || imageRect.Height <= 0.0f)
            {
                Invalidate();
                return;
            }

            float scale = imageRect.Width / _image.Width;
            PointF clientA = Transform(a, scale, imageRect.X, imageRect.Y);
            PointF clientB = Transform(b, scale, imageRect.X, imageRect.Y);
            float left = Math.Min(clientA.X, clientB.X);
            float top = Math.Min(clientA.Y, clientB.Y);
            float right = Math.Max(clientA.X, clientB.X);
            float bottom = Math.Max(clientA.Y, clientB.Y);
            float padding = Math.Max(4.0f, imageWidth * scale + 4.0f);
            Rectangle dirty = Rectangle.FromLTRB(
                (int)Math.Floor(left - padding),
                (int)Math.Floor(top - padding),
                (int)Math.Ceiling(right + padding),
                (int)Math.Ceiling(bottom + padding));
            Invalidate(dirty);
        }
    }
}
