/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-theme-node-drawing.c: Code to draw themed elements
 *
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2009, 2010 Florian Müllner
 * Copyright 2010 Intel Corporation.
 * Copyright 2011 Quentin "Sardem FF7" Glidic
 *
 * Contains code derived from:
 *   rectangle.c: Rounded rectangle.
 *     Copyright 2008 litl, LLC.
 *   st-texture-frame.h: Expandible texture actor
 *     Copyright 2007 OpenedHand
 *     Copyright 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <math.h>

#include "st-shadow.h"
#include "st-private.h"
#include "st-theme-private.h"
#include "st-theme-context.h"
#include "st-texture-cache.h"
#include "st-theme-node-private.h"

/****
 * Rounded corners
 ****/

typedef struct {
  ClutterColor   color;
  ClutterColor   border_color_1;
  ClutterColor   border_color_2;
  guint          radius;
  guint          border_width_1;
  guint          border_width_2;
  float          resource_scale;
} StCornerSpec;

typedef enum {
  ST_PAINT_BORDERS_MODE_COLOR,
  ST_PAINT_BORDERS_MODE_SILHOUETTE
} StPaintBordersMode;

static void
elliptical_arc (cairo_t *cr,
                double   x_center,
                double   y_center,
                double   x_radius,
                double   y_radius,
                double   angle1,
                double   angle2)
{
  cairo_save (cr);
  cairo_translate (cr, x_center, y_center);
  cairo_scale (cr, x_radius, y_radius);
  cairo_arc (cr, 0, 0, 1.0, angle1, angle2);
  cairo_restore (cr);
}

static CoglTexture *
create_corner_material (StCornerSpec *corner)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  CoglContext *ctx = clutter_backend_get_cogl_context (backend);
  GError *error = NULL;
  CoglTexture *texture;
  cairo_t *cr;
  cairo_surface_t *surface;
  guint rowstride;
  guint8 *data;
  guint size;
  guint logical_size;
  guint max_border_width;
  double device_scaling;

  max_border_width = MAX(corner->border_width_2, corner->border_width_1);
  logical_size = 2 * MAX(max_border_width, corner->radius);
  size = ceilf (logical_size * corner->resource_scale);
  rowstride = size * 4;
  data = g_new0 (guint8, size * rowstride);

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 size, size,
                                                 rowstride);
  device_scaling = (double) size / logical_size;
  cairo_surface_set_device_scale (surface, device_scaling, device_scaling);
  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_scale (cr, logical_size, logical_size);

  if (max_border_width <= corner->radius)
    {
      double x_radius, y_radius;

      if (max_border_width != 0)
        {
          cairo_set_source_rgba (cr,
                                 corner->border_color_1.red / 255.,
                                 corner->border_color_1.green / 255.,
                                 corner->border_color_1.blue / 255.,
                                 corner->border_color_1.alpha / 255.);

          cairo_arc (cr, 0.5, 0.5, 0.5, 0, 2 * M_PI);
          cairo_fill (cr);
        }

      cairo_set_source_rgba (cr,
                             corner->color.red / 255.,
                             corner->color.green / 255.,
                             corner->color.blue / 255.,
                             corner->color.alpha / 255.);

      x_radius = 0.5 * (1.0 - (double) corner->border_width_2 / corner->radius);
      y_radius = 0.5 * (1.0 - (double) corner->border_width_1 / corner->radius);

      /* TOPRIGHT */
      elliptical_arc (cr,
                      0.5, 0.5,
                      x_radius, y_radius,
                      3 * M_PI / 2, 2 * M_PI);

      /* BOTTOMRIGHT */
      elliptical_arc (cr,
                      0.5, 0.5,
                      x_radius, y_radius,
                      0, M_PI / 2);

      /* TOPLEFT */
      elliptical_arc (cr,
                      0.5, 0.5,
                      x_radius, y_radius,
                      M_PI, 3 * M_PI / 2);

      /* BOTTOMLEFT */
      elliptical_arc (cr,
                      0.5, 0.5,
                      x_radius, y_radius,
                      M_PI / 2, M_PI);

      cairo_fill (cr);
    }
  else
    {
      double radius;

      radius = (gdouble)corner->radius / max_border_width;

      cairo_set_source_rgba (cr,
                             corner->border_color_1.red / 255.,
                             corner->border_color_1.green / 255.,
                             corner->border_color_1.blue / 255.,
                             corner->border_color_1.alpha / 255.);

      cairo_arc (cr, radius, radius, radius, M_PI, 3 * M_PI / 2);
      cairo_line_to (cr, 1.0 - radius, 0.0);
      cairo_arc (cr, 1.0 - radius, radius, radius, 3 * M_PI / 2, 2 * M_PI);
      cairo_line_to (cr, 1.0, 1.0 - radius);
      cairo_arc (cr, 1.0 - radius, 1.0 - radius, radius, 0, M_PI / 2);
      cairo_line_to (cr, radius, 1.0);
      cairo_arc (cr, radius, 1.0 - radius, radius, M_PI / 2, M_PI);
      cairo_fill (cr);
    }
  cairo_destroy (cr);

  cairo_surface_destroy (surface);

  texture = COGL_TEXTURE (cogl_texture_2d_new_from_data (ctx, size, size,
                                                         CLUTTER_CAIRO_FORMAT_ARGB32,
                                                         rowstride,
                                                         data,
                                                         &error));

  if (error)
    {
      g_warning ("Failed to allocate texture: %s", error->message);
      g_error_free (error);
    }

  g_free (data);

  return texture;
}

static char *
corner_to_string (StCornerSpec *corner)
{
  return g_strdup_printf ("st-theme-node-corner:%02x%02x%02x%02x,%02x%02x%02x%02x,%02x%02x%02x%02x,%u,%u,%u,%.4f",
                          corner->color.red, corner->color.blue, corner->color.green, corner->color.alpha,
                          corner->border_color_1.red, corner->border_color_1.green, corner->border_color_1.blue, corner->border_color_1.alpha,
                          corner->border_color_2.red, corner->border_color_2.green, corner->border_color_2.blue, corner->border_color_2.alpha,
                          corner->radius,
                          corner->border_width_1,
                          corner->border_width_2,
                          corner->resource_scale);
}

static CoglTexture *
load_corner (StTextureCache  *cache,
             const char      *key,
             void            *datap,
             GError         **error)
{
  return create_corner_material ((StCornerSpec *) datap);
}

/* To match the CSS specification, we want the border to look like it was
 * drawn over the background. But actually drawing the border over the
 * background will produce slightly bad antialiasing at the edges, so
 * compute the effective border color instead.
 */
#define NORM(x) (t = (x) + 127, (t + (t >> 8)) >> 8)
#define MULT(c,a) NORM(c*a)

static void
premultiply (ClutterColor *color)
{
  guint t;
  color->red = MULT (color->red, color->alpha);
  color->green = MULT (color->green, color->alpha);
  color->blue = MULT (color->blue, color->alpha);
}

static void
unpremultiply (ClutterColor *color)
{
  if (color->alpha != 0)
    {
      color->red = MIN((color->red * 255 + 127) / color->alpha, 255);
      color->green = MIN((color->green * 255 + 127) / color->alpha, 255);
      color->blue = MIN((color->blue * 255 + 127) / color->alpha, 255);
    }
}

static void
over (const ClutterColor *source,
      const ClutterColor *destination,
      ClutterColor       *result)
{
  guint t;
  ClutterColor src = *source;
  ClutterColor dst = *destination;

  premultiply (&src);
  premultiply (&dst);

  result->alpha = src.alpha + NORM ((255 - src.alpha) * dst.alpha);
  result->red   = src.red +   NORM ((255 - src.alpha) * dst.red);
  result->green = src.green + NORM ((255 - src.alpha) * dst.green);
  result->blue  = src.blue +  NORM ((255 - src.alpha) * dst.blue);

  unpremultiply (result);
}

/*
 * st_theme_node_reduce_border_radius:
 * @node: a #StThemeNode
 * @width: The width of the box
 * @height: The height of the box
 * @corners: (array length=4) (out): reduced corners
 *
 * Implements the corner overlap algorithm mentioned at
 * http://www.w3.org/TR/css3-background/#corner-overlap
 */
static void
st_theme_node_reduce_border_radius (StThemeNode  *node,
                                    float         width,
                                    float         height,
                                    guint        *corners)
{
  gfloat scale;
  guint sum;

  scale = 1.0;

  /* top */
  sum = node->border_radius[ST_CORNER_TOPLEFT]
    + node->border_radius[ST_CORNER_TOPRIGHT];

  if (sum > 0)
    scale = MIN (width / sum, scale);

  /* right */
  sum = node->border_radius[ST_CORNER_TOPRIGHT]
    + node->border_radius[ST_CORNER_BOTTOMRIGHT];

  if (sum > 0)
    scale = MIN (height / sum, scale);

  /* bottom */
  sum = node->border_radius[ST_CORNER_BOTTOMLEFT]
    + node->border_radius[ST_CORNER_BOTTOMRIGHT];

  if (sum > 0)
    scale = MIN (width / sum, scale);

  /* left */
  sum = node->border_radius[ST_CORNER_BOTTOMLEFT]
    + node->border_radius[ST_CORNER_TOPLEFT];

  if (sum > 0)
    scale = MIN (height / sum, scale);

  corners[ST_CORNER_TOPLEFT]     = node->border_radius[ST_CORNER_TOPLEFT]     * scale;
  corners[ST_CORNER_TOPRIGHT]    = node->border_radius[ST_CORNER_TOPRIGHT]    * scale;
  corners[ST_CORNER_BOTTOMLEFT]  = node->border_radius[ST_CORNER_BOTTOMLEFT]  * scale;
  corners[ST_CORNER_BOTTOMRIGHT] = node->border_radius[ST_CORNER_BOTTOMRIGHT] * scale;
}

static void
st_theme_node_get_corner_border_widths (StThemeNode *node,
                                        StCorner     corner_id,
                                        guint       *border_width_1,
                                        guint       *border_width_2)
{
  switch (corner_id)
    {
      case ST_CORNER_TOPLEFT:
        if (border_width_1)
            *border_width_1 = node->border_width[ST_SIDE_TOP];
        if (border_width_2)
            *border_width_2 = node->border_width[ST_SIDE_LEFT];
        break;
      case ST_CORNER_TOPRIGHT:
        if (border_width_1)
            *border_width_1 = node->border_width[ST_SIDE_TOP];
        if (border_width_2)
            *border_width_2 = node->border_width[ST_SIDE_RIGHT];
        break;
      case ST_CORNER_BOTTOMRIGHT:
        if (border_width_1)
            *border_width_1 = node->border_width[ST_SIDE_BOTTOM];
        if (border_width_2)
            *border_width_2 = node->border_width[ST_SIDE_RIGHT];
        break;
      case ST_CORNER_BOTTOMLEFT:
        if (border_width_1)
            *border_width_1 = node->border_width[ST_SIDE_BOTTOM];
        if (border_width_2)
            *border_width_2 = node->border_width[ST_SIDE_LEFT];
        break;
      default:
        g_assert_not_reached();
        break;
    }
}

