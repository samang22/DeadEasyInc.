#include "Scavenging/CorpseProcessor.h"
#include "Scavenging/CorpseHandlingComponent.h"
#include "Scavenging/Handcart.h"
#include "Scavenging/ScavengingData.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Zombies/BorderZombie.h"
#include "Core/BorderGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

ACorpseProcessor::ACorpseProcessor()
{
    PrimaryActorTick.bCanEverTick=true;
    Rotor=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rotor"));Rotor->SetupAttachment(GetStaticMeshComponent());
    Rotor->SetMobility(EComponentMobility::Movable);Rotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);Rotor->SetCanEverAffectNavigation(false);
    Intake=CreateDefaultSubobject<UBoxComponent>(TEXT("Intake"));Intake->SetupAttachment(GetStaticMeshComponent());
    Intake->SetRelativeLocation(FVector(-48,0,-10));Intake->SetBoxExtent(FVector(18,35,30));
    Intake->SetCollisionObjectType(ECC_GameTraceChannel3);Intake->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Intake->SetCollisionResponseToAllChannels(ECR_Ignore);
    Intake->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Overlap);Intake->SetGenerateOverlapEvents(true);Intake->SetCanEverAffectNavigation(false);
    ConsumePoint=CreateDefaultSubobject<USceneComponent>(TEXT("ConsumePoint"));ConsumePoint->SetupAttachment(GetStaticMeshComponent());
    ConsumePoint->SetRelativeLocation(FVector(0,0,-10));
    FeedChute=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FeedChute"));FeedChute->SetupAttachment(GetStaticMeshComponent());
    FeedChute->SetMobility(EComponentMobility::Movable);FeedChute->SetCanEverAffectNavigation(false);
    FeedChute->SetCollisionObjectType(ECC_GameTraceChannel5);FeedChute->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    FeedChute->SetCollisionResponseToAllChannels(ECR_Ignore);FeedChute->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Block);
    for (int32 Index=0;Index<2;++Index)
    {
        auto* Side=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ChuteSide%d"),Index));Side->SetupAttachment(GetStaticMeshComponent());
        Side->SetMobility(EComponentMobility::Movable);Side->SetCanEverAffectNavigation(false);
        Side->SetCollisionObjectType(ECC_GameTraceChannel5);Side->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
        Side->SetCollisionResponseToAllChannels(ECR_Ignore);Side->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Block);ChuteSides.Add(Side);
    }
    FeedMotor=CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("FeedMotor"));
    FeedMotor->LinearStiffness=3000.f;FeedMotor->LinearDamping=500.f;FeedMotor->InterpolationSpeed=8.f;
    for (int32 Index=0;Index<4;++Index)
    {
        auto* Part=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("HopperPart%d"),Index));
        Part->SetupAttachment(GetStaticMeshComponent());Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCanEverAffectNavigation(false);HopperParts.Add(Part);
    }
}
void ACorpseProcessor::BeginPlay()
{
    Super::BeginPlay();
    if (!ScavengingSettings) ScavengingSettings=GetClass()->GetDefaultObject<ACorpseProcessor>()->ScavengingSettings;
    if (auto* Mode=GetWorld()->GetAuthGameMode<ABorderGameMode>()) Mode->OnRunFailed.AddUniqueDynamic(this,&ACorpseProcessor::ResetProcessing);
}
void ACorpseProcessor::EndPlay(const EEndPlayReason::Type Reason) { ResetProcessing();Super::EndPlay(Reason); }
void ACorpseProcessor::ResetProcessing()
{
    FeedMotor->ReleaseComponent();
    if (IsValid(ProcessingCorpse)) { ProcessingCorpse->ProcessingOwner.Reset();ProcessingCorpse->ApplyCorpseCollision(); }
    ProcessingCorpse=nullptr;FeedTime=0.f;
}
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
    const auto* Mode=GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if (Condition==EBuildingCondition::Destroyed || (Mode && (Mode->bGameOver || Mode->bTransitioning || Mode->bIsNight))) { ResetProcessing();return; }
    auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0);
    auto* Player=Pawn ? Pawn->FindComponentByClass<UPlayerLoadoutComponent>() : nullptr;
    if (!Player) { ResetProcessing();return; }
    if (!ScavengingSettings) return;
    const FName Bone=ScavengingSettings->GrabBone;
    if (ProcessingCorpse && (!IsValid(ProcessingCorpse) || ProcessingCorpse->IsActorBeingDestroyed())) { ResetProcessing();return; }
    if (!ProcessingCorpse && GetWorld()->GetTimeSeconds()>=NextIntakeScan)
    {
        NextIntakeScan=GetWorld()->GetTimeSeconds()+.12f;
        TArray<AActor*> Candidates;Intake->GetOverlappingActors(Candidates,ABorderZombie::StaticClass());
        for (AActor* Candidate : Candidates)
        {
            auto* Corpse=Cast<ABorderZombie>(Candidate);
            if (!Corpse || !Corpse->bDead || Corpse->ProcessingOwner.IsValid()) continue;
            const auto* PhysicsBody=Corpse->GetMesh()->GetBodyInstance(Bone);
            const FVector Pelvis=PhysicsBody ? PhysicsBody->GetUnrealWorldTransform().GetLocation() : Corpse->GetMesh()->GetSocketLocation(Bone);
            const FVector Local=Intake->GetComponentTransform().InverseTransformPosition(Pelvis);
            const FVector Extent=Intake->GetUnscaledBoxExtent();
            if (FMath::Abs(Local.X)>=Extent.X || FMath::Abs(Local.Y)>=Extent.Y || FMath::Abs(Local.Z)>=Extent.Z) continue;
            if (Corpse->Carrier.IsValid()) Corpse->Carrier->DetachCorpse(Corpse);
            if (auto* Work=Pawn->FindComponentByClass<UCorpseHandlingComponent>()) Work->DetachCorpse(Corpse);
            ProcessingCorpse=Corpse;Corpse->ProcessingOwner=this;FeedStart=Pelvis;FeedTime=0.f;
            // The motor pulls the real ragdoll through the housing, without pushing the machine or floor.
            Corpse->GetMesh()->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Ignore);
            Corpse->GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Ignore);
            FeedMotor->GrabComponentAtLocation(Corpse->GetMesh(),Bone,Pelvis);
            if (!FeedMotor->GetGrabbedComponent()) ResetProcessing();
            break;
        }
    }
    if (!ProcessingCorpse) return;
    FeedTime+=Dt;
    const float Duration=FMath::Max(.5f,ScavengingSettings->ProcessingSeconds);
    const float Alpha=FMath::Clamp(FeedTime/Duration,0.f,1.f);
    FeedMotor->SetTargetLocation(FMath::Lerp(FeedStart,ConsumePoint->GetComponentLocation(),Alpha));
    Rotor->AddLocalRotation(FRotator(0,Dt*720.f,0));
    if (FeedTime<Duration) return;
    const auto* PhysicsBody=ProcessingCorpse->GetMesh()->GetBodyInstance(Bone);
    const FVector Pelvis=PhysicsBody ? PhysicsBody->GetUnrealWorldTransform().GetLocation() : ProcessingCorpse->GetMesh()->GetSocketLocation(Bone);
    const FVector Local=GetStaticMeshComponent()->GetComponentTransform().InverseTransformPosition(Pelvis);
    const bool bInside=FMath::Abs(Local.X)<48.f && FMath::Abs(Local.Y)<48.f && Local.Z<35.f && Local.Z>-48.f;
    if (!bInside)
    {
        if (FeedTime>Duration+4.f) ResetProcessing();
        return;
    }
    const int32 Wood=FMath::Max(0,ScavengingSettings->CorpseWood),Iron=FMath::Max(0,ScavengingSettings->CorpseIron),Fuel=FMath::Max(0,FMath::RoundToInt(ScavengingSettings->CorpseFuel));
    Player->AddResource(TEXT("Wood"),Wood);Player->AddResource(TEXT("Iron"),Iron);Player->AddResource(TEXT("Fuel"),Fuel);
    if (!IsValid(Boiler)) for (TActorIterator<ACentralBoiler> It(GetWorld());It;++It) { Boiler=*It;break; }
    if (Boiler && Boiler->CombatHealth->Health>0.f)
    {
        const int32 Transfer=FMath::Min(Player->GetResource(TEXT("Fuel")),FMath::FloorToInt(FMath::Max(0.f,Boiler->Capacity()-Boiler->Fuel)));
        if (Transfer>0) { Boiler->AddFuel(Transfer);Player->SpendResource(TEXT("Fuel"),Transfer); }
    }
    Player->ShowResourceGain(Wood,Iron,Fuel);
    ABorderZombie* Consumed=ProcessingCorpse;ResetProcessing();Consumed->Destroy();OnProcessed(1);
}
