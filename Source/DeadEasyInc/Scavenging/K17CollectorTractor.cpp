#include "Scavenging/K17CollectorTractor.h"
#include "Scavenging/EquipmentMountComponent.h"
#include "Scavenging/TractorIntakeComponent.h"
#include "Core/BorderGameMode.h"
#include "Progression/PlayerLoadoutComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Math/UnrealMathUtility.h"

AK17CollectorTractor::AK17CollectorTractor()
{
    PrimaryActorTick.bCanEverTick = true;
    TractorIntakeComponent=CreateDefaultSubobject<UTractorIntakeComponent>(TEXT("TractorIntakeComponent"));
    MountState=CreateDefaultSubobject<UEquipmentMountComponent>(TEXT("MountState"));

    // The root is the only WORLD-blocking component. Meshes must not block movement.
    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->InitBoxExtent(FVector(200.f, 90.f, 60.f));
    Collision->SetCollisionProfileName(TEXT("BlockAll"));
    Collision->SetSimulatePhysics(false);
    Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Ignore);
    Collision->SetGenerateOverlapEvents(false);
    Collision->SetMobility(EComponentMobility::Movable);

    CameraBoom=CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(Collision);
    CameraBoom->TargetArmLength=550.f;
    CameraBoom->SetRelativeLocation(FVector(0.f,0.f,180.f));
    CameraBoom->SetRelativeRotation(FRotator(-20.f,0.f,0.f));
    CameraBoom->bDoCollisionTest=true;
    CameraBoom->bEnableCameraLag=true;
    CameraBoom->CameraLagSpeed=8.f;
    CameraBoom->bUsePawnControlRotation=false;
    CameraBoom->SetAutoActivate(false);
    CameraBoom->SetComponentTickEnabled(false);
    CameraBoom->AddTickPrerequisiteActor(this);

    ThirdPersonCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
    ThirdPersonCamera->SetupAttachment(CameraBoom,USpringArmComponent::SocketName);
    ThirdPersonCamera->bUsePawnControlRotation=false;
    ThirdPersonCamera->SetAutoActivate(false);

    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(Collision);
    Body->SetRelativeLocation(FVector(0.f, 0.f, -70.f));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    auto ConfigureWheel = [this](UStaticMeshComponent* Wheel, const FVector& Position)
    {
        Wheel->SetupAttachment(Collision);
        Wheel->SetRelativeLocation(Position);
        Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Wheel->SetCanEverAffectNavigation(false);
    };

    WheelFL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelFL"));
    WheelFR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelFR"));
    WheelRL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelRL"));
    WheelRR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelRR"));
    ConfigureWheel(WheelFL, FVector(+106.f, +94.f, -18.f));
    ConfigureWheel(WheelFR, FVector(+106.f, -94.f, -18.f));
    ConfigureWheel(WheelRL, FVector(-112.f, +94.f, -18.f));
    ConfigureWheel(WheelRR, FVector(-112.f, -94.f, -18.f));

    // From model README: world-local front flap hinge is (+229, 0, +105).
    FrontFlapPivot = CreateDefaultSubobject<USceneComponent>(TEXT("FrontFlapPivot"));
    FrontFlapPivot->SetupAttachment(Collision);
    FrontFlapPivot->SetRelativeLocation(FVector(+229.f, 0.f, +35.f));

    FrontFlap = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontFlap"));
    FrontFlap->SetupAttachment(FrontFlapPivot);
    FrontFlap->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Character root/capsule center attaches here; adjust Z for your Character capsule.
    StandingPosition = CreateDefaultSubobject<UArrowComponent>(TEXT("StandingPosition"));
    StandingPosition->SetupAttachment(Collision);
    StandingPosition->SetRelativeLocation(FVector(-205.f, 0.f, +60.f));
    StandingPosition->ArrowColor = FColor::Yellow;
    StandingPosition->ArrowSize = 1.7f;

    InteractionArea=CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionArea"));
    InteractionArea->SetupAttachment(Collision);
    InteractionArea->SetRelativeLocation(FVector(-240.f,0.f,60.f));
    InteractionArea->InitBoxExtent(FVector(65.f,80.f,65.f));
    InteractionArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionArea->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionArea->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    InteractionArea->SetGenerateOverlapEvents(false);
    InteractionArea->SetCanEverAffectNavigation(false);

    IntakeArea = CreateDefaultSubobject<UBoxComponent>(TEXT("IntakeArea"));
    IntakeArea->SetupAttachment(Collision);
    IntakeArea->SetRelativeLocation(FVector(+485.f, 0.f, -20.f));
    IntakeArea->InitBoxExtent(FVector(250.f, 150.f, 100.f));
    IntakeArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    IntakeArea->SetCollisionResponseToAllChannels(ECR_Ignore);
    IntakeArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    IntakeArea->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    IntakeArea->SetGenerateOverlapEvents(true);
    IntakeArea->SetCanEverAffectNavigation(false);
    IntakeTarget=CreateDefaultSubobject<USceneComponent>(TEXT("IntakeTarget"));
    IntakeTarget->SetupAttachment(Collision);IntakeTarget->SetRelativeLocation(FVector(235.f,0.f,-20.f));
    CargoStoragePoint=CreateDefaultSubobject<USceneComponent>(TEXT("CargoStoragePoint"));
    CargoStoragePoint->SetupAttachment(Collision);CargoStoragePoint->SetRelativeLocation(FVector(0.f,0.f,40.f));
    CageRoot=CargoStoragePoint;
    CageMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CageMesh"));
    CageMesh->SetupAttachment(CageRoot);
    CageMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CageMesh->SetCanEverAffectNavigation(false);
    CageCargoArea=CreateDefaultSubobject<UBoxComponent>(TEXT("CageCargoArea"));
    CageCargoArea->SetupAttachment(CageRoot);
    CageCargoArea->SetRelativeLocation(FVector(0.f,0.f,110.f));
    CageCargoArea->InitBoxExtent(FVector(160.f,95.f,105.f));
    CageCargoArea->SetCollisionObjectType(ECC_GameTraceChannel5);
    CageCargoArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CageCargoArea->SetCollisionResponseToAllChannels(ECR_Ignore);
    CageCargoArea->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Overlap);
    CageCargoArea->SetGenerateOverlapEvents(false);
    CageCargoArea->SetCanEverAffectNavigation(false);
    CageSpawnPoint=CreateDefaultSubobject<USceneComponent>(TEXT("CageSpawnPoint"));
    CageSpawnPoint->SetupAttachment(CageRoot);
    CageSpawnPoint->SetRelativeLocation(FVector(0.f,0.f,170.f));

    auto ConfigureCageBox=[this](UBoxComponent* Box,const FVector& Position,const FVector& Extent)
    {
        Box->SetupAttachment(CageRoot);
        Box->SetMobility(EComponentMobility::Movable);
        Box->SetRelativeLocation(Position);Box->InitBoxExtent(Extent);
        Box->SetCollisionObjectType(ECC_GameTraceChannel5);
        Box->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Block);
        Box->SetGenerateOverlapEvents(false);Box->SetCanEverAffectNavigation(false);
    };
    CageFloor=CreateDefaultSubobject<UBoxComponent>(TEXT("CageFloor"));
    ConfigureCageBox(CageFloor,FVector::ZeroVector,FVector(160.f,95.f,8.f));
    CageWallFront=CreateDefaultSubobject<UBoxComponent>(TEXT("CageWallFront"));
    CageWallBack=CreateDefaultSubobject<UBoxComponent>(TEXT("CageWallBack"));
    CageWallLeft=CreateDefaultSubobject<UBoxComponent>(TEXT("CageWallLeft"));
    CageWallRight=CreateDefaultSubobject<UBoxComponent>(TEXT("CageWallRight"));
    ConfigureCageBox(CageWallFront,FVector(168.f,0.f,100.f),FVector(8.f,103.f,100.f));
    ConfigureCageBox(CageWallBack,FVector(-168.f,0.f,100.f),FVector(8.f,103.f,100.f));
    ConfigureCageBox(CageWallLeft,FVector(0.f,103.f,100.f),FVector(160.f,8.f,100.f));
    ConfigureCageBox(CageWallRight,FVector(0.f,-103.f,100.f),FVector(160.f,8.f,100.f));
    DischargePoint=CreateDefaultSubobject<USceneComponent>(TEXT("DischargePoint"));
    DischargePoint->SetupAttachment(Collision);DischargePoint->SetRelativeLocation(FVector(320.f,0.f,100.f));
}

