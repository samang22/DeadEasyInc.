#include "Progression/PlayerLoadoutComponent.h"
#include "Scavenging/CorpseHandlingComponent.h"
#include "Buildings/Stations/ProgressionStation.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Buildings/Data/BuildingSettings.h"
#include "Core/BorderGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Combat/Weapons/EquippedWeapon.h"

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
