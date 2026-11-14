#include "Scavenging/Handcart.h"
#include "Scavenging/CorpseCargoComponent.h"
#include "Scavenging/CorpseProcessor.h"
#include "Scavenging/ScavengingData.h"
#include "Zombies/BorderZombie.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

bool AHandcart::CanCloseTailgate() const
{
    FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_GameTraceChannel3);Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CartTailgateClosing),false,this);
    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(Hits,TailgateClosingArea->GetComponentLocation(),TailgateClosingArea->GetComponentQuat(),Objects,
        FCollisionShape::MakeBox(TailgateClosingArea->GetScaledBoxExtent()),Params);
    return Hits.IsEmpty();
}
void AHandcart::ResetDischarge()
{
    bDischarging=false;bClosingTailgate=false;TailgateAlpha=BedTipAlpha=SpillTime=0.f;
    TailgatePivot->SetRelativeRotation(ClosedGateRotation);BedPivot->SetRelativeRotation(ClosedBedRotation);
    SetActorTickEnabled(Driver!=nullptr);
}
void AHandcart::TickDischarge(float Dt)
{
    if (!bDischarging) return;
    const float Seconds=Settings ? FMath::Max(.1f,Settings->TailgateSeconds) : .7f;
    if (bClosingTailgate)
    {
        if (!CanCloseTailgate()) return;
        TailgateAlpha=FMath::FInterpConstantTo(TailgateAlpha,0.f,Dt,1.f/Seconds);
        BedTipAlpha=FMath::FInterpConstantTo(BedTipAlpha,0.f,Dt,1.f/Seconds);
        if (TailgateAlpha<=KINDA_SMALL_NUMBER && BedTipAlpha<=KINDA_SMALL_NUMBER) { ResetDischarge();return; }
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
