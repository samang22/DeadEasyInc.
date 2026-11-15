#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Scavenging/ScavengingData.h"
#include "CorpseHandlingComponent.generated.h"

UCLASS(ClassGroup=(Player),meta=(BlueprintSpawnableComponent))
class DEADEASYINC_API UCorpseHandlingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCorpseHandlingComponent();
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) TObjectPtr<UScavengingSettings> Settings;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) EWorkTool Tool=EWorkTool::Fork;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bActive=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class ABorderZombie> Held;
    int32 Capacity() const;
    bool CanWork() const;
    void Select();
    void Cycle(float Direction);
    void Primary();
    void Store();
    void Release();
    void SetWorkActive(bool Active);
    void RefreshVisuals();
    class AHandcart* EnsureHandcart();
    bool IsPushing() const;
    class AElectricCart* GetMountedVehicle() const;
    class AHandcart* GetUsableHandcart() const;
    void StopPushing();
    void DetachCorpse(class ABorderZombie* Corpse);
    bool AdoptCorpse(class ABorderZombie* Corpse,int32 Slot=INDEX_NONE);
    void ReleaseCargo();
    int32 CargoCount() const;
    void RecoverLegacyCargo();
    bool Interact();
    FString Hint() const;
    virtual void TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    AActor* Focus(FHitResult& Hit) const;
    class UPlayerLoadoutComponent* Loadout() const;
    UPROPERTY() TObjectPtr<class UPhysicsHandleComponent> Handle;
    UPROPERTY() TObjectPtr<class AWorkEquipment> Equipment;
    float NormalSpeed=500.f;
    float NextUseTime=0.f;
    UPROPERTY() TObjectPtr<class AHandcart> Handcart;
};
