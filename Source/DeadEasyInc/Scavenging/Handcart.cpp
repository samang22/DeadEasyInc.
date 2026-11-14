#include "Scavenging/Handcart.h"
#include "Scavenging/ScavengingData.h"
#include "Scavenging/CorpseProcessor.h"
#include "Zombies/BorderZombie.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Scavenging/CorpseCargoComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Core/BorderGameMode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/OverlapResult.h"

AHandcart::AHandcart()
{
    PrimaryActorTick.bCanEverTick = true;

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
    CargoVolume=CreateDefaultSubobject<UBoxComponent>(TEXT("CargoVolume"));CargoVolume->SetupAttachment(Bed);
    CargoVolume->SetRelativeLocation(FVector(0,-4,72));CargoVolume->SetBoxExtent(FVector(50,78,110));
    CargoVolume->SetCollisionObjectType(ECC_GameTraceChannel5);CargoVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CargoVolume->SetCollisionResponseToAllChannels(ECR_Ignore);CargoVolume->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Overlap);
    CargoVolume->SetGenerateOverlapEvents(true);CargoVolume->SetCanEverAffectNavigation(false);
    CargoFloor=CreateDefaultSubobject<UBoxComponent>(TEXT("CargoFloor"));CargoFloor->SetupAttachment(Bed);ConfigureCargoBox(CargoFloor);
    for (int32 Index=0;Index<3;++Index)
    {
        auto* Wall=CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("CargoWall%d"),Index));Wall->SetupAttachment(Bed);ConfigureCargoBox(Wall);CargoWalls.Add(Wall);
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
void AHandcart::BeginPlay()
{
    Super::BeginPlay();
    if (!Settings) Settings=GetClass()->GetDefaultObject<AHandcart>()->Settings;
    CargoStorage->OnDetached.AddUObject(this,&AHandcart::HandleCargoDetached);
    CargoStorage->OnAttached.AddUObject(this,&AHandcart::HandleCargoAttached);
    CargoStorage->Configure(Settings,CargoVolume);
    SetActorTickEnabled(false);
    ClosedBedRotation=BedPivot->GetRelativeRotation().Quaternion();ClosedGateRotation=TailgatePivot->GetRelativeRotation().Quaternion();
    LastWheelLocation=GetActorLocation();
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
    FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(CartGround),false,this);
    if (Driver) Params.AddIgnoredActor(Driver);
    const FVector Start=GetActorLocation();
    if (!GetWorld()->LineTraceSingleByObjectType(Hit,Start+FVector(0,0,200),Start-FVector(0,0,800),FCollisionObjectQueryParams(ECC_WorldStatic),Params)) return false;
    if (Hit.ImpactNormal.Z<.65f) return false;
    FVector Position=Start;Position.Z=Hit.ImpactPoint.Z+(Settings ? FMath::Max(1.f,Settings->CartWheelRadius) : 51.f)*FMath::Abs(GetActorScale3D().Z);SetActorLocation(Position);LastWheelLocation=Position;return true;
}
bool AHandcart::StartPush(APawn* Player)
{
    if (!Player || Driver || FVector::DistSquared2D(Player->GetActorLocation(),GetActorLocation())>FMath::Square(450.f)) return false;
    Driver=Player;FollowVelocity=FVector::ZeroVector;FollowYawRate=0.f;LastWheelLocation=GetActorLocation();SetActorTickEnabled(true);
    if (auto* Character=Cast<ACharacter>(Player))
    { NormalSpeed=Character->GetCharacterMovement()->MaxWalkSpeed;Character->GetCharacterMovement()->MaxWalkSpeed=FMath::Min(NormalSpeed,Settings ? Settings->CartSpeed : 350.f); }
    return true;
}
void AHandcart::StopPush()
{
    APawn* Previous=Driver;
    if (auto* Character=Cast<ACharacter>(Driver)) Character->GetCharacterMovement()->MaxWalkSpeed=NormalSpeed;
    Driver=nullptr;FollowVelocity=FVector::ZeroVector;FollowYawRate=0.f;SetActorTickEnabled(bDischarging);
    if (bDischarging) bClosingTailgate=true;
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
void AHandcart::ReleaseCargo() { StopPush();CargoStorage->ReleaseCargo();ResetDischarge(); }
bool AHandcart::Dump()
{
    if (!Driver) return false;
    if (bDischarging) { bClosingTailgate=true;return true; }
    FollowVelocity=FVector::ZeroVector;FollowYawRate=0.f;
    bDischarging=true;bClosingTailgate=false;SpillTime=0.f;SetActorTickEnabled(true);return true;
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

    // 손수레 이동
    if (Driver && !bDischarging)
    {
        const float TurnSpeed =
            Settings ? Settings->CartTurnSpeed : 90.f;

        const float MoveSpeed =
            Settings ? Settings->CartSpeed : 350.f;

        const float PushDistance =
            Settings ? Settings->CartPushDistance : 200.f;

        const float WheelRadius =
            Settings
                ? FMath::Max(1.f, Settings->CartWheelRadius)
                : 51.f;

        // Driver 시선 방향
        const FRotator Wanted(
            0.f,
            Driver->GetViewRotation().Yaw,
            0.f
        );

        // 목표 회전
        const float YawDelta=FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,Wanted.Yaw);
        const float TargetYawRate=FMath::Clamp(YawDelta/.18f,-TurnSpeed,TurnSpeed);
        FollowYawRate=FMath::FInterpConstantTo(FollowYawRate,TargetYawRate,Dt,Settings ? Settings->CartTurnAcceleration : 240.f);
        const float YawStep=FMath::Clamp(FollowYawRate*Dt,-FMath::Abs(YawDelta),FMath::Abs(YawDelta));
        const FRotator Rotation(0.f,GetActorRotation().Yaw+YawStep,0.f);

        // 목표 위치
        FVector Desired =
            Driver->GetActorLocation() +
            Wanted.Vector() * PushDistance;

        Desired.Z = GetActorLocation().Z;

        // 충돌 검사 파라미터
        FCollisionQueryParams Params(
            SCENE_QUERY_STAT(CartMove),
            false,
            this
        );

        Params.AddIgnoredActor(Driver);

        // 지면 탐색
        FHitResult Ground;

        const bool bHasGround =
            GetWorld()->LineTraceSingleByObjectType(
                Ground,
                Desired + FVector(0.f, 0.f, 100.f),
                Desired - FVector(0.f, 0.f, 250.f),
                FCollisionObjectQueryParams(ECC_WorldStatic),
                Params
            );

        if (bHasGround && Ground.ImpactNormal.Z > 0.65f)
        {
            // 바퀴 높이 보정
            Desired.Z =
                Ground.ImpactPoint.Z +
                WheelRadius * FMath::Abs(GetActorScale3D().Z);

            // 이동 보간
            const FVector Delta=Desired-GetActorLocation();
            const FVector DesiredVelocity=Delta.GetSafeNormal()*FMath::Min(MoveSpeed,Delta.Size()/.18f);
            const bool bDecelerating=DesiredVelocity.SizeSquared()<FollowVelocity.SizeSquared();
            const float Acceleration=Settings ? (bDecelerating ? Settings->CartDeceleration : Settings->CartAcceleration) : (bDecelerating ? 1200.f : 800.f);
            FollowVelocity=FMath::VInterpConstantTo(FollowVelocity,DesiredVelocity,Dt,FMath::Max(1.f,Acceleration));
            FVector Travel=FollowVelocity*Dt;
            if (Travel.SizeSquared()>Delta.SizeSquared()) Travel=Delta;
            const FVector Step=GetActorLocation()+Travel;

            // 회전된 박스 충돌 영역 검사
            const bool bBlocked =
                GetWorld()->OverlapBlockingTestByChannel(
                    Step,
                    Rotation.Quaternion(),
                    Collision->GetCollisionObjectType(),
                    FCollisionShape::MakeBox(
                        Collision->GetScaledBoxExtent()
                    ),
                    Params,
                    FCollisionResponseParams(
                        Collision->GetCollisionResponseToChannels()
                    )
                );

            if (!bBlocked)
            {
                FHitResult Hit;

                SetActorLocationAndRotation(
                    Step,
                    Rotation,
                    true,
                    &Hit,
                    ETeleportType::None
                );
                if (Hit.bBlockingHit) { FollowVelocity=FVector::ZeroVector;FollowYawRate=0.f; }
            }
            else { FollowVelocity=FVector::ZeroVector;FollowYawRate=0.f; }
        }
        else { FollowVelocity=FVector::ZeroVector;FollowYawRate=0.f; }
    }

    TickDischarge(Dt);
    UpdateWheelRoll();
}

void AHandcart::EndPlay(const EEndPlayReason::Type Reason) { ReleaseCargo();CargoStorage->OnDetached.RemoveAll(this);CargoStorage->OnAttached.RemoveAll(this);Super::EndPlay(Reason); }

void AHandcart::UpdateWheelRoll()
{
    const FVector Delta=GetActorLocation()-LastWheelLocation;LastWheelLocation=GetActorLocation();
    if (Delta.SizeSquared()>FMath::Square(200.f)) return; // Checkpoint teleports are not wheel travel.
    const float Distance=FVector::DotProduct(Delta,GetActorForwardVector());
    const float Radius=FMath::Max(1.f,(Settings ? Settings->CartWheelRadius : 51.f)*FMath::Abs(GetActorScale3D().Z));
    WheelAxle->AddLocalRotation(FQuat(FVector::RightVector,Distance/Radius));
}
