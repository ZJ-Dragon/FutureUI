/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * test-theme.c: test program for CSS styling code
 *
 * Copyright 2009, 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <clutter/clutter.h>
#include "st-theme.h"
#include "st-theme-context.h"
#include "st-label.h"
#include "st-button.h"
#include <math.h>
#include <string.h>
#include <meta-test/meta-context-test.h>
#include <meta/meta-backend.h>

#include <gtk/gtk.h>

static ClutterActor *stage;
static StThemeNode *root;
static StThemeNode *group1;
static StThemeNode *text1;
static StThemeNode *text2;
static StThemeNode *group2;
static StThemeNode *text3;
static StThemeNode *text4;
static StThemeNode *group3;
static StThemeNode *group4;
static StThemeNode *group5;
static StThemeNode *group6;
static StThemeNode *button;
static gboolean fail;

static const char *test;

static void
assert_font (StThemeNode *node,
	     const char  *node_description,
	     const char  *expected)
{
  char *value = pango_font_description_to_string (st_theme_node_get_font (node));

  if (strcmp (expected, value) != 0)
    {
      g_print ("%s: %s.font: expected: %s, got: %s\n",
	       test, node_description, expected, value);
      fail = TRUE;
    }

  g_free (value);
}

static void
assert_font_features (StThemeNode *node,
                      const char  *node_description,
                      const char  *expected)
{
  char *value = st_theme_node_get_font_features (node);

  if (g_strcmp0 (expected, value) != 0)
    {
      g_print ("%s: %s.font-feature-settings: expected: %s, got: %s\n",
               test, node_description, expected, value);
      fail = TRUE;
    }

  if (value)
    g_free (value);
}

static char *
text_decoration_to_string (StTextDecoration decoration)
{
  GString *result = g_string_new (NULL);

  if (decoration & ST_TEXT_DECORATION_UNDERLINE)
    g_string_append(result, " underline");
  if (decoration & ST_TEXT_DECORATION_OVERLINE)
    g_string_append(result, " overline");
  if (decoration & ST_TEXT_DECORATION_LINE_THROUGH)
    g_string_append(result, " line_through");
  if (decoration & ST_TEXT_DECORATION_BLINK)
    g_string_append(result, " blink");

  if (result->len > 0)
    g_string_erase (result, 0, 1);
  else
    g_string_append(result, "none");

  return g_string_free (result, FALSE);
}

static void
assert_text_decoration (StThemeNode     *node,
			const char      *node_description,
			StTextDecoration expected)
{
  StTextDecoration value = st_theme_node_get_text_decoration (node);
  if (expected != value)
    {
      char *es = text_decoration_to_string (expected);
      char *vs = text_decoration_to_string (value);

      g_print ("%s: %s.text-decoration: expected: %s, got: %s\n",
	       test, node_description, es, vs);
      fail = TRUE;

      g_free (es);
      g_free (vs);
    }
}

static void
assert_foreground_color (StThemeNode *node,
			 const char  *node_description,
			 guint32      expected)
{
  ClutterColor color;
  guint32 value;

  st_theme_node_get_foreground_color (node, &color);
  value = clutter_color_to_pixel (&color);

  if (expected != value)
    {
      g_print ("%s: %s.color: expected: #%08x, got: #%08x\n",
	       test, node_description, expected, value);
      fail = TRUE;
    }
}

static void
assert_background_color (StThemeNode *node,
			 const char  *node_description,
			 guint32      expected)
{
  ClutterColor color;
  guint32 value;

  st_theme_node_get_background_color (node, &color);
  value = clutter_color_to_pixel (&color);

  if (expected != value)
    {
      g_print ("%s: %s.background-color: expected: #%08x, got: #%08x\n",
	       test, node_description, expected, value);
      fail = TRUE;
    }
}

