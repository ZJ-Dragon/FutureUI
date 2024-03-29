/* pie-chart.vala
 *
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Petr Štětka <pstetka@redhat.com>
 */

using Gtk;

public class Usage.PieChart : Gtk.DrawingArea {
    int used_percentages = 0;
    int other_percentages = 0;
    Gdk.RGBA used_color;
    Gdk.RGBA others_color;
    Gdk.RGBA available;

    class construct {
        set_css_name ("PieChart");
    }

    public PieChart () {
        set_styles ();

        this.draw.connect ((context) => {
            int height = this.get_allocated_height ();
            int width = this.get_allocated_width ();

            double xc = width / 2.0;
            double yc = height / 2.0;
            double radius = int.min (width, height) / 2.0;
            double angle1 = - Math.PI / 2.0;
            double ratio;
            double angle2 = - Math.PI / 2.0;

            if (used_percentages > 0) {
                angle1 = - Math.PI / 2.0;
                ratio = (double) used_percentages / 100;
                angle2 = ratio * 2 * Math.PI - Math.PI / 2.0;
                context.move_to (xc, yc);
                Gdk.cairo_set_source_rgba (context, used_color);
                context.arc (xc, yc, radius, angle1, angle2);
                context.fill ();
            }

            if (other_percentages > 0) {
                angle1 = angle2;
                ratio = (double) other_percentages / 100;
                angle2 = ratio * 2 * Math.PI - Math.PI / 2.0;
                context.move_to (xc, yc);
                Gdk.cairo_set_source_rgba (context, others_color);
                context.arc (xc, yc, radius, angle1, angle2);
                context.fill ();
            }

            angle1 = angle2;
            angle2 = 2 * Math.PI - Math.PI / 2.0;
            context.move_to (xc, yc);
            Gdk.cairo_set_source_rgba (context, available);
            context.arc (xc, yc, radius, angle1, angle2);
            context.fill ();
            return true;
        });
    }

    private void set_styles () {
        var context = get_style_context ();
        context.add_class ("used");
        used_color = context.get_color (context.get_state ());
        context.add_class ("others");
        others_color = context.get_color (context.get_state ());
        context.add_class ("available");
        available = context.get_color (context.get_state ());
    }

    public void update (int used_percentages, int other_percentages) {
        this.used_percentages = used_percentages;
        this.other_percentages = used_percentages + other_percentages;
        this.queue_draw ();
    }
}
