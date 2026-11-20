#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DayNightCinematicComponent.generated.h"

class ALevelSequenceActor;
class APlayerController;
class UCameraComponent;
class ULevelSequence;
class ULevelSequencePlayer;

/** Manages view targets and sequence playback. The director still owns the phase timeline. */
UCLASS()
class DEADEASYINC_API UDayNightCinematicComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDayNightCinematicComponent();
    void BeginOverview(UCameraComponent& Camera, AActor* ViewTarget, const AActor* Focus,
        bool bAutoFrame, const FVector& Offset, float BlendTime);
    void RestoreView(float BlendTime);
    void PlaySequence(ULevelSequence* Sequence, const FGuid& OwnerBinding, float Duration,
        FSimpleDelegate InOnFinished);
    void StopPlayback();
    bool HasSequencePlayer() const { return SequencePlayer != nullptr; }

private:
    UFUNCTION() void HandleSequenceFinished();

    UPROPERTY(Transient) TObjectPtr<ULevelSequencePlayer> SequencePlayer;
    UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> SequenceActor;
    TWeakObjectPtr<APlayerController> OverviewController;
    FSimpleDelegate OnFinished;
};