static const char *
side_to_string (StSide side)
{
  switch (side)
    {
    case ST_SIDE_TOP:
      return "top";
    case ST_SIDE_RIGHT:
      return "right";
    case ST_SIDE_BOTTOM:
      return "bottom";
    case ST_SIDE_LEFT:
      return "left";
    default:
      return "<unknown>";
    }
}

static void
assert_border_color (StThemeNode *node,
                     const char  *node_description,
                     StSide       side,
                     guint32      expected)
{
  ClutterColor color;
  guint32 value;

  st_theme_node_get_border_color (node, side, &color);
  value = clutter_color_to_pixel (&color);

  if (expected != value)
    {
      g_print ("%s: %s.border-%s-color: expected: #%08x, got: #%08x\n",
	       test, node_description, side_to_string (side), expected, value);
      fail = TRUE;
    }
}

static void
assert_background_image (StThemeNode *node,
			 const char  *node_description,
			 const char  *expected)
{
  GFile *value = st_theme_node_get_background_image (node);
  GFile *expected_file;

  if (expected != NULL && value != NULL)
    {
      expected_file = g_file_new_for_path (expected);

      if (!g_file_equal (expected_file, value))
        {
          char *uri = g_file_get_uri (expected_file);
          g_print ("%s: %s.background-image: expected: %s, got: %s\n",
                   test, node_description, expected, uri);
          fail = TRUE;
          g_free (uri);
        }
    }
}

#define LENGTH_EPSILON 0.001

static void
assert_length (const char     *node_description,
	       const char     *property_description,
	       double          expected,
	       double          value)
{
  if (fabs (expected - value) > LENGTH_EPSILON)
    {
      g_print ("%s %s.%s: expected: %3f, got: %3f\n",
	       test, node_description, property_description, expected, value);
      fail = TRUE;
    }
}

static void
test_defaults (void)
{
  test = "defaults";
  /* font comes from context */
  assert_font (root, "stage", "sans-serif 12");
  /* black is the default foreground color */
  assert_foreground_color (root, "stage", 0x00000ff);
}

static void
test_lengths (void)
{
  test = "lengths";
  /* 12pt == 16px at 96dpi */
  assert_length ("group1", "padding-top", 16.,
		 st_theme_node_get_padding (group1, ST_SIDE_TOP));
  /* 12px == 12px */
  assert_length ("group1", "padding-right", 12.,
		 st_theme_node_get_padding (group1, ST_SIDE_RIGHT));
  /* 2em == 32px (with a 12pt font) */
  assert_length ("group1", "padding-bottom", 32.,
		 st_theme_node_get_padding (group1, ST_SIDE_BOTTOM));
  /* 1in == 72pt == 96px, at 96dpi */
  assert_length ("group1", "padding-left", 96.,
		 st_theme_node_get_padding (group1, ST_SIDE_LEFT));

  /* 12pt == 16px at 96dpi */
  assert_length ("group1", "margin-top", 16.,
		 st_theme_node_get_margin (group1, ST_SIDE_TOP));
  /* 12px == 12px */
  assert_length ("group1", "margin-right", 12.,
		 st_theme_node_get_margin (group1, ST_SIDE_RIGHT));
  /* 2em == 32px (with a 12pt font) */
  assert_length ("group1", "margin-bottom", 32.,
		 st_theme_node_get_margin (group1, ST_SIDE_BOTTOM));
  /* 1in == 72pt == 96px, at 96dpi */
  assert_length ("group1", "margin-left", 96.,
		 st_theme_node_get_margin (group1, ST_SIDE_LEFT));
}

static void
test_classes (void)
{
  test = "classes";
  /* .special-text class overrides size and style;
   * the StBin.special-text selector doesn't match */
  assert_font (text1, "text1", "sans-serif Italic 32px");
}

