#pragma once

#include "CoreMinimal.h"
#include "DownloadFile.generated.h"

USTRUCT(BlueprintType)
struct DOWNLOADTOOKIT_API FDownloadFile
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
		FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString URL;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		int32 Size;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString HASH;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString SavePath;

	FORCEINLINE bool operator==(const FDownloadFile& Rhs)
	{
		bool bSameName = Name == Rhs.Name;
		bool bSameURL = URL == Rhs.URL;
		bool bSameSize = Size == Rhs.Size;
		bool bSameHash = HASH == Rhs.HASH;

		return bSameName && bSameURL && bSameSize && bSameHash;
	}
	FDownloadFile() = default;
	FDownloadFile(const FDownloadFile&) = default;
};