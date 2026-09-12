#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "TankVehicleWheel.generated.h"

/**
 * 坦克负重轮的物理参数（履带车辆）。
 *
 * 与轮式车的三点区别：
 *  1. **物理轮 = 履带接地段**，不是轮盘本体：轮心在网格空间 z=41.75、履带底面 z=0，
 *     所以有效半径取 41.75（网格）→ ×整车缩放 0.5 = 20.875（世界）。这样静止时履带底面正好贴地。
 *     视觉轮盘（半径 35 网格）继续按仿真结果摆放，与物理轮解耦。
 *  2. **不走引擎/变速箱/差速器**：坦克靠左右履带差速转向，由 ATankVehicle 每帧
 *     `SetDriveTorque(按轮)` 直接给扭矩 —— 因此必须放开 `ExternalTorqueCombineMethod`
 *     （默认 None = 外部力矩无效，这一点不设就永远不会动）。
 *  3. **不做转向轮**：`MaxSteerAngle = 0`，A/D 不作用在轮子的偏转角上。
 */
UCLASS()
class TANK_API UTankVehicleWheel : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UTankVehicleWheel(const FObjectInitializer& ObjectInitializer);
};
