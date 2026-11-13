#include "Progression/PlayerLoadoutComponent.h"
#include "Interaction/GateSwitch.h"
#include "Buildings/Defense/BorderGate.h"
#include "Buildings/Stations/ProgressionStation.h"
#include "Combat/Weapons/EquippedWeapon.h"
#include "Core/BorderGameMode.h"
#include "Player/DeadEasyIncCharacter.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

UPlayerLoadoutComponent::UPlayerLoadoutComponent() { PrimaryComponentTick.bCanEverTick = true; }

void UPlayerLoadoutComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!Settings) return;
    State.Resources = Settings->StartingResources;
    State.OwnedWeapons = Settings->StartingWeapons;
    for (FName Id : State.OwnedWeapons)
        if (UWeaponDefinition* Weapon = FindWeapon(Id))
        {
            (Weapon->Category == EWeaponCategory::Ranged ? State.Ranged : State.Melee) = Id;
            State.Magazines.Add(Id, FMath::Max(1, Weapon->MagazineSize));
        }
    Equip();
}

void UPlayerLoadoutComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    CloseMenu();
    if (Equipped) Equipped->Destroy();
    Super::EndPlay(Reason);
}

void UPlayerLoadoutComponent::BindInput(UEnhancedInputComponent* Input)
{
    if (FireAction)
    {
        Input->BindAction(FireAction, ETriggerEvent::Started, this, &UPlayerLoadoutComponent::StartFire);
        Input->BindAction(FireAction, ETriggerEvent::Completed, this, &UPlayerLoadoutComponent::StopFire);
        Input->BindAction(FireAction, ETriggerEvent::Canceled, this, &UPlayerLoadoutComponent::StopFire);
    }
    if (ReloadAction) Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &UPlayerLoadoutComponent::Reload);
    if (RangedAction) Input->BindAction(RangedAction, ETriggerEvent::Started, this, &UPlayerLoadoutComponent::SelectRanged);
    if (MeleeAction) Input->BindAction(MeleeAction, ETriggerEvent::Started, this, &UPlayerLoadoutComponent::SelectMelee);
    if (CycleAction) Input->BindAction(CycleAction, ETriggerEvent::Triggered, this, &UPlayerLoadoutComponent::Cycle);
    if (InteractAction) Input->BindAction(InteractAction, ETriggerEvent::Started, this, &UPlayerLoadoutComponent::Interact);
    if (CloseAction) Input->BindAction(CloseAction, ETriggerEvent::Started, this, &UPlayerLoadoutComponent::CloseMenu);
}

UWeaponDefinition* UPlayerLoadoutComponent::FindWeapon(FName Id) const
{
    if (Settings) for (UWeaponDefinition* Weapon : Settings->Weapons) if (Weapon && Weapon->Id == Id) return Weapon;
    return nullptr;
}

UWeaponDefinition* UPlayerLoadoutComponent::GetActiveWeapon() const
{
    const FName Id = State.ActiveCategory == EWeaponCategory::Ranged ? State.Ranged : State.Melee;
    return State.OwnedWeapons.Contains(Id) ? FindWeapon(Id) : nullptr;
}

bool UPlayerLoadoutComponent::CanAct() const
{
    const ABorderGameMode* Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>();
    return !GetOwner()->ActorHasTag(TEXT("Dead")) && (!Mode || (!Mode->bGameOver && !Mode->bTransitioning));
}

int32 UPlayerLoadoutComponent::GetResource(FName Id) const { return State.Resources.FindRef(Id); }
void UPlayerLoadoutComponent::AddResource(FName Id, int32 Amount)
{
    if (Amount > 0 && !Id.IsNone())
        State.Resources.FindOrAdd(Id) = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(GetResource(Id)) + Amount));
}
bool UPlayerLoadoutComponent::SpendResource(FName Id, int32 Amount)
{
    if (Amount < 0 || GetResource(Id) < Amount) return false;
    State.Resources.FindOrAdd(Id) -= Amount;
    return true;
}
bool UPlayerLoadoutComponent::CanAfford(const TMap<FName,int32>& Cost) const
{
    for (const auto& Pair : Cost) if (Pair.Value < 0 || GetResource(Pair.Key) < Pair.Value) return false;
    return true;
}
void UPlayerLoadoutComponent::Pay(const TMap<FName,int32>& Cost)
{
    for (const auto& Pair : Cost) SpendResource(Pair.Key, Pair.Value);
}

