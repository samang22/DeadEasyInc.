#include "Scavenging/HandcartMovementComponent.h"
#include "Scavenging/ScavengingData.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

UHandcartMovementComponent::UHandcartMovementComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UHandcartMovementComponent::Configure(UScavengingSettings* InSettings,UBoxComponent* InCollision,USceneComponent* InWheelAxle)
{
    if (!Collision) LastWheelLocation=GetOwner()->GetActorLocation();
    Settings=InSettings;Collision=InCollision;WheelAxle=InWheelAxle;
}
void UHandcartMovementComponent::ResetMotion() { FollowVelocity=FVector::ZeroVector;FollowYawRate=0.f; }
void UHandcartMovementComponent::BeginPush(APawn* Player)
{
    ResetMotion();LastWheelLocation=GetOwner()->GetActorLocation();
    if (auto* Character=Cast<ACharacter>(Player))
    { NormalSpeed=Character->GetCharacterMovement()->MaxWalkSpeed;Character->GetCharacterMovement()->MaxWalkSpeed=FMath::Min(NormalSpeed,Settings ? Settings->CartSpeed : 350.f); }
}
void UHandcartMovementComponent::EndPush(APawn* Player)
{
    if (auto* Character=Cast<ACharacter>(Player)) Character->GetCharacterMovement()->MaxWalkSpeed=NormalSpeed;
    ResetMotion();
}
bool UHandcartMovementComponent::SnapToGround(APawn* Player)
{
    AActor* Cart=GetOwner();
    FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(CartGround),false,Cart);
    if (Player) Params.AddIgnoredActor(Player);
    const FVector Start=Cart->GetActorLocation();
    if (!GetWorld()->LineTraceSingleByObjectType(Hit,Start+FVector(0,0,200),Start-FVector(0,0,800),FCollisionObjectQueryParams(ECC_WorldStatic),Params)) return false;
    if (Hit.ImpactNormal.Z<.65f) return false;
    FVector Position=Start;Position.Z=Hit.ImpactPoint.Z+(Settings ? FMath::Max(1.f,Settings->CartWheelRadius) : 51.f)*FMath::Abs(Cart->GetActorScale3D().Z);Cart->SetActorLocation(Position);LastWheelLocation=Position;return true;
}
void UHandcartMovementComponent::Advance(float Dt,APawn* Player)
{
    if (!IsValid(Player) || !Collision) return;
    AActor* Cart=GetOwner();
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

    // Player 시선 방향
    const FRotator Wanted(
        0.f,
        Player->GetViewRotation().Yaw,
        0.f
    );

    // 목표 회전
    const float YawDelta=FMath::FindDeltaAngleDegrees(Cart->GetActorRotation().Yaw,Wanted.Yaw);
    const float TargetYawRate=FMath::Clamp(YawDelta/.18f,-TurnSpeed,TurnSpeed);
    FollowYawRate=FMath::FInterpConstantTo(FollowYawRate,TargetYawRate,Dt,Settings ? Settings->CartTurnAcceleration : 240.f);
    const float YawStep=FMath::Clamp(FollowYawRate*Dt,-FMath::Abs(YawDelta),FMath::Abs(YawDelta));
    const FRotator Rotation(0.f,Cart->GetActorRotation().Yaw+YawStep,0.f);

    // 목표 위치
    FVector Desired =
        Player->GetActorLocation() +
        Wanted.Vector() * PushDistance;

    Desired.Z = Cart->GetActorLocation().Z;

    // 충돌 검사 파라미터
    FCollisionQueryParams Params(
        SCENE_QUERY_STAT(CartMove),
        false,
        Cart
    );

    Params.AddIgnoredActor(Player);

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
            WheelRadius * FMath::Abs(Cart->GetActorScale3D().Z);

        // 이동 보간
        const FVector Delta=Desired-Cart->GetActorLocation();
        const FVector DesiredVelocity=Delta.GetSafeNormal()*FMath::Min(MoveSpeed,Delta.Size()/.18f);
        const bool bDecelerating=DesiredVelocity.SizeSquared()<FollowVelocity.SizeSquared();
        const float Acceleration=Settings ? (bDecelerating ? Settings->CartDeceleration : Settings->CartAcceleration) : (bDecelerating ? 1200.f : 800.f);
        FollowVelocity=FMath::VInterpConstantTo(FollowVelocity,DesiredVelocity,Dt,FMath::Max(1.f,Acceleration));
        FVector Travel=FollowVelocity*Dt;
        if (Travel.SizeSquared()>Delta.SizeSquared()) Travel=Delta;
        const FVector Step=Cart->GetActorLocation()+Travel;

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

            Cart->SetActorLocationAndRotation(
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

void UHandcartMovementComponent::UpdateWheelRoll()
{
    AActor* Cart=GetOwner();
    const FVector Delta=Cart->GetActorLocation()-LastWheelLocation;LastWheelLocation=Cart->GetActorLocation();
    if (Delta.SizeSquared()>FMath::Square(200.f)) return; // Checkpoint teleports are not wheel travel.
    const float Distance=FVector::DotProduct(Delta,Cart->GetActorForwardVector());
    const float Radius=FMath::Max(1.f,(Settings ? Settings->CartWheelRadius : 51.f)*FMath::Abs(Cart->GetActorScale3D().Z));
    WheelAxle->AddLocalRotation(FQuat(FVector::RightVector,Distance/Radius));
}
