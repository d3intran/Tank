#include "BattleHUD.h"
#include "Tank.h"
#include "TankPawn.h"
#include "TankVehicle.h"
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
	APawn* MyTank = nullptr;
	if (APlayerController* PC = GetOwningPlayerController())
	{
		MyTank = PC->GetPawn();
	}

	if (MyTank)
	{
		DrawLocalStatus(MyTank);
		DrawAimReticle(MyTank);
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

void ABattleHUD::DrawLocalStatus(APawn* MyTank)
{
	if (!MyTank)
	{
		return;
	}

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

	// ---- 装填进度与重生保护 ----
	float ReloadRatio = 1.0f;
	bool bSpawnProtected = false;
	if (ATankPawn* PawnTank = Cast<ATankPawn>(MyTank))
	{
		ReloadRatio = PawnTank->GetReloadProgress();
		bSpawnProtected = PawnTank->IsSpawnProtected();
	}
	else if (ATankVehicle* VehicleTank = Cast<ATankVehicle>(MyTank))
	{
		ReloadRatio = VehicleTank->GetReloadProgress();
		bSpawnProtected = VehicleTank->IsSpawnProtected();
	}

	const float ReloadY = BarY + OwnBarSize.Y + 10.0f;
	const bool bReady = ReloadRatio >= 1.0f;
	const FLinearColor ReloadColor = bReady
		? FLinearColor(0.25f, 0.65f, 0.95f, 0.9f)
		: FLinearColor(0.55f, 0.45f, 0.20f, 0.9f);
	DrawBar(ReloadRatio, BarX, ReloadY, ReloadBarSize.X, ReloadBarSize.Y, ReloadColor);
	DrawText(bReady ? TEXT("主炮就绪") : TEXT("装填中…"),
		bReady ? FLinearColor(0.4f, 0.85f, 1.0f) : FLinearColor(0.8f, 0.75f, 0.5f),
		BarX, ReloadY + ReloadBarSize.Y + 4.0f, SmallFont, 1.0f);

	// ---- 重生保护提示 ----
	if (bSpawnProtected)
	{
		DrawCenteredText(TEXT("重生保护中（免疫伤害）"), Canvas->SizeY * 0.32f,
			FLinearColor(0.35f, 0.85f, 1.0f, 0.95f), GEngine ? GEngine->GetMediumFont() : nullptr, 1.0f);
	}
}

void ABattleHUD::DrawCircle2D(float CenterX, float CenterY, float Radius, int32 Segments, FLinearColor Color, float Thickness)
{
	if (Segments < 3 || Radius <= 0.0f)
	{
		return;
	}

	const float AngleStep = 2.0f * PI / static_cast<float>(Segments);
	float PrevX = CenterX + Radius;
	float PrevY = CenterY;

	for (int32 i = 1; i <= Segments; ++i)
	{
		const float Angle = static_cast<float>(i) * AngleStep;
		const float NextX = CenterX + Radius * FMath::Cos(Angle);
		const float NextY = CenterY + Radius * FMath::Sin(Angle);
		DrawLine(PrevX, PrevY, NextX, NextY, Color, Thickness);
		PrevX = NextX;
		PrevY = NextY;
	}
}

void ABattleHUD::DrawAimReticle(APawn* MyTank)
{
	if (!MyTank || !Canvas || !GetWorld())
	{
		return;
	}

	// 1. 获取主炮落点与瞄准状态
	FVector AimPoint = FVector::ZeroVector;
	float AimDistanceCm = 0.0f;
	bool bLockedOnEnemy = false;
	bool bHit = false;

	if (ATankVehicle* VehicleTank = Cast<ATankVehicle>(MyTank))
	{
		const FAimTraceResult& Aim = VehicleTank->GetAimResult();
		AimPoint = Aim.AimPoint;
		AimDistanceCm = Aim.AimDistance;
		bLockedOnEnemy = Aim.bLockedOnEnemy;
		bHit = Aim.bHit;
	}
	else if (ATankPawn* PawnTank = Cast<ATankPawn>(MyTank))
	{
		if (UStaticMeshComponent* Gun = PawnTank->GetGunMesh())
		{
			const FVector MuzzleLoc = Gun->GetComponentTransform().TransformPosition(FVector(200.0f, 0.0f, 0.0f));
			const FVector GunForward = Gun->GetForwardVector();
			const FVector TraceEnd = MuzzleLoc + GunForward * 10000.0f;

			FHitResult Hit;
			FCollisionQueryParams Params(TEXT("PawnGunAimTrace"), false, PawnTank);
			Params.AddIgnoredActor(PawnTank);

			if (GetWorld()->LineTraceSingleByChannel(Hit, MuzzleLoc, TraceEnd, ECC_Visibility, Params))
			{
				AimPoint = Hit.ImpactPoint;
				AimDistanceCm = Hit.Distance;
				AActor* Target = Hit.GetActor();
				bLockedOnEnemy = (Target != nullptr && Target != PawnTank && Target->IsA<APawn>());
				bHit = true;
			}
			else
			{
				AimPoint = TraceEnd;
				AimDistanceCm = 10000.0f;
				bHit = false;
			}
		}
	}

	if (AimPoint.IsNearlyZero())
	{
		return;
	}

	// 2. 投影主炮落点到屏幕空间
	const FVector Projected = Canvas->Project(AimPoint);
	const bool bValidProjection = Projected.Z > 0.0f;
	const FVector2D ScreenCenter(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);

	// 3. 绘制屏幕中心视线基准白十字（玩家视线中心基准）
	{
		const float Gap = 4.0f;
		const float Len = 8.0f;
		const FLinearColor WhiteCross(1.0f, 1.0f, 1.0f, 0.75f);
		DrawLine(ScreenCenter.X - Gap - Len, ScreenCenter.Y, ScreenCenter.X - Gap, ScreenCenter.Y, WhiteCross, 1.5f);
		DrawLine(ScreenCenter.X + Gap, ScreenCenter.Y, ScreenCenter.X + Gap + Len, ScreenCenter.Y, WhiteCross, 1.5f);
		DrawLine(ScreenCenter.X, ScreenCenter.Y - Gap - Len, ScreenCenter.X, ScreenCenter.Y - Gap, WhiteCross, 1.5f);
		DrawLine(ScreenCenter.X, ScreenCenter.Y + Gap, ScreenCenter.X, ScreenCenter.Y + Gap + Len, WhiteCross, 1.5f);
		DrawRect(WhiteCross, ScreenCenter.X - 1.0f, ScreenCenter.Y - 1.0f, 2.0f, 2.0f);
	}

	if (!bValidProjection)
	{
		return;
	}

	// 4. 计算伺服追赶与收敛状态（像素距离）
	const FVector2D AimScreen(Projected.X, Projected.Y);
	const float PixelDist = FVector2D::Distance(ScreenCenter, AimScreen);
	const bool bConverged = PixelDist < AimConvergenceThreshold;

	// 5. 状态分级颜色与动态散布圆环半径
	FLinearColor ReticleColor;
	float DynamicRadius = AimReticleBaseRadius;

	if (bLockedOnEnemy)
	{
		// 敌方载具锁定：高亮警示纯红
		ReticleColor = FLinearColor(1.0f, 0.15f, 0.15f, 0.95f);
		DynamicRadius = AimReticleBaseRadius * 1.15f;
	}
	else if (bConverged)
	{
		// 伺服转到位：荧光绿
		ReticleColor = FLinearColor(0.2f, 1.0f, 0.35f, 0.95f);
		DynamicRadius = AimReticleBaseRadius;
	}
	else
	{
		// 炮塔伺服追赶中：青蓝，伴随动态扩圈（模拟主炮晃动/未稳定）
		ReticleColor = FLinearColor(0.25f, 0.85f, 1.0f, 0.85f);
		DynamicRadius = FMath::Clamp(AimReticleBaseRadius + PixelDist * 0.05f, AimReticleBaseRadius, 24.0f);

		// 牵引引导线：从视线中心指向主炮物理落点
		DrawLine(ScreenCenter.X, ScreenCenter.Y, AimScreen.X, AimScreen.Y, FLinearColor(0.25f, 0.85f, 1.0f, 0.35f), 1.0f);
	}

	// 6. 绘制动态主炮落点圆环与 4 向角标
	DrawCircle2D(AimScreen.X, AimScreen.Y, DynamicRadius, 24, ReticleColor, 1.6f);
	DrawLine(AimScreen.X - DynamicRadius - 5.0f, AimScreen.Y, AimScreen.X - DynamicRadius - 1.0f, AimScreen.Y, ReticleColor, 1.6f);
	DrawLine(AimScreen.X + DynamicRadius + 1.0f, AimScreen.Y, AimScreen.X + DynamicRadius + 5.0f, AimScreen.Y, ReticleColor, 1.6f);
	DrawLine(AimScreen.X, AimScreen.Y - DynamicRadius - 5.0f, AimScreen.X, AimScreen.Y - DynamicRadius - 1.0f, ReticleColor, 1.6f);
	DrawLine(AimScreen.X, AimScreen.Y + DynamicRadius + 1.0f, AimScreen.X, AimScreen.Y + DynamicRadius + 5.0f, ReticleColor, 1.6f);
	DrawRect(ReticleColor, AimScreen.X - 1.0f, AimScreen.Y - 1.0f, 2.0f, 2.0f);

	// 7. 测距与锁定状态文本（带阴影）
	UFont* SmallFont = GEngine ? GEngine->GetSmallFont() : nullptr;
	const float DistM = AimDistanceCm * 0.01f;
	FString DistText;
	if (bLockedOnEnemy)
	{
		DistText = FString::Printf(TEXT("[锁定 %.0fm]"), DistM);
	}
	else
	{
		DistText = FString::Printf(TEXT("%.0fm"), DistM);
	}

	const float TextX = AimScreen.X + DynamicRadius + 8.0f;
	const float TextY = AimScreen.Y - 6.0f;
	DrawText(DistText, FLinearColor(0.0f, 0.0f, 0.0f, 0.85f), TextX + 1.0f, TextY + 1.0f, SmallFont, 1.0f);
	DrawText(DistText, ReticleColor, TextX, TextY, SmallFont, 1.0f);
}

void ABattleHUD::DrawOverheadBars(APawn* MyTank)
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
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Tank = *It;
		// 用 Controller 归属跳过自己的坦克（PC->GetPawn 在重生瞬间为空，指针比对会漏判）
		if (!Tank || Tank->IsPendingKillPending() || !Tank->GetController() || Tank->GetController() == LocalPC)
		{
			continue;
		}
		if (!Tank->IsA<ATankPawn>() && !Tank->IsA<ATankVehicle>())
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

		float HalfHeight = 60.0f;
		if (ATankPawn* PawnTank = Cast<ATankPawn>(Tank))
		{
			HalfHeight = PawnTank->GetSimpleCollisionHalfHeight();
		}
		else if (ATankVehicle* VehicleTank = Cast<ATankVehicle>(Tank))
		{
			HalfHeight = VehicleTank->GetBodyHalfHeight();
		}

		const float BarAnchorZ = HalfHeight * 2.0f + 8.0f;
		const FVector BarWorldLoc = TankLoc + FVector(0.0f, 0.0f, BarAnchorZ);

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

		// 名牌画在血条上方；名字与血条同生共死，所以沿用同一套投影与遮挡判据，不单独再测一次射线
		const FString PlayerName = ResolvePlayerName(Tank);
		if (!PlayerName.IsEmpty())
		{
			DrawOverheadName(PlayerName, Projected.X, Projected.Y);
		}
	}
}

