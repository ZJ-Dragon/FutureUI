/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-private.h: Private declarations and functions
 *
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2010 Florian Müllner
 * Copyright 2010 Intel Corporation
 * Copyright 2010 Giovanni Campagna
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
#include <math.h>
#include <string.h>

#include "st-private.h"

/**
 * _st_actor_get_preferred_width:
 * @actor: a #ClutterActor
 * @for_height: as with clutter_actor_get_preferred_width()
 * @y_fill: %TRUE if @actor will fill its allocation vertically
 * @min_width_p: as with clutter_actor_get_preferred_width()
 * @natural_width_p: as with clutter_actor_get_preferred_width()
 *
 * Like clutter_actor_get_preferred_width(), but if @y_fill is %FALSE,
 * then it will compute a width request based on the assumption that
 * @actor will be given an allocation no taller than its natural
 * height.
 */
void
_st_actor_get_preferred_width  (ClutterActor *actor,
                                gfloat        for_height,
                                gboolean      y_fill,
                                gfloat       *min_width_p,
                                gfloat       *natural_width_p)
{
  if (!y_fill && for_height != -1)
    {
      ClutterRequestMode mode;
      gfloat natural_height;

      mode = clutter_actor_get_request_mode (actor);
      if (mode == CLUTTER_REQUEST_WIDTH_FOR_HEIGHT)
        {
          clutter_actor_get_preferred_height (actor, -1, NULL, &natural_height);
          if (for_height > natural_height)
            for_height = natural_height;
        }
    }

  clutter_actor_get_preferred_width (actor, for_height, min_width_p, natural_width_p);
}

/**
 * _st_actor_get_preferred_height:
 * @actor: a #ClutterActor
 * @for_width: as with clutter_actor_get_preferred_height()
 * @x_fill: %TRUE if @actor will fill its allocation horizontally
 * @min_height_p: as with clutter_actor_get_preferred_height()
 * @natural_height_p: as with clutter_actor_get_preferred_height()
 *
 * Like clutter_actor_get_preferred_height(), but if @x_fill is
 * %FALSE, then it will compute a height request based on the
 * assumption that @actor will be given an allocation no wider than
 * its natural width.
 */
void
_st_actor_get_preferred_height (ClutterActor *actor,
                                gfloat        for_width,
                                gboolean      x_fill,
                                gfloat       *min_height_p,
                                gfloat       *natural_height_p)
{
  if (!x_fill && for_width != -1)
    {
      ClutterRequestMode mode;
      gfloat natural_width;

      mode = clutter_actor_get_request_mode (actor);
      if (mode == CLUTTER_REQUEST_HEIGHT_FOR_WIDTH)
        {
          clutter_actor_get_preferred_width (actor, -1, NULL, &natural_width);
          if (for_width > natural_width)
            for_width = natural_width;
        }
    }

  clutter_actor_get_preferred_height (actor, for_width, min_height_p, natural_height_p);
}

/**
 * _st_set_text_from_style:
 * @text: Target #ClutterText
 * @theme_node: Source #StThemeNode
 *
 * Set various GObject properties of the @text object using
 * CSS information from @theme_node.
 */
