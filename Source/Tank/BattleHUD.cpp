#include "BattleHUD.h"
#include "Tank.h"
#include "TankPawn.h"
#include "TankHealth.h"
#include "TankPlayerState.h"
#include "TankGameState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace
{
	// 低血呼吸闪烁的频率（Hz）
	constexpr float LowHealthBreathHz = 1.2f;
}

void ABattleHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GetWorld())
	{
		return;
	}

	// 本机坦克：PC->GetPawn 在重生瞬间为空，正好用来区分「活着」和「等待重生」
	ATankPawn* MyTank = nullptr;
	if (APlayerController* PC = GetOwningPlayerController())
	{
		MyTank = Cast<ATankPawn>(PC->GetPawn());
	}

	if (MyTank)
	{
		DrawLocalStatus(MyTank);
	}
	else
	{
		DrawDeathNotice();
	}

	DrawOverheadBars(MyTank);
	DrawScoreboard();
	DrawVictoryPanel();
}

void ABattleHUD::DrawBar(float Ratio, float X, float Y, float W, float H, FLinearColor FillColor)
{
	// 底槽 + 填充
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), X - 2.0f, Y - 2.0f, W + 4.0f, H + 4.0f);
	DrawRect(FLinearColor(0.08f, 0.08f, 0.08f, 0.8f), X, Y, W, H);
	if (Ratio > 0.0f)
	{
		DrawRect(FillColor, X, Y, W * FMath::Clamp(Ratio, 0.0f, 1.0f), H);
	}
}

void ABattleHUD::DrawLocalStatus(ATankPawn* MyTank)
{
	UTankHealth* Health = MyTank->FindComponentByClass<UTankHealth>();
	if (!Health)
	{
		return;
	}

	const float BarX = 40.0f;
	const float BarY = Canvas->SizeY - OwnBarSize.Y - 40.0f;

	// ---- 血条：分段变色，低血呼吸闪烁 ----
	const float Ratio = FMath::Clamp(Health->GetCurrentHealth() / FMath::Max(Health->GetMaxHealth(), 1.0f), 0.0f, 1.0f);

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
		// alpha 在 0.35~1.0 间正弦起伏
		const float Breath = 0.675f + 0.325f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.0f * PI * LowHealthBreathHz);
		Color = FLinearColor(0.85f, 0.12f, 0.12f, Breath);
	}
	DrawBar(Ratio, BarX, BarY, OwnBarSize.X, OwnBarSize.Y, Color);

	UFont* SmallFont = GEngine ? GEngine->GetSmallFont() : nullptr;
	DrawText(FString::Printf(TEXT("HP %.0f / %.0f"), Health->GetCurrentHealth(), Health->GetMaxHealth()),
		FLinearColor::White, BarX, BarY - 20.0f, SmallFont, 1.0f);

	// ---- 装填进度：GetReloadProgress 返回 0(刚开火)~1(可开火) ----
	const float ReloadY = BarY + OwnBarSize.Y + 10.0f;
	const float ReloadRatio = MyTank->GetReloadProgress();
	const bool bReady = ReloadRatio >= 1.0f;
	const FLinearColor ReloadColor = bReady
		? FLinearColor(0.25f, 0.65f, 0.95f, 0.9f)
		: FLinearColor(0.55f, 0.45f, 0.20f, 0.9f);
	DrawBar(ReloadRatio, BarX, ReloadY, ReloadBarSize.X, ReloadBarSize.Y, ReloadColor);
	DrawText(bReady ? TEXT("主炮就绪") : TEXT("装填中…"),
		bReady ? FLinearColor(0.4f, 0.85f, 1.0f) : FLinearColor(0.8f, 0.75f, 0.5f),
		BarX, ReloadY + ReloadBarSize.Y + 4.0f, SmallFont, 1.0f);

	// ---- 重生保护提示 ----
	if (MyTank->IsSpawnProtected())
	{
		DrawCenteredText(TEXT("重生保护中（免疫伤害）"), Canvas->SizeY * 0.32f,
			FLinearColor(0.35f, 0.85f, 1.0f, 0.95f), GEngine ? GEngine->GetMediumFont() : nullptr, 1.0f);
	}
}