FString UPlayerLoadoutComponent::ResearchStatus(const UResearchDefinition* Definition) const
{
    if (!Definition) return TEXT("Unavailable");
    if (State.Research.Contains(Definition->Id)) return TEXT("Researched");
    for (FName Id : Definition->Prerequisites) if (!State.Research.Contains(Id)) return TEXT("Requires prior research");
    return CanAfford(Definition->Cost) ? TEXT("Research") : TEXT("Not enough materials");
}
FString UPlayerLoadoutComponent::CraftStatus(const UWeaponDefinition* Weapon) const
{
    if (!Weapon || !Weapon->WeaponClass) return TEXT("Unavailable");
    if (State.OwnedWeapons.Contains(Weapon->Id)) return TEXT("Owned");
    if (!Weapon->RequiredResearch.IsNone() && !State.Research.Contains(Weapon->RequiredResearch)) return TEXT("Research required");
    return CanAfford(Weapon->CraftCost) ? TEXT("Craft") : TEXT("Not enough materials");
}

bool UPlayerLoadoutComponent::CanUseStation(bool bResearch) const
{
    AProgressionStation* Station = OpenStation.Get();
    const ABorderGameMode* Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>();
    return CanAct() && bMenuOpen && Station && Station == GetFocusedStation() &&
        Station->Condition != EBuildingCondition::Destroyed && (!Mode || !Mode->bIsNight) &&
        Station->StationType == (bResearch ? EStationType::Research : EStationType::Crafting);
}
bool UPlayerLoadoutComponent::Research(FName Id)
{
    if (!Settings || !CanUseStation(true)) return false;
    for (UResearchDefinition* Definition : Settings->Research)
        if (Definition && Definition->Id == Id)
        {
            Message = ResearchStatus(Definition);
            if (Message != TEXT("Research")) return false;
            Pay(Definition->Cost);
            State.Research.AddUnique(Id);
            Message = TEXT("Research unlocked. Committed at next dawn.");
            return true;
        }
    return false;
}
bool UPlayerLoadoutComponent::Craft(FName Id)
{
    UWeaponDefinition* Weapon = FindWeapon(Id);
    if (!CanUseStation(false) || !Weapon) return false;
    Message = CraftStatus(Weapon);
    if (Message != TEXT("Craft")) return false;
    Pay(Weapon->CraftCost);
    State.OwnedWeapons.AddUnique(Id);
    State.Magazines.Add(Id, FMath::Max(1, Weapon->MagazineSize));
    (Weapon->Category == EWeaponCategory::Ranged ? State.Ranged : State.Melee) = Id;
    Message = TEXT("Weapon crafted. Committed at next dawn.");
    return true;
}

AProgressionStation* UPlayerLoadoutComponent::GetFocusedStation() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Player) return nullptr;
    FVector Eye;
    FRotator View;
    Player->GetPlayerViewPoint(Eye, View);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(StationUse), false, Pawn);
    if (Equipped) Params.AddIgnoredActor(Equipped);
    GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + View.Vector() * (Settings ? Settings->InteractionReach : 300.f), ECC_Visibility, Params);
    return Cast<AProgressionStation>(Hit.GetActor());
}

AGateSwitch* UPlayerLoadoutComponent::GetFocusedGateSwitch() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Player) return nullptr;
    FVector Eye; FRotator View;
    Player->GetPlayerViewPoint(Eye,View);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GateSwitchUse),false,Pawn);
    if (Equipped) Params.AddIgnoredActor(Equipped);
    GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*(Settings ? Settings->InteractionReach : 300.f),ECC_Visibility,Params);
    return Cast<AGateSwitch>(Hit.GetActor());
}

bool UPlayerLoadoutComponent::ConfirmNight()
{
    ABorderGameMode* Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if (!bNightConfirmationOpen || !Mode || !NightSwitch.IsValid() || !CanAct()) return false;
    ABorderGate* Gate = NightSwitch->Gate;
    if (!Mode->CanBeginNight(Gate))
    {
        Message = TEXT("출입구의 플레이어·좀비·시체를 치워주세요.");
        return false;
    }
    CloseMenu();
    return Mode->BeginNight(Gate);
}