void
_st_set_text_from_style (ClutterText *text,
                         StThemeNode *theme_node)
{

  ClutterColor color;
  StTextDecoration decoration;
  PangoAttrList *attribs = NULL;
  const PangoFontDescription *font;
  PangoAttribute *foreground;
  StTextAlign align;
  gdouble spacing;
  gchar *font_features;

  font = st_theme_node_get_font (theme_node);
  clutter_text_set_font_description (text, (PangoFontDescription *) font);

  attribs = pango_attr_list_new ();

  st_theme_node_get_foreground_color (theme_node, &color);
  clutter_text_set_cursor_color (text, &color);
  foreground = pango_attr_foreground_new (color.red * 255,
                                          color.green * 255,
                                          color.blue * 255);
  pango_attr_list_insert (attribs, foreground);

  if (color.alpha != 255)
    {
      PangoAttribute *alpha;

      /* An alpha value of 0 means "system inherited", so the
       * minimum regular value is 1.
       */
      if (color.alpha == 0)
        alpha = pango_attr_foreground_alpha_new (1);
      else
        alpha = pango_attr_foreground_alpha_new (color.alpha * 255);

      pango_attr_list_insert (attribs, alpha);
    }

  decoration = st_theme_node_get_text_decoration (theme_node);
  if (decoration)
    {
      if (decoration & ST_TEXT_DECORATION_UNDERLINE)
        {
          PangoAttribute *underline = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
          pango_attr_list_insert (attribs, underline);
        }
      if (decoration & ST_TEXT_DECORATION_LINE_THROUGH)
        {
          PangoAttribute *strikethrough = pango_attr_strikethrough_new (TRUE);
          pango_attr_list_insert (attribs, strikethrough);
        }
      /* Pango doesn't have an equivalent attribute for _OVERLINE, and we deliberately
       * skip BLINK (for now...)
       */
    }

  spacing = st_theme_node_get_letter_spacing (theme_node);
  if (spacing)
    {
      PangoAttribute *letter_spacing = pango_attr_letter_spacing_new ((int)(.5 + spacing) * PANGO_SCALE);
      pango_attr_list_insert (attribs, letter_spacing);
    }

  font_features = st_theme_node_get_font_features (theme_node);
  if (font_features)
    {
      pango_attr_list_insert (attribs, pango_attr_font_features_new (font_features));
      g_free (font_features);
    }

  clutter_text_set_attributes (text, attribs);

  if (attribs)
    pango_attr_list_unref (attribs);

  align = st_theme_node_get_text_align (theme_node);
  if (align == ST_TEXT_ALIGN_JUSTIFY)
    {
      clutter_text_set_justify (text, TRUE);
      clutter_text_set_line_alignment (text, PANGO_ALIGN_LEFT);
    }
  else
    {
      clutter_text_set_justify (text, FALSE);
      clutter_text_set_line_alignment (text, (PangoAlignment) align);
    }
}

/**
 * _st_create_texture_pipeline:
 * @src_texture: The CoglTexture for the pipeline
 *
 * Creates a simple pipeline which contains the given texture as a
 * single layer.
 */
CoglPipeline *
_st_create_texture_pipeline (CoglTexture *src_texture)
{
  static CoglPipeline *texture_pipeline_template = NULL;
  CoglPipeline *pipeline;

  g_return_val_if_fail (src_texture != NULL, NULL);

  /* The only state used in the pipeline that would affect the shader
     generation is the texture type on the layer. Therefore we create
     a template pipeline which sets this state and all texture
     pipelines are created as a copy of this. That way Cogl can find
     the shader state for the pipeline more quickly by looking at the
     pipeline ancestry instead of resorting to the shader cache. */
  if (G_UNLIKELY (texture_pipeline_template == NULL))
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      texture_pipeline_template = cogl_pipeline_new (ctx);
      cogl_pipeline_set_layer_null_texture (texture_pipeline_template, 0);
    }

  pipeline = cogl_pipeline_copy (texture_pipeline_template);

  if (src_texture != NULL)
    cogl_pipeline_set_layer_texture (pipeline, 0, src_texture);

  return pipeline;
}

/*****
 * Shadows
 *****/

static gdouble *
calculate_gaussian_kernel (gdouble   sigma,
                           guint     n_values)
{
  gdouble *ret, sum;
  gdouble exp_divisor;
  int half, i;

  g_return_val_if_fail (sigma > 0, NULL);

  half = n_values / 2;

  ret = g_malloc (n_values * sizeof (gdouble));
  sum = 0.0;

  exp_divisor = 2 * sigma * sigma;

  /* n_values of 1D Gauss function */
  for (i = 0; i < (int)n_values; i++)
    {
      ret[i] = exp (-(i - half) * (i - half) / exp_divisor);
      sum += ret[i];
    }

  /* normalize */
  for (i = 0; i < (int)n_values; i++)
    ret[i] /= sum;

  return ret;
}

