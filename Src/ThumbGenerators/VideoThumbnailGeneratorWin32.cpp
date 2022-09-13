#include "VideoThumbnailGeneratorWin32.h"
#include <gdiplus.h>
#pragma warning(disable:4127)  // Disable warning C4127: conditional expression is constant

RECT correctAspectRatio(const RECT& src, const MFRatio& srcPAR);

const LONGLONG SEEK_TOLERANCE = 10000000;
const LONGLONG MAX_FRAMES_TO_SKIP = 10;

//-------------------------------------------------------------------
// ThumbnailGenerator constructor
//-------------------------------------------------------------------

VideoThumbnailGeneratorWin32::VideoThumbnailGeneratorWin32()
    : pReader_(NULL)
{
    ZeroMemory(&format_, sizeof(format_));

    HRESULT hr = S_OK;

    // Initialize COM
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    if (SUCCEEDED(hr))
    {
        // Initialize Media Foundation.
        MFStartup(MF_VERSION);
    }
}

//-------------------------------------------------------------------
// ThumbnailGenerator destructor
//-------------------------------------------------------------------

VideoThumbnailGeneratorWin32::~VideoThumbnailGeneratorWin32()
{
    SafeRelease(&pReader_);    
    MFShutdown();
    CoUninitialize();
}

bool VideoThumbnailGeneratorWin32::init(HWND hwnd)
{
    hwnd_ = hwnd;
    return true;
}

//-------------------------------------------------------------------
// OpenFile: Opens a video file.
//-------------------------------------------------------------------

HRESULT VideoThumbnailGeneratorWin32::openFile(const WCHAR* wszFileName)
{
    HRESULT hr = S_OK;

    IMFAttributes *pAttributes = NULL;

    SafeRelease(&pReader_);

    // Configure the source reader to perform video processing.
    //
    // This includes:
    //   - YUV to RGB-32
    //   - Software deinterlace

    hr = MFCreateAttributes(&pAttributes, 1);

    if (SUCCEEDED(hr))
    {
        hr = pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    }

    // Create the source reader from the URL.

    if (SUCCEEDED(hr))
    {
        hr = MFCreateSourceReaderFromURL(wszFileName, pAttributes, &pReader_);
    }

    if (SUCCEEDED(hr))
    {
        // Attempt to find a video stream.
        hr = selectVideoStream();
    }
    
    return hr;
}
bool VideoThumbnailGeneratorWin32::getThumbs(QString path, int count, QImage images[]) {
    return getThumbs(path.toStdWString().c_str(), count, images);
}

bool VideoThumbnailGeneratorWin32::getThumbs(const WCHAR* wszFileName, DWORD count, QImage images[])
{
    HRESULT hr = S_OK;
    hr = openFile(wszFileName);
    if (FAILED(hr))
    {
        return false;
    }

    // Generate new thumbs.

    if (SUCCEEDED(hr))
    {
        if (pRT_ == NULL)
        {
            // Create the Direct2D resources.
            if (FAILED(createDrawingResources(hwnd_)))
                return false;
        }

        assert(pRT_ != NULL);

        hr = createThumbs(pRT_, 1, images);

        SafeRelease(&pRT_);

        if (SUCCEEDED(hr))
        {
            return true;
        }
    }
    return false;
}

//-------------------------------------------------------------------
// GetDuration: Finds the duration of the current video file.
//-------------------------------------------------------------------

HRESULT VideoThumbnailGeneratorWin32::getDuration(LONGLONG *phnsDuration)
{
    PROPVARIANT var;
    PropVariantInit(&var);

    HRESULT hr = S_OK;

    if (pReader_ == NULL)
    {
        return MF_E_NOT_INITIALIZED;
    }
    
    hr = pReader_->GetPresentationAttribute(
        (DWORD)MF_SOURCE_READER_MEDIASOURCE, 
        MF_PD_DURATION, 
        &var 
        );

    if (SUCCEEDED(hr))
    {
        assert(var.vt == VT_UI8);
        *phnsDuration = var.hVal.QuadPart;
    }

    PropVariantClear(&var);

    return hr; 
}

//-------------------------------------------------------------------
// CanSeek: Queries whether the current video file is seekable.
//-------------------------------------------------------------------

