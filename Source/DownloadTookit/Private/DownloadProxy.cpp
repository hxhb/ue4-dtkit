// Fill out your copyright notice in the Description page of Project Settings.


#include "DownloadProxy.h"
#include "DownloadTookitLog.h"

// engine header
#include "Containers/Ticker.h"
#include "Containers/Queue.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "Misc/CString.h"
#include "Templates/SharedPointer.h"
#include "Interfaces/IHttpRequest.h"
#include "Kismet/KismetStringLibrary.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/IPlatformFileModule.h"

#if HACK_HTTP_LOG_GETCONTENT_WARNING
static class TArray<uint8>& HackCurlResponse(IHttpResponse* InCurlHttpResponse);
#endif
static TArray<uint8>& GetResponseContentData(FHttpResponsePtr InHttpResponse);
static FString GetFileNameByURL(const FString& InURL);

#define SLICE_SIZE 1024*1024*20 // 20MB

UDownloadProxy::UDownloadProxy()
	:Super()
{
	Reset();
}

void UDownloadProxy::RequestDownload(const FString& InURL, const FString& InSavePathOpt, bool bInSliceOpt, int32 InSliceByteSizeOpt, bool bInForceOpt)
{
#if WITH_LOG
	UE_LOG(DownloadTookitLog, Log, TEXT("RequestDownload::InURL:%s\nInSavePath:%s\nbSlice:%s\nInSliceByteSize:%d"), *InURL, *InSavePathOpt, bInSliceOpt ? TEXT("true") : TEXT("false"), InSliceByteSizeOpt);
#endif
	if (bInForceOpt || ((!HttpRequest.IsValid() || HttpRequest->GetStatus() != EHttpRequestStatus::Processing) && (Status != EDownloadStatus::Downloading)))
	{
		// Reset(); // reset all member data to default

		FDownloadFile MakeDownloadFileInfo;
		if (bInSliceOpt)
		{
			bUseSlice = bInSliceOpt;
			SliceByteSize = InSliceByteSizeOpt > 0 ? InSliceByteSizeOpt : SLICE_SIZE;  // range:0-999 is first 1000 byte.
			UE_LOG(DownloadTookitLog, Log, TEXT("RequestDownload:SliceByteSize is %d."),SliceByteSize);
		}
		
		MakeDownloadFileInfo.URL = InURL;
		MakeDownloadFileInfo.Name = FGenericPlatformHttp::UrlDecode(GetFileNameByURL(MakeDownloadFileInfo.URL));
		if (!InSavePathOpt.IsEmpty())
		{
			MakeDownloadFileInfo.SavePath = InSavePathOpt;
		}
		else
		{
			MakeDownloadFileInfo.SavePath = FPaths::Combine(FPaths::ProjectSavedDir(),MakeDownloadFileInfo.Name);
			UE_LOG(DownloadTookitLog, Warning, TEXT("RequestDownload: InSavePath is empty,default is %s."), *MakeDownloadFileInfo.SavePath);
		}
		
		PreRequestHeadInfo(MakeDownloadFileInfo);
	}
	else
	{
		UE_LOG(DownloadTookitLog, Log, TEXT("RequestDownload::The Download mision is active,please cancel it and try again."));
	}
}

void UDownloadProxy::Pause()
{
	if (HttpRequest.IsValid() && HttpRequest->GetStatus() == EHttpRequestStatus::Processing)
	{
		HttpRequest->CancelRequest();
		Status = EDownloadStatus::Paused;
		DownloadSpeed = 0;
		LastRequestedTotalByte = TotalDownloadedByte;
#if WITH_LOG
		UE_LOG(DownloadTookitLog, Warning, TEXT("Download mission is paused,downloaded size is:%d."), TotalDownloadedByte);
#endif
		TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDownloadProxy::Tick));
		OnDownloadPausedDyMultiDlg.Broadcast(this);

	}
}

