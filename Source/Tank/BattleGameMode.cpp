#include "BattleGameMode.h"
#include "Tank.h"
#include "TankPawn.h"
#include "TankVehicle.h"
#include "BattleHUD.h"
#include "TankPlayerState.h"
#include "TankGameState.h"
#include "TankPlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "EngineUtils.h"

ABattleGameMode::ABattleGameMode()
{
	DefaultPawnClass = ATankPawn::StaticClass();
	HUDClass = ABattleHUD::StaticClass();

	// M3 计分：击杀数放自定义 PlayerState（UE5 的 APlayerState 只有 Score，没有 Kills）
	PlayerStateClass = ATankPlayerState::StaticClass();
	// 回合状态（是否结束/赢家）要复制给客户端，必须放 GameState 而不是 GameMode
	GameStateClass = ATankGameState::StaticClass();
	// 阵亡期间的镜头由 TankPlayerController 接管（无 Pawn 时避免镜头掉到原点朝天）
	PlayerControllerClass = ATankPlayerController::StaticClass();
}

void ABattleGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTank, Log, TEXT("[Battle] 本局载具：%s"), bUseChaosVehicle ? TEXT("Chaos 载具 ATankVehicle") : TEXT("手写地形跟随 ATankPawn"));

	// 把「先到多少杀」同步给客户端 HUD
	if (ATankGameState* GS = GetGameState<ATankGameState>())
	{
		GS->SetKillsToWin(KillsToWin);
	}

	// 占有巡检：PIE 多开下主机玩家的 PlayerController 会被引擎重建，初次占有丢失且不走 PostLogin。
	// 定时兜底（引擎标准 RestartPlayer 自带重挂孤儿 Pawn 的能力），真机 dedicated server 上此巡检恒为空转
	GetWorldTimerManager().SetTimer(HealTimerHandle, this,
		&ABattleGameMode::EnsureAllPlayersHavePawns, 2.0f, true);
	EnsureAllPlayersHavePawns();
}

void ABattleGameMode::NotifyKill(AController* Killer, AController* Victim)
{
	if (!HasAuthority())
	{
		return;
	}

	// 死亡数：无论击杀者是谁都要记
	if (ATankPlayerState* VictimPS = Victim ? Cast<ATankPlayerState>(Victim->PlayerState) : nullptr)
	{
		VictimPS->AddDeath();
	}

	// 自杀 / 无归属（如被环境击杀）不计击杀分
	if (Killer == nullptr || Killer == Victim)
	{
		UE_LOG(LogTank, Log, TEXT("[Battle] 击杀无归属（自杀或环境击杀），不记分"));
		return;
	}

	ATankPlayerState* KillerPS = Cast<ATankPlayerState>(Killer->PlayerState);
	if (!KillerPS)
	{
		return;
	}

	KillerPS->AddKill();
	UE_LOG(LogTank, Log, TEXT("[Battle] %s 击杀 %s（击杀数 %d/%d）"),
		*KillerPS->GetPlayerName(), *GetNameSafe(Victim->PlayerState),
		KillerPS->GetKills(), KillsToWin);

	if (!bMatchOver && KillerPS->GetKills() >= KillsToWin)
	{
		DeclareWinner(KillerPS);
	}
}

void ABattleGameMode::DeclareWinner(ATankPlayerState* Winner)
{
	if (bMatchOver || !Winner)
	{
		return;
	}
	bMatchOver = true;

	UE_LOG(LogTank, Warning, TEXT("[Battle] ===== 回合结束：%s 达成 %d 杀获胜 ====="),
		*Winner->GetPlayerName(), Winner->GetKills());

	// 复制给所有客户端，HUD 据此显示胜利面板
	if (ATankGameState* GS = GetGameState<ATankGameState>())
	{
		GS->SetMatchOver(Winner->GetPlayerName());
	}

	if (RoundRestartDelay <= 0.0f)
	{
		ResetRound();
	}
	else
	{
		GetWorldTimerManager().SetTimer(RoundResetTimerHandle, this,
			&ABattleGameMode::ResetRound, RoundRestartDelay, false);
	}
}

void ABattleGameMode::ResetRound()
{
	UE_LOG(LogTank, Warning, TEXT("[Battle] 回合重置：清零战绩，全体回炉重生"));

	// 1. 清零所有玩家战绩
	if (AGameStateBase* GS = GameState)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (ATankPlayerState* TPS = Cast<ATankPlayerState>(PS))
			{
				TPS->ResetScore();
			}
		}
	}

	// 2. 清掉回合结束标记
	if (ATankGameState* TankGS = GetGameState<ATankGameState>())
	{
		TankGS->ClearMatchOver();
	}
	bMatchOver = false;

	// 3. 全体回炉：销毁现有坦克，让占有巡检在 2s 内按「离存活坦克最远」重新补发。
	//    复用 M2 已端到端验证过的重生链路，不另写一套传送逻辑。
	TArray<APawn*> Tanks;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* P = *It;
		if (P && (P->IsA<ATankPawn>() || P->IsA<ATankVehicle>()))
		{
			Tanks.Add(P);
		}
	}
	for (APawn* Tank : Tanks)
	{
		if (IsValid(Tank))
		{
			// 直接 Destroy 而不是走 HandleDeath——回合重开不该产生击杀分
			Tank->Destroy();
		}
	}
}


void ABattleGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	UE_LOG(LogTank, Log, TEXT("[Battle] Player joined: %s（当前 %d 人）"),
		*NewPlayer->GetHumanReadableName(), GetNumPlayers());
	EnsureAllPlayersHavePawns();
}

void ABattleGameMode::EnsureAllPlayersHavePawns()
{
	// 单段巡检：只对「GetPawn 为空」的玩家调 RestartPlayer。
	// 引擎 RestartPlayerAtPlayerStart 对 GetPawn 非空（占有脱钩）的玩家会直接重新 Possess 旧 Pawn，
	// 自带收编能力——不要跨 PC 抢Possess、不要销毁"无主"Pawn（三段式版本会互相打架：
	// 收编触发对方的 UnPossessed、清理销毁引擎正在重挂的坦克，玩家陷入永久重生循环）
	for (TActorIterator<APlayerController> It(GetWorld()); It; ++It)
	{
		APlayerController* PC = *It;
		if (PC && !PC->IsPendingKillPending() && !PC->GetPawn())
		{
			UE_LOG(LogTank, Warning, TEXT("[Battle] %s 无 Pawn，补发"), *PC->GetHumanReadableName());
			RestartPlayer(PC);
		}
	}
}

UClass* ABattleGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// 载具选型开关（Config/DefaultGame.ini 的 [/Script/Tank.BattleGameMode] 段）：
	// true = Chaos 载具 ATankVehicle，false = 手写地形跟随 ATankPawn。
	return bUseChaosVehicle ? ATankVehicle::StaticClass() : ATankPawn::StaticClass();
}

APawn* ABattleGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// 引擎默认实现用 Default 碰撞处理（出生点被占直接失败且不调整）——孤儿车堵门时补发永远失败。
	// 改为 AdjustIfPossibleButAlwaysSpawn：贴边也要生出来
	if (NewPlayer == nullptr || StartSpot == nullptr)
	{
		return nullptr;
	}
	FRotator StartRotation(ForceInit);
	StartRotation.Yaw = StartSpot->GetActorRotation().Yaw;
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	return GetWorld()->SpawnActor<APawn>(GetDefaultPawnClassForController(NewPlayer),
		StartSpot->GetActorLocation(), StartRotation, SpawnInfo);
}

void ABattleGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	UE_LOG(LogTank, Log, TEXT("[Battle] Player left: %s（剩余 %d）"),
		*Exiting->GetHumanReadableName(), GetNumPlayers());
}

bool ABattleGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	// 【必须覆盖】引擎默认实现在 Player->StartSpot 非空时返回 true，而 Login 时
	// AGameModeBase 已经把首次选中的出生点存进了 PlayerController->StartSpot：
	//     AActor* const StartSpot = FindPlayerStart(Player, Portal);
	//     if (StartSpot != nullptr) { Player->StartSpot = StartSpot; }
	// 于是后续每次 RestartPlayer → FindPlayerStart 都会在这里短路、直接返回首次出生点，
	// ChoosePlayerStart_Implementation（FFA「离所有存活坦克最远」选点）在重生时永远不被调用，
	// 等于所有重生都固定回自己最初的出生点。
	// 返回 false 才能让每次重生都重新走一遍选点逻辑。
	return false;
}

AActor* ABattleGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// FFA 选点：取「距所有存活坦克最远」的出生点。
	// 不可用轮转——固定人数下轮转恒落同一点，重生叠在活坦克上会触发物理穿透解算，
	// 把对方硬挤飞，观感就是"自己被瞬移到重生点"
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Starts.Add(*It);
	}
	if (Starts.Num() == 0)
	{
		return Super::ChoosePlayerStart(Player);
	}

	TArray<FVector> TankLocs;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Tank = *It;
		if (Tank && Tank->GetController() && Tank->GetController() != Player &&
			(Tank->IsA<ATankPawn>() || Tank->IsA<ATankVehicle>()))
		{
			TankLocs.Add(Tank->GetActorLocation());
		}
	}

	APlayerStart* Best = nullptr;
	float BestScore = -1.0f;
	for (APlayerStart* Start : Starts)
	{
		float MinDist = TankLocs.Num() > 0 ? FLT_MAX : 0.0f;
		for (const FVector& Loc : TankLocs)
		{
			MinDist = FMath::Min(MinDist, FVector::Dist2D(Start->GetActorLocation(), Loc));
		}
		if (MinDist > BestScore)
		{
			BestScore = MinDist;
			Best = Start;
		}
	}
	return Best ? Best : Super::ChoosePlayerStart(Player);
}
