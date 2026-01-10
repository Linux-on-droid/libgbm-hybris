#include <fcntl.h> 
#include <xf86drm.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <malloc.h>

#include <linux/memfd.h>

#include <gbm.h>
#include "gbm_backend_abi.h"

#include <hybris/gralloc/gralloc.h>

#include <hardware/gralloc.h>

#include <assert.h>

#define DRM_EVDI_GBM_ADD_BUFF 0x05
#define DRM_EVDI_GBM_DEL_BUFF 0x0B
#define DRM_EVDI_GBM_CREATE_BUFF 0x0C

#define DRM_IOCTL_EVDI_GBM_DEL_BUFF DRM_IOWR(DRM_COMMAND_BASE +  \
	DRM_EVDI_GBM_DEL_BUFF, struct drm_evdi_gbm_del_buff)

#define DRM_IOCTL_EVDI_GBM_ADD_BUFF DRM_IOWR(0x40 +  \
	DRM_EVDI_GBM_ADD_BUFF, struct drm_evdi_gbm_add_buf)

#define DRM_IOCTL_EVDI_GBM_CREATE_BUFF DRM_IOWR(DRM_COMMAND_BASE +  \
	DRM_EVDI_GBM_CREATE_BUFF, struct drm_evdi_gbm_create_buff)

struct drm_evdi_gbm_add_buf {
	int fd;
	int id;
};

struct drm_evdi_gbm_del_buff {
	int id;
};

struct gbm_hybris_bo {
   struct gbm_bo base;
//   buffer_handle_t handle;
   int evdi_lindroid_buff_id;
};

struct gbm_hybris_surface {
   void *reserved_for_egl_gbm;
   struct gbm_surface base;
};

struct drm_evdi_gbm_create_buff {
	int *id;
	uint32_t *stride;
	uint32_t format;
	uint32_t width;
	uint32_t height;
};

static const struct gbm_core *core;

int memfd_create(const char *name, unsigned int flags);

struct gbm_hybris_bo *gbm_hybris_bo(struct gbm_bo *bo)
{
   return (struct gbm_hybris_bo *) bo;
}

static int get_hal_pixel_format(uint32_t gbm_format)
{
    int format;

    switch (gbm_format) {
    case GBM_FORMAT_ABGR8888:
        format = HAL_PIXEL_FORMAT_RGBA_8888;
        break;
    case GBM_FORMAT_XBGR8888:
        format = HAL_PIXEL_FORMAT_RGBX_8888;
        break;
    case GBM_FORMAT_RGB888:
        format = HAL_PIXEL_FORMAT_RGB_888;
        break;
    case GBM_FORMAT_RGB565:
        format = HAL_PIXEL_FORMAT_RGB_565;
        break;
    case GBM_FORMAT_ARGB8888:
        format = HAL_PIXEL_FORMAT_BGRA_8888;
        break;
    case GBM_FORMAT_GR88:
        /* GR88 corresponds to YV12 which is planar */
        format = HAL_PIXEL_FORMAT_YV12;
        break;
    case GBM_FORMAT_ABGR16161616F:
        format = HAL_PIXEL_FORMAT_RGBA_FP16;
        break;
    case GBM_FORMAT_ABGR2101010:
        format = HAL_PIXEL_FORMAT_RGBA_1010102;
        break;
    default:
        format = HAL_PIXEL_FORMAT_RGBA_8888; // Invalid or unsupported format assume RGBA8888
        break;
    }

    return format;
}

struct gbm_bo* hybris_gbm_bo_create(struct gbm_device* device, uint32_t width, uint32_t height, uint32_t format, uint32_t flags, const uint64_t *modifiers, const unsigned int count) {
    if (!device) {
        fprintf(stderr, "[libgbm-hybris] Invalid GBM device.\n");
        return NULL;
    }

    struct gbm_hybris_bo *bo;
    bo = calloc(1, sizeof(struct gbm_hybris_bo));

    // Not inited in libgbm?
    bo->base.v0.user_data = NULL;

    if (!bo) {
        fprintf(stderr, "[libgbm-hybris] Failed to allocate memory for GBM buffer object.\n");
        return NULL;
    }

    format = core->v0.format_canonicalize(format);

    bo->base.gbm = device;

    bo->base.v0.width = width;
    bo->base.v0.height = height;
    bo->base.v0.format = format;

    int usage = 0;

    if (flags & GBM_BO_USE_SCANOUT)
        usage |= GRALLOC_USAGE_HW_FB;
    if (flags & GBM_BO_USE_RENDERING)
        usage |= GRALLOC_USAGE_HW_RENDER;
    if (flags & GBM_BO_USE_LINEAR)
        usage |= GRALLOC_USAGE_SW_READ_RARELY | GRALLOC_USAGE_SW_WRITE_RARELY;

    int stride = 0;
    buffer_handle_t handle = NULL;
    struct drm_evdi_gbm_create_buff cmd;
    cmd.width = width;
    cmd.height = height;
    cmd.format = HAL_PIXEL_FORMAT_RGBA_8888;
    cmd.stride = &stride;
    cmd.id = &bo->evdi_lindroid_buff_id;
    int ret = ioctl(device->v0.fd, DRM_IOCTL_EVDI_GBM_CREATE_BUFF, &cmd);

