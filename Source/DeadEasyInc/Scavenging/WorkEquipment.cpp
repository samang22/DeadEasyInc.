#include "Scavenging/WorkEquipment.h"
#include "Components/StaticMeshComponent.h"

AWorkEquipment::AWorkEquipment()
{
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Fork=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fork"));Fork->SetupAttachment(RootComponent);
    Hammer=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hammer"));Hammer->SetupAttachment(RootComponent);
    Hammer->SetRelativeLocation(FVector(45.f,22.f,-20.f));
    Hammer->SetCollisionEnabled(ECollisionEnabled::NoCollision);Hammer->SetCanEverAffectNavigation(false);
    Pack=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pack"));Pack->SetupAttachment(RootComponent);
    Cart=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cart"));Cart->SetupAttachment(RootComponent);
    CargoRoot=CreateDefaultSubobject<USceneComponent>(TEXT("CargoRoot"));CargoRoot->SetupAttachment(RootComponent);
    for (int32 Index=0;Index<4;++Index)
    {
        auto* Part=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ForkPart%d"),Index));
        Part->SetupAttachment(Fork);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCanEverAffectNavigation(false);ForkParts.Add(Part);
        auto* Wheel=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("CartWheel%d"),Index));
        Wheel->SetupAttachment(RootComponent);Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);Wheel->SetCanEverAffectNavigation(false);CartParts.Add(Wheel);
    }
    for (UStaticMeshComponent* Mesh : {Fork.Get(),Pack.Get(),Cart.Get()})
    { Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCanEverAffectNavigation(false); }
}
void AWorkEquipment::Refresh(EWorkTool Tool,bool bActive,bool bHandcart)
{
    Hammer->SetVisibility(bActive && Tool==EWorkTool::Repair);
    Fork->SetVisibility(bActive && Tool!=EWorkTool::Repair);
    for (UStaticMeshComponent* Part : ForkParts) Part->SetVisibility(bActive && Tool!=EWorkTool::Repair);
    for (UStaticMeshComponent* Part : CartParts) Part->SetVisibility(false);
    Cart->SetVisibility(false);
    Pack->SetVisibility(false);
}
