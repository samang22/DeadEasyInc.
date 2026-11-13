#include "Zombies/BorderZombie.h"
#include "Zombies/AI/BuildingTargetRegistry.h"
#include "Zombies/Data/ZombieSettings.h"
#include "Buildings/Building.h"
#include "Buildings/Defense/BorderWall.h"
#include "Buildings/Heating/CentralBoiler.h"
#include "Buildings/Data/BuildingSettings.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

static float RouteLength(const TArray<FVector>& Points)
{
    float Length=0;
    for (int32 Index=1;Index<Points.Num();++Index) Length+=FVector::Dist(Points[Index-1],Points[Index]);
    return Length;
}

void ABorderZombie::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UBuildingTargetRegistry* Registry=GetWorld()->GetSubsystem<UBuildingTargetRegistry>()) Registry->Release(this);
    Super::EndPlay(Reason);
}

bool ABorderZombie::BuildSegment(const FVector& Start,const FVector& End,TArray<FVector>& Points) const
{
    UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav || !Nav->GetDefaultNavDataInstance())
    {
        // Keeps simple maps usable; fixed obstacles still reject this fallback route.
        Points={Start,End}; return true;
    }
    FNavLocation Destination;
    if (!Nav->ProjectPointToNavigation(End,Destination,FVector(100,100,1000))) return false;
    UNavigationPath* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),Start,Destination.Location,const_cast<ABorderZombie*>(this));
    if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num()<2) return false;
    Points=Path->PathPoints;
    return true;
}

bool ABorderZombie::InspectRoute(const TArray<FVector>& Points,ABuilding*& Blocker) const
{
    Blocker=nullptr;
    const float Radius=GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Half=GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieRoute),false,this);
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    for (int32 Index=1;Index<Points.Num();++Index)
    {
        FHitResult Hit;
        if (GetWorld()->SweepSingleByObjectType(Hit,Points[Index-1]+FVector(0,0,Half+5),Points[Index]+FVector(0,0,Half+5),
            FQuat::Identity,Objects,FCollisionShape::MakeCapsule(Radius,FMath::Max(Radius,Half-10)),Params))
        {
            ABuilding* Building=Cast<ABuilding>(Hit.GetActor());
            if (!Building && Hit.ImpactNormal.Z>=GetCharacterMovement()->GetWalkableFloorZ()) continue;
            if (!Building || Building->ActorHasTag(TEXT("Dead")) || (Building->Settings && !Building->Settings->bZombieTarget)) return false;
            Blocker=Building;
            return true;
        }
    }
    return true;
}

bool ABorderZombie::PlanRoute(AActor* Goal,const FVector& Destination,TArray<FVector>& Points,ABuilding*& Blocker) const
{
    const FVector Start=GetNavAgentLocation();
    if (!BuildSegment(Start,Destination,Points)) return false;
    const bool bDirectValid=InspectRoute(Points,Blocker) && !(Blocker && Blocker->IsA<ACentralBoiler>() && Goal!=Blocker);
    if (bDirectValid && !Blocker) return true;
    const float DirectLength=RouteLength(Points);
    struct FPortalCandidate { FBorderOpening Opening; double Score; };
    TArray<FPortalCandidate> Candidates;
    UBuildingTargetRegistry* Registry=GetWorld()->GetSubsystem<UBuildingTargetRegistry>();
    for (const FBorderOpening& Opening : Registry->GetOpenings())
        Candidates.Add({Opening,FVector::Dist2D(Start,Opening.Center)+FVector::Dist2D(Opening.Center,Destination)});
    Candidates.Sort([](const auto& A,const auto& B){return A.Score<B.Score;});
    float Best=TNumericLimits<float>::Max(); TArray<FVector> BestRoute;
    for (int32 Index=0;Index<FMath::Min(Candidates.Num(),Settings ? Settings->PortalSearchCount : 4);++Index)
    {
        const auto& Opening=Candidates[Index].Opening;
        const float Side=FVector::DotProduct(Start-Opening.Center,Opening.Normal)>=0 ? 1.f : -1.f;
        FVector Near=Opening.Center+Opening.Normal*(Side*150.f),Far=Opening.Center-Opening.Normal*(Side*150.f);
        Near.Z=Far.Z=Start.Z;
        TArray<FVector> First,Middle,Last;
        if (!BuildSegment(Start,Near,First) || !BuildSegment(Near,Far,Middle) || !BuildSegment(Far,Destination,Last)) continue;
        TArray<FVector> Combined=First;
        Combined.Append(Middle); Combined.Append(Last);
        ABuilding* Obstacle=nullptr;
        if (!InspectRoute(Combined,Obstacle) || Obstacle) continue;
        const float Length=RouteLength(Combined);
        if (Length>DirectLength+(Settings ? Settings->MaxPortalDetour : 2500.f)) continue;
        if (Length<Best) { Best=Length; BestRoute=MoveTemp(Combined); }
    }
    if (!BestRoute.IsEmpty()) { Points=MoveTemp(BestRoute); Blocker=nullptr; return true; }
    return bDirectValid;
}