void UPlayerLoadoutComponent::Interact()
{
    if (bMenuOpen) { CloseMenu(); return; }
    if (!CanAct()) return;
    const ABorderGameMode* Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if (Mode && Mode->bIsNight) { Message = TEXT("Stations are available during the day."); return; }
    AGateSwitch* Switch = GetFocusedGateSwitch();
    AProgressionStation* Station = GetFocusedStation();
    if (!Switch && (!Station || Station->Condition == EBuildingCondition::Destroyed)) return;
    if (Switch && !IsValid(Switch->Gate)) { Message = TEXT("Gate switch is not connected."); return; }
    bNightConfirmationOpen = Switch != nullptr;
    NightSwitch = Switch;
    StopFire();
    bReloading = false;
    OpenStation = Station;
    bMenuOpen = true;
    Message.Reset();
    if (APlayerController* Player = Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController()))
    {
        Player->SetIgnoreMoveInput(true);
        Player->SetIgnoreLookInput(true);
        Player->bShowMouseCursor = true;
        Player->bEnableClickEvents = true;
        FInputModeGameAndUI ModeInput;
        ModeInput.SetHideCursorDuringCapture(false);
        ModeInput.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Player->SetInputMode(ModeInput);
    }
}
void UPlayerLoadoutComponent::CloseMenu()
{
    if (!bMenuOpen) return;
    bMenuOpen = false;
    bNightConfirmationOpen = false;
    NightSwitch.Reset();
    OpenStation.Reset();
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
        if (APlayerController* Player = Cast<APlayerController>(Pawn->GetController()))
        {
            Player->SetIgnoreMoveInput(false);
            Player->SetIgnoreLookInput(false);
            Player->bShowMouseCursor = false;
            Player->bEnableClickEvents = false;
            Player->SetInputMode(FInputModeGameOnly());
        }
    Equip();
}

