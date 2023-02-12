/*
History:
2023.02.06    u.k.    project initialized
*/

#include "pch.h"
#include "stdio.h"
#include "idl_export.h"
#include <malloc.h>

#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

#include "EDSDK.h"
#include "EDSDKErrors.h"
#include "EDSDKTypes.h"

#include <atlimage.h>
#include <array>
#include <atomic>

#include <fstream>

constexpr int pHeight = 640;
constexpr int pWidth = 960;
constexpr int pChannel = 3;
constexpr int pPitch = pChannel * pWidth;
constexpr int pBytes = pChannel * pHeight * pWidth;
//typedef std::array<unsigned char, pBytes> PArray;  // preview

BYTE parray[pBytes] = {};
// parray + 220 * 220 * 3 = (BYTE)255;

/****************************************************************/
/*  TEST													*/
/****************************************************************/
int IDL_STDCALL Hello(int argc, void* argv[])
{

	{
		MessageBox(NULL, "Hello", "Hello", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
		return 0;
	}

}

/****************************************************************/
/*  common non-camera functions
/****************************************************************/
void sleep_milisecond(int milisecond)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milisecond));
}

/****************************************************************/
/*  error / info functions
/****************************************************************/

namespace INFO
{
	inline void error(const char * text)
	{
		MessageBox(NULL, text, "Error", MB_OK | MB_ICONERROR | MB_TASKMODAL);
	}

	inline void info(const char * text)
	{
		MessageBox(NULL, text, "Info", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
	}
}

/****************************************************************/
/*  Canon for hida observation
/****************************************************************/

typedef struct _EVF_DATASET
{
	EdsStreamRef	stream; // JPEG stream.
	EdsUInt32		zoom;
	EdsRect			zoomRect;
	EdsPoint		imagePosition;
	EdsUInt32		histogram[256 * 4]; //(YRGB) YRGBYRGBYRGBYRGB....
	EdsSize			sizeJpegLarge;
}EVF_DATASET;

class CaptureHandler
{
private:
	std::atomic<bool> isReady;

public:
	
	CaptureHandler()
	{
		isReady.store(true);
	}

	~CaptureHandler() {}

	void setReady(bool val)
	{
		isReady.store(val);
	}

	bool getReady()
	{
		return isReady.load();
	}
};

EdsError downloadImage(EdsDirectoryItemRef);

class CameraEventListener
{
public:
	static EdsError EDSCALLBACK  handleObjectEvent(
		EdsUInt32			inEvent,
		EdsBaseRef			inRef,
		EdsVoid* inContext
	)
	{

		//std::shared_ptr<CaptureHandler>	ptrCapCtrl = *reinterpret_cast<std::shared_ptr<CaptureHandler>*>(inContext);
		CaptureHandler * ptr_chdl = (CaptureHandler *)inContext;

		//if (ptrCapCtrl->isReady.load())
		std::fstream file_out;
		file_out.open("D:\\test.txt", std::ios_base::app);
		file_out << "kEdsObjectEvent_DirItemRequestTransfer : " << kEdsObjectEvent_DirItemRequestTransfer << std::endl;
		file_out << "kEdsObjectEvent_DirItemRequestTransferDT : " << kEdsObjectEvent_DirItemRequestTransferDT << std::endl;
		file_out << "kEdsObjectEvent_DirItemCreated : " << kEdsObjectEvent_DirItemCreated << std::endl;
		file_out << "inEvent : " << inEvent << std::endl;
		file_out << "ptr_chdl : " << ptr_chdl << std::endl;
		file_out.close();

		switch (inEvent)
		{
		case kEdsObjectEvent_DirItemRequestTransfer:
			//fireEvent(controller, "download", inRef);

			file_out.open("D:\\test.txt", std::ios_base::app);
			file_out << "kEdsObjectEvent_DirItemRequestTransfer detected\n";
			file_out.close();

			//ptrCapCtrl->isReady.store(false);
			ptr_chdl->setReady(false);
			
			//CoInitializeEx(NULL, COINIT_MULTITHREADED);
			downloadImage(inRef);
			//CoUninitialize();
			break;

		case kEdsObjectEvent_DirItemRequestTransferDT:
			//fireEvent(controller, "download", inRef);

			file_out.open("D:\\test.txt", std::ios_base::app);
			file_out << "kEdsObjectEvent_DirItemRequestTransferDT detected\n";
			file_out.close();

			//ptrCapCtrl->isReady.store(false);
			ptr_chdl->setReady(false);

			//CoInitializeEx(NULL, COINIT_MULTITHREADED);
			downloadImage(inRef);
			//CoUninitialize();
			break;

		case  kEdsObjectEvent_DirItemCreated:

			file_out.open("D:\\test.txt", std::ios_base::app);
			file_out << "kEdsObjectEvent_DirItemCreated detected\n";
			file_out.close();
			
			ptr_chdl->setReady(false);
			downloadImage(inRef);
			break;

		default:
			//Object without the necessity is released
			if (inRef != NULL)
			{
				EdsRelease(inRef);
			}
			break;
		}

		if (inRef != NULL)
		{
			EdsRelease(inRef);
		}

		return EDS_ERR_OK;
	}