void AK17CollectorTractor::BeginPlay()
{
    Super::BeginPlay();
    Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel3,ECR_Ignore);
    CageRoot=CargoStoragePoint;
    TractorIntakeComponent->Configure(IntakeArea,IntakeTarget,CageSpawnPoint,CageCargoArea,DischargePoint,IntakePollInterval);
    // Don't auto-snap here: the level may deliberately position the machine above terrain.
}

void AK17CollectorTractor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    EndUse(Driver.Get(), true);
    RestoreMountedCamera(true);
    Super::EndPlay(EndPlayReason);
}

void AK17CollectorTractor::SetDriveInput(float Forward, float Right)
{
    if (!IsMounted()) return;
    ForwardInput = FMath::Clamp(Forward, -1.f, 1.f);
    RightInput = FMath::Clamp(Right, -1.f, 1.f);
    LastInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void AK17CollectorTractor::ClearDriveInput()
{
    ForwardInput = 0.f;
    RightInput = 0.f;
}

bool AK17CollectorTractor::CanInteract(APawn* User) const
{
    return IsValid(User) && !User->ActorHasTag(TEXT("Dead")) && !IsMounted() &&
        FVector::DistSquared(User->GetActorLocation(),InteractionArea->GetComponentLocation()) <= FMath::Square(325.f);
}
bool AK17CollectorTractor::BeginUse(APawn* User) { return Mount(Cast<ACharacter>(User)); }
bool AK17CollectorTractor::IsUsedBy(const APawn* User) const { return User && Driver.Get()==User; }
bool AK17CollectorTractor::ExecutePrimaryAction()
{
    if (!IsMounted() || FMath::Abs(CurrentSpeed)>3.f || !TractorIntakeComponent->BeginDischarge()) return false;
    OnFrontDischargeRequested();return true;
}
void AK17CollectorTractor::ToggleIntake()
{
    if (!IsMounted() || TractorIntakeComponent->IsDischarging()) return;
    bIntakeEnabled=!bIntakeEnabled;
    TractorIntakeComponent->SetIntakeEnabled(bIntakeEnabled);
}
int32 AK17CollectorTractor::GetCargoCount() const { return TractorIntakeComponent->GetCargoCount(); }
FString AK17CollectorTractor::GetUseHint() const
{
    return FString::Printf(TEXT("K-17 | 적재 %d/%d | 흡입 %s | 좌클릭: 흡입 전환 | 우클릭: 전방 배출 | F: 하차"),
        GetCargoCount(),TractorIntakeComponent->MaxCargo,TractorIntakeComponent->IsDischarging() ? TEXT("배출 중") :
        (TractorIntakeComponent->IsIntakeEnabled() ? TEXT("ON") : TEXT("OFF")));
}
bool AK17CollectorTractor::Mount(ACharacter* Character)
{
    if (!CanInteract(Character) || !MountState->Mount(Character,StandingPosition)) return false;
    Driver=Character;
    ClearDriveInput();CurrentSpeed=0.f;
    BeginMountedCamera(Character);
    return true;
}

bool AK17CollectorTractor::TraceGroundAt(const FVector& At, FHitResult& OutHit) const
{
    if (!GetWorld()) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(K17Ground), false, this);
    if (Driver.IsValid()) Query.AddIgnoredActor(Driver.Get());
    return GetWorld()->LineTraceSingleByObjectType(
        OutHit, At + FVector(0.f, 0.f, 300.f), At - FVector(0.f, 0.f, 500.f),
        FCollisionObjectQueryParams(ECC_WorldStatic), Query);
}

