#include "BattleGameMode.h"
#include "Tank.h"
#include "TankPawn.h"
#include "BattleHUD.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

ABattleGameMode::ABattleGameMode()
{
	DefaultPawnClass = ATankPawn::StaticClass();
	HUDClass = ABattleHUD::StaticClass();
}

void ABattleGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 占有巡检：PIE 多开下主机玩家的 PlayerController 会被引擎重建，初次占有丢失且不走 PostLogin。
	// 定时兜底（引擎标准 RestartPlayer 自带重挂孤儿 Pawn 的能力），真机 dedicated server 上此巡检恒为空转
	GetWorldTimerManager().SetTimer(HealTimerHandle, this,
		&ABattleGameMode::EnsureAllPlayersHavePawns, 2.0f, true);
	EnsureAllPlayersHavePawns();
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
	for (TActorIterator<ATankPawn> It(GetWorld()); It; ++It)
	{
		ATankPawn* Tank = *It;
		if (Tank && Tank->GetController() && Tank->GetController() != Player)
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
