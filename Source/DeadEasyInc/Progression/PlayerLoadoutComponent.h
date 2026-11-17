#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Progression/Data/LoadoutData.h"
#include "PlayerLoadoutComponent.generated.h"

class UEnhancedInputComponent;
class UInputAction;
class AProgressionStation;
struct FInputActionValue;

UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class DEADEASYINC_API UPlayerLoadoutComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPlayerLoadoutComponent();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<ULoadoutSettings> Settings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FLoadoutState State;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> FireAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> ReloadAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> RangedAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> MeleeAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> CycleAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> InteractAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> CloseAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> WorkAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> StoreCorpseAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> UpgradeAction;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class ABuilding> UpgradeBuilding;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bMenuOpen = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bReloading = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bNightConfirmationOpen = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Message;
    float LastHitTime = -100.f;
    float LastHeadshotTime=-100.f;
    bool bLastHitWasHeadshot=false;
    FString DamageDebugText;
    float DamageDebugUntil=0.f;
    FString Notice;
    float NoticeUntil=0.f;
    bool bNoticeError=false;
    int32 GainWood=0,GainIron=0,GainFuel=0;
    float GainUntil=0.f;
    FName PendingAction;
    FName PendingId;
    FString PendingLabel;
    TMap<FName,int32> PendingCost;
    void Notify(const FString& Text,bool Error=false);
    void ShowResourceGain(int32 Wood,int32 Iron,int32 Fuel);
    bool ConfirmTransaction();
    void CancelTransaction();
    FString MissingMaterials(const TMap<FName,int32>& Cost) const;
    void BindInput(UEnhancedInputComponent* Input);
    UFUNCTION(BlueprintPure) UWeaponDefinition* GetActiveWeapon() const;
    UFUNCTION(BlueprintPure) int32 GetResource(FName Id) const;
    UFUNCTION(BlueprintCallable) void AddResource(FName Id, int32 Amount);
    bool SpendResource(FName Id, int32 Amount);
    FString ResearchStatus(const UResearchDefinition* Research) const;
    FString CraftStatus(const UWeaponDefinition* Weapon) const;
    UFUNCTION(BlueprintCallable) bool Research(FName Id);
    UFUNCTION(BlueprintCallable) bool Craft(FName Id);
    FString ToolStatus(const UToolDefinition* Tool) const;
    bool CraftTool(FName Id);
    TMap<FName,int32> CraftCost(const TMap<FName,int32>& Base) const;
    void SelectWork();
    void RefreshWeapon() { Equip(); }
    void StoreCorpse();
    void OpenUpgrade();
    bool ConfirmUpgrade();
    ABuilding* FocusedBuilding() const;
    void CaptureWorldSave(ULoadoutSave* Save) const;
    void RestoreWorldSave(const ULoadoutSave* Save);
    AProgressionStation* GetFocusedStation() const;
    AProgressionStation* GetOpenStation() const { return OpenStation.Get(); }
    void Interact();
    class AGateSwitch* GetFocusedGateSwitch() const;
    bool ConfirmNight();
    void CloseMenu();
    void StopActions();
    void RestoreState(const FLoadoutState& Saved);
    bool LoadDawn(int32& Day);
    bool SaveDawn(int32 Day);
    virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UWeaponDefinition* FindWeapon(FName Id) const;
    bool CanAct() const;
    bool CanUseStation(bool bResearch) const;
    bool CanAfford(const TMap<FName,int32>& Cost) const;
    void Pay(const TMap<FName,int32>& Cost);
    void StartFire();
    void StopFire();
    void Fire();
    void Reload();
    void SelectRanged();
    void SelectMelee();
    void Cycle(const FInputActionValue& Value);
    void Equip();
    UPROPERTY() TObjectPtr<AEquippedWeapon> Equipped;
    TWeakObjectPtr<AProgressionStation> OpenStation;
    TWeakObjectPtr<class AGateSwitch> NightSwitch;
    bool bFireHeld = false;
    bool bDryFirePlayed=false;
    float NextAttackTime = 0.f;
    float ReloadEndTime = 0.f;
    bool bExecutingTransaction=false;
    bool QueueTransaction(FName Action,FName Id,const FString& Label,const TMap<FName,int32>& Cost);
};
