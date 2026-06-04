# VeloxOS

A hobby x86-64 operating system built from scratch targeting UEFI firmware directly. No GRUB, no BIOS compatibility layer. VeloxOS boots via its own `BOOTX64.EFI` application, initialises a complete hardware abstraction stack, and launches a graphical desktop environment backed by a custom filesystem.

---

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Boot Process](#boot-process)
- [Memory Management](#memory-management)
- [Interrupt Handling](#interrupt-handling)
- [Graphics Subsystem](#graphics-subsystem)
- [Input Subsystem](#input-subsystem)
- [Desktop Environment](#desktop-environment)
- [VeloxFS & RAM Disk](#veloxfs--ram-disk)
- [Console](#console)
- [Kernel Standard Library](#kernel-standard-library)
- [Build System](#build-system)
- [Running](#running)
- [Repository Structure](#repository-structure)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                     kdesktop (GUI)                      │
│         Windows · Taskbar · Icons · Terminal            │
├───────────────────┬─────────────────────────────────────┤
│   VeloxFS         │        Double Buffer / baregfx      │
│   (filesystem)    │        (compositor + damage rect)   │
├───────────────────┴──────────┬──────────────────────────┤
│         RAM Disk (block dev) │   Console (TTF renderer) │
├──────────────────────────────┴──────────────────────────┤
│          Event Queue · Keyboard Driver · Mouse Driver   │
├─────────────────────────────────────────────────────────┤
│       IDT · PIC · PIT · ISR stubs (interrupts.asm)      │
├─────────────────────────────────────────────────────────┤
│  VMM (4-level paging)  │  PMM (bitmap)  │  Heap (list)  │
├─────────────────────────────────────────────────────────┤
│              UEFI GOP Framebuffer / Hardware            │
└─────────────────────────────────────────────────────────┘
```

All layers are implemented from scratch in C and x86-64 NASM. There are no external runtime dependencies beyond `gnu-efi` for the UEFI boot handoff, and `stb_truetype` / `stb_sprintf` (single-header libraries) for font rasterisation and formatted output.

---

## Boot Process

The kernel entry point is `efi_main` in `kernel_native.c`, conforming to the UEFI application ABI.

**Sequence:**

1. `InitializeLib` — gnu-efi boilerplate, initialises `BS` (Boot Services) and `ST` (System Table).
2. `LocateProtocol(EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID)` — acquires the GOP framebuffer address, resolution, and pixels-per-scanline from firmware. These values are stored in the global `fb_base`, `ScreenWidth`, `ScreenHeight`, and `PixelsPerScanLine`.
3. `init_ttf_font(16)` — called while Boot Services are still active so UEFI memory allocation can back font glyph tables.
4. `exit_uefi_services` — calls `GetMemoryMap` twice (once to size, once to fill), then `ExitBootServices`. The UEFI memory map descriptor array is preserved for PMM consumption.
5. After `ExitBootServices`, Boot Services and UEFI runtime calls are no longer available. All subsequent allocations go through the kernel heap.
6. `console_init` — binds the GOP framebuffer to the software console.
7. `pmm_init` — parses the saved UEFI memory map and builds the physical page bitmap.
8. `vmm_create_kernel_pml4` — constructs a new 4-level page table identity-mapping the first 4 GB and the GOP framebuffer, then loads it into `CR3`.
9. `db_init` — initialises the double buffer compositor.
10. `idt_init` / `pic_init` / `pit_init` — hardware interrupt infrastructure.
11. `keyboard_init_state` / `mouse_init_state` / `mouse_enable` — PS/2 input drivers.
12. `sti` — interrupts enabled.
13. Main loop: `hlt` until an interrupt fires, then drain the event queue and dispatch to `handle_input`.

`kdesktop_run` is entered from the shell command layer and takes over from there.

---

## Memory Management

### Physical Memory Manager (`pmm.c`)

A flat bitmap allocator over physical RAM.

- **Initialisation:** Iterates the UEFI memory map. All pages are initially marked used (`0xFF`). Pages typed `EfiConventionalMemory` are freed into the bitmap. The bitmap itself is placed in the first suitably large conventional region at or above `0x1000000` (16 MB), then locked.
- **Allocation strategy:** Next-fit with a persistent `last_index` hint. Single-page allocation scans from `last_index` forward, wrapping once. Multi-page allocation searches for contiguous runs, skipping ahead on collision to avoid redundant per-page checks.
- **Deallocation:** Marks pages free, updates `last_index` to the lower of current hint and freed address to accelerate the next search.
- **Bitmap encoding:** 1 bit per 4 KB page, set = allocated, clear = free.

```c
uint64_t pmm_alloc_page(void);            // Returns physical address or 0
uint64_t pmm_alloc_pages(uint64_t count); // Contiguous run
void     pmm_dealloc_page(uint64_t addr);
void     pmm_dealloc_pages(uint64_t addr, uint64_t count);
uint64_t pmm_get_free_pages(void);
uint64_t pmm_get_total_pages(void);
```

### Virtual Memory Manager (`vmm.c`)

Standard x86-64 four-level paging: PML4 → PDPT → PD → PT. Each table level is one 4 KB page allocated on demand from the PMM.

- `vmm_map_page(pml4, virt, phys, flags)` — walks or creates all four table levels, writes the leaf PTE, and issues `invlpg` to flush the TLB entry.
- `vmm_create_kernel_pml4()` — builds a kernel address space by identity-mapping every 4 KB page in the first 4 GB (`0x0` – `0xFFFFFFFF`) plus the GOP framebuffer region (which may reside above 4 GB on some firmware). The resulting PML4 physical address is loaded into `CR3`.

Flags used throughout: `PAGE_PRESENT | PAGE_WRITE` (`0x3`).

### Kernel Heap (`memory.c`)

An intrusive linked-list allocator layered over the PMM.

- **Pre-`ExitBootServices`:** `kmalloc` / `kfree` delegate to UEFI `AllocatePool` / `FreePool`.
- **Post-`ExitBootServices`:** `kmalloc` walks the heap node list for a free block of sufficient size (first-fit). If none exists, it calls `pmm_alloc_pages` for as many pages as the request plus header requires, links the new node in, and returns. `kfree` marks the node free for future reuse without returning pages to the PMM. Allocation is 8-byte aligned. `my_realloc` is a copy-alloc-free triple.

```c
void* kmalloc(UINTN size);
void  kfree(void* ptr);
void* my_realloc(void* ptr, UINTN old_size, UINTN new_size);
```

**Allocation ordering is critical.** The heap is a linear chain; large allocations (RAM disk, filesystem structures) must be made before small persistent objects (window structs, icon structs) to avoid clobbering them.

---

## Interrupt Handling

### IDT (`idt.c`, `interrupts.asm`)

256-entry IDT, each gate 16 bytes, loaded via `lidt`. Gates use the code segment value read from `CS` at init time.

**Exception vectors installed (0–31):**

| Vector | Exception |
|--------|-----------|
| 0 | Divide by Zero |
| 1 | Debug |
| 2 | NMI |
| 3 | Breakpoint |
| 6 | Invalid Opcode |
| 8 | Double Fault (error code) |
| 13 | General Protection Fault (error code) |
| 14 | Page Fault (error code) |

**IRQ vectors installed (32–47, remapped from PIC):**

| Vector | IRQ | Device |
|--------|-----|--------|
| 32 | 0 | PIT Timer |
| 33 | 1 | PS/2 Keyboard |
| 44 | 12 | PS/2 Mouse |

### Assembly stubs (`interrupts.asm`)

NASM macros generate three stub variants:

- `ISR_NOERRCODE n` — pushes a synthetic zero error code and the vector number, jumps to `isr_common`.
- `ISR_ERRCODE n` — the CPU has already pushed an error code; pushes the vector number, jumps to `isr_common`.
- `IRQ n, irq_num` — pushes zero and the logical IRQ number, jumps to `irq_common`.

Both `isr_common` and `irq_common` save all 15 general-purpose registers (RAX–R15), set up the three C ABI arguments (`rdi` → interrupt frame, `rsi` → error code or IRQ number, `rdx` → vector for exceptions), and call the appropriate C handler. `interrupt_exit` restores registers and executes `iretq`.

### PIC (`kernel_native.c`)

8259A master/slave pair initialised in ICW1–ICW4 sequence. IRQs remapped to vectors 32–47. All IRQ lines unmasked. EOI is sent by `irq_handler` in `idt.c` after each handler returns; slave EOI sent additionally for IRQ ≥ 8.

### PIT (`kernel_native.c`)

Channel 0, Mode 2 (rate generator), divisor `1193180 / 100` = 100 Hz. Each tick increments `system_ticks` in `timer.c`. `get_uptime_ms()` returns `system_ticks * 10`.

---

## Graphics Subsystem

### `sysfb.h` — Static Back Buffer Pool

A single `static unsigned char g_back_buffer_pool[1920 * 1080 * 4]` in BSS, 4 KB aligned. `sys_allocate_back_buffer` returns a pointer into this pool if the requested framebuffer size fits, without any heap allocation. This eliminates heap fragmentation from the largest single allocation in the system.

### `baregfx.h` — Compositor (`bgfx_context`)

A zero-dependency single-header graphics layer.

**Virtual coordinate system:** All drawing coordinates are expressed in the range 0–10000, where 10000 = 100% of screen width or height. `bgfx_v2p_x` / `bgfx_v2p_y` convert to physical pixels at draw time, decoupling rendering logic from resolution.

**Damage tracking:** `bgfx_context` maintains a dirty rectangle (`dirty_min_x/y`, `dirty_max_x/y`). Every draw call expands this rectangle via `bgfx_mark_dirty`. `bgfx_swap_buffers` copies only the dirty region from the back buffer to the GOP front buffer, then clears the dirty flag. On static frames, no copy occurs.

**Primitives:**
- `bgfx_draw_rect_v` — filled rectangle in virtual coordinates, writes directly to back buffer.
- `bgfx_blit_surface_v` — nearest-neighbour scaled blit of an arbitrary pixel buffer with optional fast 50% alpha blend (single bit-shift per channel, no per-channel overflow).
- `bgfx_swap_buffers` — dirty-region blit to the physical framebuffer.

**Event queue:** A lock-free ring buffer (`BGFX_EVENT_QUEUE_SIZE = 64`) in the context struct for passing input events to the compositor layer. Head advances on push, tail advances on poll.

### `double_buffer.c` — High-Level Drawing API

Wraps `bgfx_context` and exposes pixel-coordinate drawing primitives used by the rest of the kernel.

```c
int           db_init(void);
void          db_swap(void);
void          db_clear(unsigned int color);
void          db_put_pixel(int x, int y, unsigned int color);
void          db_fill_rect(int x, int y, int w, int h, unsigned int color);
void          db_draw_rect(int x, int y, int w, int h, unsigned int color);  // outline
void          db_draw_line(int x0, int y0, int x1, int y1, unsigned int color); // Bresenham
void          db_mark_dirty(int x, int y, int w, int h);
unsigned int* db_get_buffer(void);
```

`db_fill_rect` writes rows directly to the back buffer pointer for speed and calls `db_mark_dirty` once for the whole block. `db_put_pixel` calls `db_mark_dirty` per pixel — high-frequency callers should batch with explicit `db_mark_dirty` calls instead.

`kdesktop.c` extends this with `db_draw_rect_transparent`, which performs per-pixel alpha blending against the existing back buffer contents using the formula `(src * α + dst * (255 − α)) >> 8` on packed RB and G channels separately to avoid channel bleed.

### Font Rendering

`font_ttf.c` wraps `stb_truetype` to rasterise glyphs from `AovelSansRounded.ttf` at size 16. `ttf_put_char_buf` writes an anti-aliased glyph directly into a provided pixel buffer. `get_ttf_char_width` returns the advance width for a given character, used for proportional text layout throughout the desktop and console.

---

## Input Subsystem

### Event Queue (`event.c`)

A statically allocated ring buffer of `OS_Event` structs, capacity 8192.

```c
typedef struct {
    EventType type;   // EVENT_NONE, EVENT_KEYBOARD, EVENT_MOUSE, EVENT_TIMER, EVENT_SYSTEM
    int64_t data1;    // ASCII / mouse X / tick
    int64_t data2;    // scancode / mouse Y
    int64_t data3;    // modifiers / button mask
    int64_t data4;    // reserved
} OS_Event;
```

`push_event` and `pop_event` bracket the queue manipulation with `cli`/`sti` to prevent concurrent modification by IRQ handlers. The kernel main loop calls `pop_event` in a drain loop until `EVENT_NONE` is returned.

### Keyboard (`keyboard_k.c`)

PS/2 keyboard driver. `keyboard_irq_handler` is called from IRQ 1. Reads the scancode from port `0x60`, updates shift/ctrl/alt state in `KeyboardState`, converts to ASCII via a scancode table, and pushes an `EVENT_KEYBOARD` event with `data1 = ASCII`, `data2 = scancode`, `data3 = modifier flags`.

### Mouse (`mouse_k.c`)

PS/2 mouse driver. `mouse_irq_handler` is called from IRQ 12. Accumulates the standard 3-byte PS/2 packet, interprets the sign bits and overflow flags from byte 0, and updates `MouseState.x/y` (clamped to screen bounds). Pushes `EVENT_MOUSE` with `data1 = x`, `data2 = y`, `data3 = button mask`.

---

## Desktop Environment

`kdesktop.c` implements the full windowed desktop. It is entered via `kdesktop_run()` which loops draining the event queue, dispatching to `kdesktop_handle_mouse` / `kdesktop_handle_keyboard`, and calling `kdesktop_render()` only when `needs_redraw` is set — preventing unnecessary frame renders when the system is idle.

### Window Manager

Windows are `Window` structs in a singly-linked list (`window_list`). Focused window is tracked in `focused_window`.

Each `Window` contains:
- Position, size, title, colour scheme.
- `dragging`, `drag_offset_x/y` — drag state for title bar move.
- `input_buffer[128]`, `input_pos` — command line accumulator.
- `terminal_buffer` (80×25 chars), `terminal_cursor_x/y` — for `WINDOW_TERMINAL` type.

**Z-ordering:** `kdesktop_render` builds a stack array from the window list, draws non-focused windows back-to-front, then draws the focused window last (topmost).

**Hit testing:** Mouse clicks check the title bar region (top 35 px of window rect) for drag initiation and the close button (rightmost 35 px of title bar). Body clicks focus the window.

### Terminal Windows

`kdesktop_create_terminal` allocates an 80×25 character grid. `kdesktop_terminal_write_char` handles newline, carriage return, backspace, printable characters, automatic line wrap based on window width, and scrolling (shifts all rows up by one, wipes the last row). `kdesktop_terminal_render` iterates the character grid, uses `get_ttf_char_width` for proportional pen advancement, and calls `ttf_put_char_buf` into the back buffer.

**Command dispatch:** On `\n`, `kdesktop_exec_command` is called with the accumulated `input_buffer`. Built-in terminal commands: `help`, `clear`, `exit`, `heap`, `mem`.

### Rendering Pipeline (per frame)

1. `db_clear` — fill back buffer with desktop background colour.
2. `kdesktop_draw_icons` — for each `DesktopIcon`: fill the icon square, draw symbol, render centred label with proportional TTF.
3. Non-focused windows (back-to-front): shadow rectangles, transparent window body, title bar, border, title text. Terminal content rendered separately after window chrome.
4. Focused window (same as above, drawn last).
5. `kdesktop_draw_taskbar` — fill taskbar bar, draw window buttons.
6. `kdesktop_draw_cursor` — rasterise an 16×16 triangular software cursor.
7. `db_swap` — dirty-region blit to screen.

### Theme

Adwaita-inspired dark palette:

| Name | Value | Use |
|------|-------|-----|
| `ADWAITA_BG` | `0x1E1E1E` | Desktop background |
| `ADWAITA_WINDOW_BG` | `0x2D2D2D` | Window body |
| `ADWAITA_HEADER` | `0x353535` | Title bar |
| `ADWAITA_BORDER` | `0x454545` | Inactive border |
| `ADWAITA_ACCENT` | `0x3584E4` | Focused border |
| `ADWAITA_TEXT` | `0xEEEEEE` | All text |

---

## VeloxFS & RAM Disk

### RAM Disk (`ramdisk.c`)

A flat byte array allocated from the kernel heap at init time, presenting a 512-byte sector interface.

- **Size:** 16,384 sectors × 512 bytes = **8 MB**.
- `ram_disk_read(lba, count, buf)` / `ram_disk_write(lba, count, buf)` — bounds-clamped `kmemcpy` operations. Reads and writes that start or extend past the end of the disk are silently clamped to the available range.
- The buffer is zeroed on init with `kmemset`.

### VeloxFS (`veloxfs.h`)

A custom filesystem implemented as a single-header library (`#define veloxfs_IMPLEMENTATION`). Interfaced through an IO adapter struct:

```c
typedef struct {
    int (*read)(void* user, uint64_t offset, void* buf, uint32_t size);
    int (*write)(void* user, uint64_t offset, const void* buf, uint32_t size);
    void* user_data;
} veloxfs_io;
```

The kernel provides `veloxfs_read_adapter` and `veloxfs_write_adapter`, which translate byte offsets to LBA arithmetic (`offset / 512`) and forward to `ram_disk_read` / `ram_disk_write`.

**Mount sequence in `init_filesystem`:**

1. `ram_disk_init()` — allocate and zero the block device.
2. `veloxfs_mount(&g_fs, g_io)` — attempt to mount an existing filesystem.
3. On failure (status `-2` = no valid superblock), `veloxfs_format(g_io, 16384, 0)` writes a fresh filesystem.
4. Retry `veloxfs_mount`. Panic (`while(1)`) if this also fails.

`g_fs` and `g_io` are file-scope globals so the handle survives past `init_filesystem` for subsequent file operations.

**Glue macros** adapt VeloxFS's internal allocation and string calls to kernel equivalents:

```c
#define veloxfs_MALLOC(sz)        kmalloc(sz)
#define veloxfs_FREE(p)           kfree(p)
#define veloxfs_CALLOC(n, sz)     /* kmalloc + kmemset */
#define veloxfs_MEMSET(d,c,n)     kmemset(d,c,n)
#define veloxfs_MEMCPY(d,s,n)     kmemcpy(d,s,n)
#define veloxfs_STRLEN(s)         strlen(s)
#define veloxfs_STRCMP(a,b)       strcmp(a,b)
#define veloxfs_STRNCMP(a,b,n)    strncmp(a,b,n)
#define veloxfs_STRNCPY(d,s,n)    strncpy(d,s,n)
#define veloxfs_SNPRINTF(d,n,...) snprintf(d,n,__VA_ARGS__)
```

---

## Console

`console.c` provides a framebuffer text console used during early boot (before the desktop environment starts) and for kernel diagnostic output.

- **Cursor:** `CursorX`, `CursorY` globals, initialised to `(50, 50)`.
- **Character rendering:** Printable characters are rendered via `ttf_put_char` directly to `fb_base` (not the double buffer). Cursor advances by `get_ttf_char_width(c) + 1`.
- **Newline:** `CursorY` incremented by `(font_ascent - font_descent + font_lineGap) * font_scale + 4`.
- **Backspace:** `console_backspace_variable(width)` erases the exact glyph-width pixel region and retracts the cursor.
- **Scroll:** `console_scroll` uses `kmemcpy` to shift the entire framebuffer up by one line height in-place, then `kmemset` to zero the bottom strip. CursorY is decremented by one line height.
- **`console_clear`:** Full framebuffer `kmemset` to zero, cursor reset to `(50, 50)`.

---

## Kernel Standard Library

The kernel includes minimal freestanding implementations of common functions:

**`kstring.c`**

```c
void*  kmemset(void* dest, int ch, size_t count);
void*  kmemcpy(void* dest, const void* src, size_t count);
int    strcmp(const char* s1, const char* s2);
int    strncmp(const char* s1, const char* s2, size_t n);
char*  strncpy(char* dest, const char* src, size_t n);
size_t strlen(const char* str);
```

**`kstdlib.c`**

```c
void ksprintf(char* buf, const char* fmt, ...);
void kprintf(const char* fmt, ...);
int  snprintf(char* buf, size_t n, const char* fmt, ...);
double pow(double x, double y);
double sqrt(double x);
double ceil(double x);
double floor(double x);
```

`kprintf` and `ksprintf` are backed by `stb_sprintf` (single-header, no libc dependency). `kprintf` output goes to the console via `console_print`.

**`stdint.h`** — Freestanding integer type definitions (`int8_t` through `uint64_t`, `size_t`, `ptrdiff_t`, `NULL`). Included instead of the standard library header.

---

## Build System

**Toolchain:** GCC + GNU LD + NASM + objcopy, targeting `elf64-x86-64`. `gnu-efi` provides the UEFI CRT0 object and linker script.

**Compiler flags:**

| Flag | Purpose |
|------|---------|
| `-fno-stack-protector` | No libssp in freestanding environment |
| `-fpic` | Position-independent code required by UEFI PE format |
| `-fshort-wchar` | UEFI uses 16-bit `wchar_t` |
| `-mno-red-zone` | Interrupt handlers invalidate the 128-byte red zone below RSP |
| `-DEFI_FUNCTION_WRAPPER` | Enables gnu-efi calling convention wrapper on x86-64 |
| `-O2` | Optimisation |

**Build pipeline:**

```
.c files  ──→ gcc (CFLAGS) ──→ .o files ─┐
.asm files ─→ nasm -f elf64 ──→ .o files ─┤
                                           ├→ ld (gnu-efi LDS) → kernel.so
                                           └→ objcopy → BOOTX64.EFI
```

The intermediate `kernel.so` is a relocatable ELF64 shared object. `objcopy` strips it down to the PE/COFF sections required by UEFI (`-j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc`) and converts it to `efi-app-x86_64` format.

**Targets:**

```bash
make          # Build BOOTX64.EFI
make run      # Build, copy to esp/EFI/BOOT/, launch QEMU
make clean    # Remove all build artefacts
```

---

## Running

**Dependencies:**

```bash
# Ubuntu / Debian
sudo apt install gcc nasm binutils gnu-efi ovmf qemu-system-x86
```

**Build and run:**

```bash
make run
```

The `run` target:
1. Creates `esp/EFI/BOOT/BOOTX64.EFI`.
2. Writes `esp/startup.nsh` to auto-execute the EFI application.
3. Copies `OVMF_VARS_4M.fd` to `uefi_vars.fd` (writable UEFI variable store) if not present.
4. Launches QEMU with KVM, 4 GB RAM, OVMF firmware, FAT-formatted ESP image, no network, serial on stdio.

```bash
qemu-system-x86_64 -enable-kvm \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=uefi_vars.fd \
  -drive format=raw,file=fat:rw:esp \
  -net none \
  -m 4096M -serial stdio
```

Serial output (`kprintf`) appears in the terminal that launched QEMU. The graphical output appears in the QEMU window.

---

## Repository Structure

```
KernelPriaxis/
├── kernel_native.c     # Entry point (efi_main), hardware init sequence
├── interrupts.asm      # ISR/IRQ NASM stubs for all interrupt vectors
├── idt.c / idt.h       # IDT setup, exception and IRQ dispatch
├── pmm.c / pmm.h       # Physical memory manager (bitmap allocator)
├── vmm.c / vmm.h       # Virtual memory manager (4-level paging)
├── memory.c / memory.h # Kernel heap (linked-list, PMM-backed)
├── console.c / console.h  # Framebuffer text console
├── double_buffer.c/h   # Drawing API, damage-tracked compositor wrapper
├── baregfx.h           # Low-level compositor (virtual coords, dirty rect, swap)
├── sysfb.h             # Static BSS back buffer pool
├── kdesktop.c / kdesktop.h  # Windowed desktop environment
├── event.c / event.h   # IRQ-safe ring buffer event queue
├── keyboard_k.c/h      # PS/2 keyboard driver
├── mouse_k.c/h         # PS/2 mouse driver
├── timer.c / timer.h   # PIT tick counter and uptime
├── font_ttf.c/h        # stb_truetype glyph rasteriser wrapper
├── font_data.c         # Embedded TTF font binary
├── font.c / font.h     # Legacy bitmap font (unused in desktop path)
├── ramdisk.c/h         # Sector-addressed in-memory block device
├── veloxfs.h           # VeloxFS filesystem (single-header implementation)
├── commands.c/h        # Shell command parsing and dispatch
├── kstdlib.c/h         # kprintf, ksprintf, snprintf, math functions
├── kstring.c/h         # kmemset, kmemcpy, strcmp, strlen, etc.
├── stdint.h            # Freestanding integer type definitions
├── stb_truetype.h      # stb single-header TTF rasteriser
├── stb_sprintf.h       # stb single-header printf implementation
├── AovelSansRounded.ttf # System font
├── linker_uefi.ld      # Linker script (load at 0x100000, 4KB section alignment)
├── Makefile            # Build system
└── LICENSE
```

---

## License

See [LICENSE](LICENSE).
