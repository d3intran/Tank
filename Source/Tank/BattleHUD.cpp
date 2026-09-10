#include "BattleHUD.h"
#include "Tank.h"
#include "TankPawn.h"
#include "TankHealth.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"

void ABattleHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GetWorld())
	{
		return;
	}

	// 自己的坦克 → 左下角血条
	ATankPawn* MyTank = nullptr;
	if (APlayerController* PC = GetOwningPlayerController())
	{
		MyTank = Cast<ATankPawn>(PC->GetPawn());
	}

	if (MyTank)
	{
		if (UTankHealth* Health = MyTank->FindComponentByClass<UTankHealth>())
		{
			const float Ratio = FMath::Clamp(Health->GetCurrentHealth() / FMath::Max(Health->GetMaxHealth(), 1.0f), 0.0f, 1.0f);

			// 分段变色：>50% 绿 / 25~50% 黄 / <25% 红 + 呼吸频率闪烁
			FLinearColor Color;
			if (Ratio > 0.5f)
			{
				Color = FLinearColor(0.15f, 0.75f, 0.25f, 0.9f);
			}
			else if (Ratio > 0.25f)
			{
				Color = FLinearColor(0.92f, 0.78f, 0.1f, 0.9f);
			}
			else
			{
				// 呼吸闪烁：alpha 在 0.35~1.0 间以 ~1.2Hz 正弦起伏
				const float Breath = 0.675f + 0.325f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.0f * PI * 1.2f);
				Color = FLinearColor(0.85f, 0.12f, 0.12f, Breath);
			}

			const float BarX = 40.0f;
			const float BarY = Canvas->SizeY - OwnBarSize.Y - 40.0f;
			DrawHealthBar(Ratio, BarX, BarY, OwnBarSize.X, OwnBarSize.Y, Color);
		}
	}

	// 其他玩家坦克 → 头顶红色血条（>80m 距离剔除）
	APlayerController* LocalPC = GetOwningPlayerController();
	for (TActorIterator<ATankPawn> It(GetWorld()); It; ++It)
	{
		ATankPawn* Tank = *It;
		// 用 Controller 归属跳过自己的坦克（PC->GetPawn 在重生瞬间为空，指针比对会漏判）
		if (!Tank || Tank->IsPendingKillPending() || !Tank->GetController() || Tank->GetController() == LocalPC)
		{
			continue;
		}
		UTankHealth* Health = Tank->FindComponentByClass<UTankHealth>();
		if (!Health || Health->IsDepleted())
		{
			continue;
		}

		const FVector TankLoc = Tank->GetActorLocation();
		if (MyTank && FVector::Dist2D(TankLoc, MyTank->GetActorLocation()) > 8000.0f)
		{
			continue;
		}

		// UE5 Canvas::Project 返回 FVector：XY=屏幕像素，Z=相机后方深度（0=在视锥内可用）
		const FVector Projected = Canvas->Project(TankLoc + FVector(0.0f, 0.0f, 240.0f));
		const bool bOnScreen = Projected.Z == 0.0f
			&& Projected.X > -OverheadBarSize.X && Projected.X < Canvas->SizeX + OverheadBarSize.X
			&& Projected.Y > 0.0f && Projected.Y < Canvas->SizeY;
		if (!bOnScreen)
		{
			continue;
		}

		const float Ratio = FMath::Clamp(Health->GetCurrentHealth() / FMath::Max(Health->GetMaxHealth(), 1.0f), 0.0f, 1.0f);
		DrawHealthBar(Ratio, Projected.X - OverheadBarSize.X * 0.5f, Projected.Y, OverheadBarSize.X, OverheadBarSize.Y,
			FLinearColor(0.85f, 0.12f, 0.12f, 0.9f));
	}
}

void ABattleHUD::DrawHealthBar(float HealthRatio, float X, float Y, float W, float H, FLinearColor FillColor)
{
	// 底槽 + 药水填充
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), X - 2.0f, Y - 2.0f, W + 4.0f, H + 4.0f);
	DrawRect(FLinearColor(0.08f, 0.08f, 0.08f, 0.8f), X, Y, W, H);
	if (HealthRatio > 0.0f)
	{
		DrawRect(FillColor, X, Y, W * HealthRatio, H);
	}
}