static CoglPipeline *
st_theme_node_lookup_corner (StThemeNode    *node,
                             float           width,
                             float           height,
                             float           resource_scale,
                             StCorner        corner_id)
{
  CoglTexture *texture = NULL;
  CoglPipeline *material = NULL;
  char *key;
  StTextureCache *cache;
  StCornerSpec corner;
  guint radius[4];

  cache = st_texture_cache_get_default ();

  st_theme_node_reduce_border_radius (node, width, height, radius);

  if (radius[corner_id] == 0)
    return NULL;

  corner.radius = radius[corner_id];
  corner.color = node->background_color;
  corner.resource_scale = resource_scale;
  st_theme_node_get_corner_border_widths (node, corner_id,
                                          &corner.border_width_1,
                                          &corner.border_width_2);

  switch (corner_id)
    {
      case ST_CORNER_TOPLEFT:
        over (&node->border_color[ST_SIDE_TOP], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_LEFT], &corner.color, &corner.border_color_2);
        break;
      case ST_CORNER_TOPRIGHT:
        over (&node->border_color[ST_SIDE_TOP], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_RIGHT], &corner.color, &corner.border_color_2);
        break;
      case ST_CORNER_BOTTOMRIGHT:
        over (&node->border_color[ST_SIDE_BOTTOM], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_RIGHT], &corner.color, &corner.border_color_2);
        break;
      case ST_CORNER_BOTTOMLEFT:
        over (&node->border_color[ST_SIDE_BOTTOM], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_LEFT], &corner.color, &corner.border_color_2);
        break;
      default:
        g_assert_not_reached();
        break;
    }

  if (corner.color.alpha == 0 &&
      corner.border_color_1.alpha == 0 &&
      corner.border_color_2.alpha == 0)
    {
      if (node->box_shadow == NULL)
        return NULL;
      else  /* We still need a corner texture to render the box-shadow */
        corner.color = (ClutterColor) {0, 0, 0, 255};
    }

  key = corner_to_string (&corner);
  texture = st_texture_cache_load (cache, key, ST_TEXTURE_CACHE_POLICY_FOREVER, load_corner, &corner, NULL);

  if (texture)
    {
      material = _st_create_texture_pipeline (texture);
      cogl_object_unref (texture);
    }

  g_free (key);

  return material;
}

static void
get_background_scale (StThemeNode *node,
                      gdouble      painting_area_width,
                      gdouble      painting_area_height,
                      gdouble      background_image_width,
                      gdouble      background_image_height,
                      gdouble     *scale_w,
                      gdouble     *scale_h)
{
  *scale_w = -1.0;
  *scale_h = -1.0;

  switch (node->background_size)
    {
      case ST_BACKGROUND_SIZE_AUTO:
        *scale_w = 1.0f;
        break;
      case ST_BACKGROUND_SIZE_CONTAIN:
        *scale_w = MIN (painting_area_width / background_image_width,
                        painting_area_height / background_image_height);
        break;
      case ST_BACKGROUND_SIZE_COVER:
        *scale_w = MAX (painting_area_width / background_image_width,
                        painting_area_height / background_image_height);
        break;
      case ST_BACKGROUND_SIZE_FIXED:
        if (node->background_size_w > -1)
          {
            *scale_w = node->background_size_w / background_image_width;
            if (node->background_size_h > -1)
              *scale_h = node->background_size_h / background_image_height;
          }
        else if (node->background_size_h > -1)
          *scale_w = node->background_size_h / background_image_height;
        break;
      default:
        g_assert_not_reached();
        break;
    }
  if (*scale_h < 0.0)
    *scale_h = *scale_w;
}

static void
get_background_coordinates (StThemeNode *node,
                            gdouble      painting_area_width,
                            gdouble      painting_area_height,
                            gdouble      background_image_width,
                            gdouble      background_image_height,
                            gdouble     *x,
                            gdouble     *y)
{
  /* honor the specified position if any */
  if (node->background_position_set)
    {
      *x = node->background_position_x;
      *y = node->background_position_y;
    }
  else
    {
      /* center the background on the widget */
      *x = (painting_area_width / 2.0) - (background_image_width / 2.0);
      *y = (painting_area_height / 2.0) - (background_image_height / 2.0);
    }
}

static void
get_background_position (StThemeNode             *self,
                         const ClutterActorBox   *allocation,
                         float                    resource_scale,
                         ClutterActorBox         *result,
                         ClutterActorBox         *texture_coords)
{
  gdouble painting_area_width, painting_area_height;
  gdouble background_image_width, background_image_height;
  gdouble x1, y1;
  gdouble scale_w, scale_h;

  /* get the background image size */
  background_image_width = cogl_texture_get_width (self->background_texture);
  background_image_height = cogl_texture_get_height (self->background_texture);

  background_image_width /= resource_scale;
  background_image_height /= resource_scale;

  /* get the painting area size */
  painting_area_width = allocation->x2 - allocation->x1;
  painting_area_height = allocation->y2 - allocation->y1;

  /* scale if requested */
  get_background_scale (self,
                        painting_area_width, painting_area_height,
                        background_image_width, background_image_height,
                        &scale_w, &scale_h);

  background_image_width *= scale_w;
  background_image_height *= scale_h;

  /* get coordinates */
  get_background_coordinates (self,
                              painting_area_width, painting_area_height,
                              background_image_width, background_image_height,
                              &x1, &y1);

  if (self->background_repeat)
    {
      gdouble width = allocation->x2 - allocation->x1 + x1;
      gdouble height = allocation->y2 - allocation->y1 + y1;

      *result = *allocation;

      /* reference image is at x1, y1 */
      texture_coords->x1 = x1 / background_image_width;
      texture_coords->y1 = y1 / background_image_height;
      texture_coords->x2 = width / background_image_width;
      texture_coords->y2 = height / background_image_height;
    }
  else
    {
      result->x1 = x1;
      result->y1 = y1;
      result->x2 = x1 + background_image_width;
      result->y2 = y1 + background_image_height;

      texture_coords->x1 = texture_coords->y1 = 0;
      texture_coords->x2 = texture_coords->y2 = 1;
    }
}

/* Use of this function marks code which doesn't support
 * non-uniform colors.
 */
static void
get_arbitrary_border_color (StThemeNode   *node,
                            ClutterColor  *color)
{
  if (color)
    st_theme_node_get_border_color (node, ST_SIDE_TOP, color);
}

static gboolean
st_theme_node_has_visible_outline (StThemeNode *node)
{
  if (node->background_color.alpha > 0)
    return TRUE;

  if (node->background_gradient_end.alpha > 0)
    return TRUE;

  if (node->border_radius[ST_CORNER_TOPLEFT] > 0 ||
      node->border_radius[ST_CORNER_TOPRIGHT] > 0 ||
      node->border_radius[ST_CORNER_BOTTOMLEFT] > 0 ||
      node->border_radius[ST_CORNER_BOTTOMRIGHT] > 0)
    return TRUE;

  if (node->border_width[ST_SIDE_TOP] > 0 ||
      node->border_width[ST_SIDE_LEFT] > 0 ||
      node->border_width[ST_SIDE_RIGHT] > 0 ||
      node->border_width[ST_SIDE_BOTTOM] > 0)
    return TRUE;

  return FALSE;
}

static cairo_pattern_t *
create_cairo_pattern_of_background_gradient (StThemeNode *node,
                                             float        width,
                                             float        height)
{
  cairo_pattern_t *pattern;

  g_return_val_if_fail (node->background_gradient_type != ST_GRADIENT_NONE,
                        NULL);

  if (node->background_gradient_type == ST_GRADIENT_VERTICAL)
    pattern = cairo_pattern_create_linear (0, 0, 0, height);
  else if (node->background_gradient_type == ST_GRADIENT_HORIZONTAL)
    pattern = cairo_pattern_create_linear (0, 0, width, 0);
  else
    {
      gdouble cx, cy;

      cx = width / 2.;
      cy = height / 2.;
      pattern = cairo_pattern_create_radial (cx, cy, 0, cx, cy, MIN (cx, cy));
    }

  cairo_pattern_add_color_stop_rgba (pattern, 0,
                                     node->background_color.red / 255.,
                                     node->background_color.green / 255.,
                                     node->background_color.blue / 255.,
                                     node->background_color.alpha / 255.);
  cairo_pattern_add_color_stop_rgba (pattern, 1,
                                     node->background_gradient_end.red / 255.,
                                     node->background_gradient_end.green / 255.,
                                     node->background_gradient_end.blue / 255.,
                                     node->background_gradient_end.alpha / 255.);
  return pattern;
}

static cairo_pattern_t *
create_cairo_pattern_of_background_image (StThemeNode *node,
                                          float        width,
                                          float        height,
                                          float        resource_scale,
                                          gboolean    *needs_background_fill)
{
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  cairo_content_t  content;
  cairo_matrix_t   matrix;
  GFile *file;

  StTextureCache *texture_cache;

  gdouble background_image_width, background_image_height;
  gdouble x, y;
  gdouble scale_w, scale_h;

  file = st_theme_node_get_background_image (node);

  texture_cache = st_texture_cache_get_default ();

  surface = st_texture_cache_load_file_to_cairo_surface (texture_cache, file,
                                                         node->cached_scale_factor,
                                                         resource_scale);

  if (surface == NULL)
    return NULL;

  g_assert (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE);

  content = cairo_surface_get_content (surface);
  pattern = cairo_pattern_create_for_surface (surface);

  background_image_width = cairo_image_surface_get_width (surface);
  background_image_height = cairo_image_surface_get_height (surface);

  *needs_background_fill = TRUE;

  cairo_matrix_init_identity (&matrix);

  if (resource_scale != 1.0)
    {
      background_image_width /= resource_scale;
      background_image_height /= resource_scale;

      cairo_matrix_scale (&matrix, resource_scale, resource_scale);
    }

  get_background_scale (node,
                        width, height,
                        background_image_width, background_image_height,
                        &scale_w, &scale_h);

  if ((scale_w != 1) || (scale_h != 1))
    cairo_matrix_scale (&matrix, 1.0/scale_w, 1.0/scale_h);

  background_image_width *= scale_w;
  background_image_height *= scale_h;

  get_background_coordinates (node,
                              width, height,
                              background_image_width, background_image_height,
                              &x, &y);
  cairo_matrix_translate (&matrix, -x, -y);

  if (node->background_repeat)
    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

  /* If it's opaque, fills up the entire allocated
   * area, then don't bother doing a background fill first
   */
  if (content != CAIRO_CONTENT_COLOR_ALPHA)
    {
      if (node->background_repeat ||
          (x >= 0 &&
           y >= 0 &&
           background_image_width - x >= width &&
           background_image_height -y >= height))
        *needs_background_fill = FALSE;
    }

  cairo_pattern_set_matrix (pattern, &matrix);

  return pattern;
}

/* fill_exterior = TRUE means that pattern is a surface pattern and
 * we should extend the pattern with a solid fill from its edges.
 * This is a bit of a hack; the alternative would be to make the
 * surface of the surface pattern 1 pixel bigger and use CAIRO_EXTEND_PAD.
 */
static void
paint_shadow_pattern_to_cairo_context (StShadow *shadow_spec,
                                       cairo_pattern_t *pattern,
                                       gboolean         fill_exterior,
                                       cairo_t         *cr,
                                       cairo_path_t    *interior_path,
                                       cairo_path_t    *outline_path)
{
  /* If there are borders, clip the shadow to the interior
   * of the borders; if there is a visible outline, clip the shadow to
   * that outline
   */
  cairo_path_t *path = (interior_path != NULL) ? interior_path : outline_path;
  double x1, x2, y1, y2;

  /* fill_exterior only makes sense if we're clipping the shadow - filling
   * to the edges of the surface would be silly */
  g_assert (!(fill_exterior && path == NULL));

  cairo_save (cr);
  if (path != NULL)
    {
      cairo_append_path (cr, path);

      /* There's no way to invert a path in cairo, so we need bounds for
       * the area we are drawing in order to create the "exterior" region.
       * Pixel align to hit fast paths.
       */
      if (fill_exterior)
        {
          cairo_path_extents (cr, &x1, &y1, &x2, &y2);
          x1 = floor (x1);
          y1 = floor (y1);
          x2 = ceil (x2);
          y2 = ceil (y2);
        }

      cairo_clip (cr);
    }

  cairo_set_source_rgba (cr,
                         shadow_spec->color.red / 255.0,
                         shadow_spec->color.green / 255.0,
                         shadow_spec->color.blue / 255.0,
                         shadow_spec->color.alpha / 255.0);
  if (fill_exterior)
    {
      cairo_surface_t *surface;
      int width, height;
      double xscale, yscale;
      cairo_matrix_t matrix;

      cairo_save (cr);

      /* Start with a rectangle enclosing the bounds of the clipped
       * region */
      cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);

      /* Then subtract out the bounds of the surface in the surface
       * pattern; we transform the context by the inverse of the
       * pattern matrix to get to surface coordinates */

      if (cairo_pattern_get_surface (pattern, &surface) != CAIRO_STATUS_SUCCESS)
        /* Something went wrong previously */
        goto no_surface;

      cairo_surface_get_device_scale (surface, &xscale, &yscale);
      width = cairo_image_surface_get_width  (surface);
      height = cairo_image_surface_get_height (surface);

      cairo_pattern_get_matrix (pattern, &matrix);
      cairo_matrix_invert (&matrix);
      cairo_matrix_scale (&matrix, 1.0 / xscale, 1.0 / yscale);
      cairo_transform (cr, &matrix);

      cairo_rectangle (cr, 0, height, width, - height);
      cairo_fill (cr);

    no_surface:
      cairo_restore (cr);
    }

  cairo_mask (cr, pattern);
  cairo_restore (cr);
}

