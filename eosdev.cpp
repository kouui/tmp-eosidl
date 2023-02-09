

#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <string>

#include <atlimage.h>

#include "EDSDK.h"
#include "EDSDKErrors.h"
#include "EDSDKTypes.h"


typedef struct _EVF_DATASET
{
    EdsStreamRef	stream; // JPEG stream.
    EdsUInt32		zoom;
    EdsRect			zoomRect;
    EdsPoint		imagePosition;
    EdsUInt32		histogram[256 * 4]; //(YRGB) YRGBYRGBYRGBYRGB....
    EdsSize			sizeJpegLarge;
}EVF_DATASET;



class CameraEventListener
{
public:
    static EdsError EDSCALLBACK  handleObjectEvent(
        EdsUInt32			inEvent,
        EdsBaseRef			inRef,
        EdsVoid* inContext
    )
    {

        //std::cout << "Object Callback : " << inEvent  << "\n";
        

        return EDS_ERR_OK;
    }

    static EdsError EDSCALLBACK  handlePropertyEvent(
        EdsUInt32			inEvent,
        EdsUInt32			inPropertyID,
        EdsUInt32			inParam,
        EdsVoid* inContext
    )
    {
        if (inPropertyID == kEdsPropID_Evf_OutputDevice) {
            std::cout << "Property Callback : " << inEvent << " " << inPropertyID << "\n";
        }
        

        return EDS_ERR_OK;
    }

    static EdsError EDSCALLBACK  handleStateEvent(
        EdsUInt32			inEvent,
        EdsUInt32			inParam,
        EdsVoid* inContext
    )
    {

        
        std::cout << "State Callback : " << inEvent << "\n";

        return EDS_ERR_OK;
    }

};

EdsError getFirstCamera(EdsCameraRef* camera)
{
    EdsError err = EDS_ERR_OK;
    EdsCameraListRef cameraList = NULL;
    EdsUInt32 count = 0;
    // カメラリストの取得
    err = EdsGetCameraList(&cameraList);
    // カメラの数の取得
    if (err == EDS_ERR_OK)
    {
        err = EdsGetChildCount(cameraList, &count);
        if (count == 0)
        {
            err = EDS_ERR_DEVICE_NOT_FOUND;
        }
    }
    // 最初のカメラを取得
    if (err == EDS_ERR_OK)
    {
        err = EdsGetChildAtIndex(cameraList, 0, camera);
    }
    // カメラリストのリリース
    if (cameraList != NULL)
    {
        EdsRelease(cameraList);
        cameraList = NULL;
    }
    return err;
}

EdsError startLiveview(EdsCameraRef camera)
{
    EdsError err = EDS_ERR_OK;
    // LiveView 画像の出力デバイスを取得する
    EdsUInt32 device;
    err = EdsGetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
    // PCLiveView は、LiveView 画像の出力デバイスとして PC を設定することから始まる
    if (err == EDS_ERR_OK)
    {
        device |= kEdsEvfOutputDevice_PC;
        err = EdsSetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);

        if (err == EDS_ERR_OK) std::cout << "live View started\n";
    }
    // プロパティ設定が正常に行われた場合、カメラからプロパティ変更イベント通知が発行されます。
    // プロパティ変更通知が到着したら、LiveView 画像のダウンロードを開始します。
    return err;
}

EdsError downloadEvfData(EdsCameraRef camera)
{
    EdsError err = EDS_ERR_OK;
    EdsStreamRef stream = NULL;
    EdsEvfImageRef evfImage = NULL;
    // メモリストリームの作成
    err = EdsCreateMemoryStream(0, &stream);
    // EvfImageRef の作成
    if (err == EDS_ERR_OK)
    {
        err = EdsCreateEvfImageRef(stream, &evfImage);
    }
    // LiveView 画像データのダウンロード
    if (err == EDS_ERR_OK)
    {
        err = EdsDownloadEvfImage(camera, evfImage);
    }
    // image buffer
    if (err == EDS_ERR_OK)
    {
        EVF_DATASET dataSet = { 0 };
        dataSet.stream = stream;
        // Get magnification ratio (x1, x5, or x10).
        EdsGetPropertyData(evfImage, kEdsPropID_Evf_Zoom, 0, sizeof(dataSet.zoom), &dataSet.zoom);
        std::cout << "zoom ratio =" << dataSet.zoom  << "\n";
        // Get position of image data. (when enlarging)
        // Upper left coordinate using JPEG Large size as a reference.
        EdsGetPropertyData(evfImage, kEdsPropID_Evf_ImagePosition, 0, sizeof(dataSet.imagePosition), &dataSet.imagePosition);
        // Get histogram (RGBY).
        EdsGetPropertyData(evfImage, kEdsPropID_Evf_Histogram, 0, sizeof(dataSet.histogram), dataSet.histogram);
        // Get rectangle of the focus border.
        EdsGetPropertyData(evfImage, kEdsPropID_Evf_ZoomRect, 0, sizeof(dataSet.zoomRect), &dataSet.zoomRect);
        std::cout << "zoomPoint = (" << dataSet.zoomRect.point.x << "," << dataSet.zoomRect.point.y << ")\n";
        std::cout << "zoomRect : height = " << dataSet.zoomRect.size.height << ", width = " << dataSet.zoomRect.size.width << "\n";
        // point.x * 2 + size.width  = 5472
        // point.y * 2 + size.height = 3648
        // 記録画素数（L）	5,472x3,648ピクセル EOS 7D Mark II, reference : https://car.watch.impress.co.jp/docs/special/675096.html
        // Get the size as a reference of the coordinates of rectangle of the focus border.
        EdsGetPropertyData(evfImage, kEdsPropID_Evf_CoordinateSystem, 0, sizeof(dataSet.sizeJpegLarge), &dataSet.sizeJpegLarge);

        // Get image (JPEG) pointer.
        unsigned char* pbyteImage = NULL;
        EdsGetPointer(dataSet.stream, (EdsVoid**)&pbyteImage);

        if (pbyteImage != NULL)
        {
            EdsUInt64 size;
            EdsGetLength(dataSet.stream, &size);
            std::cout << "pbyteImage has size = " << size << "\n";

            // create cImage object
            {
                CImage image;
                CComPtr<IStream> stream1;
                stream = NULL;

                HGLOBAL hMem = ::GlobalAlloc(GHND, size);
                LPVOID pBuff = ::GlobalLock(hMem);

                memcpy(pBuff, pbyteImage, size);

                ::GlobalUnlock(hMem);
                CreateStreamOnHGlobal(hMem, TRUE, &stream1);

                image.Load(stream1);

                std::cout << "image height = " << image.GetHeight() << ", width = " << image.GetWidth() << "\n";

                image.Destroy();

                ::GlobalFree(hMem);
            }
        }
    }
       
    // stream の解放
    if (stream != NULL)
    {
        EdsRelease(stream);
        stream = NULL;
    }
    // evfImage の解放
    if (evfImage != NULL)
    {
        EdsRelease(evfImage);
        evfImage = NULL;
    }
    return err;
}

