#include "Core/DayNight/DayNightCinematicComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneObjectBindingID.h"

UDayNightCinematicComponent::UDayNightCinematicComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDayNightCinematicComponent::BeginOverview(UCameraComponent& Camera, AActor* ViewTarget,
    const AActor* Focus, bool bAutoFrame, const FVector& Offset, float BlendTime)
{
    APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0);
    if (!Player) return;

    OverviewController = Player;
    if (bAutoFrame && !ViewTarget)
    {
        const FVector FocusLocation = IsValid(Focus) ? Focus->GetActorLocation() : GetOwner()->GetActorLocation();
        Camera.SetWorldLocation(FocusLocation + Offset);
        Camera.SetWorldRotation((-Offset).Rotation());
    }
    Camera.Activate();
    Player->SetViewTargetWithBlend(IsValid(ViewTarget) ? ViewTarget : GetOwner(), BlendTime, VTBlend_Cubic, 0.f, true);
}

void UDayNightCinematicComponent::RestoreView(float BlendTime)
{
    if (APlayerController* Player = OverviewController.Get())
    {
        AActor* ViewTarget = Player->GetPawn();
        Player->SetViewTargetWithBlend(ViewTarget ? ViewTarget : Player, BlendTime, VTBlend_Cubic, 0.f, true);
    }
    OverviewController.Reset();
}

void UDayNightCinematicComponent::PlaySequence(ULevelSequence* Sequence, const FGuid& OwnerBinding,
    float Duration, FSimpleDelegate InOnFinished)
{
    if (!Sequence) return;

    FMovieSceneSequencePlaybackSettings Playback;
    Playback.bPauseAtEnd = true;
    ALevelSequenceActor* CreatedActor = nullptr;
    SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(GetOwner(), Sequence, Playback, CreatedActor);
    SequenceActor = CreatedActor;
    if (!SequencePlayer) return;

    OnFinished = MoveTemp(InOnFinished);
    if (OwnerBinding.IsValid())
        SequenceActor->SetBinding(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(OwnerBinding)), {GetOwner()}, false);
    SequencePlayer->SetDisableCameraCuts(true);
    SequencePlayer->OnFinished.AddDynamic(this, &UDayNightCinematicComponent::HandleSequenceFinished);
    SequencePlayer->OnStop.AddDynamic(this, &UDayNightCinematicComponent::HandleSequenceFinished);
    SequencePlayer->SetPlayRate(FMath::Max(.01f, static_cast<float>(SequencePlayer->GetDuration().AsSeconds()) / Duration));
    SequencePlayer->Play();
}

void UDayNightCinematicComponent::HandleSequenceFinished()
{
    // Completion can synchronously ask this component to stop playback.
    FSimpleDelegate Completion = OnFinished;
    Completion.ExecuteIfBound();
}

void UDayNightCinematicComponent::StopPlayback()
{
    OnFinished.Unbind();
    if (SequencePlayer)
    {
        SequencePlayer->OnFinished.RemoveDynamic(this, &UDayNightCinematicComponent::HandleSequenceFinished);
        SequencePlayer->OnStop.RemoveDynamic(this, &UDayNightCinematicComponent::HandleSequenceFinished);
        SequencePlayer->Stop();
        SequencePlayer = nullptr;
    }
    if (SequenceActor)
    {
        SequenceActor->Destroy();
        SequenceActor = nullptr;
    }
}
