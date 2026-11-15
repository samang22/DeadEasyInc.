#include "Scavenging/HandcartDischargeComponent.h"
#include "Scavenging/ScavengingData.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

UHandcartDischargeComponent::UHandcartDischargeComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UHandcartDischargeComponent::Configure(UScavengingSettings* InSettings,USceneComponent* InBedPivot,USceneComponent* InTailgatePivot,UBoxComponent* InClosingArea)
{
    Settings=InSettings;BedPivot=InBedPivot;TailgatePivot=InTailgatePivot;TailgateClosingArea=InClosingArea;
    ClosedBedRotation=BedPivot->GetRelativeRotation().Quaternion();ClosedGateRotation=TailgatePivot->GetRelativeRotation().Quaternion();
}
void UHandcartDischargeComponent::Start() { bDischarging=true;bClosingTailgate=false;SpillTime=0.f; }
void UHandcartDischargeComponent::RequestClose() { if (bDischarging) bClosingTailgate=true; }
bool UHandcartDischargeComponent::CanCloseTailgate() const
{
    FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_GameTraceChannel3);Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CartTailgateClosing),false,GetOwner());
    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(Hits,TailgateClosingArea->GetComponentLocation(),TailgateClosingArea->GetComponentQuat(),Objects,
        FCollisionShape::MakeBox(TailgateClosingArea->GetScaledBoxExtent()),Params);
    return Hits.IsEmpty();
}
void UHandcartDischargeComponent::Reset()
{
    bDischarging=false;bClosingTailgate=false;TailgateAlpha=BedTipAlpha=SpillTime=0.f;
    TailgatePivot->SetRelativeRotation(ClosedGateRotation);BedPivot->SetRelativeRotation(ClosedBedRotation);
}
void UHandcartDischargeComponent::Advance(float Dt)
{
    if (!bDischarging) return;
    const float Seconds=Settings ? FMath::Max(.1f,Settings->TailgateSeconds) : .7f;
    if (bClosingTailgate)
    {
        // A blocked gate must not prevent the bin from returning to level after dismount.
        BedTipAlpha=FMath::FInterpConstantTo(BedTipAlpha,0.f,Dt,1.f/Seconds);
        if (CanCloseTailgate()) TailgateAlpha=FMath::FInterpConstantTo(TailgateAlpha,0.f,Dt,1.f/Seconds);
        if (TailgateAlpha<=KINDA_SMALL_NUMBER && BedTipAlpha<=KINDA_SMALL_NUMBER) { Reset();return; }
    }
    else
    {
        TailgateAlpha=FMath::FInterpConstantTo(TailgateAlpha,1.f,Dt,1.f/Seconds);
        if (TailgateAlpha>=1.f-KINDA_SMALL_NUMBER)
        {
            BedTipAlpha=FMath::FInterpConstantTo(BedTipAlpha,1.f,Dt,1.f/Seconds);
            SpillTime+=Dt;
            // The bin supports the real ragdolls. Opening and tilting is the entire discharge operation.
            if (SpillTime>=(Settings ? Settings->CartSpillSeconds : 3.f)) bClosingTailgate=true;
        }
    }
    const float Angle=Settings ? Settings->TailgateOpenAngle : 110.f;
    TailgatePivot->SetRelativeRotation(ClosedGateRotation*FQuat(FVector::ForwardVector,FMath::DegreesToRadians(-Angle*TailgateAlpha)));
    const float Tip=Settings ? Settings->CartTipAngle : 25.f;
    BedPivot->SetRelativeRotation(ClosedBedRotation*FRotator(-Tip*BedTipAlpha,0,0).Quaternion());
}