static void
paint_background_image_shadow_to_cairo_context (StThemeNode     *node,
                                                StShadow        *shadow_spec,
                                                cairo_pattern_t *pattern,
                                                cairo_t         *cr,
                                                cairo_path_t    *interior_path,
                                                cairo_path_t    *outline_path,
                                                int              x,
                                                int              y,
                                                int              width,
                                                int              height,
                                                float            resource_scale)
{
  cairo_pattern_t *shadow_pattern;

  g_assert (shadow_spec != NULL);
  g_assert (pattern != NULL);

  if (outline_path != NULL)
    {
      cairo_surface_t *clipped_surface;
      cairo_pattern_t *clipped_pattern;
      cairo_t *temp_cr;

      /* Prerender the pattern to a temporary surface,
       * so it's properly clipped before we create a shadow from it
       */
      width = ceilf (width * resource_scale);
      height = ceilf (height * resource_scale);
      clipped_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
      cairo_surface_set_device_scale (clipped_surface, resource_scale, resource_scale);
      temp_cr = cairo_create (clipped_surface);

      cairo_set_operator (temp_cr, CAIRO_OPERATOR_CLEAR);
      cairo_paint (temp_cr);
      cairo_set_operator (temp_cr, CAIRO_OPERATOR_SOURCE);

      if (interior_path != NULL)
        {
          cairo_append_path (temp_cr, interior_path);
          cairo_clip (temp_cr);
        }

      cairo_append_path (temp_cr, outline_path);
      cairo_translate (temp_cr, x, y);
      cairo_set_source (temp_cr, pattern);
      cairo_clip (temp_cr);
      cairo_paint (temp_cr);
      cairo_destroy (temp_cr);

      clipped_pattern = cairo_pattern_create_for_surface (clipped_surface);
      cairo_surface_destroy (clipped_surface);

      shadow_pattern = _st_create_shadow_cairo_pattern (shadow_spec,
                                                        clipped_pattern);
      cairo_pattern_destroy (clipped_pattern);
    }
  else
    {
      shadow_pattern = _st_create_shadow_cairo_pattern (shadow_spec,
                                                        pattern);
    }

  paint_shadow_pattern_to_cairo_context (shadow_spec,
                                         shadow_pattern, FALSE,
                                         cr,
                                         interior_path,
                                         outline_path);
  cairo_pattern_destroy (shadow_pattern);
}

/* gets the extents of a cairo_path_t; slightly inefficient, but much simpler than
 * computing from the raw path data */
static void
path_extents (cairo_path_t *path,
              double       *x1,
              double       *y1,
              double       *x2,
              double       *y2)

{
  cairo_surface_t *dummy = cairo_image_surface_create (CAIRO_FORMAT_A8, 1, 1);
  cairo_t *cr = cairo_create (dummy);

  cairo_append_path (cr, path);
  cairo_path_extents (cr, x1, y1, x2, y2);

  cairo_destroy (cr);
  cairo_surface_destroy (dummy);
}

static void
paint_inset_box_shadow_to_cairo_context (StThemeNode     *node,
                                         StShadow        *shadow_spec,
                                         float            resource_scale,
                                         cairo_t         *cr,
                                         cairo_path_t    *shadow_outline)
{
  cairo_surface_t *shadow_surface;
  cairo_pattern_t *shadow_pattern;
  double extents_x1, extents_y1, extents_x2, extents_y2;
  double shrunk_extents_x1, shrunk_extents_y1, shrunk_extents_x2, shrunk_extents_y2;
  gboolean fill_exterior;

  g_assert (shadow_spec != NULL);
  g_assert (shadow_outline != NULL);

  /* Create the pattern used to create the inset shadow; as the shadow
   * should be drawn as if everything outside the outline was opaque,
   * we use a temporary surface to draw the background as a solid shape,
   * which is inverted when creating the shadow pattern.
   */

  /* First we need to find the size of the temporary surface
   */
  path_extents (shadow_outline,
                &extents_x1, &extents_y1, &extents_x2, &extents_y2);

  /* Shrink the extents by the spread, and offset */
  shrunk_extents_x1 = extents_x1 + shadow_spec->xoffset + shadow_spec->spread;
  shrunk_extents_y1 = extents_y1 + shadow_spec->yoffset + shadow_spec->spread;
  shrunk_extents_x2 = extents_x2 + shadow_spec->xoffset - shadow_spec->spread;
  shrunk_extents_y2 = extents_y2 + shadow_spec->yoffset - shadow_spec->spread;

  if (shrunk_extents_x1 >= shrunk_extents_x2 || shrunk_extents_y1 >= shrunk_extents_y2)
    {
      /* Shadow occupies entire area within border */
      shadow_pattern = cairo_pattern_create_rgb (0., 0., 0.);
      fill_exterior = FALSE;
    }
  else
    {
      /* Bounds of temporary surface */
      int surface_x = floor (shrunk_extents_x1);
      int surface_y = floor (shrunk_extents_y1);
      int surface_width = ceil ((shrunk_extents_x2 - surface_x) * resource_scale);
      int surface_height = ceil ((shrunk_extents_y2 - surface_y) * resource_scale);

      /* Center of the original path */
      double x_center = (extents_x1 + extents_x2) / 2;
      double y_center = (extents_y1 + extents_y2) / 2;

      cairo_pattern_t *pattern;
      cairo_t *temp_cr;
      cairo_matrix_t matrix;

      shadow_surface = cairo_image_surface_create (CAIRO_FORMAT_A8, surface_width, surface_height);
      cairo_surface_set_device_scale (shadow_surface, resource_scale, resource_scale);
      temp_cr = cairo_create (shadow_surface);

      /* Match the coordinates in the temporary context to the parent context */
      cairo_translate (temp_cr, - surface_x, - surface_y);

      /* Shadow offset */
      cairo_translate (temp_cr, shadow_spec->xoffset, shadow_spec->yoffset);

      /* Scale the path around the center to match the shrunk bounds */
      cairo_translate (temp_cr, x_center, y_center);
      cairo_scale (temp_cr,
                   (shrunk_extents_x2 - shrunk_extents_x1) / (extents_x2 - extents_x1),
                   (shrunk_extents_y2 - shrunk_extents_y1) / (extents_y2 - extents_y1));
      cairo_translate (temp_cr, - x_center, - y_center);

      cairo_append_path (temp_cr, shadow_outline);
      cairo_fill (temp_cr);
      cairo_destroy (temp_cr);

      pattern = cairo_pattern_create_for_surface (shadow_surface);
      cairo_surface_destroy (shadow_surface);

      /* The pattern needs to be offset back to coordinates in the parent context */
      cairo_matrix_init_translate (&matrix, - surface_x, - surface_y);
      cairo_pattern_set_matrix (pattern, &matrix);

      shadow_pattern = _st_create_shadow_cairo_pattern (shadow_spec, pattern);
      fill_exterior = TRUE;

      cairo_pattern_destroy (pattern);
    }

  paint_shadow_pattern_to_cairo_context (shadow_spec,
                                         shadow_pattern, fill_exterior,
                                         cr,
                                         shadow_outline,
                                         NULL);

  cairo_pattern_destroy (shadow_pattern);
}

/* In order for borders to be smoothly blended with non-solid backgrounds,
 * we need to use cairo.  This function is a slow fallback path for those
 * cases (gradients, background images, etc).
 */