bool UDownloadProxy::Resume()
{
	bool bResumeStatus = false;
	if (HttpRequest.IsValid() && Status == EDownloadStatus::Paused)
	{
		FDownloadRange ResumeRange;
		ResumeRange.BeginPosition = TotalDownloadedByte;
		if (bUseSlice)
			ResumeRange.EndPosition = (TotalDownloadedByte + SliceByteSize) < InternalDownloadFileInfo.Size ? (TotalDownloadedByte + SliceByteSize - 1) : (InternalDownloadFileInfo.Size - 1);
		else
			ResumeRange.EndPosition = InternalDownloadFileInfo.Size - 1;

		if (DoDownloadRequest(InternalDownloadFileInfo, ResumeRange))
		{
			bResumeStatus = true;
			OnDownloadResumedDyMultiDlg.Broadcast(this);
		}
	}
	else
	{
		UE_LOG(DownloadTookitLog, Error, TEXT("Download Resume Faild, becouse the download mission is not paused."));
	}
	return bResumeStatus;
}

bool UDownloadProxy::ReDownload()
{
	bool bStatus = false;
	if (GetDownloadStatus() == EDownloadStatus::Downloading)
	{
		PreRequestHeadInfo(PassInDownloadFileInfo);
		bStatus = true;
	}

	UE_LOG(DownloadTookitLog, Warning, TEXT("ReDwonload is %s."), bStatus ? TEXT("accept"):TEXT("not accept,because the downloadproxy is downloading"));
	return bStatus;
}

void UDownloadProxy::Cancel()
{
	if (HttpRequest.IsValid())
	{
		HttpRequest->CancelRequest();
		HttpRequest->OnHeaderReceived().Unbind();
		HttpRequest->OnRequestProgress().Unbind();
		HttpRequest->OnProcessRequestComplete().Unbind();

		FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
		Status = EDownloadStatus::Canceled;
#if WITH_LOG
		UE_LOG(DownloadTookitLog, Warning, TEXT("Download Cancel"));
#endif
		OnDownloadCanceledDyMultiDlg.Broadcast(this);
		Reset();
	}
}

void UDownloadProxy::Reset()
{
	if(Status != EDownloadStatus::Canceled)
		Cancel();
	HttpRequest = NULL;
	InternalDownloadFileInfo = FDownloadFile();
	PassInDownloadFileInfo = FDownloadFile();
	Status = EDownloadStatus::NotStarted;
	TotalDownloadedByte = 0;
	LastRequestedTotalByte = 0;
	DownloadSpeed = 0;
	DeltaTime = 0.f;
	Md5Proxy.Reset();
	bUseSlice = false;
	SliceCount = 0;
	SliceByteSize = 0;

	// clear all delegete
	OnDownloadCompleteDyMultiDlg.Clear();
	OnDownloadCanceledDyMultiDlg.Clear();
	OnDownloadResumedDyMultiDlg.Clear();
	OnDownloadPausedDyMultiDlg.Clear();
}

bool UDownloadProxy::Tick(float delta)
{
	DeltaTime = delta;
	return true;
}

FDownloadFile UDownloadProxy::GetDownloadedFileInfo() const
{
	if (Status == EDownloadStatus::Succeeded)
	{
		return InternalDownloadFileInfo;
	}
	else
	{
		UE_LOG(DownloadTookitLog, Warning, TEXT("The Download Mission is not successed."));
		return PassInDownloadFileInfo;
	}
}

EDownloadStatus UDownloadProxy::GetDownloadStatus() const
{
	return Status;
}

int32 UDownloadProxy::GetDownloadedSize() const
{
	return TotalDownloadedByte;
}

int32 UDownloadProxy::GetTotalSize() const
{
	return InternalDownloadFileInfo.Size;
}

float UDownloadProxy::GetDownloadProgress() const
{
	float result=0.f;
	if (Status == EDownloadStatus::Downloading || Status == EDownloadStatus::Paused)
	{
		result = (double)TotalDownloadedByte/(double)InternalDownloadFileInfo.Size;
	}
	
	return result;
}

int32 UDownloadProxy::GetDownloadSpeed()const
{
	return DownloadSpeed;
}

float UDownloadProxy::GetDownloadSpeedKbs() const
{
	float result = 0.f;
	if (GetDownloadStatus() == EDownloadStatus::Downloading)
	{
		double KbSpeed = (double)GetDownloadSpeed() / 1024.f;
		float fps = 1.0f / DeltaTime;
		result = (float)(KbSpeed * fps);
	}
	return result;
}

