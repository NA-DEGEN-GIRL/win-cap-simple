#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

#pragma function(memset)
#pragma function(memcpy)

void *__cdecl memset(void *destination, int value, size_t size)
{
    BYTE *bytes = (BYTE *)destination;
    while (size--)
    {
        *bytes++ = (BYTE)value;
    }
    return destination;
}

void *__cdecl memcpy(void *destination, const void *source, size_t size)
{
    BYTE *dst = (BYTE *)destination;
    const BYTE *src = (const BYTE *)source;
    while (size--)
    {
        *dst++ = *src++;
    }
    return destination;
}

#define APP_CLASS L"WinCapNativeMain"
#define OVERLAY_CLASS L"WinCapNativeOverlay"
#define PICKER_CLASS L"WinCapNativePicker"
#define BURST_DIALOG_CLASS L"WinCapNativeBurstDialog"
#define VIEWER_CLASS L"WinCapNativeBurstViewer"
#define BURST_MARKER_CLASS L"WinCapNativeBurstMarker"

#define IDC_NEW 1001
#define IDC_MODE 1002
#define IDC_DISPLAY 1003
#define IDC_DELAY 1004
#define IDC_BURST 1005
#define IDC_VIEW_BURST 1006
#define IDC_COPY 1007
#define IDC_SAVE 1008
#define IDC_MARKER 1009
#define IDC_COLOR 1010
#define IDC_WIDTH 1011
#define IDC_CLEAR 1012
#define IDC_STATUS 1013
#define IDC_CAPTUREBLT 1014

#define IDC_PICK_LIST 2001
#define IDC_PICK_OK 2002
#define IDC_PICK_CANCEL 2003

#define IDC_BURST_DURATION 3001
#define IDC_BURST_INTERVAL 3002
#define IDC_BURST_OK 3003
#define IDC_BURST_CANCEL 3004

#define IDC_VIEW_LIST 4001
#define IDC_VIEW_USE 4002
#define IDC_VIEW_COPY 4003
#define IDC_VIEW_CLOSE 4004

#define MAX_MONITORS 16
#define MAX_WINDOWS 256
#define MAX_BURST_FILES 4096

typedef enum CaptureMode
{
    MODE_RECTANGLE = 0,
    MODE_FREEFORM = 1,
    MODE_WINDOW = 2,
    MODE_DISPLAY = 3
} CaptureMode;

typedef struct BitmapImage
{
    HBITMAP bitmap;
    void *bits;
    int width;
    int height;
    int stride;
} BitmapImage;

typedef struct PointVec
{
    POINT *items;
    int count;
    int capacity;
} PointVec;

typedef struct Stroke
{
    POINT *points;
    int count;
    int capacity;
    COLORREF color;
    int width;
    struct Stroke *next;
} Stroke;

typedef struct MonitorItem
{
    RECT bounds;
    BOOL primary;
} MonitorItem;

typedef struct CaptureTarget
{
    CaptureMode mode;
    RECT bounds;
    HWND window;
    POINT *points;
    int pointCount;
} CaptureTarget;

typedef struct WindowEntry
{
    HWND window;
    RECT bounds;
    WCHAR title[260];
} WindowEntry;

typedef struct PickerState
{
    HWND hwnd;
    HWND list;
    HWND owner;
    BOOL done;
    BOOL ok;
    WindowEntry entries[MAX_WINDOWS];
    int count;
    HWND selectedWindow;
    RECT selectedBounds;
} PickerState;

typedef struct BurstDialogState
{
    HWND hwnd;
    HWND owner;
    HWND durationEdit;
    HWND intervalEdit;
    BOOL done;
    BOOL ok;
    int durationSeconds;
    int intervalMilliseconds;
} BurstDialogState;

typedef struct OverlayState
{
    HWND hwnd;
    CaptureMode mode;
    RECT virtualBounds;
    BOOL done;
    BOOL ok;
    BOOL dragging;
    POINT start;
    POINT current;
    RECT selected;
    PointVec points;
} OverlayState;

typedef struct BurstMarkerState
{
    CaptureMode mode;
    RECT virtualBounds;
    RECT bounds;
    POINT *points;
    int pointCount;
} BurstMarkerState;

typedef struct BurstViewerState
{
    HWND hwnd;
    HWND owner;
    HWND list;
    BOOL done;
    BOOL ok;
    WCHAR files[MAX_BURST_FILES][MAX_PATH];
    int count;
    BitmapImage preview;
    BitmapImage selected;
} BurstViewerState;

typedef struct BurstFrame
{
    BYTE *bits;
    DWORD bytes;
    BOOL valid;
} BurstFrame;

typedef struct BurstFrameStore
{
    BurstFrame *frames;
    BYTE *block;
    DWORD frameBytes;
    int count;
} BurstFrameStore;

typedef struct BurstPerfScope
{
    DWORD oldPriorityClass;
    int oldThreadPriority;
    HANDLE mmcss;
} BurstPerfScope;

typedef struct DxgiDeviceCache
{
    RECT bounds;
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    IDXGIOutput1 *output;
} DxgiDeviceCache;

typedef UINT MMRESULT;
typedef MMRESULT (WINAPI *TimeBeginPeriodProc)(UINT period);
typedef MMRESULT (WINAPI *TimeEndPeriodProc)(UINT period);
typedef HANDLE (WINAPI *AvSetMmThreadCharacteristicsProc)(LPCWSTR taskName, LPDWORD taskIndex);
typedef BOOL (WINAPI *AvRevertMmThreadCharacteristicsProc)(HANDLE handle);

static HINSTANCE g_instance;
static HWND g_main;
static HWND g_controls[32];
static BitmapImage g_image;
static Stroke *g_strokes;
static Stroke *g_activeStroke;
static BOOL g_markerEnabled;
static BOOL g_includeLayeredWindows;
static BOOL g_lastBurstUsedDxgi;
static COLORREF g_markerColor = RGB(232, 43, 43);
static int g_markerWidth = 6;
static MonitorItem g_monitors[MAX_MONITORS];
static int g_monitorCount;
static BOOL g_monitorsLoaded;
static WCHAR g_lastBurstFolder[MAX_PATH];
static POINT *g_scratchPoints;
static int g_scratchPointCapacity;
static HMODULE g_winmmModule;
static HMODULE g_avrtModule;
static AvSetMmThreadCharacteristicsProc g_avSetMmThreadCharacteristics;
static AvRevertMmThreadCharacteristicsProc g_avRevertMmThreadCharacteristics;
static DxgiDeviceCache g_dxgiCache;

static TimeBeginPeriodProc g_timeBeginPeriod;
static TimeEndPeriodProc g_timeEndPeriod;

static int AbsInt(int value)
{
    return value < 0 ? -value : value;
}

static WCHAR ToLowerAscii(WCHAR ch)
{
    if (ch >= L'A' && ch <= L'Z')
    {
        return (WCHAR)(ch + (L'a' - L'A'));
    }
    return ch;
}

static BOOL ContainsTextInsensitive(const WCHAR *text, const WCHAR *needle)
{
    int i;
    int j;
    if (!text || !needle || !needle[0])
    {
        return FALSE;
    }
    for (i = 0; text[i]; ++i)
    {
        for (j = 0; needle[j]; ++j)
        {
            if (!text[i + j] || ToLowerAscii(text[i + j]) != ToLowerAscii(needle[j]))
            {
                break;
            }
        }
        if (!needle[j])
        {
            return TRUE;
        }
    }
    return FALSE;
}

static void *AllocZero(SIZE_T size)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

static void *AllocMem(SIZE_T size)
{
    return HeapAlloc(GetProcessHeap(), 0, size);
}

static void FreeMem(void *memory)
{
    if (memory)
    {
        HeapFree(GetProcessHeap(), 0, memory);
    }
}

static POINT *GetScratchPoints(int count)
{
    POINT *next;
    if (count <= 0)
    {
        return 0;
    }
    if (count > g_scratchPointCapacity)
    {
        next = (POINT *)AllocMem(sizeof(POINT) * count);
        if (!next)
        {
            return 0;
        }
        FreeMem(g_scratchPoints);
        g_scratchPoints = next;
        g_scratchPointCapacity = count;
    }
    return g_scratchPoints;
}

static void SetStatusText(const WCHAR *text)
{
    if (g_controls[IDC_STATUS - 1000])
    {
        SetWindowTextW(g_controls[IDC_STATUS - 1000], text);
    }
}

static DWORD GetCaptureRop(void)
{
    return g_includeLayeredWindows ? (SRCCOPY | CAPTUREBLT) : SRCCOPY;
}

static int RectWidth(RECT rect)
{
    return rect.right - rect.left;
}

static int RectHeight(RECT rect)
{
    return rect.bottom - rect.top;
}

static void NormalizeRectFromPoints(POINT a, POINT b, RECT *rect)
{
    rect->left = a.x < b.x ? a.x : b.x;
    rect->top = a.y < b.y ? a.y : b.y;
    rect->right = a.x > b.x ? a.x : b.x;
    rect->bottom = a.y > b.y ? a.y : b.y;
}

static void BoundsFromPoints(const POINT *points, int count, RECT *rect)
{
    int i;
    rect->left = points[0].x;
    rect->top = points[0].y;
    rect->right = points[0].x + 1;
    rect->bottom = points[0].y + 1;
    for (i = 1; i < count; ++i)
    {
        if (points[i].x < rect->left) rect->left = points[i].x;
        if (points[i].y < rect->top) rect->top = points[i].y;
        if (points[i].x + 1 > rect->right) rect->right = points[i].x + 1;
        if (points[i].y + 1 > rect->bottom) rect->bottom = points[i].y + 1;
    }
}

static BOOL PointVecAppend(PointVec *vec, POINT point)
{
    POINT *next;
    int nextCapacity;
    if (vec->count == vec->capacity)
    {
        nextCapacity = vec->capacity == 0 ? 128 : vec->capacity * 2;
        next = (POINT *)AllocZero(sizeof(POINT) * nextCapacity);
        if (!next)
        {
            return FALSE;
        }
        if (vec->items)
        {
            CopyMemory(next, vec->items, sizeof(POINT) * vec->count);
            FreeMem(vec->items);
        }
        vec->items = next;
        vec->capacity = nextCapacity;
    }
    vec->items[vec->count++] = point;
    return TRUE;
}

static void PointVecFree(PointVec *vec)
{
    FreeMem(vec->items);
    vec->items = 0;
    vec->count = 0;
    vec->capacity = 0;
}

static BOOL StrokeAppend(Stroke *stroke, POINT point)
{
    POINT *next;
    int nextCapacity;
    if (stroke->count == stroke->capacity)
    {
        nextCapacity = stroke->capacity == 0 ? 64 : stroke->capacity * 2;
        next = (POINT *)AllocZero(sizeof(POINT) * nextCapacity);
        if (!next)
        {
            return FALSE;
        }
        if (stroke->points)
        {
            CopyMemory(next, stroke->points, sizeof(POINT) * stroke->count);
            FreeMem(stroke->points);
        }
        stroke->points = next;
        stroke->capacity = nextCapacity;
    }
    stroke->points[stroke->count++] = point;
    return TRUE;
}

static void FreeStrokes(void)
{
    Stroke *stroke = g_strokes;
    Stroke *next;
    while (stroke)
    {
        next = stroke->next;
        FreeMem(stroke->points);
        FreeMem(stroke);
        stroke = next;
    }
    g_strokes = 0;
    g_activeStroke = 0;
}

static BOOL CreateBitmapImage(BitmapImage *image, int width, int height)
{
    BITMAPINFO info;
    ZeroMemory(image, sizeof(*image));
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    image->bitmap = CreateDIBSection(0, &info, DIB_RGB_COLORS, &image->bits, 0, 0);
    if (!image->bitmap || !image->bits)
    {
        ZeroMemory(image, sizeof(*image));
        return FALSE;
    }
    image->width = width;
    image->height = height;
    image->stride = width * 4;
    return TRUE;
}

static void FreeBitmapImage(BitmapImage *image)
{
    if (image->bitmap)
    {
        DeleteObject(image->bitmap);
    }
    ZeroMemory(image, sizeof(*image));
}

