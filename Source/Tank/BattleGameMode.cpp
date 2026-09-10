#include "BattleGameMode.h"
#include "Tank.h"
#include "TankPawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

ABattleGameMode::ABattleGameMode()
{
	DefaultPawnClass = ATankPawn::StaticClass();
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
	// 三段式巡检（PIE 多开下主机玩家会被引擎解除占有，且孤儿车会堵死出生点）：
	// 1) 占有脱钩（PC 有 Pawn 引用但 Pawn->Controller 已空）→ 直接 Possess 收编，免重生
	// 2) 彻底无 Pawn → 标准 RestartPlayer 补发（配合下方 SpawnDefaultPawnFor 的 Adjust 碰撞处理）
	// 3) 清理无主 Pawn，防止孤儿车越积越多
	TArray<APlayerController*> PCs;
	for (TActorIterator<APlayerController> It(GetWorld()); It; ++It)
	{
		if (*It && !(*It)->IsPendingKillPending())
		{
			PCs.Add(*It);
		}
	}

	for (APlayerController* PC : PCs)
	{
		APawn* P = PC->GetPawn();
		if (P && P->GetController() != PC)
		{
			UE_LOG(LogTank, Warning, TEXT("[Battle] %s 占有脱钩，收编孤儿 %s"),
				*PC->GetHumanReadableName(), *P->GetName());
			PC->Possess(P);
		}
	}

	for (APlayerController* PC : PCs)
	{
		if (!PC->GetPawn())
		{
			UE_LOG(LogTank, Warning, TEXT("[Battle] %s 无 Pawn，补发"), *PC->GetHumanReadableName());
			RestartPlayer(PC);
		}
	}

	TArray<APawn*> Orphans;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (*It && !(*It)->IsPendingKillPending() && It->GetController() == nullptr)
		{
			Orphans.Add(*It);
		}
	}
	for (APawn* Orphan : Orphans)
	{
		UE_LOG(LogTank, Warning, TEXT("[Battle] 清理无主 Pawn %s"), *Orphan->GetName());
		Orphan->Destroy();
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
	// 轮转分配：ChoosePlayerStart 在 Login 期调用，此时 GetNumPlayers() 尚未含新玩家，
	// 按已入场人数取模保证三人各占一点——引擎默认按评分选点会重复选同一点导致坦克叠罗汉
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Starts.Add(*It);
	}
	if (Starts.Num() == 0)
	{
		return Super::ChoosePlayerStart(Player);
	}
	return Starts[GetNumPlayers() % Starts.Num()];
}
