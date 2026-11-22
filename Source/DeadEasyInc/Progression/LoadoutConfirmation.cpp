#include "Progression/PlayerLoadoutComponent.h"
#include "Engine/World.h"

void UPlayerLoadoutComponent::Notify(const FString& Text,bool Error)
{
    Message=Text;Notice=Text;bNoticeError=Error;NoticeUntil=GetWorld()->GetTimeSeconds()+4.f;
}
void UPlayerLoadoutComponent::ShowResourceGain(int32 Wood,int32 Iron,int32 Fuel)
{
    const float Now=GetWorld()->GetTimeSeconds();
    if (Now>GainUntil) GainWood=GainIron=GainFuel=0;
    GainWood+=Wood;GainIron+=Iron;GainFuel+=Fuel;GainUntil=Now+3.5f;
}
FString UPlayerLoadoutComponent::MissingMaterials(const TMap<FName,int32>& Cost) const
{
    FString Missing;
    for (const auto& Pair : Cost) if (GetResource(Pair.Key)<Pair.Value)
    {
        const FString Name=Pair.Key==TEXT("Wood") ? TEXT("목재") : Pair.Key==TEXT("Iron") ? TEXT("금속") : Pair.Key.ToString();
        Missing+=FString::Printf(TEXT("%s %d개 부족  "),*Name,Pair.Value-GetResource(Pair.Key));
    }
    return Missing.IsEmpty() ? FString() : TEXT("재료 부족: ")+Missing;
}
bool UPlayerLoadoutComponent::QueueTransaction(FName Action,FName Id,const FString& Label,const TMap<FName,int32>& Cost)
{
    if (bExecutingTransaction) return true;
    PendingAction=Action;PendingId=Id;PendingLabel=Label;PendingCost=Cost;return false;
}
void UPlayerLoadoutComponent::CancelTransaction()
{
    PendingAction=NAME_None;PendingId=NAME_None;PendingLabel.Reset();PendingCost.Reset();
}
bool UPlayerLoadoutComponent::ConfirmTransaction()
{
    if (PendingAction.IsNone() || !bMenuOpen || !CanAct()) return false;
    const FName Action=PendingAction,Id=PendingId;
    const FString Missing=MissingMaterials(PendingCost);
    if (!Missing.IsEmpty()) { Notify(Missing,true);CancelTransaction();return false; }
    bExecutingTransaction=true;
    bool Result=false;
    if (Action==TEXT("Craft")) Result=Craft(Id);
    else if (Action==TEXT("Tool")) Result=CraftTool(Id);
    else if (Action==TEXT("Armor")) Result=CraftArmor(Id);
    else if (Action==TEXT("Upgrade")) Result=ConfirmUpgrade();
    else if (Action==TEXT("Research")) Result=Research(Id);
    bExecutingTransaction=false;CancelTransaction();return Result;
}
