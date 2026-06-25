# Ooey-Station: Sonic-Style Game Development Guide

Creating a high-speed platformer in the vein of *Sonic the Hedgehog* requires specific physics, robust collision detection, and smooth camera handling. This guide details how to implement these mechanics within the constraints of the Ooey-Station engine using the Booey scripting language.

## 1. Core Concepts: Speed and Sub-pixel Math

A defining characteristic of Sonic-style games is momentum. Movement isn't an instant transition from 0 to max speed; it involves acceleration, deceleration, and friction. Because speeds and positions require fractional precision, we must use Booey's `fixed` point math type (16.16 format) for all physics calculations.

### Physics Constants
Define your core physics parameters as constants (or global variables) at the top of your script.

```booey
vars:
    # Speeds are in pixels per frame
    ACCEL: fixed = 0.046875f
    DECEL: fixed = 0.5f
    FRICTION: fixed = 0.046875f
    TOP_SPEED: fixed = 6.0f
    GRAVITY: fixed = 0.21875f
    JUMP_FORCE: fixed = -6.5f
    ROLL_DECEL: fixed = 0.125f
    ROLL_FRICTION: fixed = 0.0234375f
```

## 2. The Player State Machine

The character has several states that dictate how physics and inputs are applied.

```booey
vars:
    STATE_NORMAL: int = 0
    STATE_JUMPING: int = 1
    STATE_ROLLING: int = 2
    
    player_state: int = STATE_NORMAL
    
    # Position
    px: fixed = 100.0f
    py: fixed = 100.0f
    
    # Velocity
    vx: fixed = 0.0f
    vy: fixed = 0.0f
    
    # Ground state
    grounded: bool = false
```

### Implementing Momentum

In your `update()` loop, apply acceleration based on input, and friction when no input is provided.

```booey
fn update_movement():
    if player_state == STATE_NORMAL:
        if btn_held(LEFT):
            if vx > 0.0f:
                vx -= DECEL  # Skidding
            else:
                vx -= ACCEL
        elif btn_held(RIGHT):
            if vx < 0.0f:
                vx += DECEL  # Skidding
            else:
                vx += ACCEL
        else:
            # Apply friction when no input is pressed
            if vx > 0.0f:
                vx -= FRICTION
                if vx < 0.0f: vx = 0.0f
            elif vx < 0.0f:
                vx += FRICTION
                if vx > 0.0f: vx = 0.0f
                
        # Enforce Top Speed
        if vx > TOP_SPEED: vx = TOP_SPEED
        if vx < -TOP_SPEED: vx = -TOP_SPEED
```

## 3. Slopes and Tile Collision

Classic Sonic collision relies on reading the properties of the terrain beneath the player's feet. Since Booey tilemaps are grid-based, we simulate smooth slopes by reading a "heightmap" value for specific slope tiles, or by checking pixel intersections.

### Tile Setup
Let's define a few basic tiles in the `tiles:` block.
```booey
tiles:
    solid 16x16:
        # ... 16x16 square ...
    slope_45 16x16:
        # ... A 45 degree slope ...
```

### Sensor Points
Instead of a simple bounding box, sonic-style physics uses specific "sensor" points extending from the player to the ground.
- **Sensor A (Left Foot)**
- **Sensor B (Right Foot)**
- **Sensor C/D (Wall sensors left/right)**
- **Sensor E/F (Ceiling sensors)**

```booey
fn check_ground():
    # Simplified collision: assume flat ground for this example
    # In a full game, you would map world coordinates to tile coordinates
    # and read the tile ID to determine height.
    
    let tile_y: int = (int(py) + 16) / 16  # Assuming 16px tall sprite
    let tile_x: int = int(px) / 16
    
    # Simulate hitting a solid floor at Y=200
    if py >= 200.0f:
        py = 200.0f
        vy = 0.0f
        grounded = true
        if player_state == STATE_JUMPING:
            player_state = STATE_NORMAL
    else:
        grounded = false
```

## 4. Gravity and Jumping

Gravity is constantly applied when airborne. Jumping imparts a strong negative Y velocity.

```booey
fn update_gravity_and_jump():
    if not grounded:
        vy += GRAVITY
    else:
        if btn_pressed(A):
            vy = JUMP_FORCE
            grounded = false
            player_state = STATE_JUMPING
            # play_sound(jump_sfx)
            
        # Rolling
        if btn_held(DOWN) and abs(vx) > 1.0f:
            player_state = STATE_ROLLING
```

