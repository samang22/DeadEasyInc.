#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BorderHUD.generated.h"

UCLASS()
class DEADEASYINC_API ABorderHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