bool AK17CollectorTractor::SnapToGround()
{
    FHitResult Ground;
    if (!TraceGroundAt(GetActorLocation(), Ground) || Ground.ImpactNormal.Z < MinGroundNormalZ)
        return false;
    FVector Location = GetActorLocation();
    Location.Z = Ground.ImpactPoint.Z + Collision->GetScaledBoxExtent().Z + GroundClearance;
    SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    return true;
}

bool AK17CollectorTractor::Dismount() { return EndUse(Driver.Get()); }

bool AK17CollectorTractor::EndUse(APawn* User, bool bForce)
{
    if (Driver.IsValid() && Driver.Get()!=User) return false;
    ACharacter* Previous=Driver.Get();
    if (!MountState->Dismount(bForce)) return false;
    RestoreMountedCamera(bForce);
    bIntakeEnabled=false;TractorIntakeComponent->OnUseEnded();
    Driver.Reset();ClearDriveInput();CurrentSpeed=0.f;
    if (IsValid(Previous) && !Previous->IsActorBeingDestroyed() && GetWorld() && !GetWorld()->bIsTearingDown)
        if (auto* Loadout=Previous->FindComponentByClass<UPlayerLoadoutComponent>()) Loadout->RefreshWeapon();
    return true;
}

void AK17CollectorTractor::BeginMountedCamera(ACharacter* Character)
{
    auto* Player=Cast<APlayerController>(Character->GetController());
    if (!Player || !Player->IsLocalController() || !CameraBoom || !ThirdPersonCamera) return;
    CameraController=Player;
    CameraCharacter=Character;
    PreviousControlRotation=Player->GetControlRotation();
    PreviousBoomRotation=CameraBoom->GetRelativeRotation();
    bPreviousUseControllerYaw=Character->bUseControllerRotationYaw;
    bPreviousUseControllerPitch=Character->bUseControllerRotationPitch;
    bPreviousUseControllerRoll=Character->bUseControllerRotationRoll;
    // Look input still updates ControlRotation, but must not rotate the attached Character.
    Character->bUseControllerRotationYaw=false;
    Character->bUseControllerRotationPitch=false;
    Character->bUseControllerRotationRoll=false;
    Player->SetControlRotation(CameraBoom->GetComponentRotation());
    CameraBoom->Activate(true);
    ThirdPersonCamera->Activate();
    // The Actor is the view target. Lock the outgoing POV while changing ControlRotation.
    Player->SetViewTargetWithBlend(this,CameraBlendTime,VTBlend_Cubic,0.f,true);
}

