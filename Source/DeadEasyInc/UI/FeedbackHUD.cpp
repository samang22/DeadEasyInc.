#include "UI/BorderHUD.h"
#include "Progression/PlayerLoadoutComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

void ABorderHUD::DrawNotifications(UPlayerLoadoutComponent* Player)
{
    const float Now=GetWorld()->GetTimeSeconds();
    if (Now<Player->GainUntil)
    {
        const float W=FMath::Min(760.f,Canvas->SizeX-40.f),X=(Canvas->SizeX-W)*.5f,Y=Canvas->SizeY*.16f;
        const float Alpha=FMath::Clamp(Player->GainUntil-Now,0.f,1.f);
        DrawRect(FLinearColor(.02f,.18f,.12f,.95f*Alpha),X-8,Y-8,W+16,100);
        DrawText(TEXT("시체 분류 완료!"),FLinearColor(.4f,1.f,.55f,Alpha),X+20,Y,nullptr,1.65f);
        DrawText(FString::Printf(TEXT("연료 +%d     목재 +%d     금속 +%d"),Player->GainFuel,Player->GainWood,Player->GainIron),
            FLinearColor(1.f,.9f,.35f,Alpha),X+20,Y+43,nullptr,1.7f);
    }
    if (Now<Player->NoticeUntil)
    {
        const float W=FMath::Min(900.f,Canvas->SizeX-40.f),X=(Canvas->SizeX-W)*.5f,Y=Canvas->SizeY*.75f;
        DrawRect(Player->bNoticeError ? FLinearColor(.35f,.025f,.025f,.98f) : FLinearColor(.025f,.1f,.15f,.98f),X,Y,W,60);
        DrawText(Player->Notice,Player->bNoticeError ? FLinearColor(1.f,.65f,.5f) : FLinearColor::White,X+15,Y+17,nullptr,1.2f);
    }
}
void ABorderHUD::DrawTransactionConfirmation(UPlayerLoadoutComponent* Player)
{
    DrawRect(FLinearColor(0,0,0,.85f),0,0,Canvas->SizeX,Canvas->SizeY);
    AddHitBox(FVector2D::ZeroVector,FVector2D(Canvas->SizeX,Canvas->SizeY),TEXT("ModalBlocker"),true,100);
    const float W=FMath::Min(680.f,Canvas->SizeX-40.f),X=(Canvas->SizeX-W)*.5f,Y=Canvas->SizeY*.3f;
    DrawRect(FLinearColor(.04f,.06f,.08f,1.f),X-20,Y-25,W+40,290);
    DrawText(TEXT("재료를 사용해 진행하시겠습니까?"),FLinearColor::White,X,Y,nullptr,1.4f);
    DrawText(Player->PendingLabel,FLinearColor(.6f,.9f,1.f),X,Y+45,nullptr,1.2f);
    float CostY=Y+80;
    for (const auto& Pair : Player->PendingCost)
    {
        DrawText(FString::Printf(TEXT("%s: %d / 보유 %d"),*Pair.Key.ToString(),Pair.Value,Player->GetResource(Pair.Key)),FLinearColor::White,X,CostY);CostY+=20;
    }
    DrawRect(FLinearColor(.06f,.3f,.18f),X,Y+175,220,50);
    DrawText(TEXT("확정"),FLinearColor::White,X+75,Y+188);
    AddHitBox(FVector2D(X,Y+175),FVector2D(220,50),TEXT("ConfirmTransaction"),true,102);
    DrawRect(FLinearColor(.22f,.23f,.25f),X+250,Y+175,220,50);
    DrawText(TEXT("취소"),FLinearColor::White,X+325,Y+188);
    AddHitBox(FVector2D(X+250,Y+175),FVector2D(220,50),TEXT("CancelTransaction"),true,102);
}
