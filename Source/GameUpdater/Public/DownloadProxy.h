// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// project header
#include "DownloadFile.h"

// engine header
#include "Http.h"
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DownloadProxy.generated.h"


class UDownloadProxy;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDownloadComplete, UDownloadProxy*, Proxy, bool, bSuccess);

UENUM(BlueprintType)
enum class EDownloadStatus:uint8
{
	NotStarted,
	Downloading,
	Pause,
	Canceled,
	Failed,
	Succeeded
};

UENUM(BlueprintType)
enum class EDownloadType :uint8
{
	Start,
	Resume
};
/**
 * 
 */
UCLASS(BlueprintType)
class GAMEUPDATER_API UDownloadProxy : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
		void RequestDownload(const FDownloadFile& InDownloadFile);
	UFUNCTION(BlueprintCallable)
		void Pause();
	UFUNCTION(BlueprintCallable)
		void Resume();
	UFUNCTION(BlueprintCallable)
		void Cancel();
	UFUNCTION(BlueprintCallable)
		bool Tick(float delta);

	void OnDownloadProcess(FHttpRequestPtr RequestPtr, int32 byteSent, int32 byteReceive, EDownloadType RequestType);
	void OnDownloadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully);

public:
	UPROPERTY(BlueprintAssignable)
		FOnDownloadComplete OnDownloadCompleteDyMulitDlg;
public:
	FDelegateHandle TickDelegateHandle;
	TSharedPtr<IHttpRequest> HttpRequest;
	FDownloadFile DownloadFileInfo;
	EDownloadStatus Status;
	int32 TotalDownloadedByte;
	int32 RecentlyPauseTimeDownloadByte;
};
