#include "Buildings/Lighting/BorderSpotlight.h"
#include "Core/DayNight/DayNightDirector.h"
#include "Core/BorderGameMode.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ABorderSpotlight::ABorderSpotlight()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Static);
    Fixture = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fixture"));
    Fixture->SetupAttachment(RootComponent);
    Fixture->SetMobility(EComponentMobility::Static);
    Fixture->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Fixture->SetCanEverAffectNavigation(false);
    Spotlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Spotlight"));
    Spotlight->SetupAttachment(RootComponent);
    Spotlight->SetMobility(EComponentMobility::Movable);
    Spotlight->SetRelativeLocation(FVector(25.f,0.f,-10.f));
    Spotlight->SetRelativeRotation(FRotator(-50.f,0.f,0.f));
    Spotlight->SetIntensityUnits(ELightUnits::Lumens);
    Spotlight->SetIntensity(8000.f);
    Spotlight->SetLightColor(FLinearColor(1.f,.9f,.72f));
    Spotlight->SetAttenuationRadius(3500.f);
    Spotlight->SetInnerConeAngle(25.f);
    Spotlight->SetOuterConeAngle(45.f);
    Spotlight->SetCastShadows(false);
}

void ABorderSpotlight::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<ADayNightDirector> It(GetWorld()); It; ++It)
    {
        Director = *It;
        It->NightLightComponents.AddUnique(Spotlight);
        Spotlight->SetVisibility(It->AreNightLightsOn());
        return;
    }
    // Maps without a lighting director still follow the gameplay phase.
    Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if (Mode.IsValid()) Mode->OnPhaseStateChanged.AddDynamic(this,&ABorderSpotlight::HandlePhase);
    Spotlight->SetVisibility(Mode.IsValid() && Mode->bIsNight);
}

void ABorderSpotlight::HandlePhase(bool IsNight, int32 Day)
{
    Spotlight->SetVisibility(IsNight);
}

void ABorderSpotlight::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Director.IsValid()) Director->NightLightComponents.Remove(Spotlight);
    if (Mode.IsValid()) Mode->OnPhaseStateChanged.RemoveAll(this);
    Super::EndPlay(Reason);
}