FString ABattleHUD::ResolvePlayerName(const APawn* Tank)
{
	const AController* Ctrl = Tank ? Tank->GetController() : nullptr;
	const APlayerState* PS = Ctrl ? Ctrl->PlayerState : nullptr;
	return PS ? PS->GetPlayerName() : FString();
}

void ABattleHUD::DrawOverheadName(const FString& PlayerName, float CenterX, float BarTopY)
{
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	float TextW = 0.0f;
	float TextH = 0.0f;
	GetTextSize(PlayerName, TextW, TextH, Font, 1.0f);

	const float TextX = CenterX - TextW * 0.5f;
	const float TextY = BarTopY - TextH - 3.0f;

	// 半透明底衬：Canvas 直绘没有描边，白字压在天空/浅色地面上会糊成一片
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), TextX - 4.0f, TextY - 1.0f, TextW + 8.0f, TextH + 2.0f);

	// 四向各偏 1px 画深色，等效描边（比再加一个字体资产省事，灰盒阶段够用）
	const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.75f);
	DrawText(PlayerName, Shadow, TextX - 1.0f, TextY, Font, 1.0f);
	DrawText(PlayerName, Shadow, TextX + 1.0f, TextY, Font, 1.0f);
	DrawText(PlayerName, Shadow, TextX, TextY - 1.0f, Font, 1.0f);
	DrawText(PlayerName, Shadow, TextX, TextY + 1.0f, Font, 1.0f);
	DrawText(PlayerName, FLinearColor(1.0f, 1.0f, 1.0f, 0.95f), TextX, TextY, Font, 1.0f);
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