static BOOL LoadBmpImage(const WCHAR *path, BitmapImage *image)
{
    DIBSECTION section;
    HBITMAP bitmap;
    ZeroMemory(image, sizeof(*image));
    bitmap = (HBITMAP)LoadImageW(0, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!bitmap)
    {
        return FALSE;
    }
    ZeroMemory(&section, sizeof(section));
    if (GetObjectW(bitmap, sizeof(section), &section) == 0 || !section.dsBm.bmBits)
    {
        DeleteObject(bitmap);
        return FALSE;
    }
    image->bitmap = bitmap;
    image->bits = section.dsBm.bmBits;
    image->width = section.dsBm.bmWidth;
    image->height = section.dsBm.bmHeight < 0 ? -section.dsBm.bmHeight : section.dsBm.bmHeight;
    image->stride = section.dsBm.bmWidthBytes;
    return TRUE;
}

static BOOL SaveBmpBits(const WCHAR *path, int width, int height, int stride, const void *bits)
{
    HANDLE file;
    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;
    DWORD written;
    DWORD pixelBytes;
    DWORD fileBytes;
    BYTE header[sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)];
    BOOL ok;

    if (!bits || width <= 0 || height <= 0 || stride <= 0)
    {
        return FALSE;
    }

    pixelBytes = (DWORD)(stride * height);
    fileBytes = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + pixelBytes;
    ZeroMemory(&fileHeader, sizeof(fileHeader));
    ZeroMemory(&infoHeader, sizeof(infoHeader));
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileBytes;
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = -height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = pixelBytes;

    CopyMemory(header, &fileHeader, sizeof(fileHeader));
    CopyMemory(header + sizeof(fileHeader), &infoHeader, sizeof(infoHeader));

    file = CreateFileW(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    ok = WriteFile(file, header, sizeof(header), &written, 0) && written == sizeof(header);
    ok = ok && WriteFile(file, bits, pixelBytes, &written, 0) && written == pixelBytes;
    CloseHandle(file);
    return ok;
}

static BOOL SaveBmpImage(const WCHAR *path, const BitmapImage *image)
{
    if (!image->bitmap || !image->bits)
    {
        return FALSE;
    }

    return SaveBmpBits(path, image->width, image->height, image->stride, image->bits);
}

static HGLOBAL CreateClipboardDib(const BitmapImage *image)
{
    HGLOBAL memory;
    BYTE *base;
    BYTE *pixels;
    BITMAPINFOHEADER *header;
    DWORD rowBytes;
    DWORD pixelBytes;
    SIZE_T totalBytes;
    int y;

    if (!image || !image->bits || image->width <= 0 || image->height <= 0 || image->stride <= 0)
    {
        return 0;
    }

    rowBytes = (DWORD)image->width * 4;
    if (image->stride < (int)rowBytes)
    {
        return 0;
    }

    pixelBytes = rowBytes * (DWORD)image->height;
    totalBytes = sizeof(BITMAPINFOHEADER) + pixelBytes;
    memory = GlobalAlloc(GMEM_MOVEABLE, totalBytes);
    if (!memory)
    {
        return 0;
    }

    base = (BYTE *)GlobalLock(memory);
    if (!base)
    {
        GlobalFree(memory);
        return 0;
    }

    ZeroMemory(base, totalBytes);
    header = (BITMAPINFOHEADER *)base;
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = image->width;
    header->biHeight = image->height;
    header->biPlanes = 1;
    header->biBitCount = 32;
    header->biCompression = BI_RGB;
    header->biSizeImage = pixelBytes;

    pixels = base + sizeof(BITMAPINFOHEADER);
    for (y = 0; y < image->height; ++y)
    {
        const BYTE *src = (const BYTE *)image->bits + (image->height - 1 - y) * image->stride;
        BYTE *dst = pixels + y * rowBytes;
        CopyMemory(dst, src, rowBytes);
    }

    GlobalUnlock(memory);
    return memory;
}

static int CopyBitmapImageToClipboard(HWND owner, BitmapImage *image)
{
    HGLOBAL dib;
    HANDLE bitmapResult;
    HANDLE dibResult;
    int ok;

    if (!image || !image->bitmap || !image->bits)
    {
        return 0;
    }

    dib = CreateClipboardDib(image);
    if (!OpenClipboard(owner))
    {
        if (dib)
        {
            GlobalFree(dib);
        }
        return 2;
    }

    if (!EmptyClipboard())
    {
        CloseClipboard();
        if (dib)
        {
            GlobalFree(dib);
        }
        return 0;
    }

    bitmapResult = SetClipboardData(CF_BITMAP, image->bitmap);
    if (bitmapResult)
    {
        image->bitmap = 0;
    }

    dibResult = 0;
    if (dib)
    {
        dibResult = SetClipboardData(CF_DIB, dib);
        if (dibResult)
        {
            dib = 0;
        }
    }

    CloseClipboard();
    if (dib)
    {
        GlobalFree(dib);
    }

    ok = bitmapResult || dibResult;
    return ok ? 1 : 0;
}

static BOOL CaptureRectToImage(RECT bounds, BitmapImage *image)
{
    HDC screen;
    HDC memory;
    HGDIOBJ oldBitmap;
    BOOL ok;
    int width = RectWidth(bounds);
    int height = RectHeight(bounds);
    if (width <= 0 || height <= 0)
    {
        return FALSE;
    }
    if (!CreateBitmapImage(image, width, height))
    {
        return FALSE;
    }
    screen = GetDC(0);
    memory = CreateCompatibleDC(screen);
    oldBitmap = SelectObject(memory, image->bitmap);
    ok = BitBlt(memory, 0, 0, width, height, screen, bounds.left, bounds.top, GetCaptureRop());
    SelectObject(memory, oldBitmap);
    DeleteDC(memory);
    ReleaseDC(0, screen);
    if (!ok)
    {
        FreeBitmapImage(image);
        return FALSE;
    }
    return TRUE;
}

static void SortInts(int *values, int count)
{
    int i;
    int j;
    int key;
    for (i = 1; i < count; ++i)
    {
        key = values[i];
        j = i - 1;
        while (j >= 0 && values[j] > key)
        {
            values[j + 1] = values[j];
            --j;
        }
        values[j + 1] = key;
    }
}

static void ClearPixels(DWORD *row, int start, int end)
{
    int x;
    if (start < 0) start = 0;
    if (end < start) return;
    for (x = start; x < end; ++x)
    {
        row[x] = 0x00FFFFFF;
    }
}

static void ApplyFreeformMaskWithNodes(BitmapImage *image, RECT bounds, const POINT *points, int count, int *nodes)
{
    int y;
    int i;
    int nodeCount;
    int screenY;
    int left;
    int right;
    int clearStart;
    DWORD *row;

    if (!nodes)
    {
        return;
    }

    for (y = 0; y < image->height; ++y)
    {
        screenY = bounds.top + y;
        nodeCount = 0;
        for (i = 0; i < count; ++i)
        {
            int j = i == 0 ? count - 1 : i - 1;
            int yi = points[i].y;
            int yj = points[j].y;
            if (yi != yj && ((yi < screenY && yj >= screenY) || (yj < screenY && yi >= screenY)))
            {
                nodes[nodeCount++] = points[i].x + (screenY - yi) * (points[j].x - points[i].x) / (yj - yi);
            }
        }

        SortInts(nodes, nodeCount);
        row = (DWORD *)((BYTE *)image->bits + y * image->stride);
        clearStart = 0;
        for (i = 0; i + 1 < nodeCount; i += 2)
        {
            left = nodes[i] - bounds.left;
            right = nodes[i + 1] - bounds.left;
            if (right < 0 || left >= image->width)
            {
                continue;
            }
            if (left < 0) left = 0;
            if (right > image->width) right = image->width;
            ClearPixels(row, clearStart, left);
            clearStart = right;
        }
        ClearPixels(row, clearStart, image->width);
    }
}

static void ApplyFreeformMask(BitmapImage *image, RECT bounds, const POINT *points, int count)
{
    int *nodes;

    nodes = (int *)AllocMem(sizeof(int) * count);
    if (!nodes)
    {
        return;
    }
    ApplyFreeformMaskWithNodes(image, bounds, points, count, nodes);
    FreeMem(nodes);
}

static BOOL CaptureTargetToImage(const CaptureTarget *target, BitmapImage *image)
{
    RECT bounds = target->bounds;
    if (target->mode == MODE_WINDOW && target->window)
    {
        GetWindowRect(target->window, &bounds);
    }
    if (!CaptureRectToImage(bounds, image))
    {
        return FALSE;
    }
    if (target->mode == MODE_FREEFORM && target->points && target->pointCount >= 3)
    {
        ApplyFreeformMask(image, bounds, target->points, target->pointCount);
    }
    return TRUE;
}

static void ReplaceCurrentImage(BitmapImage *image)
{
    FreeBitmapImage(&g_image);
    FreeStrokes();
    g_image = *image;
    ZeroMemory(image, sizeof(*image));
    InvalidateRect(g_main, 0, TRUE);
}

static void GetPreviewRect(HWND hwnd, RECT *rect)
{
    RECT client;
    int availableWidth;
    int availableHeight;
    int width;
    int height;
    GetClientRect(hwnd, &client);
    rect->left = 12;
    rect->top = 48;
    rect->right = client.right - 12;
    rect->bottom = client.bottom - 34;
    if (!g_image.bitmap || g_image.width <= 0 || g_image.height <= 0)
    {
        return;
    }
    availableWidth = RectWidth(*rect);
    availableHeight = RectHeight(*rect);
    width = availableWidth;
    height = MulDiv(width, g_image.height, g_image.width);
    if (height > availableHeight)
    {
        height = availableHeight;
        width = MulDiv(height, g_image.width, g_image.height);
    }
    rect->left += (availableWidth - width) / 2;
    rect->top += (availableHeight - height) / 2;
    rect->right = rect->left + width;
    rect->bottom = rect->top + height;
}

static BOOL ClientToImagePoint(HWND hwnd, int x, int y, POINT *point)
{
    RECT preview;
    GetPreviewRect(hwnd, &preview);
    if (!g_image.bitmap || x < preview.left || x >= preview.right || y < preview.top || y >= preview.bottom)
    {
        return FALSE;
    }
    point->x = MulDiv(x - preview.left, g_image.width, RectWidth(preview));
    point->y = MulDiv(y - preview.top, g_image.height, RectHeight(preview));
    if (point->x < 0) point->x = 0;
    if (point->y < 0) point->y = 0;
    if (point->x >= g_image.width) point->x = g_image.width - 1;
    if (point->y >= g_image.height) point->y = g_image.height - 1;
    return TRUE;
}