static void
test_type_inheritance (void)
{
  test = "type_inheritance";
  /* From StBin element selector */
  assert_length ("button", "padding-top", 10.,
		 st_theme_node_get_padding (button, ST_SIDE_TOP));
  /* From StButton element selector */
  assert_length ("button", "padding-right", 20.,
		 st_theme_node_get_padding (button, ST_SIDE_RIGHT));
}

static void
test_adjacent_selector (void)
{
  test = "adjacent_selector";
  /* #group1 > #text1 matches text1 */
  assert_foreground_color (text1, "text1", 0x00ff00ff);
  /* stage > #text2 doesn't match text2 */
  assert_foreground_color (text2, "text2", 0x000000ff);
}

static void
test_padding (void)
{
  test = "padding";
  /* Test that a 4-sided padding property assigns the right paddings to
   * all sides */
  assert_length ("group2", "padding-top", 1.,
		 st_theme_node_get_padding (group2, ST_SIDE_TOP));
  assert_length ("group2", "padding-right", 2.,
		 st_theme_node_get_padding (group2, ST_SIDE_RIGHT));
  assert_length ("group2", "padding-bottom", 3.,
		 st_theme_node_get_padding (group2, ST_SIDE_BOTTOM));
  assert_length ("group2", "padding-left", 4.,
		 st_theme_node_get_padding (group2, ST_SIDE_LEFT));
}

static void
test_margin (void)
{
  test = "margin";
  /* Test that a 4-sided margin property assigns the right margin to
   * all sides */
  assert_length ("group2", "margin-top", 1.,
		 st_theme_node_get_margin (group2, ST_SIDE_TOP));
  assert_length ("group2", "margin-right", 2.,
		 st_theme_node_get_margin (group2, ST_SIDE_RIGHT));
  assert_length ("group2", "margin-bottom", 3.,
		 st_theme_node_get_margin (group2, ST_SIDE_BOTTOM));
  assert_length ("group2", "margin-left", 4.,
		 st_theme_node_get_margin (group2, ST_SIDE_LEFT));

  /* Test that a 3-sided margin property assigns the right margin to
   * all sides */
  assert_length ("group4", "margin-top", 1.,
     st_theme_node_get_margin (group4, ST_SIDE_TOP));
  assert_length ("group4", "margin-right", 2.,
     st_theme_node_get_margin (group4, ST_SIDE_RIGHT));
  assert_length ("group4", "margin-bottom", 3.,
     st_theme_node_get_margin (group4, ST_SIDE_BOTTOM));
  assert_length ("group4", "margin-left", 2.,
     st_theme_node_get_margin (group4, ST_SIDE_LEFT));

  /* Test that a 2-sided margin property assigns the right margin to
   * all sides */
  assert_length ("group5", "margin-top", 1.,
     st_theme_node_get_margin (group5, ST_SIDE_TOP));
  assert_length ("group5", "margin-right", 2.,
     st_theme_node_get_margin (group5, ST_SIDE_RIGHT));
  assert_length ("group5", "margin-bottom", 1.,
     st_theme_node_get_margin (group5, ST_SIDE_BOTTOM));
  assert_length ("group5", "margin-left", 2.,
     st_theme_node_get_margin (group5, ST_SIDE_LEFT));

  /* Test that all sides have a margin of 0 when not specified */
  assert_length ("group6", "margin-top", 0.,
     st_theme_node_get_margin (group6, ST_SIDE_TOP));
  assert_length ("group6", "margin-right", 0.,
     st_theme_node_get_margin (group6, ST_SIDE_RIGHT));
  assert_length ("group6", "margin-bottom", 0.,
     st_theme_node_get_margin (group6, ST_SIDE_BOTTOM));
  assert_length ("group6", "margin-left", 0.,
     st_theme_node_get_margin (group6, ST_SIDE_LEFT));
}