## 5. The Camera System

A high-speed game needs a smooth camera that anticipates movement but has a deadzone so it doesn't jitter.

```booey
vars:
    cam_x: int = 0
    cam_y: int = 0

fn update_camera():
    let target_x: int = int(px) - 320 # 320 is half screen width
    let target_y: int = int(py) - 240 # 240 is half screen height
    
    # Simple deadzone / lerp follow
    if cam_x < target_x - 32:
        cam_x += min(abs(target_x - 32 - cam_x), 8)
    elif cam_x > target_x + 32:
        cam_x -= min(abs(cam_x - (target_x + 32)), 8)
        
    cam_y = target_y # Simplified vertical follow
    
    # Clamp camera to level boundaries
    cam_x = clamp(cam_x, 0, 2000)
    cam_y = clamp(cam_y, 0, 1000)
    
    # Update tilemap scroll offset
    # TSCROLL opcode: tscroll(layer, scroll_x, scroll_y)
    tscroll(0, cam_x, cam_y)
```

## 6. Rings and Dropping Mechanics

Rings are entities scattered throughout the level. When hit, Sonic loses his rings, and they scatter physically.

```booey
vars:
    rings_collected: int = 0
    MAX_DROPPED: int = 32
    # Arrays to store dropped rings
    dropped_x: fixed[32] 
    dropped_y: fixed[32]
    dropped_vx: fixed[32]
    dropped_vy: fixed[32]
    dropped_active: bool[32]

fn scatter_rings():
    if rings_collected == 0:
        # Die!
        return
        
    let num_to_drop: int = min(rings_collected, MAX_DROPPED)
    rings_collected = 0
    
    let angle: fixed = 0.0f
    let speed: fixed = 4.0f
    
    for i in range(num_to_drop):
        dropped_active[i] = true
        dropped_x[i] = px
        dropped_y[i] = py
        
        # Simple circular spread using fixed sin/cos approx
        # For a real implementation, you'd want a lookup table or built-in sin()
        dropped_vx[i] = sin(angle) * speed
        dropped_vy[i] = cos(angle) * speed
        
        angle += 3.14159f * 2.0f / fixed(num_to_drop)
```

## 7. Putting it Together: The Update Loop

```booey
fn update():
    update_movement()
    update_gravity_and_jump()
    
    # Apply velocities
    px += vx
    py += vy
    
    check_ground()
    update_camera()
    
    # Drawing Phase
    cls(sky_blue)
    
    # Draw Background (Layer 0)
    tdraw(0)
    
    # Draw Player relative to camera
    let draw_x: int = int(px) - cam_x
    let draw_y: int = int(py) - cam_y
    
    # Animation logic
    let current_sprite: sprite_id = spr_idle
    if player_state == STATE_ROLLING or player_state == STATE_JUMPING:
        current_sprite = spr_roll # Rotate frame based on frame()
    elif abs(vx) > 0.5f:
        current_sprite = spr_run
        
    # Draw sprite with horizontal flip if moving left
    if vx < 0.0f:
        draw_sprite_ex(current_sprite, draw_x, draw_y, 1)
    else:
        draw_sprite(current_sprite, draw_x, draw_y)
        
    # HUD
    draw_text(10, 10, "RINGS: " + str(rings_collected), yellow)
```

## 8. Loop-de-Loops and Layer Switching

The signature loop-de-loop requires tricking the collision system.
A loop consists of two overlapping paths.
1. The lower path (entrance).
2. The upper path (exit).

**Implementation strategy:**
- Use two background layers (Layer 0 and Layer 1).
- Set up a "layer toggle" trigger area right before the loop entrance.
- When Sonic passes the trigger moving right, set a variable `collision_layer = 1`.
- Update your `check_ground()` logic to read tile IDs from the current `collision_layer`.
- Inside the loop, gravity pulls sonic "down", but if velocity is high enough, slope collision physics overrides gravity, pushing him around the track.
- Once exiting the loop, another trigger area resets `collision_layer = 0`.

*Note: True 360-degree physics requires calculating the angle of the slope tile and separating velocity into `ground_speed` (along the curve) and `x/y` speeds. This is advanced math, but achievable with Booey's fixed point trigonometric functions!*
