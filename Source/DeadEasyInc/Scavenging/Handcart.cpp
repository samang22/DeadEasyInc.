#include "Scavenging/Handcart.h"
#include "Scavenging/HandcartMovementComponent.h"
#include "Scavenging/HandcartDischargeComponent.h"
#include "Scavenging/ScavengingData.h"
#include "Zombies/BorderZombie.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Scavenging/CorpseCargoComponent.h"
#include "Core/BorderGameMode.h"
#include "Engine/World.h"
#include "Progression/PlayerLoadoutComponent.h"

AHandcart::AHandcart()
{
    PrimaryActorTick.bCanEverTick = true;

    CartMovement=CreateDefaultSubobject<UHandcartMovementComponent>(TEXT("CartMovement"));
    Discharge=CreateDefaultSubobject<UHandcartDischargeComponent>(TEXT("Discharge"));
    CargoStorage=CreateDefaultSubobject<UCorpseCargoComponent>(TEXT("CargoStorage"));
    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
    RootComponent = Collision;
    
    SetRootComponent(Collision);
    // 손수레 크기에 맞게 조정할 기본값
    Collision->InitBoxExtent(FVector(100.f, 70.f, 50.f));
    // 충돌 설정
    Collision->SetCollisionProfileName(TEXT("BlockAll"));

    // 물리 시뮬레이션 비활성화
    Collision->SetSimulatePhysics(false);

    // 불필요한 Overlap 이벤트 비활성화
    Collision->SetGenerateOverlapEvents(false);

    // 이동 가능한 액터
    Collision->SetMobility(EComponentMobility::Movable);    
    GrabTarget=CreateDefaultSubobject<UBoxComponent>(TEXT("GrabTarget"));GrabTarget->SetupAttachment(RootComponent);
    GrabTarget->SetRelativeLocation(FVector(-160,0,20));GrabTarget->SetBoxExtent(FVector(45,70,25));
    GrabTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);GrabTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
    GrabTarget->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);GrabTarget->SetCanEverAffectNavigation(false);
    
    BedPivot=CreateDefaultSubobject<USceneComponent>(TEXT("BedPivot"));BedPivot->SetupAttachment(RootComponent);
    
    WheelAxle=CreateDefaultSubobject<USceneComponent>(TEXT("WheelAxle"));WheelAxle->SetupAttachment(RootComponent);
    
    Bed=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bed"));Bed->SetupAttachment(BedPivot);
    
    TailgatePivot=CreateDefaultSubobject<USceneComponent>(TEXT("TailgatePivot"));TailgatePivot->SetupAttachment(Bed);
    Tailgate=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tailgate"));Tailgate->SetupAttachment(TailgatePivot);
    Tailgate->SetCollisionEnabled(ECollisionEnabled::NoCollision);Tailgate->SetCanEverAffectNavigation(false);
    auto ConfigureCargoBox=[](UBoxComponent* Box)
    {
        Box->SetMobility(EComponentMobility::Movable);Box->SetCollisionObjectType(ECC_GameTraceChannel5);
        Box->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Block);Box->SetCanEverAffectNavigation(false);
    };
    
    CargoVolume=CreateDefaultSubobject<UBoxComponent>(TEXT("CargoVolume"));
    CargoVolume->SetupAttachment(Bed);
    CargoVolume->SetRelativeLocation(FVector(0,-4,72));
    CargoVolume->SetBoxExtent(FVector(50,78,110));
    CargoVolume->SetCollisionObjectType(ECC_GameTraceChannel5);
    CargoVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CargoVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    CargoVolume->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Overlap);
    CargoVolume->SetGenerateOverlapEvents(true);CargoVolume->SetCanEverAffectNavigation(false);
    CargoFloor=CreateDefaultSubobject<UBoxComponent>(TEXT("CargoFloor"));
    CargoFloor->SetupAttachment(Bed);
    ConfigureCargoBox(CargoFloor);
    for (int32 Index=0;Index<4;++Index)
    {
        auto* Wall=CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("CargoWall%d"),Index));
        Wall->SetupAttachment(Bed);ConfigureCargoBox(Wall);
        CargoWalls.Add(Wall);
    }
    
    TailgateCollision=CreateDefaultSubobject<UBoxComponent>(TEXT("TailgateCollision"));TailgateCollision->SetupAttachment(TailgatePivot);ConfigureCargoBox(TailgateCollision);
    TailgateClosingArea=CreateDefaultSubobject<UBoxComponent>(TEXT("TailgateClosingArea"));TailgateClosingArea->SetupAttachment(Bed);
    TailgateClosingArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);TailgateClosingArea->SetCanEverAffectNavigation(false);
    HandleMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));HandleMesh->SetupAttachment(RootComponent);
    for (int32 Index=0;Index<4;++Index)
    {
        auto* Wheel=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wheel%d"),Index));Wheel->SetupAttachment(WheelAxle);
        Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);Wheel->SetCanEverAffectNavigation(false);Wheels.Add(Wheel);
    }
    for (auto* Part : {Bed.Get(),HandleMesh.Get()}) { Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCanEverAffectNavigation(false); }
}
void AHandcart::PostLoad()
{
    Super::PostLoad();
    // Older Blueprint defaults can serialize a shorter array than the native component list.
    // Keep their component overrides and append only missing native wall references.
    for (int32 Index=0;;++Index)
    {
        auto* Wall=Cast<UBoxComponent>(GetDefaultSubobjectByName(*FString::Printf(TEXT("CargoWall%d"),Index)));
        if (!Wall) break;
        CargoWalls.AddUnique(Wall);
    }
}

