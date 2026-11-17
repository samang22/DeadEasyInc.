#include "Scavenging/TrackedCartMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

UTrackedCartMovementComponent::UTrackedCartMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTrackedCartMovementComponent::Configure(UBoxComponent* InRootCollision)
{
    RootCollision = InRootCollision;
}

void UTrackedCartMovementComponent::SetInput(float Forward, float Turn)
{
    ThrottleInput = FMath::Clamp(Forward, -1.f, 1.f);
    TurnInput = FMath::Clamp(Turn, -1.f, 1.f);
}

void UTrackedCartMovementComponent::StopInput()
{
    ThrottleInput = 0.f;
    TurnInput = 0.f;
}

void UTrackedCartMovementComponent::ResetMotion()
{
    StopInput();
    CurrentSpeed = 0.f;
    CurrentYawRate = 0.f;
}

bool UTrackedCartMovementComponent::SnapToGround()
{
    AActor* Vehicle = GetOwner();
    if (!RootCollision || !GetWorld() || !Vehicle) return false;

    FHitResult Ground;
    const FVector Origin = Vehicle->GetActorLocation();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(TrackedGround), false, Vehicle);
    const bool bFound = GetWorld()->LineTraceSingleByObjectType(
        Ground, Origin + FVector(0.f, 0.f, 250.f), Origin - FVector(0.f, 0.f, 1200.f),
        FCollisionObjectQueryParams(ECC_WorldStatic), Params);
    if (!bFound || Ground.ImpactNormal.Z < MinGroundNormalZ) return false;

    const float HalfHeight = RootCollision->GetScaledBoxExtent().Z;
    return Vehicle->SetActorLocation(FVector(Origin.X, Origin.Y, Ground.ImpactPoint.Z + HalfHeight + FMath::Max(GroundClearance, 1.f)), false);
}

