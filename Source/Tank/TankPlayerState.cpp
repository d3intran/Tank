#include "TankPlayerState.h"
#include "Tank.h"
#include "Net/UnrealNetwork.h"

ATankPlayerState::ATankPlayerState()
{
	// 计分板数据量极小，不需要高频更新
	SetNetUpdateFrequency(10.0f);
}

void ATankPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 无 COND_ 条件：击杀数是公开信息，所有端（含非本人客户端）都要能看到计分板
	DOREPLIFETIME(ATankPlayerState, Kills);
	DOREPLIFETIME(ATankPlayerState, Deaths);
}

void ATankPlayerState::AddKill()
{
	if (!HasAuthority())
	{
		// 客户端本地改了也会被下一次复制覆盖，直接挡掉更省事
		return;
	}
	++Kills;
	OnRep_Kills(); // 服务器本地不会走 OnRep，手动广播一次让主机 HUD 立即刷新
}

void ATankPlayerState::AddDeath()
{
	if (!HasAuthority())
	{
		return;
	}
	++Deaths;
	OnRep_Deaths();
}

void ATankPlayerState::ResetScore()
{
	if (!HasAuthority())
	{
		return;
	}
	Kills = 0;
	Deaths = 0;
	OnRep_Kills();
	OnRep_Deaths();
}

void ATankPlayerState::OnRep_Kills()
{
	OnScoreChanged.Broadcast();
}

void ATankPlayerState::OnRep_Deaths()
{
	OnScoreChanged.Broadcast();
}