bool UDownloadProxy::HashCheck(const FString& InMD5Hash)const
{
	bool result = false;
	if (Status == EDownloadStatus::Succeeded)
	{
		FString SavedFilePath = FPaths::Combine(InternalDownloadFileInfo.SavePath, InternalDownloadFileInfo.Name);
		if (FPaths::FileExists(SavedFilePath))
		{
			result = InMD5Hash.Equals(InternalDownloadFileInfo.HASH,ESearchCase::IgnoreCase);
#if WITH_LOG
			UE_LOG(DownloadTookitLog, Log, TEXT("InMD5Hash is %s,CalcedHash is %s,is equal %s"), *InMD5Hash, *InternalDownloadFileInfo.HASH, result ? TEXT("true") : TEXT("false"));
#endif
		}
	}
	else
	{
		UE_LOG(DownloadTookitLog, Warning, TEXT("CheckFileHash:The Download Mission is not successed."));
	}
	return result;
}

void UDownloadProxy::OnDownloadProcess(FHttpRequestPtr RequestPtr, int32 byteSent, int32 byteReceive)
{
	if (EHttpRequestStatus::Processing != RequestPtr->GetStatus())
	{
#if WITH_LOG
		UE_LOG(DownloadTookitLog, Log, TEXT("OnDownloadProcess:Request Status is %d,is not processing."), (int32)RequestPtr->GetStatus());
#endif
		return;
	}
	if (EDownloadStatus::Downloading != Status)
	{
		int32 ReceiveLength = RequestPtr->GetResponse()->GetContentLength();
		if (InternalDownloadFileInfo.Size == ReceiveLength || // request full file
			// request slice
			(bUseSlice && (SliceByteSize == ReceiveLength || ReceiveLength == (InternalDownloadFileInfo.Size - (SliceByteSize*SliceCount)))) ||
			// resume request
			(ReceiveLength == (InternalDownloadFileInfo.Size - TotalDownloadedByte))
			)
		{
			Status = EDownloadStatus::Downloading;
		}
		else
		{
			return;
		}
	}


	TArray<uint8>& ResponseDataArray = GetResponseContentData(HttpRequest->GetResponse());
	uint32 CurrentRequestTotalLength = ResponseDataArray.Num();

	uint32 PaddingLength = PaddingLength = CurrentRequestTotalLength - (TotalDownloadedByte - LastRequestedTotalByte);
	unsigned char* PaddingData = const_cast<unsigned char*>(ResponseDataArray.GetData() + (TotalDownloadedByte - LastRequestedTotalByte));

//#if WITH_LOG
//	FString Log = FString::Printf(TEXT("\n-------------\nTotalDownloadByte:%d\nLastRequestedTotalByte:%d\nCurrentRequestTotalLength:%d\nPaddingData:%x\nPaddingLength:%d\n------------\n"), TotalDownloadedByte, LastRequestedTotalByte, CurrentRequestTotalLength,PaddingData, PaddingLength);
//	UE_LOG(DownloadTookitLog, Log, TEXT("%s"), *Log);
//#endif

	if (Status != EDownloadStatus::Paused && PaddingLength > 0)
	{
		if (FFileHelper::SaveArrayToFile(TArrayView<const uint8>(PaddingData, PaddingLength),*InternalDownloadFileInfo.SavePath, &IFileManager::Get(), EFileWrite::FILEWRITE_Append | EFileWrite::FILEWRITE_AllowRead | EFileWrite::FILEWRITE_EvenIfReadOnly))
		{
			Md5Proxy.Update(PaddingData, PaddingLength);
			DownloadSpeed = PaddingLength;
			TotalDownloadedByte += PaddingLength;
		}
#if WITH_LOG
		UE_LOG(DownloadTookitLog, Log, TEXT("OnDownloadProcess:PaddingLength is %d,Toltal Downloaded Byte is %d,Last Recently is %dbyte."), PaddingLength, TotalDownloadedByte, LastRequestedTotalByte);
#endif
	}
}

