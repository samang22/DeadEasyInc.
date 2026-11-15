#include "UI/BorderHUD.h"
#include "Player/DeadEasyIncPlayerController.h"
#include "Engine/Canvas.h"

void ABorderHUD::DrawPauseMenu(ADeadEasyIncPlayerController* Player)
{
    const float Width=460.f,X=(Canvas->SizeX-Width)*.5f,Y=(Canvas->SizeY-360.f)*.5f;
    DrawRect(FLinearColor(0,0,0,.65f),0,0,Canvas->SizeX,Canvas->SizeY);
    DrawRect(FLinearColor(.035f,.045f,.06f,.98f),X,Y,Width,360);
    DrawText(TEXT("일시정지 / 설정"),FLinearColor::White,X+25,Y+20,nullptr,1.5f);
    auto Button=[&](FName Name,const FString& Label,float BX,float BY,float BW)
    {
        DrawRect(FLinearColor(.09f,.19f,.25f),BX,BY,BW,42);
        DrawText(Label,FLinearColor::White,BX+16,BY+11);
        AddHitBox(FVector2D(BX,BY),FVector2D(BW,42),Name,true);
    };
    Button(TEXT("Pause.Resume"),TEXT("게임으로 돌아가기  [ESC]"),X+25,Y+75,Width-50);
    DrawText(FString::Printf(TEXT("전체 볼륨   %d%%"),FMath::RoundToInt(Player->MasterVolume*100.f)),FLinearColor::White,X+25,Y+146);
    Button(TEXT("Pause.VolumeDown"),TEXT("-"),X+310,Y+135,50);
    Button(TEXT("Pause.VolumeUp"),TEXT("+"),X+375,Y+135,60);
    DrawText(FString::Printf(TEXT("마우스 감도   %.1f"),Player->MouseSensitivity),FLinearColor::White,X+25,Y+206);
    Button(TEXT("Pause.SensitivityDown"),TEXT("-"),X+310,Y+195,50);
    Button(TEXT("Pause.SensitivityUp"),TEXT("+"),X+375,Y+195,60);
    Button(TEXT("Pause.Quit"),TEXT("게임 종료"),X+25,Y+265,Width-50);
    DrawText(TEXT("볼륨과 감도는 자동 저장됩니다."),FLinearColor(.65f,.7f,.75f),X+25,Y+328);
}