static void DrawStrokes(HDC dc, RECT destination, int imageWidth, int imageHeight)
{
    Stroke *stroke;
    int i;
    HPEN pen;
    HGDIOBJ oldPen;
    POINT *scaled;
    int scaledWidth;
    if (imageWidth <= 0 || imageHeight <= 0)
    {
        return;
    }
    for (stroke = g_strokes; stroke; stroke = stroke->next)
    {
        if (stroke->count <= 0)
        {
            continue;
        }
        scaled = GetScratchPoints(stroke->count);
        if (!scaled)
        {
            continue;
        }
        for (i = 0; i < stroke->count; ++i)
        {
            scaled[i].x = destination.left + MulDiv(stroke->points[i].x, RectWidth(destination), imageWidth);
            scaled[i].y = destination.top + MulDiv(stroke->points[i].y, RectHeight(destination), imageHeight);
        }
        scaledWidth = MulDiv(stroke->width, RectWidth(destination), imageWidth);
        if (scaledWidth < 1) scaledWidth = 1;
        pen = CreatePen(PS_SOLID, scaledWidth, stroke->color);
        oldPen = SelectObject(dc, pen);
        if (stroke->count == 1)
        {
            Ellipse(dc, scaled[0].x - 1, scaled[0].y - 1, scaled[0].x + 1, scaled[0].y + 1);
        }
        else
        {
            Polyline(dc, scaled, stroke->count);
        }
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
}

static BOOL CreateFlattenedImage(BitmapImage *flat)
{
    HDC screen;
    HDC src;
    HDC dst;
    HGDIOBJ oldSrc;
    HGDIOBJ oldDst;
    RECT imageRect;
    if (!g_image.bitmap)
    {
        return FALSE;
    }
    if (!CreateBitmapImage(flat, g_image.width, g_image.height))
    {
        return FALSE;
    }
    screen = GetDC(0);
    src = CreateCompatibleDC(screen);
    dst = CreateCompatibleDC(screen);
    oldSrc = SelectObject(src, g_image.bitmap);
    oldDst = SelectObject(dst, flat->bitmap);
    BitBlt(dst, 0, 0, g_image.width, g_image.height, src, 0, 0, SRCCOPY);
    imageRect.left = 0;
    imageRect.top = 0;
    imageRect.right = g_image.width;
    imageRect.bottom = g_image.height;
    DrawStrokes(dst, imageRect, g_image.width, g_image.height);
    SelectObject(src, oldSrc);
    SelectObject(dst, oldDst);
    DeleteDC(src);
    DeleteDC(dst);
    ReleaseDC(0, screen);
    return TRUE;
}

static void CopyCurrentImage(void)
{
    BitmapImage flat;
    int copyResult;
    if (!CreateFlattenedImage(&flat))
    {
        SetStatusText(L"Nothing to copy.");
        return;
    }
    copyResult = CopyBitmapImageToClipboard(g_main, &flat);
    if (copyResult == 1)
    {
        SetStatusText(L"Copied to clipboard.");
    }
    else if (copyResult == 2)
    {
        SetStatusText(L"Clipboard is busy.");
    }
    else
    {
        SetStatusText(L"Copy failed.");
    }
    FreeBitmapImage(&flat);
}

static void SaveCurrentImage(void)
{
    OPENFILENAMEW ofn;
    WCHAR path[MAX_PATH];
    BitmapImage flat;
    ZeroMemory(path, sizeof(path));
    lstrcpyW(path, L"capture.bmp");
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = L"BMP image (*.bmp)\0*.bmp\0\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"bmp";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn))
    {
        return;
    }
    if (CreateFlattenedImage(&flat))
    {
        if (SaveBmpImage(path, &flat))
        {
            SetStatusText(L"Saved.");
        }
        else
        {
            SetStatusText(L"Save failed.");
        }
        FreeBitmapImage(&flat);
    }
}

static BOOL CALLBACK MonitorEnumProc(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM data)
{
    MONITORINFO info;
    WCHAR label[128];
    int index;
    HWND combo = (HWND)data;
    if (g_monitorCount >= MAX_MONITORS)
    {
        return FALSE;
    }
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
    {
        return TRUE;
    }
    index = g_monitorCount++;
    g_monitors[index].bounds = info.rcMonitor;
    g_monitors[index].primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    wsprintfW(label, L"Display %d%s  %dx%d", index + 1, g_monitors[index].primary ? L" primary" : L"", RectWidth(info.rcMonitor), RectHeight(info.rcMonitor));
    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)label);
    return TRUE;
}

static void EnsureMonitorsLoaded(void)
{
    HWND combo = g_controls[IDC_DISPLAY - 1000];
    if (g_monitorsLoaded)
    {
        return;
    }
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    g_monitorCount = 0;
    EnumDisplayMonitors(0, 0, MonitorEnumProc, (LPARAM)combo);
    if (g_monitorCount == 0)
    {
        RECT primary;
        primary.left = 0;
        primary.top = 0;
        primary.right = GetSystemMetrics(SM_CXSCREEN);
        primary.bottom = GetSystemMetrics(SM_CYSCREEN);
        g_monitors[0].bounds = primary;
        g_monitors[0].primary = TRUE;
        g_monitorCount = 1;
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"Display 1 primary");
    }
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
    g_monitorsLoaded = TRUE;
}

static CaptureMode GetSelectedMode(void)
{
    LRESULT index = SendMessageW(g_controls[IDC_MODE - 1000], CB_GETCURSEL, 0, 0);
    if (index < 0)
    {
        return MODE_RECTANGLE;
    }
    return (CaptureMode)index;
}

static int GetSelectedDelay(void)
{
    LRESULT index = SendMessageW(g_controls[IDC_DELAY - 1000], CB_GETCURSEL, 0, 0);
    if (index == 1) return 1000;
    if (index == 2) return 3000;
    if (index == 3) return 5000;
    if (index == 4) return 10000;
    return 0;
}

static COLORREF GetSelectedColor(void)
{
    LRESULT index = SendMessageW(g_controls[IDC_COLOR - 1000], CB_GETCURSEL, 0, 0);
    if (index == 1) return RGB(245, 190, 32);
    if (index == 2) return RGB(36, 111, 214);
    if (index == 3) return RGB(20, 22, 25);
    return RGB(232, 43, 43);
}

static int GetSelectedWidth(void)
{
    LRESULT index = SendMessageW(g_controls[IDC_WIDTH - 1000], CB_GETCURSEL, 0, 0);
    if (index == 0) return 3;
    if (index == 2) return 10;
    if (index == 3) return 16;
    return 6;
}

static BOOL CALLBACK EnumWindowsForPicker(HWND window, LPARAM data)
{
    PickerState *state = (PickerState *)data;
    DWORD pid;
    DWORD currentPid = GetCurrentProcessId();
    int len;
    RECT bounds;
    WCHAR label[360];
    if (state->count >= MAX_WINDOWS)
    {
        return FALSE;
    }
    if (!IsWindowVisible(window) || IsIconic(window))
    {
        return TRUE;
    }
    GetWindowThreadProcessId(window, &pid);
    if (pid == currentPid)
    {
        return TRUE;
    }
    len = GetWindowTextLengthW(window);
    if (len <= 0)
    {
        return TRUE;
    }
    if (!GetWindowRect(window, &bounds) || RectWidth(bounds) < 32 || RectHeight(bounds) < 32)
    {
        return TRUE;
    }
    state->entries[state->count].window = window;
    state->entries[state->count].bounds = bounds;
    GetWindowTextW(window, state->entries[state->count].title, 260);
    wsprintfW(label, L"%s  (%dx%d)", state->entries[state->count].title, RectWidth(bounds), RectHeight(bounds));
    SendMessageW(state->list, LB_ADDSTRING, 0, (LPARAM)label);
    ++state->count;
    return TRUE;
}

static void PickerAccept(PickerState *state)
{
    LRESULT index = SendMessageW(state->list, LB_GETCURSEL, 0, 0);
    if (index >= 0 && index < state->count)
    {
        state->selectedWindow = state->entries[index].window;
        state->selectedBounds = state->entries[index].bounds;
        state->ok = TRUE;
    }
    state->done = TRUE;
    DestroyWindow(state->hwnd);
}

