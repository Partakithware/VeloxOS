#ifndef SYS_FB_H
#define SYS_FB_H

/* Bootloader Framebuffer Info Structure (Pure primitive types, zero includes) */
typedef struct {
	unsigned long long physical_address;
	unsigned int width;
	unsigned int height;
	unsigned int pixels_per_scanline;
	unsigned int memory_model;
} sys_boot_fb_t;

#define KERNEL_BACK_BUFFER_MAX_SIZE (1920 * 1080 * 4)

/* Reserve max 1080p frame in BSS section */
static unsigned char g_back_buffer_pool[KERNEL_BACK_BUFFER_MAX_SIZE] __attribute__((aligned(4096)));

/* Allocates a backbuffer from the static pool based on physical screen requirements */
static inline unsigned int* sys_allocate_back_buffer(unsigned int width, unsigned int height, unsigned int pitch) {
	unsigned int required_size = height * pitch * 4; /* 4 bytes per pixel (32-bit RGB) */
	
	if (required_size > KERNEL_BACK_BUFFER_MAX_SIZE) {
		return 0; /* Framebuffer exceeds static allocation pool */
	}
	
	return (unsigned int*)g_back_buffer_pool;
}

#endif /* SYS_FB_H */