#pragma once
#include <QImage>
#include <windows.h>
#include <windowsx.h>

// Media Foundation 
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

// Shell
#include <shobjidl.h>
#include <shellapi.h> 

// Direct2D
#include <D2d1.h>
#include <D2d1helper.h>

// Misc
#include <strsafe.h>
#include <assert.h>
#include <propvarutil.h>

template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

class VideoThumbnailGeneratorWin32
{
public:

    VideoThumbnailGeneratorWin32();
    ~VideoThumbnailGeneratorWin32();
    static bool init(HWND hwnd);

    HRESULT     createThumbs(ID2D1RenderTarget *pRT, DWORD count, QImage images[]);
    HRESULT     openFile(const WCHAR* wszFileName);
    bool        getThumbs(const WCHAR* wszFileName, DWORD count, QImage images[]);
    void        abort() {aborted_ = true;}

protected:
    HRESULT     createThumb(ID2D1RenderTarget *pRT, LONGLONG& hnsPos, QImage *image);
    HRESULT     getDuration(LONGLONG *phnsDuration);
    HRESULT     canSeek(BOOL *pbCanSeek);
    HRESULT     selectVideoStream();
    HRESULT     createDrawingResources(HWND hwnd);

    struct FormatInfo
    {
        UINT32          imageWidthPels;
        UINT32          imageHeightPels;
        BOOL            bTopDown;
        RECT            rcPicture;    // Corrected for pixel aspect ratio

        FormatInfo() : imageWidthPels(0), imageHeightPels(0), bTopDown(FALSE)
        {
            SetRectEmpty(&rcPicture);
        }
    };
    HRESULT     getVideoFormat(FormatInfo *pFormat);

    IMFSourceReader *pReader_ = NULL;
    FormatInfo      format_;

    ID2D1HwndRenderTarget *pRT_ = NULL;      // Render target for D2D animation

    static HWND hwnd_;
    volatile bool aborted_ = false;
};






