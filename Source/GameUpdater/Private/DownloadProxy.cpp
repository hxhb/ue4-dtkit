// Fill out your copyright notice in the Description page of Project Settings.


#include "DownloadProxy.h"
#include "GameUpdaterLog.h"

// engine header
#include "Misc/SecureHash.h"
#include "Templates/SharedPointer.h"
#include "Interfaces/IHttpRequest.h"
#include "Kismet/KismetStringLibrary.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/IPlatformFileModule.h"

#if HACK_HTTP_LOG_GETCONTENT_WARNING
static class FHackHttpResponse* HackCurlResponse(IHttpResponse* InCurlHttpResponse);
#endif
static TArray<uint8>& GetResponseContentData(FHttpResponsePtr InHttpResponse);


UDownloadProxy::UDownloadProxy()
	:Super(),Status(EDownloadStatus::NotStarted), DownloadFileTotalSize(0),TotalDownloadedByte(0),RecentlyPauseTimeDownloadByte(0),DownloadSpeed(0)
{
}

void UDownloadProxy::RequestDownload(const FString& InURL, const FString& InSavePath)
{
	if (!HttpRequest.IsValid() && Status == EDownloadStatus::NotStarted)
	{
		DownloadFileInfo.URL = InURL;
		{
			FString Path;
			FString Name;
			FString Extension;
			FPaths::Split(DownloadFileInfo.URL, Path, Name, Extension);
			DownloadFileInfo.Name = Name + TEXT(".") + Extension;
		}
		if (!InSavePath.IsEmpty())
		{
			DownloadFileInfo.SavePath = InSavePath;
		}
		else
		{
			UE_LOG(GameUpdaterLog,Warning,TEXT("RequestDownload: InSavePath is empty,default is FPaths::ProjectSavedDir()"))
			DownloadFileInfo.SavePath = FPaths::ProjectSavedDir();
		}
		PreRequestHeadInfo(DownloadFileInfo);
	}
}

void UDownloadProxy::Pause()
{
	if (HttpRequest.IsValid() && HttpRequest->GetStatus() == EHttpRequestStatus::Processing)
	{
		HttpRequest->CancelRequest();
		Status = EDownloadStatus::Pause;
		DownloadSpeed = 0;
		RecentlyPauseTimeDownloadByte = TotalDownloadedByte;
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Warning, TEXT("Download mission is paused,downloaded size is:%d."), TotalDownloadedByte);
#endif
		TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDownloadProxy::Tick));
		OnDownloadPausedDyMulitDlg.Broadcast(this);

	}
}

void UDownloadProxy::Resume()
{
	if (HttpRequest.IsValid() && Status == EDownloadStatus::Pause)
	{
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Log, TEXT("Download Resume is beginning"));
#endif
		HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->OnRequestProgress().BindUObject(this, &UDownloadProxy::OnDownloadProcess,EDownloadType::Resume);
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UDownloadProxy::OnDownloadComplete);
		HttpRequest->SetURL(DownloadFileInfo.URL);
		HttpRequest->SetVerb(TEXT("GET"));
		FString RangeArgs = TEXT("bytes=") + FString::FromInt(TotalDownloadedByte) + TEXT("-");
		HttpRequest->SetHeader(TEXT("Range"), RangeArgs);
		if (HttpRequest->ProcessRequest())
		{
			TArray<uint8>& ResponseDataArray = GetResponseContentData(HttpRequest->GetResponse());

#if WITH_LOG
			UE_LOG(GameUpdaterLog, Log, TEXT("ResponseData Array allocated memory is %d."), ResponseDataArray.GetAllocatedSize());
#endif
			ResponseDataArray.Reserve(DownloadFileTotalSize - TotalDownloadedByte);
#if WITH_LOG
			UE_LOG(GameUpdaterLog, Log, TEXT("Reserved ResponseData Array allocated memory is %d."), ResponseDataArray.GetAllocatedSize());
			UE_LOG(GameUpdaterLog, Log, TEXT("Resume ResponseData Array allocated memory is %d."), ResponseDataArray.GetAllocatedSize());
#endif
			Status = EDownloadStatus::Downloading;
#if WITH_LOG
			UE_LOG(GameUpdaterLog, Log, TEXT("Download Resume,Range Args:%s"),*RangeArgs);
			OnDownloadResumedDyMulitDlg.Broadcast(this);
#endif
		}
	}
	else
	{
		UE_LOG(GameUpdaterLog, Error, TEXT("Download Resume Faild."));
	}
}