//void UDownloadProxy::OnDownloadHeaderReceived(FHttpRequestPtr RequestPtr, const FString& InHeaderName, const FString& InNewHeaderValue)
//{
//	if (InHeaderName.Equals(TEXT("Content-Length")))
//	{
//		int32 ReceiveLength = UKismetStringLibrary::Conv_StringToInt(InNewHeaderValue);
//		if (InternalDownloadFileInfo.Size == ReceiveLength || // request full file
//			// request slice
//			(bUseSlice && (SliceByteSize == ReceiveLength || ReceiveLength == (InternalDownloadFileInfo.Size - (SliceByteSize*SliceCount)))) ||
//			// resume request
//			(ReceiveLength == (InternalDownloadFileInfo.Size-TotalDownloadedByte))
//			)
//		{
//			Status = EDownloadStatus::Downloading;
//		}
////		else
////		{
////#if WITH_LOG
////			UE_LOG(DownloadTookitLog, Warning, TEXT("OnDownloadHeaderReceived:HEAD and GET Request have different Content-Length(HEAD:%d,GET:%d)"),InternalDownloadFileInfo.Size, ReceiveLength);
////			UE_LOG(DownloadTookitLog, Warning, TEXT("OnDownloadHeaderReceived:HEAD and GET Request have different Content-Length(Slice:%d,GET:%d)"), SliceByteSize, ReceiveLength);
////			UE_LOG(DownloadTookitLog, Warning, TEXT("OnDownloadHeaderReceived:HEAD and GET Request have different Content-Length(SliceTotal:%d,GET:%d)"), (SliceByteSize*SliceCount), ReceiveLength);
////			UE_LOG(DownloadTookitLog, Warning, TEXT("OnDownloadHeaderReceived:HEAD and GET Request have different Content-Length(Total-downloaded:%d,GET:%d)"), (InternalDownloadFileInfo.Size - TotalDownloadedByte), ReceiveLength);
////#endif
////		}
//	}
//}

void UDownloadProxy::OnDownloadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully)
{
#if WITH_LOG
	UE_LOG(DownloadTookitLog, Warning, TEXT("OnDownloadComplete:Http Request is %s"), bConnectedSuccessfully ? TEXT("True") : TEXT("false"));
#endif
	
	if (Status != EDownloadStatus::Downloading)
	{
		UE_LOG(DownloadTookitLog, Warning, TEXT("OnDownloadComplete:Current status is not downloading"));
		return;
	}
	if (TickDelegateHandle.IsValid())
	{
		FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	}
	if (Status == EDownloadStatus::Paused)
	{
		return;
	}
	bool bDownloadSuccessd = false;
	if (bConnectedSuccessfully)
	{
		bool bHttpRequestSuccessed = RequestPtr.IsValid() && RequestPtr->GetStatus() == EHttpRequestStatus::Succeeded;
		bool bResponseSuccessd = RequestPtr.IsValid() && RequestPtr->GetResponse().IsValid() && (RequestPtr->GetResponse()->GetResponseCode() >= 200 && RequestPtr->GetResponse()->GetResponseCode() < 300);
		bDownloadSuccessd = bConnectedSuccessfully && bHttpRequestSuccessed && bResponseSuccessd;

#if WITH_LOG
		if (RequestPtr.IsValid())
			UE_LOG(DownloadTookitLog, Warning, TEXT("OnDownloadComplete:Http Request Status is %d"), (int32)RequestPtr->GetStatus());
		if (RequestPtr.IsValid() && RequestPtr->GetResponse().IsValid())
			UE_LOG(DownloadTookitLog, Warning, TEXT("OnDownloadComplete:Request Response code is %d."), RequestPtr->GetResponse()->GetResponseCode());
#endif
	}
	UE_LOG(DownloadTookitLog, Log, TEXT("OnDownloadComplete:TotalDownloadedByte is %d,FileTotalSize is %d"), TotalDownloadedByte,InternalDownloadFileInfo.Size);
	if (bDownloadSuccessd && bUseSlice && TotalDownloadedByte < InternalDownloadFileInfo.Size)
	{
		++SliceCount;
		FDownloadRange SliceRange;
		SliceRange.BeginPosition = TotalDownloadedByte;
		SliceRange.EndPosition = (TotalDownloadedByte + SliceByteSize) < InternalDownloadFileInfo.Size ? (TotalDownloadedByte + SliceByteSize -1) : (InternalDownloadFileInfo.Size - 1);
		bool bRequestSuccess = DoDownloadRequest(InternalDownloadFileInfo, SliceRange);
		UE_LOG(DownloadTookitLog, Log, TEXT("OnDownloadComplete:Request Next Slice Content %s,count is %d."),bRequestSuccess?TEXT("Success"):TEXT("Faild"),SliceCount);

		return;
	}
	
	Status = bDownloadSuccessd ? EDownloadStatus::Succeeded : EDownloadStatus::Failed;
#if WITH_LOG
		UE_LOG(DownloadTookitLog, Warning, TEXT("OnDownloadComplete:Download Mission %s."), bDownloadSuccessd ? TEXT("Successfuly"):TEXT("Faild"));
#endif
	if (bDownloadSuccessd)
	{
		InternalDownloadFileInfo.HASH = ANSI_TO_TCHAR(Md5Proxy.Final());
		UE_LOG(DownloadTookitLog, Warning, TEXT("OnDownloadComplete:Hash calc result is %s"), *InternalDownloadFileInfo.HASH);
	}
	DownloadSpeed = 0;
	OnDownloadCompleteDyMultiDlg.Broadcast(this, bDownloadSuccessd);
}