static guchar *
blur_pixels (guchar  *pixels_in,
             gint     width_in,
             gint     height_in,
             gint     rowstride_in,
             gdouble  blur,
             gint    *width_out,
             gint    *height_out,
             size_t  *rowstride_out)
{
  guchar *pixels_out;
  gdouble sigma;

  /* The CSS specification defines (or will define) the blur radius as twice
   * the Gaussian standard deviation. See:
   *
   * http://lists.w3.org/Archives/Public/www-style/2010Sep/0002.html
   */
  sigma = blur / 2.;

  if ((guint) blur == 0)
    {
      *width_out  = width_in;
      *height_out = height_in;
      *rowstride_out = rowstride_in;
      pixels_out = g_memdup2 (pixels_in, *rowstride_out * *height_out);
    }
  else
    {
      gdouble *kernel;
      guchar  *line;
      gint     n_values, half;
      gint     x_in, y_in, x_out, y_out, i;

      n_values = (gint) 5 * sigma;
      half = n_values / 2;

      *width_out  = width_in  + 2 * half;
      *height_out = height_in + 2 * half;
      *rowstride_out = (*width_out + 3) & ~3;

      pixels_out = g_malloc0 (*rowstride_out * *height_out);
      line       = g_malloc0 (*rowstride_out);

      kernel = calculate_gaussian_kernel (sigma, n_values);

      /* vertical blur */
      for (x_in = 0; x_in < width_in; x_in++)
        for (y_out = 0; y_out < *height_out; y_out++)
          {
            guchar *pixel_in, *pixel_out;
            gint i0, i1;

            y_in = y_out - half;

            /* We read from the source at 'y = y_in + i - half'; clamp the
             * full i range [0, n_values) so that y is in [0, height_in).
             */
            i0 = MAX (half - y_in, 0);
            i1 = MIN (height_in + half - y_in, n_values);

            pixel_in  =  pixels_in + (y_in + i0 - half) * rowstride_in + x_in;
            pixel_out =  pixels_out + y_out * *rowstride_out + (x_in + half);

            for (i = i0; i < i1; i++)
              {
                *pixel_out += *pixel_in * kernel[i];
                pixel_in += rowstride_in;
              }
          }

      /* horizontal blur */
      for (y_out = 0; y_out < *height_out; y_out++)
        {
          memcpy (line, pixels_out + y_out * *rowstride_out, *rowstride_out);

          for (x_out = 0; x_out < *width_out; x_out++)
            {
              gint i0, i1;
              guchar *pixel_out, *pixel_in;

              /* We read from the source at 'x = x_out + i - half'; clamp the
               * full i range [0, n_values) so that x is in [0, width_out).
               */
              i0 = MAX (half - x_out, 0);
              i1 = MIN (*width_out + half - x_out, n_values);

              pixel_in  = line + x_out + i0 - half;
              pixel_out = pixels_out + *rowstride_out * y_out + x_out;

              *pixel_out = 0;
              for (i = i0; i < i1; i++)
                {
                  *pixel_out += *pixel_in * kernel[i];
                  pixel_in++;
                }
            }
        }
      g_free (kernel);
      g_free (line);
    }

  return pixels_out;
}

