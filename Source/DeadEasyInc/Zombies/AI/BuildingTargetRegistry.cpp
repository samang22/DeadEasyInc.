#include "Zombies/AI/BuildingTargetRegistry.h"
#include "Buildings/Building.h"
#include "Buildings/Data/BuildingSettings.h"
#include "Buildings/Defense/BorderWall.h"
#include "Buildings/Defense/BorderGate.h"
#include "Zombies/BorderZombie.h"
#include "Shared/Health/CombatHealthComponent.h"
#include "Components/StaticMeshComponent.h"

void UBuildingTargetRegistry::Register(ABuilding* Building) { Buildings.Add(Building); ++Revision; }
void UBuildingTargetRegistry::Unregister(ABuilding* Building) { Buildings.Remove(Building); Reservations.Remove(Building); ++Revision; }
TArray<ABuilding*> UBuildingTargetRegistry::GetTargets() const
{
    TArray<ABuilding*> Result;
    for (const auto& Weak : Buildings)
        if (Weak.IsValid() && Weak->CombatHealth->Health>0.f && (!Weak->Settings || Weak->Settings->bZombieTarget)) Result.Add(Weak.Get());
    return Result;
}
void UBuildingTargetRegistry::Release(ABorderZombie* Zombie)
{
    for (auto& Pair : Reservations) Pair.Value.Remove(Zombie);
}
FVector UBuildingTargetRegistry::ClaimApproach(ABuilding* Building, ABorderZombie* Zombie)
{
    auto& Claims=Reservations.FindOrAdd(Building);
    for (auto It=Claims.CreateIterator();It;++It)
        if (!It.Key().IsValid() || It.Key()->bDead) It.RemoveCurrent();
    const FBox Box=Building->GetStaticMeshComponent()->Bounds.GetBox();
    const FVector Center=Box.GetCenter(), Extent=Box.GetExtent();
    TArray<FVector> Points;
    for (int32 Side=0;Side<4;++Side)
        for (int32 Index=0;Index<3;++Index)
        {
            const float T=(Index-1)*.6f;
            FVector Point=Center;
            if (Side<2) { Point.X+=(Side==0 ? 1 : -1)*(Extent.X+75.f); Point.Y+=T*Extent.Y; }
            else { Point.Y+=(Side==2 ? 1 : -1)*(Extent.Y+75.f); Point.X+=T*Extent.X; }
            Point.Z=Zombie->GetNavAgentLocation().Z;
            Points.Add(Point);
        }
    if (const int32* Existing=Claims.Find(Zombie)) return Points[FMath::Clamp(*Existing,0,Points.Num()-1)];
    float Best=TNumericLimits<float>::Max(); int32 Selected=0;
    for (int32 Index=0;Index<Points.Num();++Index)
    {
        int32 Occupants=0;
        for (const auto& Pair : Claims) if (Pair.Value==Index) ++Occupants;
        const float Score=FVector::DistSquared2D(Points[Index],Zombie->GetActorLocation())+Occupants*4000000.f;
        if (Score<Best) { Best=Score; Selected=Index; }
    }
    Claims.Add(Zombie,Selected);
    return Points[Selected];
}
const TArray<FBorderOpening>& UBuildingTargetRegistry::GetOpenings()
{
    if (CachedRevision==Revision) return Openings;
    CachedRevision=Revision;Openings.Reset();
    TArray<ABorderWall*> Walls;
    for (const auto& Weak : Buildings)
        if (ABorderWall* Wall=Cast<ABorderWall>(Weak.Get()))
        {
            Walls.Add(Wall);
            const ABorderGate* Gate=Cast<ABorderGate>(Wall);
            if (Wall->CombatHealth->Health<=0 || (Gate && Gate->OpenFraction>0.99f))
            {
                const FBox Box=Wall->GetStaticMeshComponent()->Bounds.GetBox();
                const FVector Extent=Box.GetExtent();
                if (FMath::Max(Extent.X,Extent.Y)>45.f)
                    Openings.Add({Box.GetCenter(),Extent.X>Extent.Y ? FVector(0,1,0) : FVector(1,0,0)});
            }
        }
    // Detect intentionally empty intervals between neighbouring wall sections as well.
    for (ABorderWall* A : Walls)
    {
        const FBox BoxA=A->GetStaticMeshComponent()->Bounds.GetBox();
        const bool bAlongX=BoxA.GetExtent().X>BoxA.GetExtent().Y;
        ABorderWall* Next=nullptr;float Nearest=TNumericLimits<float>::Max();
        for (ABorderWall* B : Walls)
        {
            if (A==B) continue;
            const FBox BoxB=B->GetStaticMeshComponent()->Bounds.GetBox();
            const FVector Delta=BoxB.GetCenter()-BoxA.GetCenter();
            const float Along=bAlongX ? Delta.X : Delta.Y;
            const float Across=bAlongX ? FMath::Abs(Delta.Y) : FMath::Abs(Delta.X);
            if (Along>0 && Along<Nearest && Across<75.f) { Nearest=Along; Next=B; }
        }
        if (!Next) continue;
        const FBox BoxB=Next->GetStaticMeshComponent()->Bounds.GetBox();
        const float Begin=bAlongX ? BoxA.Max.X : BoxA.Max.Y;
        const float End=bAlongX ? BoxB.Min.X : BoxB.Min.Y;
        if (End-Begin<100.f || End-Begin>3000.f) continue;
        FVector Center=BoxA.GetCenter();
        if (bAlongX) Center.X=(Begin+End)*.5f; else Center.Y=(Begin+End)*.5f;
        Openings.Add({Center,bAlongX ? FVector(0,1,0) : FVector(1,0,0)});
    }
    return Openings;
}