void ABattleHUD::DrawOverheadBars(ATankPawn* MyTank)
{
	APlayerController* LocalPC = GetOwningPlayerController();
	if (!LocalPC)
	{
		return;
	}

	// 相机视点整帧不变：循环外取一次，既做遮挡射线起点，也省掉每个目标一次重复查询
	FVector ViewLoc;
	FRotator ViewRot;
	LocalPC->GetPlayerViewPoint(ViewLoc, ViewRot);

	// 其他玩家坦克 → 头顶红色血条（>80m 距离剔除），并做遮挡剔除
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

		// UCanvas::Project 返回的 Z 是裁剪空间 NDC 深度（= clipZ/W），并且只在该点位于
		// 相机身后（V.W <= 0）时才被 bClampToZeroPlane 夹成 0（引擎原文注释：
		// "if behind the screen, clamp depth to the screen"）。
		// 所以判据是 Z > 0（在相机前方），原来写的 Z == 0 恰好反了——
		// 那只会在坦克跑到身后时才通过，正前方的坦克反而永远画不出头顶血条。
		const FVector BarWorldLoc = TankLoc + FVector(0.0f, 0.0f, 240.0f);
		const FVector Projected = Canvas->Project(BarWorldLoc);
		const bool bOnScreen = Projected.Z > 0.0f
			&& Projected.X > -OverheadBarSize.X && Projected.X < Canvas->SizeX + OverheadBarSize.X
			&& Projected.Y > 0.0f && Projected.Y < Canvas->SizeY;
		if (!bOnScreen)
		{
			continue;
		}

		// 遮挡剔除：Canvas 直绘没有深度测试，不测就会「透视」显示墙后坦克的血条。
		// 从相机沿视线打到血条锚点，命中建筑/掩体就不画。只对已通过筛选的目标做，开销可忽略。
		FCollisionQueryParams OcclusionParams(TEXT("OverheadBarOcclusion"), /*bTraceComplex=*/false, LocalPC->GetPawn());
		OcclusionParams.AddIgnoredActor(Tank);
		if (MyTank)
		{
			OcclusionParams.AddIgnoredActor(MyTank);
		}

		FHitResult OcclusionHit;
		if (GetWorld()->LineTraceSingleByChannel(OcclusionHit, ViewLoc, BarWorldLoc, ECC_Visibility, OcclusionParams))
		{
			continue;
		}

		const float Ratio = FMath::Clamp(Health->GetCurrentHealth() / FMath::Max(Health->GetMaxHealth(), 1.0f), 0.0f, 1.0f);
		DrawBar(Ratio, Projected.X - OverheadBarSize.X * 0.5f, Projected.Y, OverheadBarSize.X, OverheadBarSize.Y,
			FLinearColor(0.85f, 0.12f, 0.12f, 0.9f));
	}
}

