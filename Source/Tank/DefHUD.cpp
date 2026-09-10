#include "DefHUD.h"
#include "DefGameMode.h"
#include "WallHealth.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"

void ADefHUD::DrawHUD()
{
	Super::DrawHUD();

	ADefGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADefGameMode>() : nullptr;
	if (!GM)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const float Scale = 1.2f;
	float Y = 20.0f;

	// 左上角战况
	if (const UWallHealthComponent* Wall = GM->GetWallHealth())
	{
		DrawText(FString::Printf(TEXT("城墙 HP: %.0f / %.0f"), Wall->GetCurrentHealth(), Wall->GetMaxHealth()),
			Wall->GetCurrentHealth() / FMath::Max(Wall->GetMaxHealth(), 1.0f) < 0.3f
				? FLinearColor::Red
				: FLinearColor::White,
			20.0f, Y, Font, Scale);
		Y += 30.0f;
	}
	DrawText(FString::Printf(TEXT("活丧尸: %d"), GM->GetAliveZombieCount()),
		FLinearColor::White, 20.0f, Y, Font, Scale);

	// 结局大字 + 重开倒计时
	const EDefMatchState State = GM->GetMatchState();
	if (State != EDefMatchState::InProgress)
	{
		const bool bWon = State == EDefMatchState::Won;
		const float CenterX = static_cast<float>(Canvas->SizeX) * 0.5f;
		const float TitleY = static_cast<float>(Canvas->SizeY) * 0.35f;
		const FString Title = bWon ? TEXT("守 住 了 ！") : TEXT("城 墙 陷 落 …");
		const FLinearColor TitleColor = bWon ? FLinearColor::Green : FLinearColor::Red;
		DrawText(Title, TitleColor, CenterX - 220.0f, TitleY, Font, 4.0f);
		const FString Sub = FString::Printf(TEXT("%.0f 秒后重新开始"), FMath::Max(GM->GetRestartCountdown(), 0.0f));
		DrawText(Sub, FLinearColor::Yellow, CenterX - 90.0f, TitleY + 90.0f, Font, 1.5f);
	}
}
