#include "Zombies/AI/ZombieNavigationComponent.h"
#include "Zombies/BorderZombie.h"
#include "Zombies/AI/BuildingTargetRegistry.h"
#include "Zombies/Data/ZombieSettings.h"
#include "Buildings/Building.h"
#include "Buildings/Data/BuildingSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UZombieNavigationComponent::UZombieNavigationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UZombieNavigationComponent::InitializeRoute(const ABorderZombie& Zombie)
{
    NextDecisionTime = Zombie.GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.f, .65f);
    LastProgressLocation = Zombie.GetActorLocation();
    LastProgressTime = Zombie.GetWorld()->GetTimeSeconds();
}

FVector UZombieNavigationComponent::GetTargetPoint(const AActor& Seeker, const AActor& Target)
{
    FVector Point = Target.GetActorLocation();
    if (UPrimitiveComponent* Shape = Cast<UPrimitiveComponent>(Target.GetRootComponent()))
    {
        FVector Surface;
        if (Shape->GetClosestPointOnCollision(Seeker.GetActorLocation(), Surface) >= 0.f) Point = Surface;
    }
    return Point;
}

bool UZombieNavigationComponent::CanReach(const ACharacter& Seeker, AActor* Target, float Range)
{
    if (!IsValid(Target) || Target->ActorHasTag(TEXT("Dead"))) return false;
    const FVector Point = GetTargetPoint(Seeker, *Target);
    if (FVector::Dist(Seeker.GetActorLocation(), Point) > Range + Seeker.GetCapsuleComponent()->GetScaledCapsuleRadius()) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieSight), false, &Seeker);
    const bool bHit = Seeker.GetWorld()->LineTraceSingleByChannel(Hit, Seeker.GetActorLocation(), Point, ECC_Visibility, Params);
    return !bHit || Hit.GetActor() == Target;
}

static float RouteLength(const TArray<FVector>& Points)
{
    float Length=0;
    for (int32 Index=1;Index<Points.Num();++Index) Length+=FVector::Dist(Points[Index-1],Points[Index]);
    return Length;
}

bool UZombieNavigationComponent::BuildSegment(ABorderZombie& Zombie, const FVector& Start,const FVector& End,TArray<FVector>& Points,bool bAllowPartial) const
{
    UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(Zombie.GetWorld());
    if (!Nav || !Nav->GetDefaultNavDataInstance())
    {
        // Keeps simple maps usable; fixed obstacles still reject this fallback route.
        Points={Start,End}; return true;
    }
    FNavLocation Destination;
    if (!Nav->ProjectPointToNavigation(End,Destination,FVector(bAllowPartial ? 1000.f : 100.f,bAllowPartial ? 1000.f : 100.f,1000.f))) return false;
    UNavigationPath* Path=UNavigationSystemV1::FindPathToLocationSynchronously(Zombie.GetWorld(),Start,Destination.Location,&Zombie);
    if (!Path || !Path->IsValid() || (!bAllowPartial && Path->IsPartial()) || Path->PathPoints.Num()<2) return false;
    Points=Path->PathPoints;
    return true;
}

bool UZombieNavigationComponent::InspectRoute(const ABorderZombie& Zombie, const TArray<FVector>& Points,ABuilding*& Blocker) const
{
    Blocker=nullptr;
    const float Radius=Zombie.GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Half=Zombie.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieRoute),false,&Zombie);
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    for (int32 Index=1;Index<Points.Num();++Index)
    {
        FHitResult Hit;
        if (Zombie.GetWorld()->SweepSingleByObjectType(Hit,Points[Index-1]+FVector(0,0,Half+5),Points[Index]+FVector(0,0,Half+5),
            FQuat::Identity,Objects,FCollisionShape::MakeCapsule(Radius,FMath::Max(Radius,Half-10)),Params))
        {
            ABuilding* Building=Cast<ABuilding>(Hit.GetActor());
            if (!Building && Hit.ImpactNormal.Z>=Zombie.GetCharacterMovement()->GetWalkableFloorZ()) continue;
            if (!Building || Building->ActorHasTag(TEXT("Dead")) || (Building->Settings && !Building->Settings->bZombieTarget)) return false;
            Blocker=Building;
            return true;
        }
    }
    return true;
}