void ABattleHUD::DrawScoreboard()
{
	AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS)
	{
		return;
	}

	// 数据源：PlayerArray 里的 TankPlayerState（击杀/死亡都是复制值，全端一致）
	TArray<ATankPlayerState*> Rows;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (ATankPlayerState* TPS = Cast<ATankPlayerState>(PS))
		{
			Rows.Add(TPS);
		}
	}
	if (Rows.Num() == 0)
	{
		return;
	}

	// 击杀降序；同分按死亡数升序（死得少排前面）
	Rows.Sort([](const ATankPlayerState& A, const ATankPlayerState& B)
	{
		if (A.GetKills() != B.GetKills())
		{
			return A.GetKills() > B.GetKills();
		}
		return A.GetDeaths() < B.GetDeaths();
	});

	APlayerController* LocalPC = GetOwningPlayerController();
	ATankPlayerState* MyPS = LocalPC ? Cast<ATankPlayerState>(LocalPC->PlayerState) : nullptr;

	const int32 KillsToWin = [this]()
	{
		if (ATankGameState* TankGS = GetWorld()->GetGameState<ATankGameState>())
		{
			return TankGS->GetKillsToWin();
		}
		return 10;
	}();

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;

	const float X = Canvas->SizeX - ScoreboardWidth - ScoreboardMargin;
	float Y = ScoreboardMargin;

	// 底板
	const float PanelH = ScoreboardRowHeight * (Rows.Num() + 1) + 14.0f;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), X - 8.0f, Y - 8.0f, ScoreboardWidth + 16.0f, PanelH);

	DrawText(FString::Printf(TEXT("FFA 个人战   先到 %d 杀"), KillsToWin),
		FLinearColor(1.0f, 0.9f, 0.5f), X, Y, Font, 1.1f);
	Y += ScoreboardRowHeight + 4.0f;

	for (ATankPlayerState* PS : Rows)
	{
		const bool bIsMe = (PS == MyPS);
		const FLinearColor RowColor = bIsMe
			? FLinearColor(0.45f, 0.95f, 1.0f)
			: FLinearColor(0.85f, 0.85f, 0.85f);

		if (bIsMe)
		{
			DrawRect(FLinearColor(0.1f, 0.35f, 0.45f, 0.45f), X - 6.0f, Y - 1.0f, ScoreboardWidth + 12.0f, ScoreboardRowHeight);
		}

		DrawText(PS->GetPlayerName(), RowColor, X, Y, Font, 1.0f);
		DrawText(FString::Printf(TEXT("%d 杀 / %d 死"), PS->GetKills(), PS->GetDeaths()),
			RowColor, X + ScoreboardWidth - 110.0f, Y, Font, 1.0f);

		Y += ScoreboardRowHeight;
	}
}

void ABattleHUD::DrawDeathNotice()
{
	UFont* BigFont = GEngine ? GEngine->GetLargeFont() : nullptr;
	UFont* SmallFont = GEngine ? GEngine->GetSmallFont() : nullptr;

	DrawCenteredText(TEXT("坦克被击毁"), Canvas->SizeY * 0.40f,
		FLinearColor(1.0f, 0.3f, 0.25f, 0.95f), BigFont, 1.2f);
	DrawCenteredText(TEXT("正在重生…"), Canvas->SizeY * 0.40f + 42.0f,
		FLinearColor(0.9f, 0.9f, 0.9f, 0.9f), SmallFont, 1.0f);
}

void ABattleHUD::DrawVictoryPanel()
{
	ATankGameState* TankGS = GetWorld()->GetGameState<ATankGameState>();
	if (!TankGS || !TankGS->IsMatchOver())
	{
		return;
	}

	UFont* BigFont = GEngine ? GEngine->GetLargeFont() : nullptr;
	UFont* SmallFont = GEngine ? GEngine->GetSmallFont() : nullptr;

	// 半透明压暗底，突出面板
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), 0.0f, Canvas->SizeY * 0.28f, Canvas->SizeX, Canvas->SizeY * 0.28f);

	// 本机是否赢家（PlayerState 比对，别用名字字符串比）
	const APlayerController* LocalPC = GetOwningPlayerController();
	const FString MyName = (LocalPC && LocalPC->PlayerState) ? LocalPC->PlayerState->GetPlayerName() : FString();
	const bool bIWin = !MyName.IsEmpty() && MyName == TankGS->GetWinnerName();

	DrawCenteredText(bIWin ? TEXT("胜利！") : TEXT("回合结束"),
		Canvas->SizeY * 0.31f,
		bIWin ? FLinearColor(0.4f, 1.0f, 0.45f, 1.0f) : FLinearColor(1.0f, 0.85f, 0.35f, 1.0f),
		BigFont, 1.3f);

	DrawCenteredText(FString::Printf(TEXT("赢家：%s"), *TankGS->GetWinnerName()),
		Canvas->SizeY * 0.31f + 46.0f, FLinearColor::White, SmallFont, 1.1f);
	DrawCenteredText(TEXT("回合即将重开…"),
		Canvas->SizeY * 0.31f + 74.0f, FLinearColor(0.85f, 0.85f, 0.85f, 0.9f), SmallFont, 1.0f);
}

void ABattleHUD::DrawCenteredText(const FString& Text, float CenterY, const FLinearColor& Color, UFont* Font, float Scale)
{
	float W = 0.0f;
	float H = 0.0f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, (Canvas->SizeX - W) * 0.5f, CenterY, Font, Scale);
}
