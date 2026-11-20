#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scavenging/UsableEquipment.h"
#include "K17CollectorTractor.generated.h"

class ACharacter;
class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class APlayerController;

/** K-17: +X forward / +Y left / +Z up; single-player kinematic prototype. */
UCLASS(Blueprintable)
class DEADEASYINC_API AK17CollectorTractor : public AActor, public IUsableEquipment
{
    GENERATED_BODY()

public:
    AK17CollectorTractor();
    virtual bool CanInteract(APawn* User) const override;
    virtual bool BeginUse(APawn* User) override;
    virtual bool EndUse(APawn* User, bool bForce = false) override;
    virtual bool IsUsedBy(const APawn* User) const override;
    virtual bool IsDirectlyDriven() const override { return true; }
    virtual bool ExecutePrimaryAction() override;
    virtual void ExecuteSecondaryAction() override { ToggleIntake(); }
    virtual FString GetUseHint() const override;
    UFUNCTION(BlueprintCallable, Category="K17|Intake") void ToggleIntake();
    UFUNCTION(BlueprintPure, Category="K17|Intake") int32 GetCargoCount() const;
    virtual UBoxComponent* GetInteractionArea() const override { return InteractionArea; }
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="K17|Mount")
    TObjectPtr<UBoxComponent> InteractionArea;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="K17|Mount")
    TObjectPtr<class UEquipmentMountComponent> MountState;

    virtual void Tick(float DeltaTime) override;
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Camera")
    TObjectPtr<UCameraComponent> ThirdPersonCamera;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Camera", meta=(ClampMin="0"))
    float CameraBlendTime = 0.3f;

    /** Character stays possessed. Redirect Character::DoMove input to this function. */
    UFUNCTION(BlueprintCallable, Category="K17|Drive")
    virtual void SetDriveInput(float Forward, float Right) override;

    UFUNCTION(BlueprintCallable, Category="K17|Drive")
    void ClearDriveInput();

    UFUNCTION(BlueprintCallable, Category="K17|Mount")
    bool Mount(ACharacter* Character);

    /** Returns false when no unobstructed dismount point is available. */
    UFUNCTION(BlueprintCallable, Category="K17|Mount")
    bool Dismount();

    UFUNCTION(BlueprintPure, Category="K17|Mount")
    bool IsMounted() const { return Driver.IsValid(); }

    UFUNCTION(BlueprintPure, Category="K17|Mount")
    ACharacter* GetDriver() const { return Driver.Get(); }

    /** Set true during forward discharge. This only controls front flap visuals. */
    UFUNCTION(BlueprintCallable, Category="K17|Intake")
    void SetFrontFlapOpen(bool bOpen);

    /** Finds nearby candidate actors in front of the vehicle; gameplay collection is external. */
    UFUNCTION(BlueprintCallable, Category="K17|Intake")
    void FindIntakeCandidates(TArray<AActor*>& OutActors) const;

    /** Notification after native intake reserves a valid corpse. Storage is managed by TractorIntakeComponent. */
    UFUNCTION(BlueprintImplementableEvent, Category="K17|Intake")
    void OnIntakeCandidate(AActor* Candidate);

    /** Notification when native front-only batch discharge starts; useful for additional visual effects. */
    UFUNCTION(BlueprintImplementableEvent, Category="K17|Intake")
    void OnFrontDischargeRequested();

    UFUNCTION(BlueprintCallable, Category="K17|Intake")
    void RequestFrontDischarge();

    UFUNCTION(BlueprintCallable, Category="K17|Placement")
    bool SnapToGround();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Parts")
    TObjectPtr<UBoxComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Parts")
    TObjectPtr<UStaticMeshComponent> Body;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Parts")
    TObjectPtr<UStaticMeshComponent> WheelFL;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Parts")
    TObjectPtr<UStaticMeshComponent> WheelFR;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Parts")
    TObjectPtr<UStaticMeshComponent> WheelRL;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Parts")
    TObjectPtr<UStaticMeshComponent> WheelRR;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Parts")
    TObjectPtr<USceneComponent> FrontFlapPivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Parts")
    TObjectPtr<UStaticMeshComponent> FrontFlap;

    /** Visible placement arrow. Points +X (vehicle forward). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Parts")
    TObjectPtr<UArrowComponent> StandingPosition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Parts")
    TObjectPtr<UBoxComponent> IntakeArea;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Intake") TObjectPtr<USceneComponent> IntakeTarget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Intake") TObjectPtr<USceneComponent> CargoStoragePoint;
    // Alias of the existing CargoStoragePoint subobject; preserve saved BP transforms and references.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="K17|Cage") TObjectPtr<USceneComponent> CageRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Cage") TObjectPtr<UStaticMeshComponent> CageMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Cage") TObjectPtr<UBoxComponent> CageCargoArea;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Cage") TObjectPtr<USceneComponent> CageSpawnPoint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Cage") TObjectPtr<UBoxComponent> CageFloor;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Cage") TObjectPtr<UBoxComponent> CageWallFront;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Cage") TObjectPtr<UBoxComponent> CageWallBack;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Cage") TObjectPtr<UBoxComponent> CageWallLeft;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Cage") TObjectPtr<UBoxComponent> CageWallRight;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Intake") TObjectPtr<USceneComponent> DischargePoint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="K17|Intake") TObjectPtr<class UTractorIntakeComponent> TractorIntakeComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Drive", meta=(ClampMin="0"))
    float MaxForwardSpeed = 440.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Drive", meta=(ClampMin="0"))
    float MaxReverseSpeed = 220.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Drive", meta=(ClampMin="0"))
    float Acceleration = 330.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Drive", meta=(ClampMin="0"))
    float Deceleration = 460.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Drive", meta=(ClampMin="0"))
    float TurnSpeedDegrees = 75.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Drive", meta=(ClampMin="1"))
    float WheelRadiusCm = 54.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Ground")
    float GroundClearance = 12.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Ground", meta=(ClampMin="0", ClampMax="1"))
    float MinGroundNormalZ = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Intake")
    bool bIntakeEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Intake", meta=(ClampMin="0.05"))
    float IntakePollInterval = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Flap")
    float FlapOpenPitch = 65.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="K17|Flap", meta=(ClampMin="1"))
    float FlapInterpSpeed = 4.f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void BeginMountedCamera(ACharacter* Character);
    void RestoreMountedCamera(bool bImmediate);
    void UpdateMountedCamera(float PreviousVehicleYaw);
    void AdvanceMovement(float DeltaTime);
    void AdvanceFlap(float DeltaTime);

    UPROPERTY()
    TObjectPtr<class UK17MovementComponent> DriveMovement;

    UPROPERTY(Transient)
    TWeakObjectPtr<ACharacter> Driver;

    UPROPERTY(Transient) TWeakObjectPtr<APlayerController> CameraController;
    UPROPERTY(Transient) TWeakObjectPtr<ACharacter> CameraCharacter;
    FRotator PreviousControlRotation = FRotator::ZeroRotator;
    FRotator PreviousBoomRotation = FRotator::ZeroRotator;
    bool bPreviousUseControllerYaw = false;
    bool bPreviousUseControllerPitch = false;
    bool bPreviousUseControllerRoll = false;

    bool bFrontFlapOpen = false;
};