static void
test_border (void)
{
  test = "border";

  /* group2 is defined as having a thin black border along the top three
   * sides with rounded joins, then a square-joined green border at the
   * bottom
   */

  assert_length ("group2", "border-top-width", 2.,
		 st_theme_node_get_border_width (group2, ST_SIDE_TOP));
  assert_length ("group2", "border-right-width", 2.,
		 st_theme_node_get_border_width (group2, ST_SIDE_RIGHT));
  assert_length ("group2", "border-bottom-width", 5.,
		 st_theme_node_get_border_width (group2, ST_SIDE_BOTTOM));
  assert_length ("group2", "border-left-width", 2.,
		 st_theme_node_get_border_width (group2, ST_SIDE_LEFT));

  assert_border_color (group2, "group2", ST_SIDE_TOP,    0x000000ff);
  assert_border_color (group2, "group2", ST_SIDE_RIGHT,  0x000000ff);
  assert_border_color (group2, "group2", ST_SIDE_BOTTOM, 0x0000ffff);
  assert_border_color (group2, "group2", ST_SIDE_LEFT,   0x000000ff);

  assert_length ("group2", "border-radius-topleft", 10.,
		 st_theme_node_get_border_radius (group2, ST_CORNER_TOPLEFT));
  assert_length ("group2", "border-radius-topright", 10.,
		 st_theme_node_get_border_radius (group2, ST_CORNER_TOPRIGHT));
  assert_length ("group2", "border-radius-bottomright", 0.,
		 st_theme_node_get_border_radius (group2, ST_CORNER_BOTTOMRIGHT));
  assert_length ("group2", "border-radius-bottomleft", 0.,
		 st_theme_node_get_border_radius (group2, ST_CORNER_BOTTOMLEFT));
}

static void
test_background (void)
{
  test = "background";
  /* group1 has a background: shortcut property setting color and image */
  assert_background_color (group1, "group1", 0xff0000ff);
  assert_background_image (group1, "group1", "some-background.png");
  /* text1 inherits the background image but not the color */
  assert_background_color (text1,  "text1",  0x00000000);
  assert_background_image (text1,  "text1",  "some-background.png");
  /* text2 inherits both, but then background: none overrides both */
  assert_background_color (text2,  "text2",  0x00000000);
  assert_background_image (text2,  "text2",  NULL);
  /* background-image property */
  assert_background_image (group2, "group2", "other-background.png");
}

static void
test_font (void)
{
  test = "font";
  /* font specified with font: */
  assert_font (group2, "group2", "serif Italic 12px");
  /* text3 inherits and overrides individually properties */
  assert_font (text3,  "text3",  "serif Bold Oblique Small-Caps 24px");
}

static void
test_font_features (void)
{
  test = "font_features";
  /* group1 has font-feature-settings: "tnum" */
  assert_font_features (group1, "group1", "\"tnum\"");
  /* text2 should inherit from group1 */
  assert_font_features (text2,  "text2", "\"tnum\"");
  /* group2 has font-feature-settings: "tnum", "zero" */
  assert_font_features (group2, "group2", "\"tnum\", \"zero\"");
  /* text3 should inherit from group2 using the inherit keyword */
  assert_font_features (text3,  "text3",  "\"tnum\", \"zero\"");
  /* text4 has font-feature-settings: normal */
  assert_font_features (text4,  "text4",  NULL);
}

