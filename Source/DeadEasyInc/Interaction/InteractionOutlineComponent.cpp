#include "Interaction/InteractionOutlineComponent.h"

#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
    // Keep these in sync with the post-process material generator.
    constexpr int32 UnfocusedStencil = 245;
    constexpr int32 FocusedStencil = 246;
}

void UInteractionOutlineComponent::BeginPlay()
{
    Super::BeginPlay();
    SetFocused(false);
}

void UInteractionOutlineComponent::SetFocused(bool bFocused)
{
    if (bOutlineEnabled && (bFocused || bOutlineWhenUnfocused))
        ApplyStencil(bFocused ? FocusedStencil : UnfocusedStencil);
    else
        RestoreMeshDepth();
}

void UInteractionOutlineComponent::ApplyStencil(int32 Stencil)
{
    if (!PreviousStates.IsEmpty())
    {
        for (const FMeshDepthState& Previous : PreviousStates)
            if (UMeshComponent* Mesh = Previous.Mesh.Get())
                Mesh->SetCustomDepthStencilValue(Stencil);
        return;
    }

    TInlineComponentArray<UMeshComponent*> Meshes(GetOwner());
    for (UMeshComponent* Mesh : Meshes)
    {
        if (!Mesh->IsRegistered()) continue;
        PreviousStates.Add({Mesh, Mesh->CustomDepthStencilValue, Mesh->CustomDepthStencilWriteMask, Mesh->bRenderCustomDepth != 0});
        Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_255);
        Mesh->SetCustomDepthStencilValue(Stencil);
        Mesh->SetRenderCustomDepth(true);
    }
}

void UInteractionOutlineComponent::RestoreMeshDepth()
{
    for (const FMeshDepthState& Previous : PreviousStates)
    {
        if (UMeshComponent* Mesh = Previous.Mesh.Get())
        {
            Mesh->SetCustomDepthStencilValue(Previous.StencilValue);
            Mesh->SetCustomDepthStencilWriteMask(Previous.StencilMask);
            Mesh->SetRenderCustomDepth(Previous.bRenderedCustomDepth);
        }
    }
    PreviousStates.Reset();
}

void UInteractionOutlineComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    RestoreMeshDepth();
    Super::EndPlay(Reason);
}
