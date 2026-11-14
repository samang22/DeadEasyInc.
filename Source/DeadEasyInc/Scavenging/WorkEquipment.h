#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Scavenging/ScavengingData.h"
#include "WorkEquipment.generated.h"

// Meshes and component transforms are authored in the derived Blueprint.
UCLASS()
class DEADEASYINC_API AWorkEquipment : public AActor
{
    GENERATED_BODY()
public:
    AWorkEquipment();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Fork;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Pack;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> Cart;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class USceneComponent> CargoRoot;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<class UStaticMeshComponent>> ForkParts;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<class UStaticMeshComponent>> CartParts;
    void Refresh(EWorkTool Tool, bool bActive, bool bHandcart);
};
