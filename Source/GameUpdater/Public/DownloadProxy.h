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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDownloadPaused, UDownloadProxy*, Proxy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDownloadCanceled, UDownloadProxy*, Proxy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDownloadResumed, UDownloadProxy*, Proxy);

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
	UDownloadProxy();
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
	UFUNCTION(BlueprintCallable)
		int32 GetDownloadedSize()const;
	UFUNCTION(BlueprintCallable)
		int32 GetTotalSize()const;
	// byte,current frame - recently frame
	UFUNCTION(BlueprintCallable)
		int32 GetDownloadSpeed()const;
	UFUNCTION(BlueprintCallable)
		float GetDownloadSpeedKbs()const;

	void OnDownloadProcess(FHttpRequestPtr RequestPtr, int32 byteSent, int32 byteReceive, EDownloadType RequestType);
	void OnDownloadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully);

public:
	UPROPERTY(BlueprintAssignable)
		FOnDownloadComplete OnDownloadCompleteDyMulitDlg;
	UPROPERTY(BlueprintAssignable)
		FOnDownloadPaused OnDownloadPausedDyMulitDlg;
	UPROPERTY(BlueprintAssignable)
		FOnDownloadCanceled OnDownloadCanceledDyMulitDlg;
	UPROPERTY(BlueprintAssignable)
		FOnDownloadResumed OnDownloadResumedDyMulitDlg;
protected:
	void PreRequestHeadInfo(const FDownloadFile& InDownloadFile);
	void OnRequestHeadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully);

private:
	FDelegateHandle TickDelegateHandle;
	TSharedPtr<IHttpRequest> HttpRequest;
	FDownloadFile DownloadFileInfo;
	EDownloadStatus Status;
	int32 RequestContentLength;
	int32 TotalDownloadedByte;
	int32 RecentlyPauseTimeDownloadByte;
	int32 DownloadSpeed;
	float DeltaTime;
};
