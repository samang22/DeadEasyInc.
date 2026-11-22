#include "Scavenging/CorpseProcessor.h"
#include "Scavenging/CorpseFeedComponent.h"
#include "Scavenging/ScavengingData.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Zombies/BorderZombie.h"
#include "Core/BorderGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

ACorpseProcessor::ACorpseProcessor()
{
    PrimaryActorTick.bCanEverTick=true;
    FeedProcess=CreateDefaultSubobject<UCorpseFeedComponent>(TEXT("FeedProcess"));
    Intake=CreateDefaultSubobject<UBoxComponent>(TEXT("Intake"));Intake->SetupAttachment(GetStaticMeshComponent());
    Intake->SetRelativeLocation(FVector(-48,0,-10));Intake->SetBoxExtent(FVector(18,35,30));
    Intake->SetCollisionObjectType(ECC_GameTraceChannel3);Intake->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Intake->SetCollisionResponseToAllChannels(ECR_Ignore);
    Intake->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Overlap);Intake->SetGenerateOverlapEvents(true);Intake->SetCanEverAffectNavigation(false);
    ConsumePoint=CreateDefaultSubobject<USceneComponent>(TEXT("ConsumePoint"));ConsumePoint->SetupAttachment(GetStaticMeshComponent());
    ConsumePoint->SetRelativeLocation(FVector(0,0,-10));
    FeedMotor=CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("FeedMotor"));
    FeedMotor->LinearStiffness=3000.f;FeedMotor->LinearDamping=500.f;FeedMotor->InterpolationSpeed=8.f;

}
void ACorpseProcessor::BeginPlay()
{
    Super::BeginPlay();
    if (!ScavengingSettings) ScavengingSettings=GetClass()->GetDefaultObject<ACorpseProcessor>()->ScavengingSettings;
    FeedProcess->Configure(this,Intake,ConsumePoint,GetStaticMeshComponent(),FeedMotor);
    FeedMotor->SetComponentTickEnabled(false);
    if (auto* Mode=GetWorld()->GetAuthGameMode<ABorderGameMode>()) Mode->OnRunFailed.AddUniqueDynamic(this,&ACorpseProcessor::ResetProcessing);
}
void ACorpseProcessor::EndPlay(const EEndPlayReason::Type Reason) { ResetProcessing();Super::EndPlay(Reason); }
void ACorpseProcessor::ResetProcessing() { FeedProcess->Reset(); }
bool ACorpseProcessor::SnapHousingToGround()
{
    if (GetWorld()->IsGameWorld()) return false;
    ScavengingSettings=GetClass()->GetDefaultObject<ACorpseProcessor>()->ScavengingSettings;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SorterGroundPlacement),false,this);
    if (Boiler) Params.AddIgnoredActor(Boiler);
    FCollisionObjectQueryParams Objects(ECC_WorldStatic);FHitResult Hit;
    const FVector Position=GetActorLocation();
    if (!GetWorld()->LineTraceSingleByObjectType(Hit,Position+FVector(0,0,1000),Position-FVector(0,0,5000),Objects,Params)) return false;
    FVector Min,Max;GetStaticMeshComponent()->GetLocalBounds(Min,Max);
    FVector Grounded=Position;Grounded.Z=Hit.ImpactPoint.Z-Min.Z*GetActorScale3D().Z+2.f;
    // Editor placement of a static mesh actor is allowed; this never runs during play.
    SetActorLocation(Grounded);return true;
}
void ACorpseProcessor::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* Mode=GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if (Condition==EBuildingCondition::Destroyed || (Mode && (Mode->bGameOver || Mode->bTransitioning || Mode->bIsNight))) { ResetProcessing();return; }
    auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0);
    auto* Player=Pawn ? Pawn->FindComponentByClass<UPlayerLoadoutComponent>() : nullptr;
    if (!Player) { ResetProcessing();return; }
    if (!ScavengingSettings) return;
    const TArray<ABorderZombie*> Consumed=FeedProcess->Advance(Dt,Pawn,ScavengingSettings);
    const int32 Count=Consumed.Num();
    if (Count==0) return;
    const int32 Wood=FMath::Max(0,ScavengingSettings->CorpseWood)*Count,Iron=FMath::Max(0,ScavengingSettings->CorpseIron)*Count,Fuel=FMath::Max(0,FMath::RoundToInt(ScavengingSettings->CorpseFuel))*Count;
    Player->AddResource(TEXT("Wood"),Wood);Player->AddResource(TEXT("Iron"),Iron);Player->AddResource(TEXT("Fuel"),Fuel);
    if (!IsValid(Boiler)) for (TActorIterator<ACentralBoiler> It(GetWorld());It;++It) { Boiler=*It;break; }
    if (Boiler && Boiler->CombatHealth->Health>0.f)
    {
        const int32 Transfer=FMath::Min(Player->GetResource(TEXT("Fuel")),FMath::FloorToInt(FMath::Max(0.f,Boiler->Capacity()-Boiler->Fuel)));
        if (Transfer>0) { Boiler->AddFuel(Transfer);Player->SpendResource(TEXT("Fuel"),Transfer); }
    }
    Player->ShowResourceGain(Wood,Iron,Fuel);
    ResetProcessing();
    for (ABorderZombie* Corpse : Consumed) if (IsValid(Corpse)) Corpse->Destroy();
    if (Mode) Mode->RecordProcessedCorpses(Count);
    OnProcessed(Count);
}
