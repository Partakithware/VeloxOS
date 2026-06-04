# Makefile using gnu-efi for proper UEFI build

CC = gcc
LD = ld
OBJCOPY = objcopy

EFI_INC = /usr/include/efi
EFI_LIB = /usr/lib
EFI_CRT = $(EFI_LIB)/crt0-efi-x86_64.o
EFI_LDS = /usr/lib/elf_x86_64_efi.lds

CFLAGS = -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I. \
         -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
         -DEFI_FUNCTION_WRAPPER -Wall -O2

LDFLAGS = -nostdlib -znocombreloc -T $(EFI_LDS) -shared \
          -Bsymbolic -L$(EFI_LIB) $(EFI_CRT)

TARGET = BOOTX64.EFI
SO = kernel.so

.PHONY: all clean run

all: $(TARGET)

interrupts.o: interrupts.asm
	nasm -f elf64 interrupts.asm -o interrupts.o

idt.o: idt.c idt.h
	$(CC) $(CFLAGS) -c idt.c -o idt.o

kernel_native.o: kernel_native.c idt.h
	$(CC) $(CFLAGS) -c kernel_native.c -o kernel_native.o


mouse_k.o: mouse_k.c mouse_k.h
	$(CC) $(CFLAGS) -c mouse_k.c -o mouse_k.o

keyboard_k.o: keyboard_k.c keyboard_k.h
	$(CC) $(CFLAGS) -c keyboard_k.c -o keyboard_k.o

event.o: event.c event.h
	$(CC) $(CFLAGS) -c event.c -o event.o

font.o: font.c font.h
	$(CC) $(CFLAGS) -c font.c -o font.o

console.o: console.c console.h
	$(CC) $(CFLAGS) -c console.c -o console.o

commands.o: commands.c commands.h
	$(CC) $(CFLAGS) -c commands.c -o commands.o

memory.o: memory.c memory.h
	$(CC) $(CFLAGS) -c memory.c -o memory.o

kstdlib.o: kstdlib.c kstdlib.h
	$(CC) $(CFLAGS) -c kstdlib.c -o kstdlib.o

font_data.o: font_data.c
	$(CC) $(CFLAGS) -c font_data.c -o font_data.o

font_ttf.o: font_ttf.c
	$(CC) $(CFLAGS) -c font_ttf.c -o font_ttf.o

kstring.o: kstring.c kstring.h
	$(CC) $(CFLAGS) -c kstring.c -o kstring.o

timer.o: timer.c timer.h
	$(CC) $(CFLAGS) -c timer.c -o timer.o

pmm.o: pmm.c pmm.h
	$(CC) $(CFLAGS) -c pmm.c -o pmm.o

double_buffer.o: double_buffer.c double_buffer.h
	$(CC) $(CFLAGS) -c double_buffer.c -o double_buffer.o

kdesktop.o: kdesktop.c kdesktop.h
	$(CC) $(CFLAGS) -c kdesktop.c -o kdesktop.o

vmm.o: vmm.c vmm.h
	$(CC) $(CFLAGS) -c vmm.c -o vmm.o

ramdisk.o: ramdisk.c ramdisk.h
	$(CC) $(CFLAGS) -c ramdisk.c -o ramdisk.o

$(SO): interrupts.o idt.o kernel_native.o mouse_k.o keyboard_k.o event.o font.o console.o commands.o memory.o kstdlib.o font_data.o font_ttf.o kstring.o timer.o pmm.o double_buffer.o kdesktop.o vmm.o ramdisk.o
	$(LD) $(LDFLAGS) interrupts.o idt.o kernel_native.o mouse_k.o keyboard_k.o event.o font.o console.o commands.o memory.o kstdlib.o font_data.o font_ttf.o kstring.o timer.o pmm.o double_buffer.o kdesktop.o vmm.o ramdisk.o -o $@ -lefi -lgnuefi

$(TARGET): $(SO)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic \
	           -j .dynsym -j .rel -j .rela -j .reloc \
	           --target=efi-app-x86_64 $< $@
	@echo "UEFI application built: $(TARGET)"

run: $(TARGET)
	@mkdir -p esp/EFI/BOOT
	@cp $(TARGET) esp/EFI/BOOT/
	@echo "FS0:\EFI\BOOT\BOOTX64.EFI" > esp/startup.nsh
	@if [ ! -f uefi_vars.fd ]; then \
		cp /usr/share/OVMF/OVMF_VARS_4M.fd uefi_vars.fd 2>/dev/null || true; \
		chmod +w uefi_vars.fd 2>/dev/null || true; \
	fi
	qemu-system-x86_64 -enable-kvm \
		-drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
		-drive if=pflash,format=raw,file=uefi_vars.fd \
		-drive format=raw,file=fat:rw:esp \
		-net none \
		-m 4096M -serial stdio

clean:
	rm -f *.o $(SO) $(TARGET) uefi_vars.fd
	rm -rf esp