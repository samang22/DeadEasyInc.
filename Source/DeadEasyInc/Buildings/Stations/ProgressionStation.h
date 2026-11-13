#pragma once
#include "CoreMinimal.h"
#include "Buildings/Building.h"
#include "ProgressionStation.generated.h"

UENUM(BlueprintType)
enum class EStationType : uint8 { Research, Crafting };

UCLASS()
class DEADEASYINC_API AProgressionStation : public ABuilding
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) EStationType StationType = EStationType::Research;
};

UCLASS()
class DEADEASYINC_API AResearchStation : public AProgressionStation
{
    GENERATED_BODY()
public:
    AResearchStation() { StationType = EStationType::Research; }
};

UCLASS()
class DEADEASYINC_API AWorkbenchStation : public AProgressionStation
{
    GENERATED_BODY()
public:
    AWorkbenchStation() { StationType = EStationType::Crafting; }
};