void AHandcart::BeginPlay()
{
    Super::BeginPlay();
    if (!Settings) Settings=GetClass()->GetDefaultObject<AHandcart>()->Settings;
    CargoStorage->OnDetached.AddUObject(this,&AHandcart::HandleCargoDetached);
    CargoStorage->OnAttached.AddUObject(this,&AHandcart::HandleCargoAttached);
    CargoStorage->Configure(Settings,CargoVolume);
    SetActorTickEnabled(false);
    CartMovement->Configure(Settings,Collision,WheelAxle);
    Discharge->Configure(Settings,BedPivot,TailgatePivot,TailgateClosingArea);
    Bed->AttachToComponent(BedPivot,FAttachmentTransformRules::KeepRelativeTransform);
    for (auto* Wheel : {Wheels[0].Get(),Wheels[1].Get(),Wheels[2].Get(),Wheels[3].Get()}) Wheel->AttachToComponent(WheelAxle,FAttachmentTransformRules::KeepRelativeTransform);
    Collision->SetMobility(EComponentMobility::Movable);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Collision->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    GrabTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GrabTarget->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
}
bool AHandcart::SnapToGround()
{
    if (!Settings) Settings=GetClass()->GetDefaultObject<AHandcart>()->Settings;
    CartMovement->Configure(Settings,Collision,WheelAxle);
    return CartMovement->SnapToGround(Driver);
}
bool AHandcart::StartPush(APawn* Player)
{
    if (!Player || Driver || FVector::DistSquared2D(Player->GetActorLocation(),GetActorLocation())>FMath::Square(450.f)) return false;
    Driver=Player;CartMovement->BeginPush(Player);SetActorTickEnabled(true);
    return true;
}
void AHandcart::StopPush()
{
    APawn* Previous=Driver;
    CartMovement->EndPush(Driver);
    Driver=nullptr;SetActorTickEnabled(Discharge->IsActive());
    Discharge->RequestClose();
    if (IsValid(Previous) && !Previous->IsActorBeingDestroyed() && !GetWorld()->bIsTearingDown)
        if (auto* Loadout=Previous->FindComponentByClass<UPlayerLoadoutComponent>()) Loadout->RefreshWeapon();
}
int32 AHandcart::CargoCount() const { return CargoStorage->CargoCount(); }
bool AHandcart::LoadCorpse(ABorderZombie* Corpse,int32 Slot)
{
    if (!IsValid(Corpse) || !Corpse->bDead || Corpse->ProcessingOwner.IsValid()) return false;
    if (Corpse->Carrier.IsValid()) Corpse->Carrier->DetachCorpse(Corpse);
    CargoStorage->Configure(Settings,CargoVolume);
    int32 AssignedSlot=INDEX_NONE;
    if (!CargoStorage->LoadCorpse(Corpse,Slot,AssignedSlot)) return false;
    HandleCargoAttached(Corpse);return true;
}
void AHandcart::RefreshCargo() { CargoStorage->RefreshTracking(); }
void AHandcart::HandleCargoAttached(ABorderZombie* Corpse)
{
    if (Corpse) { Corpse->Carrier=this;Corpse->TransportSlot=INDEX_NONE; }
}
void AHandcart::HandleCargoDetached(ABorderZombie* Corpse)
{
    if (Corpse && Corpse->Carrier.Get()==this) { Corpse->Carrier.Reset();Corpse->TransportSlot=INDEX_NONE; }
}
void AHandcart::DetachCorpse(ABorderZombie* Corpse)
{
    CargoStorage->DetachCorpse(Corpse);HandleCargoDetached(Corpse);
}
void AHandcart::ReleaseCargo() { StopPush();CargoStorage->ReleaseCargo();Discharge->Reset();SetActorTickEnabled(false); }
bool AHandcart::Dump()
{
    if (!Driver) return false;
    if (Discharge->IsActive()) { Discharge->RequestClose();return true; }
    CartMovement->ResetMotion();Discharge->Start();SetActorTickEnabled(true);return true;
}

void AHandcart::Tick(float Dt)
{
    Super::Tick(Dt);

    const auto* Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>();

    // Driver 상태 확인
    if (Driver)
    {
        const bool bInvalidState =
            Driver->ActorHasTag(TEXT("Dead")) ||
            (Mode && (
                Mode->bGameOver ||
                Mode->bTransitioning ||
                Mode->bIsNight
            ));

        const bool bTooFar =
            FVector::DistSquared2D(
                Driver->GetActorLocation(),
                GetActorLocation()
            ) > FMath::Square(450.f);

        if (bInvalidState || bTooFar)
        {
            StopPush();
        }
    }

    if (Driver && !Discharge->IsActive()) AdvanceDriverMovement(Dt);
    Discharge->Advance(Dt);
    UpdateWheelVisuals();
    if (!Driver && !Discharge->IsActive()) SetActorTickEnabled(false);
}

void AHandcart::EndPlay(const EEndPlayReason::Type Reason) { ReleaseCargo();CargoStorage->OnDetached.RemoveAll(this);CargoStorage->OnAttached.RemoveAll(this);Super::EndPlay(Reason); }

void AHandcart::AdvanceDriverMovement(float Dt) { CartMovement->Advance(Dt, Driver); }
void AHandcart::UpdateWheelVisuals() { CartMovement->UpdateWheelRoll(); }
