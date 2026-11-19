// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DeadEasyIncGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class DEADEASYINC_API UDeadEasyIncGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float fRenderScale = 70.f;
};