	static EdsError EDSCALLBACK  handlePropertyEvent(
		EdsUInt32			inEvent,
		EdsUInt32			inPropertyID,
		EdsUInt32			inParam,
		EdsVoid* inContext
	)
	{
		/*
		if (inPropertyID == kEdsPropID_Evf_OutputDevice) {
			std::cout << "Property Callback : " << inEvent << " " << inPropertyID << "\n";
		}
		*/


		return EDS_ERR_OK;
	}

	static EdsError EDSCALLBACK  handleStateEvent(
		EdsUInt32			inEvent,
		EdsUInt32			inParam,
		EdsVoid* inContext
	)
	{




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

		//if (err == EDS_ERR_OK) std::cout << "live View started\n";
	}
	// プロパティ設定が正常に行われた場合、カメラからプロパティ変更イベント通知が発行されます。
	// プロパティ変更通知が到着したら、LiveView 画像のダウンロードを開始します。
	return err;
}

EdsError downloadEvfData(EdsCameraRef camera, BYTE * prevarray)
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
		//std::cout << "zoom ratio =" << dataSet.zoom << "\n";
		// Get position of image data. (when enlarging)
		// Upper left coordinate using JPEG Large size as a reference.
		EdsGetPropertyData(evfImage, kEdsPropID_Evf_ImagePosition, 0, sizeof(dataSet.imagePosition), &dataSet.imagePosition);
		// Get histogram (RGBY).
		EdsGetPropertyData(evfImage, kEdsPropID_Evf_Histogram, 0, sizeof(dataSet.histogram), dataSet.histogram);
		// Get rectangle of the focus border.
		EdsGetPropertyData(evfImage, kEdsPropID_Evf_ZoomRect, 0, sizeof(dataSet.zoomRect), &dataSet.zoomRect);
		//std::cout << "zoomPoint = (" << dataSet.zoomRect.point.x << "," << dataSet.zoomRect.point.y << ")\n";
		//std::cout << "zoomRect : height = " << dataSet.zoomRect.size.height << ", width = " << dataSet.zoomRect.size.width << "\n";
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
			//std::cout << "pbyteImage has size = " << size << "\n";

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

				if (false) {
					int height = image.GetHeight(); // 640
					int width = image.GetWidth();   // 960
					int pitch = image.GetPitch();   // -2880
					char message[100];
					sprintf_s(message, "(H,W,P,S)=(%d,%d,%d,%llu)", height, width, pitch, size);
					INFO::info(message);
				}
				//std::cout << "image height = " << image.GetHeight() << ", width = " << image.GetWidth() << "\n";

				const BYTE * src = (BYTE*)image.GetBits();
				if (image.GetPitch() < 0) src -= pPitch * (pHeight-1);


				memcpy(prevarray, src, pBytes);
				
				//BYTE * pBitmapData = new BYTE[pBytes];
				//memcpy(pBitmapData, src, pBytes);
				//delete pBitmapData;

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
		//if (err == EDS_ERR_OK) std::cout << "live View ended\n";
	}
	return err;
}

EdsError takePicture(EdsCameraRef camera)
{
	EdsError err;
	err = EdsSendCommand(camera, kEdsCameraCommand_PressShutterButton
		, kEdsCameraCommand_ShutterButton_Completely);
	err = EdsSendCommand(camera, kEdsCameraCommand_PressShutterButton
		, kEdsCameraCommand_ShutterButton_OFF);
	return err;
}

