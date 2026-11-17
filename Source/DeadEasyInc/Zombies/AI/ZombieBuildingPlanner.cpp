#include "Zombies/BorderZombie.h"
#include "Core/BorderGameMode.h"
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
    if (bDead && (Reason == EEndPlayReason::Destroyed || Reason == EEndPlayReason::RemovedFromWorld))
    {
        const auto* Mode = GetWorld()->GetAuthGameMode<ABorderGameMode>();
        UE_LOG(LogTemp, Warning, TEXT("Corpse removed: %s Reason=%d Day=%d Night=%d Position=%s"),
            *GetName(), static_cast<int32>(Reason), Mode ? Mode->DayNumber : 0, Mode && Mode->bIsNight, *GetActorLocation().ToString());
    }
    if (UBuildingTargetRegistry* Registry=GetWorld()->GetSubsystem<UBuildingTargetRegistry>()) Registry->Release(this);
    Super::EndPlay(Reason);
}

bool ABorderZombie::BuildSegment(const FVector& Start,const FVector& End,TArray<FVector>& Points,bool bAllowPartial) const
{
    UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav || !Nav->GetDefaultNavDataInstance())
    {
        // Keeps simple maps usable; fixed obstacles still reject this fallback route.
        Points={Start,End}; return true;
    }
    FNavLocation Destination;
    if (!Nav->ProjectPointToNavigation(End,Destination,FVector(bAllowPartial ? 1000.f : 100.f,bAllowPartial ? 1000.f : 100.f,1000.f))) return false;
    UNavigationPath* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),Start,Destination.Location,const_cast<ABorderZombie*>(this));
    if (!Path || !Path->IsValid() || (!bAllowPartial && Path->IsPartial()) || Path->PathPoints.Num()<2) return false;
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
    const bool bDirectValid=InspectRoute(Points,Blocker);
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
    bUsingFallback=false;
    UBuildingTargetRegistry* Registry=GetWorld()->GetSubsystem<UBuildingTargetRegistry>();
    Registry->Release(this);
    TArray<ABuilding*> Candidates=Registry->GetTargets();
    ABuilding* BestGoal=nullptr;ABuilding* BestBlocker=nullptr;
    float BestLength=TNumericLimits<float>::Max();
    TArray<FVector> BestRoute;
    // Anything already attackable is preferred to travelling to another building.
    for (ABuilding* Candidate : Candidates)
        if (CanReach(Candidate,AttackReach))
        {
            const float Distance=FVector::DistSquared(GetActorLocation(),TargetPoint(Candidate));
            if (!BestGoal || Distance<BestLength) { BestGoal=Candidate;BestLength=Distance; }
        }
    if (!BestGoal)
    {
        Candidates.Sort([this](const ABuilding& A,const ABuilding& B)
        {
            return FVector::DistSquared(GetActorLocation(),TargetPoint(const_cast<ABuilding*>(&A)))<
                FVector::DistSquared(GetActorLocation(),TargetPoint(const_cast<ABuilding*>(&B)));
        });
        // Try nearby objectives first, stopping at the first usable route.
        for (ABuilding* Candidate : Candidates)
        {
            TArray<FVector> Path;ABuilding* Obstacle=nullptr;
            if (!PlanRoute(Candidate,Registry->ClaimApproach(Candidate,this),Path,Obstacle)) continue;
            BestGoal=Candidate;BestBlocker=Obstacle;BestRoute=MoveTemp(Path);break;
        }
    }
    if (!BestGoal)
    {
        // Keep an objective even if no complete attack route exists. Walk only as
        // far as a valid (possibly partial) navigation path allows, then wait.
        float Nearest=TNumericLimits<float>::Max();
        for (ABuilding* Candidate : Registry->GetTargets())
        {
            const float Distance=FVector::DistSquared2D(GetActorLocation(),TargetPoint(Candidate));
            if (Distance<Nearest) { Nearest=Distance;BestGoal=Candidate; }
        }
        if (BestGoal)
        {
            bUsingFallback=true;
            if (BuildSegment(GetNavAgentLocation(),Registry->ClaimApproach(BestGoal,this),BestRoute,true))
            {
                ABuilding* Obstacle=nullptr;
                if (!InspectRoute(BestRoute,Obstacle)) BestRoute.Reset();
                else BestBlocker=Obstacle;
            }
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
    const bool bPlayerDetected=IsValid(Player) && !Player->ActorHasTag(TEXT("Dead")) &&
        FVector::DistSquared(GetActorLocation(),Player->GetActorLocation())<=FMath::Square(DetectionRadius);
    if (bPlayerDetected)
    {
        Registry->Release(this);
        CurrentObjective=Player;
        ABuilding* Blocker=nullptr;
        if (CanReach(Player,AttackReach))
        { bUsingFallback=false;CurrentTarget=Player;Route.Reset();return; }
        const FVector Destination=Cast<ACharacter>(Player) ? Cast<ACharacter>(Player)->GetNavAgentLocation() : Player->GetActorLocation();
        if (PlanRoute(Player,Destination,Route,Blocker) && (Blocker ||
            (!Route.IsEmpty() && FVector::Dist(Route.Last(),Destination)<=AttackReach+75.f)))
        {
            bUsingFallback=false;
            CurrentTarget=Blocker ? static_cast<AActor*>(Blocker) : Player;
            Waypoint=1;NextPathTime=0.f;
            return;
        }
        CurrentTarget=nullptr;Route.Reset();
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
    if (bUsingFallback && Waypoint==Route.Num()-1 && FVector::DistSquared2D(GetActorLocation(),Route.Last())<1600.f)
    { GetCharacterMovement()->StopMovementImmediately();return; }
    FVector Direction=Route[FMath::Clamp(Waypoint,0,Route.Num()-1)]-GetActorLocation();
    Direction.Z=0;Direction.Normalize();
    PushCorpses(Direction,DeltaSeconds);
    AddMovementInput(Direction);
}
