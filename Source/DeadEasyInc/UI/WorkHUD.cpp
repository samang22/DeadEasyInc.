#include "UI/BorderHUD.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Buildings/Stations/ProgressionStation.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Buildings/Data/BuildingSettings.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

static FString WorkCost(const TMap<FName,int32>& Cost)
{
    FString Text;for (const auto& Pair : Cost) Text+=FString::Printf(TEXT("%s %d   "),*Pair.Key.ToString(),Pair.Value);
    return Text.IsEmpty() ? TEXT("Free") : Text;
}
void ABorderHUD::DrawUpgrade(UPlayerLoadoutComponent* Player)
{
    ABuilding* Building=Player->UpgradeBuilding;if (!Building || !Building->Settings) return;
    const float W=FMath::Min(700.f,Canvas->SizeX-40.f),X=(Canvas->SizeX-W)*.5f,Y=Canvas->SizeY*.25f;
    DrawRect(FLinearColor(.025f,.035f,.045f,.98f),X-20,Y-20,W+40,330);
    DrawText(FString::Printf(TEXT("%s - LEVEL %d"),*Building->GetBuildingName().ToString(),Building->UpgradeLevel),FLinearColor::White,X,Y,nullptr,1.3f);
    if (Building->Settings->Levels.IsValidIndex(Building->UpgradeLevel))
    {
        const auto& Next=Building->Settings->Levels[Building->UpgradeLevel];
        DrawText(Next.Description.ToString(),FLinearColor(.7f,.85f,1.f),X,Y+45);
        DrawText(WorkCost(Next.Cost),FLinearColor::White,X,Y+80);
        DrawText(TEXT("Research: ")+Next.RequiredResearch.ToString(),FLinearColor::White,X,Y+105);
        DrawRect(FLinearColor(.08f,.25f,.18f),X,Y+145,W,40);
        DrawText(Building->UpgradeStatus(Player),FLinearColor::White,X+15,Y+155);
        AddHitBox(FVector2D(X,Y+145),FVector2D(W,40),TEXT("UpgradeBuilding"),true);
    }
    else DrawText(TEXT("Maximum level"),FLinearColor::Green,X,Y+60);
    if (Cast<ACentralBoiler>(Building))
    {
        DrawText(TEXT("Refill from fuel reserve [Click]"),FLinearColor(.9f,.75f,.3f),X,Y+200);
        AddHitBox(FVector2D(X,Y+195),FVector2D(W,30),TEXT("FillBoiler"),true);
    }
    DrawText(Player->Message,FLinearColor(.9f,.75f,.3f),X,Y+245);
    DrawText(TEXT("Close [F / Esc]"),FLinearColor::White,X,Y+275);
    AddHitBox(FVector2D(X,Y+270),FVector2D(200,30),TEXT("CloseStation"),true);
}
void ABorderHUD::DrawResearchTree(UPlayerLoadoutComponent* Player)
{
    const float Left=270.f,Top=110.f,Bottom=Canvas->SizeY-45.f;
    DrawRect(FLinearColor(.035f,.045f,.055f,.99f),0,Top-15,Canvas->SizeX,Canvas->SizeY-Top+15);
    DrawRect(FLinearColor(.018f,.025f,.035f),0,Top-15,Left-10,Canvas->SizeY-Top+15);
    float MX=0,MY=0;PlayerOwner->GetMousePosition(MX,MY);
    if (PlayerOwner->IsInputKeyDown(EKeys::RightMouseButton) && MX>Left)
    {
        if (bDraggingTree) TreePan+=FVector2D(MX,MY)-LastMouse;
        bDraggingTree=true;
    }
    else bDraggingTree=false;
    LastMouse=FVector2D(MX,MY);
    const float Wheel=PlayerOwner->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
    if (!FMath::IsNearlyZero(Wheel) && MX>Left) TreeZoom=FMath::Clamp(TreeZoom+Wheel*.08f,.45f,1.5f);
    DrawText(FString::Printf(TEXT("RESEARCH - LEVEL %d"),Player->GetOpenStation()->UpgradeLevel),FLinearColor::White,15,Top,nullptr,1.1f);
    DrawText(TEXT("Upgrade desk [Click]"),FLinearColor(.6f,.9f,1.f),15,Top+30);
    AddHitBox(FVector2D(15,Top+25),FVector2D(230,28),TEXT("OpenUpgrade"),true);
    DrawText(FString::Printf(TEXT("Wood %d   Iron %d"),Player->GetResource(TEXT("Wood")),Player->GetResource(TEXT("Iron"))),FLinearColor(.9f,.8f,.5f),15,Top+60);
    DrawText(TEXT("RMB drag | Wheel zoom"),FLinearColor(.7f,.7f,.7f),Left+10,Top);
    auto Position=[&](const UResearchDefinition* Research){return FVector2D(Left+20,Top+50)+TreePan+Research->TreePosition*TreeZoom;};
    auto Visible=[&](FVector2D P){return P.X>=Left && P.Y>=Top+30 && P.X+155*TreeZoom<Canvas->SizeX && P.Y+55*TreeZoom<Bottom;};
    for (const UResearchDefinition* Research : Player->Settings->Research)
    {
        if (!Research) continue;
        for (FName Prereq : Research->Prerequisites)
            for (const UResearchDefinition* Parent : Player->Settings->Research) if (Parent && Parent->Id==Prereq)
            {
                const FVector2D A=Position(Parent),B=Position(Research);
                if (Visible(A) && Visible(B)) DrawLine(A.X+155*TreeZoom,A.Y+25*TreeZoom,B.X,B.Y+25*TreeZoom,
                    SelectedResearch==Research->Id || SelectedResearch==Parent->Id ? FLinearColor(.2f,.9f,1.f) : FLinearColor(.25f,.3f,.35f),2.f);
            }
    }
    const UResearchDefinition* Selected=nullptr;
    for (const UResearchDefinition* Research : Player->Settings->Research)
    {
        if (!Research) continue;if (Research->Id==SelectedResearch) Selected=Research;
        const FVector2D P=Position(Research);if (!Visible(P)) continue;
        const FString Status=Player->ResearchStatus(Research);
        const bool Done=Player->State.Research.Contains(Research->Id),Available=Status==TEXT("Research");
        const FLinearColor Color=Done ? FLinearColor(.08f,.33f,.14f) : Available ? FLinearColor(.12f,.23f,.28f) : FLinearColor(.1f,.11f,.13f);
        DrawRect(SelectedResearch==Research->Id ? FLinearColor(.2f,.9f,1.f) : FLinearColor(.3f,.35f,.4f),P.X-2,P.Y-2,155*TreeZoom+4,55*TreeZoom+4);
        DrawRect(Color,P.X,P.Y,155*TreeZoom,55*TreeZoom);
        DrawText(Research->DisplayName.ToString(),FLinearColor::White,P.X+5,P.Y+5,nullptr,TreeZoom);
        DrawText(Done ? TEXT("Completed") : WorkCost(Research->Cost),FLinearColor(.75f,.8f,.85f),P.X+5,P.Y+27*TreeZoom,nullptr,.85f*TreeZoom);
        AddHitBox(P,FVector2D(155,55)*TreeZoom,FName(*(TEXT("SelectResearch.")+Research->Id.ToString())),true);
    }
    if (Selected)
    {
        DrawText(Selected->DisplayName.ToString(),FLinearColor::White,15,Top+90);
        DrawText(Selected->Description.ToString(),FLinearColor(.75f,.8f,.85f),15,Top+120);
        DrawText(FString::Printf(TEXT("Desk level %d"),Selected->RequiredResearchLevel),FLinearColor::White,15,Top+180);
        DrawText(WorkCost(Selected->Cost),FLinearColor::White,15,Top+210);
        DrawText(TEXT("Prerequisites:"),FLinearColor::White,15,Top+245);
        float Y=Top+270;
        for (FName Id : Selected->Prerequisites) { DrawText(Id.ToString(),Player->State.Research.Contains(Id) ? FLinearColor::Green : FLinearColor::Red,15,Y);Y+=20; }
        DrawRect(FLinearColor(.08f,.3f,.2f),15,Bottom-105,230,40);
        DrawText(Player->ResearchStatus(Selected),FLinearColor::White,25,Bottom-95);
        AddHitBox(FVector2D(15,Bottom-105),FVector2D(230,40),FName(*(TEXT("Research.")+Selected->Id.ToString())),true);
    }
    DrawText(TEXT("Changes saved next dawn"),FLinearColor(.9f,.75f,.3f),15,Bottom-50);
    DrawText(TEXT("Close [F / Esc]"),FLinearColor::White,15,Bottom-20);
    AddHitBox(FVector2D(15,Bottom-25),FVector2D(230,30),TEXT("CloseStation"),true);
}
