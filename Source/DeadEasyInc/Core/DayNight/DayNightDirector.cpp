#include "Core/DayNight/DayNightDirector.h"
#include "Core/DayNight/DayNightCinematicComponent.h"
#include "Buildings/Defense/BorderGate.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/Light.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "LevelSequence.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "Buildings/Heating/CentralBoiler.h"

ADayNightDirector::ADayNightDirector()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    OverviewCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("OverviewCamera"));
    OverviewCamera->SetupAttachment(RootComponent);
    OverviewCamera->SetRelativeLocation(OverviewOffset);
    OverviewCamera->SetRelativeRotation((-OverviewOffset).Rotation());
    OverviewCamera->FieldOfView=70.f;
    Cinematic=CreateDefaultSubobject<UDayNightCinematicComponent>(TEXT("Cinematic"));
}
void ADayNightDirector::BeginPlay()
{
    Super::BeginPlay();
    if (!Sun) for (TActorIterator<ADirectionalLight> It(GetWorld());It;++It) { Sun=*It;break; }
    if (!Sky) for (TActorIterator<ASkyLight> It(GetWorld());It;++It) { Sky=*It;break; }
    if (!OverviewFocus) for (TActorIterator<ACentralBoiler> It(GetWorld());It;++It) { OverviewFocus=*It;break; }
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
    for (ALight* Light : NightLights) if (IsValid(Light)) Light->GetLightComponent()->SetVisibility(bNightLightsOn);
    for (ULightComponent* Light : NightLightComponents) if (IsValid(Light)) Light->SetVisibility(bNightLightsOn);
    if (ActiveGate.IsValid()) ActiveGate->SetOpenFraction(1.f-Alpha);
}
void ADayNightDirector::BeginTransition(ABorderGate* Gate,float Duration) { StartTransition(Gate,Duration,true); }
void ADayNightDirector::BeginDayTransition(ABorderGate* Gate,float Duration) { StartTransition(Gate,Duration,false); }
void ADayNightDirector::StartTransition(ABorderGate* Gate,float Duration,bool bToNight)
{
    bActive=false;Cinematic->StopPlayback();Cinematic->RestoreView(CameraBlendTime);
    bTransitionToNight=bToNight;
    TransitionAlpha=bToNight ? 0.f : 1.f;
    bNightLightsOn=!bToNight;
    ActiveGate=Gate;
    TransitionDuration=FMath::Max(.1f,Duration);
    StartTime=GetWorld()->GetTimeSeconds();
    bActive=true;SetActorTickEnabled(true);
    Cinematic->BeginOverview(*OverviewCamera, OverviewViewTarget, OverviewFocus, bAutoFrameShelter,
        OverviewOffset, CameraBlendTime);
    ApplyLighting();
    Cinematic->PlaySequence(bToNight ? ClosingSequence.Get() : OpeningSequence.Get(), DirectorBinding,
        TransitionDuration, FSimpleDelegate::CreateUObject(this, &ADayNightDirector::CompleteTransition));
}
void ADayNightDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bActive) return;
    if (!Cinematic->HasSequencePlayer())
    {
        const float Progress=FMath::Clamp((GetWorld()->GetTimeSeconds()-StartTime)/TransitionDuration,0.f,1.f);
        TransitionAlpha=bTransitionToNight ? Progress : 1.f-Progress;
        if (Progress>=1.f) { CompleteTransition();return; }
    }
    ApplyLighting();
}
void ADayNightDirector::CompleteTransition()
{
    if (!bActive) return;
    if (bTransitionToNight) SetNight();
    else ResetDay();
    OnTransitionFinished.Broadcast();
}
void ADayNightDirector::SetNight()
{
    bActive=false;SetActorTickEnabled(false);Cinematic->StopPlayback();
    TransitionAlpha=1.f;bNightLightsOn=true;
    ApplyLighting();Cinematic->RestoreView(CameraBlendTime);
}
void ADayNightDirector::ResetDay()
{
    bActive=false;SetActorTickEnabled(false);Cinematic->StopPlayback();
    TransitionAlpha=0.f;bNightLightsOn=false;
    ApplyLighting();ActiveGate.Reset();Cinematic->RestoreView(CameraBlendTime);
}
void ADayNightDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    bActive=false;Cinematic->StopPlayback();Cinematic->RestoreView(CameraBlendTime);Super::EndPlay(Reason);
}