void ABorderZombie::ChooseBuildingGoal()
{
    UBuildingTargetRegistry* Registry=GetWorld()->GetSubsystem<UBuildingTargetRegistry>();
    Registry->Release(this);
    TArray<ABuilding*> Candidates=Registry->GetTargets();
    const bool bHasInterior=Candidates.ContainsByPredicate([](ABuilding* A){return !A->IsA<ABorderWall>();});
    Candidates.RemoveAll([&](ABuilding* A)
    {
        if (bHasInterior && A->IsA<ABorderWall>()) return true;
        return !A->IsA<ACentralBoiler>() && !A->IsA<ABorderWall>() &&
            FVector::Dist2D(GetActorLocation(),A->GetActorLocation())>(Settings ? Settings->MaxObjectiveDistance : 6000.f);
    });
    Candidates.Sort([&](const ABuilding& A,const ABuilding& B)
    {
        const int32 PA=A.Settings ? A.Settings->ZombieTargetPriority : 100;
        const int32 PB=B.Settings ? B.Settings->ZombieTargetPriority : 100;
        return PA!=PB ? PA>PB : FVector::DistSquared2D(GetActorLocation(),A.GetActorLocation())<FVector::DistSquared2D(GetActorLocation(),B.GetActorLocation());
    });
    ABuilding* BestGoal=nullptr; ABuilding* BestBlocker=nullptr;
    int32 BestTier=MAX_int32,BestPriority=MIN_int32;
    bool bBestBlocked=true; float BestLength=TNumericLimits<float>::Max();
    TArray<FVector> BestRoute;
    for (ABuilding* Candidate : Candidates)
    {
        TArray<FVector> Path; ABuilding* Obstacle=nullptr;
        if (!PlanRoute(Candidate,Registry->ClaimApproach(Candidate,this),Path,Obstacle)) continue;
        const int32 Tier=Candidate->IsA<ACentralBoiler>() ? 2 : Candidate->IsA<ABorderWall>() ? 3 : 1;
        const int32 Priority=Candidate->Settings ? Candidate->Settings->ZombieTargetPriority : 100;
        const bool bBlocked=Obstacle && Obstacle!=Candidate;
        const float Length=RouteLength(Path);
        if (!BestGoal || Tier<BestTier || (Tier==BestTier &&
            ((!bBlocked && bBestBlocked) || (bBlocked==bBestBlocked && (Priority>BestPriority || (Priority==BestPriority && Length<BestLength))))))
        {
            BestGoal=Candidate;BestBlocker=Obstacle;BestTier=Tier;BestPriority=Priority;bBestBlocked=bBlocked;BestLength=Length;BestRoute=MoveTemp(Path);
        }
    }
    Registry->Release(this);
    CurrentObjective=BestGoal;
    CurrentTarget=BestBlocker ? BestBlocker : BestGoal;
    Route=MoveTemp(BestRoute); Waypoint=1;
    if (BestGoal) Registry->ClaimApproach(BestGoal,this);
    if (BestBlocker && BestBlocker!=BestGoal)
    {
        TArray<FVector> WallRoute; ABuilding* Obstacle=nullptr;
        if (BuildSegment(GetNavAgentLocation(),Registry->ClaimApproach(BestBlocker,this),WallRoute) &&
            InspectRoute(WallRoute,Obstacle) && (!Obstacle || Obstacle==BestBlocker)) Route=MoveTemp(WallRoute);
    }
}

void ABorderZombie::Think(float Now)
{
    UBuildingTargetRegistry* Registry=GetWorld()->GetSubsystem<UBuildingTargetRegistry>();
    NextDecisionTime=Now+FMath::Max(.2f,Settings ? Settings->DecisionInterval : .65f);
    APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
    if (CanReach(Player,DetectionRadius))
    {
        Registry->Release(this);
        CurrentObjective=Player;
        ABuilding* Blocker=nullptr;
        if (PlanRoute(Player,Cast<ACharacter>(Player) ? Cast<ACharacter>(Player)->GetNavAgentLocation() : Player->GetActorLocation(),Route,Blocker))
            CurrentTarget=Blocker ? static_cast<AActor*>(Blocker) : Player;
        else { CurrentTarget=nullptr;Route.Reset(); }
        Waypoint=1; NextPathTime=0.f;
        return;
    }
    if (CurrentObjective==Player) { CurrentObjective=nullptr;CurrentTarget=nullptr; }
    const bool bStuck=FVector::DistSquared2D(LastProgressLocation,GetActorLocation())<900.f && Now-LastProgressTime>2.f;
    if (!IsValid(CurrentObjective) || CurrentObjective->ActorHasTag(TEXT("Dead")) || !IsValid(CurrentTarget) ||
        CurrentTarget->ActorHasTag(TEXT("Dead")) || Registry->GetRevision()!=PlanRevision || Now>=NextPathTime || bStuck)
    {
        ChooseBuildingGoal();
        PlanRevision=Registry->GetRevision();
        NextPathTime=Now+FMath::Max(1.f,Settings ? Settings->PathRefreshInterval : 2.5f);
        LastProgressTime=Now;LastProgressLocation=GetActorLocation();
    }
    else if (FVector::DistSquared2D(LastProgressLocation,GetActorLocation())>=900.f)
    { LastProgressLocation=GetActorLocation(); LastProgressTime=Now; }
}

void ABorderZombie::FollowRoute(float DeltaSeconds)
{
    if (Route.Num()<2) { GetCharacterMovement()->StopMovementImmediately(); return; }
    while (Waypoint<Route.Num()-1 && FVector::DistSquared2D(GetActorLocation(),Route[Waypoint])<1600.f) ++Waypoint;
    FVector Direction=Route[FMath::Clamp(Waypoint,0,Route.Num()-1)]-GetActorLocation();
    Direction.Z=0;Direction.Normalize();
    PushCorpses(Direction,DeltaSeconds);
    AddMovementInput(Direction);
}
