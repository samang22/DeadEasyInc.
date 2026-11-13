#include "Core/DayNight/DayNightDirector.h"
#include "Buildings/Defense/BorderGate.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/Light.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneObjectBindingID.h"
#include "Engine/World.h"

ADayNightDirector::ADayNightDirector() { PrimaryActorTick.bCanEverTick = true; }
void ADayNightDirector::BeginPlay()
{
    Super::BeginPlay();
    if (Sun) { DaySunIntensity = Sun->GetLightComponent()->Intensity; DaySunRotation = Sun->GetActorRotation(); }
    if (Sky) DaySkyIntensity = Sky->GetLightComponent()->Intensity;
    ResetDay();
}
void ADayNightDirector::ApplyLighting()
{
    const float Alpha = FMath::Clamp(TransitionAlpha,0.f,1.f);
    if (Sun)
    {
        Sun->GetLightComponent()->SetIntensity(FMath::Lerp(DaySunIntensity, Settings ? Settings->NightSunIntensity : .05f,Alpha));
        FRotator Night = DaySunRotation;
        Night.Pitch = Settings ? Settings->NightSunPitch : -25.f;
        Sun->SetActorRotation(FMath::Lerp(DaySunRotation,Night,Alpha));
    }
    if (Sky) Sky->GetLightComponent()->SetIntensity(FMath::Lerp(DaySkyIntensity,Settings ? Settings->NightSkyIntensity : .15f,Alpha));
    for (ALight* Light : NightLights) if (IsValid(Light)) Light->GetLightComponent()->SetVisibility(Alpha >= .35f);
    for (ULightComponent* Light : NightLightComponents) if (IsValid(Light)) Light->SetVisibility(Alpha >= .35f);
    if (ActiveGate.IsValid()) ActiveGate->SetOpenFraction(1.f-Alpha);
}
void ADayNightDirector::BeginTransition(ABorderGate* Gate, float Duration)
{
    ResetDay();
    ActiveGate = Gate;
    TransitionDuration = FMath::Max(.1f,Duration);
    StartTime = GetWorld()->GetTimeSeconds();
    bActive = true;
    if (ClosingSequence)
    {
        FMovieSceneSequencePlaybackSettings Playback;
        Playback.bPauseAtEnd = true;
        ALevelSequenceActor* CreatedActor = nullptr;
        SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this,ClosingSequence,Playback,CreatedActor);
        SequenceActor = CreatedActor;
        if (SequencePlayer)
        {
            if (DirectorBinding.IsValid())
                SequenceActor->SetBinding(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(DirectorBinding)),{this},false);
            SequencePlayer->OnFinished.AddDynamic(this,&ADayNightDirector::CompleteTransition);
            SequencePlayer->OnStop.AddDynamic(this,&ADayNightDirector::CompleteTransition);
            SequencePlayer->SetPlayRate(FMath::Max(.01f,static_cast<float>(SequencePlayer->GetDuration().AsSeconds())/TransitionDuration));
            SequencePlayer->Play();
        }
    }
}
void ADayNightDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bActive) return;
    if (!SequencePlayer)
    {
        TransitionAlpha = FMath::Clamp((GetWorld()->GetTimeSeconds()-StartTime)/TransitionDuration,0.f,1.f);
        if (TransitionAlpha >= 1.f) { CompleteTransition(); return; }
    }
    ApplyLighting();
}
void ADayNightDirector::CompleteTransition()
{
    if (!bActive) return;
    SetNight();
    OnTransitionFinished.Broadcast();
}
void ADayNightDirector::SetNight()
{
    bActive = false;
    TransitionAlpha = 1.f;
    ApplyLighting();
}
void ADayNightDirector::ResetDay()
{
    bActive = false;
    if (SequencePlayer) { SequencePlayer->Stop(); SequencePlayer = nullptr; }
    if (SequenceActor) { SequenceActor->Destroy(); SequenceActor = nullptr; }
    TransitionAlpha = 0.f;
    ApplyLighting();
    ActiveGate.Reset();
}