void UDownloadProxy::Cancel()
{
	if (HttpRequest.IsValid())
	{
		HttpRequest->CancelRequest();
		FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
		Status = EDownloadStatus::Canceled;
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Warning, TEXT("Download Cancel"));
		OnDownloadCanceledDyMulitDlg.Broadcast(this);
#endif
	}
}

void UDownloadProxy::Reset()
{
	Cancel();
	HttpRequest = NULL;
	DownloadFileInfo = FDownloadFile();
	Status = EDownloadStatus::NotStarted;
	DownloadFileTotalSize = 0;
	TotalDownloadedByte = 0;
	RecentlyPauseTimeDownloadByte = 0;
	DownloadSpeed = 0;
	DeltaTime = 0.f;
}

bool UDownloadProxy::Tick(float delta)
{
	DeltaTime = delta;
	return true;
}

int32 UDownloadProxy::GetDownloadedSize() const
{
	return TotalDownloadedByte;
}

int32 UDownloadProxy::GetTotalSize() const
{
	return DownloadFileTotalSize;
}

float UDownloadProxy::GetDownloadProgress() const
{
	float result=0.f;
	if (Status == EDownloadStatus::Downloading)
	{
		result = (double)TotalDownloadedByte/(double)DownloadFileTotalSize;
	}
	
	return result;
}

int32 UDownloadProxy::GetDownloadSpeed()const
{
	return DownloadSpeed;
}

float UDownloadProxy::GetDownloadSpeedKbs() const
{
	double KbSpeed = (double)GetDownloadSpeed() / 1024.f;
	float fps = 1.0f / DeltaTime;
	return (float)(KbSpeed * fps);
}

bool UDownloadProxy::HashCheck(const FString& InMD5Hash, bool bInAsync)
{
	bool result = false;
	if (Status == EDownloadStatus::Succeeded)
	{
		FString SavedFilePath = FPaths::Combine(DownloadFileInfo.SavePath, DownloadFileInfo.Name);
		if (FPaths::FileExists(SavedFilePath))
		{
			FMD5Hash CalcHash = FMD5Hash::HashFile(*SavedFilePath);
			FString CalcHashStringValue = LexToString(CalcHash);
			result = InMD5Hash.Equals(CalcHashStringValue,ESearchCase::IgnoreCase);
#if WITH_LOG
			UE_LOG(GameUpdaterLog, Log, TEXT("InMD5Hash is %s,CalcedHash is %s,is equal %s"), *InMD5Hash, *CalcHashStringValue, result ? TEXT("true") : TEXT("false"));
#endif
			OnHashCheckedDyMulitDlg.Broadcast(this,result);
		}
	}
	else
	{
		UE_LOG(GameUpdaterLog, Warning, TEXT("CheckFileHash:The Download Mission is not successed."));
	}
	return result;
}

