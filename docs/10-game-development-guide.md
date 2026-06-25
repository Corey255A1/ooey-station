# Ooey-Station: Game Development Guide

Welcome to the Ooey-Station! This guide will walk you through the process of creating games using the Booey scripting language. Whether you are a human developer or an AI generating a game, these best practices and examples will help you get started.

## 1. The Game Loop

Every Ooey-Station game revolves around two core functions that you must define in your script: `init()` and `update()`.

- **`init()`**: Called exactly once when the game is loaded. Use this to set up initial variable states, positions, and spawn entities.
- **`update()`**: Called 60 times per second (60 FPS). This is your main game loop. You should handle input, update game logic, clear the screen, and draw graphics here.

```booey
vars:
    x: int = 100

fn init():
    # Setup goes here
    pass

fn update():
    # Logic and rendering goes here
    x += 1
```

## 2. Hello World

The simplest possible game: clearing the screen and drawing some text.

```booey
game:
    title: "Hello World"
    author: "Documentation"

palette:
    blue: rgb(0, 0, 128)
    white: rgb(255, 255, 255)

fn init():
    pass

fn update():
    cls(blue)
    draw_text(100, 100, "Hello, Ooey-Station!", white)
```

## 3. Handling Input

Use the `btn_held()`, `btn_pressed()`, and `btn_released()` functions to read the gamepad state.

- `btn_held` is good for continuous movement (like walking).
- `btn_pressed` is good for one-off actions (like jumping or shooting).

```booey
vars:
    player_x: int = 320
    player_y: int = 240

fn update():
    # Movement
    if btn_held(LEFT):
        player_x -= 3
    if btn_held(RIGHT):
        player_x += 3
        
    # Action
    if btn_pressed(A):
        play_sound(jump_sfx)
        
    cls(bg_color)
    draw_sprite(player, player_x, player_y)
```

## 4. Working with Sprites

Define your sprites in the `sprites:` block using the ASCII-grid syntax. Remember to define your colors in the `palette:` block first!

```booey
palette:
    trans: rgb(255, 0, 255) # Implicitly transparent
    red: rgb(255, 0, 0)
    blue: rgb(0, 0, 255)

sprites:
    hero 8x8:
        . . r r r r . .
        . r r b b r r .
        r r r r r r r r
        r r r r r r r r
        . r r . . r r .
        . r r . . r r .
        . b b . . b b .
        b b b . . b b b

vars:
    px: int = 50
    py: int = 50

fn update():
    cls(trans)
    
    # Draw the sprite
    draw_sprite(hero, px, py)
    
    # Draw it flipped horizontally
    # (Assuming SPREX opcode/intrinsic takes flags: 1 = Flip H)
    draw_sprite_ex(hero, px + 20, py, 1) 
```

## 5. Best Practices & Optimization

1. **Clear the Screen First**: Always call `cls()` at the beginning of your `update()` function unless you intentionally want "trailing" effects.
2. **Use Fixed-Point Math for Smooth Movement**: Integer math can cause jerky movement for slow-moving objects. Use the `fixed` type (e.g., `1.5f`) for positions and velocities, and cast them to `int` only when passing them to drawing functions.
3. **Avoid Complex Loops in `update()`**: The VM enforces limits to prevent infinite loops. Try to keep per-frame logic light. If you must process a lot of data, distribute the work across multiple frames.
4. **Collision Filtering**: `check_collision` is fast, but checking every object against every other object is `O(N^2)`. Use simple distance checks (`abs(x1 - x2) > max_dist`) to rule out collisions before calling the more precise bounding box function.

## 6. Full Example: Pong Clone

Here is a complete, playable example of a two-player Pong clone to demonstrate all the concepts coming together.

```booey
game:
    title: "Booey Pong"
    author: "Tutorial"

palette:
    bg: rgb(20, 20, 20)
    white: rgb(240, 240, 240)

sounds:
    hit:
        type: tone
        start_freq: 600
        duration_ms: 50
    score:
        type: sweep
        start_freq: 400
        end_freq: 200
        duration_ms: 300

vars:
    # Paddles
    p1_y: int = 200
    p2_y: int = 200
    paddle_w: int = 10
    paddle_h: int = 60
    
    # Ball
    bx: int = 320
    by: int = 240
    # Using fixed point for velocity to allow angles
    bvx: fixed = -4.0f 
    bvy: fixed = 2.5f
    
    # Scores
    score1: int = 0
    score2: int = 0

fn reset_ball():
    bx = 320
    by = 240
    bvx = -bvx # send to whoever just scored

fn update():
    # --- Input ---
    # Player 1 uses Up/Down D-Pad
    if btn_held(UP):
        p1_y -= 5
    if btn_held(DOWN):
        p1_y += 5
        
    # Player 2 uses X/A (mapped on right side of controller)
    if btn_held(X):
        p2_y -= 5
    if btn_held(A):
        p2_y += 5
        
    # Clamp paddles to screen
    p1_y = clamp(p1_y, 0, 480 - paddle_h)
    p2_y = clamp(p2_y, 0, 480 - paddle_h)

    # --- Ball Logic ---
    bx = bx + int(bvx)
    by = by + int(bvy)
    
    # Top/Bottom wall bounce
    if by <= 0 or by >= 470:
        bvy = -bvy
        play_sound(hit)
        
    # Paddle 1 collision (Left)
    if bx <= 30 + paddle_w and by + 10 >= p1_y and by <= p1_y + paddle_h:
        bx = 30 + paddle_w
        bvx = -bvx
        play_sound(hit)
        
    # Paddle 2 collision (Right)
    if bx + 10 >= 610 - paddle_w and by + 10 >= p2_y and by <= p2_y + paddle_h:
        bx = 610 - paddle_w - 10
        bvx = -bvx
        play_sound(hit)
        
    # Scoring
    if bx < 0:
        score2 += 1
        play_sound(score)
        reset_ball()
    elif bx > 640:
        score1 += 1
        play_sound(score)
        reset_ball()

    # --- Drawing ---
    cls(bg)
    
    # Center net line
    for y in range(0, 480, 20):
        fill_rect(318, y, 4, 10, white)
        
    # Paddles
    fill_rect(30, p1_y, paddle_w, paddle_h, white)
    fill_rect(610 - paddle_w, p2_y, paddle_w, paddle_h, white)
    
    # Ball (10x10)
    fill_rect(bx, by, 10, 10, white)
    
    # Scores
    draw_text(200, 30, str(score1), white)
    draw_text(440, 30, str(score2), white)
```