void UPlayerLoadoutComponent::StartFire() { if (CanAct() && !bMenuOpen) { bFireHeld = true; Fire(); } }
void UPlayerLoadoutComponent::StopFire() { bFireHeld = false; }
void UPlayerLoadoutComponent::StopActions() { StopFire(); bReloading = false; CloseMenu(); }
void UPlayerLoadoutComponent::Fire()
{
    UWeaponDefinition* Weapon = GetActiveWeapon();
    const float Now = GetWorld()->GetTimeSeconds();
    if (!CanAct() || bMenuOpen || !Weapon || !Equipped || bReloading || Now < NextAttackTime) return;
    if (Weapon->Category == EWeaponCategory::Ranged)
    {
        int32& Ammo = State.Magazines.FindOrAdd(Weapon->Id);
        if (Ammo <= 0) { Message = TEXT("Empty magazine - press R"); return; }
        --Ammo;
    }
    if (Equipped->Attack(Weapon)) LastHitTime = Now;
    NextAttackTime = Now + FMath::Max(.05f, Weapon->AttackInterval);
}
void UPlayerLoadoutComponent::Reload()
{
    UWeaponDefinition* Weapon = GetActiveWeapon();
    if (!CanAct() || bMenuOpen || bReloading || !Weapon || Weapon->Category != EWeaponCategory::Ranged ||
        State.Magazines.FindRef(Weapon->Id) >= Weapon->MagazineSize) return;
    bReloading = true;
    ReloadEndTime = GetWorld()->GetTimeSeconds() + FMath::Max(.1f, Weapon->ReloadTime);
    if (Equipped) Equipped->PlayReload();
}
void UPlayerLoadoutComponent::SelectRanged()
{
    if (!CanAct() || bMenuOpen) return;
    State.ActiveCategory = EWeaponCategory::Ranged;
    Equip();
}
void UPlayerLoadoutComponent::SelectMelee()
{
    if (!CanAct() || bMenuOpen) return;
    State.ActiveCategory = EWeaponCategory::Melee;
    Equip();
}
void UPlayerLoadoutComponent::Cycle(const FInputActionValue& Value)
{
    if (!CanAct() || bMenuOpen || !Settings || FMath::IsNearlyZero(Value.Get<float>())) return;
    TArray<FName> Choices;
    for (UWeaponDefinition* Weapon : Settings->Weapons)
        if (Weapon && Weapon->Category == State.ActiveCategory && State.OwnedWeapons.Contains(Weapon->Id)) Choices.Add(Weapon->Id);
    if (Choices.IsEmpty()) return;
    FName& Selected = State.ActiveCategory == EWeaponCategory::Ranged ? State.Ranged : State.Melee;
    const int32 Index = Choices.IndexOfByKey(Selected);
    Selected = Choices[Index == INDEX_NONE ? 0 : (Index + (Value.Get<float>() > 0 ? 1 : Choices.Num()-1)) % Choices.Num()];
    Equip();
}
void UPlayerLoadoutComponent::Equip()
{
    StopFire();
    bReloading = false;
    if (Equipped) { Equipped->Destroy(); Equipped = nullptr; }
    UWeaponDefinition* Weapon = GetActiveWeapon();
    ADeadEasyIncCharacter* Character = Cast<ADeadEasyIncCharacter>(GetOwner());
    if (!Character || !Weapon || !Weapon->WeaponClass) return;
    FActorSpawnParameters Params;
    Params.Owner = Character;
    Params.Instigator = Character;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Equipped = GetWorld()->SpawnActor<AEquippedWeapon>(Weapon->WeaponClass, FTransform::Identity, Params);
    if (!Equipped) return;
    Equipped->AttachToComponent(Character->GetFirstPersonCameraComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    Equipped->SetActorRelativeTransform(Equipped->ViewTransform);
}

void UPlayerLoadoutComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
    const ABorderGameMode* Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if (bMenuOpen)
    {
        const bool bInvalid = bNightConfirmationOpen ? !NightSwitch.IsValid() || GetFocusedGateSwitch()!=NightSwitch.Get() :
            !OpenStation.IsValid() || OpenStation->Condition==EBuildingCondition::Destroyed || GetFocusedStation()!=OpenStation.Get();
        if (!CanAct() || bInvalid || (Mode && Mode->bIsNight)) CloseMenu();
    }
    if (!CanAct()) { StopFire(); bReloading = false; return; }
    UWeaponDefinition* Weapon = GetActiveWeapon();
    if (bReloading && Weapon && GetWorld()->GetTimeSeconds() >= ReloadEndTime)
    {
        State.Magazines.Add(Weapon->Id, FMath::Max(1,Weapon->MagazineSize));
        bReloading = false;
    }
    if (bFireHeld && Weapon && Weapon->bAutomatic) Fire();
}

void UPlayerLoadoutComponent::RestoreState(const FLoadoutState& Saved)
{
    StopActions();
    State = Saved;
    for (FName Id : State.OwnedWeapons)
        if (UWeaponDefinition* Weapon = FindWeapon(Id))
            State.Magazines.FindOrAdd(Id) = FMath::Clamp(State.Magazines.FindRef(Id),0,FMath::Max(1,Weapon->MagazineSize));
    Message.Reset();
    Equip();
}
bool UPlayerLoadoutComponent::LoadDawn(int32& Day)
{
    if (!Settings || !UGameplayStatics::DoesSaveGameExist(Settings->SaveSlot,0)) return false;
    ULoadoutSave* Save = Cast<ULoadoutSave>(UGameplayStatics::LoadGameFromSlot(Settings->SaveSlot,0));
    if (!Save || Save->Version != 1 || Save->MapName != UGameplayStatics::GetCurrentLevelName(this,true)) return false;
    RestoreState(Save->State);
    Day = FMath::Max(1,Save->Day);
    return true;
}
bool UPlayerLoadoutComponent::SaveDawn(int32 Day)
{
    if (!Settings) return false;
    ULoadoutSave* Save = Cast<ULoadoutSave>(UGameplayStatics::CreateSaveGameObject(ULoadoutSave::StaticClass()));
    Save->State = State;
    Save->Day = Day;
    Save->MapName = UGameplayStatics::GetCurrentLevelName(this,true);
    const bool bSaved = UGameplayStatics::SaveGameToSlot(Save,Settings->SaveSlot,0);
    Message = bSaved ? TEXT("Dawn checkpoint saved") : TEXT("Save failed - progression is not committed to disk");
    return bSaved;
}