void AK17CollectorTractor::RestoreMountedCamera(bool bImmediate)
{
    ACharacter* Character=CameraCharacter.Get();
    if (Character)
    {
        Character->bUseControllerRotationYaw=bPreviousUseControllerYaw;
        Character->bUseControllerRotationPitch=bPreviousUseControllerPitch;
        Character->bUseControllerRotationRoll=bPreviousUseControllerRoll;
    }
    if (APlayerController* Player=CameraController.Get())
    {
        // If possession changed (death/respawn), never send the new pawn back to the old Character.
        APawn* CurrentPawn=Player->GetPawn();
        if (CurrentPawn==Character && IsValid(Character)) Player->SetControlRotation(PreviousControlRotation);
        AActor* Target=IsValid(CurrentPawn) ? static_cast<AActor*>(CurrentPawn) : static_cast<AActor*>(Player);
        Player->SetViewTargetWithBlend(Target,bImmediate ? 0.f : CameraBlendTime,VTBlend_Cubic,0.f,true);
    }
    if (!CameraController.IsExplicitlyNull() || !CameraCharacter.IsExplicitlyNull())
    {
        CameraBoom->SetRelativeRotation(PreviousBoomRotation);
        CameraBoom->Deactivate();
        ThirdPersonCamera->Deactivate();
    }
    CameraController.Reset();CameraCharacter.Reset();
}

void AK17CollectorTractor::UpdateMountedCamera(float PreviousVehicleYaw)
{
    APlayerController* Player=CameraController.Get();
    if (!Player || !Driver.IsValid()) return;
    FRotator Look=Player->GetControlRotation();
    // Preserve mouse orbit relative to the chassis as the vehicle turns.
    Look.Yaw=FRotator::NormalizeAxis(Look.Yaw+FMath::FindDeltaAngleDegrees(PreviousVehicleYaw,GetActorRotation().Yaw));
    Player->SetControlRotation(Look);
    CameraBoom->SetWorldRotation(Look);
}

void AK17CollectorTractor::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    if (ThirdPersonCamera && ThirdPersonCamera->IsActive())
        ThirdPersonCamera->GetCameraView(DeltaTime,OutResult);
    else Super::CalcCamera(DeltaTime,OutResult);
}

void AK17CollectorTractor::AnimateWheels(const FVector& OldLocation, float OldYaw, float NewYaw)
{
    const float ForwardDist = FVector::DotProduct(GetActorLocation() - OldLocation, GetActorForwardVector());
    const float YawRadians = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(OldYaw, NewYaw));
    const float HalfTrackCm = 94.f;
    const float LeftDegrees = FMath::RadiansToDegrees((ForwardDist - YawRadians * HalfTrackCm) / WheelRadiusCm);
    const float RightDegrees = FMath::RadiansToDegrees((ForwardDist + YawRadians * HalfTrackCm) / WheelRadiusCm);
    WheelFL->AddLocalRotation(FRotator(-LeftDegrees, 0.f, 0.f));
    WheelRL->AddLocalRotation(FRotator(-LeftDegrees, 0.f, 0.f));
    WheelFR->AddLocalRotation(FRotator(-RightDegrees, 0.f, 0.f));
    WheelRR->AddLocalRotation(FRotator(-RightDegrees, 0.f, 0.f));
}

