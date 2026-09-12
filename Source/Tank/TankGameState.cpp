#include "TankGameState.h"
#include "Tank.h"
#include "Net/UnrealNetwork.h"

ATankGameState::ATankGameState()
{
	// 回合状态变化极少，低频复制足够
	SetNetUpdateFrequency(5.0f);
}

void ATankGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATankGameState, bMatchOver);
	DOREPLIFETIME(ATankGameState, WinnerName);
	DOREPLIFETIME(ATankGameState, KillsToWin);
}

void ATankGameState::SetMatchOver(const FString& InWinnerName)
{
	if (!HasAuthority())
	{
		return;
	}
	bMatchOver = true;
	WinnerName = InWinnerName;
	OnRep_MatchOver();
	UE_LOG(LogTank, Log, TEXT("[Battle] GameState 标记回合结束，赢家 %s"), *WinnerName);
}

void ATankGameState::ClearMatchOver()
{
	if (!HasAuthority())
	{
		return;
	}
	bMatchOver = false;
	WinnerName.Reset();
	OnRep_MatchOver();
}

void ATankGameState::SetKillsToWin(int32 InKillsToWin)
{
	if (!HasAuthority())
	{
		return;
	}
	KillsToWin = FMath::Max(1, InKillsToWin);
}

void ATankGameState::OnRep_MatchOver()
{
	UE_LOG(LogTank, Log, TEXT("[Battle] 回合状态同步：MatchOver=%d 赢家=%s"),
		bMatchOver ? 1 : 0, *WinnerName);
}