static CoglTexture *
st_theme_node_prerender_background (StThemeNode *node,
                                    float        actor_width,
                                    float        actor_height,
                                    float        resource_scale)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  CoglContext *ctx = clutter_backend_get_cogl_context (backend);
  GError *error = NULL;
  StBorderImage *border_image;
  CoglTexture *texture;
  guint radius[4];
  int i;
  cairo_t *cr;
  cairo_surface_t *surface;
  StShadow *shadow_spec;
  StShadow *box_shadow_spec;
  cairo_pattern_t *pattern = NULL;
  cairo_path_t *outline_path = NULL;
  gboolean draw_solid_background = TRUE;
  gboolean background_is_translucent;
  gboolean interior_dirty;
  gboolean draw_background_image_shadow = FALSE;
  gboolean has_visible_outline;
  ClutterColor border_color;
  guint border_width[4];
  guint rowstride;
  guchar *data;
  ClutterActorBox actor_box;
  ClutterActorBox paint_box;
  cairo_path_t *interior_path = NULL;
  float width, height;
  int texture_width;
  int texture_height;

  border_image = st_theme_node_get_border_image (node);

  shadow_spec = st_theme_node_get_background_image_shadow (node);
  box_shadow_spec = st_theme_node_get_box_shadow (node);

  actor_box.x1 = 0;
  actor_box.x2 = actor_width;
  actor_box.y1 = 0;
  actor_box.y2 = actor_height;

  /* If there's a background image shadow, we
   * may need to create an image bigger than the nodes
   * allocation
   */
  st_theme_node_get_background_paint_box (node, &actor_box, &paint_box);

  /* translate the boxes so the paint box is at 0,0
  */
  actor_box.x1 += - paint_box.x1;
  actor_box.x2 += - paint_box.x1;
  actor_box.y1 += - paint_box.y1;
  actor_box.y2 += - paint_box.y1;

  width = paint_box.x2 - paint_box.x1;
  height = paint_box.y2 - paint_box.y1;

  texture_width = ceilf (width * resource_scale);
  texture_height = ceilf (height * resource_scale);

  rowstride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, texture_width);
  data = g_new0 (guchar, texture_height * rowstride);

  /* We zero initialize the destination memory, so it's fully transparent
   * by default.
   */
  interior_dirty = FALSE;

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 texture_width, texture_height,
                                                 rowstride);
  cairo_surface_set_device_scale (surface, resource_scale, resource_scale);
  cr = cairo_create (surface);

  /* TODO - support non-uniform border colors */
  get_arbitrary_border_color (node, &border_color);

  st_theme_node_reduce_border_radius (node, width, height, radius);

  for (i = 0; i < 4; i++)
    border_width[i] = st_theme_node_get_border_width (node, i);

  /* Note we don't support translucent background images on top
   * of gradients. It's strictly either/or.
   */
  if (node->background_gradient_type != ST_GRADIENT_NONE)
    {
      pattern = create_cairo_pattern_of_background_gradient (node, width, height);
      draw_solid_background = FALSE;

      /* If the gradient has any translucent areas, we need to
       * erase the interior region before drawing, so that we show
       * what's actually under the gradient and not whatever is
       * left over from filling the border, etc.
       */
      if (node->background_color.alpha < 255 ||
          node->background_gradient_end.alpha < 255)
        background_is_translucent = TRUE;
      else
        background_is_translucent = FALSE;
    }
  else
    {
      GFile *background_image;

      background_image = st_theme_node_get_background_image (node);

      if (background_image != NULL)
        {
          pattern = create_cairo_pattern_of_background_image (node,
                                                              width, height,
                                                              resource_scale,
                                                              &draw_solid_background);
          if (shadow_spec && pattern != NULL)
            draw_background_image_shadow = TRUE;
        }

      /* We never need to clear the interior region before drawing the
       * background image, because it either always fills the entire area
       * opaquely, or we draw the solid background behind it.
       */
      background_is_translucent = FALSE;
    }

  if (pattern == NULL)
    draw_solid_background = TRUE;

  /* drawing the solid background implicitly clears the interior
   * region, so if we're going to draw a solid background before drawing
   * the background pattern, then we don't need to bother also clearing the
   * background region.
   */
  if (draw_solid_background)
    background_is_translucent = FALSE;

  has_visible_outline = st_theme_node_has_visible_outline (node);

  /* Create a path for the background's outline first */
  if (radius[ST_CORNER_TOPLEFT] > 0)
    cairo_arc (cr,
               actor_box.x1 + radius[ST_CORNER_TOPLEFT],
               actor_box.y1 + radius[ST_CORNER_TOPLEFT],
               radius[ST_CORNER_TOPLEFT], M_PI, 3 * M_PI / 2);
  else
    cairo_move_to (cr, actor_box.x1, actor_box.y1);
  cairo_line_to (cr, actor_box.x2 - radius[ST_CORNER_TOPRIGHT], actor_box.x1);
  if (radius[ST_CORNER_TOPRIGHT] > 0)
    cairo_arc (cr,
               actor_box.x2 - radius[ST_CORNER_TOPRIGHT],
               actor_box.x1 + radius[ST_CORNER_TOPRIGHT],
               radius[ST_CORNER_TOPRIGHT], 3 * M_PI / 2, 2 * M_PI);
  cairo_line_to (cr, actor_box.x2, actor_box.y2 - radius[ST_CORNER_BOTTOMRIGHT]);
  if (radius[ST_CORNER_BOTTOMRIGHT] > 0)
    cairo_arc (cr,
               actor_box.x2 - radius[ST_CORNER_BOTTOMRIGHT],
               actor_box.y2 - radius[ST_CORNER_BOTTOMRIGHT],
               radius[ST_CORNER_BOTTOMRIGHT], 0, M_PI / 2);
  cairo_line_to (cr, actor_box.x1 + radius[ST_CORNER_BOTTOMLEFT], actor_box.y2);
  if (radius[ST_CORNER_BOTTOMLEFT] > 0)
    cairo_arc (cr,
               actor_box.x1 + radius[ST_CORNER_BOTTOMLEFT],
               actor_box.y2 - radius[ST_CORNER_BOTTOMLEFT],
               radius[ST_CORNER_BOTTOMLEFT], M_PI / 2, M_PI);
  cairo_close_path (cr);

  outline_path = cairo_copy_path (cr);

  /* If we have a solid border, we fill the outline shape with the border
   * color and create the inline shape for the background;
   * otherwise the outline shape is filled with the background
   * directly
   */
  if (border_image == NULL &&
      (border_width[ST_SIDE_TOP] > 0 ||
       border_width[ST_SIDE_RIGHT] > 0 ||
       border_width[ST_SIDE_BOTTOM] > 0 ||
       border_width[ST_SIDE_LEFT] > 0))
    {
      cairo_set_source_rgba (cr,
                             border_color.red / 255.,
                             border_color.green / 255.,
                             border_color.blue / 255.,
                             border_color.alpha / 255.);
      cairo_fill (cr);

      /* We were sloppy when filling in the border, and now the interior
       * is filled with the border color, too.
       */
      interior_dirty = TRUE;

      if (radius[ST_CORNER_TOPLEFT] > MAX(border_width[ST_SIDE_TOP],
                                          border_width[ST_SIDE_LEFT]))
        elliptical_arc (cr,
                        actor_box.x1 + radius[ST_CORNER_TOPLEFT],
                        actor_box.y1 + radius[ST_CORNER_TOPLEFT],
                        radius[ST_CORNER_TOPLEFT] - border_width[ST_SIDE_LEFT],
                        radius[ST_CORNER_TOPLEFT] - border_width[ST_SIDE_TOP],
                        M_PI, 3 * M_PI / 2);
      else
        cairo_move_to (cr,
                       actor_box.x1 + border_width[ST_SIDE_LEFT],
                       actor_box.y1 + border_width[ST_SIDE_TOP]);

      cairo_line_to (cr,
                     actor_box.x2 - MAX(radius[ST_CORNER_TOPRIGHT], border_width[ST_SIDE_RIGHT]),
                     actor_box.y1 + border_width[ST_SIDE_TOP]);

      if (radius[ST_CORNER_TOPRIGHT] > MAX(border_width[ST_SIDE_TOP],
                                           border_width[ST_SIDE_RIGHT]))
        elliptical_arc (cr,
                        actor_box.x2 - radius[ST_CORNER_TOPRIGHT],
                        actor_box.y1 + radius[ST_CORNER_TOPRIGHT],
                        radius[ST_CORNER_TOPRIGHT] - border_width[ST_SIDE_RIGHT],
                        radius[ST_CORNER_TOPRIGHT] - border_width[ST_SIDE_TOP],
                        3 * M_PI / 2, 2 * M_PI);
      else
        cairo_line_to (cr,
                       actor_box.x2 - border_width[ST_SIDE_RIGHT],
                       actor_box.y1 + border_width[ST_SIDE_TOP]);

      cairo_line_to (cr,
                     actor_box.x2 - border_width[ST_SIDE_RIGHT],
                     actor_box.y2 - MAX(radius[ST_CORNER_BOTTOMRIGHT], border_width[ST_SIDE_BOTTOM]));

      if (radius[ST_CORNER_BOTTOMRIGHT] > MAX(border_width[ST_SIDE_BOTTOM],
                                              border_width[ST_SIDE_RIGHT]))
        elliptical_arc (cr,
                        actor_box.x2 - radius[ST_CORNER_BOTTOMRIGHT],
                        actor_box.y2 - radius[ST_CORNER_BOTTOMRIGHT],
                        radius[ST_CORNER_BOTTOMRIGHT] - border_width[ST_SIDE_RIGHT],
                        radius[ST_CORNER_BOTTOMRIGHT] - border_width[ST_SIDE_BOTTOM],
                        0, M_PI / 2);
      else
        cairo_line_to (cr,
                       actor_box.x2 - border_width[ST_SIDE_RIGHT],
                       actor_box.y2 - border_width[ST_SIDE_BOTTOM]);

      cairo_line_to (cr,
                     MAX(radius[ST_CORNER_BOTTOMLEFT], border_width[ST_SIDE_LEFT]),
                     actor_box.y2 - border_width[ST_SIDE_BOTTOM]);

      if (radius[ST_CORNER_BOTTOMLEFT] > MAX(border_width[ST_SIDE_BOTTOM],
                                             border_width[ST_SIDE_LEFT]))
        elliptical_arc (cr,
                        actor_box.x1 + radius[ST_CORNER_BOTTOMLEFT],
                        actor_box.y2 - radius[ST_CORNER_BOTTOMLEFT],
                        radius[ST_CORNER_BOTTOMLEFT] - border_width[ST_SIDE_LEFT],
                        radius[ST_CORNER_BOTTOMLEFT] - border_width[ST_SIDE_BOTTOM],
                        M_PI / 2, M_PI);
      else
        cairo_line_to (cr,
                       actor_box.x1 + border_width[ST_SIDE_LEFT],
                       actor_box.y2 - border_width[ST_SIDE_BOTTOM]);

      cairo_close_path (cr);

      interior_path = cairo_copy_path (cr);

      /* clip drawing to the region inside of the borders
       */
      cairo_clip (cr);

      /* But fill the pattern as if it started at the edge of outline,
       * behind the borders.  This is similar to
       * background-clip: border-box; semantics.
       */
      cairo_append_path (cr, outline_path);
    }

  if (interior_dirty && background_is_translucent)
    {
      cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
      cairo_fill_preserve (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    }

  if (draw_solid_background)
    {
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

      cairo_set_source_rgba (cr,
                             node->background_color.red / 255.,
                             node->background_color.green / 255.,
                             node->background_color.blue / 255.,
                             node->background_color.alpha / 255.);
      cairo_fill_preserve (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    }

  if (draw_background_image_shadow)
    {
      paint_background_image_shadow_to_cairo_context (node,
                                                      shadow_spec,
                                                      pattern,
                                                      cr,
                                                      interior_path,
                                                      has_visible_outline?  outline_path : NULL,
                                                      actor_box.x1,
                                                      actor_box.y1,
                                                      width, height,
                                                      resource_scale);
      cairo_append_path (cr, outline_path);
    }

  cairo_translate (cr, actor_box.x1, actor_box.y1);

  if (pattern != NULL)
    {
      cairo_set_source (cr, pattern);
      cairo_fill (cr);
      cairo_pattern_destroy (pattern);
    }

  if (box_shadow_spec && box_shadow_spec->inset)
    {
      paint_inset_box_shadow_to_cairo_context (node,
                                               box_shadow_spec,
                                               resource_scale,
                                               cr,
                                               interior_path ? interior_path
                                                             : outline_path);
    }

  if (outline_path != NULL)
    cairo_path_destroy (outline_path);

  if (interior_path != NULL)
    cairo_path_destroy (interior_path);

  texture = COGL_TEXTURE (cogl_texture_2d_new_from_data (ctx,
                                                         texture_width,
                                                         texture_height,
                                                         CLUTTER_CAIRO_FORMAT_ARGB32,
                                                         rowstride,
                                                         data,
                                                         &error));

  if (error)
    {
      g_warning ("Failed to allocate texture: %s", error->message);
      g_error_free (error);
    }

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  g_free (data);

  return texture;
}

static void st_theme_node_paint_borders (StThemeNodePaintState *state,
                                         CoglFramebuffer       *framebuffer,
                                         const ClutterActorBox *box,
                                         StPaintBordersMode     mode,
                                         guint8                 paint_opacity);

void
st_theme_node_invalidate_border_image (StThemeNode *node)
{
  cogl_clear_object (&node->border_slices_texture);
  cogl_clear_object (&node->border_slices_pipeline);
}

static gboolean
st_theme_node_load_border_image (StThemeNode *node,
                                 gfloat       resource_scale)
{
  if (node->border_slices_texture == NULL)
    {
      StBorderImage *border_image;
      GFile *file;

      border_image = st_theme_node_get_border_image (node);
      if (border_image == NULL)
        goto out;

      file = st_border_image_get_file (border_image);

      node->border_slices_texture = st_texture_cache_load_file_to_cogl_texture (st_texture_cache_get_default (),
                                                                                file,
                                                                                node->cached_scale_factor,
                                                                                resource_scale);
      if (node->border_slices_texture == NULL)
        goto out;

      node->border_slices_pipeline = _st_create_texture_pipeline (node->border_slices_texture);
    }

 out:
  return node->border_slices_texture != NULL;
}

void
st_theme_node_invalidate_background_image (StThemeNode *node)
{
  cogl_clear_object (&node->background_texture);
  cogl_clear_object (&node->background_pipeline);
  cogl_clear_object (&node->background_shadow_pipeline);
}

static gboolean
st_theme_node_load_background_image (StThemeNode *node,
                                     gfloat       resource_scale)
{
  if (node->background_texture == NULL)
    {
      GFile *background_image;
      StShadow *background_image_shadow_spec;

      background_image = st_theme_node_get_background_image (node);
      if (background_image == NULL)
        goto out;

      background_image_shadow_spec = st_theme_node_get_background_image_shadow (node);
      node->background_texture = st_texture_cache_load_file_to_cogl_texture (st_texture_cache_get_default (),
                                                                             background_image,
                                                                             node->cached_scale_factor,
                                                                             resource_scale);
      if (node->background_texture == NULL)
        goto out;

      node->background_pipeline = _st_create_texture_pipeline (node->background_texture);

      if (node->background_repeat)
        cogl_pipeline_set_layer_wrap_mode (node->background_pipeline, 0,
                                           COGL_PIPELINE_WRAP_MODE_REPEAT);

      if (background_image_shadow_spec)
        {
          node->background_shadow_pipeline = _st_create_shadow_pipeline (background_image_shadow_spec,
                                                                         node->background_texture,
                                                                         resource_scale);
        }
    }

 out:
  return node->background_texture != NULL;
}

static gboolean
st_theme_node_invalidate_resources_for_file (StThemeNode *node,
                                             GFile       *file)
{
  StBorderImage *border_image;
  gboolean changed = FALSE;
  GFile *theme_file;

  theme_file = st_theme_node_get_background_image (node);
  if ((theme_file != NULL) && g_file_equal (theme_file, file))
    {
      st_theme_node_invalidate_background_image (node);
      changed = TRUE;
    }

  border_image = st_theme_node_get_border_image (node);
  theme_file = border_image ? st_border_image_get_file (border_image) : NULL;
  if ((theme_file != NULL) && g_file_equal (theme_file, file))
    {
      st_theme_node_invalidate_border_image (node);
      changed = TRUE;
    }

  return changed;
}

static void st_theme_node_compute_maximum_borders (StThemeNodePaintState *state);
static void st_theme_node_prerender_shadow (StThemeNodePaintState *state);

static void
st_theme_node_render_resources (StThemeNodePaintState *state,
                                StThemeNode           *node,
                                float                  width,
                                float                  height,
                                float                  resource_scale)
{
  gboolean has_border;
  gboolean has_border_radius;
  gboolean has_inset_box_shadow;
  gboolean has_large_corners;
  StShadow *box_shadow_spec;

  g_return_if_fail (width > 0 && height > 0);

  /* FIXME - need to separate this into things that need to be recomputed on
   * geometry change versus things that can be cached regardless, such as
   * a background image.
   */
  st_theme_node_paint_state_free (state);

  st_theme_node_paint_state_set_node (state, node);
  state->alloc_width = width;
  state->alloc_height = height;
  state->resource_scale = resource_scale;

  _st_theme_node_ensure_background (node);
  _st_theme_node_ensure_geometry (node);

  box_shadow_spec = st_theme_node_get_box_shadow (node);
  has_inset_box_shadow = box_shadow_spec && box_shadow_spec->inset;

  if (node->border_width[ST_SIDE_TOP] > 0 ||
      node->border_width[ST_SIDE_LEFT] > 0 ||
      node->border_width[ST_SIDE_RIGHT] > 0 ||
      node->border_width[ST_SIDE_BOTTOM] > 0)
    has_border = TRUE;
  else
    has_border = FALSE;

  if (node->border_radius[ST_CORNER_TOPLEFT] > 0 ||
      node->border_radius[ST_CORNER_TOPRIGHT] > 0 ||
      node->border_radius[ST_CORNER_BOTTOMLEFT] > 0 ||
      node->border_radius[ST_CORNER_BOTTOMRIGHT] > 0)
    has_border_radius = TRUE;
  else
    has_border_radius = FALSE;

  /* The cogl code pads each corner to the maximum border radius,
   * which results in overlapping corner areas if the radius
   * exceeds the actor's halfsize, causing rendering errors.
   * Fall back to cairo in these cases. */
  has_large_corners = FALSE;

  if (has_border_radius) {
    guint border_radius[4];
    int corner;

    st_theme_node_reduce_border_radius (node, width, height, border_radius);

    for (corner = 0; corner < 4; corner ++) {
      if (border_radius[corner] * 2 > height ||
          border_radius[corner] * 2 > width) {
        has_large_corners = TRUE;
        break;
      }
    }
  }

  state->corner_material[ST_CORNER_TOPLEFT] =
    st_theme_node_lookup_corner (node, width, height, resource_scale, ST_CORNER_TOPLEFT);
  state->corner_material[ST_CORNER_TOPRIGHT] =
    st_theme_node_lookup_corner (node, width, height, resource_scale, ST_CORNER_TOPRIGHT);
  state->corner_material[ST_CORNER_BOTTOMRIGHT] =
    st_theme_node_lookup_corner (node, width, height, resource_scale, ST_CORNER_BOTTOMRIGHT);
  state->corner_material[ST_CORNER_BOTTOMLEFT] =
    st_theme_node_lookup_corner (node, width, height, resource_scale, ST_CORNER_BOTTOMLEFT);

  /* Use cairo to prerender the node if there is a gradient, or
   * background image with borders and/or rounded corners,
   * or large corners, since we can't do those things
   * easily with cogl.
   *
   * FIXME: if we could figure out ahead of time that a
   * background image won't overlap with the node borders,
   * then we could use cogl for that case.
   */
  if ((node->background_gradient_type != ST_GRADIENT_NONE)
      || (has_inset_box_shadow && (has_border || node->background_color.alpha > 0))
      || (st_theme_node_get_background_image (node) && (has_border || has_border_radius))
      || has_large_corners)
    state->prerendered_texture = st_theme_node_prerender_background (node, width, height,
                                                                     resource_scale);

  if (state->prerendered_texture)
    state->prerendered_pipeline = _st_create_texture_pipeline (state->prerendered_texture);
  else
    state->prerendered_pipeline = NULL;

  if (box_shadow_spec && !has_inset_box_shadow)
    {
      st_theme_node_compute_maximum_borders (state);

      if (st_theme_node_load_border_image (node, resource_scale))
        state->box_shadow_pipeline = _st_create_shadow_pipeline (box_shadow_spec,
                                                                 node->border_slices_texture,
                                                                 state->resource_scale);
      else if (state->prerendered_texture != NULL)
        state->box_shadow_pipeline = _st_create_shadow_pipeline (box_shadow_spec,
                                                                 state->prerendered_texture,
                                                                 state->resource_scale);
      else
        st_theme_node_prerender_shadow (state);
    }

  /* If we don't have cached textures yet, check whether we can cache
     them. */
  if (!node->cached_textures)
    {
      if (state->prerendered_pipeline == NULL &&
          width >= node->box_shadow_min_width &&
          height >= node->box_shadow_min_height)
        {
          st_theme_node_paint_state_copy (&node->cached_state, state);
          node->cached_textures = TRUE;
        }
    }
}

static void
st_theme_node_update_resources (StThemeNodePaintState *state,
                                StThemeNode           *node,
                                float                  width,
                                float                  height,
                                float                  resource_scale)
{
  gboolean had_prerendered_texture = FALSE;
  gboolean had_box_shadow = FALSE;
  StShadow *box_shadow_spec;

  g_return_if_fail (width > 0 && height > 0);

  /* Free handles we can't reuse */
  had_prerendered_texture = (state->prerendered_texture != NULL);
  cogl_clear_object (&state->prerendered_texture);

  if (state->prerendered_pipeline != NULL)
    {
      cogl_clear_object (&state->prerendered_pipeline);

      if (node->border_slices_texture == NULL &&
          state->box_shadow_pipeline != NULL)
        {
          cogl_clear_object (&state->box_shadow_pipeline);
          had_box_shadow = TRUE;
        }
    }

  st_theme_node_paint_state_set_node (state, node);
  state->alloc_width = width;
  state->alloc_height = height;
  state->resource_scale = resource_scale;

  box_shadow_spec = st_theme_node_get_box_shadow (node);

  if (had_prerendered_texture)
    {
      state->prerendered_texture = st_theme_node_prerender_background (node, width, height, resource_scale);
      state->prerendered_pipeline = _st_create_texture_pipeline (state->prerendered_texture);
    }
  else
    {
      int corner_id;

      for (corner_id = 0; corner_id < 4; corner_id++)
        if (state->corner_material[corner_id] == NULL)
          state->corner_material[corner_id] =
            st_theme_node_lookup_corner (node, width, height, resource_scale, corner_id);
    }

  if (had_box_shadow)
    state->box_shadow_pipeline = _st_create_shadow_pipeline (box_shadow_spec,
                                                             state->prerendered_texture,
                                                             state->resource_scale);
}

static void
paint_material_with_opacity (CoglPipeline    *material,
                             CoglFramebuffer *framebuffer,
                             ClutterActorBox *box,
                             ClutterActorBox *coords,
                             guint8           paint_opacity)
{
  cogl_pipeline_set_color4ub (material,
                              paint_opacity, paint_opacity, paint_opacity, paint_opacity);

  if (coords)
    cogl_framebuffer_draw_textured_rectangle (framebuffer, material,
                                              box->x1, box->y1, box->x2, box->y2,
                                              coords->x1, coords->y1, coords->x2, coords->y2);
  else
    cogl_framebuffer_draw_rectangle (framebuffer, material,
                                     box->x1, box->y1, box->x2, box->y2);
}

static void
st_theme_node_ensure_color_pipeline (StThemeNode *node)
{
  static CoglPipeline *color_pipeline_template = NULL;

  if (node->color_pipeline != NULL)
    return;

  if (G_UNLIKELY (color_pipeline_template == NULL))
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      color_pipeline_template = cogl_pipeline_new (ctx);
    }

  node->color_pipeline = cogl_pipeline_copy (color_pipeline_template);
}

