#include "UI/BorderHUD.h"
#include "Buildings/Building.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Interaction/BuildingRepairComponent.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Core/BorderGameMode.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Buildings/Stations/ProgressionStation.h"
#include "Buildings/Data/BuildingSettings.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Scavenging/CorpseHandlingComponent.h"
#include "Scavenging/CorpseProcessor.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

void ABorderHUD::DrawHUD()
{
    Super::DrawHUD();
    APawn* Pawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
    UBuildingRepairComponent* Repair = Pawn ? Pawn->FindComponentByClass<UBuildingRepairComponent>() : nullptr;
    if (!Canvas || !Repair) return;
    if (const ABorderGameMode* Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>())
    {
        DrawText(FString::Printf(TEXT("Day %d - %s"), Mode->DayNumber, Mode->bTransitioning ? TEXT("CLOSING GATE") : Mode->bIsNight ? TEXT("NIGHT") : TEXT("DAY")),
            FLinearColor::White, 30, 30);
        if (Mode->bGameOver)
        {
            DrawText(TEXT("GAME OVER - Returning to dawn..."), FLinearColor::Red,
                Canvas->SizeX * 0.5f - 180, Canvas->SizeY * 0.5f - 60);
            return;
        }
        if (Mode->bTransitioning)
        {
            DrawText(TEXT("야간 근무 준비 중..."),FLinearColor::White,Canvas->SizeX*.5f-100,Canvas->SizeY*.75f);
            return;
        }
        if (Mode->bIsNight && Mode->bWaveRegistered)
            DrawText(FString::Printf(TEXT("Remaining: %d"),Mode->RemainingZombies),FLinearColor::White,30,48);
    }
    const float X = Canvas->SizeX * 0.5f;
    const float Y = Canvas->SizeY * 0.5f;
    if (UPlayerLoadoutComponent* Loadout = Pawn->FindComponentByClass<UPlayerLoadoutComponent>())
    {
        DrawLoadout(Loadout);
        if (Loadout->bMenuOpen)
        {
            if (!Loadout->PendingAction.IsNone()) DrawTransactionConfirmation(Loadout);
            else if (Loadout->UpgradeBuilding) DrawUpgrade(Loadout);
            else if (Loadout->bNightConfirmationOpen) DrawNightConfirmation(Loadout);
            else DrawStationMenu(Loadout);
            DrawNotifications(Loadout);return;
        }
        DrawNotifications(Loadout);
    }
    DrawLine(X - 4, Y, X + 4, Y, FLinearColor::White);
    DrawLine(X, Y - 4, X, Y + 4, FLinearColor::White);
    DrawText(FString::Printf(TEXT("Wood: %d"), Repair->GetWood()), FLinearColor::White, 30, Canvas->SizeY - 50);
    ABuilding* Wall = Repair->GetFocusedBuilding();
    if (!Wall) return;
    DrawText(FString::Printf(TEXT("%s: %.0f / %.0f"), *Wall->GetBuildingName().ToString(), Wall->CombatHealth->Health, Wall->CombatHealth->MaxHealth),
        FLinearColor::White, X - 100, Y + 30);
    const TCHAR* Hint = !Wall->bRepairAllowed ? TEXT("Repairs unavailable at night") :
        Wall->Condition == EBuildingCondition::Destroyed && !Wall->CanRestoreCollision() ? TEXT("Clear the passage before restoring") :
        !Wall->CanRepair() ? TEXT("Fully repaired") : Repair->GetWood() < Repair->GetWoodPerStroke() ? TEXT("Not enough wood") : TEXT("");
    const FString RepairHint = *Hint ? FString(Hint) : FString::Printf(TEXT("3 + Wheel: repair tool | Hold E (%d wood / stroke)"), Repair->GetWoodPerStroke());
    DrawText(RepairHint, FLinearColor::White, X - 100, Y + 55);
    if (Wall->Condition == EBuildingCondition::Destroyed)
        DrawText(FString::Printf(TEXT("Restoration: %.0f / %.0f"),Wall->RestorationProgress,Wall->GetRestorationTarget()),FLinearColor::White,X-100,Y+78);
}