bool UZombieNavigationComponent::PlanRoute(ABorderZombie& Zombie, UBuildingTargetRegistry& Registry, const FVector& Destination,TArray<FVector>& Points,ABuilding*& Blocker) const
{
    const FVector Start=Zombie.GetNavAgentLocation();
    if (!BuildSegment(Zombie,Start,Destination,Points)) return false;
    const bool bDirectValid=InspectRoute(Zombie,Points,Blocker);
    if (bDirectValid && !Blocker) return true;
    const float DirectLength=RouteLength(Points);
    struct FPortalCandidate { FBorderOpening Opening; double Score; };
    TArray<FPortalCandidate> Candidates;
    for (const FBorderOpening& Opening : Registry.GetOpenings())
        Candidates.Add({Opening,FVector::Dist2D(Start,Opening.Center)+FVector::Dist2D(Opening.Center,Destination)});
    Candidates.Sort([](const auto& A,const auto& B){return A.Score<B.Score;});
    float Best=TNumericLimits<float>::Max(); TArray<FVector> BestRoute;
    for (int32 Index=0;Index<FMath::Min(Candidates.Num(),Zombie.Settings ? Zombie.Settings->PortalSearchCount : 4);++Index)
    {
        const auto& Opening=Candidates[Index].Opening;
        const float Side=FVector::DotProduct(Start-Opening.Center,Opening.Normal)>=0 ? 1.f : -1.f;
        FVector Near=Opening.Center+Opening.Normal*(Side*150.f),Far=Opening.Center-Opening.Normal*(Side*150.f);
        Near.Z=Far.Z=Start.Z;
        TArray<FVector> First,Middle,Last;
        if (!BuildSegment(Zombie,Start,Near,First) || !BuildSegment(Zombie,Near,Far,Middle) || !BuildSegment(Zombie,Far,Destination,Last)) continue;
        TArray<FVector> Combined=First;
        Combined.Append(Middle); Combined.Append(Last);
        ABuilding* Obstacle=nullptr;
        if (!InspectRoute(Zombie,Combined,Obstacle) || Obstacle) continue;
        const float Length=RouteLength(Combined);
        if (Length>DirectLength+(Zombie.Settings ? Zombie.Settings->MaxPortalDetour : 2500.f)) continue;
        if (Length<Best) { Best=Length; BestRoute=MoveTemp(Combined); }
    }
    if (!BestRoute.IsEmpty()) { Points=MoveTemp(BestRoute); Blocker=nullptr; return true; }
    return bDirectValid;
}

void UZombieNavigationComponent::ChooseBuildingGoal(ABorderZombie& Zombie, UBuildingTargetRegistry& Registry, TObjectPtr<AActor>& CurrentObjective, TObjectPtr<AActor>& CurrentTarget)
{
    bUsingFallback=false;
    Registry.Release(&Zombie);
    TArray<ABuilding*> Candidates=Registry.GetTargets();
    ABuilding* BestGoal=nullptr;ABuilding* BestBlocker=nullptr;
    float BestLength=TNumericLimits<float>::Max();
    TArray<FVector> BestRoute;
    // Anything already attackable is preferred to travelling to another building.
    for (ABuilding* Candidate : Candidates)
        if (CanReach(Zombie,Candidate,Zombie.AttackReach))
        {
            const float Distance=FVector::DistSquared(Zombie.GetActorLocation(),GetTargetPoint(Zombie, *Candidate));
            if (!BestGoal || Distance<BestLength) { BestGoal=Candidate;BestLength=Distance; }
        }
    if (!BestGoal)
    {
        Candidates.Sort([&Zombie](const ABuilding& A,const ABuilding& B)
        {
            return FVector::DistSquared(Zombie.GetActorLocation(),GetTargetPoint(Zombie, A))<
                FVector::DistSquared(Zombie.GetActorLocation(),GetTargetPoint(Zombie, B));
        });
        // Try nearby objectives first, stopping at the first usable route.
        for (ABuilding* Candidate : Candidates)
        {
            TArray<FVector> Path;ABuilding* Obstacle=nullptr;
            if (!PlanRoute(Zombie,Registry,Registry.ClaimApproach(Candidate,&Zombie),Path,Obstacle)) continue;
            BestGoal=Candidate;BestBlocker=Obstacle;BestRoute=MoveTemp(Path);break;
        }
    }
    if (!BestGoal)
    {
        // Keep an objective even if no complete attack route exists. Walk only as
        // far as a valid (possibly partial) navigation path allows, then wait.
        float Nearest=TNumericLimits<float>::Max();
        for (ABuilding* Candidate : Registry.GetTargets())
        {
            const float Distance=FVector::DistSquared2D(Zombie.GetActorLocation(),GetTargetPoint(Zombie, *Candidate));
            if (Distance<Nearest) { Nearest=Distance;BestGoal=Candidate; }
        }
        if (BestGoal)
        {
            bUsingFallback=true;
            if (BuildSegment(Zombie,Zombie.GetNavAgentLocation(),Registry.ClaimApproach(BestGoal,&Zombie),BestRoute,true))
            {
                ABuilding* Obstacle=nullptr;
                if (!InspectRoute(Zombie,BestRoute,Obstacle)) BestRoute.Reset();
                else BestBlocker=Obstacle;
            }
        }
    }
    Registry.Release(&Zombie);
    CurrentObjective=BestGoal;
    CurrentTarget=BestBlocker ? BestBlocker : BestGoal;
    Route=MoveTemp(BestRoute); Waypoint=1;
    if (BestGoal) Registry.ClaimApproach(BestGoal,&Zombie);
    if (BestBlocker && BestBlocker!=BestGoal)
    {
        TArray<FVector> WallRoute; ABuilding* Obstacle=nullptr;
        if (BuildSegment(Zombie,Zombie.GetNavAgentLocation(),Registry.ClaimApproach(BestBlocker,&Zombie),WallRoute) &&
            InspectRoute(Zombie,WallRoute,Obstacle) && (!Obstacle || Obstacle==BestBlocker)) Route=MoveTemp(WallRoute);
    }
}

