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
#include "EngineUtils.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
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
void ADayNightDirector::BeginOverview()
{
    APlayerController* Player=UGameplayStatics::GetPlayerController(this,0);
    if (!Player) return;
    OverviewController=Player;
    if (bAutoFrameShelter && !OverviewViewTarget)
    {
        const FVector Focus=IsValid(OverviewFocus) ? OverviewFocus->GetActorLocation() : GetActorLocation();
        OverviewCamera->SetWorldLocation(Focus+OverviewOffset);
        OverviewCamera->SetWorldRotation((-OverviewOffset).Rotation());
    }
    OverviewCamera->Activate();
    Player->SetViewTargetWithBlend(IsValid(OverviewViewTarget) ? OverviewViewTarget.Get() : this,CameraBlendTime,VTBlend_Cubic,0.f,true);
}
void ADayNightDirector::RestoreView()
{
    if (APlayerController* Player=OverviewController.Get())
        Player->SetViewTargetWithBlend(Player->GetPawn() ? static_cast<AActor*>(Player->GetPawn()) : static_cast<AActor*>(Player),
            CameraBlendTime,VTBlend_Cubic,0.f,true);
    OverviewController.Reset();
}
void ADayNightDirector::StopPlayback()
{
    if (SequencePlayer)
    {
        SequencePlayer->OnFinished.RemoveAll(this);SequencePlayer->OnStop.RemoveAll(this);
        SequencePlayer->Stop();SequencePlayer=nullptr;
    }
    if (SequenceActor) { SequenceActor->Destroy();SequenceActor=nullptr; }
}
void ADayNightDirector::BeginTransition(ABorderGate* Gate,float Duration) { StartTransition(Gate,Duration,true); }
void ADayNightDirector::BeginDayTransition(ABorderGate* Gate,float Duration) { StartTransition(Gate,Duration,false); }
void ADayNightDirector::StartTransition(ABorderGate* Gate,float Duration,bool bToNight)
{
    bActive=false;StopPlayback();RestoreView();
    bTransitionToNight=bToNight;
    TransitionAlpha=bToNight ? 0.f : 1.f;
    bNightLightsOn=!bToNight;
    ActiveGate=Gate;
    TransitionDuration=FMath::Max(.1f,Duration);
    StartTime=GetWorld()->GetTimeSeconds();
    bActive=true;SetActorTickEnabled(true);
    BeginOverview();ApplyLighting();
    ULevelSequence* Sequence=bToNight ? ClosingSequence.Get() : OpeningSequence.Get();
    if (Sequence)
    {
        FMovieSceneSequencePlaybackSettings Playback;Playback.bPauseAtEnd=true;
        ALevelSequenceActor* CreatedActor=nullptr;
        SequencePlayer=ULevelSequencePlayer::CreateLevelSequencePlayer(this,Sequence,Playback,CreatedActor);
        SequenceActor=CreatedActor;
        if (SequencePlayer)
        {
            if (DirectorBinding.IsValid())
                SequenceActor->SetBinding(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(DirectorBinding)),{this},false);
            SequencePlayer->SetDisableCameraCuts(true);
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
    bActive=false;SetActorTickEnabled(false);StopPlayback();
    TransitionAlpha=1.f;bNightLightsOn=true;
    ApplyLighting();RestoreView();
}
void ADayNightDirector::ResetDay()
{
    bActive=false;SetActorTickEnabled(false);StopPlayback();
    TransitionAlpha=0.f;bNightLightsOn=false;
    ApplyLighting();ActiveGate.Reset();RestoreView();
}
void ADayNightDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    bActive=false;StopPlayback();RestoreView();Super::EndPlay(Reason);
}