    bo->base.v0.stride = stride;

    return &bo->base;
}

static void hybris_gbm_bo_destroy(struct gbm_bo *_bo)
{
    struct gbm_hybris_bo *bo = gbm_hybris_bo(_bo);
//    if (bo->handle) {
  //      hybris_gralloc_release(bo->handle, 1);
    struct drm_evdi_gbm_del_buff close_args = {
        .id = bo->evdi_lindroid_buff_id
    };

//    if (ioctl(bo->base.gbm->v0.fd, DRM_IOCTL_EVDI_GBM_DEL_BUFF, &close_args) < 0) {
  //      perror("[libgbm-hybris] DRM_IOCTL_GEM_CLOSE failed");
    //} else {
//        printf("[libgbm-hybris]  Released GEM buffer with handle %u\n",  bo->evdi_lindroid_buff_id);
  //  }
//    native_handle_close(bo->handle);
//    native_handle_delete(bo->handle);
//}
    free(bo);
}


static void hybris_gbm_device_destroy(struct gbm_device *device)
{
    if(device->v0.fd)
        close(device->v0.fd);
    free(device);
}


struct gbm_bo *hybris_gbm_bo_create_with_modifiers(struct gbm_device *gbm,
                             uint32_t width, uint32_t height,
                             uint32_t format,
                             const uint64_t *modifiers,
                             const unsigned int count)
{
//TBD: it do not work that way :D
   return NULL;
}

struct gbm_bo * hybris_gbm_bo_create_with_modifiers2(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, const uint64_t *modifiers, const unsigned int count, uint32_t flags){
//TBD
    printf("[libgbm-hybris] gbm_bo_create_with_modifiers2\n");
    return NULL;
}

struct gbm_bo *hybris_gbm_bo_import(struct gbm_device *gbm, uint32_t type, void *buffer, uint32_t usage){
// How do that even work with fake dma buf's?
   printf("[libgbm-hybris] gbm_bo_import called\n");
   return NULL;
}

// Suprisingly not part of libgbm
uint32_t hybris_gbm_bo_get_stride(struct gbm_bo* bo, int plane) {
//    printf("[libgbm-hybris] gbm_bo_get_stride called\n");
    return bo ? (uint32_t)(bo->v0.stride) : 0;
}

uint32_t hybris_gbm_bo_get_stride_for_plane(struct gbm_bo *bo, int plane)
{
//TBD
   printf("[libgbm-hybris] gbm_bo_get_stride_for_plane called\n");
   return 0;
}

uint64_t hybris_gbm_bo_get_modifier(struct gbm_bo* bo) {
//TBD: Implement modifier
//    printf("[libgbm-hybris] gbm_bo_get_modifier called\n");
    return 0;
}

void* hybris_gbm_bo_map(struct gbm_bo *bo, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t flags, uint32_t *stride, void **map_data) {
//TBD: Implement based on grlloc lock
    printf("[libgbm-hybris] gbm_bo_map called with x: %u, y: %u, width: %u, height: %u, flags: %u\n", x, y, width, height, flags);
    return NULL;
}

void hybris_gbm_surface_destroy(struct gbm_surface *surf) {
//TBD: Implement surfaces
    printf("[libgbm-hybris] gbm_surface_destroy called\n");
}


struct gbm_bo* hybris_gbm_surface_lock_front_buffer(struct gbm_surface* surface) {
//TBD: Implement surfaces
    printf("[libgbm-hybris] gbm_surface_lock_front_buffer called\n");
    return (struct gbm_bo*)malloc(sizeof(struct gbm_bo));
}

void hybris_gbm_surface_release_buffer(struct gbm_surface* surface, struct gbm_bo* bo) {
//TBD: Implement surfaces
    printf("[libgbm-hybris] gbm_surface_release_buffer called\n");
    if (bo) {
        free(bo);
    }
}

int hybris_gbm_bo_get_fd(struct gbm_bo* _bo) {
    if(!_bo) {
        printf("[libgbm-hybris] gbm_bo_get_fd missing bo\n");
        return -1;
    }

    struct gbm_hybris_bo *bo = gbm_hybris_bo(_bo);
    if(!bo) {
        printf("[libgbm-hybris] gbm_bo_get_fd missing bo->handle\n");
        return -1;
    }

    if(bo->evdi_lindroid_buff_id == -1) {
        printf("[libgbm-hybris] missing evdi_lindroid_buff_id\n");
        return -1;
    }

    int fd = memfd_create("whatever", MFD_CLOEXEC);

    if (fd == -1) {
        printf("[libgbm-hybris] memfd_create failed\n");
        return -1;
    }

      if(write(fd, &bo->evdi_lindroid_buff_id, sizeof(int)) != sizeof(int)) {
        printf("[libgbm-hybris] failed to write evdi_lindroid_buff_id into mefd\n");
        close(fd);
        return -1;
    }

   return fd;
}