void UDownloadProxy::PreRequestHeadInfo(const FDownloadFile& InDownloadFile,bool bAutoDownload)
{
	PassInDownloadFileInfo = InDownloadFile;
	if (PassInDownloadFileInfo.SavePath.IsEmpty())
	{
		PassInDownloadFileInfo.SavePath = FPaths::Combine(FPaths::ProjectSavedDir(),GetFileNameByURL(InDownloadFile.URL));
	}
	InternalDownloadFileInfo = PassInDownloadFileInfo;

	TSharedRef<IHttpRequest> HttpHeadRequest = FHttpModule::Get().CreateRequest();
	HttpHeadRequest->OnHeaderReceived().BindUObject(this, &UDownloadProxy::OnRequestHeadHeaderReceived);
	HttpHeadRequest->OnProcessRequestComplete().BindUObject(this, &UDownloadProxy::OnRequestHeadComplete,bAutoDownload);
	HttpHeadRequest->SetURL(InternalDownloadFileInfo.URL);
	HttpHeadRequest->SetVerb(TEXT("HEAD"));
	if (HttpHeadRequest->ProcessRequest())
	{
		UE_LOG(DownloadTookitLog, Log, TEXT("Request Head."));
	}
}

void UDownloadProxy::OnRequestHeadHeaderReceived(FHttpRequestPtr RequestPtr, const FString& InHeaderName, const FString& InNewHeaderValue)
{
#if WITH_LOG
	UE_LOG(DownloadTookitLog, Log, TEXT("OnRequestHeadHeaderReceived::Header Name:%s\tHeaderValue:%s"),*InHeaderName,*InNewHeaderValue);
#endif
	if (InHeaderName.Equals(TEXT("Content-Length")))
	{
		InternalDownloadFileInfo.Size = UKismetStringLibrary::Conv_StringToInt(InNewHeaderValue);
	}
}

void UDownloadProxy::OnRequestHeadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully, bool bAutoDownload)
{
	UE_LOG(DownloadTookitLog, Log, TEXT("OnRequestHeadComplete"));
	bool bHttpRequestSuccessed = RequestPtr.IsValid() && RequestPtr->GetStatus() == EHttpRequestStatus::Succeeded;
	bool bResponseSuccessd = RequestPtr.IsValid() && RequestPtr->GetResponse().IsValid() && (RequestPtr->GetResponse()->GetResponseCode() >= 200 && RequestPtr->GetResponse()->GetResponseCode() < 300);
	bool bDownloadSuccessd = bConnectedSuccessfully && bHttpRequestSuccessed && bResponseSuccessd;
	
	if (bDownloadSuccessd)
	{
		if (bAutoDownload)
		{
			// FString SaveFilePath = FPaths::Combine(InternalDownloadFileInfo.SavePath, InternalDownloadFileInfo.Name);
			if (FPaths::FileExists(InternalDownloadFileInfo.SavePath))
			{
				bool bDeleted = FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*InternalDownloadFileInfo.SavePath);

				UE_LOG(DownloadTookitLog, Warning, TEXT("OnRequestHeadComplete: Delete Exists File %s."), bDeleted ? TEXT("Successfuly") : TEXT("Faild"));
				if (!bDeleted)
					return;
			}
			PreDownloadRequest();

			FDownloadRange TheRange;
			// Range:0-FILE_SIZE-1 is request full file
			// Range:0-SLICE_SIZE is request part of file(SLICE_SIZE+1 byte)
			TheRange.BeginPosition = TotalDownloadedByte;
			if (bUseSlice)
			{
				TheRange.EndPosition = (TotalDownloadedByte + SliceByteSize) < InternalDownloadFileInfo.Size ? (TotalDownloadedByte + SliceByteSize -1) : (InternalDownloadFileInfo.Size - 1);
			}
			else
			{
				TheRange.EndPosition = InternalDownloadFileInfo.Size - 1;
			}

			DoDownloadRequest(InternalDownloadFileInfo, TheRange);
		}
	}
	else
	{
		OnDownloadCompleteDyMultiDlg.Broadcast(this, false);
	}