EdsError downloadImage(EdsDirectoryItemRef directoryItem)
{
	EdsError err = EDS_ERR_OK;
	EdsStreamRef stream = NULL;
	// Get directory item information
	EdsDirectoryItemInfo dirItemInfo;
	err = EdsGetDirectoryItemInfo(directoryItem, &dirItemInfo);

	std::fstream file_out;
	file_out.open("D:\\test.txt", std::ios_base::app);
	file_out << "dirItemInfo.szFileName : " <<dirItemInfo.szFileName << std::endl;
	file_out.close();

	// Create file stream for transfer destination
	if (err == EDS_ERR_OK)
	{
		err = EdsCreateFileStream("D:\\IMG_0001.JPG",//dirItemInfo.szFileName,
			kEdsFileCreateDisposition_CreateAlways,
			kEdsAccess_ReadWrite, &stream);
	}
	// Download image
	if (err == EDS_ERR_OK)
	{
		err = EdsDownload(directoryItem, dirItemInfo.size, stream);
	}
	// Issue notification that download is complete
	if (err == EDS_ERR_OK)
	{
		err = EdsDownloadComplete(directoryItem);
	}
	// Release stream
	if (stream != NULL)
	{
		EdsRelease(stream);
		stream = NULL;
	}
	return err;
}

/*  my functions  */




EdsCameraRef camera = NULL;
bool isReady = false;
//CImage pimage;
const bool isDebug = true;

//std::shared_ptr<CaptureHandler> ptrCaptureHandler;

CaptureHandler chdl;

bool checkReady()
{
	if (!isReady) {
		INFO::error("camera or sdk not ready");
		return false;
	}

	if (camera == NULL) {
		INFO::error("camera is null");
		return false;
	}

	return true;
}

bool checkSuccess(EdsError err, const char * text)
{
	if (err != EDS_ERR_OK) {

		std::string fullText("Failed in : ");
		fullText += std::string(text);

		INFO::error(fullText.c_str());
		return false;
	}
	return true;
}

int init_camera()
{
	//EdsError err = EDS_ERR_OK;

	// init SDK
	if (!checkSuccess(EdsInitializeSDK(), "init SDK")) return 1;

	// init camera
	if (!checkSuccess(getFirstCamera(&camera), "init camera")) return 1;

	isReady = true;

	return 0;
}

int set_callback()
{
	if (!checkReady()) return 1;

	//ptrCaptureHandler = std::make_shared<CaptureHandler>();


	//Set Property Event Handler
	if (!checkSuccess(
		//EdsSetPropertyEventHandler(camera, kEdsPropertyEvent_All, CameraEventListener::handlePropertyEvent, (EdsVoid*) ptrCaptureHandler.get()),
		EdsSetPropertyEventHandler(camera, kEdsPropertyEvent_All, CameraEventListener::handlePropertyEvent, (EdsVoid*)NULL),
		"set property event callback")) return 1;

	//Set Object Event Handler
	if (!checkSuccess(
		EdsSetObjectEventHandler(camera, kEdsObjectEvent_All, CameraEventListener::handleObjectEvent, (EdsVoid*)(&chdl)),
		"set object event callback")) return 1;

	//Set State Event Handler
	if (!checkSuccess(
		EdsSetCameraStateEventHandler(camera, kEdsStateEvent_All, CameraEventListener::handleStateEvent, (EdsVoid*)NULL),
		"set state event callback")) return 1;

	return 0;
}

int connect_camera()
{
	if (!checkReady()) return 1;

	// connect to camera
	if (!checkSuccess(EdsOpenSession(camera), "connect camera")) return 1;

	//Acquisition of camera information

	EdsDeviceInfo deviceInfo;
	//std::cout << "camera connected." << "\n";

	if (!checkSuccess(EdsGetDeviceInfo(camera, &deviceInfo), "get camera info")) return 1;

	if (!checkReady()) return 1;


	char message[100];
	sprintf_s(message, "device description : %s", deviceInfo.szDeviceDescription);
	INFO::info(message);

	//Preservation ahead is set to PC
	EdsUInt32 saveTo = kEdsSaveTo_Camera;
	if (!checkSuccess(EdsSetPropertyData(camera, kEdsPropID_SaveTo, 0, sizeof(saveTo), &saveTo), "save to pc")) return 1;

	return 0;
}


