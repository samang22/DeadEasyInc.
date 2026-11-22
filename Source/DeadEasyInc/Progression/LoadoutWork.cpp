#include "Progression/PlayerLoadoutComponent.h"
#include "Scavenging/CorpseHandlingComponent.h"
#include "Buildings/Stations/ProgressionStation.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Buildings/Data/BuildingSettings.h"
#include "Core/BorderGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Combat/Weapons/EquippedWeapon.h"
#include "Engine/SkeletalMesh.h"
#include "Player/DeadEasyIncCharacter.h"

void UPlayerLoadoutComponent::SelectWork()
{
    if (!CanAct() || bMenuOpen) return;
    if (auto* Work=GetOwner()->FindComponentByClass<UCorpseHandlingComponent>()) { Work->Select();Equip(); }
}
void UPlayerLoadoutComponent::StoreCorpse()
{
    if (!CanAct() || bMenuOpen) return;
    if (auto* Work=GetOwner()->FindComponentByClass<UCorpseHandlingComponent>()) Work->Store();
}
TMap<FName,int32> UPlayerLoadoutComponent::CraftCost(const TMap<FName,int32>& Base) const
{
    auto Result=Base;const auto* Station=GetOpenStation();
    if (Station && Station->Settings && Station->Settings->Levels.IsValidIndex(Station->UpgradeLevel-1))
    {
        const float Factor=FMath::Clamp(Station->Settings->Levels[Station->UpgradeLevel-1].CraftCostMultiplier,.1f,1.f);
        for (auto& Cost : Result) Cost.Value=FMath::CeilToInt(FMath::Max(0,Cost.Value)*Factor);
    }
    return Result;
}
FString UPlayerLoadoutComponent::ToolStatus(const UToolDefinition* Tool) const
{
    if (!Tool) return TEXT("Unavailable");
    if (State.OwnedTools.Contains(Tool->Id)) return TEXT("Owned");
    if (!Tool->RequiredResearch.IsNone() && !State.Research.Contains(Tool->RequiredResearch)) return TEXT("Research required");
    if (GetOpenStation() && GetOpenStation()->UpgradeLevel<Tool->RequiredWorkbenchLevel) return TEXT("Upgrade workbench");
    return CanAfford(CraftCost(Tool->Cost)) ? TEXT("Craft") : TEXT("Not enough materials");
}
bool UPlayerLoadoutComponent::CraftTool(FName Id)
{
    if (!Settings || !CanUseStation(false)) return false;
    for (UToolDefinition* Tool : Settings->Tools) if (Tool && Tool->Id==Id)
    {
        Message=ToolStatus(Tool);if (Message!=TEXT("Craft")) { const FString Missing=Message==TEXT("Not enough materials") ? MissingMaterials(CraftCost(Tool->Cost)) : FString();Notify(Missing.IsEmpty() ? Message : Missing,true);return false; }
        if (!QueueTransaction(TEXT("Tool"),Id,Tool->DisplayName.ToString(),CraftCost(Tool->Cost))) return false;
        Pay(CraftCost(Tool->Cost));State.OwnedTools.AddUnique(Id);Notify(TEXT("손수레 제작 완료! 운반 용량이 증가했습니다."));
        if (auto* Work=GetOwner()->FindComponentByClass<UCorpseHandlingComponent>()) Work->RefreshVisuals();return true;
    }
    return false;
}
const UArmorDefinition* UPlayerLoadoutComponent::FindArmor(FName Id) const
{
    if (!Settings || Id.IsNone()) return nullptr;
    for (const UArmorDefinition* Armor : Settings->Armor)
        if (Armor && Armor->Id == Id) return Armor;
    return nullptr;
}

FString UPlayerLoadoutComponent::ArmorStatus(const UArmorDefinition* Armor) const
{
    if (!Armor || Armor->Id.IsNone()) return TEXT("Unavailable");
    if (State.EquippedArmor.FindRef(Armor->Slot) == Armor->Id) return TEXT("Equipped");
    if (State.OwnedArmor.Contains(Armor->Id)) return TEXT("Equip");
    if (!Armor->RequiredResearch.IsNone() && !State.Research.Contains(Armor->RequiredResearch))
        return TEXT("Research required");
    if (const auto* Station = GetOpenStation(); Station && Station->UpgradeLevel < Armor->RequiredWorkbenchLevel)
        return TEXT("Upgrade workbench");
    return CanAfford(CraftCost(Armor->CraftCost)) ? TEXT("Craft") : TEXT("Not enough materials");
}