static void
st_theme_node_paint_borders (StThemeNodePaintState *state,
                             CoglFramebuffer       *framebuffer,
                             const ClutterActorBox *box,
                             StPaintBordersMode     mode,
                             guint8                 paint_opacity)
{
  StThemeNode *node = state->node;
  float width, height;
  guint border_width[4];
  guint border_radius[4];
  guint max_border_radius = 0;
  guint max_width_radius[4];
  int corner_id, side_id;
  ClutterColor border_color;
  guint8 alpha;
  gboolean corners_are_transparent;

  width = box->x2 - box->x1;
  height = box->y2 - box->y1;

  /* TODO - support non-uniform border colors */
  get_arbitrary_border_color (node, &border_color);

  for (side_id = 0; side_id < 4; side_id++)
    border_width[side_id] = st_theme_node_get_border_width(node, side_id);

  st_theme_node_reduce_border_radius (node, width, height, border_radius);

  for (corner_id = 0; corner_id < 4; corner_id++)
    {
      guint border_width_1, border_width_2;

      st_theme_node_get_corner_border_widths (node, corner_id,
                                              &border_width_1, &border_width_2);

      if (border_radius[corner_id] > max_border_radius)
        max_border_radius = border_radius[corner_id];
      max_width_radius[corner_id] = MAX(MAX(border_width_1, border_width_2),
                                        border_radius[corner_id]);
    }

  /* borders */
  if (border_width[ST_SIDE_TOP] > 0 ||
      border_width[ST_SIDE_RIGHT] > 0 ||
      border_width[ST_SIDE_BOTTOM] > 0 ||
      border_width[ST_SIDE_LEFT] > 0)
    {
      ClutterColor effective_border;
      gboolean skip_corner_1, skip_corner_2;
      float rects[16];

      over (&border_color, &node->background_color, &effective_border);
      alpha = paint_opacity * effective_border.alpha / 255;

      if (alpha > 0)
        {
          st_theme_node_ensure_color_pipeline (node);
          cogl_pipeline_set_color4ub (node->color_pipeline,
                                      effective_border.red * alpha / 255,
                                      effective_border.green * alpha / 255,
                                      effective_border.blue * alpha / 255,
                                      alpha);

          /* NORTH */
          skip_corner_1 = border_radius[ST_CORNER_TOPLEFT] > 0;
          skip_corner_2 = border_radius[ST_CORNER_TOPRIGHT] > 0;

          rects[0] = skip_corner_1 ? max_width_radius[ST_CORNER_TOPLEFT] : 0;
          rects[1] = 0;
          rects[2] = skip_corner_2 ? width - max_width_radius[ST_CORNER_TOPRIGHT] : width;
          rects[3] = border_width[ST_SIDE_TOP];

          /* EAST */
          skip_corner_1 = border_radius[ST_CORNER_TOPRIGHT] > 0;
          skip_corner_2 = border_radius[ST_CORNER_BOTTOMRIGHT] > 0;

          rects[4] = width - border_width[ST_SIDE_RIGHT];
          rects[5] = skip_corner_1 ? max_width_radius[ST_CORNER_TOPRIGHT]
                             : border_width[ST_SIDE_TOP];
          rects[6] = width;
          rects[7] = skip_corner_2 ? height - max_width_radius[ST_CORNER_BOTTOMRIGHT]
                             : height - border_width[ST_SIDE_BOTTOM];

          /* SOUTH */
          skip_corner_1 = border_radius[ST_CORNER_BOTTOMLEFT] > 0;
          skip_corner_2 = border_radius[ST_CORNER_BOTTOMRIGHT] > 0;

          rects[8] = skip_corner_1 ? max_width_radius[ST_CORNER_BOTTOMLEFT] : 0;
          rects[9] = height - border_width[ST_SIDE_BOTTOM];
          rects[10] = skip_corner_2 ? width - max_width_radius[ST_CORNER_BOTTOMRIGHT]
                             : width;
          rects[11] = height;

          /* WEST */
          skip_corner_1 = border_radius[ST_CORNER_TOPLEFT] > 0;
          skip_corner_2 = border_radius[ST_CORNER_BOTTOMLEFT] > 0;

          rects[12] = 0;
          rects[13] = skip_corner_1 ? max_width_radius[ST_CORNER_TOPLEFT]
                             : border_width[ST_SIDE_TOP];
          rects[14] = border_width[ST_SIDE_LEFT];
          rects[15] = skip_corner_2 ? height - max_width_radius[ST_CORNER_BOTTOMLEFT]
                             : height - border_width[ST_SIDE_BOTTOM];

          cogl_framebuffer_draw_rectangles (framebuffer,
                                            node->color_pipeline,
					    rects, 4);
        }
    }

  corners_are_transparent = mode == ST_PAINT_BORDERS_MODE_COLOR &&
                            node->background_color.alpha == 0 &&
                            node->border_color[0].alpha == 0;

  /* corners */
  if (max_border_radius > 0 && paint_opacity > 0 && !corners_are_transparent)
    {
      for (corner_id = 0; corner_id < 4; corner_id++)
        {
          if (state->corner_material[corner_id] == NULL)
            continue;

          cogl_pipeline_set_color4ub (state->corner_material[corner_id],
                                      paint_opacity, paint_opacity,
                                      paint_opacity, paint_opacity);

          switch (corner_id)
            {
              case ST_CORNER_TOPLEFT:
                cogl_framebuffer_draw_textured_rectangle (framebuffer,
                  state->corner_material[corner_id], 0, 0,
                  max_width_radius[ST_CORNER_TOPLEFT], max_width_radius[ST_CORNER_TOPLEFT],
                  0, 0, 0.5, 0.5);
                break;
              case ST_CORNER_TOPRIGHT:
                cogl_framebuffer_draw_textured_rectangle (framebuffer,
                  state->corner_material[corner_id],
                  width - max_width_radius[ST_CORNER_TOPRIGHT], 0,
                  width, max_width_radius[ST_CORNER_TOPRIGHT],
                  0.5, 0, 1, 0.5);
                break;
              case ST_CORNER_BOTTOMRIGHT:
                cogl_framebuffer_draw_textured_rectangle (framebuffer,
                  state->corner_material[corner_id],
                  width - max_width_radius[ST_CORNER_BOTTOMRIGHT],
                  height - max_width_radius[ST_CORNER_BOTTOMRIGHT],
                  width, height,
                  0.5, 0.5, 1, 1);
                break;
              case ST_CORNER_BOTTOMLEFT:
                cogl_framebuffer_draw_textured_rectangle (framebuffer,
                  state->corner_material[corner_id],
                  0, height - max_width_radius[ST_CORNER_BOTTOMLEFT],
                  max_width_radius[ST_CORNER_BOTTOMLEFT], height,
                  0, 0.5, 0.5, 1);
                break;
              default:
                g_assert_not_reached();
                break;
            }
        }
    }

  /* background color */
  alpha = mode == ST_PAINT_BORDERS_MODE_SILHOUETTE ?
          255 :
          paint_opacity * node->background_color.alpha / 255;
  if (alpha > 0)
    {
      st_theme_node_ensure_color_pipeline (node);
      cogl_pipeline_set_color4ub (node->color_pipeline,
                                  node->background_color.red * alpha / 255,
                                  node->background_color.green * alpha / 255,
                                  node->background_color.blue * alpha / 255,
                                  alpha);

      /* We add padding to each corner, so that all corners end up as if they
       * had a border-radius of max_border_radius, which allows us to treat
       * corners as uniform further on.
       */
      for (corner_id = 0; corner_id < 4; corner_id++)
        {
          float verts[8];
          int n_rects;

          /* corner texture does not need padding */
          if (max_border_radius == border_radius[corner_id])
            continue;

          n_rects = border_radius[corner_id] == 0 ? 1 : 2;

          switch (corner_id)
            {
              case ST_CORNER_TOPLEFT:
                verts[0] = border_width[ST_SIDE_LEFT];
                verts[1] = MAX(border_radius[corner_id],
                               border_width[ST_SIDE_TOP]);
                verts[2] = max_border_radius;
                verts[3] = max_border_radius;
                if (n_rects == 2)
                  {
                    verts[4] = MAX(border_radius[corner_id],
                                   border_width[ST_SIDE_LEFT]);
                    verts[5] = border_width[ST_SIDE_TOP];
                    verts[6] = max_border_radius;
                    verts[7] = MAX(border_radius[corner_id],
                                   border_width[ST_SIDE_TOP]);
                  }
                break;
              case ST_CORNER_TOPRIGHT:
                verts[0] = width - max_border_radius;
                verts[1] = MAX(border_radius[corner_id],
                               border_width[ST_SIDE_TOP]);
                verts[2] = width - border_width[ST_SIDE_RIGHT];
                verts[3] = max_border_radius;
                if (n_rects == 2)
                  {
                    verts[4] = width - max_border_radius;
                    verts[5] = border_width[ST_SIDE_TOP];
                    verts[6] = width - MAX(border_radius[corner_id],
                                           border_width[ST_SIDE_RIGHT]);
                    verts[7] = MAX(border_radius[corner_id],
                                   border_width[ST_SIDE_TOP]);
                  }
                break;
              case ST_CORNER_BOTTOMRIGHT:
                verts[0] = width - max_border_radius;
                verts[1] = height - max_border_radius;
                verts[2] = width - border_width[ST_SIDE_RIGHT];
                verts[3] = height - MAX(border_radius[corner_id],
                                        border_width[ST_SIDE_BOTTOM]);
                if (n_rects == 2)
                  {
                    verts[4] = width - max_border_radius;
                    verts[5] = height - MAX(border_radius[corner_id],
                                            border_width[ST_SIDE_BOTTOM]);
                    verts[6] = width - MAX(border_radius[corner_id],
                                           border_width[ST_SIDE_RIGHT]);
                    verts[7] = height - border_width[ST_SIDE_BOTTOM];
                  }
                break;
              case ST_CORNER_BOTTOMLEFT:
                verts[0] = border_width[ST_SIDE_LEFT];
                verts[1] = height - max_border_radius;
                verts[2] = max_border_radius;
                verts[3] = height - MAX(border_radius[corner_id],
                                        border_width[ST_SIDE_BOTTOM]);
                if (n_rects == 2)
                  {
                    verts[4] = MAX(border_radius[corner_id],
                                   border_width[ST_SIDE_LEFT]);
                    verts[5] = height - MAX(border_radius[corner_id],
                                            border_width[ST_SIDE_BOTTOM]);
                    verts[6] = max_border_radius;
                    verts[7] = height - border_width[ST_SIDE_BOTTOM];
                  }
                break;
              default:
                g_assert_not_reached();
                break;
            }
          cogl_framebuffer_draw_rectangles (framebuffer,
                                            node->color_pipeline,
                                            verts, n_rects);
        }

      /* Once we've drawn the borders and corners, if the corners are bigger
       * then the border width, the remaining area is shaped like
       *
       *  ########
       * ##########
       * ##########
       *  ########
       *
       * We draw it in at most 3 pieces - first the top and bottom if
       * necessary, then the main rectangle
       */
      if (max_border_radius > border_width[ST_SIDE_TOP])
        cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                         MAX(max_border_radius, border_width[ST_SIDE_LEFT]),
                                         border_width[ST_SIDE_TOP],
                                         width - MAX(max_border_radius, border_width[ST_SIDE_RIGHT]),
                                         max_border_radius);
      if (max_border_radius > border_width[ST_SIDE_BOTTOM])
        cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                         MAX(max_border_radius, border_width[ST_SIDE_LEFT]),
                                         height - max_border_radius,
                                         width - MAX(max_border_radius, border_width[ST_SIDE_RIGHT]),
                                         height - border_width[ST_SIDE_BOTTOM]);

      cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                       border_width[ST_SIDE_LEFT],
                                       MAX(border_width[ST_SIDE_TOP], max_border_radius),
                                       width - border_width[ST_SIDE_RIGHT],
                                       height - MAX(border_width[ST_SIDE_BOTTOM], max_border_radius));
    }
}

