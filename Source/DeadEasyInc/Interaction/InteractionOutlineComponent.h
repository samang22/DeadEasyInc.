#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionOutlineComponent.generated.h"

class UMeshComponent;
enum class ERendererStencilMask : uint8;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class DEADEASYINC_API UInteractionOutlineComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outline")
    bool bOutlineEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outline", meta=(EditCondition="bOutlineEnabled"))
    bool bOutlineWhenUnfocused = true;

    void SetFocused(bool bFocused);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    struct FMeshDepthState
    {
        TWeakObjectPtr<UMeshComponent> Mesh;
        int32 StencilValue;
        ERendererStencilMask StencilMask;
        bool bRenderedCustomDepth;
    };

    void ApplyStencil(int32 Stencil);
    void RestoreMeshDepth();

    TArray<FMeshDepthState> PreviousStates;
};
