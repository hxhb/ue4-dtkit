// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// project header
#include "DownloadFile.h"
#include "MD5Wrapper.hpp"

// engine header
#include "openssl/md5.h"
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
	Paused,
	Canceled,
	Failed,
	Succeeded
};

struct FDownloadRange
{
	uint32 BeginPosition;
	uint32 EndPosition;
};

UCLASS(BlueprintType)
class DOWNLOADTOOKIT_API UDownloadProxy : public UObject
{
	GENERATED_BODY()
public:
	UDownloadProxy();
public:
	/*
		The function is request download a file by URL.
		- InURL: http address of content
		- InSavePathOpt: save the download file to path,need include file name
		- bInSliceOpt: enbale slice download(save memory)
		- InSliceByteSizeOpt: when bInSliceOpt is ture,the option could set a SliceByteSize
		- bForceOpt: forece cancel downloading mission.
	*/
	UFUNCTION(BlueprintCallable,meta=(AdvancedDisplay="InSavePathOpt,bInSliceOpt,InSliceByteSizeOpt,bInForceOpt"))
		void RequestDownload(const FString& InURL,const FString& InSavePathOpt = TEXT(""),bool bInSliceOpt=false,int32 InSliceByteSizeOpt=0,bool bInForceOpt=false);
	UFUNCTION(BlueprintCallable)
		void Pause();
	UFUNCTION(BlueprintCallable)
		bool Resume();
	UFUNCTION(BlueprintCallable)
		bool ReDownload();
	// just cancel download misiion,but dont clean member data.
	UFUNCTION(BlueprintCallable)
		void Cancel();
	// reset all member data to default and clear all delegate(if the download misiion is active,cancel it.)
	UFUNCTION(BlueprintCallable)
		void Reset();
	UFUNCTION(BlueprintCallable)
		bool Tick(float delta);
	UFUNCTION(BlueprintCallable)
		FDownloadFile GetDownloadedFileInfo()const;
	UFUNCTION(BlueprintCallable)
		EDownloadStatus GetDownloadStatus()const;
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
		bool HashCheck(const FString& InMD5Hash)const;

public:
	UPROPERTY(BlueprintAssignable)
		FOnDownloadComplete OnDownloadCompleteDyMultiDlg;
	UPROPERTY(BlueprintAssignable)
		FOnDownloadPaused OnDownloadPausedDyMultiDlg;
	UPROPERTY(BlueprintAssignable)
		FOnDownloadCanceled OnDownloadCanceledDyMultiDlg;
	UPROPERTY(BlueprintAssignable)
		FOnDownloadResumed OnDownloadResumedDyMultiDlg;

protected:
	// download file
	void PreDownloadRequest();
	bool DoDownloadRequest(const FDownloadFile& InDownloadFile, const FDownloadRange& InRange);
	void OnDownloadProcess(FHttpRequestPtr RequestPtr, int32 byteSent, int32 byteReceive);
	void OnDownloadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully);
	// void OnDownloadHeaderReceived(FHttpRequestPtr RequestPtr, const FString& InHeaderName, const FString& InNewHeaderValue);
	// request head get the file size
	void PreRequestHeadInfo(const FDownloadFile& InDownloadFile, bool bAutoDownload=true);
	void OnRequestHeadHeaderReceived(FHttpRequestPtr RequestPtr, const FString& InHeaderName, const FString& InNewHeaderValue);
	void OnRequestHeadComplete(FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bConnectedSuccessfully, bool bAutoDownload);

private:
	FDelegateHandle TickDelegateHandle;
	TSharedPtr<IHttpRequest> HttpRequest;
	FDownloadFile PassInDownloadFileInfo;
	FDownloadFile InternalDownloadFileInfo;
	EDownloadStatus Status;
	int32 TotalDownloadedByte;
	int32 LastRequestedTotalByte;
	int32 DownloadSpeed;
	float DeltaTime;
	FMD5Wrapper Md5Proxy;
	bool bUseSlice;
	uint32 SliceCount;
	int32 SliceByteSize;
};
