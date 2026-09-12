#include "TankVehicleWheel.h"

UTankVehicleWheel::UTankVehicleWheel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 有效半径：轮心到履带底面（网格 41.75 → 世界 20.875）
	WheelRadius = 41.75f * 0.5f;
	WheelWidth = 40.0f;   // 履带接地宽度（世界 cm）
	WheelMass = 150.0f;   // 单轮质量（惯性用；整车质量由 UChaosVehicleMovementComponent::Mass 覆盖）

	// 驱动力矩/制动力矩全部由 ATankVehicle 按侧直接施加
	ExternalTorqueCombineMethod = ETorqueCombineMethod::Override;
	bAffectedByEngine = false;
	bAffectedByBrake = false;
	bAffectedByHandbrake = false;
	MaxSteerAngle = 0.0f;

	// 履带接地：摩擦大于轮胎、高侧向刚度几乎不侧滑
	FrictionForceMultiplier = 4.0f;
	SideSlipModifier = 1.0f;      // 关键修复：原 0.2f 在打滑时直接砍掉 80% 抓地力导致冰上漂移与溜坡；1.0f 保持摩擦圆
	CorneringStiffness = 1500.0f; // 履带高侧向刚度，杜绝侧滑横甩
	SlipThreshold = 50.0f;
	SkidThreshold = 50.0f;

	// 悬挂：静态压缩 ≈ 单轮载荷(4000kg/12≈333kg) / SpringRate(250) ≈ 1.3cm
	SpringRate = 250.0f;
	SpringPreload = 50.0f;
	SuspensionDampingRatio = 0.6f;
	SuspensionMaxRaise = 12.0f;
	SuspensionMaxDrop = 15.0f;
	WheelLoadRatio = 1.0f;

	// 球扫掠 + **简单碰撞**：
	// 插件在 ComplexSweep 下会把 TraceParams.bTraceComplex 设成 true，而本作关卡地面
	// （road_hd / CityGate 系列导入网格）只有**简单**碰撞（复杂碰撞段未启用）→ 复杂射线打不中，
	// 12 个轮全部探不到地（实测）。SimpleSweep 是引擎默认值，与地面现有碰撞数据匹配。
	SweepShape = ESweepShape::Spherecast;
	SweepType = ESweepType::SimpleSweep;
}