static FString ResourceCostText(const TMap<FName,int32>& Cost)
{
    TArray<FName> Keys;
    Cost.GetKeys(Keys);
    Keys.Sort(FNameLexicalLess());
    FString Text;
    for (FName Key : Keys)
    {
        if (!Text.IsEmpty()) Text += TEXT("  |  ");
        Text += FString::Printf(TEXT("%s %d"),*Key.ToString(),Cost.FindRef(Key));
    }
    return Text.IsEmpty() ? TEXT("Free") : Text;
}

void ABorderHUD::DrawLoadout(UPlayerLoadoutComponent* Loadout)
{
    DrawText(FString::Printf(TEXT("Iron: %d"),Loadout->GetResource(TEXT("Iron"))),FLinearColor::White,30,Canvas->SizeY-75);
    DrawText(TEXT("1 Guns   2 Melee   3 Work   Wheel Select   LMB Use   R Reload   F Interact   U Upgrade"),FLinearColor::White,30,Canvas->SizeY-25);
    if (const auto* Work=Loadout->GetOwner()->FindComponentByClass<UCorpseHandlingComponent>())
        DrawText(Work->Hint(),FLinearColor(.65f,.85f,1.f),30,Canvas->SizeY-100);
    for (TActorIterator<ACentralBoiler> It(GetWorld());It;++It)
    {
        DrawText(FString::Printf(TEXT("Boiler L%d | Fuel %.0f / %.0f | Reserve %d %s"),It->UpgradeLevel,It->Fuel,It->Capacity(),Loadout->GetResource(TEXT("Fuel")),It->Fuel<=0 ? TEXT("[COLD - losing health at night]") : TEXT("")),It->Fuel<=0 ? FLinearColor::Red : FLinearColor(.9f,.7f,.3f),30,115);
        break;
    }
    if (const auto* Health=Loadout->GetOwner()->FindComponentByClass<UCombatHealthComponent>())
        DrawText(FString::Printf(TEXT("Health %.0f / %.0f"),Health->Health,Health->MaxHealth),FLinearColor::White,30,135);
    const UWeaponDefinition* Weapon = Loadout->GetActiveWeapon();
    FString WeaponText = Weapon ? Weapon->DisplayName.ToString() : TEXT("No weapon in this category");
    if (Weapon && Weapon->Category == EWeaponCategory::Ranged)
        WeaponText += Loadout->bReloading ? TEXT("  [Reloading]") : FString::Printf(TEXT("  %d / %d | Reserve: unlimited"),Loadout->State.Magazines.FindRef(Weapon->Id),Weapon->MagazineSize);
    DrawText(WeaponText,FLinearColor::White,30,65);
    if (!Loadout->bMenuOpen)
        for (TActorIterator<ACorpseProcessor> It(GetWorld());It;++It)
        {
            if (FVector::DistSquared(It->GetActorLocation(),Loadout->GetOwner()->GetActorLocation())>FMath::Square(1200.f)) continue;
            FVector2D Screen;
            if (PlayerOwner->ProjectWorldLocationToScreen(It->Intake->GetComponentLocation()+FVector(0,0,35),Screen))
                DrawText(TEXT("▼ 시체 투입구"),FLinearColor(.3f,1.f,.6f),Screen.X-65,Screen.Y-20,nullptr,1.2f);
        }
    DrawText(Loadout->Message,FLinearColor(.9f,.8f,.4f),30,95);
    if (!Loadout->bMenuOpen && Loadout->GetFocusedStation())
        DrawText(TEXT("F - Open station"),FLinearColor(.4f,1.f,.5f),Canvas->SizeX*.5f-100,Canvas->SizeY*.5f+85);
    if (!Loadout->bMenuOpen)
        if (const auto* Work=Loadout->GetOwner()->FindComponentByClass<UCorpseHandlingComponent>())
            if (Work->IsPushing() || Work->GetUsableHandcart())
                DrawText(Work->IsPushing() ? TEXT("F - 손수레 놓기") : TEXT("F - 손수레 잡기"),FLinearColor(.4f,1.f,.5f),Canvas->SizeX*.5f-90,Canvas->SizeY*.5f+110,nullptr,1.2f);
    if (!Loadout->bMenuOpen && Loadout->GetFocusedGateSwitch())
        DrawText(TEXT("F - 문 닫고 야간 근무 시작"),FLinearColor(.4f,1.f,.5f),Canvas->SizeX*.5f-140,Canvas->SizeY*.5f+85);
    if (GetWorld()->GetTimeSeconds()-Loadout->LastHitTime < .15f)
    {
        const float X=Canvas->SizeX*.5f,Y=Canvas->SizeY*.5f;
        const FLinearColor Color=Loadout->bLastHitWasHeadshot ? FLinearColor(1.f,.7f,.05f) : FLinearColor::Red;
        DrawLine(X-9,Y-9,X+9,Y+9,Color,Loadout->bLastHitWasHeadshot ? 3.f : 1.f);
        DrawLine(X-9,Y+9,X+9,Y-9,Color,Loadout->bLastHitWasHeadshot ? 3.f : 1.f);
    }
    if (GetWorld()->GetTimeSeconds()-Loadout->LastHeadshotTime<.6f)
        DrawText(TEXT("HEADSHOT"),FLinearColor(1.f,.7f,.05f),Canvas->SizeX*.5f-55,Canvas->SizeY*.5f-45,nullptr,1.3f);
    if (GetWorld()->GetTimeSeconds()<Loadout->DamageDebugUntil)
        DrawText(Loadout->DamageDebugText,FLinearColor(.4f,1.f,1.f),30,165);
}

