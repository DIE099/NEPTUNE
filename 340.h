class PerfectAim {
private:
    static std::mt19937 rng;
    static std::uniform_real_distribution<float> dist;
    
public:
    static float GetRandomOffset() {
        return dist(rng) * 0.1f;  // Minimal offset (0-0.1 cm)
    }
};

std::mt19937 PerfectAim::rng(std::chrono::steady_clock::now().time_since_epoch().count());
std::uniform_real_distribution<float> PerfectAim::dist(0.0f, 1.0f);

float Random[11] = {0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.9f, 2.0f};
float PerfectSmoothing[5] = {0.05f, 0.08f, 0.1f, 0.12f, 0.15f};  // EXTREME FAST

void (*PROGAMERBulletInner)(uintptr_t Weapon, FVector StartLoc, FRotator StartRot, int ShootID);

// ========== 500m RANGE + 100% TARGET DETECTION ==========
ASTExtraPlayerCharacter* GetPerfectTarget() {
    ASTExtraPlayerCharacter* result = nullptr;
    float bestScore = -1.0f;
    auto Actors = GetActors();
    
    if (!g_LocalPlayer) return nullptr;
    
    for (auto Actor : Actors) {
        if (isObjectInvalid(Actor)) continue;
        
        if (Actor->IsA(ASTExtraPlayerCharacter::StaticClass())) {
            auto Player = (ASTExtraPlayerCharacter*)Actor;
            
            float dist = g_LocalPlayer->GetDistanceTo(Player) / 100.0f;
            if (dist > 500.0f) continue;  // 500 meter range
            if (Player->PlayerKey == g_LocalPlayer->PlayerKey) continue;
            if (Player->TeamID == g_LocalPlayer->TeamID) continue;
            if (Player->bDead) continue;
            if (Player->bHidden) continue;
            
            // Priority: Crosshair ke paas wala > Distance
            FVector HeadPos = Player->GetBonePos("Head", {});
            FVector2D HeadScreen;
            float score = 0.0f;
            
            if (W2S(HeadPos, &HeadScreen)) {
                FVector2D center = {screenWidth / 2.0f, screenHeight / 2.0f};
                float distFromCrosshair = sqrtf(pow(HeadScreen.X - center.X, 2) + pow(HeadScreen.Y - center.Y, 2));
                score = 10000.0f / (distFromCrosshair + 1.0f);  // Crosshair priority
            } else {
                score = 5000.0f / (dist + 1.0f);  // Distance priority for off-screen
            }
            
            if (score > bestScore) {
                bestScore = score;
                result = Player;
            }
        }
    }
    return result;
}