bool UPlayerLoadoutComponent::CraftArmor(FName Id)
{
    if (!CanUseStation(false)) return false;
    const UArmorDefinition* Armor = FindArmor(Id);
    if (!Armor) return false;
    const FString Status = ArmorStatus(Armor);
    if (Status != TEXT("Craft") && Status != TEXT("Equip"))
    {
        const FString Missing = Status == TEXT("Not enough materials") ? MissingMaterials(CraftCost(Armor->CraftCost)) : FString();
        Notify(Missing.IsEmpty() ? Status : Missing, true);
        return false;
    }
    const bool bCrafting = Status == TEXT("Craft");
    const TMap<FName, int32> Cost = bCrafting ? CraftCost(Armor->CraftCost) : TMap<FName, int32>{};
    if (!QueueTransaction(TEXT("Armor"), Id, Armor->DisplayName.ToString(), Cost)) return false;
    Pay(Cost);
    State.OwnedArmor.AddUnique(Id);
    State.EquippedArmor.Add(Armor->Slot, Id);
    RefreshArmor();
    Notify(FString::Printf(TEXT("%s equipped"), *Armor->DisplayName.ToString()));
    return true;
}

void UPlayerLoadoutComponent::RefreshArmor()
{
    TArray<USkeletalMesh*> Meshes;
    float DamageFactor = 1.f;
    constexpr EArmorSlot Slots[] = { EArmorSlot::Chest, EArmorSlot::Head, EArmorSlot::Hands, EArmorSlot::Legs, EArmorSlot::Feet };
    for (EArmorSlot Slot : Slots)
    {
        const FName Id = State.EquippedArmor.FindRef(Slot);
        const UArmorDefinition* Armor = State.OwnedArmor.Contains(Id) ? FindArmor(Id) : nullptr;
        USkeletalMesh* Mesh = Settings ? Settings->ArmorBaseMeshes.FindRef(Slot).Get() : nullptr;
        if (Armor && Armor->Slot == Slot)
        {
            DamageFactor *= 1.f - FMath::Clamp(Armor->ZombieDamageReduction, 0.f, .9f);
            if (Armor->WorldMesh)
                Mesh = Armor->WorldMesh;
        }
        if (Mesh) Meshes.Add(Mesh);
    }
    ZombieDamageReduction = FMath::Clamp(1.f - DamageFactor, 0.f, .9f);
    if (auto* Character = Cast<ADeadEasyIncCharacter>(GetOwner()))
    {
        if (Meshes.Num() != UE_ARRAY_COUNT(Slots)) Meshes.Reset();
        Character->SetArmorAppearance(Meshes);
    }
}

ABuilding* UPlayerLoadoutComponent::FocusedBuilding() const
{
    const auto* Pawn=Cast<APawn>(GetOwner());const auto* Player=Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Player) return nullptr;
    FVector Eye;FRotator View;Player->GetPlayerViewPoint(Eye,View);FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(UpgradeUse),false,Pawn);if (Equipped) Params.AddIgnoredActor(Equipped);
    GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*(Settings ? Settings->InteractionReach : 300.f),ECC_Visibility,Params);
    return Cast<ABuilding>(Hit.GetActor());
}
void UPlayerLoadoutComponent::OpenUpgrade()
{
    if (!CanAct()) return;
    const auto* Mode=GetWorld()->GetAuthGameMode<ABorderGameMode>();
    if (Mode && Mode->bIsNight) return;
    ABuilding* Building=FocusedBuilding();
    if (!Building || !Building->Settings || Building->Settings->Levels.Num()<2 || Building->Condition==EBuildingCondition::Destroyed) return;
    if (bMenuOpen) CloseMenu();
    UpgradeBuilding=Building;bMenuOpen=true;StopFire();bReloading=false;
    if (Equipped) Equipped->CancelReloadAudio();
    if (auto* Work=GetOwner()->FindComponentByClass<UCorpseHandlingComponent>()) Work->Release();
    if (auto* Player=Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController()))
    {
        Player->SetIgnoreMoveInput(true);Player->SetIgnoreLookInput(true);Player->bShowMouseCursor=true;Player->bEnableClickEvents=true;
        FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);Player->SetInputMode(Input);
    }
}
bool UPlayerLoadoutComponent::ConfirmUpgrade()
{
    if (!bMenuOpen || !CanAct() || !IsValid(UpgradeBuilding) || FocusedBuilding()!=UpgradeBuilding) return false;
    Message=UpgradeBuilding->UpgradeStatus(this);
    if (Message!=TEXT("Upgrade"))
    {
        FString Missing;
        if (UpgradeBuilding->Settings && UpgradeBuilding->Settings->Levels.IsValidIndex(UpgradeBuilding->UpgradeLevel)) Missing=MissingMaterials(UpgradeBuilding->Settings->Levels[UpgradeBuilding->UpgradeLevel].Cost);
        Notify(Missing.IsEmpty() ? Message : Missing,true);return false;
    }
    const auto Cost=UpgradeBuilding->Settings->Levels[UpgradeBuilding->UpgradeLevel].Cost;
    if (!QueueTransaction(TEXT("Upgrade"),NAME_None,UpgradeBuilding->GetBuildingName().ToString()+FString::Printf(TEXT(" Lv.%d → Lv.%d"),UpgradeBuilding->UpgradeLevel,UpgradeBuilding->UpgradeLevel+1),Cost)) return false;
    if (!UpgradeBuilding->Upgrade(this)) { Notify(TEXT("업그레이드 조건을 다시 확인해주세요."),true);return false; }
    Notify(TEXT("건물 업그레이드 완료! 파괴되어도 단계는 유지됩니다."));return true;
}