HRESULT VideoThumbnailGeneratorWin32::canSeek(BOOL *pbCanSeek)
{
    HRESULT hr = S_OK;

    ULONG flags = 0;

    PROPVARIANT var;
    PropVariantInit(&var);

    if (pReader_ == NULL)
    {
        return MF_E_NOT_INITIALIZED;
    }

    *pbCanSeek = FALSE;

    hr = pReader_->GetPresentationAttribute(
        (DWORD)MF_SOURCE_READER_MEDIASOURCE,
        MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS,
        &var
        );

    if (SUCCEEDED(hr))
    {
        hr = PropVariantToUInt32(var, &flags);
    }

    if (SUCCEEDED(hr))
    {
        // If the source has slow seeking, we will treat it as
        // not supporting seeking. 

        if ((flags & MFMEDIASOURCE_CAN_SEEK) && 
            !(flags & MFMEDIASOURCE_HAS_SLOW_SEEK))
        {
            *pbCanSeek = TRUE;
        }
    }

    return hr;
}

//-------------------------------------------------------------------
// CreateBitmaps
//
// Creates an array of thumbnails from the video file.
//
// pRT:      Direct2D render target. Used to create the bitmaps.
// count:    Number of thumbnails to create.
// pSprites: An array of Sprite objects to hold the bitmaps.
//
// Note: The caller allocates the sprite objects.
//-------------------------------------------------------------------

HRESULT VideoThumbnailGeneratorWin32::createThumbs(
    ID2D1RenderTarget *pRT, 
    DWORD count, 
    QImage images[]
    )
{
    HRESULT hr = S_OK;
    BOOL bCanSeek = 0;

    LONGLONG hnsDuration = 0;
    LONGLONG hnsRangeStart = 0;
    LONGLONG hnsRangeEnd = 0;
    LONGLONG hnsIncrement = 0;

    hr = canSeek(&bCanSeek);

    if (FAILED(hr)) { return hr; }

    if (aborted_) { return -1;}
    if (bCanSeek)
    {
        hr = getDuration(&hnsDuration);

        if (FAILED(hr)) { return hr; }

        hnsRangeStart = 0;
        hnsRangeEnd = hnsDuration;

        // We have the file duration , so we'll take bitmaps from
        // several positions in the file. Occasionally, the first frame 
        // in a video is black, so we don't start at time 0.

        hnsIncrement = (hnsRangeEnd - hnsRangeStart) / (count + 1);

    }

    // Generate the bitmaps and invalidate the button controls so
    // they will be redrawn.
    for (DWORD i = 0; i < count; i++)
    {
        if (aborted_) { return -1;}
        LONGLONG hPos = hnsIncrement * (i + 1);

        hr = createThumb(
            pRT, 
            hPos, 
            &images[i]
        );
    }

    return hr;
}

//
// Private methods
//

//-------------------------------------------------------------------
// CreateBitmap
//
// Creates one video thumbnail.
//
// pRT:      Direct2D render target. Used to create the bitmap.
// hnsPos:   The seek position.
// pSprite:  A Sprite object to hold the bitmap.
//-------------------------------------------------------------------

