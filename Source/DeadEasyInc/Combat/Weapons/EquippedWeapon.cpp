#include "Combat/Weapons/EquippedWeapon.h"
#include "Progression/Data/LoadoutData.h"
#include "Zombies/BorderZombie.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

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
            GetWorld()->LineTraceSingleByChannel(Hit, Eye, End, ECC_Visibility, Params);
        if (ABorderZombie* Zombie = Cast<ABorderZombie>(Hit.GetActor()))
        {
            if (!Zombie->bDead)
            {
                UGameplayStatics::ApplyPointDamage(Zombie, FMath::Max(0.f, Definition->Damage), Direction, Hit, Player, this, nullptr);
                bHitZombie = true;
            }
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