CoglPipeline *
_st_create_shadow_pipeline (StShadow    *shadow_spec,
                            CoglTexture *src_texture,
                            float        resource_scale)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  CoglContext *ctx = clutter_backend_get_cogl_context (backend);
  g_autoptr (ClutterPaintNode) texture_node = NULL;
  g_autoptr (ClutterPaintNode) blur_node = NULL;
  g_autoptr (CoglOffscreen) offscreen = NULL;
  g_autoptr (GError) error = NULL;
  ClutterPaintContext *paint_context;
  CoglFramebuffer *fb;
  CoglPipeline *pipeline;
  CoglTexture *texture;
  float sampling_radius;
  float sigma;
  int src_height, dst_height;
  int src_width, dst_width;
  CoglPipeline *texture_pipeline;

  static CoglPipelineKey texture_pipeline_key =
    "st-create-shadow-pipeline-saturate-alpha";
  static CoglPipeline *shadow_pipeline_template = NULL;

  g_return_val_if_fail (shadow_spec != NULL, NULL);
  g_return_val_if_fail (src_texture != NULL, NULL);

  sampling_radius = resource_scale * shadow_spec->blur;
  sigma = sampling_radius / 2.f;
  sampling_radius = ceilf (sampling_radius);

  src_width = cogl_texture_get_width (src_texture);
  src_height = cogl_texture_get_height (src_texture);
  dst_width = src_width + 2 * sampling_radius;
  dst_height = src_height + 2 * sampling_radius;

  texture = cogl_texture_2d_new_with_size (ctx, dst_width, dst_height);
  if (!texture)
    return NULL;

  offscreen = cogl_offscreen_new_with_texture (texture);
  fb = COGL_FRAMEBUFFER (offscreen);
  if (!cogl_framebuffer_allocate (fb, &error))
    {
      cogl_clear_object (&texture);
      return NULL;
    }

  cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0.f, 0.f, 0.f, 0.f);
  cogl_framebuffer_orthographic (fb, 0, 0, dst_width, dst_height, 0, 1.0);

  /* Blur */
  blur_node = clutter_blur_node_new (dst_width, dst_height, sigma);
  clutter_paint_node_add_rectangle (blur_node,
                                    &(ClutterActorBox) {
                                      0.f, 0.f,
                                      dst_width, dst_height,
                                    });

  /* Texture */
  texture_pipeline = cogl_context_get_named_pipeline (ctx,
                                                      &texture_pipeline_key);

  if (G_UNLIKELY (texture_pipeline == NULL))
    {
      CoglSnippet *snippet;

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  "",
                                  "if (cogl_color_out.a > 0.0)\n"
                                  "  cogl_color_out.a = 1.0;");

      texture_pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_add_snippet (texture_pipeline, snippet);
      cogl_object_unref (snippet);

      cogl_context_set_named_pipeline (ctx,
                                       &texture_pipeline_key,
                                       texture_pipeline);
    }

  /* No need to unref texture_pipeline since the named pipeline hash
   * doesn't change its ref count from 1. Also no need to copy texture_pipeline
   * since we'll be completely finished with it after clutter_paint_node_paint.
   */

  cogl_pipeline_set_layer_texture (texture_pipeline, 0, src_texture);
  texture_node = clutter_pipeline_node_new (texture_pipeline);
  clutter_paint_node_add_child (blur_node, texture_node);
  clutter_paint_node_add_rectangle (texture_node,
                                    &(ClutterActorBox) {
                                      .x1 = sampling_radius,
                                      .y1 = sampling_radius,
                                      .x2 = src_width + sampling_radius,
                                      .y2 = src_height + sampling_radius,
                                    });

  paint_context =
    clutter_paint_context_new_for_framebuffer (fb, NULL, CLUTTER_PAINT_FLAG_NONE);
  clutter_paint_node_paint (blur_node, paint_context);
  clutter_paint_context_destroy (paint_context);

  if (G_UNLIKELY (shadow_pipeline_template == NULL))
    {
      shadow_pipeline_template = cogl_pipeline_new (ctx);

      /* We set up the pipeline to blend the shadow texture with the combine
       * constant, but defer setting the latter until painting, so that we can
       * take the actor's overall opacity into account. */
      cogl_pipeline_set_layer_combine (shadow_pipeline_template, 0,
                                       "RGBA = MODULATE (CONSTANT, TEXTURE[A])",
                                       NULL);
    }

  pipeline = cogl_pipeline_copy (shadow_pipeline_template);
  cogl_pipeline_set_layer_texture (pipeline, 0, texture);

  cogl_clear_object (&texture);

  return pipeline;
}

