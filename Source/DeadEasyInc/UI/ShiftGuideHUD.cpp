#include "UI/BorderHUD.h"
#include "Core/BorderGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Scavenging/CorpseProcessor.h"
#include "Scavenging/ElectricCart.h"
#include "Scavenging/Handcart.h"

#define LOCTEXT_NAMESPACE "DeadEasyInc.ShiftGuide"

void ABorderHUD::DrawShiftGuide(const ABorderGameMode& Mode, const APawn& Pawn)
{
    if (Mode.DayNumber > 2 || (Mode.DayNumber == 2 && Mode.bIsNight)) return;

    const float Now = GetWorld()->GetTimeSeconds();
    if (GuideDay != Mode.DayNumber || bGuideNight != Mode.bIsNight)
    {
        GuideDay = Mode.DayNumber;
        bGuideNight = Mode.bIsNight;
        GuidePhaseStart = Now;
        GuideWaveStart = -1.f;
        bReachedGuideSorter = bReachedGuideCart = false;

        if (GuideDay == 1 && !bGuideNight)
        {
            for (TActorIterator<ACorpseProcessor> It(GetWorld()); It; ++It) { GuideSorter = *It; break; }
            for (TActorIterator<AHandcart> It(GetWorld()); It; ++It)
                if (!It->IsA<AElectricCart>()) { GuideCart = *It; break; }
        }
    }

    FText Title, Objective, Controls;
    if (GuideDay == 2)
    {
        if (Now - GuidePhaseStart > 20.f) return;
        Title = LOCTEXT("DayTwoTitle", "DAY 2 | PUT YOUR SALVAGE TO WORK");
        Objective = LOCTEXT("DayTwoObjective", "Research, craft, and repair for tonight.");
        Controls = LOCTEXT("DayTwoControls", "F: use desks | 3 + wheel: hammer | Hold E: repair");
    }
    else if (bGuideNight)
    {
        if (!Mode.bWaveRegistered) return;
        if (GuideWaveStart < 0.f) GuideWaveStart = Now;
        if (Now - GuideWaveStart > 15.f) return;
        Title = LOCTEXT("NightTitle", "FIRST SHIFT | NIGHT DUTY");
        Objective = LOCTEXT("NightObjective", "Protect the boiler. Clear the zombie wave.");
        Controls = LOCTEXT("NightControls", "1 / 2: weapons | LMB: attack | R: reload");
    }
    else
    {
        const int32 Count = Mode.GetFirstShiftCorpsesProcessed();
        constexpr int32 Goal = ABorderGameMode::FirstShiftCorpseGoal;
        FFormatNamedArguments Progress;
        Progress.Add(TEXT("Processed"), Count);
        Progress.Add(TEXT("Goal"), Goal);
        Title = FText::Format(LOCTEXT("CleanupProgress", "FIRST SHIFT | BODIES PROCESSED {Processed}/{Goal}"), Progress);
        if (Count >= Goal)
        {
            Objective = LOCTEXT("CleanupCompleteObjective", "Cleanup complete. Start the night when ready.");
            Controls = LOCTEXT("CleanupCompleteControls", "F at the gate switch, then confirm.");
        }
        else if (Count == 0)
        {
            const ACorpseProcessor* Sorter = GuideSorter.Get();
            bReachedGuideSorter |= !Sorter || FVector::DistSquared2D(Pawn.GetActorLocation(), Sorter->GetActorLocation()) <= FMath::Square(650.f);
            if (bReachedGuideSorter)
            {
                Objective = LOCTEXT("FeedSorterObjective", "Pick up a body and drop it into the sorter intake.");
                Controls = LOCTEXT("FeedSorterControls", "3 + wheel: corpse fork | LMB: pick up / drop");
            }
            else
            {
                Objective = LOCTEXT("FindSorterObjective", "The last shift left you three bodies. Find the sorter.");
                Controls = LOCTEXT("FindSorterHint", "Look for the sorter beside the central boiler.");
            }
        }
        else
        {
            const AHandcart* Cart = GuideCart.Get();
            bReachedGuideCart |= Cart && FVector::DistSquared2D(Pawn.GetActorLocation(), Cart->GetActorLocation()) <= FMath::Square(500.f);
            if (bReachedGuideCart)
            {
                Objective = LOCTEXT("UseCartObjective", "Optional: place bodies in the handcart to move them together.");
                Controls = LOCTEXT("UseCartControls", "F: grab cart | RMB at the sorter: unload");
            }
            else
            {
                Objective = LOCTEXT("FindCartObjective", "Salvage received. Look for more bodies by the handcart.");
                Controls = LOCTEXT("FindCartHint", "Carry them by hand or take them together in the cart.");
            }
        }
    }

    const FString Lines[] = {Title.ToString(), Objective.ToString(), Controls.ToString()};
    float Width = 0.f, LineStep = 23.f;
    for (const FString& Line : Lines)
    {
        float LineWidth, LineHeight;
        GetTextSize(Line, LineWidth, LineHeight, ShiftGuideFont);
        Width = FMath::Max(Width, LineWidth);
        LineStep = FMath::Max(LineStep, LineHeight + 4.f);
    }
    const float Scale = FMath::Clamp((Canvas->SizeX - 56.f) / FMath::Max(Width, 1.f), .55f, 1.f);
    const float Y = FMath::Min(190.f, Canvas->SizeY * .36f);
    DrawRect(FLinearColor(.025f, .03f, .035f, .8f), 20.f, Y, Width * Scale + 24.f, (7.f + 3.f * LineStep) * Scale);
    const FLinearColor Colors[] = {FLinearColor(.98f, .76f, .36f), FLinearColor::White, FLinearColor(.73f, .87f, .93f)};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Lines); ++Index)
        DrawText(Lines[Index], Colors[Index], 32.f, Y + (8.f + Index * LineStep) * Scale, ShiftGuideFont, Scale);
}

#undef LOCTEXT_NAMESPACE
