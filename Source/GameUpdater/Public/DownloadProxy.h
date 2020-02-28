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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHashChecked, UDownloadProxy*, Proxy, bool, bMatch);

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
	UFUNCTION(BlueprintCallable,meta=(AdvancedDisplay="InSavePath"))
		void RequestDownload(const FString& InURL,const FString& InSavePath = TEXT(""));
	UFUNCTION(BlueprintCallable)
		void Pause();
	UFUNCTION(BlueprintCallable)
		void Resume();
	UFUNCTION(BlueprintCallable)
		void Cancel();
	UFUNCTION(BlueprintCallable)
		void Reset();
	UFUNCTION(BlueprintCallable)
		bool Tick(float delta);
	UFUNCTION(BlueprintCallable)
		int32 GetDownloadedSize()const;
	UFUNCTION(BlueprintCallable)
		int32 GetTotalSize()const;
	UFUNCTION(BlueprintCallable)
		float GetDownloadProgress()const;
	// byte,current frame - recently frame
	UFUNCTION(BlueprintCallable)
		int32 GetDownloadSpeed()const;
	UFUNCTION(BlueprintCallable)
		float GetDownloadSpeedKbs()const;
	UFUNCTION(BlueprintCallable)
		bool HashCheck(const FString& InMD5Hash,bool bInAsync);


public:
	UPROPERTY(BlueprintAssignable)
		FOnDownloadComplete OnDownloadCompleteDyMulitDlg;
	UPROPERTY(BlueprintAssignable)
		FOnDownloadPaused OnDownloadPausedDyMulitDlg;
	UPROPERTY(BlueprintAssignable)
		FOnDownloadCanceled OnDownloadCanceledDyMulitDlg;
	UPROPERTY(BlueprintAssignable)
		FOnDownloadResumed OnDownloadResumedDyMulitDlg;
	UPROPERTY(BlueprintAssignable)
		FOnHashChecked OnHashCheckedDyMulitDlg;
protected:
	// download file
	void DoDownloadRequest(const FDownloadFile& InDownloadFile, int32 InFileSize);
	void OnDownloadProcess(FHttpRequestPtr RequestPtr, int32 byteSent, int32 byteReceive, EDownloadType RequestType);
	void OnDownloadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully);

	// request head get the file size
	void PreRequestHeadInfo(const FDownloadFile& InDownloadFile);
	void OnRequestHeadHeaderReceived(FHttpRequestPtr RequestPtr, const FString& InHeaderName, const FString& InNewHeaderValue);
	void OnRequestHeadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully);

private:
	FDelegateHandle TickDelegateHandle;
	TSharedPtr<IHttpRequest> HttpRequest;
	FDownloadFile DownloadFileInfo;
	EDownloadStatus Status;
	int32 FileTotalSize;
	int32 RequestContentLength;
	int32 TotalDownloadedByte;
	int32 RecentlyPauseTimeDownloadByte;
	int32 DownloadSpeed;
	float DeltaTime;
};