int hybris_gbm_bo_get_plane_count(struct gbm_bo *bo)
{
//TBD and rename to bo_get_planes
//   printf("[libgbm-hybris] gbm_bo_get_plane_count called\n");
   return 1;
}

int hybris_gbm_bo_get_fd_for_plane(struct gbm_bo *bo, int plane)
{
    if (plane != 0) {
        fprintf(stderr, "[libgbm-hybris] Error: requested plane %d, only 0 is supported\n", plane);
        errno = EINVAL;
        return -1;
    }

    return hybris_gbm_bo_get_fd(bo);
}

uint32_t hybris_bo_get_offset(struct gbm_bo *bo, int plane)
{
//   printf("[libgbm-hybris] gbm_bo_get_offset called\n");
   return 0;
}

struct gbm_surface *hybris_gbm_surface_create_with_modifiers(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, const uint64_t *modifiers, const unsigned int count){
//TBD: Implement surfaces
   printf("[libgbm-hybris] gbm_surface_create_with_modifiers\n");
   if ((count && !modifiers) || (modifiers && !count)) {
      errno = EINVAL;
      return NULL;
   }

   return NULL;
}

struct gbm_surface *hybris_gbm_surface_create(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, uint32_t flags, const uint64_t *modifiers, const unsigned count) {
//TBD: Implement surfaces
    printf("[libgbm-hybris] gbm_surface_create called with width: %u, height: %u, format: %u, flags: %u\n", width, height, format, flags);
    struct gbm_hybris_surface *surf;
    surf = calloc(1, sizeof *surf);
    if (surf == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    surf->base.gbm = gbm;
    surf->base.v0.width = width;
    surf->base.v0.height = height;
    surf->base.v0.format = get_hal_pixel_format(format);
    surf->base.v0.flags = flags;
    surf->base.v0.modifiers = calloc(count, sizeof(*modifiers));
    if (count && !surf->base.v0.modifiers) {
        errno = ENOMEM;
        free(surf);
        return NULL;
    }

    //uint64_t *v0_modifiers = surf->base.v0.modifiers;
    //for (int i = 0; i < count; i++) {
        // compressed buffers don't render correctly when imported
      //  if (modifiers[i] & ~DRM_FORMAT_MOD_NVIDIA_BLOCK_LINEAR_2D(0x0, 0x1, 0x3, 0xff, 0xf))
      //      continue;
      //  *v0_modifiers++ = modifiers[i];
    //}
    //surf->base.v0.count = v0_modifiers - surf->base.v0.modifiers;

    return &surf->base;
}

void hybris_gbm_bo_unmap(struct gbm_bo* bo, void* map_data) {
//TBD: Implement using gralloc unlock
//    printf("[libgbm-hybris] gbm_bo_unmap called\n");
    if (map_data) {
        free(map_data);
    }
}

char *hybris_gbm_format_get_name(uint32_t gbm_format, struct gbm_format_name_desc *desc)
{
//TBD
   //gbm_format = gbm_format_canonicalize(gbm_format);
//   printf("[libgbm-hybris] gbm_format_get_name called\n");
   desc->name[0] = 0;
   desc->name[1] = 0;
   desc->name[2] = 0;
   desc->name[3] = 0;
   desc->name[4] = 0;

   return desc->name;
}

static struct gbm_device *hybris_device_create(int fd, uint32_t gbm_backend_version){
  //  printf("[libgbm-hybris] hybris_device_create called\n");
    struct gbm_device *device;

    if (gbm_backend_version != GBM_BACKEND_ABI_VERSION) {
        printf("Wrong gbm version, built for: %d current: %d\n", GBM_BACKEND_ABI_VERSION, gbm_backend_version);
        return NULL;
    }

    device = calloc(1, sizeof *device);
    if (!device)
       return NULL;

   device->v0.fd = fd;
   device->v0.backend_version = gbm_backend_version;
   device->v0.bo_create = hybris_gbm_bo_create;
   device->v0.bo_destroy = hybris_gbm_bo_destroy;
   device->v0.destroy = hybris_gbm_device_destroy;
   device->v0.bo_get_fd = hybris_gbm_bo_get_fd;
   device->v0.bo_get_stride = hybris_gbm_bo_get_stride;
   device->v0.bo_get_modifier = hybris_gbm_bo_get_modifier;
   device->v0.bo_get_planes = hybris_gbm_bo_get_plane_count;
   device->v0.bo_get_plane_fd = hybris_gbm_bo_get_fd_for_plane;
   device->v0.surface_create = hybris_gbm_surface_create;
   device->v0.bo_get_offset = hybris_bo_get_offset;
   return device;
}

struct gbm_backend gbm_hybris_backend = {
   .v0.backend_version = GBM_BACKEND_ABI_VERSION,
   .v0.backend_name = "hybris",
   .v0.create_device = hybris_device_create,
};

struct gbm_backend * gbmint_get_backend(const struct gbm_core *gbm_core);

struct gbm_backend *
gbmint_get_backend(const struct gbm_core *gbm_core) {
   core = gbm_core;
   return &gbm_hybris_backend;
};
