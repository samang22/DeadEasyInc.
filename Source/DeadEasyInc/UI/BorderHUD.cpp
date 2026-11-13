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
            if (Loadout->bNightConfirmationOpen) DrawNightConfirmation(Loadout);
            else DrawStationMenu(Loadout);
            return;
        }
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
    const FString RepairHint = *Hint ? FString(Hint) : FString::Printf(TEXT("Hold E to repair (%d wood / stroke)"), Repair->GetWoodPerStroke());
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
    DrawText(TEXT("1 Guns   2 Melee   Wheel Select   LMB Attack   R Reload   F Station"),FLinearColor::White,30,Canvas->SizeY-25);
    const UWeaponDefinition* Weapon = Loadout->GetActiveWeapon();
    FString WeaponText = Weapon ? Weapon->DisplayName.ToString() : TEXT("No weapon in this category");
    if (Weapon && Weapon->Category == EWeaponCategory::Ranged)
        WeaponText += Loadout->bReloading ? TEXT("  [Reloading]") : FString::Printf(TEXT("  %d / %d | Reserve: unlimited"),Loadout->State.Magazines.FindRef(Weapon->Id),Weapon->MagazineSize);
    DrawText(WeaponText,FLinearColor::White,30,65);
    DrawText(Loadout->Message,FLinearColor(.9f,.8f,.4f),30,95);
    if (!Loadout->bMenuOpen && Loadout->GetFocusedStation())
        DrawText(TEXT("F - Open station"),FLinearColor(.4f,1.f,.5f),Canvas->SizeX*.5f-100,Canvas->SizeY*.5f+85);
    if (!Loadout->bMenuOpen && Loadout->GetFocusedGateSwitch())
        DrawText(TEXT("F - 문 닫고 야간 근무 시작"),FLinearColor(.4f,1.f,.5f),Canvas->SizeX*.5f-140,Canvas->SizeY*.5f+85);
    if (GetWorld()->GetTimeSeconds()-Loadout->LastHitTime < .15f)
    {
        const float X=Canvas->SizeX*.5f,Y=Canvas->SizeY*.5f;
        DrawLine(X-9,Y-9,X+9,Y+9,FLinearColor::Red);
        DrawLine(X-9,Y+9,X+9,Y-9,FLinearColor::Red);
    }
}

void ABorderHUD::DrawStationMenu(UPlayerLoadoutComponent* Loadout)
{
    AProgressionStation* Station=Loadout->GetOpenStation();
    if (!Station || !Loadout->Settings) return;
    const float Width=FMath::Min(760.f,Canvas->SizeX-40.f);
    const float X=(Canvas->SizeX-Width)*.5f,Y=120.f;
    const bool bResearch=Station->StationType==EStationType::Research;
    const int32 Count=bResearch ? Loadout->Settings->Research.Num() : Loadout->Settings->Weapons.Num();
    const float RowHeight=FMath::Min(60.f,(Canvas->SizeY-Y-125.f)/FMath::Max(1,Count));
    DrawRect(FLinearColor(.025f,.035f,.045f,.98f),X-15,Y-15,Width+30,RowHeight*Count+115);
    DrawText(bResearch ? TEXT("RESEARCH DESK") : TEXT("WORKBENCH"),FLinearColor::White,X,Y,nullptr,1.3f);
    DrawText(FString::Printf(TEXT("Wood %d    Iron %d"),Loadout->GetResource(TEXT("Wood")),Loadout->GetResource(TEXT("Iron"))),FLinearColor(.8f,.85f,.9f),X,Y+30);
    for (int32 Index=0;Index<Count;++Index)
    {
        const UResearchDefinition* Research=bResearch ? Loadout->Settings->Research[Index].Get() : nullptr;
        const UWeaponDefinition* Weapon=bResearch ? nullptr : Loadout->Settings->Weapons[Index].Get();
        if (!Research && !Weapon) continue;
        const FString Status=bResearch ? Loadout->ResearchStatus(Research) : Loadout->CraftStatus(Weapon);
        const bool bAvailable=Status==TEXT("Research") || Status==TEXT("Craft");
        const float RowY=Y+60+Index*RowHeight;
        DrawRect(bAvailable ? FLinearColor(.08f,.22f,.18f) : FLinearColor(.09f,.10f,.12f),X,RowY,Width,RowHeight-3);
        DrawText((bResearch ? Research->DisplayName : Weapon->DisplayName).ToString()+TEXT("  -  ")+Status,
            bAvailable ? FLinearColor(.5f,1.f,.7f) : FLinearColor(.65f,.65f,.65f),X+10,RowY+4);
        DrawText(ResourceCostText(bResearch ? Research->Cost : Weapon->CraftCost),FLinearColor(.8f,.8f,.8f),X+10,RowY+23);
        const FString Name=(bResearch ? TEXT("Research.") : TEXT("Craft."))+(bResearch ? Research->Id : Weapon->Id).ToString();
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
    if (Name==TEXT("ConfirmNight")) Loadout->ConfirmNight();
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