EdsError endLiveview(EdsCameraRef camera)
{
    EdsError err = EDS_ERR_OK;
    // LiveView 画像の出力デバイスの取得
    EdsUInt32 device;
    err = EdsGetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
    // PC が LiveView 画像出力デバイスから切断された場合、PCLiveView は終了する
    if (err == EDS_ERR_OK)
    {
        device &= ~kEdsEvfOutputDevice_PC;
        err = EdsSetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
        if (err == EDS_ERR_OK) std::cout << "live View ended\n";
    }
    return err;
}








int main()
{
    EdsError err = EDS_ERR_OK;
    EdsCameraRef camera = NULL;
    bool isSDKLoaded = false;

    // SDK 初期化
    err = EdsInitializeSDK();
    if (err == EDS_ERR_OK)
    {
        isSDKLoaded = true;
        std::cout << "SDK initialized." << "\n";
    }

    // 最初のカメラを取得する
    if (err == EDS_ERR_OK)
    {
        // See Sample 2.
        err = getFirstCamera(&camera);
        std::cout << "camera (first) info aquired." << "\n";
    }

    //Set Property Event Handler

    if (err == EDS_ERR_OK)
    {
        err = EdsSetPropertyEventHandler(camera, kEdsPropertyEvent_All, CameraEventListener::handlePropertyEvent, (EdsVoid*)NULL);
        if (err == EDS_ERR_OK) std::cout << "Property Event Handler registered.\n";
    }

    //Set Object Event Handler
    if (err == EDS_ERR_OK)
    {
        err = EdsSetObjectEventHandler(camera, kEdsObjectEvent_All, CameraEventListener::handleObjectEvent, (EdsVoid*)NULL);
        if (err == EDS_ERR_OK) std::cout << "Object Event Handler registered.\n";

    }

    //Set State Event Handler
    if (err == EDS_ERR_OK)
    {
        err = EdsSetCameraStateEventHandler(camera, kEdsStateEvent_All, CameraEventListener::handleStateEvent, (EdsVoid*)NULL);
        if (err == EDS_ERR_OK) std::cout << "State Event Handler registered.\n";

    }

    // カメラと接続する
    if (err == EDS_ERR_OK)
    {

        err = EdsOpenSession(camera);
        

        //Acquisition of camera information
        if (err == EDS_ERR_OK)
        {
            EdsDeviceInfo deviceInfo;
            std::cout << "camera connected." << "\n";

            err = EdsGetDeviceInfo(camera, &deviceInfo);
            if (err == EDS_ERR_OK && camera == NULL)
            {
                err = EDS_ERR_DEVICE_NOT_FOUND;
                std::cout << "camera not found." << "\n";
            }
            std::cout << "device description : " << deviceInfo.szDeviceDescription << "\n";
        }


        
        // live View
        if (err == EDS_ERR_OK)
        {
            err = startLiveview(camera);
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));

            if (err == EDS_ERR_OK)
            {
                //HANDLE hThread = (HANDLE)_beginthread(threadProc, 0, camera);

                err = downloadEvfData(camera);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            if (err == EDS_ERR_OK)
            {
                //err = downloadEvfData(camera);
            }

            err = endLiveview(camera);

        }
    }

    // カメラとの接続を解除する
    if (err == EDS_ERR_OK)
    {

        err = EdsCloseSession(camera);
        std::cout << "camera disconnected." << "\n";
    }
    // カメラのリリース
    if (camera != NULL)
    {
        EdsRelease(camera);
        std::cout << "camera released." << "\n";
    }
    // SDK 終了
    if (isSDKLoaded)
    {
        EdsTerminateSDK();
        std::cout << "SDK closed." << "\n";
    }

    std::cout << "Hello World!\n";
}

