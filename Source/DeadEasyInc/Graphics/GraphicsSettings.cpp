#include "Graphics/GraphicsSettings.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"

void FGraphicsSettings::SetRenderScale(float Percent)
{
	// 10% ~ 100% 범위
	Percent = FMath::Clamp(Percent, 10.0f, 100.0f);

	IConsoleVariable* CVar =
		IConsoleManager::Get().FindConsoleVariable(
			TEXT("r.ScreenPercentage")
		);

	if (!CVar)
	{
		UE_LOG(LogTemp, Error,
			TEXT("r.ScreenPercentage not found"));
		return;
	}

	CVar->Set(Percent, ECVF_SetByGameSetting);

	UE_LOG(LogTemp, Warning,
		TEXT("Requested Render Scale: %.1f%%, CVar: %.1f%%"),
		Percent, CVar->GetFloat());
}

void FGraphicsSettings::ApplyPixelatedRendering()
{
	IConsoleManager& Console = IConsoleManager::Get();

	if (IConsoleVariable* CVar =
		Console.FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
	{
		CVar->Set(1, ECVF_SetByGameSetting);
	}

	if (IConsoleVariable* CVar =
		Console.FindConsoleVariable(TEXT("r.Upscale.Quality")))
	{
		// Nearest Neighbor
		CVar->Set(0, ECVF_SetByGameSetting);
	}
}