static void
test_pseudo_class (void)
{
  StWidget *label;
  StThemeNode *labelNode;

  test = "pseudo_class";
  /* text4 has :visited and :hover pseudo-classes, so should pick up both of these */
  assert_foreground_color (text4,   "text4",  0x888888ff);
  assert_text_decoration  (text4,   "text4",  ST_TEXT_DECORATION_UNDERLINE);
  /* :hover pseudo-class matches, but class doesn't match */
  assert_text_decoration  (group3,  "group3", 0);

  /* Test the StWidget add/remove pseudo_class interfaces */
  label = st_label_new ("foo");
  clutter_actor_add_child (stage, CLUTTER_ACTOR (label));

  labelNode = st_widget_get_theme_node (label);
  assert_foreground_color (labelNode, "label", 0x000000ff);
  assert_text_decoration  (labelNode, "label", 0);
  assert_length ("label", "border-width", 0.,
                 st_theme_node_get_border_width (labelNode, ST_SIDE_TOP));

  st_widget_add_style_pseudo_class (label, "visited");
  g_assert (st_widget_has_style_pseudo_class (label, "visited"));
  labelNode = st_widget_get_theme_node (label);
  assert_foreground_color (labelNode, "label", 0x888888ff);
  assert_text_decoration  (labelNode, "label", 0);
  assert_length ("label", "border-width", 0.,
                 st_theme_node_get_border_width (labelNode, ST_SIDE_TOP));

  st_widget_add_style_pseudo_class (label, "hover");
  g_assert (st_widget_has_style_pseudo_class (label, "hover"));
  labelNode = st_widget_get_theme_node (label);
  assert_foreground_color (labelNode, "label", 0x888888ff);
  assert_text_decoration  (labelNode, "label", ST_TEXT_DECORATION_UNDERLINE);
  assert_length ("label", "border-width", 0.,
                 st_theme_node_get_border_width (labelNode, ST_SIDE_TOP));

  st_widget_remove_style_pseudo_class (label, "visited");
  g_assert (!st_widget_has_style_pseudo_class (label, "visited"));
  g_assert (st_widget_has_style_pseudo_class (label, "hover"));
  labelNode = st_widget_get_theme_node (label);
  assert_foreground_color (labelNode, "label", 0x000000ff);
  assert_text_decoration  (labelNode, "label", ST_TEXT_DECORATION_UNDERLINE);
  assert_length ("label", "border-width", 0.,
                 st_theme_node_get_border_width (labelNode, ST_SIDE_TOP));

  st_widget_add_style_pseudo_class (label, "boxed");
  labelNode = st_widget_get_theme_node (label);
  assert_foreground_color (labelNode, "label", 0x000000ff);
  assert_text_decoration  (labelNode, "label", ST_TEXT_DECORATION_UNDERLINE);
  assert_length ("label", "border-width", 1.,
                 st_theme_node_get_border_width (labelNode, ST_SIDE_TOP));

  st_widget_remove_style_pseudo_class (label, "hover");
  labelNode = st_widget_get_theme_node (label);
  assert_foreground_color (labelNode, "label", 0x000000ff);
  assert_text_decoration  (labelNode, "label", 0);
  assert_length ("label", "border-width", 1.,
                 st_theme_node_get_border_width (labelNode, ST_SIDE_TOP));

  st_widget_remove_style_pseudo_class (label, "boxed");
  g_assert (!st_widget_has_style_pseudo_class (label, "boxed"));
  g_assert (st_widget_has_style_pseudo_class (label, "insensitive"));
  labelNode = st_widget_get_theme_node (label);
  assert_foreground_color (labelNode, "label", 0x000000ff);
  assert_text_decoration  (labelNode, "label", 0);
  assert_length ("label", "border-width", 0.,
                 st_theme_node_get_border_width (labelNode, ST_SIDE_TOP));

  clutter_actor_set_reactive (CLUTTER_ACTOR (label), TRUE);
  g_assert (st_widget_get_style_pseudo_class (label) == NULL);
}

static void
test_inline_style (void)
{
  test = "inline_style";
  /* These properties come from the inline-style specified when creating the node */
  assert_foreground_color (text3,   "text3",  0x00000ffff);
  assert_length ("text3", "padding-bottom", 12.,
                 st_theme_node_get_padding (text3, ST_SIDE_BOTTOM));
}