void ABorderHUD::DrawStationMenu(UPlayerLoadoutComponent* Loadout)
{
    AProgressionStation* Station=Loadout->GetOpenStation();
    if (!Station || !Loadout->Settings) return;
    const float Width=FMath::Min(760.f,Canvas->SizeX-40.f);
    const float X=(Canvas->SizeX-Width)*.5f,Y=120.f;
    const bool bResearch=Station->StationType==EStationType::Research;
    if (bResearch) { DrawResearchTree(Loadout);return; }
    const int32 Count=bResearch ? Loadout->Settings->Research.Num() : Loadout->Settings->Weapons.Num()+Loadout->Settings->Tools.Num();
    const float RowHeight=FMath::Min(60.f,(Canvas->SizeY-Y-125.f)/FMath::Max(1,Count));
    DrawRect(FLinearColor(.025f,.035f,.045f,.98f),X-15,Y-15,Width+30,RowHeight*Count+115);
    DrawText(FString::Printf(TEXT("WORKBENCH - LEVEL %d   [Upgrade]"),Station->UpgradeLevel),FLinearColor::White,X,Y,nullptr,1.3f);
    DrawText(FString::Printf(TEXT("Wood %d    Iron %d"),Loadout->GetResource(TEXT("Wood")),Loadout->GetResource(TEXT("Iron"))),FLinearColor(.8f,.85f,.9f),X,Y+30);
    AddHitBox(FVector2D(X+Width-140,Y),FVector2D(140,30),TEXT("OpenUpgrade"),true);
    for (int32 Index=0;Index<Count;++Index)
    {
        const UResearchDefinition* Research=bResearch ? Loadout->Settings->Research[Index].Get() : nullptr;
        const UWeaponDefinition* Weapon=Loadout->Settings->Weapons.IsValidIndex(Index) ? Loadout->Settings->Weapons[Index].Get() : nullptr;
        const int32 ToolIndex=Index-Loadout->Settings->Weapons.Num();
        const UToolDefinition* Tool=Loadout->Settings->Tools.IsValidIndex(ToolIndex) ? Loadout->Settings->Tools[ToolIndex].Get() : nullptr;
        if (!Research && !Weapon && !Tool) continue;
        const FString Status=bResearch ? Loadout->ResearchStatus(Research) : Tool ? Loadout->ToolStatus(Tool) : Loadout->CraftStatus(Weapon);
        const bool bAvailable=Status==TEXT("Research") || Status==TEXT("Craft");
        const float RowY=Y+60+Index*RowHeight;
        DrawRect(bAvailable ? FLinearColor(.08f,.22f,.18f) : FLinearColor(.09f,.10f,.12f),X,RowY,Width,RowHeight-3);
        DrawText((bResearch ? Research->DisplayName : Tool ? Tool->DisplayName : Weapon->DisplayName).ToString()+TEXT("  -  ")+Status,
            bAvailable ? FLinearColor(.5f,1.f,.7f) : FLinearColor(.65f,.65f,.65f),X+10,RowY+4);
        DrawText(ResourceCostText(bResearch ? Research->Cost : Loadout->CraftCost(Tool ? Tool->Cost : Weapon->CraftCost)),FLinearColor(.8f,.8f,.8f),X+10,RowY+23);
        const FString Name=(bResearch ? TEXT("Research.") : Tool ? TEXT("Tool.") : TEXT("Craft."))+(bResearch ? Research->Id : Tool ? Tool->Id : Weapon->Id).ToString();
        AddHitBox(FVector2D(X,RowY),FVector2D(Width,RowHeight-3),FName(*Name),true);
    }
    const float FooterY=Y+60+Count*RowHeight;
    DrawText(TEXT("Saved at next dawn. Defeat rolls back today's changes."),FLinearColor(.9f,.8f,.4f),X,FooterY+3);
    DrawText(TEXT("Close [F / Esc]"),FLinearColor::White,X,FooterY+23);
    AddHitBox(FVector2D(X,FooterY+20),FVector2D(200,25),TEXT("CloseStation"),true);
}

