#include "BattleGameMode.h"
#include "Tank.h"
#include "TankPawn.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"

ABattleGameMode::ABattleGameMode()
{
	DefaultPawnClass = ATankPawn::StaticClass();
}

void ABattleGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	UE_LOG(LogTank, Log, TEXT("[Battle] Player joined: %s（当前 %d 人）"),
		*NewPlayer->GetHumanReadableName(), GetNumPlayers());
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