void UZombieNavigationComponent::UpdateTarget(ABorderZombie& Zombie, float Now, TObjectPtr<AActor>& CurrentObjective, TObjectPtr<AActor>& CurrentTarget)
{
    const bool bShouldUpdate = Now >= NextDecisionTime || !IsValid(CurrentTarget) || CurrentTarget->ActorHasTag(TEXT("Dead"));
    if (!bShouldUpdate) return;

    UWorld* World = Zombie.GetWorld();
    UBuildingTargetRegistry* Registry = World ? World->GetSubsystem<UBuildingTargetRegistry>() : nullptr;
    if (!Registry) return;
    NextDecisionTime=Now+FMath::Max(.2f,Zombie.Settings ? Zombie.Settings->DecisionInterval : .65f);
    APawn* Player=UGameplayStatics::GetPlayerPawn(&Zombie,0);
    const bool bPlayerDetected=IsValid(Player) && !Player->ActorHasTag(TEXT("Dead")) &&
        FVector::DistSquared(Zombie.GetActorLocation(),Player->GetActorLocation())<=FMath::Square(Zombie.DetectionRadius);
    if (bPlayerDetected)
    {
        Registry->Release(&Zombie);
        CurrentObjective=Player;
        ABuilding* Blocker=nullptr;
        if (CanReach(Zombie,Player,Zombie.AttackReach))
        { bUsingFallback=false;CurrentTarget=Player;Route.Reset();return; }
        const FVector Destination=Cast<ACharacter>(Player) ? Cast<ACharacter>(Player)->GetNavAgentLocation() : Player->GetActorLocation();
        if (PlanRoute(Zombie,*Registry,Destination,Route,Blocker) && (Blocker ||
            (!Route.IsEmpty() && FVector::Dist(Route.Last(),Destination)<=Zombie.AttackReach+75.f)))
        {
            bUsingFallback=false;
            CurrentTarget=Blocker ? static_cast<AActor*>(Blocker) : Player;
            Waypoint=1;NextPathTime=0.f;
            return;
        }
        CurrentTarget=nullptr;Route.Reset();
    }
    if (CurrentObjective==Player) { CurrentObjective=nullptr;CurrentTarget=nullptr; }
    const bool bStuck=FVector::DistSquared2D(LastProgressLocation,Zombie.GetActorLocation())<900.f && Now-LastProgressTime>2.f;
    if (!IsValid(CurrentObjective) || CurrentObjective->ActorHasTag(TEXT("Dead")) || !IsValid(CurrentTarget) ||
        CurrentTarget->ActorHasTag(TEXT("Dead")) || Registry->GetRevision()!=PlanRevision || Now>=NextPathTime || bStuck)
    {
        ChooseBuildingGoal(Zombie, *Registry, CurrentObjective, CurrentTarget);
        PlanRevision=Registry->GetRevision();
        NextPathTime=Now+FMath::Max(1.f,Zombie.Settings ? Zombie.Settings->PathRefreshInterval : 2.5f);
        LastProgressTime=Now;LastProgressLocation=Zombie.GetActorLocation();
    }
    else if (FVector::DistSquared2D(LastProgressLocation,Zombie.GetActorLocation())>=900.f)
    { LastProgressLocation=Zombie.GetActorLocation(); LastProgressTime=Now; }
}

bool UZombieNavigationComponent::TryGetMoveDirection(const AActor& Mover, FVector& Direction)
{
    if (Route.Num() < 2) return false;

    const FVector Location = Mover.GetActorLocation();
    while (Waypoint < Route.Num() - 1 && FVector::DistSquared2D(Location, Route[Waypoint]) < 1600.f) ++Waypoint;
    if (bUsingFallback && Waypoint == Route.Num() - 1 && FVector::DistSquared2D(Location, Route.Last()) < 1600.f)
        return false;

    Direction = Route[FMath::Clamp(Waypoint, 0, Route.Num() - 1)] - Location;
    Direction.Z = 0;
    Direction.Normalize();
    return true;
}
