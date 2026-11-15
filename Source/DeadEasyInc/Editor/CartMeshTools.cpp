#include "Editor/CartMeshTools.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "MeshDescription.h"

namespace
{
TArray<TArray<FPolygonID>> FindParts(const FMeshDescription& Mesh)
{
    TArray<FPolygonID> IDs;for (FPolygonID ID : Mesh.Polygons().GetElementIDs()) IDs.Add(ID);
    TArray<int32> Parents;for (int32 I=0;I<IDs.Num();++I) Parents.Add(I);
    auto Root=[&](int32 I){while (Parents[I]!=I) { Parents[I]=Parents[Parents[I]];I=Parents[I]; }return I;};
    TMap<FIntVector,int32> Positions;
    for (int32 I=0;I<IDs.Num();++I)
        for (FVertexID Vertex : Mesh.GetPolygonVertices(IDs[I]))
        {
            const FVector3f P=Mesh.GetVertexPosition(Vertex)*1000.f;
            const FIntVector Key(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y),FMath::RoundToInt(P.Z));
            if (const int32* Other=Positions.Find(Key))
            { const int32 A=Root(I),B=Root(*Other);Parents[FMath::Max(A,B)]=FMath::Min(A,B); }
            else Positions.Add(Key,I);
        }
    TMap<int32,int32> Groups;TArray<TArray<FPolygonID>> Result;
    for (int32 I=0;I<IDs.Num();++I)
    {
        const int32 R=Root(I);
        if (!Groups.Contains(R)) { Groups.Add(R,Result.Num());Result.AddDefaulted(); }
        Result[Groups[R]].Add(IDs[I]);
    }
    return Result;
}
void WriteSubset(UStaticMesh* Target,const FMeshDescription& Source,const TSet<FPolygonID>& Selected,bool bKeepSelected)
{
    FMeshDescription Mesh=Source;TArray<FPolygonID> Remove;
    for (FPolygonID ID : Mesh.Polygons().GetElementIDs()) if (Selected.Contains(ID)!=bKeepSelected) Remove.Add(ID);
    for (FPolygonID ID : Remove) Mesh.DeletePolygon(ID);
    TArray<FVertexInstanceID> Instances;
    for (FVertexInstanceID ID : Mesh.VertexInstances().GetElementIDs()) if (Mesh.GetVertexInstanceConnectedTriangleIDs(ID).Num()==0) Instances.Add(ID);
    for (FVertexInstanceID ID : Instances) Mesh.DeleteVertexInstance(ID);
    TArray<FEdgeID> Edges;
    for (FEdgeID ID : Mesh.Edges().GetElementIDs()) if (Mesh.GetEdgeConnectedTriangleIDs(ID).Num()==0) Edges.Add(ID);
    for (FEdgeID ID : Edges) Mesh.DeleteEdge(ID);
    TArray<FVertexID> Vertices;
    for (FVertexID ID : Mesh.Vertices().GetElementIDs()) if (Mesh.GetVertexConnectedEdgeIDs(ID).Num()==0) Vertices.Add(ID);
    for (FVertexID ID : Vertices) Mesh.DeleteVertex(ID);
    FElementIDRemappings Remap;Mesh.Compact(Remap);
    Target->Modify();Target->CreateMeshDescription(0,MoveTemp(Mesh));Target->CommitMeshDescription(0);
    Target->Build(false);Target->PostEditChange();Target->MarkPackageDirty();
}
}
#endif
TArray<FString> UCartMeshTools::DescribeParts(UStaticMesh* Source)
{
    TArray<FString> Result;
#if WITH_EDITOR
    if (!Source || !Source->GetMeshDescription(0)) return Result;
    const FMeshDescription& Mesh=*Source->GetMeshDescription(0);const auto Parts=FindParts(Mesh);
    for (int32 I=0;I<Parts.Num();++I)
    {
        FBox Bounds(ForceInit);
        for (FPolygonID Polygon : Parts[I]) for (FVertexID Vertex : Mesh.GetPolygonVertices(Polygon)) Bounds+=FVector(Mesh.GetVertexPosition(Vertex));
        Result.Add(FString::Printf(TEXT("%d polygons=%d min=%s max=%s"),I,Parts[I].Num(),*Bounds.Min.ToString(),*Bounds.Max.ToString()));
    }
#endif
    return Result;
}
bool UCartMeshTools::SplitParts(UStaticMesh* Source,const TArray<int32>& SelectedParts,UStaticMesh* Body,UStaticMesh* Tailgate)
{
#if WITH_EDITOR
    if (!Source || !Body || !Tailgate || Source==Body || Source==Tailgate || Body==Tailgate || !Source->GetMeshDescription(0)) return false;
    const FMeshDescription Mesh=*Source->GetMeshDescription(0);const auto Parts=FindParts(Mesh);TSet<FPolygonID> Selected;
    for (int32 Index : SelectedParts) { if (!Parts.IsValidIndex(Index)) return false;for (FPolygonID ID : Parts[Index]) Selected.Add(ID); }
    if (Selected.Num()==0 || Selected.Num()==Mesh.Polygons().Num()) return false;
    WriteSubset(Body,Mesh,Selected,false);WriteSubset(Tailgate,Mesh,Selected,true);return true;
#else
    return false;
#endif
}