static void
st_theme_node_paint_sliced_shadow (StThemeNodePaintState *state,
                                   CoglFramebuffer       *framebuffer,
                                   const ClutterActorBox *box,
                                   guint8                 paint_opacity)
{
  StThemeNode *node = state->node;
  guint border_radius[4];
  CoglColor color;
  StShadow *box_shadow_spec;
  gfloat xoffset, yoffset;
  gfloat width, height;
  gfloat shadow_width, shadow_height;
  gfloat xend, yend, top, bottom, left, right;
  gfloat s_top, s_bottom, s_left, s_right;
  gfloat shadow_blur_radius, x_spread_factor, y_spread_factor;
  float rectangles[8 * 9];
  gint idx;
  ClutterColor background_color;
  static const ClutterColor invisible_occluded = {3, 2, 1, 0};

  if (paint_opacity == 0)
    return;

  st_theme_node_reduce_border_radius (node, box->x2 - box->x1, box->y2 - box->y1, border_radius);

  box_shadow_spec = st_theme_node_get_box_shadow (node);

  /* Compute input & output areas :
   *
   *        yoffset ----------------------------
   *                |      |            |      |
   *                |      |            |      |
   *                |      |            |      |
   *           top  ----------------------------
   *                |      |            |      |
   *                |      |            |      |
   *                |      |            |      |
   *         bottom ----------------------------
   *                |      |            |      |
   *                |      |            |      |
   *                |      |            |      |
   *           yend ----------------------------
   *            xoffset   left        right    xend
   *
   *      s_top    = top in offscreen's coordinates (0.0 - 1.0)
   *      s_bottom = bottom in offscreen's coordinates (0.0 - 1.0)
   *      s_left   = left in offscreen's coordinates (0.0 - 1.0)
   *      s_right  = right in offscreen's coordinates (0.0 - 1.0)
   */
  if (box_shadow_spec->blur == 0)
    shadow_blur_radius = 0;
  else
    shadow_blur_radius = ceilf (1.5 * box_shadow_spec->blur / 2.0) * 2.0;

  shadow_width = state->box_shadow_width + 2 * shadow_blur_radius;
  shadow_height = state->box_shadow_height + 2 * shadow_blur_radius;

  /* Compute input regions parameters */
  s_top = shadow_blur_radius + box_shadow_spec->blur +
    MAX (node->border_radius[ST_CORNER_TOPLEFT],
         node->border_radius[ST_CORNER_TOPRIGHT]);
  s_bottom = shadow_blur_radius + box_shadow_spec->blur +
    MAX (node->border_radius[ST_CORNER_BOTTOMLEFT],
         node->border_radius[ST_CORNER_BOTTOMRIGHT]);
  s_left = shadow_blur_radius + box_shadow_spec->blur +
    MAX (node->border_radius[ST_CORNER_TOPLEFT],
         node->border_radius[ST_CORNER_BOTTOMLEFT]);
  s_right = shadow_blur_radius + box_shadow_spec->blur +
    MAX (node->border_radius[ST_CORNER_TOPRIGHT],
         node->border_radius[ST_CORNER_BOTTOMRIGHT]);

  /* Compute output regions parameters */
  xoffset = box->x1 + box_shadow_spec->xoffset - shadow_blur_radius - box_shadow_spec->spread;
  yoffset = box->y1 + box_shadow_spec->yoffset - shadow_blur_radius - box_shadow_spec->spread;
  width = box->x2 - box->x1 + 2 * shadow_blur_radius;
  height = box->y2 - box->y1 + 2 * shadow_blur_radius;

  x_spread_factor = (width + 2 * box_shadow_spec->spread) / width;
  y_spread_factor = (height + 2 * box_shadow_spec->spread) / height;

  width += 2 * box_shadow_spec->spread;
  height += 2 * box_shadow_spec->spread;

  xend = xoffset + width;
  yend = yoffset + height;

  top = s_top * y_spread_factor;
  bottom = s_bottom * y_spread_factor;
  left = s_left * x_spread_factor;
  right = s_right * x_spread_factor;

  bottom = height - bottom;
  right  = width - right;

  /* Final adjustments */
  s_top /= shadow_height;
  s_bottom /= shadow_height;
  s_left /= shadow_width;
  s_right /= shadow_width;

  s_bottom = 1.0 - s_bottom;
  s_right  = 1.0 - s_right;

  top += yoffset;
  bottom += yoffset;
  left += xoffset;
  right += xoffset;

  /* Setup pipeline */
  cogl_color_init_from_4ub (&color,
                            box_shadow_spec->color.red   * paint_opacity / 255,
                            box_shadow_spec->color.green * paint_opacity / 255,
                            box_shadow_spec->color.blue  * paint_opacity / 255,
                            box_shadow_spec->color.alpha * paint_opacity / 255);
  cogl_color_premultiply (&color);

  cogl_pipeline_set_layer_combine_constant (state->box_shadow_pipeline, 0, &color);

  idx = 0;

  if (yoffset < top)
    {
      if (xoffset < left)
        {
          /* Top left corner */
          rectangles[idx++] = xoffset;
          rectangles[idx++] = yoffset;
          rectangles[idx++] = left;
          rectangles[idx++] = top;

          rectangles[idx++] = 0;
          rectangles[idx++] = 0;
          rectangles[idx++] = s_left;
          rectangles[idx++] = s_top;
        }

      /* Top middle */
      rectangles[idx++] = left;
      rectangles[idx++] = yoffset;
      rectangles[idx++] = right;
      rectangles[idx++] = top;

      rectangles[idx++] = s_left;
      rectangles[idx++] = 0;
      rectangles[idx++] = s_right;
      rectangles[idx++] = s_top;

      if (xend > right)
        {
          /* Top right corner */
          rectangles[idx++] = right;
          rectangles[idx++] = yoffset;
          rectangles[idx++] = xend;
          rectangles[idx++] = top;

          rectangles[idx++] = s_right;
          rectangles[idx++] = 0;
          rectangles[idx++] = 1;
          rectangles[idx++] = s_top;
        }
    }

  if (xoffset < left)
    {
      /* Left middle */
      rectangles[idx++] = xoffset;
      rectangles[idx++] = top;
      rectangles[idx++] = left;
      rectangles[idx++] = bottom;

      rectangles[idx++] = 0;
      rectangles[idx++] = s_top;
      rectangles[idx++] = s_left;
      rectangles[idx++] = s_bottom;
    }

  /* Center middle is not definitely occluded? */
  st_theme_node_get_background_color (node, &background_color);
  if (!clutter_color_equal (&background_color, &invisible_occluded) ||
      paint_opacity < 255 ||
      xoffset > shadow_blur_radius || left < 0 ||
      yoffset > shadow_blur_radius || top < 0)
    {
      rectangles[idx++] = left;
      rectangles[idx++] = top;
      rectangles[idx++] = right;
      rectangles[idx++] = bottom;

      rectangles[idx++] = s_left;
      rectangles[idx++] = s_top;
      rectangles[idx++] = s_right;
      rectangles[idx++] = s_bottom;
    }

  if (xend > right)
    {
      /* Right middle */
      rectangles[idx++] = right;
      rectangles[idx++] = top;
      rectangles[idx++] = xend;
      rectangles[idx++] = bottom;

      rectangles[idx++] = s_right;
      rectangles[idx++] = s_top;
      rectangles[idx++] = 1;
      rectangles[idx++] = s_bottom;
    }

  if (yend > bottom)
    {
      if (xoffset < left)
        {
          /* Bottom left corner */
          rectangles[idx++] = xoffset;
          rectangles[idx++] = bottom;
          rectangles[idx++] = left;
          rectangles[idx++] = yend;

          rectangles[idx++] = 0;
          rectangles[idx++] = s_bottom;
          rectangles[idx++] = s_left;
          rectangles[idx++] = 1;
        }

      /* Bottom middle */
      rectangles[idx++] = left;
      rectangles[idx++] = bottom;
      rectangles[idx++] = right;
      rectangles[idx++] = yend;

      rectangles[idx++] = s_left;
      rectangles[idx++] = s_bottom;
      rectangles[idx++] = s_right;
      rectangles[idx++] = 1;

      if (xend > right)
        {
          /* Bottom right corner */
          rectangles[idx++] = right;
          rectangles[idx++] = bottom;
          rectangles[idx++] = xend;
          rectangles[idx++] = yend;

          rectangles[idx++] = s_right;
          rectangles[idx++] = s_bottom;
          rectangles[idx++] = 1;
          rectangles[idx++] = 1;
        }
    }

  cogl_framebuffer_draw_textured_rectangles (framebuffer, state->box_shadow_pipeline,
                                             rectangles, idx / 8);

#if 0
  /* Visual feedback on shadow's 9-slice and original offscreen buffer,
     for debug purposes */
  cogl_framebuffer_draw_rectangle (framebuffer, state->box_shadow_pipeline,
                                   xend, yoffset, xend + shadow_width, yoffset + shadow_height);

  st_theme_node_ensure_color_pipeline (node);
  cogl_pipeline_set_color4ub (node->color_pipeline, 0xff, 0x0, 0x0, 0xff);

  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   xoffset, top, xend, top + 1);
  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   xoffset, bottom, xend, bottom + 1);
  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   left, yoffset, left + 1, yend);
  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   right, yoffset, right + 1, yend);

  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   xend, yoffset, xend + shadow_width, yoffset + 1);
  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   xend, yoffset + shadow_height, xend + shadow_width, yoffset + shadow_height + 1);
  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   xend, yoffset, xend + 1, yoffset + shadow_height);
  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   xend + shadow_width, yoffset, xend + shadow_width + 1, yoffset + shadow_height);

  s_top *= shadow_height;
  s_bottom *= shadow_height;
  s_left *= shadow_width;
  s_right *= shadow_width;

  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   xend, yoffset + s_top, xend + shadow_width, yoffset + s_top + 1);
  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   xend, yoffset + s_bottom, xend + shadow_width, yoffset + s_bottom + 1);
  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   xend + s_left, yoffset, xend + s_left + 1, yoffset + shadow_height);
  cogl_framebuffer_draw_rectangle (framebuffer, node->color_pipeline,
                                   xend + s_right, yoffset, xend + s_right + 1, yoffset + shadow_height);