CoglPipeline *
_st_create_shadow_pipeline_from_actor (StShadow     *shadow_spec,
                                       ClutterActor *actor)
{
  ClutterContent *image = NULL;
  CoglPipeline *shadow_pipeline = NULL;
  float resource_scale;
  float width, height;
  ClutterPaintContext *paint_context;

  g_return_val_if_fail (clutter_actor_has_allocation (actor), NULL);

  clutter_actor_get_size (actor, &width, &height);

  if (width == 0 || height == 0)
    return NULL;

  resource_scale = clutter_actor_get_resource_scale (actor);

  width = ceilf (width * resource_scale);
  height = ceilf (height * resource_scale);

  image = clutter_actor_get_content (actor);
  if (image && CLUTTER_IS_IMAGE (image))
    {
      CoglTexture *texture;

      texture = clutter_image_get_texture (CLUTTER_IMAGE (image));
      if (texture &&
          cogl_texture_get_width (texture) == width &&
          cogl_texture_get_height (texture) == height)
        shadow_pipeline = _st_create_shadow_pipeline (shadow_spec, texture,
                                                      resource_scale);
    }

  if (shadow_pipeline == NULL)
    {
      CoglTexture *buffer;
      CoglOffscreen *offscreen;
      CoglFramebuffer *fb;
      CoglContext *ctx;
      CoglColor clear_color;
      GError *catch_error = NULL;
      float x, y;

      ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
      buffer = cogl_texture_2d_new_with_size (ctx, width, height);

      if (buffer == NULL)
        return NULL;

      offscreen = cogl_offscreen_new_with_texture (buffer);
      fb = COGL_FRAMEBUFFER (offscreen);

      if (!cogl_framebuffer_allocate (fb, &catch_error))
        {
          g_error_free (catch_error);
          g_object_unref (offscreen);
          cogl_object_unref (buffer);
          return NULL;
        }

      cogl_color_init_from_4ub (&clear_color, 0, 0, 0, 0);
      clutter_actor_get_position (actor, &x, &y);
      x *= resource_scale;
      y *= resource_scale;

      cogl_framebuffer_clear (fb, COGL_BUFFER_BIT_COLOR, &clear_color);
      cogl_framebuffer_translate (fb, -x, -y, 0);
      cogl_framebuffer_orthographic (fb, 0, 0, width, height, 0, 1.0);
      cogl_framebuffer_scale (fb, resource_scale, resource_scale, 1);

      clutter_actor_set_opacity_override (actor, 255);

      paint_context =
        clutter_paint_context_new_for_framebuffer (fb, NULL,
                                                   CLUTTER_PAINT_FLAG_NONE);
      clutter_actor_paint (actor, paint_context);
      clutter_paint_context_destroy (paint_context);

      clutter_actor_set_opacity_override (actor, -1);

      g_object_unref (fb);

      shadow_pipeline = _st_create_shadow_pipeline (shadow_spec, buffer,
                                                    resource_scale);

      cogl_object_unref (buffer);
    }

  return shadow_pipeline;
}

/**
 * _st_create_shadow_cairo_pattern:
 * @shadow_spec: the definition of the shadow
 * @src_pattern: surface pattern for which we create the shadow
 *               (must be a surface pattern)
 *
 * This is a utility function for creating shadows used by
 * st-theme-node.c; it's in this file to share the gaussian
 * blur implementation. The usage of this function is quite different
 * depending on whether shadow_spec->inset is %TRUE or not. If
 * shadow_spec->inset is %TRUE, the caller should pass in a @src_pattern
 * which is the <i>inverse</i> of what they want shadowed, and must take
 * care of the spread and offset from the shadow spec themselves. If
 * shadow_spec->inset is %FALSE then the caller should pass in what they
 * want shadowed directly, and this function takes care of the spread and
 * the offset.
 */
