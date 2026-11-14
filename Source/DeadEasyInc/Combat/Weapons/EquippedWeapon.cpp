#include "Combat/Weapons/EquippedWeapon.h"
#include "Progression/Data/LoadoutData.h"
#include "Zombies/BorderZombie.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"

static TAutoConsoleVariable<int32> CVarDamageDebug(TEXT("de.DamageDebug"),0,TEXT("Show weapon traces, bones and damage (0/1)."),ECVF_Cheat);

AEquippedWeapon::AEquippedWeapon()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    AttachmentMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttachmentMesh"));
    WeaponMesh->SetupAttachment(RootComponent);
    AttachmentMesh->SetupAttachment(WeaponMesh);
    for (UStaticMeshComponent* Part : {WeaponMesh.Get(), AttachmentMesh.Get()})
    {
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        Part->SetOnlyOwnerSee(true);
        Part->CastShadow = false;
    }
}

void AEquippedWeapon::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    SetActorRelativeLocation(FMath::VInterpTo(GetRootComponent()->GetRelativeLocation(), ViewTransform.GetLocation(), DeltaSeconds, 18.f));
}

bool AEquippedWeapon::Attack(const UWeaponDefinition* Definition)
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Player || !Definition) return false;
    FVector Eye;
    FRotator View;
    Player->GetPlayerViewPoint(Eye, View);
    bool bHitZombie = false;
    bool bHeadshot=false;
    int32 Hits=0,HeadHits=0;
    float RequestedDamage=0.f,AppliedDamage=0.f,RemainingHealth=0.f;
    FName LastBone;
    const bool bDebug=CVarDamageDebug.GetValueOnGameThread()!=0;
    const bool bMelee = Definition->Category == EWeaponCategory::Melee;
    const int32 Count = bMelee ? 1 : FMath::Clamp(Definition->Pellets, 1, 32);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PlayerWeapon), false, Pawn);
    Params.AddIgnoredActor(this);
    for (int32 Index=0; Index<Count; ++Index)
    {
        const FVector Direction = FMath::VRandCone(View.Vector(), FMath::DegreesToRadians(FMath::Max(0.f, Definition->SpreadDegrees)));
        const FVector End = Eye + Direction * FMath::Max(1.f, Definition->Range);
        FHitResult Hit;
        if (bMelee)
            GetWorld()->SweepSingleByChannel(Hit, Eye, End, FQuat::Identity, ECC_Visibility,
                FCollisionShape::MakeSphere(FMath::Max(1.f, Definition->MeleeRadius)), Params);
        else
            GetWorld()->LineTraceSingleByChannel(Hit, Eye, End, ECC_GameTraceChannel4, Params);
        if (ABorderZombie* Zombie = Cast<ABorderZombie>(Hit.GetActor()))
        {
            if (!Zombie->bDead)
            {
                const bool Head=!bMelee && Zombie->IsHeadshot(Hit);
                const float Damage=FMath::Max(0.f,Definition->Damage)*(Head ? FMath::Max(1.f,Definition->HeadshotMultiplier) : 1.f);
                const float Applied=UGameplayStatics::ApplyPointDamage(Zombie,Damage,Direction,Hit,Player,this,nullptr);
                if (Applied>0.f) { bHitZombie=true;bHeadshot|=Head;++Hits;HeadHits+=Head ? 1 : 0; }
                RequestedDamage+=Damage;AppliedDamage+=Applied;RemainingHealth=Zombie->Health;LastBone=Hit.BoneName;
                if (bDebug) DrawDebugSphere(GetWorld(),Hit.ImpactPoint,Head ? 7.f : 4.f,8,Head ? FColor::Orange : FColor::Green,false,2.f);
            }
        }
        if (bDebug) DrawDebugLine(GetWorld(),Eye,Hit.bBlockingHit ? Hit.ImpactPoint : End,Hit.bBlockingHit ? FColor::Green : FColor::Silver,false,2.f,0,.7f);
    }
    if (auto* Loadout=Pawn->FindComponentByClass<UPlayerLoadoutComponent>())
    {
        const float Now=GetWorld()->GetTimeSeconds();
        if (bHitZombie)
        {
            Loadout->LastHitTime=Now;Loadout->bLastHitWasHeadshot=bHeadshot;
            if (bHeadshot) Loadout->LastHeadshotTime=Now;
        }
        if (bDebug)
        {
            Loadout->DamageDebugUntil=Now+4.f;
            Loadout->DamageDebugText=Hits>0 ? FString::Printf(TEXT("Bone %s | Hits %d / Head %d | Damage %.0f (applied %.0f) | HP %.0f"),*LastBone.ToString(),Hits,HeadHits,RequestedDamage,AppliedDamage,RemainingHealth) : TEXT("MISS / BLOCKED - no living zombie damaged");
        }
    }
    if (AttackSound) UGameplayStatics::PlaySound2D(this, AttackSound);
    SetActorRelativeLocation(ViewTransform.GetLocation() + FVector(-3,0,bMelee ? -3 : 1));
    return bHitZombie;
}

void AEquippedWeapon::PlayReload()
{
    if (ReloadSound) UGameplayStatics::PlaySound2D(this, ReloadSound);
}