void UTrackedCartMovementComponent::Advance(float Dt, const AActor* Driver)
{
    AActor* Vehicle = GetOwner();
    if (!Vehicle || !RootCollision || !GetWorld() || Dt <= 0.f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TrackedCart] Invalid State: Vehicle=%d Collision=%d World=%d Dt=%.4f"),
            Vehicle != nullptr,
            RootCollision != nullptr,
            GetWorld() != nullptr,
            Dt);
        return;
    }

    // 로그 출력 주기 제한 (0.5초)
    static float LastDebugTime = -100.f;
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    const bool bDebugLog = CurrentTime - LastDebugTime >= 0.5f;

    if (bDebugLog)
    {
        LastDebugTime = CurrentTime;

        UE_LOG(LogTemp, Warning,
            TEXT("[TrackedCart] INPUT: Throttle=%.2f Turn=%.2f Dt=%.4f"),
            ThrottleInput,
            TurnInput,
            Dt);
    }

    // 1. 속도 계산
    const float DesiredSpeed = ThrottleInput >= 0.f
        ? ThrottleInput * MaxForwardSpeed
        : ThrottleInput * MaxReverseSpeed;

    const bool bBraking =
        FMath::IsNearlyZero(ThrottleInput) ||
        (CurrentSpeed * DesiredSpeed < 0.f) ||
        (FMath::Abs(DesiredSpeed) < FMath::Abs(CurrentSpeed));

    CurrentSpeed = FMath::FInterpConstantTo(
        CurrentSpeed,
        DesiredSpeed,
        Dt,
        bBraking ? BrakeDeceleration : Acceleration
    );

    // 2. 회전 계산
    const float TurnLimit =
        FMath::Abs(CurrentSpeed) < 15.f
        ? PivotTurnSpeedDegrees
        : TurnSpeedDegrees;

    const float TargetYawRate = TurnInput * TurnLimit;

    CurrentYawRate = FMath::FInterpConstantTo(
        CurrentYawRate,
        TargetYawRate,
        Dt,
        TurnAcceleration
    );

    if (bDebugLog)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TrackedCart] MOTION: DesiredSpeed=%.2f CurrentSpeed=%.2f YawRate=%.2f"),
            DesiredSpeed,
            CurrentSpeed,
            CurrentYawRate);
    }

    if (FMath::IsNearlyZero(CurrentSpeed, 0.01f) &&
        FMath::IsNearlyZero(CurrentYawRate, 0.01f))
    {
        if (bDebugLog)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[TrackedCart] STOP: Speed and YawRate are zero"));
        }
        return;
    }

    // 3. 목표 위치 및 회전 계산
    const FRotator Rotation(
        0.f,
        Vehicle->GetActorRotation().Yaw + CurrentYawRate * Dt,
        0.f
    );

    const FVector Start = Vehicle->GetActorLocation();

    FVector Destination =
        Start + Rotation.Vector() * CurrentSpeed * Dt;

    if (bDebugLog)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TrackedCart] POSITION: Start=%s Destination=%s"),
            *Start.ToString(),
            *Destination.ToString());
    }

    // 4. 충돌 검사 파라미터
    FCollisionQueryParams Params(
        SCENE_QUERY_STAT(TrackedMove),
        false,
        Vehicle
    );

    if (Driver)
    {
        Params.AddIgnoredActor(Driver);
    }

    // 5. 지면 탐색
    FHitResult Ground;

    const FVector TraceStart =
        Destination + FVector(0.f, 0.f, 200.f);

    const FVector TraceEnd =
        Destination - FVector(0.f, 0.f, 350.f);

    const bool bGround = GetWorld()->LineTraceSingleByObjectType(
        Ground,
        TraceStart,
        TraceEnd,
        FCollisionObjectQueryParams(ECC_WorldStatic),
        Params
    );

    if (bDebugLog || !bGround ||
        Ground.ImpactNormal.Z < MinGroundNormalZ)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TrackedCart] GROUND: Hit=%d NormalZ=%.3f Required=%.3f Actor=%s ImpactZ=%.2f"),
            bGround,
            Ground.ImpactNormal.Z,
            MinGroundNormalZ,
            *GetNameSafe(Ground.GetActor()),
            Ground.ImpactPoint.Z);
    }

    if (!bGround || Ground.ImpactNormal.Z < MinGroundNormalZ)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TrackedCart] BLOCKED: Ground check failed"));

        ResetMotion();
        return;
    }

    // 6. 지면 높이 보정
    Destination.Z =
        Ground.ImpactPoint.Z +
        RootCollision->GetScaledBoxExtent().Z +
        FMath::Max(GroundClearance, 1.f); // Keep ground contact out of the blocking overlap query.

    if (bDebugLog)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TrackedCart] HEIGHT: GroundZ=%.2f BoxExtentZ=%.2f Clearance=%.2f FinalZ=%.2f"),
            Ground.ImpactPoint.Z,
            RootCollision->GetScaledBoxExtent().Z,
            GroundClearance,
            Destination.Z);
    }

    // 7. 목적지 충돌 검사
    TArray<FOverlapResult> Overlaps;
    const bool bOverlap = GetWorld()->OverlapMultiByChannel(
        Overlaps,
        Destination,
        Rotation.Quaternion(),
        RootCollision->GetCollisionObjectType(),
        FCollisionShape::MakeBox(
            RootCollision->GetScaledBoxExtent()
        ),
        Params,
        FCollisionResponseParams(
            RootCollision->GetCollisionResponseToChannels()
        )
    );

    if (bDebugLog || bOverlap)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TrackedCart] OVERLAP: Blocked=%d Position=%s"),
            bOverlap,
            *Destination.ToString());
    }

    if (bOverlap)
    {
        for (const FOverlapResult& Result : Overlaps)
        {
            if (!Result.bBlockingHit)
                continue;

            UE_LOG(LogTemp, Warning,
                TEXT("[TrackedCart] Blocking Actor=%s Component=%s"),
                *GetNameSafe(Result.GetActor()),
                *GetNameSafe(Result.GetComponent()));
        }

        ResetMotion();
        return;
    }

    // 8. 실제 이동
    FHitResult Hit;

    const bool bMoved = Vehicle->SetActorLocationAndRotation(
        Destination,
        Rotation,
        true,
        &Hit,
        ETeleportType::None
    );

    if (bDebugLog || Hit.bBlockingHit)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TrackedCart] MOVE: Success=%d BlockingHit=%d HitActor=%s HitComponent=%s ActualPosition=%s"),
            bMoved,
            Hit.bBlockingHit,
            *GetNameSafe(Hit.GetActor()),
            *GetNameSafe(Hit.GetComponent()),
            *Vehicle->GetActorLocation().ToString());
    }

    if (Hit.bBlockingHit)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TrackedCart] BLOCKED: Sweep collision"));

        ResetMotion();
    }
}