cairo_pattern_t *
_st_create_shadow_cairo_pattern (StShadow        *shadow_spec_in,
                                 cairo_pattern_t *src_pattern)
{
  g_autoptr(StShadow) shadow_spec = NULL;
  static cairo_user_data_key_t shadow_pattern_user_data;
  cairo_t *cr;
  cairo_surface_t *src_surface;
  cairo_surface_t *surface_in;
  cairo_surface_t *surface_out;
  cairo_pattern_t *dst_pattern;
  guchar          *pixels_in, *pixels_out;
  gint             width_in, height_in, rowstride_in;
  gint             width_out, height_out;
  size_t           rowstride_out;
  cairo_matrix_t   shadow_matrix;
  double           xscale_in, yscale_in;
  int i, j;

  g_return_val_if_fail (shadow_spec_in != NULL, NULL);
  g_return_val_if_fail (src_pattern != NULL, NULL);

  if (cairo_pattern_get_surface (src_pattern, &src_surface) != CAIRO_STATUS_SUCCESS)
    /* The most likely reason we can't get the pattern is that sizing went hairwire
     * and the caller tried to create a surface too big for memory, leaving us with
     * a pattern in an error state; we return a transparent pattern for the shadow.
     */
    return cairo_pattern_create_rgba(1.0, 1.0, 1.0, 0.0);

  width_in  = cairo_image_surface_get_width  (src_surface);
  height_in = cairo_image_surface_get_height (src_surface);

  cairo_surface_get_device_scale (src_surface, &xscale_in, &yscale_in);

  if (xscale_in != 1.0 || yscale_in != 1.0)
    {
      /* Scale the shadow specifications in a temporary copy so that
       * we can work everywhere in absolute surface coordinates */
      double scale = (xscale_in + yscale_in) / 2.0;
      shadow_spec = st_shadow_new (&shadow_spec_in->color,
                                   shadow_spec_in->xoffset * xscale_in,
                                   shadow_spec_in->yoffset * yscale_in,
                                   shadow_spec_in->blur * scale,
                                   shadow_spec_in->spread * scale,
                                   shadow_spec_in->inset);
    }
  else
    {
      shadow_spec = st_shadow_ref (shadow_spec_in);
    }

  /* We want the output to be a color agnostic alpha mask,
   * so we need to strip the color channels from the input
   */
  if (cairo_image_surface_get_format (src_surface) != CAIRO_FORMAT_A8)
    {
      surface_in = cairo_image_surface_create (CAIRO_FORMAT_A8,
                                               width_in, height_in);

      cr = cairo_create (surface_in);
      cairo_set_source_surface (cr, src_surface, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
    }
  else
    {
      surface_in = cairo_surface_reference (src_surface);
    }

  pixels_in = cairo_image_surface_get_data (surface_in);
  rowstride_in = cairo_image_surface_get_stride (surface_in);

  pixels_out = blur_pixels (pixels_in, width_in, height_in, rowstride_in,
                            shadow_spec->blur,
                            &width_out, &height_out, &rowstride_out);
  cairo_surface_destroy (surface_in);

  /* Invert pixels for inset shadows */
  if (shadow_spec->inset)
    {
      for (j = 0; j < height_out; j++)
        {
          guchar *p = pixels_out + rowstride_out * j;
          for (i = 0; i < width_out; i++, p++)
            *p = ~*p;
        }
    }

  surface_out = cairo_image_surface_create_for_data (pixels_out,
                                                     CAIRO_FORMAT_A8,
                                                     width_out,
                                                     height_out,
                                                     rowstride_out);
  cairo_surface_set_device_scale (surface_out, xscale_in, yscale_in);
  cairo_surface_set_user_data (surface_out, &shadow_pattern_user_data,
                               pixels_out, (cairo_destroy_func_t) g_free);

  dst_pattern = cairo_pattern_create_for_surface (surface_out);
  cairo_surface_destroy (surface_out);

  cairo_pattern_get_matrix (src_pattern, &shadow_matrix);

  if (shadow_spec->inset)
    {
      /* Scale the matrix in surface absolute coordinates */
      cairo_matrix_scale (&shadow_matrix, 1.0 / xscale_in, 1.0 / yscale_in);

      /* For inset shadows, offsets and spread radius have already been
       * applied to the original pattern, so all left to do is shift the
       * blurred image left, so that it aligns centered under the
       * unblurred one
       */
      cairo_matrix_translate (&shadow_matrix,
                              (width_out - width_in) / 2.0,
                              (height_out - height_in) / 2.0);

      /* Scale back the matrix in original coordinates */
      cairo_matrix_scale (&shadow_matrix, xscale_in, yscale_in);

      cairo_pattern_set_matrix (dst_pattern, &shadow_matrix);
      return dst_pattern;
    }

  /* Read all the code from the cairo_pattern_set_matrix call
   * at the end of this function to here from bottom to top,
   * because each new affine transformation is applied in
   * front of all the previous ones */

  /* 6. Invert the matrix back */
  cairo_matrix_invert (&shadow_matrix);

  /* Scale the matrix in surface absolute coordinates */
  cairo_matrix_scale (&shadow_matrix, 1.0 / xscale_in, 1.0 / yscale_in);

  /* 5. Adjust based on specified offsets */
  cairo_matrix_translate (&shadow_matrix,
                          shadow_spec->xoffset,
                          shadow_spec->yoffset);

  /* 4. Recenter the newly scaled image */
  cairo_matrix_translate (&shadow_matrix,
                          - shadow_spec->spread,
                          - shadow_spec->spread);

  /* 3. Scale up the blurred image to fill the spread */
  cairo_matrix_scale (&shadow_matrix,
                      (width_in + 2.0 * shadow_spec->spread) / width_in,
                      (height_in + 2.0 * shadow_spec->spread) / height_in);

  /* 2. Shift the blurred image left, so that it aligns centered
   * under the unblurred one */
  cairo_matrix_translate (&shadow_matrix,
                          - (width_out - width_in) / 2.0,
                          - (height_out - height_in) / 2.0);

  /* Scale back the matrix in scaled coordinates */
  cairo_matrix_scale (&shadow_matrix, xscale_in, yscale_in);

  /* 1. Invert the matrix so we can work with it in pattern space
   */
  cairo_matrix_invert (&shadow_matrix);

  cairo_pattern_set_matrix (dst_pattern, &shadow_matrix);

  return dst_pattern;
}

void
_st_paint_shadow_with_opacity (StShadow        *shadow_spec,
                               CoglFramebuffer *framebuffer,
                               CoglPipeline    *shadow_pipeline,
                               ClutterActorBox *box,
                               guint8           paint_opacity)
{
  ClutterActorBox shadow_box;
  CoglColor color;

  g_return_if_fail (shadow_spec != NULL);
  g_return_if_fail (shadow_pipeline != NULL);

  st_shadow_get_box (shadow_spec, box, &shadow_box);

  cogl_color_init_from_4ub (&color,
                            shadow_spec->color.red   * paint_opacity / 255,
                            shadow_spec->color.green * paint_opacity / 255,
                            shadow_spec->color.blue  * paint_opacity / 255,
                            shadow_spec->color.alpha * paint_opacity / 255);
  cogl_color_premultiply (&color);
  cogl_pipeline_set_layer_combine_constant (shadow_pipeline, 0, &color);
  cogl_framebuffer_draw_rectangle (framebuffer,
                                   shadow_pipeline,
                                   shadow_box.x1, shadow_box.y1,
                                   shadow_box.x2, shadow_box.y2);
}