HRESULT VideoThumbnailGeneratorWin32::createThumb(
    ID2D1RenderTarget *pRT, 
    LONGLONG& hnsPos, 
    QImage *image
    )
{
    HRESULT     hr = S_OK;
    DWORD       dwFlags = 0;

    BYTE        *pBitmapData = NULL;    // Bitmap data
    DWORD       cbBitmapData = 0;       // Size of data, in bytes
    LONGLONG    hnsTimeStamp = 0;
    BOOL        bCanSeek = FALSE;       // Can the source seek?  
    DWORD       cSkipped = 0;           // Number of skipped frames

    IMFMediaBuffer *pBuffer = 0;
    IMFSample *pSample = NULL;
    ID2D1Bitmap *pBitmap = NULL;

    hr = canSeek(&bCanSeek);
    if (FAILED(hr)) 
    { 
        return hr; 
    }

    if (aborted_) { return -1;}

    if (bCanSeek && (hnsPos > 0))
    {
        PROPVARIANT var;
        PropVariantInit(&var);

        var.vt = VT_I8;
        var.hVal.QuadPart = hnsPos;

        hr = pReader_->SetCurrentPosition(GUID_NULL, var);

        if (FAILED(hr)) { goto done; }

    }

    // Pulls video frames from the source reader.

    // NOTE: Seeking might be inaccurate, depending on the container
    //       format and how the file was indexed. Therefore, the first
    //       frame that we get might be earlier than the desired time.
    //       If so, we skip up to MAX_FRAMES_TO_SKIP frames.

    while (1)
    {
        if (aborted_) { hr = -1; goto done; }

        IMFSample *pSampleTmp = NULL;

        hr = pReader_->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 
            0, 
            NULL, 
            &dwFlags, 
            NULL, 
            &pSampleTmp
            );
    
        if (FAILED(hr)) { goto done; }

        if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM)
        {
            break;
        }

        if (dwFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
        {
            // Type change. Get the new format.
            hr = getVideoFormat(&format_);
        
            if (FAILED(hr)) { goto done; }
        }

        if (pSampleTmp == NULL)
        {
            continue;
        }

        // We got a sample. Hold onto it.

        SafeRelease(&pSample);

        if (aborted_) { hr = -1; goto done; }

        pSample = pSampleTmp;
        pSample->AddRef();

        if (SUCCEEDED( pSample->GetSampleTime(&hnsTimeStamp) ))
        {
            // Keep going until we get a frame that is within tolerance of the
            // desired seek position, or until we skip MAX_FRAMES_TO_SKIP frames.

            // During this process, we might reach the end of the file, so we
            // always cache the last sample that we got (pSample).

            if ( (cSkipped < MAX_FRAMES_TO_SKIP) && 
                 (hnsTimeStamp + SEEK_TOLERANCE < hnsPos) )  
            {
                SafeRelease(&pSampleTmp);

                ++cSkipped;
                continue;
            }
        }

        SafeRelease(&pSampleTmp);

        hnsPos = hnsTimeStamp;
        break;
    }

    if (aborted_) { hr = -1; goto done; }

    if (pSample)
    {
        UINT32 pitch = 4 * format_.imageWidthPels;

        // Get the bitmap data from the sample, and use it to create a
        // Direct2D bitmap object. Then use the Direct2D bitmap to 
        // initialize the sprite.

        hr = pSample->ConvertToContiguousBuffer(&pBuffer);

        if (FAILED(hr)) { goto done; }

        hr = pBuffer->Lock(&pBitmapData, NULL, &cbBitmapData);
        
        if (FAILED(hr)) { goto done; }

        assert(cbBitmapData == (pitch * format_.imageHeightPels));

        if (aborted_) { hr = -1; goto done; }

        hr = pRT->CreateBitmap( 
            D2D1::SizeU(format_.imageWidthPels, format_.imageHeightPels),
            pBitmapData,
            pitch,
            D2D1::BitmapProperties( 
                // Format = RGB32
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE) 
                ),
            &pBitmap
            );

        if (FAILED(hr)) { goto done; }

        if (aborted_) { hr = -1; goto done; }

        QImage tmpImage = QImage(pBitmapData, format_.imageWidthPels, format_.imageHeightPels, format_.imageWidthPels*4, QImage::Format_RGB32);
        *image = tmpImage.scaled(200, 200, Qt::KeepAspectRatio);
    }
    else
    {
        hr = MF_E_END_OF_STREAM;
    }

done:

    if (pBitmapData)
    {
        pBuffer->Unlock();
    }
    SafeRelease(&pBuffer);
    SafeRelease(&pSample);
    SafeRelease(&pBitmap);

    return hr;
}

//-------------------------------------------------------------------
// SelectVideoStream
//
// Finds the first video stream and sets the format to RGB32.
//-------------------------------------------------------------------

HRESULT VideoThumbnailGeneratorWin32::selectVideoStream()
{
    HRESULT hr = S_OK;

    IMFMediaType *pType = NULL;

    // Configure the source reader to give us progressive RGB32 frames.
    // The source reader will load the decoder if needed.

    hr = MFCreateMediaType(&pType);

    if (SUCCEEDED(hr))
    {
        hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    }

    if (SUCCEEDED(hr))
    {
        hr = pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    }


    if (SUCCEEDED(hr))
    {
        hr = pReader_->SetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 
            NULL,
            pType
            );
    }

    // Ensure the stream is selected.
    if (SUCCEEDED(hr))
    {
        hr = pReader_->SetStreamSelection(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 
            TRUE
            );
    }

    if (SUCCEEDED(hr))
    {
        hr = getVideoFormat(&format_);
    }

    SafeRelease(&pType);
    return hr;
}