void UDownloadProxy::OnDownloadProcess(FHttpRequestPtr RequestPtr, int32 byteSent, int32 byteReceive, EDownloadType RequestType)
{
	if (EHttpRequestStatus::Processing != RequestPtr->GetStatus())
	{
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Log, TEXT("OnDownloadProcess:Request Status is %d,is not processing."), (int32)RequestPtr->GetStatus());
#endif
		return;
	}
	if (EDownloadStatus::Downloading != Status)
	{
		return;
	}

	uint32 PaddingLength = 0;
	unsigned char* PaddingData = NULL;

	TArray<uint8>& ResponseDataArray = GetResponseContentData(HttpRequest->GetResponse());

	switch (RequestType)
	{
		case EDownloadType::Start:
		{
			uint32 CurrentRequestTotalLength = ResponseDataArray.Num();
			PaddingLength = CurrentRequestTotalLength - TotalDownloadedByte;
			PaddingData = const_cast<unsigned char*>(ResponseDataArray.GetData() + TotalDownloadedByte);
			break;
		}
		case EDownloadType::Resume:
		{
			uint32 CurrentRequestTotalLength = ResponseDataArray.Num();
			PaddingLength = CurrentRequestTotalLength - (TotalDownloadedByte - RecentlyPauseTimeDownloadByte);
			PaddingData = const_cast<unsigned char*>(ResponseDataArray.GetData() + (TotalDownloadedByte - RecentlyPauseTimeDownloadByte));
			break;
		}
	}

	if (Status != EDownloadStatus::Pause && PaddingLength > 0)
	{
		UE_LOG(GameUpdaterLog, Log, TEXT("OnDownloadProcess:PaddingLength is %d,Toltal Downloaded Byte is %d,Recently is %d."), PaddingLength,TotalDownloadedByte,RecentlyPauseTimeDownloadByte);
		if (FFileHelper::SaveArrayToFile(TArrayView<const uint8>(PaddingData, PaddingLength),*FPaths::Combine(DownloadFileInfo.SavePath, DownloadFileInfo.Name), &IFileManager::Get(), EFileWrite::FILEWRITE_Append | EFileWrite::FILEWRITE_AllowRead | EFileWrite::FILEWRITE_EvenIfReadOnly))
		{
			DownloadSpeed = PaddingLength;
			TotalDownloadedByte += PaddingLength;
		}
	}
}

void UDownloadProxy::OnDownloadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully)
{
#if WITH_LOG
	UE_LOG(GameUpdaterLog, Warning, TEXT("OnDownloadComplete:Http Request is %s"), bConnectedSuccessfully ? TEXT("True") : TEXT("false"));
#endif

	if (TickDelegateHandle.IsValid())
	{
		FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	}
	if (Status == EDownloadStatus::Pause)
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
			UE_LOG(GameUpdaterLog, Warning, TEXT("OnDownloadComplete:Http Request Status is %d"), (int32)RequestPtr->GetStatus());
		if (RequestPtr.IsValid() && RequestPtr->GetResponse().IsValid())
			UE_LOG(GameUpdaterLog, Warning, TEXT("OnDownloadComplete:Request Response code is %d."), RequestPtr->GetResponse()->GetResponseCode());
#endif
	}
	
	Status = bDownloadSuccessd ? EDownloadStatus::Succeeded : EDownloadStatus::Failed;
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Warning, TEXT("DownloadProxy Mission %s."), bDownloadSuccessd ?TEXT("Successfuly"):TEXT("Faild"));
#endif
	DownloadSpeed = 0;
	OnDownloadCompleteDyMulitDlg.Broadcast(this, bDownloadSuccessd);
}


void UDownloadProxy::PreRequestHeadInfo(const FDownloadFile& InDownloadFile)
{
	TSharedRef<IHttpRequest> HttpHeadRequest = FHttpModule::Get().CreateRequest();
	HttpHeadRequest->OnHeaderReceived().BindUObject(this, &UDownloadProxy::OnRequestHeadHeaderReceived);
	HttpHeadRequest->OnProcessRequestComplete().BindUObject(this, &UDownloadProxy::OnRequestHeadComplete);
	HttpHeadRequest->SetURL(DownloadFileInfo.URL);
	HttpHeadRequest->SetVerb(TEXT("HEAD"));
	if (HttpHeadRequest->ProcessRequest())
	{
		UE_LOG(GameUpdaterLog, Log, TEXT("Request Head."));
	}
}

