#include "Combat/Weapons/EquippedWeapon.h"
#include "Combat/Weapons/WeaponAudioComponent.h"
#include "Progression/Data/LoadoutData.h"
#include "Zombies/BorderZombie.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Player/DeadEasyIncCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"

static TAutoConsoleVariable<int32> CVarDamageDebug(TEXT("de.DamageDebug"),0,TEXT("Show weapon traces, bones and damage (0/1)."),ECVF_Cheat);

namespace
{
    constexpr float MeleeBlendOut = .16f;
}

AEquippedWeapon::AEquippedWeapon()
{
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
    WeaponAudio = CreateDefaultSubobject<UWeaponAudioComponent>(TEXT("WeaponAudio"));
}

void AEquippedWeapon::AttachToPlayer(ADeadEasyIncCharacter& Character)
{
    const USkeletalMeshComponent* FirstPerson = Character.GetFirstPersonMesh();
    USceneComponent* Mount = Character.GetWeaponMount();
    if (Mount && FirstPerson && FirstPerson->DoesSocketExist(Mount->GetAttachSocketName()) &&
        AttachToComponent(Mount, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
    {
        WeaponMesh->FirstPersonPrimitiveType = FirstPerson->FirstPersonPrimitiveType;
        AttachmentMesh->FirstPersonPrimitiveType = FirstPerson->FirstPersonPrimitiveType;
        SetActorRelativeTransform(FTransform(HandGripRotation, HandGripLocation, ViewTransform.GetScale3D()));
        return;
    }

    AttachToComponent(Character.GetFirstPersonCameraComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    SetActorRelativeTransform(ViewTransform);
}

UAnimMontage* AEquippedWeapon::PlayUpperBodyAnimation(UAnimSequence* Sequence, FName Slot, float BlendIn, float BlendOut, float Rate)
{
    if (!Sequence) return nullptr;
    const auto* Character = Cast<ADeadEasyIncCharacter>(GetOwner());
    auto* Body = Character ? Character->GetMesh() : nullptr;
    auto* AnimInstance = Body ? Body->GetAnimInstance() : nullptr;
    if (!AnimInstance) return nullptr;

    UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, Slot, BlendIn, BlendOut, Rate);
    if (!Montage || AnimInstance->Montage_Play(Montage, Rate, EMontagePlayReturnType::MontageLength, 0.f, false) <= 0.f)
        return nullptr;

    BodyAnimInstance = AnimInstance;
    return Montage;
}

bool AEquippedWeapon::PlayMeleeSwing(UAnimSequence* Sequence, float AttackInterval)
{
    if (!Sequence) return false;
    const float Rate = FMath::Max(1.f, Sequence->GetPlayLength() / AttackInterval);
    UAnimMontage* Montage = PlayUpperBodyAnimation(Sequence, TEXT("MeleeUpperBody"), .05f, MeleeBlendOut, Rate);
    if (!Montage) return false;
    MeleeMontage = Montage;
    return true;
}

void AEquippedWeapon::StopMeleeSwing()
{
    if (!MeleeMontage) return;
    if (UAnimInstance* AnimInstance = BodyAnimInstance.Get())
        AnimInstance->Montage_Stop(.07f, MeleeMontage);
    MeleeMontage = nullptr;
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
    Params.bReturnPhysicalMaterial=true;
    // The gameplay hit window remains instantaneous while the swing animation plays visually.
    WeaponAudio->PlayWeaponSound(bMelee ? (Definition->Audio.SwingSound ? Definition->Audio.SwingSound.Get() : AttackSound.Get()) :
        (Definition->Audio.FireSound ? Definition->Audio.FireSound.Get() : AttackSound.Get()));
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
        // Melee uses one single sweep per attack, so one blocking impact cannot double-play.
        ABorderZombie* Zombie = Cast<ABorderZombie>(Hit.GetActor());
        if (bMelee && Hit.bBlockingHit) WeaponAudio->PlayImpact(Hit, Zombie != nullptr);
        if (Zombie)
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
    return bHitZombie;
}

void AEquippedWeapon::ConfigureAudio(UWeaponDefinition* Definition)
{
    WeaponDefinition = Definition;
    WeaponAudio->Configure(Definition);
}

void AEquippedWeapon::PlayEquip()
{
    WeaponAudio->PlayEquip();
    if (WeaponDefinition)
        HoldMontage = PlayUpperBodyAnimation(WeaponDefinition->HoldAnimation, TEXT("WeaponHold"), .18f, .18f, 1.f);
    if (HoldMontage)
    {
        const FName Section = HoldMontage->GetSectionName(0);
        BodyAnimInstance->Montage_SetNextSection(Section, Section, HoldMontage);
    }
}
void AEquippedWeapon::PlayUnequip() { WeaponAudio->PlayUnequip(); }
void AEquippedWeapon::PlayDryFire() { WeaponAudio->PlayDryFire(); }
void AEquippedWeapon::PlayReload() { WeaponAudio->PlayReload(ReloadSound); }
void AEquippedWeapon::CancelReloadAudio() { WeaponAudio->CancelReloadAudio(); }

void AEquippedWeapon::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelReloadAudio();
    StopMeleeSwing();
    if (HoldMontage)
        if (UAnimInstance* AnimInstance = BodyAnimInstance.Get())
            AnimInstance->Montage_Stop(.18f, HoldMontage);
    Super::EndPlay(Reason);
}