HRESULT VideoThumbnailGeneratorWin32::createDrawingResources(HWND hwnd)
{
    HRESULT hr = S_OK;
    RECT rcClient = { 0 };

    ID2D1Factory *pFactory = NULL;
    ID2D1HwndRenderTarget *pRenderTarget = NULL;

    GetClientRect(hwnd, &rcClient);

    hr = D2D1CreateFactory(
                D2D1_FACTORY_TYPE_SINGLE_THREADED,
                &pFactory
                );

    if (SUCCEEDED(hr))
    {
        hr = pFactory->CreateHwndRenderTarget(
                    D2D1::RenderTargetProperties(),
                    D2D1::HwndRenderTargetProperties(
                        hwnd,
                        D2D1::SizeU(rcClient.right, rcClient.bottom)
                        ),
                    &pRenderTarget
                    );
    }

    if (SUCCEEDED(hr))
    {
        pRT_ = pRenderTarget;
        pRT_->AddRef();
    }

    SafeRelease(&pFactory);
    SafeRelease(&pRenderTarget);
    return hr;
}


//-------------------------------------------------------------------
// GetVideoFormat
// 
// Gets format information for the video stream.
//
// iStream: Stream index.
// pFormat: Receives the format information.
//-------------------------------------------------------------------

HRESULT VideoThumbnailGeneratorWin32::getVideoFormat(FormatInfo *pFormat)
{
    HRESULT hr = S_OK;
    UINT32  width = 0, height = 0;
    LONG lStride = 0;
    MFRatio par = { 0 , 0 };

    FormatInfo& format = *pFormat;

    GUID subtype = { 0 };

    IMFMediaType *pType = NULL;

    // Get the media type from the stream.
    hr = pReader_->GetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 
        &pType
        );
    
    if (FAILED(hr)) { goto done; }

    // Make sure it is a video format.
    hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);

    if (FAILED(hr)) { goto done; }

    if (subtype != MFVideoFormat_RGB32)
    {
        hr = E_UNEXPECTED;
        goto done;
    }

    // Get the width and height
    hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);

    if (FAILED(hr)) { goto done; }

    // Get the stride to find out if the bitmap is top-down or bottom-up.
    lStride = (LONG)MFGetAttributeUINT32(pType, MF_MT_DEFAULT_STRIDE, 1);

    format.bTopDown = (lStride > 0); 

    // Get the pixel aspect ratio. (This value might not be set.)
    hr = MFGetAttributeRatio(pType, MF_MT_PIXEL_ASPECT_RATIO, (UINT32*)&par.Numerator, (UINT32*)&par.Denominator);
    if (SUCCEEDED(hr) && (par.Denominator != par.Numerator))
    {
        RECT rcSrc = { 0, 0, (LONG)width, (LONG)height };

        format.rcPicture = correctAspectRatio(rcSrc, par);
    }
    else
    {
        // Either the PAR is not set (assume 1:1), or the PAR is set to 1:1.
        SetRect(&format.rcPicture, 0, 0, width, height);
    }

    format.imageWidthPels = width;
    format.imageHeightPels = height;

done:
    SafeRelease(&pType);

    return hr;
}

HWND VideoThumbnailGeneratorWin32::hwnd_;

//-----------------------------------------------------------------------------
// CorrectAspectRatio
//
// Converts a rectangle from the source's pixel aspect ratio (PAR) to 1:1 PAR.
// Returns the corrected rectangle.
//
// For example, a 720 x 486 rect with a PAR of 9:10, when converted to 1x1 PAR,  
// is stretched to 720 x 540. 
//-----------------------------------------------------------------------------

RECT correctAspectRatio(const RECT& src, const MFRatio& srcPAR)
{
    // Start with a rectangle the same size as src, but offset to the origin (0,0).
    RECT rc = {0, 0, src.right - src.left, src.bottom - src.top};

    if ((srcPAR.Numerator != 1) || (srcPAR.Denominator != 1))
    {
        // Correct for the source's PAR.

        if (srcPAR.Numerator > srcPAR.Denominator)
        {
            // The source has "wide" pixels, so stretch the width.
            rc.right = MulDiv(rc.right, srcPAR.Numerator, srcPAR.Denominator);
        }
        else if (srcPAR.Numerator < srcPAR.Denominator)
        {
            // The source has "tall" pixels, so stretch the height.
            rc.bottom = MulDiv(rc.bottom, srcPAR.Denominator, srcPAR.Numerator);
        }
        // else: PAR is 1:1, which is a no-op.
    }
    return rc;
}
