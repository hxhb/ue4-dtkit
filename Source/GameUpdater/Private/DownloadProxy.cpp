// Fill out your copyright notice in the Description page of Project Settings.


#include "DownloadProxy.h"
#include "GameUpdaterLog.h"

// engine header
#include "Templates/SharedPointer.h"
#include "Interfaces/IHttpRequest.h"


#if HACK_HTTP_LOG_GETCONTENT_WARNING
static class FHackHttpResponse* HackCurlResponse(IHttpResponse* InCurlHttpResponse);
#endif
static TArray<uint8>& GetResponseContentData(FHttpResponsePtr InHttpResponse);


UDownloadProxy::UDownloadProxy()
	:Super(),Status(EDownloadStatus::NotStarted),TotalDownloadedByte(0),RecentlyPauseTimeDownloadByte(0),DownloadSpeed(0)
{

}

void UDownloadProxy::RequestDownload(const FDownloadFile& InDownloadFile)
{
	if (!HttpRequest.IsValid())
	{
		DownloadFileInfo = InDownloadFile;
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
			ResponseDataArray.Reserve(DownloadFileInfo.Size + 100);
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
}

void UDownloadProxy::Pause()
{
	if (HttpRequest.IsValid() && HttpRequest->GetStatus() == EHttpRequestStatus::Processing)
	{
		HttpRequest->CancelRequest();
		Status = EDownloadStatus::Pause;
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Warning, TEXT("Download Pause"));
#endif
		TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDownloadProxy::Tick));
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
			ResponseDataArray.Reserve(DownloadFileInfo.Size + 100 - TotalDownloadedByte);
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
		Status = EDownloadStatus::Canceled;
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Warning, TEXT("Download Cancel"));
		OnDownloadCanceledDyMulitDlg.Broadcast(this);
#endif
	}
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
	return DownloadFileInfo.Size;
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

void UDownloadProxy::OnDownloadProcess(FHttpRequestPtr RequestPtr, int32 byteSent, int32 byteReceive, EDownloadType RequestType)
{
	if (EHttpRequestStatus::Processing != RequestPtr->GetStatus())
	{
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Log, TEXT("OnDownloadProcess:Request Status is %d,is not processing."), (int32)RequestPtr->GetStatus());
#endif
		return;
	}
#if WITH_LOG
	UE_LOG(GameUpdaterLog, Log, TEXT("OnDownloadProcess:Request Response code is %d."), RequestPtr->GetResponse()->GetResponseCode());
	
#endif
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
		if (FFileHelper::SaveArrayToFile(TArrayView<const uint8>(PaddingData, PaddingLength), *FPaths::Combine(FPaths::ProjectDir(), DownloadFileInfo.Name), &IFileManager::Get(), EFileWrite::FILEWRITE_Append | EFileWrite::FILEWRITE_AllowRead | EFileWrite::FILEWRITE_EvenIfReadOnly))
		{
			DownloadSpeed = PaddingLength;
			TotalDownloadedByte += PaddingLength;
#if WITH_LOG
			UE_LOG(GameUpdaterLog, Warning, TEXT("%s Downloading size:%d."), EDownloadType::Start == RequestType ? TEXT("START") : TEXT("RESUME"), TotalDownloadedByte);
#endif
		}
	}

	if (EDownloadStatus::Pause == Status)
	{
		RecentlyPauseTimeDownloadByte = TotalDownloadedByte;
		DownloadSpeed = 0;
#if WITH_LOG
		UE_LOG(GameUpdaterLog, Warning, TEXT("Download mission is paused,downloaded size is:%d."), TotalDownloadedByte);
#endif		
		OnDownloadPausedDyMulitDlg.Broadcast(this);
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
	HttpHeadRequest->OnProcessRequestComplete().BindUObject(this, &UDownloadProxy::OnRequestHeadComplete);
	HttpHeadRequest->SetURL(DownloadFileInfo.URL);
	HttpHeadRequest->SetVerb(TEXT("HEAD"));
	if (HttpHeadRequest->ProcessRequest())
	{
		UE_LOG(GameUpdaterLog, Log, TEXT("Request Download file size."));
	}
}

void UDownloadProxy::OnRequestHeadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully)
{

	bool bHttpRequestSuccessed = RequestPtr.IsValid() && RequestPtr->GetStatus() == EHttpRequestStatus::Succeeded;
	bool bResponseSuccessd = RequestPtr.IsValid() && RequestPtr->GetResponse().IsValid() && (RequestPtr->GetResponse()->GetResponseCode() >= 200 && RequestPtr->GetResponse()->GetResponseCode() < 300);
	bool bDownloadSuccessd = bConnectedSuccessfully && bHttpRequestSuccessed && bResponseSuccessd;
	
	if (bDownloadSuccessd)
	{
		UE_LOG(GameUpdaterLog, Log, TEXT("Head Content:%s"), *RequestPtr->GetResponse()->GetContentAsString());
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
