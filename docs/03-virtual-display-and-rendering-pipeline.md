# 03 — Virtual Display & Rendering Pipeline

> How the 640×480 virtual display works and how game graphics are rendered through the Booey VM, composited with the OOEY shell, and presented on screen.

---

## Table of Contents

1. [Virtual Display Architecture](#1-virtual-display-architecture)
2. [Virtual Framebuffer Implementation](#2-virtual-framebuffer-implementation)
3. [Sprite Rendering System](#3-sprite-rendering-system)
4. [Tilemap / Background System](#4-tilemap--background-system)
5. [Color Palette System](#5-color-palette-system)
6. [Drawing Primitives Available to VM](#6-drawing-primitives-available-to-vm)
7. [Performance Considerations](#7-performance-considerations)

---

## 1. Virtual Display Architecture

### 1.1 Design Philosophy

OOEY Station emulates a **fixed-resolution virtual display** inspired by classic consoles (SNES, GBA, Genesis). Every game targets a single canonical resolution — **640×480 pixels** — regardless of the host window or monitor size. This guarantees:

- **Pixel-perfect art**: Artists work in a known grid; no sub-pixel ambiguity.
- **Deterministic layout**: UI element placement, collision maps, and camera math are identical on every device.
- **Retro aesthetic**: Integer-scale rendering produces the crisp, chunky look players expect from a fantasy console.

The host application is free to run at any resolution. The virtual framebuffer is scaled and composited at presentation time.

### 1.2 Dual-Layer Rendering Model

Rendering is split into two independent layers that are composited during the final presentation pass:

```
┌─────────────────────────────────────────────────────────┐
│                    Host Window                          │
│                                                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │              Shell Layer (OOEY)                    │  │
│  │  ┌─────────────────────────────────────────────┐  │  │
│  │  │                                             │  │  │
│  │  │         Game Layer (Booey VM)                │  │  │
│  │  │         640×480 virtual framebuffer          │  │  │
│  │  │         scaled + letterboxed                 │  │  │
│  │  │                                             │  │  │
│  │  └─────────────────────────────────────────────┘  │  │
│  │                                                    │  │
│  │  [ bezel art / on-screen gamepad / menus ]         │  │
│  └───────────────────────────────────────────────────┘  │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

#### Layer 1 — Game Layer (Booey VM)

| Property | Value |
|---|---|
| Resolution | 640×480 px, fixed |
| Color depth | 8-bit indexed → 24-bit RGB via palette lookup |
| Framebuffer format | RGBA8888 (after palette resolve) |
| Origin | Top-left (0,0) |
| Coordinate system | Y-down, X-right |
| Render target | Off-screen pixel buffer (CPU-side `Uint8Array`) |
| Ownership | Fully owned by the Booey VM; OOEY never writes into it |

The Booey VM exposes drawing syscalls (see §6) that write into this buffer. At the end of each frame, the buffer is uploaded to a GPU texture for compositing.

#### Layer 2 — Shell Layer (OOEY)

| Property | Value |
|---|---|
| Resolution | Matches host window (dynamic) |
| Render target | Normal GPU render target / canvas |
| Contents | Station bezels, on-screen gamepad, menus, overlays, notifications |
| Coordinate system | Standard screen coordinates of the host |

The shell layer renders **on top of** the scaled game texture. It uses the host's native resolution so that UI elements like button labels and menu text remain crisp at any scale.

#### Compositing Order (back to front)

```
1. Clear host framebuffer to letterbox color (default: #0C0C0E)
2. Draw scaled game texture (centered, aspect-correct)
3. Draw bezel/border art (if enabled)
4. Draw on-screen gamepad overlay (if enabled)
5. Draw menus / modal dialogs (if active)
6. Draw debug overlays (FPS counter, VRAM viewer — dev mode only)
```

### 1.3 Scaling & Aspect Ratio

The virtual display always maintains a **4:3 aspect ratio** (640÷480 = 4÷3). When the host window has a different aspect ratio, the display is **letterboxed** (horizontal bars) or **pillarboxed** (vertical bars).

#### Scaling Algorithm

```
Given:
  W = host window width   (pixels)
  H = host window height  (pixels)
  VW = 640                 (virtual width)
  VH = 480                 (virtual height)

Compute:
  scale_x = W / VW
  scale_y = H / VH
  scale   = min(scale_x, scale_y)    // fit inside window

  display_width  = floor(VW * scale)
  display_height = floor(VH * scale)

  offset_x = floor((W - display_width)  / 2)
  offset_y = floor((H - display_height) / 2)
```

#### Scaling Mode Options

| Mode | Behavior | Use Case |
|---|---|---|
| `fit` (default) | Scale to largest integer or fractional factor that fits; letterbox/pillarbox remainder | General use |
| `integer` | Scale to largest **integer** factor that fits (1×, 2×, 3× …); larger letterbox margins | Pixel-art purists |
| `stretch` | Stretch to fill window; aspect ratio **not** preserved | Accessibility / user preference |

#### Integer Scaling Example

```
Window: 1920×1080

Integer scales that fit:
  1× →  640×480   ✓
  2× → 1280×960   ✓
  3× → 1920×1440  ✗ (1440 > 1080)

Best integer scale = 2×
  display_width  = 1280
  display_height =  960
  offset_x = (1920 - 1280) / 2 = 320
  offset_y = (1080 -  960) / 2 =  60
```

#### Coordinate Mapping (Screen ↔ Virtual)

To map a mouse click or touch event from screen space back to virtual framebuffer space:

```
virtual_x = floor((screen_x - offset_x) / scale)
virtual_y = floor((screen_y - offset_y) / scale)

// Clamp to valid range
virtual_x = clamp(virtual_x, 0, VW - 1)
virtual_y = clamp(virtual_y, 0, VH - 1)

// Check if click is inside the display at all
is_inside = (screen_x >= offset_x) &&
            (screen_x <  offset_x + display_width) &&
            (screen_y >= offset_y) &&
            (screen_y <  offset_y + display_height)
```

---

## 2. Virtual Framebuffer Implementation

### 2.1 Memory Layout

The virtual framebuffer is a flat, contiguous byte array representing a 640×480 RGBA image.

```
Total size: 640 × 480 × 4 = 1,228,800 bytes (~1.2 MB)

Storage order: Row-major, top-left origin
Pixel format: RGBA8888 (R at lowest byte offset)

Byte layout for pixel at (x, y):
  offset = (y * 640 + x) * 4

  buffer[offset + 0] = R   (0–255)
  buffer[offset + 1] = G   (0–255)
  buffer[offset + 2] = B   (0–255)
  buffer[offset + 3] = A   (0–255, 255 = fully opaque)
```

#### Memory Map Diagram

```
Byte offset:  0                                          2559    2560                   ...     1,228,799
             ┌──────────────────────────────────────────────┬──────────────────────────────┬───────────┐
             │  Row 0: pixels (0,0)..(639,0)                │  Row 1: pixels (0,1)..(639,1)│  ...      │
             │  640 pixels × 4 bytes = 2560 bytes/row       │                              │           │
             └──────────────────────────────────────────────┴──────────────────────────────┴───────────┘

Single pixel (RGBA8888):
  ┌─────┬─────┬─────┬─────┐
  │  R  │  G  │  B  │  A  │   4 bytes
  └─────┴─────┴─────┴─────┘
  byte 0  byte 1  byte 2  byte 3

Row stride: 640 × 4 = 2560 bytes
```

### 2.2 VirtualDisplay Class

```typescript
class VirtualDisplay {
  // --- Constants ---
  static readonly WIDTH  = 640;
  static readonly HEIGHT = 480;
  static readonly BPP    = 4;                           // bytes per pixel
  static readonly STRIDE = VirtualDisplay.WIDTH * VirtualDisplay.BPP;  // 2560
  static readonly SIZE   = VirtualDisplay.STRIDE * VirtualDisplay.HEIGHT; // 1,228,800

  // --- State ---
  private buffer: Uint8Array;          // the raw pixel buffer
  private palette: Uint32Array;        // 256-entry RGBA palette (packed)
  private dirtyRects: Rect[];          // regions modified this frame
  private gpuTexture: GPUTexture | null;

  constructor() {
    this.buffer    = new Uint8Array(VirtualDisplay.SIZE);
    this.palette   = new Uint32Array(256);
    this.dirtyRects = [];
    this.gpuTexture = null;
    this.initDefaultPalette();
  }

  // ═══════════════════════════════════════════════
  //  Buffer Access
  // ═══════════════════════════════════════════════

  /** Return raw RGBA buffer for direct read. */
  get_buffer(): Uint8Array {
    return this.buffer;
  }

  /** Pixel offset for (x, y). No bounds check — caller must validate. */
  private offset(x: number, y: number): number {
    return (y * VirtualDisplay.WIDTH + x) * VirtualDisplay.BPP;
  }

  /** Returns true if (x, y) is within the virtual display bounds. */
  private inBounds(x: number, y: number): boolean {
    return x >= 0 && x < VirtualDisplay.WIDTH &&
           y >= 0 && y < VirtualDisplay.HEIGHT;
  }

  // ═══════════════════════════════════════════════
  //  Core Drawing
  // ═══════════════════════════════════════════════

  /** Clear entire buffer to a single RGBA color. */
  clear(r: number = 0, g: number = 0, b: number = 0, a: number = 255): void {
    // Fast path: fill with a 32-bit repeated pattern
    const pixel = (a << 24) | (b << 16) | (g << 8) | r;  // little-endian ABGR
    const view  = new Uint32Array(this.buffer.buffer);
    view.fill(pixel);
    this.markFullDirty();
  }

  /** Set a single pixel. */
  set_pixel(x: number, y: number, r: number, g: number, b: number, a: number = 255): void {
    if (!this.inBounds(x, y)) return;
    const o = this.offset(x, y);
    this.buffer[o]     = r;
    this.buffer[o + 1] = g;
    this.buffer[o + 2] = b;
    this.buffer[o + 3] = a;
    this.markDirty(x, y, 1, 1);
  }

  /** Get a single pixel as [R, G, B, A]. */
  get_pixel(x: number, y: number): [number, number, number, number] {
    if (!this.inBounds(x, y)) return [0, 0, 0, 0];
    const o = this.offset(x, y);
    return [this.buffer[o], this.buffer[o+1], this.buffer[o+2], this.buffer[o+3]];
  }

  // ═══════════════════════════════════════════════
  //  Shape Drawing
  // ═══════════════════════════════════════════════

  /** Draw an axis-aligned rectangle outline (1px border). */
  draw_rect(x: number, y: number, w: number, h: number,
            r: number, g: number, b: number, a: number = 255): void {
    // Top and bottom edges
    for (let i = x; i < x + w; i++) {
      this.set_pixel(i, y, r, g, b, a);
      this.set_pixel(i, y + h - 1, r, g, b, a);
    }
    // Left and right edges
    for (let j = y; j < y + h; j++) {
      this.set_pixel(x, j, r, g, b, a);
      this.set_pixel(x + w - 1, j, r, g, b, a);
    }
    this.markDirty(x, y, w, h);
  }

  /** Draw a filled axis-aligned rectangle. */
  fill_rect(x: number, y: number, w: number, h: number,
            r: number, g: number, b: number, a: number = 255): void {
    // Clip to screen bounds
    const x0 = Math.max(x, 0);
    const y0 = Math.max(y, 0);
    const x1 = Math.min(x + w, VirtualDisplay.WIDTH);
    const y1 = Math.min(y + h, VirtualDisplay.HEIGHT);

    if (a === 255) {
      // Opaque fast path: write entire rows at once
      const rowLen = (x1 - x0) * VirtualDisplay.BPP;
      const rowBuf = new Uint8Array(rowLen);
      for (let i = 0; i < x1 - x0; i++) {
        rowBuf[i * 4]     = r;
        rowBuf[i * 4 + 1] = g;
        rowBuf[i * 4 + 2] = b;
        rowBuf[i * 4 + 3] = 255;
      }
      for (let row = y0; row < y1; row++) {
        this.buffer.set(rowBuf, this.offset(x0, row));
      }
    } else {
      // Alpha blending path
      for (let row = y0; row < y1; row++) {
        for (let col = x0; col < x1; col++) {
          this.blendPixel(col, row, r, g, b, a);
        }
      }
    }
    this.markDirty(x0, y0, x1 - x0, y1 - y0);
  }

  /** Bresenham line drawing. */
  draw_line(x0: number, y0: number, x1: number, y1: number,
            r: number, g: number, b: number, a: number = 255): void {
    let dx = Math.abs(x1 - x0);
    let dy = -Math.abs(y1 - y0);
    let sx = x0 < x1 ? 1 : -1;
    let sy = y0 < y1 ? 1 : -1;
    let err = dx + dy;

    while (true) {
      this.set_pixel(x0, y0, r, g, b, a);
      if (x0 === x1 && y0 === y1) break;
      const e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
    // Dirty rect covers bounding box of line
    this.markDirty(
      Math.min(x0, x1), Math.min(y0, y1),
      Math.abs(x1 - x0) + 1, Math.abs(y1 - y0) + 1
    );
  }

  /** Midpoint circle drawing (outline). */
  draw_circle(cx: number, cy: number, radius: number,
              r: number, g: number, b: number, a: number = 255): void {
    let x = radius;
    let y = 0;
    let d = 1 - radius;

    while (x >= y) {
      // Draw all 8 octants
      this.set_pixel(cx + x, cy + y, r, g, b, a);
      this.set_pixel(cx - x, cy + y, r, g, b, a);
      this.set_pixel(cx + x, cy - y, r, g, b, a);
      this.set_pixel(cx - x, cy - y, r, g, b, a);
      this.set_pixel(cx + y, cy + x, r, g, b, a);
      this.set_pixel(cx - y, cy + x, r, g, b, a);
      this.set_pixel(cx + y, cy - x, r, g, b, a);
      this.set_pixel(cx - y, cy - x, r, g, b, a);

      y++;
      if (d <= 0) {
        d += 2 * y + 1;
      } else {
        x--;
        d += 2 * (y - x) + 1;
      }
    }
    this.markDirty(cx - radius, cy - radius, radius * 2 + 1, radius * 2 + 1);
  }

  /** Filled circle using horizontal scanlines. */
  fill_circle(cx: number, cy: number, radius: number,
              r: number, g: number, b: number, a: number = 255): void {
    let x = radius;
    let y = 0;
    let d = 1 - radius;

    // Draw horizontal spans for each y offset
    const drawSpan = (sx: number, ex: number, row: number) => {
      for (let i = sx; i <= ex; i++) {
        this.set_pixel(i, row, r, g, b, a);
      }
    };

    while (x >= y) {
      drawSpan(cx - x, cx + x, cy + y);
      drawSpan(cx - x, cx + x, cy - y);
      drawSpan(cx - y, cx + y, cy + x);
      drawSpan(cx - y, cx + y, cy - x);

      y++;
      if (d <= 0) {
        d += 2 * y + 1;
      } else {
        x--;
        d += 2 * (y - x) + 1;
      }
    }
    this.markDirty(cx - radius, cy - radius, radius * 2 + 1, radius * 2 + 1);
  }

  // ═══════════════════════════════════════════════
  //  Sprite & Blit Operations
  // ═══════════════════════════════════════════════

  /** Draw an indexed-color sprite using the current palette. */
  draw_sprite(
    spriteData: Uint8Array,   // indexed pixel data (1 byte per pixel)
    srcX: number, srcY: number,
    srcW: number, srcH: number,
    dstX: number, dstY: number,
    sheetWidth: number        // width of the full sprite sheet in pixels
  ): void {
    for (let row = 0; row < srcH; row++) {
      for (let col = 0; col < srcW; col++) {
        const srcOffset = (srcY + row) * sheetWidth + (srcX + col);
        const colorIndex = spriteData[srcOffset];

        // Index 0 = transparent, skip
        if (colorIndex === 0) continue;

        const rgba = this.palette[colorIndex];
        const r = rgba & 0xFF;
        const g = (rgba >> 8) & 0xFF;
        const b = (rgba >> 16) & 0xFF;
        const a = (rgba >> 24) & 0xFF;

        this.set_pixel(dstX + col, dstY + row, r, g, b, a);
      }
    }
    this.markDirty(dstX, dstY, srcW, srcH);
  }

  /** Draw a sprite with horizontal and/or vertical flipping. */
  draw_sprite_flipped(
    spriteData: Uint8Array,
    srcX: number, srcY: number,
    srcW: number, srcH: number,
    dstX: number, dstY: number,
    sheetWidth: number,
    flipH: boolean,
    flipV: boolean
  ): void {
    for (let row = 0; row < srcH; row++) {
      for (let col = 0; col < srcW; col++) {
        const readCol = flipH ? (srcW - 1 - col) : col;
        const readRow = flipV ? (srcH - 1 - row) : row;

        const srcOffset = (srcY + readRow) * sheetWidth + (srcX + readCol);
        const colorIndex = spriteData[srcOffset];

        if (colorIndex === 0) continue;

        const rgba = this.palette[colorIndex];
        const r = rgba & 0xFF;
        const g = (rgba >> 8) & 0xFF;
        const b = (rgba >> 16) & 0xFF;
        const a = (rgba >> 24) & 0xFF;

        this.set_pixel(dstX + col, dstY + row, r, g, b, a);
      }
    }
    this.markDirty(dstX, dstY, srcW, srcH);
  }

  /**
   * Draw a full tile layer (see §4 for tile layer details).
   * Renders visible portion of the tile map given a camera/scroll offset.
   */
  draw_tile_layer(
    tileMap: Uint16Array,      // tile indices (row-major)
    tileSheet: Uint8Array,     // indexed tile pixel data
    mapCols: number,
    mapRows: number,
    tileSize: number,          // 8 or 16
    scrollX: number,
    scrollY: number,
    tileSheetCols: number      // tiles per row in the tile sheet
  ): void {
    const tilesPerRow = Math.ceil(VirtualDisplay.WIDTH / tileSize) + 1;
    const tilesPerCol = Math.ceil(VirtualDisplay.HEIGHT / tileSize) + 1;

    const startTileX = Math.floor(scrollX / tileSize);
    const startTileY = Math.floor(scrollY / tileSize);
    const offsetX = -(scrollX % tileSize);
    const offsetY = -(scrollY % tileSize);

    for (let ty = 0; ty < tilesPerCol; ty++) {
      for (let tx = 0; tx < tilesPerRow; tx++) {
        const mapX = (startTileX + tx) % mapCols;
        const mapY = (startTileY + ty) % mapRows;
        const tileIndex = tileMap[mapY * mapCols + mapX];

        if (tileIndex === 0) continue; // empty tile

        // Calculate source position in tile sheet
        const sheetX = (tileIndex % tileSheetCols) * tileSize;
        const sheetY = Math.floor(tileIndex / tileSheetCols) * tileSize;

        const dstX = offsetX + tx * tileSize;
        const dstY = offsetY + ty * tileSize;

        this.draw_sprite(
          tileSheet,
          sheetX, sheetY,
          tileSize, tileSize,
          dstX, dstY,
          tileSheetCols * tileSize
        );
      }
    }
  }

  /**
   * Draw a string of text using a built-in 8×8 bitmap font.
   * Characters are pulled from a 128-glyph ASCII font sheet.
   */
  draw_text(
    text: string,
    x: number, y: number,
    colorIndex: number,
    scale: number = 1
  ): void {
    const GLYPH_W = 8;
    const GLYPH_H = 8;

    const rgba = this.palette[colorIndex];
    const r = rgba & 0xFF;
    const g = (rgba >> 8) & 0xFF;
    const b = (rgba >> 16) & 0xFF;

    let cursorX = x;
    for (let i = 0; i < text.length; i++) {
      const charCode = text.charCodeAt(i);
      if (charCode < 32 || charCode > 126) {
        cursorX += GLYPH_W * scale;
        continue;
      }

      // Look up glyph in built-in font bitmap
      const glyphIndex = charCode - 32;
      const glyphCol = glyphIndex % 16;
      const glyphRow = Math.floor(glyphIndex / 16);

      for (let gy = 0; gy < GLYPH_H; gy++) {
        for (let gx = 0; gx < GLYPH_W; gx++) {
          if (this.fontBitSet(glyphCol, glyphRow, gx, gy)) {
            if (scale === 1) {
              this.set_pixel(cursorX + gx, y + gy, r, g, b, 255);
            } else {
              this.fill_rect(
                cursorX + gx * scale,
                y + gy * scale,
                scale, scale,
                r, g, b, 255
              );
            }
          }
        }
      }
      cursorX += GLYPH_W * scale;
    }
  }

  /**
   * Blit a raw RGBA buffer onto the framebuffer at (dstX, dstY).
   * Supports alpha blending.
   */
  blit(
    src: Uint8Array,
    srcW: number, srcH: number,
    dstX: number, dstY: number
  ): void {
    for (let row = 0; row < srcH; row++) {
      for (let col = 0; col < srcW; col++) {
        const si = (row * srcW + col) * 4;
        const sr = src[si], sg = src[si+1], sb = src[si+2], sa = src[si+3];

        if (sa === 0) continue;
        if (sa === 255) {
          this.set_pixel(dstX + col, dstY + row, sr, sg, sb, 255);
        } else {
          this.blendPixel(dstX + col, dstY + row, sr, sg, sb, sa);
        }
      }
    }
    this.markDirty(dstX, dstY, srcW, srcH);
  }

  // ═══════════════════════════════════════════════
  //  GPU Upload
  // ═══════════════════════════════════════════════

  /**
   * Upload the current framebuffer contents to a GPU texture
   * for compositing with the shell layer.
   *
   * Only dirty regions are re-uploaded when possible.
   */
  upload_to_texture(gl: WebGL2RenderingContext): WebGLTexture {
    if (!this.gpuTexture) {
      this.gpuTexture = this.createTexture(gl);
    }

    gl.bindTexture(gl.TEXTURE_2D, this.gpuTexture);

    if (this.isFullDirty()) {
      // Upload entire buffer
      gl.texSubImage2D(
        gl.TEXTURE_2D, 0,
        0, 0,
        VirtualDisplay.WIDTH, VirtualDisplay.HEIGHT,
        gl.RGBA, gl.UNSIGNED_BYTE,
        this.buffer
      );
    } else {
      // Upload only dirty sub-rectangles
      for (const rect of this.dirtyRects) {
        const subImage = this.extractSubImage(rect);
        gl.texSubImage2D(
          gl.TEXTURE_2D, 0,
          rect.x, rect.y,
          rect.w, rect.h,
          gl.RGBA, gl.UNSIGNED_BYTE,
          subImage
        );
      }
    }

    this.dirtyRects = [];
    return this.gpuTexture;
  }

  // ═══════════════════════════════════════════════
  //  Internal Helpers
  // ═══════════════════════════════════════════════

  /** Alpha-blend a single pixel onto the framebuffer. */
  private blendPixel(x: number, y: number,
                     sr: number, sg: number, sb: number, sa: number): void {
    if (!this.inBounds(x, y)) return;
    const o = this.offset(x, y);

    // Standard alpha compositing: out = src * srcA + dst * (1 - srcA)
    const invA = 1.0 - sa / 255.0;
    const srcA = sa / 255.0;

    this.buffer[o]     = Math.round(sr * srcA + this.buffer[o]     * invA);
    this.buffer[o + 1] = Math.round(sg * srcA + this.buffer[o + 1] * invA);
    this.buffer[o + 2] = Math.round(sb * srcA + this.buffer[o + 2] * invA);
    this.buffer[o + 3] = Math.min(255, this.buffer[o + 3] + sa);
  }

  private markDirty(x: number, y: number, w: number, h: number): void {
    this.dirtyRects.push({ x, y, w, h });
  }

  private markFullDirty(): void {
    this.dirtyRects = [{
      x: 0, y: 0,
      w: VirtualDisplay.WIDTH,
      h: VirtualDisplay.HEIGHT
    }];
  }

  private isFullDirty(): boolean {
    return this.dirtyRects.length === 1 &&
           this.dirtyRects[0].w === VirtualDisplay.WIDTH &&
           this.dirtyRects[0].h === VirtualDisplay.HEIGHT;
  }
}
```

### 2.3 Framebuffer Lifecycle (One Frame)

```
┌─────────────────────────────────────────────────────────────────┐
│ Frame N                                                         │
│                                                                 │
│  1. clear()                         ← zero out buffer           │
│  2. draw_tile_layer(bg_layer_0)     ← farthest background       │
│  3. draw_tile_layer(bg_layer_1)     ← mid background            │
│  4. draw_tile_layer(bg_layer_2)     ← near background           │
│  5. for each sprite (sorted by priority):                       │
│       draw_sprite() / draw_sprite_flipped()                     │
│  6. draw_tile_layer(fg_layer)       ← foreground overlay        │
│  7. draw_text() / draw_rect() ...  ← HUD / debug text          │
│  8. upload_to_texture()             ← send to GPU               │
│  9. Composite with Shell layer      ← OOEY draws around it      │
│ 10. Present to screen               ← swap / flip               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Sprite Rendering System

### 3.1 Sprite Data Format

Sprites are stored as **indexed-color** pixel data: one byte per pixel, where each byte is a palette index (0–255). Color index 0 is **always transparent**.

```
Sprite pixel data (indexed, 1 byte per pixel):
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ 00 │ 00 │ 03 │ 03 │ 03 │ 03 │ 00 │ 00 │  Row 0
├────┼────┼────┼────┼────┼────┼────┼────┤
│ 00 │ 03 │ 0F │ 0F │ 0F │ 0F │ 03 │ 00 │  Row 1
├────┼────┼────┼────┼────┼────┼────┼────┤
│ 03 │ 0F │ 01 │ 0F │ 0F │ 01 │ 0F │ 03 │  Row 2  (01 = eye color)
├────┼────┼────┼────┼────┼────┼────┼────┤
│ 03 │ 0F │ 0F │ 0F │ 0F │ 0F │ 0F │ 03 │  Row 3
├────┼────┼────┼────┼────┼────┼────┼────┤
│ 03 │ 0F │ 02 │ 02 │ 02 │ 02 │ 0F │ 03 │  Row 4  (02 = mouth color)
├────┼────┼────┼────┼────┼────┼────┼────┤
│ 03 │ 0F │ 0F │ 0F │ 0F │ 0F │ 0F │ 03 │  Row 5
├────┼────┼────┼────┼────┼────┼────┼────┤
│ 00 │ 03 │ 0F │ 0F │ 0F │ 0F │ 03 │ 00 │  Row 6
├────┼────┼────┼────┼────┼────┼────┼────┤
│ 00 │ 00 │ 03 │ 03 │ 03 │ 03 │ 00 │ 00 │  Row 7
└────┴────┴────┴────┴────┴────┴────┴────┘

00 = transparent (palette index 0)
03 = outline color
0F = skin/fill color
01, 02 = detail colors
```

### 3.2 Sprite Sheets

Multiple sprites are packed into a single **sprite sheet** — a large indexed-color image. Individual sprites are identified by their source rectangle `(srcX, srcY, srcW, srcH)` within the sheet.

```
Sprite sheet layout (example: 128×128 sheet with 8×8 sprites):

     0    8   16   24   32   40   48   56   64   ...  120
   ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
 0 │ 00 │ 01 │ 02 │ 03 │ 04 │ 05 │ 06 │ 07 │ 08 │ .. │ 15 │
   ├────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┤
 8 │ 16 │ 17 │ 18 │ 19 │ 20 │ 21 │ 22 │ 23 │ 24 │ .. │ 31 │
   ├────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┤
16 │ 32 │ 33 │ 34 │ 35 │ .. │    │    │    │    │    │    │
   ├────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┤
   │ .. │    │    │    │    │    │    │    │    │    │    │
   └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘

Each cell = one sprite (8×8 px)
Sprite index → sheet coordinates:
  sheetX = (index % spritesPerRow) * spriteWidth
  sheetY = (index / spritesPerRow) * spriteHeight
```

### 3.3 Sprite Limits

| Limit | Value | Rationale |
|---|---|---|
| Max sprites per frame | 128 | Balances visual richness with CPU budget |
| Max sprite dimensions | 64×64 px | Larger objects composed from multiple sprites |
| Min sprite dimensions | 1×1 px | No minimum enforced |
| Sprite sheet max size | 512×512 px (indexed) | 256 KB per sheet |
| Sprite sheets loaded | Up to 4 simultaneously | One per "bank" |
| Total sprite pixel data | 4 × 512 × 512 = 1 MB | Across all loaded sheets |

### 3.4 Sprite Attributes (Per-Sprite State)

Each active sprite has the following attributes stored in the sprite table:

```typescript
interface SpriteEntry {
  // Identity
  sheetBank: number;      // 0–3: which sprite sheet to source from
  srcX:      number;      // source X in sheet (pixels)
  srcY:      number;      // source Y in sheet (pixels)
  width:     number;      // sprite width (pixels, 1–64)
  height:    number;      // sprite height (pixels, 1–64)

  // Position (world or screen coordinates, depending on mode)
  x:         number;      // destination X (signed, allows partial off-screen)
  y:         number;      // destination Y (signed)

  // Transforms
  flipH:     boolean;     // horizontal mirror
  flipV:     boolean;     // vertical mirror
  scaleX:    number;      // horizontal scale factor (1.0 = normal)
  scaleY:    number;      // vertical scale factor (1.0 = normal)

  // Rendering
  palette:   number;      // palette bank (0–3) for palette swapping
  priority:  number;      // draw order (0 = back, 7 = front)
  visible:   boolean;     // if false, skip during rendering
  alpha:     number;      // opacity (0–255, 255 = fully opaque)
}
```

#### Memory Layout of the Sprite Table

```
128 sprites × 32 bytes each = 4096 bytes (4 KB)

Per-sprite binary layout (32 bytes):

Offset  Size  Field
──────  ────  ──────────────
  0       1   sheetBank
  1       2   srcX           (uint16, little-endian)
  3       2   srcY           (uint16)
  5       1   width
  6       1   height
  7       2   x              (int16, signed)
  9       2   y              (int16, signed)
 11       1   flags          (bit 0: flipH, bit 1: flipV, bit 2: visible)
 12       1   palette
 13       1   priority       (0–7)
 14       1   alpha
 15       2   scaleX         (fixed-point 8.8)
 17       2   scaleY         (fixed-point 8.8)
 19      13   reserved       (padding to 32 bytes)
```

### 3.5 Sprite Rendering Pipeline

```
For each frame:
  1. Collect all sprites where visible == true       (max 128)
  2. Sort by priority (ascending: 0 drawn first, 7 last)
  3. For sprites with equal priority, sort by table index (stable)
  4. For each sprite in sorted order:
     a. Skip if entirely off-screen (early rejection)
     b. Resolve palette bank → actual palette entries
     c. For each pixel in sprite:
        i.   Read indexed color from sprite sheet
        ii.  If index == 0: skip (transparent)
        iii. Look up RGBA from palette
        iv.  Apply flip transforms (swap read coordinates)
        v.   Apply scale transforms (if != 1.0)
        vi.  Alpha-blend onto framebuffer at destination
```

### 3.6 Sprite Scaling Algorithm

When `scaleX` or `scaleY` differ from 1.0, the sprite is drawn at a different size using **nearest-neighbor sampling** (preserving the pixel-art aesthetic):

```typescript
function drawScaledSprite(
  sprite: SpriteEntry,
  sheet: Uint8Array,
  sheetWidth: number,
  display: VirtualDisplay
): void {
  const dstW = Math.round(sprite.width  * sprite.scaleX);
  const dstH = Math.round(sprite.height * sprite.scaleY);

  for (let dy = 0; dy < dstH; dy++) {
    for (let dx = 0; dx < dstW; dx++) {
      // Map destination pixel back to source pixel (nearest neighbor)
      let sx = Math.floor(dx / sprite.scaleX);
      let sy = Math.floor(dy / sprite.scaleY);

      // Apply flipping
      if (sprite.flipH) sx = sprite.width  - 1 - sx;
      if (sprite.flipV) sy = sprite.height - 1 - sy;

      // Clamp to source bounds
      sx = Math.min(sx, sprite.width  - 1);
      sy = Math.min(sy, sprite.height - 1);

      const srcOffset = (sprite.srcY + sy) * sheetWidth + (sprite.srcX + sx);
      const colorIndex = sheet[srcOffset];

      if (colorIndex === 0) continue; // transparent

      const rgba = palette[colorIndex];
      display.set_pixel(
        sprite.x + dx,
        sprite.y + dy,
        rgba & 0xFF,
        (rgba >> 8)  & 0xFF,
        (rgba >> 16) & 0xFF,
        sprite.alpha
      );
    }
  }
}
```

### 3.7 Collision Detection Helpers

The rendering system provides two collision detection strategies for game code:

#### Bounding Box Collision (AABB)

Fast, constant-time check suitable for most gameplay needs:

```typescript
function collide_aabb(
  ax: number, ay: number, aw: number, ah: number,
  bx: number, by: number, bw: number, bh: number
): boolean {
  return ax < bx + bw &&
         ax + aw > bx &&
         ay < by + bh &&
         ay + ah > by;
}
```

#### Pixel-Perfect Collision

More expensive check that considers transparent pixels. Used when AABB reports overlap but the game needs precise hit detection:

```typescript
function collide_pixel(
  spriteA: SpriteEntry, sheetA: Uint8Array, sheetAWidth: number,
  spriteB: SpriteEntry, sheetB: Uint8Array, sheetBWidth: number
): boolean {
  // 1. Compute overlap rectangle
  const overlapX0 = Math.max(spriteA.x, spriteB.x);
  const overlapY0 = Math.max(spriteA.y, spriteB.y);
  const overlapX1 = Math.min(spriteA.x + spriteA.width,
                             spriteB.x + spriteB.width);
  const overlapY1 = Math.min(spriteA.y + spriteA.height,
                             spriteB.y + spriteB.height);

  if (overlapX0 >= overlapX1 || overlapY0 >= overlapY1) return false;

  // 2. Check each pixel in the overlap region
  for (let y = overlapY0; y < overlapY1; y++) {
    for (let x = overlapX0; x < overlapX1; x++) {
      // Sample from sprite A's sheet
      const aLocalX = x - spriteA.x;
      const aLocalY = y - spriteA.y;
      const aOffset = (spriteA.srcY + aLocalY) * sheetAWidth +
                      (spriteA.srcX + aLocalX);
      const aPixel = sheetA[aOffset];

      // Sample from sprite B's sheet
      const bLocalX = x - spriteB.x;
      const bLocalY = y - spriteB.y;
      const bOffset = (spriteB.srcY + bLocalY) * sheetBWidth +
                      (spriteB.srcX + bLocalX);
      const bPixel = sheetB[bOffset];

      // Both non-transparent = collision
      if (aPixel !== 0 && bPixel !== 0) return true;
    }
  }
  return false;
}
```

---

## 4. Tilemap / Background System

### 4.1 Overview

The tilemap system provides efficient, scrollable backgrounds composed of small, repeating tiles. This is the primary mechanism for rendering game worlds — from platformer levels to RPG overworlds.

### 4.2 Layer Architecture

OOEY Station supports **4 independent tile layers** rendered in a fixed order:

```
Draw order (back to front):

  Layer 0  ─── Background 0 (farthest, e.g. sky / distant mountains)
  Layer 1  ─── Background 1 (mid-ground, e.g. hills / buildings)
  Layer 2  ─── Background 2 (near-ground, e.g. platforms / terrain)
  ┄┄┄┄┄┄┄┄┄┄   Sprites are drawn here (interleaved by priority)
  Layer 3  ─── Foreground (closest, e.g. tree canopy / fog overlay)
```

> **Note**: Sprites with priority 0–3 draw *between* layers 2 and 3 (behind the foreground). Sprites with priority 4–7 draw *on top of* layer 3.

### 4.3 Tile Layer Configuration

Each layer has independent settings:

```typescript
interface TileLayer {
  // ─── Map Data ───
  mapData:     Uint16Array;   // tile indices (row-major)
  mapCols:     number;        // map width in tiles
  mapRows:     number;        // map height in tiles

  // ─── Tile Source ───
  tileSheet:   Uint8Array;    // indexed-color tile pixel data
  tileSize:    8 | 16;        // pixels per tile edge (8×8 or 16×16)
  sheetCols:   number;        // tiles per row in the tile sheet

  // ─── Scroll / Position ───
  scrollX:     number;        // horizontal scroll offset (pixels, can be fractional)
  scrollY:     number;        // vertical scroll offset (pixels)
  wrapX:       boolean;       // wrap horizontally (seamless scrolling)
  wrapY:       boolean;       // wrap vertically

  // ─── Rendering ───
  visible:     boolean;       // enable/disable layer
  alpha:       number;        // layer opacity (0–255)
  palette:     number;        // palette bank for this layer

  // ─── Parallax ───
  parallaxX:   number;        // horizontal parallax factor (1.0 = normal speed)
  parallaxY:   number;        // vertical parallax factor
}
```

### 4.4 Tile Sizes

| Size | Pixels | Bytes (indexed) | Tiles to fill screen | Use Case |
|---|---|---|---|---|
| 8×8 | 64 px | 64 bytes | 80 × 60 = 4,800 | Fine detail, fonts, small maps |
| 16×16 | 256 px | 256 bytes | 40 × 30 = 1,200 | General gameplay, platformers |

### 4.5 Tile Map Memory Layout

The tile map is a flat array of 16-bit tile indices:

```
Tile map (Uint16Array), row-major:

  Index formula: mapData[row * mapCols + col] = tileIndex

  tileIndex = 0          → empty (nothing drawn)
  tileIndex = 1..65535   → valid tile from tile sheet

Example: 64×32 tile map with 16×16 tiles
  World size: 64×16 = 1024 px wide, 32×16 = 512 px tall
  Map memory:  64 × 32 × 2 = 4096 bytes (4 KB)
```

### 4.6 Tile Attributes (Packed in Map Data)

Each 16-bit entry in the tile map encodes more than just the tile index. The upper bits carry per-tile attributes:

```
Bit layout of a 16-bit tile map entry:

  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
 ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
 │ P │FV │FH │PAL│PAL│ T │ T │ T │ T │ T │ T │ T │ T │ T │ T │ T │
 └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘

 Bits 0–10  (11 bits): Tile index (0–2047)
 Bit  11:              Reserved (must be 0)
 Bits 12–13 (2 bits):  Palette bank (0–3)
 Bit  14:              Flip horizontal
 Bit  15:              Flip vertical / Priority (overloaded, see below)
```

#### Decoding a Tile Entry

```typescript
function decodeTileEntry(entry: number) {
  return {
    tileIndex: entry & 0x07FF,           // bits 0–10
    palette:   (entry >> 12) & 0x03,     // bits 12–13
    flipH:     !!(entry & 0x4000),       // bit 14
    flipV:     !!(entry & 0x8000),       // bit 15
  };
}
```

### 4.7 Tile Rendering Algorithm

For each visible tile layer, each frame:

```
Given:
  camera_x, camera_y   = world-space camera position
  parallax_x, parallax_y = layer parallax factors

Effective scroll:
  eff_scroll_x = camera_x * parallax_x + layer.scrollX
  eff_scroll_y = camera_y * parallax_y + layer.scrollY

Visible tile range:
  start_tile_x = floor(eff_scroll_x / tile_size)
  start_tile_y = floor(eff_scroll_y / tile_size)
  sub_tile_x   = -(eff_scroll_x % tile_size)     // pixel offset within first tile
  sub_tile_y   = -(eff_scroll_y % tile_size)

  tiles_across = ceil(640 / tile_size) + 1        // +1 for partial tiles at edges
  tiles_down   = ceil(480 / tile_size) + 1

For ty = 0 to tiles_down:
  For tx = 0 to tiles_across:
    map_x = start_tile_x + tx
    map_y = start_tile_y + ty

    if layer.wrapX: map_x = map_x % map_cols  (handle negative)
    if layer.wrapY: map_y = map_y % map_rows

    if out of bounds and no wrap: skip

    entry = mapData[map_y * map_cols + map_x]
    { tileIndex, palette, flipH, flipV } = decode(entry)

    if tileIndex == 0: skip

    screen_x = sub_tile_x + tx * tile_size
    screen_y = sub_tile_y + ty * tile_size

    Draw tile from tile sheet at (screen_x, screen_y)
    using palette bank, applying flip flags
```

### 4.8 Parallax Scrolling

Parallax creates depth by scrolling layers at different rates relative to the camera:

```
Layer  parallaxX  parallaxY  Effect
─────  ─────────  ─────────  ──────────────────────────────────
  0      0.25       0.25     Distant sky — moves slowly
  1      0.50       0.50     Mid-ground hills — half speed
  2      1.00       1.00     Main gameplay layer — full speed
  3      1.25       1.00     Foreground overlay — slight overdrive
```

```
Example: Camera moves right by 100 pixels

  Layer 0 scrolls:  100 × 0.25 = 25 px   (feels far away)
  Layer 1 scrolls:  100 × 0.50 = 50 px
  Layer 2 scrolls:  100 × 1.00 = 100 px  (matches camera exactly)
  Layer 3 scrolls:  100 × 1.25 = 125 px  (foreground rushes past)
```

### 4.9 Tile Layer Memory Budget

```
Per layer:
  Map data:     max 256 × 256 × 2 bytes    = 128 KB
  Tile sheet:   max 256 × 256 × 1 byte     =  64 KB
  Layer config: ~64 bytes

Total across 4 layers:
  Map data:     4 × 128 KB = 512 KB
  Tile sheets:  4 ×  64 KB = 256 KB   (sheets may be shared)
  ────────────────────────────────
  Maximum:      ~768 KB (conservative upper bound)
```

---

## 5. Color Palette System

### 5.1 Architecture

OOEY Station uses a **256-color indexed palette** system, mirroring classic hardware that separated color definition from pixel data.

```
Palette flow:

  Sprite/Tile pixel data          Palette table              Framebuffer
  (indexed, 1 byte/px)           (256 entries)              (RGBA, 4 bytes/px)

  ┌─────┐                    ┌──────────────────┐         ┌──────────────────┐
  │ 0x0F│ ── index 0x0F ──→ │ [0x0F] = #3A7FD5 │ ──→     │ R:3A G:7F B:D5   │
  └─────┘                    └──────────────────┘         └──────────────────┘
```

### 5.2 Palette Table Structure

```
256 entries × 4 bytes (RGBA) = 1024 bytes per palette

Stored as Uint32Array[256], packed RGBA (little-endian: 0xAABBGGRR)

Entry layout:
  Bits  0– 7: Red   (0–255)
  Bits  8–15: Green (0–255)
  Bits 16–23: Blue  (0–255)
  Bits 24–31: Alpha (0–255, typically 255 for opaque, 0 for index 0)
```

#### Memory Diagram

```
Palette (1024 bytes):

Offset  Index   R     G     B     A     Hex Color    Description
──────  ─────  ────  ────  ────  ────   ─────────    ────────────────────
0x000   [  0]  0x00  0x00  0x00  0x00   #000000 00   Transparent (always)
0x004   [  1]  0x0C  0x0C  0x0E  0xFF   #0C0C0E FF   Near-black
0x008   [  2]  0xFF  0xFF  0xFF  0xFF   #FFFFFF FF   White
0x00C   [  3]  0xE0  0x40  0x40  0xFF   #E04040 FF   Red
0x010   [  4]  0x40  0xB0  0x40  0xFF   #40B040 FF   Green
0x014   [  5]  0x40  0x60  0xE0  0xFF   #4060E0 FF   Blue
  ...   ...    ...   ...   ...   ...    ...          ...
0x3FC   [255]  0x80  0x80  0x80  0xFF   #808080 FF   Last entry
```

### 5.3 Palette Banks

The system supports **4 palette banks**, each containing 256 colors. This enables palette swapping per-sprite or per-tile layer without modifying pixel data.

```
Bank 0: Default game palette          (entries 0–255)
Bank 1: Alternative palette (e.g. underwater tint)
Bank 2: Enemy variant palette
Bank 3: UI / HUD palette

Total palette memory: 4 × 1024 = 4096 bytes (4 KB)

Palette bank selection:
  - Sprites: per-sprite 'palette' field selects bank 0–3
  - Tile layers: per-layer 'palette' field selects bank 0–3
  - Per-tile: 2-bit palette override in tile map entry (bits 12–13)
```

### 5.4 Transparency

Index 0 is **always transparent** — any pixel with color index 0 is skipped during rendering and the framebuffer pixel beneath it is preserved.

```
Transparency rules:
  ┌─────────────────────────────────────────────────────────┐
  │ 1. Palette index 0 = fully transparent (never drawn)    │
  │ 2. Palette indices 1–255 = opaque by default            │
  │ 3. Per-sprite alpha modulates ALL non-transparent pixels│
  │ 4. Per-layer alpha modulates entire tile layer           │
  │ 5. clear() sets all pixels to chosen color (fully opaque)│
  └─────────────────────────────────────────────────────────┘
```

### 5.5 Palette Animation (Color Cycling)

Color cycling animates by rotating palette entries over time. This creates effects like flowing water, shimmering metal, or pulsing glow — without modifying any pixel data.

```typescript
interface PaletteCycleConfig {
  startIndex:  number;   // first palette index in the cycle range
  endIndex:    number;   // last palette index in the cycle range (inclusive)
  speed:       number;   // frames between each rotation step
  direction:   1 | -1;   // 1 = forward, -1 = reverse
  mode:        'loop' | 'pingpong';
}
```

#### Color Cycling Algorithm

```typescript
function updatePaletteCycle(
  palette: Uint32Array,
  config: PaletteCycleConfig,
  frameCount: number
): void {
  if (frameCount % config.speed !== 0) return;

  const { startIndex, endIndex, direction } = config;
  const rangeLen = endIndex - startIndex + 1;

  if (direction === 1) {
    // Rotate forward: last entry moves to first position
    const last = palette[endIndex];
    for (let i = endIndex; i > startIndex; i--) {
      palette[i] = palette[i - 1];
    }
    palette[startIndex] = last;
  } else {
    // Rotate backward: first entry moves to last position
    const first = palette[startIndex];
    for (let i = startIndex; i < endIndex; i++) {
      palette[i] = palette[i + 1];
    }
    palette[endIndex] = first;
  }
}
```

#### Example: Flowing Water

```
Frame 0:  indices 32–39 → [dark blue, blue, light blue, cyan, white, cyan, light blue, blue]
Frame 4:  indices 32–39 → [blue, dark blue, blue, light blue, cyan, white, cyan, light blue]
Frame 8:  indices 32–39 → [light blue, blue, dark blue, blue, light blue, cyan, white, cyan]
...

Water tile pixels reference indices 32–39.
As the palette rotates, the water appears to flow — zero CPU cost per pixel.
```

### 5.6 Palette Swapping for Sprite Recoloring

Games commonly create enemy variants (red knight → blue knight) by swapping palette banks rather than duplicating sprite art:

```
Original sprite uses palette bank 0:
  Index 10 → #C04040 (red armor)
  Index 11 → #D06060 (red highlight)
  Index 12 → #802020 (red shadow)

Palette bank 2 (ice variant):
  Index 10 → #4040C0 (blue armor)
  Index 11 → #6060D0 (blue highlight)
  Index 12 → #202080 (blue shadow)

Same sprite pixel data, set sprite.palette = 2 → instant recolor.
```

#### Palette Swap Implementation

```typescript
function resolveColor(
  colorIndex: number,
  paletteBank: number,
  palettes: Uint32Array[]   // array of 4 palettes, each 256 entries
): number {
  if (colorIndex === 0) return 0;   // transparent
  return palettes[paletteBank][colorIndex];
}
```

---

## 6. Drawing Primitives Available to VM

The Booey VM exposes the following drawing operations as syscalls. Game code invokes these via the VM instruction set (see VM Architecture document for opcode details).

### 6.1 Complete Primitive Reference

| # | Syscall Name | Arguments | Description |
|---|---|---|---|
| 0x40 | `gfx_clear` | `r, g, b` | Clear entire framebuffer to RGB color (A=255) |
| 0x41 | `gfx_set_pixel` | `x, y, colorIndex` | Set single pixel using palette color |
| 0x42 | `gfx_get_pixel` | `x, y → colorIndex` | Read palette index at pixel (returns 0 if out of bounds) |
| 0x43 | `gfx_draw_rect` | `x, y, w, h, colorIndex` | Draw rectangle outline |
| 0x44 | `gfx_fill_rect` | `x, y, w, h, colorIndex` | Draw filled rectangle |
| 0x45 | `gfx_draw_line` | `x0, y0, x1, y1, colorIndex` | Draw line (Bresenham) |
| 0x46 | `gfx_draw_circle` | `cx, cy, radius, colorIndex` | Draw circle outline (Midpoint algorithm) |
| 0x47 | `gfx_fill_circle` | `cx, cy, radius, colorIndex` | Draw filled circle (horizontal scanlines) |
| 0x48 | `gfx_draw_sprite` | `sheetBank, srcX, srcY, w, h, dstX, dstY` | Draw sprite from sheet |
| 0x49 | `gfx_draw_sprite_flip` | `sheetBank, srcX, srcY, w, h, dstX, dstY, flags` | Draw sprite with flip (bit 0=H, bit 1=V) |
| 0x4A | `gfx_draw_sprite_scaled` | `sheetBank, srcX, srcY, w, h, dstX, dstY, scaleX, scaleY` | Draw scaled sprite (fixed-point 8.8) |
| 0x4B | `gfx_draw_tile_layer` | `layerIndex` | Render entire tile layer using current scroll state |
| 0x4C | `gfx_draw_text` | `strPtr, strLen, x, y, colorIndex, scale` | Draw text string using built-in font |
| 0x4D | `gfx_blit` | `srcPtr, srcW, srcH, dstX, dstY` | Blit raw RGBA buffer from VM memory |
| 0x4E | `gfx_draw_tri` | `x0, y0, x1, y1, x2, y2, colorIndex` | Draw filled triangle (scanline) |
| 0x4F | `gfx_set_clip_rect` | `x, y, w, h` | Set clipping rectangle (drawing is masked outside) |
| 0x50 | `gfx_reset_clip` | — | Reset clipping to full screen (640×480) |
| 0x51 | `gfx_scroll_buffer` | `dx, dy` | Scroll framebuffer contents by (dx, dy); vacated area filled with clear color |

### 6.2 Palette & Color Syscalls

| # | Syscall Name | Arguments | Description |
|---|---|---|---|
| 0x60 | `pal_set_color` | `bank, index, r, g, b` | Set a single palette entry |
| 0x61 | `pal_get_color` | `bank, index → r, g, b` | Read a palette entry |
| 0x62 | `pal_load` | `bank, dataPtr, count` | Bulk-load palette entries from VM memory |
| 0x63 | `pal_copy` | `srcBank, dstBank, start, count` | Copy palette range between banks |
| 0x64 | `pal_cycle_setup` | `bank, start, end, speed, dir` | Configure color cycling |
| 0x65 | `pal_cycle_enable` | `bank, enabled` | Start/stop a color cycle |
| 0x66 | `pal_fade` | `bank, factor` | Fade entire palette toward black (factor 0–255, 255=normal) |
| 0x67 | `pal_fade_to` | `bank, r, g, b, factor` | Fade entire palette toward target color |

### 6.3 Tile Layer Syscalls

| # | Syscall Name | Arguments | Description |
|---|---|---|---|
| 0x70 | `tile_set_map` | `layer, dataPtr, cols, rows` | Set tile map data for a layer |
| 0x71 | `tile_set_sheet` | `layer, sheetBank` | Assign a sprite sheet as tile source |
| 0x72 | `tile_set_scroll` | `layer, scrollX, scrollY` | Set scroll position |
| 0x73 | `tile_set_parallax` | `layer, factorX, factorY` | Set parallax factors (fixed-point 8.8) |
| 0x74 | `tile_set_visible` | `layer, visible` | Show/hide layer |
| 0x75 | `tile_set_wrap` | `layer, wrapX, wrapY` | Enable/disable map wrapping |
| 0x76 | `tile_set_tile` | `layer, col, row, entry` | Set single tile (with attributes) |
| 0x77 | `tile_get_tile` | `layer, col, row → entry` | Read single tile entry |
| 0x78 | `tile_set_size` | `layer, size` | Set tile size (8 or 16) |

### 6.4 Sprite Table Syscalls

| # | Syscall Name | Arguments | Description |
|---|---|---|---|
| 0x80 | `spr_set` | `index, sheetBank, srcX, srcY, w, h` | Configure sprite source |
| 0x81 | `spr_pos` | `index, x, y` | Set sprite position |
| 0x82 | `spr_flip` | `index, flipH, flipV` | Set sprite flip flags |
| 0x83 | `spr_scale` | `index, scaleX, scaleY` | Set sprite scale (fixed-point 8.8) |
| 0x84 | `spr_palette` | `index, bank` | Set sprite palette bank |
| 0x85 | `spr_priority` | `index, priority` | Set sprite draw order (0–7) |
| 0x86 | `spr_visible` | `index, visible` | Show/hide sprite |
| 0x87 | `spr_alpha` | `index, alpha` | Set sprite opacity (0–255) |
| 0x88 | `spr_collide_aabb` | `indexA, indexB → bool` | Test AABB collision between two sprites |
| 0x89 | `spr_collide_pixel` | `indexA, indexB → bool` | Test pixel-perfect collision |
| 0x8A | `spr_reset` | `index` | Reset sprite to default state (invisible) |
| 0x8B | `spr_reset_all` | — | Reset all 128 sprite entries |

### 6.5 Clipping Region

All drawing operations respect the current clipping rectangle. Pixels outside the clip rect are silently discarded:

```
Default clip rect: (0, 0, 640, 480) — full screen

gfx_set_clip_rect(100, 50, 440, 380):
  ┌──────────────────────────────────────────────┐
  │ (0,0)               640                      │
  │                                              │
  │     ┌────────────────────────────────┐       │
  │     │ (100,50)                       │       │
  │     │     Clipped drawing area       │       │
  │     │     440 × 380 pixels           │       │
  │     │                                │       │
  │     └────────────────────────────────┘       │
  │                                    (540,430) │
  │                                        480   │
  └──────────────────────────────────────────────┘

Pixels drawn outside the dashed region are discarded.
```

---

## 7. Performance Considerations

### 7.1 Target Performance

| Metric | Target | Notes |
|---|---|---|
| Frame rate | 60 FPS (16.67 ms per frame) | VSync-locked when possible |
| Frame budget — game logic | ≤ 4 ms | VM tick + game update |
| Frame budget — rendering | ≤ 8 ms | All drawing ops + texture upload |
| Frame budget — compositing | ≤ 2 ms | Shell layer + present |
| Frame budget — headroom | ~2.67 ms | GC, OS scheduling, variance |

### 7.2 Dirty Rectangle Tracking

Instead of uploading the entire 1.2 MB framebuffer every frame, the system tracks which regions were modified and uploads only those sub-rectangles to the GPU texture.

#### Algorithm

```
1. Each draw call records a bounding dirty rect:
     markDirty(x, y, width, height)

2. At upload time:
     if only_one_rect_covering_full_screen:
       upload entire buffer (cheaper than per-rect overhead)
     else:
       merge overlapping rects (union)
       for each merged rect:
         extract sub-image from buffer
         glTexSubImage2D(rect.x, rect.y, rect.w, rect.h, ...)

3. After upload:
     clear dirty rect list
```

#### Dirty Rect Merging

```typescript
function mergeDirtyRects(rects: Rect[]): Rect[] {
  if (rects.length <= 1) return rects;

  // Sort by Y, then X
  rects.sort((a, b) => a.y - b.y || a.x - b.x);

  const merged: Rect[] = [rects[0]];

  for (let i = 1; i < rects.length; i++) {
    const curr = rects[i];
    const prev = merged[merged.length - 1];

    // Check if rects overlap or are adjacent
    if (curr.x <= prev.x + prev.w &&
        curr.y <= prev.y + prev.h) {
      // Merge: expand prev to cover both
      const x1 = Math.min(prev.x, curr.x);
      const y1 = Math.min(prev.y, curr.y);
      const x2 = Math.max(prev.x + prev.w, curr.x + curr.w);
      const y2 = Math.max(prev.y + prev.h, curr.y + curr.h);
      merged[merged.length - 1] = { x: x1, y: y1, w: x2 - x1, h: y2 - y1 };
    } else {
      merged.push(curr);
    }
  }

  // If merged area exceeds 60% of total screen, just upload everything
  const totalArea = merged.reduce((sum, r) => sum + r.w * r.h, 0);
  if (totalArea > VirtualDisplay.WIDTH * VirtualDisplay.HEIGHT * 0.6) {
    return [{ x: 0, y: 0, w: VirtualDisplay.WIDTH, h: VirtualDisplay.HEIGHT }];
  }

  return merged;
}
```

### 7.3 Double Buffering

The rendering pipeline uses logical double buffering to prevent tearing and partial-frame display:

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  Frame N                          Frame N+1                     │
│  ┌──────────────────────┐        ┌──────────────────────┐       │
│  │ Back Buffer           │        │ Back Buffer           │      │
│  │ (VM draws here)       │   ──→  │ (VM draws here)       │      │
│  │                       │  swap  │                       │      │
│  └──────────────────────┘        └──────────────────────┘       │
│            ↕ swap                          ↕ swap               │
│  ┌──────────────────────┐        ┌──────────────────────┐       │
│  │ Front Buffer          │        │ Front Buffer          │      │
│  │ (displayed on screen) │        │ (displayed on screen) │      │
│  └──────────────────────┘        └──────────────────────┘       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

Implementation:
  - Two Uint8Array buffers of 1,228,800 bytes each
  - VM always writes to the back buffer
  - At frame end, swap buffer pointers (O(1), no copy)
  - GPU texture upload reads from the (new) front buffer
  - Total memory cost: ~2.4 MB for both buffers
```

### 7.4 Blit Optimization Strategies

#### Row-at-a-Time Copy

For opaque rectangular blits (no alpha, no transparency), copy entire rows with `TypedArray.set()` instead of pixel-by-pixel:

```typescript
function fastBlit(
  src: Uint8Array, srcW: number, srcH: number, srcStride: number,
  dst: Uint8Array, dstX: number, dstY: number, dstStride: number
): void {
  for (let row = 0; row < srcH; row++) {
    const srcOffset = row * srcStride;
    const dstOffset = ((dstY + row) * (dstStride / 4) + dstX) * 4;
    dst.set(
      src.subarray(srcOffset, srcOffset + srcW * 4),
      dstOffset
    );
  }
}
```

#### 32-bit Pixel Operations

Use `Uint32Array` views for single-pixel read/write and comparison operations. One 32-bit write replaces four 8-bit writes:

```typescript
// Slow: 4 byte writes
buffer[o]     = r;
buffer[o + 1] = g;
buffer[o + 2] = b;
buffer[o + 3] = a;

// Fast: 1 uint32 write
const view32 = new Uint32Array(buffer.buffer);
view32[pixelIndex] = (a << 24) | (b << 16) | (g << 8) | r;
```

#### Sprite Tile Caching

For sprites and tiles that are drawn unchanged across frames (same palette, same source rect), cache the fully resolved RGBA tile in a small LRU cache to skip palette lookups:

```typescript
interface TileCache {
  capacity: number;                           // e.g. 512 entries
  entries: Map<string, Uint8Array>;           // key → RGBA pixels
}

function cacheKey(sheetBank: number, srcX: number, srcY: number,
                  tileSize: number, paletteBank: number, flipH: boolean,
                  flipV: boolean): string {
  return `${sheetBank}:${srcX},${srcY}:${tileSize}:${paletteBank}:${flipH?1:0}${flipV?1:0}`;
}

// On cache hit: blit pre-resolved RGBA directly (skip indexed lookup)
// On cache miss: resolve from indexed → RGBA, store in cache, then blit
```

### 7.5 Rendering Cost Estimates

Approximate per-frame costs on a mid-range device:

| Operation | Typical Count | Cost Per | Frame Total |
|---|---|---|---|
| `clear()` | 1 | 0.3 ms | 0.3 ms |
| `draw_tile_layer` (16×16) | 3 layers × 1,200 tiles | ~0.5 µs/tile | 1.8 ms |
| `draw_sprite` (16×16) | 64 sprites | ~8 µs/sprite | 0.5 ms |
| `draw_sprite_scaled` (32×32) | 16 sprites | ~25 µs/sprite | 0.4 ms |
| `draw_text` (20 chars) | 5 strings | ~15 µs/string | 0.08 ms |
| `fill_rect` | 10 rects | ~5 µs/rect | 0.05 ms |
| Dirty rect merge | 1 | 0.02 ms | 0.02 ms |
| `upload_to_texture` (partial) | 1 | 0.2–1.0 ms | ~0.5 ms |
| **Rendering subtotal** | | | **~3.65 ms** |

This leaves comfortable headroom within the 8 ms rendering budget.

### 7.6 Frame Timing & VSync

```
VSync-locked frame loop:

  requestAnimationFrame(onFrame)

  function onFrame(timestamp):
    dt = timestamp - lastTimestamp
    lastTimestamp = timestamp

    // Fixed timestep for game logic (determinism)
    accumulator += dt
    while accumulator >= TICK_MS (16.67ms):
      vm.tick()           // advance game state
      accumulator -= TICK_MS

    // Variable timestep for rendering (always render latest state)
    display.render()       // execute all queued draw calls
    display.upload()       // send to GPU
    shell.composite()      // draw OOEY shell on top
    present()              // browser/GPU swap

    requestAnimationFrame(onFrame)
```

### 7.7 Performance Monitoring

The rendering pipeline exposes diagnostic counters (dev mode only):

```typescript
interface RenderStats {
  frameTimeMs:       number;    // total frame time
  renderTimeMs:      number;    // drawing operations only
  uploadTimeMs:      number;    // GPU texture upload
  compositeTimeMs:   number;    // shell layer compositing

  spritesDrawn:      number;    // sprites rendered this frame
  tilesDrawn:        number;    // tiles rendered this frame
  primitivesDrawn:   number;    // rects, circles, lines

  dirtyRectCount:    number;    // dirty rects before merge
  mergedRectCount:   number;    // dirty rects after merge
  uploadedPixels:    number;    // total pixels sent to GPU

  tileCacheHits:     number;    // resolved from cache
  tileCacheMisses:   number;    // had to resolve from indexed data
}
```

---

## Appendix A: Quick Reference — Memory Budget Summary

| Resource | Size | Notes |
|---|---|---|
| Framebuffer (×2, double buffered) | 2 × 1,228,800 = 2.4 MB | RGBA8888, 640×480 |
| Palette banks (×4) | 4 × 1,024 = 4 KB | 256 colors × 4 bytes × 4 banks |
| Sprite table | 4 KB | 128 entries × 32 bytes |
| Sprite sheets (×4) | 4 × 262,144 = 1 MB | 512×512 indexed, max |
| Tile maps (×4) | 4 × 131,072 = 512 KB | 256×256 × 2 bytes, max |
| Tile sheets (×4) | Shared with sprite sheets | — |
| Tile cache (LRU) | ~128 KB | 512 entries × 256 bytes avg |
| Built-in font | ~12 KB | 96 glyphs × 8×8 × 1-bit + metadata |
| **Total VRAM budget** | **~4.1 MB** | Conservative upper bound |

## Appendix B: Quick Reference — Syscall Numbers

```
Graphics:   0x40–0x51
Palette:    0x60–0x67
Tile Layers:0x70–0x78
Sprites:    0x80–0x8B
```

## Appendix C: Coordinate System Summary

```
  (0,0) ──────────────────────── (639,0)
    │                                │
    │        Y increases              │
    │        ↓ downward               │
    │                                │
    │        X increases →            │
    │                                │
  (0,479) ─────────────────────── (639,479)

  Origin: top-left
  X axis: left to right (0–639)
  Y axis: top to bottom (0–479)
  Pixel coordinates are integers
  Sub-pixel positions are not supported (nearest-neighbor only)
```
