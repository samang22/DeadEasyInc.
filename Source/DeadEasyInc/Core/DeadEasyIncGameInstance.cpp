// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/DeadEasyIncGameInstance.h"

#include "Graphics/GraphicsSettings.h"

void UDeadEasyIncGameInstance::Init()
{
	Super::Init();

	FGraphicsSettings::ApplyPixelatedRendering();
	
	FGraphicsSettings::SetRenderScale(fRenderScale);
}