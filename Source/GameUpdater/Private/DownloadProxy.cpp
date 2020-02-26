// Fill out your copyright notice in the Description page of Project Settings.


#include "DownloadProxy.h"
#include "SharedPointer.h"
#include "Interfaces/IHttpRequest.h"

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
			
			TArray<uint8>& ResponseDataArray = const_cast<TArray<uint8>&>(HttpRequest->GetResponse()->GetContent());
			UE_LOG(LogTemp, Log, TEXT("ResponseData Array allocated memory is %d."), ResponseDataArray.GetAllocatedSize());
			ResponseDataArray.Reserve(DownloadFileInfo.Size + 100);
			UE_LOG(LogTemp, Log, TEXT("Reserved ResponseData Array allocated memory is %d."), ResponseDataArray.GetAllocatedSize());
			Status = EDownloadStatus::Downloading;
			UE_LOG(LogTemp, Warning, TEXT("Downloading"));
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
		UE_LOG(LogTemp, Warning, TEXT("Download Pause"));
		TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDownloadProxy::Tick));
	}
}

void UDownloadProxy::Resume()
{
	if (HttpRequest.IsValid() && Status == EDownloadStatus::Pause || Status == EDownloadStatus::Failed)
	{
		UE_LOG(LogTemp, Warning, TEXT("Download Resume begin"));
		HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->OnRequestProgress().BindUObject(this, &UDownloadProxy::OnDownloadProcess,EDownloadType::Resume);
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UDownloadProxy::OnDownloadComplete);
		HttpRequest->SetURL(DownloadFileInfo.URL);
		HttpRequest->SetVerb(TEXT("GET"));
		FString RangeArgs = TEXT("bytes=") + FString::FromInt(TotalDownloadedByte) + TEXT("-");
		HttpRequest->SetHeader(TEXT("Range"), RangeArgs);
		if (HttpRequest->ProcessRequest())
		{
			TArray<uint8>& ResponseDataArray = const_cast<TArray<uint8>&>(HttpRequest->GetResponse()->GetContent());
			UE_LOG(LogTemp, Log, TEXT("ResponseData Array allocated memory is %d."), ResponseDataArray.GetAllocatedSize());
			ResponseDataArray.Reserve(DownloadFileInfo.Size + 100 - TotalDownloadedByte);
			UE_LOG(LogTemp, Log, TEXT("Reserved ResponseData Array allocated memory is %d."), ResponseDataArray.GetAllocatedSize());

			UE_LOG(LogTemp, Log, TEXT("Resume ResponseData Array allocated memory is %d."), HttpRequest->GetResponse()->GetContent().GetAllocatedSize());
			Status = EDownloadStatus::Downloading;
			UE_LOG(LogTemp, Warning, TEXT("Download Resume,Range Args:%s"),*RangeArgs);
		}
	}
}

void UDownloadProxy::Cancel()
{
	if (HttpRequest.IsValid())
	{
		HttpRequest->CancelRequest();
		Status = EDownloadStatus::Canceled;
		UE_LOG(LogTemp, Warning, TEXT("Download Cancel"));
	}
}

bool UDownloadProxy::Tick(float delta)
{

	return true;
}

void UDownloadProxy::OnDownloadProcess(FHttpRequestPtr RequestPtr, int32 byteSent, int32 byteReceive, EDownloadType RequestType)
{
	uint32 PaddingLength = 0;
	unsigned char* PaddingData = NULL;
	const TArray<uint8>& ResponseDataArray = RequestPtr->GetResponse()->GetContent();
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
	if (Status != EDownloadStatus::Failed && FFileHelper::SaveArrayToFile(TArrayView<const uint8>(PaddingData, PaddingLength), *FPaths::Combine(FPaths::ProjectDir(), DownloadFileInfo.Name), &IFileManager::Get(), EFileWrite::FILEWRITE_Append | EFileWrite::FILEWRITE_AllowRead | EFileWrite::FILEWRITE_EvenIfReadOnly))
	{
		TotalDownloadedByte += PaddingLength;
		UE_LOG(LogTemp, Warning, TEXT("%s Downloading size:%d."), EDownloadType::Start == RequestType ? TEXT("START") : TEXT("RESUME"), TotalDownloadedByte);
		if (EDownloadStatus::Pause == Status)
		{
			RecentlyPauseTimeDownloadByte = TotalDownloadedByte;
			UE_LOG(LogTemp, Warning, TEXT("Download mission is paused,downloaded size is:%d."), TotalDownloadedByte);
		}
	}
}

void UDownloadProxy::OnDownloadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully)
{
	if (Status != EDownloadStatus::Pause)
		OnDownloadCompleteDyMulitDlg.Broadcast(this, bConnectedSuccessfully);
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	if (bConnectedSuccessfully)
	{
		Status = EDownloadStatus::Succeeded;
		UE_LOG(LogTemp, Warning, TEXT("Download Success"));
	}
	else
	{
		Status = EDownloadStatus::Failed;
		UE_LOG(LogTemp, Warning, TEXT("Download Faild"));
	}

}