#include "TankVehicle.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Tank.h"
#include "TimerManager.h"

// ============================================================================
// Enhanced Input 绑定与输入回调
// ============================================================================
void ATankVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveForwardAction)  { Input->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &ATankVehicle::MoveForward); }
		if (MoveBackwardAction) { Input->BindAction(MoveBackwardAction, ETriggerEvent::Triggered, this, &ATankVehicle::MoveBackward); }
		if (TurnRightAction)    { Input->BindAction(TurnRightAction, ETriggerEvent::Triggered, this, &ATankVehicle::TurnRight); }
		if (TurnLeftAction)     { Input->BindAction(TurnLeftAction, ETriggerEvent::Triggered, this, &ATankVehicle::TurnLeft); }
		if (TurretCWAction)     { Input->BindAction(TurretCWAction, ETriggerEvent::Triggered, this, &ATankVehicle::TurretCW); }
		if (TurretCCWAction)    { Input->BindAction(TurretCCWAction, ETriggerEvent::Triggered, this, &ATankVehicle::TurretCCW); }
		if (CameraYawAction)    { Input->BindAction(CameraYawAction, ETriggerEvent::Triggered, this, &ATankVehicle::OrbitCamera); }
		if (GunPitchAction)     { Input->BindAction(GunPitchAction, ETriggerEvent::Triggered, this, &ATankVehicle::ElevateGun); }
		if (FireAction)         { Input->BindAction(FireAction, ETriggerEvent::Triggered, this, &ATankVehicle::Fire); }
	}

	AddDefaultMappingContext();
}

void ATankVehicle::MoveForward()
{
	CurrentThrottleInput = FMath::Min(CurrentThrottleInput + 1.0f, 1.0f);
	if (!bLoggedInputReceived)
	{
		bLoggedInputReceived = true;
		UE_LOG(LogTank, Log, TEXT("[Vehicle] %s 收到驱动输入（W）：油门=%.0f"), *GetName(), CurrentThrottleInput);
	}
}

void ATankVehicle::MoveBackward()
{
	CurrentThrottleInput = FMath::Max(CurrentThrottleInput - 1.0f, -1.0f);
}

void ATankVehicle::TurnRight()
{
	CurrentSteerInput = FMath::Min(CurrentSteerInput + 1.0f, 1.0f);
}

void ATankVehicle::TurnLeft()
{
	CurrentSteerInput = FMath::Max(CurrentSteerInput - 1.0f, -1.0f);
}

void ATankVehicle::TurretCW()
{
	CurrentTurretRotateInput = 1.0f;
}

void ATankVehicle::TurretCCW()
{
	CurrentTurretRotateInput = -1.0f;
}

void ATankVehicle::OrbitCamera(const FInputActionValue& Value)
{
	CameraRelativeYaw = FRotator::NormalizeAxis(CameraRelativeYaw + Value.Get<float>() * CameraSensitivityX);
}

void ATankVehicle::ElevateGun(const FInputActionValue& Value)
{
	const float Delta = Value.Get<float>() * PitchSensitivity * (bInvertPitch ? -1.0f : 1.0f);
	DesiredGunPitch = FMath::Clamp(DesiredGunPitch + Delta, MinPitch, MaxPitch);
}

void ATankVehicle::AddDefaultMappingContext()
{
	if (bMappingContextAdded || !DefaultMappingContext)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
		bMappingContextAdded = true;
	}
}

// ============================================================================
// 网络生命周期与占有处理
// ============================================================================
void ATankVehicle::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	AddDefaultMappingContext();
	EnsureClientReady();
}

void ATankVehicle::UnPossessed()
{
	Super::UnPossessed();
	bClientReady = false;
	bMappingContextAdded = false;
}

void ATankVehicle::OnRep_Controller()
{
	Super::OnRep_Controller();
	AddDefaultMappingContext();
	EnsureClientReady();
}

void ATankVehicle::PostNetReceive()
{
	Super::PostNetReceive();
	EnsureClientReady();
	if (!TickRepairTimerHandle.IsValid())
	{
		GetWorldTimerManager().SetTimer(TickRepairTimerHandle, this, &ATankVehicle::RepairTickTimer, 1.0f, true);
	}
}

void ATankVehicle::RepairTickTimer()
{
	EnsureClientReady();
}

