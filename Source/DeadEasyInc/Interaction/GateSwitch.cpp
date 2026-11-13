#include "Interaction/GateSwitch.h"
#include "Components/StaticMeshComponent.h"
AGateSwitch::AGateSwitch()
{
    GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
}