#endif
}

static void
st_theme_node_prerender_shadow (StThemeNodePaintState *state)
{
  StThemeNode *node = state->node;
  CoglContext *ctx;
  int fb_width, fb_height;
  CoglTexture *buffer;
  CoglOffscreen *offscreen = NULL;
  CoglFramebuffer *framebuffer;
  GError *error = NULL;

  ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());

  /* Render offscreen */
  fb_width = ceilf (state->box_shadow_width * state->resource_scale);
  fb_height = ceilf (state->box_shadow_height * state->resource_scale);
  buffer = COGL_TEXTURE (cogl_texture_2d_new_with_size (ctx, fb_width, fb_height));
  if (buffer == NULL)
    return;

  offscreen = cogl_offscreen_new_with_texture (buffer);
  framebuffer = COGL_FRAMEBUFFER (offscreen);

  if (cogl_framebuffer_allocate (framebuffer, &error))
    {
      ClutterActorBox box = { 0, 0, state->box_shadow_width, state->box_shadow_height};

      cogl_framebuffer_orthographic (framebuffer, 0, 0,
                                     fb_width, fb_height, 0, 1.0);
      cogl_framebuffer_scale (framebuffer,
                              state->resource_scale,
                              state->resource_scale, 1);
      cogl_framebuffer_clear4f (framebuffer, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 0);

      st_theme_node_paint_borders (state, framebuffer, &box, ST_PAINT_BORDERS_MODE_SILHOUETTE, 0xFF);

      state->box_shadow_pipeline = _st_create_shadow_pipeline (st_theme_node_get_box_shadow (node),
                                                               buffer, state->resource_scale);
    }

  g_clear_error (&error);
  g_clear_object (&offscreen);
  cogl_clear_object (&buffer);
}

static void
st_theme_node_compute_maximum_borders (StThemeNodePaintState *state)
{
  int max_borders[4], center_radius;
  StThemeNode * node = state->node;

  /* Compute maximum borders sizes */
  max_borders[ST_SIDE_TOP] = MAX (node->border_radius[ST_CORNER_TOPLEFT],
                                  node->border_radius[ST_CORNER_TOPRIGHT]);
  max_borders[ST_SIDE_BOTTOM] = MAX (node->border_radius[ST_CORNER_BOTTOMLEFT],
                                     node->border_radius[ST_CORNER_BOTTOMRIGHT]);
  max_borders[ST_SIDE_LEFT] = MAX (node->border_radius[ST_CORNER_TOPLEFT],
                                   node->border_radius[ST_CORNER_BOTTOMLEFT]);
  max_borders[ST_SIDE_RIGHT] = MAX (node->border_radius[ST_CORNER_TOPRIGHT],
                                    node->border_radius[ST_CORNER_BOTTOMRIGHT]);

  center_radius = (node->box_shadow->blur > 0) ? (2 * node->box_shadow->blur + 1) : 1;

  node->box_shadow_min_width = max_borders[ST_SIDE_LEFT] + max_borders[ST_SIDE_RIGHT] + center_radius;
  node->box_shadow_min_height = max_borders[ST_SIDE_TOP] + max_borders[ST_SIDE_BOTTOM] + center_radius;
  if (state->alloc_width < node->box_shadow_min_width ||
      state->alloc_height < node->box_shadow_min_height)
    {
      state->box_shadow_width = state->alloc_width;
      state->box_shadow_height = state->alloc_height;
    }
  else
    {
      state->box_shadow_width = node->box_shadow_min_width;
      state->box_shadow_height = node->box_shadow_min_height;
    }
}

static void
st_theme_node_paint_sliced_border_image (StThemeNode           *node,
                                         CoglFramebuffer       *framebuffer,
                                         float                  width,
                                         float                  height,
                                         guint8                 paint_opacity)
{
  gfloat ex, ey;
  gfloat tx1, ty1, tx2, ty2;
  gint border_left, border_right, border_top, border_bottom;
  float img_width, img_height;
  StBorderImage *border_image;
  CoglPipeline *pipeline;

  border_image = st_theme_node_get_border_image (node);
  g_assert (border_image != NULL);

  st_border_image_get_borders (border_image,
                               &border_left, &border_right, &border_top, &border_bottom);

  img_width = cogl_texture_get_width (node->border_slices_texture);
  img_height = cogl_texture_get_height (node->border_slices_texture);

  tx1 = border_left / img_width;
  tx2 = (img_width - border_right) / img_width;
  ty1 = border_top / img_height;
  ty2 = (img_height - border_bottom) / img_height;

  ex = width - border_right;
  if (ex < 0)
    ex = border_right;           /* FIXME ? */

  ey = height - border_bottom;
  if (ey < 0)
    ey = border_bottom;          /* FIXME ? */

  pipeline = node->border_slices_pipeline;
  cogl_pipeline_set_color4ub (pipeline,
                              paint_opacity, paint_opacity, paint_opacity, paint_opacity);

  {
    float rectangles[] =
    {
      /* top left corner */
      0, 0, border_left, border_top,
      0.0, 0.0,
      tx1, ty1,

      /* top middle */
      border_left, 0, ex, border_top,
      tx1, 0.0,
      tx2, ty1,

      /* top right */
      ex, 0, width, border_top,
      tx2, 0.0,
      1.0, ty1,

      /* mid left */
      0, border_top, border_left, ey,
      0.0, ty1,
      tx1, ty2,

      /* center */
      border_left, border_top, ex, ey,
      tx1, ty1,
      tx2, ty2,

      /* mid right */
      ex, border_top, width, ey,
      tx2, ty1,
      1.0, ty2,

      /* bottom left */
      0, ey, border_left, height,
      0.0, ty2,
      tx1, 1.0,

      /* bottom center */
      border_left, ey, ex, height,
      tx1, ty2,
      tx2, 1.0,

      /* bottom right */
      ex, ey, width, height,
      tx2, ty2,
      1.0, 1.0
    };

    cogl_framebuffer_draw_textured_rectangles (framebuffer, pipeline, rectangles, 9);
  }
}

static void
st_theme_node_paint_outline (StThemeNode           *node,
                             CoglFramebuffer       *framebuffer,
                             const ClutterActorBox *box,
                             guint8                 paint_opacity)