int
main (int argc, char **argv)
{
  MetaContext *context;
  g_autoptr (GError) error = NULL;
  MetaBackend *backend;
  StTheme *theme;
  StThemeContext *theme_context;
  PangoFontDescription *font_desc;
  GFile *file;
  g_autofree char *cwd = NULL;

  gtk_init (&argc, &argv);

  /* meta_init() cds to $HOME */
  cwd = g_get_current_dir ();

  context = meta_create_test_context (META_CONTEXT_TEST_TYPE_NESTED,
                                      META_CONTEXT_TEST_FLAG_NONE);
  if (!meta_context_configure (context, &argc, &argv, &error))
    g_error ("Failed to configure: %s", error->message);

  if (!meta_context_setup (context, &error))
    g_error ("Failed to setup: %s", error->message);

  if (chdir (cwd) < 0)
    g_error ("chdir('%s') failed: %s", cwd, g_strerror (errno));

  /* Make sure our assumptions about resolution are correct */
  g_object_set (clutter_settings_get_default (), "font-dpi", -1, NULL);

  file = g_file_new_for_path ("test-theme.css");
  theme = st_theme_new (file, NULL, NULL);
  g_object_unref (file);

  backend = meta_context_get_backend (context);
  stage = meta_backend_get_stage (backend);
  theme_context = st_theme_context_get_for_stage (CLUTTER_STAGE (stage));
  st_theme_context_set_theme (theme_context, theme);

  font_desc = pango_font_description_from_string ("sans-serif 12");
  st_theme_context_set_font (theme_context, font_desc);
  pango_font_description_free (font_desc);

  root = st_theme_context_get_root_node (theme_context);
  group1 = st_theme_node_new (theme_context, root, NULL,
                              CLUTTER_TYPE_ACTOR, "group1", NULL, NULL, NULL);
  text1 = st_theme_node_new  (theme_context, group1, NULL,
                              CLUTTER_TYPE_TEXT, "text1", "special-text", NULL, NULL);
  text2 = st_theme_node_new  (theme_context, group1, NULL,
                              CLUTTER_TYPE_TEXT, "text2", NULL, NULL, NULL);
  group2 = st_theme_node_new (theme_context, root, NULL,
                              CLUTTER_TYPE_ACTOR, "group2", NULL, NULL, NULL);
  group4 = st_theme_node_new (theme_context, root, NULL,
                              CLUTTER_TYPE_ACTOR, "group4", NULL, NULL, NULL);
  group5 = st_theme_node_new (theme_context, root, NULL,
                              CLUTTER_TYPE_ACTOR, "group5", NULL, NULL, NULL);
  group6 = st_theme_node_new (theme_context, root, NULL,
                              CLUTTER_TYPE_ACTOR, "group6", NULL, NULL, NULL);
  text3 = st_theme_node_new  (theme_context, group2, NULL,
                              CLUTTER_TYPE_TEXT, "text3", NULL, NULL,
                              "color: #0000ff; padding-bottom: 12px;");
  text4 = st_theme_node_new  (theme_context, group2, NULL,
                              CLUTTER_TYPE_TEXT, "text4", NULL, "visited hover", NULL);
  group3 = st_theme_node_new (theme_context, group2, NULL,
                              CLUTTER_TYPE_ACTOR, "group3", NULL, "hover", NULL);
  button = st_theme_node_new (theme_context, root, NULL,
                              ST_TYPE_BUTTON, "button", NULL, NULL, NULL);

  test_defaults ();
  test_lengths ();
  test_classes ();
  test_type_inheritance ();
  test_adjacent_selector ();
  test_padding ();
  test_margin ();
  test_border ();
  test_background ();
  test_font ();
  test_font_features ();
  test_pseudo_class ();
  test_inline_style ();

  g_object_unref (button);
  g_object_unref (group1);
  g_object_unref (group2);
  g_object_unref (group3);
  g_object_unref (group4);
  g_object_unref (group5);
  g_object_unref (group6);
  g_object_unref (text1);
  g_object_unref (text2);
  g_object_unref (text3);
  g_object_unref (text4);
  g_object_unref (theme);

  g_object_unref (context);

  return fail ? 1 : 0;
}