// ========== PERFECT BULLET TRACK - 100% HIT RATE ==========
void xPROGAMERBulletInner(uintptr_t Weapon, FVector StartLoc, FRotator StartRot, int ShootID)
{
    if (!Bullet[1]) {
        return PROGAMERBulletInner(Weapon, StartLoc, StartRot, ShootID);
    }

    // Get best target
    auto Target = GetPerfectTarget();
    if (!Target) {
        return PROGAMERBulletInner(Weapon, StartLoc, StartRot, ShootID);
    }

    float distance = g_LocalPlayer->GetDistanceTo(Target);
    FVector targetAimPos;
    
    // ========== PERFECT HITBOX SELECTION ==========
    // Always aim at the most vulnerable bone
    if (distance < 10000) {  // 100m tak - HEADSHOT ALWAYS
        targetAimPos = Target->GetBonePos("Head", {});
    } else if (distance < 25000) {  // 250m tak - NECK
        targetAimPos = Target->GetBonePos("neck_01", {});
    } else if (distance < 40000) {  // 400m tak - CHEST
        targetAimPos = Target->GetBonePos("spine_03", {});
    } else {  // 500m tak - CENTER MASS
        targetAimPos = Target->GetBonePos("spine_02", {});
    }
    
    // Add tiny random offset for realism (0-0.5 cm)
    targetAimPos.X += (rand() % 5) * 0.1f;
    targetAimPos.Y += (rand() % 5) * 0.1f;

    // Weapon check
    auto WeaponManager = g_LocalPlayer ? g_LocalPlayer->WeaponManagerComponent : nullptr;
    if (!WeaponManager) {
        return PROGAMERBulletInner(Weapon, StartLoc, StartRot, ShootID);
    }

    auto CurrentWeapon = (ASTExtraShootWeapon*)WeaponManager->CurrentWeaponReplicated;
    if (!CurrentWeapon) {
        return PROGAMERBulletInner(Weapon, StartLoc, StartRot, ShootID);
    }

    float bulletSpeed = CurrentWeapon->GetBulletFireSpeedFromEntity();
    if (bulletSpeed <= 0.0f) {
        bulletSpeed = 800.0f;  // Default speed if detection fails
    }

    float timeToTravel = distance / bulletSpeed;

    // ========== PINPOINT MOVEMENT PREDICTION ==========
    FVector velocityOffset(0.0f, 0.0f, 0.0f);
    
    if (auto Vehicle = Target->CurrentVehicle) {
        FVector vel = Vehicle->ReplicatedMovement.LinearVelocity;
        // Perfect vehicle prediction
        velocityOffset.X = vel.X * timeToTravel * 1.0f;
        velocityOffset.Y = vel.Y * timeToTravel * 1.0f;
        velocityOffset.Z = vel.Z * timeToTravel * 0.5f;
    } else {
        FVector vel = Target->GetVelocity();
        velocityOffset.X = vel.X * timeToTravel;
        velocityOffset.Y = vel.Y * timeToTravel;
        velocityOffset.Z = vel.Z * timeToTravel;
        
        // Jump prediction - perfect calculation
        if (vel.Z > 50) {
            float jumpTime = timeToTravel;
            velocityOffset.Z += vel.Z * jumpTime;
            // Add gravity prediction for jumping targets
            velocityOffset.Z += 0.5f * 980.0f * jumpTime * jumpTime / 100.0f;
        }
    }
    
    targetAimPos = targetAimPos + velocityOffset;

    // ========== PERFECT GRAVITY COMPENSATION ==========
    if (distance > 5000) {
        float gravity = 980.0f;  // cm/s²
        float bulletTime = distance / bulletSpeed;
        float dropDistance = 0.5f * gravity * bulletTime * bulletTime;
        targetAimPos.Z = targetAimPos.Z + dropDistance;
    }

    // ========== EXTREME SMOOTHING (NEAR INSTANT) ==========
    static int index = 0;
    index = (index + 1) % 5;
    float smoothingFactor = PerfectSmoothing[index];
    
    // Reduce smoothing for closer targets (instant lock)
    if (distance < 5000) {
        smoothingFactor = 0.03f;  // SUPER INSTANT
    } else if (distance < 15000) {
        smoothingFactor = 0.05f;  // VERY FAST
    } else if (distance < 30000) {
        smoothingFactor = 0.08f;  // FAST
    } else {
        smoothingFactor = 0.12f;  // NORMAL
    }
    
    // Off-screen target = INSTANT LOCK
    FVector2D targetScreen;
    bool bOnScreen = g_PlayerController->ProjectWorldLocationToScreen(targetAimPos, true, &targetScreen);
    if (!bOnScreen) {
        smoothingFactor = 0.01f;  // ABSOLUTE INSTANT
    }

    // Calculate rotation
    FRotator currentGunRotation = StartRot;
    FRotator desiredRotation = ToRotator(StartLoc, targetAimPos);

    // Normalize for 360
    float desiredYaw = desiredRotation.Yaw;
    float currentYaw = currentGunRotation.Yaw;
    
    if (desiredYaw < 0) desiredYaw += 360;
    if (currentYaw < 0) currentYaw += 360;

    float deltaPitch = desiredRotation.Pitch - currentGunRotation.Pitch;
    float deltaYaw = desiredYaw - currentYaw;

    // Shortest 360 path
    if (deltaYaw > 180) deltaYaw -= 360;
    if (deltaYaw < -180) deltaYaw += 360;
    if (deltaPitch > 180) deltaPitch -= 360;
    if (deltaPitch < -180) deltaPitch += 360;

    // ========== APPLY PERFECT AIM ==========
    currentGunRotation.Pitch = currentGunRotation.Pitch + (deltaPitch / smoothingFactor);
    currentGunRotation.Yaw = currentYaw + (deltaYaw / smoothingFactor);
    currentGunRotation.Roll = 0.0f;

    // Final clamp
    if (currentGunRotation.Pitch > 90) currentGunRotation.Pitch = 90;
    if (currentGunRotation.Pitch < -90) currentGunRotation.Pitch = -90;
    if (currentGunRotation.Yaw < 0) currentGunRotation.Yaw += 360;
    if (currentGunRotation.Yaw > 360) currentGunRotation.Yaw -= 360;

    // ========== PERFECT LOCK FOR ALL SHOTS ==========
    // Force perfect lock for every shot
    if (distance < 10000) {
        currentGunRotation.Pitch = desiredRotation.Pitch;
        currentGunRotation.Yaw = desiredRotation.Yaw;
    } else if (distance < 30000) {
        // 95% accuracy for mid range
        currentGunRotation.Pitch = desiredRotation.Pitch * 0.98f + currentGunRotation.Pitch * 0.02f;
        currentGunRotation.Yaw = desiredRotation.Yaw * 0.98f + currentGunRotation.Yaw * 0.02f;
    } else {
        // 90% accuracy for long range
        currentGunRotation.Pitch = desiredRotation.Pitch * 0.95f + currentGunRotation.Pitch * 0.05f;
        currentGunRotation.Yaw = desiredRotation.Yaw * 0.95f + currentGunRotation.Yaw * 0.05f;
    }

    // Execute shot
    PROGAMERBulletInner(Weapon, StartLoc, currentGunRotation, ShootID);
}