#if WITH_LOG
	UE_LOG(DownloadTookitLog, Log, TEXT("OnRequestHeadComplete: Request Head %s."),bDownloadSuccessd?TEXT("Successfuly"):TEXT("Faild.Please check URL or Network connection."));
#endif
}

void UDownloadProxy::PreDownloadRequest()
{
	Md5Proxy.Reset();
}

bool UDownloadProxy::DoDownloadRequest(const FDownloadFile& InDownloadFile, const FDownloadRange& InRange)
{	
	bool bDoStatus = false;
	if (InRange.EndPosition <= InRange.BeginPosition)
	{
		UE_LOG(DownloadTookitLog, Error, TEXT("DoDownloadRequest:Range EndPosition(%d) less or equal BeginPosition(%d)"),InRange.EndPosition,InRange.BeginPosition);
		return false;
	}
	LastRequestedTotalByte = TotalDownloadedByte;
	HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->OnRequestProgress().BindUObject(this, &UDownloadProxy::OnDownloadProcess);
	// HttpRequest->OnHeaderReceived().BindUObject(this, &UDownloadProxy::OnDownloadHeaderReceived);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UDownloadProxy::OnDownloadComplete);
	HttpRequest->SetURL(InternalDownloadFileInfo.URL);
	HttpRequest->SetVerb(TEXT("GET"));

	FString RangeArgs = TEXT("bytes=") + FString::FromInt(InRange.BeginPosition) + TEXT("-") + FString::FromInt(InRange.EndPosition);
	UE_LOG(DownloadTookitLog, Log, TEXT("DoDownloadRequest:RangeArgs is %s"), *RangeArgs);

	HttpRequest->SetHeader(TEXT("Range"), RangeArgs);
	if (HttpRequest->ProcessRequest())
	{
		TArray<uint8>& ResponseDataArray = GetResponseContentData(HttpRequest->GetResponse());

		uint32 ReserveSize = (InRange.EndPosition-InRange.BeginPosition) + 100;
		ResponseDataArray.Reserve(ReserveSize);

#if WITH_LOG
		UE_LOG(DownloadTookitLog, Warning, TEXT("Downloading"));
#endif
		TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDownloadProxy::Tick));
		bDoStatus = true;
	}

	return bDoStatus;
}