void UDownloadProxy::OnRequestHeadHeaderReceived(FHttpRequestPtr RequestPtr, const FString& InHeaderName, const FString& InNewHeaderValue)
{
#if WITH_LOG
	UE_LOG(GameUpdaterLog, Log, TEXT("OnRequestHeadHeaderReceived::Header Name:%s\tHeaderValue:%s"),*InHeaderName,*InNewHeaderValue);
#endif
	if (InHeaderName.Equals(TEXT("Content-Length")))
	{
		DownloadFileTotalSize = UKismetStringLibrary::Conv_StringToInt(InNewHeaderValue);
	}
}

void UDownloadProxy::OnRequestHeadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully)
{

	bool bHttpRequestSuccessed = RequestPtr.IsValid() && RequestPtr->GetStatus() == EHttpRequestStatus::Succeeded;
	bool bResponseSuccessd = RequestPtr.IsValid() && RequestPtr->GetResponse().IsValid() && (RequestPtr->GetResponse()->GetResponseCode() >= 200 && RequestPtr->GetResponse()->GetResponseCode() < 300);
	bool bDownloadSuccessd = bConnectedSuccessfully && bHttpRequestSuccessed && bResponseSuccessd;
	
	if (bDownloadSuccessd)
	{
		FString SaveFilePath = FPaths::Combine(DownloadFileInfo.SavePath, DownloadFileInfo.Name);
		if (FPaths::FileExists(SaveFilePath))
		{
			bool bDeleted = FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*SaveFilePath);

			UE_LOG(GameUpdaterLog, Warning, TEXT("OnRequestHeadComplete: Delete Exists File %s."),bDeleted?TEXT("Successfuly"):TEXT("Faild"));
			if (!bDeleted)
				return;
		}
		DoDownloadRequest(DownloadFileInfo, DownloadFileTotalSize);
		
	}
#if WITH_LOG
	UE_LOG(GameUpdaterLog, Log, TEXT("OnRequestHeadComplete: Request Head %s."),bDownloadSuccessd?TEXT("Successfuly"):TEXT("Faild.Please check URL or Network connection."));
#endif
}

void UDownloadProxy::DoDownloadRequest(const FDownloadFile& InDownloadFile, int32 InFileSize)
{
	HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->OnRequestProgress().BindUObject(this, &UDownloadProxy::OnDownloadProcess, EDownloadType::Start);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UDownloadProxy::OnDownloadComplete);
	HttpRequest->SetURL(DownloadFileInfo.URL);
	HttpRequest->SetVerb(TEXT("GET"));
	if (HttpRequest->ProcessRequest())
	{
		TArray<uint8>& ResponseDataArray = GetResponseContentData(HttpRequest->GetResponse());
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Log, TEXT("ResponseData Array allocated memory is %d."), ResponseDataArray.GetAllocatedSize());
#endif
		ResponseDataArray.Reserve(InFileSize + 100);
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Log, TEXT("Reserved ResponseData Array allocated memory is %d."), ResponseDataArray.GetAllocatedSize());
#endif
		Status = EDownloadStatus::Downloading;
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Warning, TEXT("Downloading"));
#endif
		TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDownloadProxy::Tick));
	}
}

#if HACK_HTTP_LOG_GETCONTENT_WARNING
/**
 * Hack Curl implementation of an HTTP response
 */
class FHackHttpResponse : public IHttpResponse
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


static FHackHttpResponse* HackCurlResponse(IHttpResponse* InCurlHttpResponse)
{
	union HackCurlResponseUnion {
		IHttpResponse* Origin;
		FHackHttpResponse* Target;
	};

	HackCurlResponseUnion Hack;
	Hack.Origin = InCurlHttpResponse;
	return Hack.Target;
}
#endif


static TArray<uint8>& GetResponseContentData(FHttpResponsePtr InHttpResponse)
{
#if HACK_HTTP_LOG_GETCONTENT_WARNING
	TArray<uint8>& ResponseDataArray = HackCurlResponse(InHttpResponse.Get())->Payload;
#else
	TArray<uint8>& ResponseDataArray = const_cast<TArray<uint8>&>(InHttpResponse->GetResponse()->GetContent());
#endif

	return ResponseDataArray;
}
