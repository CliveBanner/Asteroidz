# Asteroidz - Features & Fixes (January 2026 Update)

## 🚀 New Features

### 💥 Combat & Physics
- **Asteroid-Unit Collisions:** Asteroids now explode upon colliding with units, dealing significant impact damage and triggering area-of-effect (AoE) damage.
- **Unit Death Rattles:** Exploding units now deal area damage to nearby units and asteroids, making group spacing more critical.
- **Area Damage System:** Implemented a generic AoE damage function (`Physics_AreaDamage`) with linear falloff.
- **Camera Shake:** Impactful explosions and collisions now trigger a screen-shake effect proportional to the damage dealt.
- **Cinematic Explosions:** Particle effects have been slowed down and lifetimes increased to better simulate the "floating" nature of debris in space.

### 🛸 Unit Management
- **Teleport Spawning:** Units produced by the Mothership no longer spawn directly on top of it. Instead, they teleport in at a nearby location with a dedicated visual effect.
- **Distant Resource Transfer:** Miners now transfer crystals to the Mothership from a safe distance (500 units) via a visual energy stream, instead of having to touch the hull.
- **Mothership Respawn:** When destroyed, the Mothership will automatically respawn at a safe location near its death point after a 3-second delay.

### ⚖️ Balance Adjustments
- **Mothership Mining:** When the Mothership mines crystals directly, the gathered energy now prioritizes replenishing its internal battery/shield (Energy) before being converted into stored resources.
- **Fighter Overhaul:** Reduced maximum firing energy, but significantly increased energy regeneration rate (25% per second), encouraging burst-fire tactics.
- **Miner Role:** Miners are now strictly non-combat drones. Their weapon systems have been removed, and they will no longer attempt to target asteroids.
- **Crystal Rarity:** Decreased the base probability of crystal spawning by ~60% and tied it directly to the continuous density field for more natural distribution.
- **Asteroid Balance:** Reduced the probability of asteroid splitting upon collision by doubling the impulse threshold. Small asteroids are now exponentially weaker in terms of health, making them much easier to clear.

## 🛠️ Fixes & UI Improvements

### 🎮 Control Scheme
- **Command Grid Merge:** Merged the Mothership and Miner command sets into a unified grid. 
- **Selection Hotkey:** Pressing **[F]** now instantly selects the Mothership, regardless of current selection or menu state.
- **Conflict Resolution:** Resolved the keybinding conflict on the 'X' key.
    - **[F]**: Select Mothership.
    - **[X]**: Open/Close Build Menu (Mothership).
    - **[V]**: Return Cargo (Miners).
    - **[Y]**: Main Cannon / Gather / Back.
- **Sticky Commands:** Unified the command logic to handle multi-unit selection more gracefully.

### 🎨 Visuals & HUD
- **Pixel Art Overhaul:** Updated the procedural generation for Miner and Fighter units to a more blocky, quantized "pixel art" style.
- **Icon Overhaul:** HUD icons have been redesigned on an 8x8 grid for a consistent low-res aesthetic.
- **Visual FX:** Added new particle effects for teleportation and resource transfer.

## 📝 Future Notes
- Merged command grid logic is located in `src/ui.c`.
- Energy regeneration is now governed by the `regen_rate` field in `UnitStats`.
- `Physics_AreaDamage` should be used for all explosive events.