void ABorderHUD::NotifyHitBoxClick(FName BoxName)
{
    Super::NotifyHitBoxClick(BoxName);
    APawn* Pawn=PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
    UPlayerLoadoutComponent* Loadout=Pawn ? Pawn->FindComponentByClass<UPlayerLoadoutComponent>() : nullptr;
    if (!Loadout || !Loadout->bMenuOpen) return;
    const FString Name=BoxName.ToString();
    if (Name==TEXT("ConfirmTransaction")) { Loadout->ConfirmTransaction();return; }
    if (Name==TEXT("CancelTransaction")) { Loadout->CancelTransaction();return; }
    if (!Loadout->PendingAction.IsNone()) return;
    if (Name==TEXT("OpenUpgrade")) Loadout->OpenUpgrade();
    else if (Name==TEXT("UpgradeBuilding")) Loadout->ConfirmUpgrade();
    else if (Name==TEXT("FillBoiler"))
    {
        if (auto* Boiler=Cast<ACentralBoiler>(Loadout->UpgradeBuilding))
        {
            const int32 Amount=FMath::Min(Loadout->GetResource(TEXT("Fuel")),FMath::FloorToInt(FMath::Max(0.f,Boiler->Capacity()-Boiler->Fuel)));
            if (Amount>0) { Boiler->AddFuel(Amount);Loadout->SpendResource(TEXT("Fuel"),Amount); }
        }
    }
    else if (Name.StartsWith(TEXT("SelectResearch."))) SelectedResearch=FName(*Name.RightChop(15));
    else if (Name.StartsWith(TEXT("Tool."))) Loadout->CraftTool(FName(*Name.RightChop(5)));
    else if (Name==TEXT("ConfirmNight")) Loadout->ConfirmNight();
    else if (Name==TEXT("CancelNight")) Loadout->CloseMenu();
    else if (Name==TEXT("CloseStation")) Loadout->CloseMenu();
    else if (Name.StartsWith(TEXT("Research."))) Loadout->Research(FName(*Name.RightChop(9)));
    else if (Name.StartsWith(TEXT("Craft."))) Loadout->Craft(FName(*Name.RightChop(6)));
}

void ABorderHUD::DrawNightConfirmation(UPlayerLoadoutComponent* Loadout)
{
    const float W=FMath::Min(660.f,Canvas->SizeX-40.f),X=(Canvas->SizeX-W)*.5f,Y=Canvas->SizeY*.35f;
    DrawRect(FLinearColor(.025f,.035f,.045f,.98f),X-20,Y-25,W+40,210);
    DrawText(TEXT("낮을 끝내고 밤을 시작하겠습니까?"),FLinearColor::White,X,Y,nullptr,1.2f);
    DrawText(TEXT("확정하면 문이 닫히고 야간 방어가 시작됩니다."),FLinearColor(.8f,.8f,.8f),X,Y+45);
    DrawText(TEXT("저장은 다음 낮 시작에 진행됩니다."),FLinearColor(.9f,.8f,.4f),X,Y+70);
    DrawRect(FLinearColor(.08f,.3f,.18f),X,Y+110,180,45);
    DrawText(TEXT("시작"),FLinearColor::White,X+55,Y+123);
    AddHitBox(FVector2D(X,Y+110),FVector2D(180,45),TEXT("ConfirmNight"),true,2);
    DrawRect(FLinearColor(.2f,.2f,.23f),X+210,Y+110,180,45);
    DrawText(TEXT("취소"),FLinearColor::White,X+265,Y+123);
    AddHitBox(FVector2D(X+210,Y+110),FVector2D(180,45),TEXT("CancelNight"),true,2);
}