int close_camera()
{
	if (!checkReady()) return 1;

	// disconnect to camera
	if (!checkSuccess(EdsCloseSession(camera), "connect camera")) return 1;
	INFO::info("camera disconnected");

	// release camera
	if (!checkSuccess(EdsRelease(camera), "release camera")) return 1;

	// end sdk
	if (!checkSuccess(EdsTerminateSDK(), "terminate sdk")) return 1;
	INFO::info("sdk terminated");

	isReady = false;
	return 0;
}

int start_preview()
{
	if (!checkReady()) return 1;
	
	if (!checkSuccess(startLiveview(camera), "start preview")) return 1;
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	if (isDebug) {
		char message[100];
		sprintf_s(message, "preview started");
		INFO::info(message);
	}

	return 0;
}

int end_preview()
{
	if (!checkReady()) return 1;

	if (!checkSuccess(endLiveview(camera), "start preview")) return 1;
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	if (isDebug) {
		char message[100];
		sprintf_s(message, "preview ended");
		INFO::info(message);
	}

	return 0;
}

int get_preview()
{
	if (!checkReady()) return 1;

	if (!checkSuccess(downloadEvfData(camera, parray), "get preview image")) return 1;


	return 0;
}

int take_picture()
{
	if (!checkReady()) return 1;

	if (!checkSuccess(takePicture(camera), "take picture")) return 1;

	sleep_milisecond(5000);
	char message[100];
	sprintf_s(message, "isReady = %s", (chdl.getReady()? "true": "false"));
	INFO::info(message);
	sprintf_s(message, "&chdl = %p", &chdl);
	INFO::info(message);
	return 0;
}

/****************************************************************/
/*  IDL related functions
/****************************************************************/

std::string getIDLString(const void* argv)
{
	IDL_STRING* s_ptr = (IDL_STRING *)argv;
	std::string str(s_ptr[0].s, s_ptr[0].s + s_ptr[0].slen);

	return std::move(str);
}

/****************************************************************/
/*  camera connection
/*  IDL>  x=call_external(dll,'myidl_initCamera')
/****************************************************************/

int IDL_STDCALL myidl_initCamera(int argc, void* argv[])
{
	if (init_camera()) return 1;
	if (set_callback()) return 1;
	if (connect_camera()) return 1;

	return 0;
}

/****************************************************************/
/*  camera disconnection
/*  IDL>  x=call_external(dll,'myidl_closeCamera')
/****************************************************************/

int IDL_STDCALL myidl_closeCamera(int argc, void* argv[])
{

	if (close_camera()) return 1;

	return 0;
}

/****************************************************************/
/*  start preview
/*  IDL>  x=call_external(dll,'myidl_startPreview')
/****************************************************************/

int IDL_STDCALL myidl_startPreview(int argc, void* argv[])
{

	if (start_preview()) return 1;


	return 0;
}

/****************************************************************/
/*  end preview
/*  IDL>  x=call_external(dll,'myidl_endPreview')
/****************************************************************/

int IDL_STDCALL myidl_endPreview(int argc, void* argv[])
{

	if (end_preview()) return 1;


	return 0;
}

/****************************************************************/
/*  aquire preview
/*  IDL>  x=call_external(dll,'myidl_getPreview', img)
/****************************************************************/

int IDL_STDCALL myidl_getPreview(int argc, void* argv[])
{

	if (get_preview()) return 1;
	BYTE* idlBuffer = (BYTE *)argv[0];
	memcpy(idlBuffer, parray, pBytes);

	return 0;
}


/****************************************************************/
/*  take picture
/*  IDL>  x=call_external(dll,'myidl_takePicture', img)
/****************************************************************/

int IDL_STDCALL myidl_takePicture(int argc, void* argv[])
{

	if (take_picture()) return 1;
	//BYTE* idlBuffer = (BYTE *)argv[0];
	//memcpy(idlBuffer, parray, pBytes);

	return 0;
}