{
  float width, height;
  int outline_width;
  float rects[16];
  ClutterColor outline_color, effective_outline;
  guint8 alpha;

  width = box->x2 - box->x1;
  height = box->y2 - box->y1;

  outline_width = st_theme_node_get_outline_width (node);
  if (outline_width == 0)
    return;

  st_theme_node_get_outline_color (node, &outline_color);
  over (&outline_color, &node->background_color, &effective_outline);

  alpha = paint_opacity * outline_color.alpha / 255;

  st_theme_node_ensure_color_pipeline (node);
  cogl_pipeline_set_color4ub (node->color_pipeline,
                              effective_outline.red * alpha / 255,
                              effective_outline.green * alpha / 255,
                              effective_outline.blue * alpha / 255,
                              alpha);

  /* The outline is drawn just outside the border, which means just
   * outside the allocation box. This means that in some situations
   * involving clip_to_allocation or the screen edges, you won't be
   * able to see the outline. In practice, it works well enough.
   */

  /* NORTH */
  rects[0] = -outline_width;
  rects[1] = -outline_width;
  rects[2] = width + outline_width;
  rects[3] = 0;

  /* EAST */
  rects[4] = width;
  rects[5] = 0;
  rects[6] = width + outline_width;
  rects[7] = height;

  /* SOUTH */
  rects[8] = -outline_width;
  rects[9] = height;
  rects[10] = width + outline_width;
  rects[11] = height + outline_width;

  /* WEST */
  rects[12] = -outline_width;
  rects[13] = 0;
  rects[14] = 0;
  rects[15] = height;

  cogl_framebuffer_draw_rectangles (framebuffer, node->color_pipeline, rects, 4);
}

static gboolean
st_theme_node_needs_new_box_shadow_for_size (StThemeNodePaintState *state,
                                             StThemeNode           *node,
                                             float                  width,
                                             float                  height,
                                             float                  resource_scale)
{
  if (!node->rendered_once)
    return TRUE;

  /* The resource scale changed, so need to recompute a new box-shadow */
  if (fabsf (state->resource_scale - resource_scale) > FLT_EPSILON)
    return TRUE;

  /* The allocation hasn't changed, no need to recompute a new
     box-shadow. */
  if (state->alloc_width == width &&
      state->alloc_height == height)
    return FALSE;

  /* If there is no shadow, no need to recompute a new box-shadow. */
  if (node->box_shadow_min_width == 0 ||
      node->box_shadow_min_height == 0)
    return FALSE;

  /* If the new size is inferior to the box-shadow minimum size (we
     already know the size has changed), we need to recompute the
     box-shadow. */
  if (width < node->box_shadow_min_width ||
      height < node->box_shadow_min_height)
    return TRUE;

  /* Now checking whether the size of the node has crossed the minimum
     box-shadow size boundary, from below to above the minimum size .
     If that's the case, we need to recompute the box-shadow */
  if (state->alloc_width < node->box_shadow_min_width ||
      state->alloc_height < node->box_shadow_min_height)
    return TRUE;

  return FALSE;
}

void
st_theme_node_paint (StThemeNode           *node,
                     StThemeNodePaintState *state,
                     CoglFramebuffer       *framebuffer,
                     const ClutterActorBox *box,
                     guint8                 paint_opacity,
                     float                  resource_scale)
{
  float width, height;
  ClutterActorBox allocation;

  /* Some things take an ActorBox, some things just width/height */
  width = box->x2 - box->x1;
  height = box->y2 - box->y1;
  allocation.x1 = allocation.y1 = 0;
  allocation.x2 = width;
  allocation.y2 = height;

  if (width <= 0 || height <= 0 || resource_scale <= 0.0f)
    return;

  /* Check whether we need to recreate the textures of the paint
   * state, either because :
   *  1) the theme node associated to the paint state has changed
   *  2) the allocation size change requires recreating textures
   */
  if (state->node != node ||
      st_theme_node_needs_new_box_shadow_for_size (state, node, width, height,
                                                   resource_scale))
    {
      /* If we had the ability to cache textures on the node, then we
         can just copy them over to the paint state and avoid all
         rendering. We end up sharing textures a cross different
         widgets. */
      if (node->rendered_once && node->cached_textures &&
          width >= node->box_shadow_min_width && height >= node->box_shadow_min_height &&
          fabsf (resource_scale - state->resource_scale) < FLT_EPSILON)
        st_theme_node_paint_state_copy (state, &node->cached_state);
      else
        st_theme_node_render_resources (state, node, width, height, resource_scale);

      node->rendered_once = TRUE;
    }
  else if (state->alloc_width != width || state->alloc_height != height ||
           fabsf (state->resource_scale - resource_scale) > FLT_EPSILON)
    st_theme_node_update_resources (state, node, width, height, resource_scale);

  /* Rough notes about the relationship of borders and backgrounds in CSS3;
   * see http://www.w3.org/TR/css3-background/ for more accurate details.
   *
   * - Things are drawn in 4 layers, from the bottom:
   *     Background color
   *     Background image
   *     Border color or border image
   *     Content
   * - The background color, gradient and image extend to and are clipped by
   *   the edge of the border area, so will be rounded if the border is
   *   rounded. (CSS3 background-clip property modifies this)
   * - The border image replaces what would normally be drawn by the border
   * - The border image is not clipped by a rounded border-radius
   * - The border radius rounds the background even if the border is
   *   zero width or a border image is being used.
   *
   * Deviations from the above as implemented here:
   *  - The combination of border image and a non-zero border radius is
   *    not supported; the background color will be drawn with square
   *    corners.
   *  - The background image is drawn above the border color, not below it.
   *  - We clip the background image to the inside edges of the border
   *    instead of the outside edges of the border (but position the image
   *    such that it's aligned to the outside edges)
   */

  if (state->box_shadow_pipeline)
    {
      if (state->alloc_width < node->box_shadow_min_width ||
          state->alloc_height < node->box_shadow_min_height)
        _st_paint_shadow_with_opacity (node->box_shadow,
                                       framebuffer,
                                       state->box_shadow_pipeline,
                                       &allocation,
                                       paint_opacity);
      else
        st_theme_node_paint_sliced_shadow (state,
                                           framebuffer,
                                           &allocation,
                                           paint_opacity);
    }

  if (state->prerendered_pipeline != NULL ||
      st_theme_node_load_border_image (node, resource_scale))
    {
      if (state->prerendered_pipeline != NULL)
        {
          ClutterActorBox paint_box;

          st_theme_node_get_background_paint_box (node,
                                                  &allocation,
                                                  &paint_box);

          paint_material_with_opacity (state->prerendered_pipeline,
                                       framebuffer,
                                       &paint_box,
                                       NULL,
                                       paint_opacity);
        }

      if (node->border_slices_pipeline != NULL)
        st_theme_node_paint_sliced_border_image (node, framebuffer, width, height, paint_opacity);
    }
  else
    {
      st_theme_node_paint_borders (state, framebuffer, box, ST_PAINT_BORDERS_MODE_COLOR, paint_opacity);
    }

  st_theme_node_paint_outline (node, framebuffer, box, paint_opacity);

  if (state->prerendered_pipeline == NULL &&
      st_theme_node_load_background_image (node, resource_scale))
    {
      ClutterActorBox background_box;
      ClutterActorBox texture_coords;
      gboolean has_visible_outline;

      /* If the node doesn't have an opaque or repeating background or
       * a border then we let its background image shadows leak out,
       * but otherwise we clip it.
       */
      has_visible_outline = st_theme_node_has_visible_outline (node);

      get_background_position (node, &allocation, resource_scale,
                               &background_box, &texture_coords);

      if (has_visible_outline || node->background_repeat)
        cogl_framebuffer_push_rectangle_clip (framebuffer,
                                              allocation.x1, allocation.y1,
                                              allocation.x2, allocation.y2);

      /* CSS based drop shadows
       *
       * Drop shadows in ST are modelled after the CSS3 box-shadow property;
       * see http://www.css3.info/preview/box-shadow/ for a detailed description.
       *
       * While the syntax of the property is mostly identical - we do not support
       * multiple shadows and allow for a more liberal placement of the color
       * parameter - its interpretation defers significantly in that the shadow's
       * shape is not determined by the bounding box, but by the CSS background
       * image. The drop shadows are allowed to escape the nodes allocation if
       * there is nothing (like a border, or the edge of the background color)
       * to logically confine it.
       */
      if (node->background_shadow_pipeline != NULL)
        _st_paint_shadow_with_opacity (node->background_image_shadow,
                                       framebuffer,
                                       node->background_shadow_pipeline,
                                       &background_box,
                                       paint_opacity);

      paint_material_with_opacity (node->background_pipeline,
                                   framebuffer,
                                   &background_box,
                                   &texture_coords,
                                   paint_opacity);

      if (has_visible_outline || node->background_repeat)
        cogl_framebuffer_pop_clip (framebuffer);
    }
}

static void
st_theme_node_paint_state_node_free_internal (StThemeNodePaintState *state,
                                              gboolean               unref_node)
{
  int corner_id;

  cogl_clear_object (&state->prerendered_texture);
  cogl_clear_object (&state->prerendered_pipeline);
  cogl_clear_object (&state->box_shadow_pipeline);

  for (corner_id = 0; corner_id < 4; corner_id++)
    cogl_clear_object (&state->corner_material[corner_id]);

  if (unref_node)
    st_theme_node_paint_state_set_node (state, NULL);

  st_theme_node_paint_state_init (state);
}

static void
st_theme_node_paint_state_node_freed (StThemeNodePaintState *state)
{
  st_theme_node_paint_state_node_free_internal (state, FALSE);
}

void
st_theme_node_paint_state_set_node (StThemeNodePaintState *state, StThemeNode *node)
{
  if (state->node)
    g_object_weak_unref (G_OBJECT (state->node),
                         (GWeakNotify) st_theme_node_paint_state_node_freed,
                         state);

  state->node = node;
  if (state->node)
    g_object_weak_ref (G_OBJECT (state->node),
                       (GWeakNotify) st_theme_node_paint_state_node_freed,
                       state);
}

void
st_theme_node_paint_state_free (StThemeNodePaintState *state)
{
  st_theme_node_paint_state_node_free_internal (state, TRUE);
}

void
st_theme_node_paint_state_init (StThemeNodePaintState *state)
{
  int corner_id;

  state->alloc_width = 0;
  state->alloc_height = 0;
  state->resource_scale = -1;
  state->node = NULL;
  state->box_shadow_pipeline = NULL;
  state->prerendered_texture = NULL;
  state->prerendered_pipeline = NULL;

  for (corner_id = 0; corner_id < 4; corner_id++)
    state->corner_material[corner_id] = NULL;
}

void
st_theme_node_paint_state_copy (StThemeNodePaintState *state,
                                StThemeNodePaintState *other)
{
  int corner_id;

  if (state == other)
    return;

  st_theme_node_paint_state_free (state);

  st_theme_node_paint_state_set_node (state, other->node);

  state->alloc_width = other->alloc_width;
  state->alloc_height = other->alloc_height;
  state->resource_scale = other->resource_scale;
  state->box_shadow_width = other->box_shadow_width;
  state->box_shadow_height = other->box_shadow_height;

  if (other->box_shadow_pipeline)
    state->box_shadow_pipeline = cogl_object_ref (other->box_shadow_pipeline);
  if (other->prerendered_texture)
    state->prerendered_texture = cogl_object_ref (other->prerendered_texture);
  if (other->prerendered_pipeline)
    state->prerendered_pipeline = cogl_object_ref (other->prerendered_pipeline);
  for (corner_id = 0; corner_id < 4; corner_id++)
    if (other->corner_material[corner_id])
      state->corner_material[corner_id] = cogl_object_ref (other->corner_material[corner_id]);
}

void
st_theme_node_paint_state_invalidate (StThemeNodePaintState *state)
{
  state->alloc_width = 0;
  state->alloc_height = 0;
  state->resource_scale = -1.0f;
}

gboolean
st_theme_node_paint_state_invalidate_for_file (StThemeNodePaintState *state,
                                               GFile                 *file)
{
  if (state->node != NULL &&
      st_theme_node_invalidate_resources_for_file (state->node, file))
    {
      st_theme_node_paint_state_invalidate (state);
      return TRUE;
    }

  return FALSE;
}