void AK17CollectorTractor::AdvanceMovement(float DeltaTime)
{
    if (!Driver.IsValid()) return;
    if (TractorIntakeComponent->IsDischarging()) { ClearDriveInput();CurrentSpeed=0.f;return; }
    // When Enhanced Input stops sending Triggered (key released), neutralize input.
    if (GetWorld()->GetTimeSeconds() - LastInputTime > 0.15f)
        ClearDriveInput();

    const float TargetSpeed = ForwardInput >= 0.f
        ? ForwardInput * MaxForwardSpeed : ForwardInput * MaxReverseSpeed;
    const float SpeedRate = FMath::IsNearlyZero(ForwardInput) || FMath::Abs(TargetSpeed) < FMath::Abs(CurrentSpeed)
        ? Deceleration : Acceleration;
    CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime, SpeedRate);
    if (FMath::IsNearlyZero(CurrentSpeed, 0.01f) && FMath::IsNearlyZero(RightInput, 0.01f)) return;

    const FVector OldLocation = GetActorLocation();
    const float OldYaw = GetActorRotation().Yaw;
    const float TargetYaw = OldYaw + RightInput * TurnSpeedDegrees * DeltaTime;
    const FRotator NewRotation(0.f, TargetYaw, 0.f);
    FVector Destination = OldLocation + NewRotation.Vector() * CurrentSpeed * DeltaTime;

    // Sample ground at center/front/rear so the large box is not buried on mild slopes.
    FHitResult Center, Front, Rear;
    const bool bCenter = TraceGroundAt(Destination, Center);
    const bool bFront = TraceGroundAt(Destination + NewRotation.Vector() * 150.f, Front);
    const bool bRear = TraceGroundAt(Destination - NewRotation.Vector() * 150.f, Rear);
    if (!bCenter || Center.ImpactNormal.Z < MinGroundNormalZ)
    {
        CurrentSpeed = 0.f;
        return;
    }
    float HighestGroundZ = Center.ImpactPoint.Z;
    if (bFront && Front.ImpactNormal.Z >= MinGroundNormalZ)
        HighestGroundZ = FMath::Max(HighestGroundZ, Front.ImpactPoint.Z);
    if (bRear && Rear.ImpactNormal.Z >= MinGroundNormalZ)
        HighestGroundZ = FMath::Max(HighestGroundZ, Rear.ImpactPoint.Z);
    Destination.Z = HighestGroundZ + Collision->GetScaledBoxExtent().Z + GroundClearance;

    // Root is a BoxCollision. Sweep against obstacles rather than blocking on every ground contact.
    FHitResult Hit;
    SetActorLocationAndRotation(Destination, NewRotation, true, &Hit, ETeleportType::None);
    if (Hit.bBlockingHit) CurrentSpeed = 0.f;

    AnimateWheels(OldLocation, OldYaw, GetActorRotation().Yaw);
}

void AK17CollectorTractor::SetFrontFlapOpen(bool bOpen)
{
    bFrontFlapOpen = bOpen;
}

void AK17CollectorTractor::AdvanceFlap(float DeltaTime)
{
    const float TargetPitch = bFrontFlapOpen ? FlapOpenPitch : 0.f;
    const float Pitch = FMath::FInterpTo(FrontFlapPivot->GetRelativeRotation().Pitch,
                                        TargetPitch, DeltaTime, FlapInterpSpeed);
    FrontFlapPivot->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
}

void AK17CollectorTractor::FindIntakeCandidates(TArray<AActor*>& OutActors) const
{
    TractorIntakeComponent->FindCandidates(OutActors);
}

void AK17CollectorTractor::PollIntake(float DeltaTime)
{
    TractorIntakeComponent->SetIntakeEnabled(bIntakeEnabled && Driver.IsValid());
}

void AK17CollectorTractor::RequestFrontDischarge()
{
    ExecutePrimaryAction();
}

void AK17CollectorTractor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    const auto* Mode=GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if (Driver.IsValid() && (Driver->ActorHasTag(TEXT("Dead")) ||
        (Mode && (Mode->bGameOver || Mode->bIsNight || Mode->bTransitioning)))) EndUse(Driver.Get(),true);
    if (Driver.IsStale())
    {
        bIntakeEnabled=false;TractorIntakeComponent->OnUseEnded();
        Driver.Reset();
        CurrentSpeed = 0.f;
        ClearDriveInput();
    }
    if ((!CameraController.IsExplicitlyNull() || !CameraCharacter.IsExplicitlyNull()) &&
        (!Driver.IsValid() || Driver.Get()!=CameraCharacter.Get() ||
         Driver->GetController()!=CameraController.Get())) RestoreMountedCamera(true);
    const float PreviousVehicleYaw=GetActorRotation().Yaw;
    AdvanceMovement(DeltaTime);
    UpdateMountedCamera(PreviousVehicleYaw);
    AdvanceFlap(DeltaTime);
    PollIntake(DeltaTime);
}
