#include "Editor/BorderNavigationTools.h"
#if WITH_EDITOR
#include "Builders/CubeBuilder.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/BrushComponent.h"
#include "Model.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#endif

bool UBorderNavigationTools::BuildStaticNavigation(ANavMeshBoundsVolume* Bounds,FVector Dimensions)
{
#if WITH_EDITOR
    if (!Bounds) return false;
    UWorld* World=Bounds->GetWorld();
    if (!Bounds->Brush)
    {
        Bounds->Brush=NewObject<UModel>(Bounds,NAME_None,RF_Transactional);
        Bounds->Brush->Initialize(Bounds,true);
    }
    UCubeBuilder* Builder=NewObject<UCubeBuilder>(Bounds);
    Builder->X=Dimensions.X;Builder->Y=Dimensions.Y;Builder->Z=Dimensions.Z;
    Bounds->BrushBuilder=Builder;
    if (!Builder->Build(World,Bounds)) return false;
    Bounds->GetBrushComponent()->BuildSimpleBrushCollision();
    Bounds->GetBrushComponent()->UpdateBounds();
    Bounds->PostEditChange();
    UNavigationSystemV1* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!Nav) return false;
    Nav->OnNavigationBoundsUpdated(Bounds);
    Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock | ENavigationBuildLock::InitialLock | ENavigationBuildLock::NoUpdateInEditor);
    Nav->OnWorldInitDone(FNavigationSystemRunMode::EditorMode);

    Nav->Build();
    for (TActorIterator<ANavigationData> It(World);It;++It)
    {
        if (FProperty* Property=FindFProperty<FProperty>(It->GetClass(),TEXT("RuntimeGeneration")))
            *Property->ContainerPtrToValuePtr<ERuntimeGenerationType>(*It)=ERuntimeGenerationType::Static;
        It->EnsureBuildCompletion();
        It->MarkPackageDirty();
    }
    return Nav->GetDefaultNavDataInstance()!=nullptr && !Nav->IsNavigationBuildInProgress();
#else
    return false;
#endif
}