#if HACK_HTTP_LOG_GETCONTENT_WARNING 
	#if !PLATFORM_APPLE
		/**
		 * Hack Curl implementation of an HTTP response
		 */
		class FHackCurlHttpResponse : public IHttpResponse
		{
		public:
			/** Request that owns this response */
			void* Request;

			/** BYTE array to fill in as the response is read via didReceiveData */
			TArray<uint8> Payload;
			/** Caches how many bytes of the response we've read so far */
			FThreadSafeCounter TotalBytesRead;
			/** Cached key/value header pairs. Parsed once request completes. Only accessible on the game thread. */
			TMap<FString, FString> Headers;
			/** Newly received headers we need to inform listeners about */
			TQueue<TPair<FString, FString>> NewlyReceivedHeaders;
			/** Cached code from completed response */
			int32 HttpCode;
			/** Cached content length from completed response */
			int32 ContentLength;
			/** True when the response has finished async processing */
			int32 volatile bIsReady;
			/** True if the response was successfully received/processed */
			int32 volatile bSucceeded;
		};

	#else
		#if WITH_SSL
		#include "Ssl.h"
		#endif
		/**
		 * Apple Response Wrapper which will be used for it's delegates to receive responses.
		 */
		@interface FHttpResponseAppleWrapper : NSObject
		{
			/** Holds the payload as we receive it. */
			TArray<uint8> Payload;
		}
		/** A handle for the response */
		@property(retain) NSHTTPURLResponse* Response;
		/** Flag whether the response is ready */
		@property BOOL bIsReady;
		/** When the response is complete, indicates whether the response was received without error. */
		@property BOOL bHadError;
		/** When the response is complete, indicates whether the response failed with an error specific to connecting to the host. */
		@property BOOL bIsHostConnectionFailure;
		/** The total number of bytes written out during the request/response */
		@property int32 BytesWritten;

		/** Delegate called when we send data. See Apple docs for when/how this should be used. */
		-(void)connection:(NSURLConnection *)connection didSendBodyData : (NSInteger)bytesWritten totalBytesWritten : (NSInteger)totalBytesWritten totalBytesExpectedToWrite : (NSInteger)totalBytesExpectedToWrite;
		/** Delegate called with we receive a response. See Apple docs for when/how this should be used. */
		-(void)connection:(NSURLConnection *)connection didReceiveResponse : (NSURLResponse *)response;
		/** Delegate called with we receive data. See Apple docs for when/how this should be used. */
		-(void)connection:(NSURLConnection *)connection didReceiveData : (NSData *)data;
		/** Delegate called with we complete with an error. See Apple docs for when/how this should be used. */
		-(void)connection:(NSURLConnection *)connection didFailWithError : (NSError *)error;
		/** Delegate called with we complete successfully. See Apple docs for when/how this should be used. */
		-(void)connectionDidFinishLoading:(NSURLConnection *)connection;

		#if WITH_SSL
		/** Delegate called when the connection is about to validate an auth challenge. We only care about server trust. See Apple docs for when/how this should be used. */
		- (void)connection:(NSURLConnection *)connection willSendRequestForAuthenticationChallenge : (NSURLAuthenticationChallenge *)challenge;
		#endif

		- (TArray<uint8>&)getPayload;
		-(int32)getBytesWritten;
		@end
		/**
		 * Apple implementation of an Http response
		 */
		class FHackAppleHttpResponse : public IHttpResponse
		{
		public:
			// This is the NSHTTPURLResponse, all our functionality will deal with.
			FHttpResponseAppleWrapper* ResponseWrapper;

			/** Request that owns this response */
			void* Request;

			/** BYTE array to fill in as the response is read via didReceiveData */
			mutable TArray<uint8> Payload;
		};
	#endif

	static TArray<uint8>& HackHttpResponsePayload(IHttpResponse* InCurlHttpResponse)
	{
		union HackHttpResponseUnion {
			IHttpResponse* Origin;
#if PLATFORM_APPLE
			FHackAppleHttpResponse* Target;
#else
			FHackCurlHttpResponse* Target;
#endif
		};

		HackHttpResponseUnion Hack;
		Hack.Origin = InCurlHttpResponse;
#if PLATFORM_APPLE
		FHttpResponseAppleWrapper* ResponseWrapper = Hack.Target->ResponseWrapper;
		TArray<uint8>& Payload = [ResponseWrapper getPayload];
#else
		TArray<uint8>& Payload = Hack.Target->Payload;
#endif
		return Payload;
	}
#endif


static TArray<uint8>& GetResponseContentData(FHttpResponsePtr InHttpResponse)
{
#if HACK_HTTP_LOG_GETCONTENT_WARNING
	TArray<uint8>& ResponseDataArray = HackHttpResponsePayload(InHttpResponse.Get());
#else
	TArray<uint8>& ResponseDataArray = const_cast<TArray<uint8>&>(InHttpResponse->GetResponse()->GetContent());
#endif

	return ResponseDataArray;
}


static FString GetFileNameByURL(const FString& InURL)
{
	if (InURL.IsEmpty())
		return TEXT("");
	FString Path;
	FString Name;
	FString Extension;
	FPaths::Split(InURL, Path, Name, Extension);
	return Name + (Extension.IsEmpty() ? TEXT("") : (FString(TEXT(".")) + Extension));
}