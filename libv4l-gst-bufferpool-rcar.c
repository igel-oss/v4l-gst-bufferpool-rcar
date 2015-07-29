/*
 * Copyright (C) 2015 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "drm.h"
#include "libkms.h"
#include <xf86drm.h>

#include <gst/video/video.h>
#include <gst/allocators/gstdmabuf.h>

#include "libv4l-gst-bufferpool.h"

#if HAVE_VISIBILITY
#define PLUGIN_PUBLIC __attribute__ ((visibility("default")))
#else
#define PLUGIN_PUBLIC
#endif

typedef struct _V4lGstBufferMeta V4lGstBufferMeta;

typedef struct _V4lGstBufferPool V4lGstBufferPool;
typedef struct _V4lGstBufferPoolClass V4lGstBufferPoolClass;

struct _V4lGstBufferMeta
{
  GstMeta meta;

  struct kms_bo *kms_bo;
};

struct _V4lGstBufferPool
{
  GstBufferPool bufferpool;

  gint drm_fd;
  struct kms_driver *kms;
  GstAllocator *allocator;
  GstVideoInfo info;
};

struct _V4lGstBufferPoolClass
{
  GstBufferPoolClass parent_class;
};

#define v4l_gst_buffer_pool_parent_class parent_class
G_DEFINE_TYPE (V4lGstBufferPool, v4l_gst_buffer_pool, GST_TYPE_BUFFER_POOL);

#define V4L_GST_BUFFER_META_API_TYPE  (v4l_gst_buffer_meta_api_get_type())
#define V4L_GST_BUFFER_META_INFO  (v4l_gst_buffer_meta_get_info())
#define get_v4l_gst_buffer_meta(b) ((V4lGstBufferMeta*)gst_buffer_get_meta((b),V4L_GST_BUFFER_META_API_TYPE))

#define V4L_GST_BUFFER_POOL(pool) ((V4lGstBufferPool *) pool)

static GType
v4l_gst_buffer_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_MEMORY_STR,
    GST_META_TAG_VIDEO_COLORSPACE_STR,
    GST_META_TAG_VIDEO_SIZE_STR, NULL
  };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("V4lGstBufferMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static void
v4l_gst_buffer_meta_free (V4lGstBufferMeta * meta, GstBuffer * buffer)
{
  kms_bo_unmap (meta->kms_bo);
  kms_bo_destroy (&meta->kms_bo);
}

static const GstMetaInfo *
v4l_gst_buffer_meta_get_info (void)
{
  static const GstMetaInfo *v4l_gst_buffer_meta_info = NULL;

  if (g_once_init_enter (&v4l_gst_buffer_meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (V4L_GST_BUFFER_META_API_TYPE, "V4lGstBufferMeta",
        sizeof (V4lGstBufferMeta), (GstMetaInitFunction) NULL,
        (GstMetaFreeFunction) v4l_gst_buffer_meta_free,
        (GstMetaTransformFunction) NULL);
    g_once_init_leave (&v4l_gst_buffer_meta_info, meta);
  }
  return v4l_gst_buffer_meta_info;
}

GstBufferPool *
v4l_gst_buffer_pool_new (void)
{
  V4lGstBufferPool *pool;

  pool = g_object_new (v4l_gst_buffer_pool_get_type (), NULL);

  pool->drm_fd = open ("/dev/dri/card0", O_RDWR | O_CLOEXEC);
  if (pool->drm_fd < 0) {
    GST_ELEMENT_ERROR (pool, RESOURCE, FAILED,
        ("Could not open drm"), ("Could not open drm"));
    return NULL;
  }

  if (kms_create (pool->drm_fd, &pool->kms)) {
    GST_ELEMENT_ERROR (pool, RESOURCE, FAILED,
        ("kms_create failed"), ("kms_create failed"));
    return NULL;
  }

  pool->allocator = gst_dmabuf_allocator_new ();

  return GST_BUFFER_POOL_CAST (pool);
}

static void
v4l_gst_buffer_pool_finalize (GObject * object)
{
  V4lGstBufferPool *pool = V4L_GST_BUFFER_POOL (object);

  if (pool->allocator)
    gst_object_unref (pool->allocator);

  if (pool->kms)
    kms_destroy (&pool->kms);

  if (pool->drm_fd >= 0)
    close (pool->drm_fd);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
v4l_gst_buffer_pool_set_config (GstBufferPool * bpool, GstStructure * config)
{
  V4lGstBufferPool *pool = V4L_GST_BUFFER_POOL (bpool);
  GstCaps *caps = NULL;

  if (!gst_buffer_pool_config_get_params (config, &caps, NULL, NULL, NULL)) {
    fprintf (stderr, "failed to get config params\n");
    return FALSE;
  }

  if (!caps) {
    fprintf (stderr, "no caps in config\n");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&pool->info, caps)) {
    fprintf (stderr, "invalid caps\n");
    return FALSE;
  }

  if (pool->info.finfo->format != GST_VIDEO_FORMAT_BGRA) {
    fprintf (stderr,
        "%s is unsupported, only BGRA format is available in this pool\n",
        gst_video_format_to_string (pool->info.finfo->format));
    return FALSE;
  }

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (bpool, config);
}

static GstFlowReturn
v4l_gst_buffer_pool_alloc_buffer (GstBufferPool * bpool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  V4lGstBufferPool *pool = V4L_GST_BUFFER_POOL (bpool);
  gint err;
  GstBuffer *buf;
  V4lGstBufferMeta *meta;
  gint stride[GST_VIDEO_MAX_PLANES] = { 0 };
  gsize offset[GST_VIDEO_MAX_PLANES] = { 0 };
  void *data = NULL;
  size_t size;
  guint attr[] = {
    KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
    KMS_WIDTH, 0,
    KMS_HEIGHT, 0,
    KMS_TERMINATE_PROP_LIST
  };

  attr[3] = ((pool->info.width + 31) >> 5) << 5;
  attr[5] = pool->info.height;

  buf = gst_buffer_new ();
  meta = (V4lGstBufferMeta *) gst_buffer_add_meta (buf,
      V4L_GST_BUFFER_META_INFO, NULL);

  err = kms_bo_create (pool->kms, attr, &meta->kms_bo);
  if (err) {
    fprintf (stderr, "Failed to create kms bo\n");
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }

  err = kms_bo_map (meta->kms_bo, &data);
  if (err) {
    fprintf (stderr, "Failed to map kms bo\n");
    kms_bo_destroy (&meta->kms_bo);
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }

  kms_bo_get_prop (meta->kms_bo, KMS_PITCH, (guint *) & stride[0]);
  size = stride[0] * pool->info.height;

  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (0, data,
                              size, 0, size, NULL, NULL));

  gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
      pool->info.finfo->format, pool->info.width, pool->info.height, 1, offset,
      stride);

  *buffer = buf;

  return GST_FLOW_OK;
}

static void
v4l_gst_buffer_pool_class_init (V4lGstBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->finalize = v4l_gst_buffer_pool_finalize;

  gstbufferpool_class->set_config = v4l_gst_buffer_pool_set_config;
  gstbufferpool_class->alloc_buffer = v4l_gst_buffer_pool_alloc_buffer;
}

static void
v4l_gst_buffer_pool_init (V4lGstBufferPool * pool)
{
  pool->drm_fd = -1;
  pool->kms = NULL;
  pool->allocator = NULL;
}

PLUGIN_PUBLIC const struct libv4l_gst_buffer_pool_ops libv4l_gst_bufferpool = {
  .add_external_sink_buffer_pool = &v4l_gst_buffer_pool_new,
};