// ============================================================================
// 客户端两段式自愈（Tick 全局自愈 + 本机输入组件自愈）
// ============================================================================
void ATankVehicle::EnsureClientReady()
{
	if (HasAuthority())
	{
		return;
	}

	// ---------- 1. Tick 自愈：客户端世界里的「每一辆」坦克都要做 ----------
	{
		const bool bWasRegistered = PrimaryActorTick.IsTickFunctionRegistered();
		const bool bWasEnabled = IsActorTickEnabled();
		if (!bWasRegistered)
		{
			RegisterAllActorTickFunctions(true, /*bDoComponents=*/false);
		}
		if (!bWasEnabled)
		{
			SetActorTickEnabled(true);
		}
		if (!bWasRegistered || !bWasEnabled)
		{
			UE_LOG(LogTank, Log, TEXT("[Net] %s Tick 自愈：注册 %d→%d / 启用 %d→%d（Role=%d）"),
				*GetName(),
				bWasRegistered ? 1 : 0, PrimaryActorTick.IsTickFunctionRegistered() ? 1 : 0,
				bWasEnabled ? 1 : 0, IsActorTickEnabled() ? 1 : 0,
				(int32)GetLocalRole());
		}
	}

	if (bClientReady)
	{
		return;
	}

	// ---------- 2. 输入自愈：只给「本机玩家的本机 Pawn」 ----------
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	if (!ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		return;
	}
	bClientReady = true;

	const bool bHadContext = bMappingContextAdded;
	bool bCreatedInput = false;
	bool bBoundInput = false;

	if (InputComponent == nullptr)
	{
		InputComponent = CreatePlayerInputComponent();
		if (InputComponent)
		{
			InputComponent->RegisterComponent();
			bCreatedInput = true;
		}
	}
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (EnhancedInput->GetActionEventBindings().Num() == 0)
		{
			SetupPlayerInputComponent(InputComponent);
			bBoundInput = true;
		}
	}

	AddDefaultMappingContext();

	UE_LOG(LogTank, Log, TEXT("[Input] %s 输入自愈：输入组件%s，动作绑定%s，IMC%s（Role=%d）"),
		*GetName(),
		bCreatedInput ? TEXT("补建") : TEXT("已在"),
		bBoundInput ? TEXT("补绑") : TEXT("已在"),
		bHadContext ? TEXT("已挂") : TEXT("新挂"),
		(int32)GetLocalRole());
}

void ATankVehicle::OnRep_NetTurretYaw()
{
	EnsureClientReady();
	if (!IsLocallyControlled())
	{
		if (FMath::Abs(CurrentTurretYaw) < 0.01f && FMath::Abs(NetTurretYaw) > 1.0f)
		{
			CurrentTurretYaw = NetTurretYaw;
			if (TurretPivot)
			{
				TurretPivot->SetRelativeRotation(FRotator(0.0f, CurrentTurretYaw, 0.0f));
			}
		}
	}
}

void ATankVehicle::OnRep_NetGunPitch()
{
	EnsureClientReady();
	if (!IsLocallyControlled())
	{
		if (FMath::Abs(CurrentPitch) < 0.01f && FMath::Abs(NetGunPitch) > 1.0f)
		{
			CurrentPitch = NetGunPitch;
			if (GunPivot)
			{
				GunPivot->SetRelativeRotation(FRotator(CurrentPitch, 0.0f, 0.0f));
			}
		}
	}
}

// ============================================================================
// 驱动指令网络 RPC
// ============================================================================
bool ATankVehicle::ServerUpdateDriveInput_Validate(float InThrottle, float InSteer, float InTurretYaw, float InGunPitch)
{
	return !FMath::IsNaN(InThrottle) && !FMath::IsNaN(InSteer) && !FMath::IsNaN(InTurretYaw) && !FMath::IsNaN(InGunPitch);
}

void ATankVehicle::ServerUpdateDriveInput_Implementation(float InThrottle, float InSteer, float InTurretYaw, float InGunPitch)
{
	ReplicatedThrottleInput = FMath::Clamp(InThrottle, -1.0f, 1.0f);
	ReplicatedSteerInput = FMath::Clamp(InSteer, -1.0f, 1.0f);
	NetTurretYaw = InTurretYaw;
	NetGunPitch = InGunPitch;
	LastDriveInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}