static LRESULT CALLBACK PickerProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PickerState *state = (PickerState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (message)
    {
    case WM_CREATE:
        state = (PickerState *)((CREATESTRUCTW *)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        state->hwnd = hwnd;
        CreateWindowW(L"STATIC", L"Select a window", WS_CHILD | WS_VISIBLE, 12, 10, 220, 22, hwnd, 0, g_instance, 0);
        state->list = CreateWindowW(L"LISTBOX", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL, 12, 36, 560, 300, hwnd, (HMENU)IDC_PICK_LIST, g_instance, 0);
        CreateWindowW(L"BUTTON", L"Capture", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 394, 350, 82, 28, hwnd, (HMENU)IDC_PICK_OK, g_instance, 0);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 490, 350, 82, 28, hwnd, (HMENU)IDC_PICK_CANCEL, g_instance, 0);
        EnumWindows(EnumWindowsForPicker, (LPARAM)state);
        if (state->count > 0) SendMessageW(state->list, LB_SETCURSEL, 0, 0);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_PICK_OK || (LOWORD(wParam) == IDC_PICK_LIST && HIWORD(wParam) == LBN_DBLCLK))
        {
            PickerAccept(state);
            return 0;
        }
        if (LOWORD(wParam) == IDC_PICK_CANCEL)
        {
            state->done = TRUE;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        state->done = TRUE;
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static BOOL ShowWindowPicker(HWND owner, HWND *selectedWindow, RECT *selectedBounds)
{
    PickerState *state;
    MSG msg;
    BOOL result = FALSE;
    state = (PickerState *)AllocZero(sizeof(PickerState));
    if (!state)
    {
        return FALSE;
    }
    state->owner = owner;
    EnableWindow(owner, FALSE);
    CreateWindowExW(WS_EX_DLGMODALFRAME, PICKER_CLASS, L"Window Capture", WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 430, owner, 0, g_instance, state);
    while (!state->done && GetMessageW(&msg, 0, 0, 0))
    {
        if (!IsDialogMessageW(state->hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (state->ok)
    {
        *selectedWindow = state->selectedWindow;
        *selectedBounds = state->selectedBounds;
        result = TRUE;
    }
    FreeMem(state);
    return result;
}

static int ParsePositiveInt(HWND edit, int fallback)
{
    WCHAR text[32];
    int i;
    int value = 0;
    GetWindowTextW(edit, text, 32);
    for (i = 0; text[i]; ++i)
    {
        if (text[i] < L'0' || text[i] > L'9')
        {
            return fallback;
        }
        value = value * 10 + (text[i] - L'0');
    }
    return value > 0 ? value : fallback;
}

static LRESULT CALLBACK BurstDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    BurstDialogState *state = (BurstDialogState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (message)
    {
    case WM_CREATE:
        state = (BurstDialogState *)((CREATESTRUCTW *)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        state->hwnd = hwnd;
        CreateWindowW(L"STATIC", L"Duration", WS_CHILD | WS_VISIBLE, 14, 18, 120, 22, hwnd, 0, g_instance, 0);
        state->durationEdit = CreateWindowW(L"EDIT", L"5", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 150, 16, 70, 24, hwnd, (HMENU)IDC_BURST_DURATION, g_instance, 0);
        CreateWindowW(L"STATIC", L"sec", WS_CHILD | WS_VISIBLE, 230, 18, 50, 22, hwnd, 0, g_instance, 0);
        CreateWindowW(L"STATIC", L"Frame interval", WS_CHILD | WS_VISIBLE, 14, 56, 120, 22, hwnd, 0, g_instance, 0);
        state->intervalEdit = CreateWindowW(L"EDIT", L"250", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 150, 54, 70, 24, hwnd, (HMENU)IDC_BURST_INTERVAL, g_instance, 0);
        CreateWindowW(L"STATIC", L"ms", WS_CHILD | WS_VISIBLE, 230, 56, 50, 22, hwnd, 0, g_instance, 0);
        CreateWindowW(L"BUTTON", L"Start", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 120, 104, 80, 28, hwnd, (HMENU)IDC_BURST_OK, g_instance, 0);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 210, 104, 80, 28, hwnd, (HMENU)IDC_BURST_CANCEL, g_instance, 0);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BURST_OK)
        {
            state->durationSeconds = ParsePositiveInt(state->durationEdit, 5);
            state->intervalMilliseconds = ParsePositiveInt(state->intervalEdit, 250);
            if (state->durationSeconds > 60) state->durationSeconds = 60;
            if (state->intervalMilliseconds < 50) state->intervalMilliseconds = 50;
            state->ok = TRUE;
            state->done = TRUE;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wParam) == IDC_BURST_CANCEL)
        {
            state->done = TRUE;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        state->done = TRUE;
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static BOOL ShowBurstDialog(HWND owner, int *durationSeconds, int *intervalMilliseconds)
{
    BurstDialogState state;
    MSG msg;
    ZeroMemory(&state, sizeof(state));
    EnableWindow(owner, FALSE);
    CreateWindowExW(WS_EX_DLGMODALFRAME, BURST_DIALOG_CLASS, L"Burst Capture", WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 210, owner, 0, g_instance, &state);
    while (!state.done && GetMessageW(&msg, 0, 0, 0))
    {
        if (!IsDialogMessageW(state.hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (state.ok)
    {
        *durationSeconds = state.durationSeconds;
        *intervalMilliseconds = state.intervalMilliseconds;
        return TRUE;
    }
    return FALSE;
}

static POINT OverlayClientToScreenPoint(const OverlayState *state, int x, int y)
{
    POINT point;
    point.x = state->virtualBounds.left + x;
    point.y = state->virtualBounds.top + y;
    return point;
}

static POINT OverlayScreenToClientPoint(const OverlayState *state, POINT screen)
{
    POINT point;
    point.x = screen.x - state->virtualBounds.left;
    point.y = screen.y - state->virtualBounds.top;
    return point;
}

static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    OverlayState *state = (OverlayState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    PAINTSTRUCT ps;
    HDC dc;
    RECT client;
    HPEN pen;
    HGDIOBJ oldPen;
    HBRUSH brush;
    POINT *clientPoints;
    int i;
    RECT selected;
    switch (message)
    {
    case WM_CREATE:
        state = (OverlayState *)((CREATESTRUCTW *)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        state->hwnd = hwnd;
        SetLayeredWindowAttributes(hwnd, 0, 88, LWA_ALPHA);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            state->done = TRUE;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        state->dragging = TRUE;
        state->start = OverlayClientToScreenPoint(state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        state->current = state->start;
        PointVecFree(&state->points);
        PointVecAppend(&state->points, state->start);
        InvalidateRect(hwnd, 0, TRUE);
        return 0;
    case WM_MOUSEMOVE:
        if (state->dragging)
        {
            POINT last;
            state->current = OverlayClientToScreenPoint(state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (state->mode == MODE_FREEFORM && state->points.count > 0)
            {
                last = state->points.items[state->points.count - 1];
                if (AbsInt(last.x - state->current.x) + AbsInt(last.y - state->current.y) >= 3)
                {
                    PointVecAppend(&state->points, state->current);
                }
            }
            InvalidateRect(hwnd, 0, TRUE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state->dragging)
        {
            ReleaseCapture();
            state->dragging = FALSE;
            state->current = OverlayClientToScreenPoint(state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (state->mode == MODE_FREEFORM)
            {
                PointVecAppend(&state->points, state->current);
                if (state->points.count >= 3)
                {
                    BoundsFromPoints(state->points.items, state->points.count, &state->selected);
                    state->ok = TRUE;
                    state->done = TRUE;
                    DestroyWindow(hwnd);
                }
            }
            else
            {
                NormalizeRectFromPoints(state->start, state->current, &state->selected);
                if (RectWidth(state->selected) >= 2 && RectHeight(state->selected) >= 2)
                {
                    state->ok = TRUE;
                    state->done = TRUE;
                    DestroyWindow(hwnd);
                }
            }
        }
        return 0;
    case WM_PAINT:
        dc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &client);
        brush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(dc, &client, brush);
        DeleteObject(brush);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutW(dc, 18, 18, state->mode == MODE_FREEFORM ? L"Drag freeform area, Esc cancels" : L"Drag rectangle, Esc cancels", state->mode == MODE_FREEFORM ? 31 : 27);
        if (state->dragging)
        {
            pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            oldPen = SelectObject(dc, pen);
            if (state->mode == MODE_FREEFORM && state->points.count > 1)
            {
                clientPoints = GetScratchPoints(state->points.count);
                if (clientPoints)
                {
                    for (i = 0; i < state->points.count; ++i)
                    {
                        clientPoints[i] = OverlayScreenToClientPoint(state, state->points.items[i]);
                    }
                    Polyline(dc, clientPoints, state->points.count);
                }
            }
            else if (state->mode != MODE_FREEFORM)
            {
                NormalizeRectFromPoints(state->start, state->current, &selected);
                selected.left -= state->virtualBounds.left;
                selected.right -= state->virtualBounds.left;
                selected.top -= state->virtualBounds.top;
                selected.bottom -= state->virtualBounds.top;
                SelectObject(dc, GetStockObject(NULL_BRUSH));
                Rectangle(dc, selected.left, selected.top, selected.right, selected.bottom);
            }
            SelectObject(dc, oldPen);
            DeleteObject(pen);
        }
        EndPaint(hwnd, &ps);
        return 0;
    case WM_CLOSE:
        state->done = TRUE;
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static LRESULT CALLBACK BurstMarkerProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    BurstMarkerState *state = (BurstMarkerState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    PAINTSTRUCT ps;
    HDC dc;
    RECT client;
    RECT marker;
    HPEN pen;
    HGDIOBJ oldPen;
    HGDIOBJ oldBrush;
    HBRUSH background;
    POINT *points;
    int i;

    switch (message)
    {
    case WM_CREATE:
        state = (BurstMarkerState *)((CREATESTRUCTW *)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
        SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
        return 0;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        dc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &client);
        background = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(dc, &client, background);
        DeleteObject(background);

        pen = CreatePen(PS_SOLID, 3, RGB(255, 72, 64));
        oldPen = SelectObject(dc, pen);
        oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));

        if (state &&
            state->mode == MODE_FREEFORM &&
            state->points &&
            state->pointCount > 1)
        {
            points = GetScratchPoints(state->pointCount + 1);
            if (points)
            {
                for (i = 0; i < state->pointCount; ++i)
                {
                    points[i].x = state->points[i].x - state->virtualBounds.left;
                    points[i].y = state->points[i].y - state->virtualBounds.top;
                }
                points[state->pointCount] = points[0];
                Polyline(dc, points, state->pointCount + 1);
            }
        }
        else if (state)
        {
            marker = state->bounds;
            InflateRect(&marker, 3, 3);
            marker.left -= state->virtualBounds.left;
            marker.right -= state->virtualBounds.left;
            marker.top -= state->virtualBounds.top;
            marker.bottom -= state->virtualBounds.top;
            Rectangle(dc, marker.left, marker.top, marker.right, marker.bottom);
        }

        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static HWND ShowBurstMarker(const CaptureTarget *target, BurstMarkerState *state)
{
    HWND hwnd;
    RECT bounds;
    if (!target || !state)
    {
        return 0;
    }

    bounds = target->bounds;
    if (target->mode == MODE_WINDOW && target->window)
    {
        GetWindowRect(target->window, &bounds);
    }

    ZeroMemory(state, sizeof(*state));
    state->mode = target->mode;
    state->bounds = bounds;
    state->points = target->points;
    state->pointCount = target->pointCount;
    state->virtualBounds.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    state->virtualBounds.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    state->virtualBounds.right = state->virtualBounds.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    state->virtualBounds.bottom = state->virtualBounds.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        BURST_MARKER_CLASS,
        L"Burst Area",
        WS_POPUP,
        state->virtualBounds.left,
        state->virtualBounds.top,
        RectWidth(state->virtualBounds),
        RectHeight(state->virtualBounds),
        0,
        0,
        g_instance,
        state);
    if (hwnd)
    {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

static BOOL ShowSelectionOverlay(CaptureMode mode, RECT *selected, POINT **points, int *pointCount)
{
    OverlayState state;
    MSG msg;
    ZeroMemory(&state, sizeof(state));
    state.mode = mode;
    state.virtualBounds.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    state.virtualBounds.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    state.virtualBounds.right = state.virtualBounds.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    state.virtualBounds.bottom = state.virtualBounds.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, OVERLAY_CLASS, L"Capture Selection",
        WS_POPUP | WS_VISIBLE, state.virtualBounds.left, state.virtualBounds.top,
        RectWidth(state.virtualBounds), RectHeight(state.virtualBounds), 0, 0, g_instance, &state);
    SetForegroundWindow(state.hwnd);
    while (!state.done && GetMessageW(&msg, 0, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (state.ok)
    {
        *selected = state.selected;
        if (mode == MODE_FREEFORM)
        {
            *points = state.points.items;
            *pointCount = state.points.count;
            state.points.items = 0;
            state.points.count = 0;
        }
        PointVecFree(&state.points);
        return TRUE;
    }
    PointVecFree(&state.points);
    return FALSE;
}

static void SleepAfterHide(void)
{
    Sleep(20);
}

static BOOL PrepareTarget(CaptureMode mode, CaptureTarget *target)
{
    int delay = GetSelectedDelay();
    int displayIndex;
    HWND selectedWindow;
    RECT selectedBounds;
    ZeroMemory(target, sizeof(*target));
    target->mode = mode;
    if (mode == MODE_WINDOW)
    {
        if (!ShowWindowPicker(g_main, &selectedWindow, &selectedBounds))
        {
            return FALSE;
        }
        ShowWindow(g_main, SW_HIDE);
        if (delay > 0) Sleep(delay);
        SleepAfterHide();
        target->window = selectedWindow;
        target->bounds = selectedBounds;
        GetWindowRect(selectedWindow, &target->bounds);
        return TRUE;
    }
    if (mode == MODE_DISPLAY)
    {
        EnsureMonitorsLoaded();
        displayIndex = (int)SendMessageW(g_controls[IDC_DISPLAY - 1000], CB_GETCURSEL, 0, 0);
        if (displayIndex < 0 || displayIndex >= g_monitorCount) displayIndex = 0;
        ShowWindow(g_main, SW_HIDE);
        if (delay > 0) Sleep(delay);
        SleepAfterHide();
        target->bounds = g_monitors[displayIndex].bounds;
        return TRUE;
    }
    ShowWindow(g_main, SW_HIDE);
    if (delay > 0) Sleep(delay);
    if (!ShowSelectionOverlay(mode, &target->bounds, &target->points, &target->pointCount))
    {
        ShowWindow(g_main, SW_SHOW);
        SetForegroundWindow(g_main);
        return FALSE;
    }
    SleepAfterHide();
    return TRUE;
}

static void FreeTarget(CaptureTarget *target)
{
    FreeMem(target->points);
    ZeroMemory(target, sizeof(*target));
}

static void RunSingleCapture(void)
{
    CaptureTarget target;
    BitmapImage image;
    CaptureMode mode = GetSelectedMode();
    if (!PrepareTarget(mode, &target))
    {
        SetStatusText(L"Capture canceled.");
        return;
    }
    ZeroMemory(&image, sizeof(image));
    if (CaptureTargetToImage(&target, &image))
    {
        ReplaceCurrentImage(&image);
        SetStatusText(L"Captured. Press Ctrl+C to copy.");
    }
    else
    {
        SetStatusText(L"Capture failed.");
    }
    ShowWindow(g_main, SW_SHOW);
    SetForegroundWindow(g_main);
    FreeTarget(&target);
}

static void EnsureDirectory(const WCHAR *path)
{
    CreateDirectoryW(path, 0);
}

static void CreateBurstFolder(WCHAR *folder)
{
    WCHAR root[MAX_PATH];
    SYSTEMTIME time;
    GetTempPathW(MAX_PATH, root);
    lstrcpyW(folder, root);
    if (folder[lstrlenW(folder) - 1] != L'\\')
    {
        lstrcatW(folder, L"\\");
    }
    lstrcatW(folder, L"WinCapSimple");
    EnsureDirectory(folder);
    lstrcatW(folder, L"\\Bursts");
    EnsureDirectory(folder);
    GetLocalTime(&time);
    wsprintfW(root, L"\\%04d%02d%02d_%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    lstrcatW(folder, root);
    EnsureDirectory(folder);
}

static void MakeFramePath(const WCHAR *folder, int index, WCHAR *path)
{
    WCHAR name[64];
    lstrcpyW(path, folder);
    lstrcatW(path, L"\\");
    wsprintfW(name, L"frame_%04d.bmp", index + 1);
    lstrcatW(path, name);
}

static BOOL CreateBurstFrameStore(BurstFrameStore *store, int frameCount, DWORD frameBytes)
{
    int i;
    ULONGLONG totalBytes;
    ZeroMemory(store, sizeof(*store));
    if (frameCount <= 0 || frameBytes == 0)
    {
        return FALSE;
    }
    totalBytes = (ULONGLONG)frameCount * (ULONGLONG)frameBytes;
    if (totalBytes > (SIZE_T)-1)
    {
        return FALSE;
    }
    store->frames = (BurstFrame *)AllocZero(sizeof(BurstFrame) * frameCount);
    store->block = (BYTE *)AllocMem((SIZE_T)totalBytes);
    if (!store->frames || !store->block)
    {
        FreeMem(store->frames);
        FreeMem(store->block);
        ZeroMemory(store, sizeof(*store));
        return FALSE;
    }
    store->frameBytes = frameBytes;
    store->count = frameCount;
    for (i = 0; i < frameCount; ++i)
    {
        store->frames[i].bits = store->block + ((SIZE_T)i * frameBytes);
        store->frames[i].bytes = frameBytes;
    }
    return TRUE;
}

static BOOL StoreBurstFrame(BurstFrameStore *store, int index, const BitmapImage *image)
{
    DWORD bytes = (DWORD)(image->stride * image->height);
    if (!store || !store->frames || index < 0 || index >= store->count || bytes != store->frameBytes)
    {
        return FALSE;
    }
    CopyMemory(store->frames[index].bits, image->bits, bytes);
    store->frames[index].valid = TRUE;
    return TRUE;
}

static BOOL StoreRawBurstFrame(BurstFrameStore *store, int index, const BYTE *bits, DWORD bytes)
{
    if (!store || !store->frames || !bits || index < 0 || index >= store->count || bytes != store->frameBytes)
    {
        return FALSE;
    }
    CopyMemory(store->frames[index].bits, bits, bytes);
    store->frames[index].valid = TRUE;
    return TRUE;
}

static void FreeBurstFrameStore(BurstFrameStore *store)
{
    if (!store)
    {
        return;
    }
    FreeMem(store->block);
    FreeMem(store->frames);
    ZeroMemory(store, sizeof(*store));
}

static int FlushBurstFrameStore(BurstFrameStore *store, int width, int height, int stride, const WCHAR *folder)
{
    int i;
    int saved = 0;
    WCHAR path[MAX_PATH];
    if (!store || !store->frames)
    {
        return 0;
    }
    for (i = 0; i < store->count; ++i)
    {
        if (!store->frames[i].valid || !store->frames[i].bits)
        {
            continue;
        }
        MakeFramePath(folder, i, path);
        if (SaveBmpBits(path, width, height, stride, store->frames[i].bits))
        {
            ++saved;
        }
    }
    return saved;
}

static BOOL ShouldBufferBurst(int frameCount, DWORD frameBytes)
{
    ULONGLONG totalBytes = (ULONGLONG)frameCount * (ULONGLONG)frameBytes;
    return totalBytes <= (256ULL * 1024ULL * 1024ULL);
}

static void SleepUntilTick(LONGLONG targetTick, LONGLONG frequency);

static BOOL RectsEqual(RECT left, RECT right)
{
    return left.left == right.left &&
        left.top == right.top &&
        left.right == right.right &&
        left.bottom == right.bottom;
}

static void ReleaseDxgiDeviceCache(void)
{
    if (g_dxgiCache.output)
    {
        IDXGIOutput1_Release(g_dxgiCache.output);
    }
    if (g_dxgiCache.context)
    {
        ID3D11DeviceContext_Release(g_dxgiCache.context);
    }
    if (g_dxgiCache.device)
    {
        ID3D11Device_Release(g_dxgiCache.device);
    }
    ZeroMemory(&g_dxgiCache, sizeof(g_dxgiCache));
}

static void StoreDxgiDeviceCache(RECT bounds, ID3D11Device *device, ID3D11DeviceContext *context, IDXGIOutput1 *output)
{
    ReleaseDxgiDeviceCache();
    g_dxgiCache.bounds = bounds;
    g_dxgiCache.device = device;
    g_dxgiCache.context = context;
    g_dxgiCache.output = output;
    ID3D11Device_AddRef(g_dxgiCache.device);
    ID3D11DeviceContext_AddRef(g_dxgiCache.context);
    IDXGIOutput1_AddRef(g_dxgiCache.output);
}

static BOOL TryCreateDxgiDuplicationFromCache(
    RECT bounds,
    ID3D11Device **deviceOut,
    ID3D11DeviceContext **contextOut,
    IDXGIOutputDuplication **duplicationOut)
{
    IDXGIOutputDuplication *duplication = 0;

    if (!g_dxgiCache.device ||
        !g_dxgiCache.context ||
        !g_dxgiCache.output ||
        !RectsEqual(g_dxgiCache.bounds, bounds))
    {
        return FALSE;
    }

    if (FAILED(IDXGIOutput1_DuplicateOutput(g_dxgiCache.output, (IUnknown *)g_dxgiCache.device, &duplication)))
    {
        ReleaseDxgiDeviceCache();
        return FALSE;
    }

    ID3D11Device_AddRef(g_dxgiCache.device);
    ID3D11DeviceContext_AddRef(g_dxgiCache.context);
    *deviceOut = g_dxgiCache.device;
    *contextOut = g_dxgiCache.context;
    *duplicationOut = duplication;
    return TRUE;
}

static BOOL CreateDxgiDuplicationForBounds(
    RECT bounds,
    ID3D11Device **deviceOut,
    ID3D11DeviceContext **contextOut,
    IDXGIOutputDuplication **duplicationOut)
{
    HRESULT hr;
    IDXGIFactory1 *factory = 0;
    IDXGIAdapter1 *adapter = 0;
    IDXGIOutput *output = 0;
    IDXGIOutput1 *output1 = 0;
    ID3D11Device *device = 0;
    ID3D11DeviceContext *context = 0;
    IDXGIOutputDuplication *duplication = 0;
    DXGI_OUTPUT_DESC outputDesc;
    D3D_FEATURE_LEVEL levels[3];
    D3D_FEATURE_LEVEL actualLevel;
    UINT adapterIndex;
    UINT outputIndex;
    BOOL found = FALSE;

    *deviceOut = 0;
    *contextOut = 0;
    *duplicationOut = 0;

    if (TryCreateDxgiDuplicationFromCache(bounds, deviceOut, contextOut, duplicationOut))
    {
        return TRUE;
    }

    hr = CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&factory);
    if (FAILED(hr))
    {
        return FALSE;
    }

    levels[0] = D3D_FEATURE_LEVEL_11_0;
    levels[1] = D3D_FEATURE_LEVEL_10_1;
    levels[2] = D3D_FEATURE_LEVEL_10_0;

    for (adapterIndex = 0; !found && IDXGIFactory1_EnumAdapters1(factory, adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
    {
        for (outputIndex = 0; !found && IDXGIAdapter1_EnumOutputs(adapter, outputIndex, &output) != DXGI_ERROR_NOT_FOUND; ++outputIndex)
        {
            ZeroMemory(&outputDesc, sizeof(outputDesc));
            if (SUCCEEDED(IDXGIOutput_GetDesc(output, &outputDesc)) &&
                RectsEqual(outputDesc.DesktopCoordinates, bounds) &&
                outputDesc.Rotation == DXGI_MODE_ROTATION_IDENTITY)
            {
                hr = D3D11CreateDevice(
                    (IDXGIAdapter *)adapter,
                    D3D_DRIVER_TYPE_UNKNOWN,
                    0,
                    0,
                    levels,
                    3,
                    D3D11_SDK_VERSION,
                    &device,
                    &actualLevel,
                    &context);
                if (SUCCEEDED(hr) &&
                    SUCCEEDED(IDXGIOutput_QueryInterface(output, &IID_IDXGIOutput1, (void **)&output1)) &&
                    SUCCEEDED(IDXGIOutput1_DuplicateOutput(output1, (IUnknown *)device, &duplication)))
                {
                    StoreDxgiDeviceCache(bounds, device, context, output1);
                    found = TRUE;
                }
            }

            if (output1)
            {
                IDXGIOutput1_Release(output1);
                output1 = 0;
            }
            if (output)
            {
                IDXGIOutput_Release(output);
                output = 0;
            }
            if (!found)
            {
                if (duplication)
                {
                    IDXGIOutputDuplication_Release(duplication);
                    duplication = 0;
                }
                if (context)
                {
                    ID3D11DeviceContext_Release(context);
                    context = 0;
                }
                if (device)
                {
                    ID3D11Device_Release(device);
                    device = 0;
                }
            }
        }

        if (adapter)
        {
            IDXGIAdapter1_Release(adapter);
            adapter = 0;
        }
    }

    if (factory)
    {
        IDXGIFactory1_Release(factory);
    }

    if (!found)
    {
        return FALSE;
    }

    *deviceOut = device;
    *contextOut = context;
    *duplicationOut = duplication;
    return TRUE;
}

static BOOL CreateDxgiStagingTexture(
    ID3D11Device *device,
    ID3D11Texture2D *source,
    ID3D11Texture2D **stagingOut,
    int *widthOut,
    int *heightOut,
    int *strideOut)
{
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D_GetDesc(source, &desc);
    if (desc.Width == 0 || desc.Height == 0)
    {
        return FALSE;
    }

    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    if (FAILED(ID3D11Device_CreateTexture2D(device, &desc, 0, stagingOut)))
    {
        return FALSE;
    }

    *widthOut = (int)desc.Width;
    *heightOut = (int)desc.Height;
    *strideOut = (int)desc.Width * 4;
    return TRUE;
}

static void CopyDxgiMappedFrame(BYTE *destination, int width, int height, int destinationStride, const D3D11_MAPPED_SUBRESOURCE *mapped)
{
    int y;
    BYTE *dst = destination;
    BYTE *src = (BYTE *)mapped->pData;
    DWORD rowBytes = (DWORD)(width * 4);

    for (y = 0; y < height; ++y)
    {
        CopyMemory(dst, src, rowBytes);
        dst += destinationStride;
        src += mapped->RowPitch;
    }
}

static BOOL TryCaptureBurstFramesDxgiDisplay(
    const CaptureTarget *target,
    int frameCount,
    const WCHAR *folder,
    LONGLONG intervalTicks,
    LONGLONG firstTick,
    LONGLONG frequency,
    int *savedOut)
{
    ID3D11Device *device = 0;
    ID3D11DeviceContext *context = 0;
    IDXGIOutputDuplication *duplication = 0;
    IDXGIResource *resource = 0;
    ID3D11Texture2D *texture = 0;
    ID3D11Texture2D *staging = 0;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    D3D11_MAPPED_SUBRESOURCE mapped;
    BurstFrameStore store;
    BYTE *lastFrame = 0;
    WCHAR path[MAX_PATH];
    LONGLONG nextTick = firstTick;
    DWORD frameBytes = 0;
    int width = 0;
    int height = 0;
    int stride = 0;
    int i;
    int saved = 0;
    BOOL buffering = FALSE;
    BOOL ok = FALSE;
    HRESULT hr;
    BitmapImage seed;

    *savedOut = 0;
    ZeroMemory(&seed, sizeof(seed));
    ZeroMemory(&store, sizeof(store));
    ZeroMemory(&mapped, sizeof(mapped));
    ZeroMemory(&frameInfo, sizeof(frameInfo));

    if (target->mode != MODE_DISPLAY)
    {
        return FALSE;
    }

    if (!CreateDxgiDuplicationForBounds(target->bounds, &device, &context, &duplication))
    {
        return FALSE;
    }

    if (!CaptureRectToImage(target->bounds, &seed))
    {
        goto cleanup;
    }
    width = seed.width;
    height = seed.height;
    stride = seed.stride;
    frameBytes = (DWORD)(stride * height);
    lastFrame = (BYTE *)AllocMem(frameBytes);
    if (!lastFrame)
    {
        goto cleanup;
    }
    CopyMemory(lastFrame, seed.bits, frameBytes);
    buffering = ShouldBufferBurst(frameCount, frameBytes);
    if (buffering)
    {
        buffering = CreateBurstFrameStore(&store, frameCount, frameBytes);
    }
    if (buffering)
    {
        StoreRawBurstFrame(&store, 0, lastFrame, frameBytes);
    }
    else
    {
        MakeFramePath(folder, 0, path);
        if (SaveBmpBits(path, width, height, stride, lastFrame))
        {
            ++saved;
        }
    }
    FreeBitmapImage(&seed);

    nextTick += intervalTicks;

    for (i = 1; i < frameCount; ++i)
    {
        SleepUntilTick(nextTick, frequency);

        hr = IDXGIOutputDuplication_AcquireNextFrame(duplication, 0, &frameInfo, &resource);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        {
        }
        else if (FAILED(hr))
        {
            goto cleanup;
        }
        else
        {
            if (FAILED(IDXGIResource_QueryInterface(resource, &IID_ID3D11Texture2D, (void **)&texture)))
            {
                IDXGIOutputDuplication_ReleaseFrame(duplication);
                goto cleanup;
            }

            if (!staging)
            {
                if (!CreateDxgiStagingTexture(device, texture, &staging, &width, &height, &stride))
                {
                    IDXGIOutputDuplication_ReleaseFrame(duplication);
                    goto cleanup;
                }
                if ((DWORD)(stride * height) != frameBytes)
                {
                    IDXGIOutputDuplication_ReleaseFrame(duplication);
                    goto cleanup;
                }
            }

            ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)staging, (ID3D11Resource *)texture);
            IDXGIOutputDuplication_ReleaseFrame(duplication);

            if (FAILED(ID3D11DeviceContext_Map(context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped)))
            {
                goto cleanup;
            }
            CopyDxgiMappedFrame(lastFrame, width, height, stride, &mapped);
            ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)staging, 0);
        }

        if (buffering)
        {
            StoreRawBurstFrame(&store, i, lastFrame, frameBytes);
        }
        else
        {
            MakeFramePath(folder, i, path);
            if (SaveBmpBits(path, width, height, stride, lastFrame))
            {
                ++saved;
            }
        }

        if (texture)
        {
            ID3D11Texture2D_Release(texture);
            texture = 0;
        }
        if (resource)
        {
            IDXGIResource_Release(resource);
            resource = 0;
        }

        nextTick += intervalTicks;
    }

    if (buffering)
    {
        for (i = 0; i < frameCount; ++i)
        {
            if (!store.frames[i].valid)
            {
                goto cleanup;
            }
        }

        for (i = 0; i < frameCount; ++i)
        {
            MakeFramePath(folder, i, path);
            if (SaveBmpBits(path, width, height, stride, store.frames[i].bits))
            {
                ++saved;
            }
        }
    }
    *savedOut = saved;
    ok = saved > 0;

cleanup:
    FreeBitmapImage(&seed);
    if (texture)
    {
        ID3D11Texture2D_Release(texture);
    }
    if (resource)
    {
        IDXGIResource_Release(resource);
    }
    if (staging)
    {
        ID3D11Texture2D_Release(staging);
    }
    if (duplication)
    {
        IDXGIOutputDuplication_Release(duplication);
    }
    if (context)
    {
        ID3D11DeviceContext_Release(context);
    }
    if (device)
    {
        ID3D11Device_Release(device);
    }
    FreeMem(lastFrame);
    FreeBurstFrameStore(&store);
    return ok;
}

static void BeginBurstPerfScope(BurstPerfScope *scope)
{
    DWORD taskIndex = 0;
    ZeroMemory(scope, sizeof(*scope));
    scope->oldPriorityClass = GetPriorityClass(GetCurrentProcess());
    scope->oldThreadPriority = GetThreadPriority(GetCurrentThread());

    if (!g_winmmModule)
    {
        g_winmmModule = LoadLibraryExW(L"winmm.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (g_winmmModule)
        {
            g_timeBeginPeriod = (TimeBeginPeriodProc)GetProcAddress(g_winmmModule, "timeBeginPeriod");
            g_timeEndPeriod = (TimeEndPeriodProc)GetProcAddress(g_winmmModule, "timeEndPeriod");
        }
    }
    if (g_timeBeginPeriod)
    {
        g_timeBeginPeriod(1);
    }

    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    if (!g_avrtModule)
    {
        g_avrtModule = LoadLibraryExW(L"avrt.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (g_avrtModule)
        {
            g_avSetMmThreadCharacteristics = (AvSetMmThreadCharacteristicsProc)GetProcAddress(g_avrtModule, "AvSetMmThreadCharacteristicsW");
            g_avRevertMmThreadCharacteristics = (AvRevertMmThreadCharacteristicsProc)GetProcAddress(g_avrtModule, "AvRevertMmThreadCharacteristics");
        }
    }
    if (g_avSetMmThreadCharacteristics)
    {
        scope->mmcss = g_avSetMmThreadCharacteristics(L"Capture", &taskIndex);
    }
}

static void EndBurstPerfScope(BurstPerfScope *scope)
{
    if (g_avRevertMmThreadCharacteristics && scope->mmcss)
    {
        g_avRevertMmThreadCharacteristics(scope->mmcss);
    }

    if (scope->oldThreadPriority != THREAD_PRIORITY_ERROR_RETURN)
    {
        SetThreadPriority(GetCurrentThread(), scope->oldThreadPriority);
    }
    if (scope->oldPriorityClass)
    {
        SetPriorityClass(GetCurrentProcess(), scope->oldPriorityClass);
    }

    if (g_timeEndPeriod)
    {
        g_timeEndPeriod(1);
    }
}

static void SleepUntilTick(LONGLONG targetTick, LONGLONG frequency)
{
    LARGE_INTEGER now;
    int remainingMs;
    QueryPerformanceCounter(&now);
    while (now.QuadPart < targetTick)
    {
        remainingMs = (int)(((targetTick - now.QuadPart) * 1000) / frequency);
        if (remainingMs > 1)
        {
            Sleep(remainingMs - 1);
        }
        else
        {
            Sleep(0);
        }
        QueryPerformanceCounter(&now);
    }
}

static int CaptureBurstFrames(const CaptureTarget *target, int durationSeconds, int intervalMilliseconds, const WCHAR *folder)
{
    int frameCount = (durationSeconds * 1000) / intervalMilliseconds;
    int i;
    int saved = 0;
    int immediateSaved = 0;
    BOOL buffering;
    WCHAR path[MAX_PATH];
    BitmapImage frame;
    BurstFrameStore store;
    BurstPerfScope perfScope;
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    LONGLONG nextTick;
    LONGLONG intervalTicks;
    HDC screen;
    HDC memory;
    HGDIOBJ oldBitmap;
    RECT bounds = target->bounds;
    DWORD frameBytes;
    int *freeformNodes = 0;

    ZeroMemory(&frame, sizeof(frame));
    ZeroMemory(&store, sizeof(store));
    if (frameCount < 1) frameCount = 1;
    QueryPerformanceFrequency(&frequency);
    intervalTicks = ((LONGLONG)intervalMilliseconds * frequency.QuadPart) / 1000;
    QueryPerformanceCounter(&counter);
    nextTick = counter.QuadPart;
    BeginBurstPerfScope(&perfScope);
    g_lastBurstUsedDxgi = FALSE;

    if (target->mode == MODE_DISPLAY &&
        TryCaptureBurstFramesDxgiDisplay(target, frameCount, folder, intervalTicks, nextTick, frequency.QuadPart, &saved))
    {
        g_lastBurstUsedDxgi = TRUE;
        EndBurstPerfScope(&perfScope);
        return saved;
    }

    if (target->mode == MODE_FREEFORM)
    {
        RECT freeformBounds = target->bounds;
        if (target->points && target->pointCount >= 3)
        {
            BoundsFromPoints(target->points, target->pointCount, &freeformBounds);
        }
        if (!CreateBitmapImage(&frame, RectWidth(freeformBounds), RectHeight(freeformBounds)))
        {
            EndBurstPerfScope(&perfScope);
            return 0;
        }
        frameBytes = (DWORD)(frame.stride * frame.height);
        buffering = ShouldBufferBurst(frameCount, frameBytes);
        if (buffering)
        {
            buffering = CreateBurstFrameStore(&store, frameCount, frameBytes);
        }
        if (target->points && target->pointCount >= 3)
        {
            freeformNodes = (int *)AllocMem(sizeof(int) * target->pointCount);
        }

        screen = GetDC(0);
        memory = CreateCompatibleDC(screen);
        oldBitmap = SelectObject(memory, frame.bitmap);

        for (i = 0; i < frameCount; ++i)
        {
            BitBlt(memory, 0, 0, frame.width, frame.height, screen, freeformBounds.left, freeformBounds.top, GetCaptureRop());
            if (target->points && target->pointCount >= 3)
            {
                ApplyFreeformMaskWithNodes(&frame, freeformBounds, target->points, target->pointCount, freeformNodes);
            }
            if (buffering && StoreBurstFrame(&store, i, &frame))
            {
            }
            else
            {
                MakeFramePath(folder, i, path);
                if (SaveBmpImage(path, &frame)) ++immediateSaved;
            }
            nextTick += intervalTicks;
            if (i + 1 < frameCount)
            {
                SleepUntilTick(nextTick, frequency.QuadPart);
            }
        }
        SelectObject(memory, oldBitmap);
        DeleteDC(memory);
        ReleaseDC(0, screen);
        FreeMem(freeformNodes);
        if (store.frames)
        {
            saved += FlushBurstFrameStore(&store, frame.width, frame.height, frame.stride, folder);
            FreeBurstFrameStore(&store);
        }
        FreeBitmapImage(&frame);
        EndBurstPerfScope(&perfScope);
        return saved + immediateSaved;
    }

    if (target->mode == MODE_WINDOW && target->window)
    {
        GetWindowRect(target->window, &bounds);
    }
    if (!CreateBitmapImage(&frame, RectWidth(bounds), RectHeight(bounds)))
    {
        EndBurstPerfScope(&perfScope);
        return 0;
    }
    frameBytes = (DWORD)(frame.stride * frame.height);
    buffering = ShouldBufferBurst(frameCount, frameBytes);
    if (buffering)
    {
        buffering = CreateBurstFrameStore(&store, frameCount, frameBytes);
    }
    screen = GetDC(0);
    memory = CreateCompatibleDC(screen);
    oldBitmap = SelectObject(memory, frame.bitmap);
    for (i = 0; i < frameCount; ++i)
    {
        if (target->mode == MODE_WINDOW && target->window)
        {
            GetWindowRect(target->window, &bounds);
        }
        BitBlt(memory, 0, 0, frame.width, frame.height, screen, bounds.left, bounds.top, GetCaptureRop());
        if (buffering && StoreBurstFrame(&store, i, &frame))
        {
        }
        else
        {
            MakeFramePath(folder, i, path);
            if (SaveBmpImage(path, &frame)) ++immediateSaved;
        }
        nextTick += intervalTicks;
        if (i + 1 < frameCount)
        {
            SleepUntilTick(nextTick, frequency.QuadPart);
        }
    }
    SelectObject(memory, oldBitmap);
    DeleteDC(memory);
    ReleaseDC(0, screen);
    if (store.frames)
    {
        saved += FlushBurstFrameStore(&store, frame.width, frame.height, frame.stride, folder);
        FreeBurstFrameStore(&store);
    }
    FreeBitmapImage(&frame);
    EndBurstPerfScope(&perfScope);
    return saved + immediateSaved;
}

static void RunBurstCapture(void)
{
    int durationSeconds;
    int intervalMilliseconds;
    CaptureTarget target;
    BurstMarkerState markerState;
    HWND marker;
    WCHAR folder[MAX_PATH];
    int saved;
    if (!ShowBurstDialog(g_main, &durationSeconds, &intervalMilliseconds))
    {
        return;
    }
    if (!PrepareTarget(GetSelectedMode(), &target))
    {
        SetStatusText(L"Burst canceled.");
        return;
    }
    marker = ShowBurstMarker(&target, &markerState);
    CreateBurstFolder(folder);
    saved = CaptureBurstFrames(&target, durationSeconds, intervalMilliseconds, folder);
    if (marker)
    {
        DestroyWindow(marker);
    }
    lstrcpyW(g_lastBurstFolder, folder);
    ShowWindow(g_main, SW_SHOW);
    SetForegroundWindow(g_main);
    FreeTarget(&target);
    wsprintfW(folder, g_lastBurstUsedDxgi ? L"Burst saved %d frames (DXGI)." : L"Burst saved %d frames (GDI).", saved);
    SetStatusText(folder);
}

static BOOL FindLatestBurstFolder(WCHAR *folder)
{
    WCHAR root[MAX_PATH];
    WCHAR query[MAX_PATH];
    WIN32_FIND_DATAW data;
    HANDLE find;
    FILETIME best;
    BOOL found = FALSE;
    GetTempPathW(MAX_PATH, root);
    if (root[lstrlenW(root) - 1] != L'\\') lstrcatW(root, L"\\");
    lstrcatW(root, L"WinCapSimple\\Bursts");
    lstrcpyW(query, root);
    lstrcatW(query, L"\\*");
    find = FindFirstFileW(query, &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    ZeroMemory(&best, sizeof(best));
    do
    {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && data.cFileName[0] != L'.')
        {
            if (!found || CompareFileTime(&data.ftCreationTime, &best) > 0)
            {
                found = TRUE;
                best = data.ftCreationTime;
                lstrcpyW(folder, root);
                lstrcatW(folder, L"\\");
                lstrcatW(folder, data.cFileName);
            }
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return found;
}

static void ViewerLoadFiles(BurstViewerState *state, const WCHAR *folder)
{
    WCHAR query[MAX_PATH];
    WIN32_FIND_DATAW data;
    HANDLE find;
    lstrcpyW(query, folder);
    lstrcatW(query, L"\\*.bmp");
    find = FindFirstFileW(query, &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && state->count < MAX_BURST_FILES)
        {
            lstrcpyW(state->files[state->count], folder);
            lstrcatW(state->files[state->count], L"\\");
            lstrcatW(state->files[state->count], data.cFileName);
            SendMessageW(state->list, LB_ADDSTRING, 0, (LPARAM)data.cFileName);
            ++state->count;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
}

static void ViewerLoadPreview(BurstViewerState *state)
{
    LRESULT index = SendMessageW(state->list, LB_GETCURSEL, 0, 0);
    FreeBitmapImage(&state->preview);
    if (index >= 0 && index < state->count)
    {
        LoadBmpImage(state->files[index], &state->preview);
    }
    InvalidateRect(state->hwnd, 0, TRUE);
}

static void ViewerUseSelected(BurstViewerState *state)
{
    LRESULT index = SendMessageW(state->list, LB_GETCURSEL, 0, 0);
    if (index >= 0 && index < state->count)
    {
        FreeBitmapImage(&state->selected);
        if (LoadBmpImage(state->files[index], &state->selected))
        {
            state->ok = TRUE;
            state->done = TRUE;
            DestroyWindow(state->hwnd);
        }
    }
}

static void ViewerCopySelected(BurstViewerState *state)
{
    LRESULT index = SendMessageW(state->list, LB_GETCURSEL, 0, 0);
    BitmapImage image;
    int copyResult;
    if (index >= 0 && index < state->count && LoadBmpImage(state->files[index], &image))
    {
        copyResult = CopyBitmapImageToClipboard(state->hwnd, &image);
        if (copyResult == 1)
        {
            SetWindowTextW(state->hwnd, L"Burst Viewer - copied");
        }
        else if (copyResult == 2)
        {
            SetWindowTextW(state->hwnd, L"Burst Viewer - clipboard busy");
        }
        else
        {
            SetWindowTextW(state->hwnd, L"Burst Viewer - copy failed");
        }
        FreeBitmapImage(&image);
    }
}

static LRESULT CALLBACK BurstViewerProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    BurstViewerState *state = (BurstViewerState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    PAINTSTRUCT ps;
    HDC dc;
    HDC memory;
    HGDIOBJ oldBitmap;
    RECT client;
    RECT preview;
    int width;
    int height;
    switch (message)
    {
    case WM_CREATE:
        state = (BurstViewerState *)((CREATESTRUCTW *)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        state->hwnd = hwnd;
        state->list = CreateWindowW(L"LISTBOX", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL, 12, 12, 180, 520, hwnd, (HMENU)IDC_VIEW_LIST, g_instance, 0);
        CreateWindowW(L"BUTTON", L"Use", WS_CHILD | WS_VISIBLE, 520, 540, 80, 28, hwnd, (HMENU)IDC_VIEW_USE, g_instance, 0);
        CreateWindowW(L"BUTTON", L"Copy", WS_CHILD | WS_VISIBLE, 610, 540, 80, 28, hwnd, (HMENU)IDC_VIEW_COPY, g_instance, 0);
        CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE, 700, 540, 80, 28, hwnd, (HMENU)IDC_VIEW_CLOSE, g_instance, 0);
        return 0;
    case WM_SIZE:
        GetClientRect(hwnd, &client);
        MoveWindow(state->list, 12, 12, 190, client.bottom - 58, TRUE);
        MoveWindow(GetDlgItem(hwnd, IDC_VIEW_CLOSE), client.right - 92, client.bottom - 38, 80, 28, TRUE);
        MoveWindow(GetDlgItem(hwnd, IDC_VIEW_COPY), client.right - 182, client.bottom - 38, 80, 28, TRUE);
        MoveWindow(GetDlgItem(hwnd, IDC_VIEW_USE), client.right - 272, client.bottom - 38, 80, 28, TRUE);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_VIEW_LIST && HIWORD(wParam) == LBN_SELCHANGE)
        {
            ViewerLoadPreview(state);
            return 0;
        }
        if (LOWORD(wParam) == IDC_VIEW_LIST && HIWORD(wParam) == LBN_DBLCLK)
        {
            ViewerUseSelected(state);
            return 0;
        }
        if (LOWORD(wParam) == IDC_VIEW_USE)
        {
            ViewerUseSelected(state);
            return 0;
        }
        if (LOWORD(wParam) == IDC_VIEW_COPY)
        {
            ViewerCopySelected(state);
            return 0;
        }
        if (LOWORD(wParam) == IDC_VIEW_CLOSE)
        {
            state->done = TRUE;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_PAINT:
        dc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &client);
        preview.left = 214;
        preview.top = 12;
        preview.right = client.right - 12;
        preview.bottom = client.bottom - 52;
        FillRect(dc, &preview, (HBRUSH)(COLOR_WINDOW + 1));
        if (state->preview.bitmap)
        {
            width = RectWidth(preview);
            height = MulDiv(width, state->preview.height, state->preview.width);
            if (height > RectHeight(preview))
            {
                height = RectHeight(preview);
                width = MulDiv(height, state->preview.width, state->preview.height);
            }
            preview.left += (RectWidth(preview) - width) / 2;
            preview.top += (RectHeight(preview) - height) / 2;
            preview.right = preview.left + width;
            preview.bottom = preview.top + height;
            memory = CreateCompatibleDC(dc);
            oldBitmap = SelectObject(memory, state->preview.bitmap);
            SetStretchBltMode(dc, HALFTONE);
            StretchBlt(dc, preview.left, preview.top, width, height, memory, 0, 0, state->preview.width, state->preview.height, SRCCOPY);
            SelectObject(memory, oldBitmap);
            DeleteDC(memory);
        }
        EndPaint(hwnd, &ps);
        return 0;
    case WM_CLOSE:
        state->done = TRUE;
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static BOOL ShowBurstViewer(HWND owner, const WCHAR *folder, BitmapImage *selected)
{
    BurstViewerState *state;
    MSG msg;
    BOOL result = FALSE;
    state = (BurstViewerState *)AllocZero(sizeof(BurstViewerState));
    if (!state)
    {
        return FALSE;
    }
    EnableWindow(owner, FALSE);
    CreateWindowExW(WS_EX_DLGMODALFRAME, VIEWER_CLASS, L"Burst Viewer", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 860, 640, owner, 0, g_instance, state);
    ViewerLoadFiles(state, folder);
    if (state->count > 0)
    {
        SendMessageW(state->list, LB_SETCURSEL, 0, 0);
        ViewerLoadPreview(state);
    }
    while (!state->done && GetMessageW(&msg, 0, 0, 0))
    {
        if (!IsDialogMessageW(state->hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    FreeBitmapImage(&state->preview);
    if (state->ok)
    {
        *selected = state->selected;
        ZeroMemory(&state->selected, sizeof(state->selected));
        result = TRUE;
    }
    FreeBitmapImage(&state->selected);
    FreeMem(state);
    return result;
}

static void OpenLatestBurstViewer(void)
{
    WCHAR folder[MAX_PATH];
    BitmapImage selected;
    ZeroMemory(&selected, sizeof(selected));
    if (g_lastBurstFolder[0])
    {
        lstrcpyW(folder, g_lastBurstFolder);
    }
    else if (!FindLatestBurstFolder(folder))
    {
        SetStatusText(L"No burst captures found.");
        return;
    }
    if (ShowBurstViewer(g_main, folder, &selected))
    {
        ReplaceCurrentImage(&selected);
        SetStatusText(L"Loaded burst frame. Press Ctrl+C to copy.");
    }
}

static void LayoutMain(HWND hwnd)
{
    RECT client;
    int y = 8;
    int x = 8;
    GetClientRect(hwnd, &client);
    MoveWindow(g_controls[IDC_NEW - 1000], x, y, 92, 28, TRUE); x += 98;
    MoveWindow(g_controls[IDC_MODE - 1000], x, y, 112, 220, TRUE); x += 118;
    MoveWindow(g_controls[IDC_DISPLAY - 1000], x, y, 154, 220, TRUE); x += 160;
    MoveWindow(g_controls[IDC_DELAY - 1000], x, y, 82, 220, TRUE); x += 88;
    MoveWindow(g_controls[IDC_BURST - 1000], x, y, 64, 28, TRUE); x += 70;
    MoveWindow(g_controls[IDC_VIEW_BURST - 1000], x, y, 82, 28, TRUE); x += 88;
    MoveWindow(g_controls[IDC_COPY - 1000], x, y, 62, 28, TRUE); x += 68;
    MoveWindow(g_controls[IDC_SAVE - 1000], x, y, 62, 28, TRUE); x += 68;
    MoveWindow(g_controls[IDC_MARKER - 1000], x, y, 70, 28, TRUE); x += 76;
    MoveWindow(g_controls[IDC_COLOR - 1000], x, y, 80, 220, TRUE); x += 86;
    MoveWindow(g_controls[IDC_WIDTH - 1000], x, y, 66, 220, TRUE); x += 72;
    MoveWindow(g_controls[IDC_CAPTUREBLT - 1000], x, y, 82, 28, TRUE); x += 88;
    MoveWindow(g_controls[IDC_CLEAR - 1000], x, y, 86, 28, TRUE);
    MoveWindow(g_controls[IDC_STATUS - 1000], 8, client.bottom - 24, client.right - 16, 20, TRUE);
}

static void AddComboItem(HWND combo, const WCHAR *text)
{
    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)text);
}

static HWND CreateControl(HWND parent, const WCHAR *className, const WCHAR *text, DWORD style, int id)
{
    HWND control = CreateWindowW(className, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, g_instance, 0);
    g_controls[id - 1000] = control;
    return control;
}

static void CreateMainControls(HWND hwnd)
{
    HWND combo;
    CreateControl(hwnd, L"BUTTON", L"New", BS_PUSHBUTTON, IDC_NEW);
    combo = CreateControl(hwnd, L"COMBOBOX", 0, CBS_DROPDOWNLIST | WS_VSCROLL, IDC_MODE);
    AddComboItem(combo, L"Rectangle");
    AddComboItem(combo, L"Freeform");
    AddComboItem(combo, L"Window");
    AddComboItem(combo, L"Display");
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
    combo = CreateControl(hwnd, L"COMBOBOX", 0, CBS_DROPDOWNLIST | WS_VSCROLL, IDC_DISPLAY);
    AddComboItem(combo, L"Load on use");
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
    combo = CreateControl(hwnd, L"COMBOBOX", 0, CBS_DROPDOWNLIST | WS_VSCROLL, IDC_DELAY);
    AddComboItem(combo, L"No delay");
    AddComboItem(combo, L"1 sec");
    AddComboItem(combo, L"3 sec");
    AddComboItem(combo, L"5 sec");
    AddComboItem(combo, L"10 sec");
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
    CreateControl(hwnd, L"BUTTON", L"Burst", BS_PUSHBUTTON, IDC_BURST);
    CreateControl(hwnd, L"BUTTON", L"View", BS_PUSHBUTTON, IDC_VIEW_BURST);
    CreateControl(hwnd, L"BUTTON", L"Copy", BS_PUSHBUTTON, IDC_COPY);
    CreateControl(hwnd, L"BUTTON", L"Save", BS_PUSHBUTTON, IDC_SAVE);
    CreateControl(hwnd, L"BUTTON", L"Marker", BS_AUTOCHECKBOX | BS_PUSHLIKE, IDC_MARKER);
    combo = CreateControl(hwnd, L"COMBOBOX", 0, CBS_DROPDOWNLIST | WS_VSCROLL, IDC_COLOR);
    AddComboItem(combo, L"Red");
    AddComboItem(combo, L"Yellow");
    AddComboItem(combo, L"Blue");
    AddComboItem(combo, L"Black");
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
    combo = CreateControl(hwnd, L"COMBOBOX", 0, CBS_DROPDOWNLIST | WS_VSCROLL, IDC_WIDTH);
    AddComboItem(combo, L"3 px");
    AddComboItem(combo, L"6 px");
    AddComboItem(combo, L"10 px");
    AddComboItem(combo, L"16 px");
    SendMessageW(combo, CB_SETCURSEL, 1, 0);
    CreateControl(hwnd, L"BUTTON", L"Layered", BS_AUTOCHECKBOX | BS_PUSHLIKE, IDC_CAPTUREBLT);
    CreateControl(hwnd, L"BUTTON", L"Clear", BS_PUSHBUTTON, IDC_CLEAR);
    CreateControl(hwnd, L"STATIC", L"Ready.", SS_LEFT, IDC_STATUS);
}

static void PaintMain(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc;
    RECT client;
    RECT preview;
    HDC memory;
    HGDIOBJ oldBitmap;
    dc = BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &client);
    FillRect(dc, &client, (HBRUSH)(COLOR_BTNFACE + 1));
    GetPreviewRect(hwnd, &preview);
    FillRect(dc, &preview, (HBRUSH)(COLOR_WINDOW + 1));
    if (g_image.bitmap)
    {
        memory = CreateCompatibleDC(dc);
        oldBitmap = SelectObject(memory, g_image.bitmap);
        SetStretchBltMode(dc, HALFTONE);
        StretchBlt(dc, preview.left, preview.top, RectWidth(preview), RectHeight(preview), memory, 0, 0, g_image.width, g_image.height, SRCCOPY);
        SelectObject(memory, oldBitmap);
        DeleteDC(memory);
        DrawStrokes(dc, preview, g_image.width, g_image.height);
    }
    else
    {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(110, 116, 126));
        DrawTextW(dc, L"New Capture", 11, &preview, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    POINT imagePoint;
    Stroke *stroke;
    switch (message)
    {
    case WM_CREATE:
        g_main = hwnd;
        CreateMainControls(hwnd);
        LayoutMain(hwnd);
        return 0;
    case WM_SIZE:
        LayoutMain(hwnd);
        InvalidateRect(hwnd, 0, TRUE);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_NEW)
        {
            RunSingleCapture();
            return 0;
        }
        if (LOWORD(wParam) == IDC_BURST)
        {
            RunBurstCapture();
            return 0;
        }
        if (LOWORD(wParam) == IDC_VIEW_BURST)
        {
            OpenLatestBurstViewer();
            return 0;
        }
        if (LOWORD(wParam) == IDC_COPY)
        {
            CopyCurrentImage();
            return 0;
        }
        if (LOWORD(wParam) == IDC_SAVE)
        {
            SaveCurrentImage();
            return 0;
        }
        if (LOWORD(wParam) == IDC_MARKER)
        {
            g_markerEnabled = SendMessageW(g_controls[IDC_MARKER - 1000], BM_GETCHECK, 0, 0) == BST_CHECKED;
            SetStatusText(g_markerEnabled ? L"Marker enabled." : L"Marker disabled.");
            return 0;
        }
        if (LOWORD(wParam) == IDC_COLOR && HIWORD(wParam) == CBN_SELCHANGE)
        {
            g_markerColor = GetSelectedColor();
            return 0;
        }
        if (LOWORD(wParam) == IDC_WIDTH && HIWORD(wParam) == CBN_SELCHANGE)
        {
            g_markerWidth = GetSelectedWidth();
            return 0;
        }
        if (LOWORD(wParam) == IDC_DISPLAY && HIWORD(wParam) == CBN_DROPDOWN)
        {
            EnsureMonitorsLoaded();
            return 0;
        }
        if (LOWORD(wParam) == IDC_CLEAR)
        {
            FreeStrokes();
            InvalidateRect(hwnd, 0, TRUE);
            SetStatusText(L"Marks cleared.");
            return 0;
        }
        if (LOWORD(wParam) == IDC_CAPTUREBLT)
        {
            g_includeLayeredWindows = SendMessageW(g_controls[IDC_CAPTUREBLT - 1000], BM_GETCHECK, 0, 0) == BST_CHECKED;
            SetStatusText(g_includeLayeredWindows ? L"Layered capture enabled." : L"Layered capture disabled.");
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (g_markerEnabled && g_image.bitmap && ClientToImagePoint(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &imagePoint))
        {
            stroke = (Stroke *)AllocZero(sizeof(Stroke));
            if (stroke)
            {
                stroke->color = g_markerColor;
                stroke->width = g_markerWidth;
                StrokeAppend(stroke, imagePoint);
                stroke->next = g_strokes;
                g_strokes = stroke;
                g_activeStroke = stroke;
                SetCapture(hwnd);
            }
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (g_activeStroke && (wParam & MK_LBUTTON) && ClientToImagePoint(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &imagePoint))
        {
            if (g_activeStroke->count == 0 ||
                AbsInt(g_activeStroke->points[g_activeStroke->count - 1].x - imagePoint.x) + AbsInt(g_activeStroke->points[g_activeStroke->count - 1].y - imagePoint.y) >= 1)
            {
                StrokeAppend(g_activeStroke, imagePoint);
                InvalidateRect(hwnd, 0, FALSE);
            }
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (g_activeStroke)
        {
            g_activeStroke = 0;
            ReleaseCapture();
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (wParam == 'C' && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            CopyCurrentImage();
            return 0;
        }
        if (wParam == 'N' && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            RunSingleCapture();
            return 0;
        }
        break;
    case WM_PAINT:
        PaintMain(hwnd);
        return 0;
    case WM_DESTROY:
        FreeBitmapImage(&g_image);
        FreeStrokes();
        ReleaseDxgiDeviceCache();
        FreeMem(g_scratchPoints);
        g_scratchPoints = 0;
        g_scratchPointCapacity = 0;
        if (g_avrtModule)
        {
            FreeLibrary(g_avrtModule);
            g_avrtModule = 0;
            g_avSetMmThreadCharacteristics = 0;
            g_avRevertMmThreadCharacteristics = 0;
        }
        if (g_winmmModule)
        {
            FreeLibrary(g_winmmModule);
            g_winmmModule = 0;
            g_timeBeginPeriod = 0;
            g_timeEndPeriod = 0;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static void RegisterWindowClasses(void)
{
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(0, IDC_ARROW);
    wc.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(1));
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = APP_CLASS;
    wc.lpfnWndProc = MainProc;
    RegisterClassW(&wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(0, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = OVERLAY_CLASS;
    wc.lpfnWndProc = OverlayProc;
    RegisterClassW(&wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = PICKER_CLASS;
    wc.lpfnWndProc = PickerProc;
    RegisterClassW(&wc);

    wc.lpszClassName = BURST_DIALOG_CLASS;
    wc.lpfnWndProc = BurstDialogProc;
    RegisterClassW(&wc);

    wc.lpszClassName = VIEWER_CLASS;
    wc.lpfnWndProc = BurstViewerProc;
    RegisterClassW(&wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = BURST_MARKER_CLASS;
    wc.lpfnWndProc = BurstMarkerProc;
    RegisterClassW(&wc);
}

static int RunSelfTest(void)
{
    RECT bounds;
    CaptureTarget target;
    WCHAR folder[MAX_PATH];
    WCHAR path[MAX_PATH];
    BitmapImage image;
    BitmapImage loaded;
    int saved;
    BOOL ok;
    bounds.left = 0;
    bounds.top = 0;
    bounds.right = 8;
    bounds.bottom = 8;
    ZeroMemory(&image, sizeof(image));
    ZeroMemory(&loaded, sizeof(loaded));
    GetTempPathW(MAX_PATH, folder);
    if (folder[lstrlenW(folder) - 1] != L'\\') lstrcatW(folder, L"\\");
    lstrcpyW(path, folder);
    lstrcatW(path, L"wincap_native_selftest.bmp");
    ok = CaptureRectToImage(bounds, &image);
    ok = ok && SaveBmpImage(path, &image);
    ok = ok && LoadBmpImage(path, &loaded);
    ok = ok && loaded.width == 8 && loaded.height == 8;
    FreeBitmapImage(&image);
    FreeBitmapImage(&loaded);
    DeleteFileW(path);

    lstrcpyW(path, folder);
    lstrcatW(path, L"wincap_native_burst_selftest");
    CreateDirectoryW(path, 0);
    ZeroMemory(&target, sizeof(target));
    target.mode = MODE_DISPLAY;
    target.bounds = bounds;
    saved = CaptureBurstFrames(&target, 1, 500, path);
    ok = ok && saved >= 1;
    MakeFramePath(path, 0, folder);
    DeleteFileW(folder);
    MakeFramePath(path, 1, folder);
    DeleteFileW(folder);
    RemoveDirectoryW(path);
    return ok ? 0 : 1;
}

static int RunDxgiSelfTest(void)
{
    MONITORINFO monitorInfo;
    POINT point;
    HMONITOR monitor;
    CaptureTarget target;
    WCHAR folder[MAX_PATH];
    WCHAR path[MAX_PATH];
    int firstSaved;
    int secondSaved;

    point.x = 0;
    point.y = 0;
    monitor = MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY);
    ZeroMemory(&monitorInfo, sizeof(monitorInfo));
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo))
    {
        return 1;
    }

    GetTempPathW(MAX_PATH, folder);
    if (folder[lstrlenW(folder) - 1] != L'\\') lstrcatW(folder, L"\\");
    lstrcatW(folder, L"wincap_native_dxgi_selftest");
    CreateDirectoryW(folder, 0);

    ZeroMemory(&target, sizeof(target));
    target.mode = MODE_DISPLAY;
    target.bounds = monitorInfo.rcMonitor;

    firstSaved = CaptureBurstFrames(&target, 1, 500, folder);
    secondSaved = CaptureBurstFrames(&target, 1, 500, folder);
    MakeFramePath(folder, 0, path);
    DeleteFileW(path);
    MakeFramePath(folder, 1, path);
    DeleteFileW(path);
    RemoveDirectoryW(folder);

    return firstSaved >= 1 && secondSaved >= 1 && g_lastBurstUsedDxgi ? 0 : 2;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previousInstance, PWSTR commandLine, int showCommand)
{
    HWND hwnd;
    MSG msg;
    g_instance = instance;
    SetProcessDPIAware();
    if (ContainsTextInsensitive(GetCommandLineW(), L"--self-test"))
    {
        if (ContainsTextInsensitive(GetCommandLineW(), L"--self-test-dxgi"))
        {
            return RunDxgiSelfTest();
        }
        return RunSelfTest();
    }
    RegisterWindowClasses();
    hwnd = CreateWindowExW(0, APP_CLASS, L"WinCap Native", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1040, 720, 0, 0, instance, 0);
    if (!hwnd)
    {
        return 1;
    }
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);
    while (GetMessageW(&msg, 0, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

void WINAPI wWinMainCRTStartup(void)
{
    STARTUPINFOW startup;
    int showCommand;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    GetStartupInfoW(&startup);
    showCommand = (startup.dwFlags & STARTF_USESHOWWINDOW) ? startup.wShowWindow : SW_SHOWDEFAULT;
    ExitProcess((UINT)wWinMain(GetModuleHandleW(0), 0, GetCommandLineW(), showCommand));
}
