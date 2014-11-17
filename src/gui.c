/*
All GUI-related features are found in this file.

The GUI is written in GTK3. There a drawable area using Cairo where we display
the picture and on which we can draw the anchors; this could arguably be made
independent.
*/

/*
TODO: When shrinking the window, status bar disappears.
TODO: UI: center drawable_area.
TODO: Destroy widgets properly.
TODO: support JPG and other formats.
*/

#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "perspector.h"

/* GUI properties */
#define BRUSHSZ 10
#define ZOOM_FACTOR 20
#define ZOOM_MIN 0.2
#define ZOOM_MAX 5

/* Anchors live in the global space since their are unique. */
static pixelset anchors = { .count = 0 };

/* Surface to store current scribbles */
static cairo_surface_t *surface = NULL;
/* Surface to store background */
static cairo_surface_t *bg = NULL;
/* Surface to store result of transform */
static cairo_surface_t *sink = NULL;

static GtkWidget *ratio_width;
static GtkWidget *ratio_height;
static GtkWidget *status;
static GtkWidget *drawable_area;
static GtkWidget *out;

static double zoom = 0;
static bool ctrl_pressed = false;

/******************************************************************************/
/* Tools */

/* Insert a suffix before extension, or at the end if no extension.
         Return value must be freed. */
char *file_suffix(const char *path, const char *suffix) {
	#include <libgen.h>
	if (!path) {
		return NULL;
	}
	if (!suffix) {
		return NULL;
	}

	char *dirc = strdup(path);
	char *basec = strdup(path);
	char *dname = dirname(dirc);
	char *bname = basename(basec);
	char *ext = strrchr(bname, '.');
	if (ext) {
		bname = strndup(bname, ext - bname);
	} else {
		ext = "";
	}

	size_t len = strlen(dname) + 1 + strlen(bname) + strlen(suffix) + strlen(ext) + 1;
	char *result = malloc(len * sizeof (char));
	snprintf(result, len, "%s/%s%s%s", dname, bname, suffix, ext);

	free(dirc);
	free(basec);
	if (*ext != '\0') {
		free(bname);
	}

	return result;
}


/******************************************************************************/
/* GTK tools */

static inline void box_prepend(GtkWidget *box, GtkWidget *child) {
	gtk_box_pack_start(GTK_BOX(box), child, FALSE, FALSE, 0);
}

static inline void box_append(GtkWidget *box, GtkWidget *child) {
	gtk_box_pack_end(GTK_BOX(box), child, FALSE, FALSE, 0);
}

/******************************************************************************/

double zoom_value(double z) {
	z = 1 + z / ZOOM_FACTOR;
	if (z < ZOOM_MIN) {
		return ZOOM_MIN;
	} else if (z > ZOOM_MAX) {
		return ZOOM_MAX;
	} else {
		return z;
	}
}

/* Draw a rectangle on the surface at the given position */
static void draw_brush(GtkWidget *widget, coord x, coord y) {
	cairo_t *cr = cairo_create(surface);
	double z = zoom_value(zoom);
	double brush = BRUSHSZ;

	cairo_set_source_rgb(cr, 1, 0, 0);
	cairo_rectangle(cr, x * z - brush / 2, y * z - brush / 2, brush, brush);
	cairo_fill(cr);

	brush = BRUSHSZ - 3;
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_rectangle(cr, x * z - brush / 2, y * z - brush / 2, brush, brush);
	cairo_fill(cr);

	cairo_destroy(cr);

	/* Now invalidate the affected region of the drawing area. */
	gtk_widget_queue_draw_area(widget, x * z - BRUSHSZ / 2, y * z - BRUSHSZ / 2, BRUSHSZ, BRUSHSZ);
}

static void clear_surface(void) {
	if (!bg) {
		return;
	}

	cairo_t *cr = cairo_create(surface);
	double z = zoom_value(zoom);
	cairo_scale(cr, z, z);
	cairo_set_source_surface(cr, bg, 0, 0);

	/* set a minimum size */
	int width = cairo_image_surface_get_width(bg);
	int height = cairo_image_surface_get_height(bg);
	gtk_widget_set_size_request(drawable_area, width * z, height * z);

	cairo_paint(cr);
	cairo_destroy(cr);

	/* Draw anchors. */
	size_t i;
	for (i = 0; i < anchors.count; i++) {
		draw_brush(drawable_area, anchors.pixels[i].x, anchors.pixels[i].y);
	}
}

static void load_image(const char *path) {
	cairo_surface_t *oldbg = bg;
	bg = cairo_image_surface_create_from_png(path);
	cairo_status_t cstatus = cairo_surface_status(bg);
	if (cstatus) {
		gtk_label_set_text(GTK_LABEL(status), cairo_status_to_string(cstatus));
		cairo_surface_destroy(bg);
		bg = oldbg;
	} else {
		clear_surface();
		char *outname = file_suffix(path, "-new");
		gtk_entry_set_text(GTK_ENTRY(out), outname);
		free(outname);
	}

}

/******************************************************************************/

static void event_open(GtkWidget *widget, gpointer data) {
	(void)widget;
	(void)data;

	GtkWidget *dialog = gtk_file_chooser_dialog_new("Open File",
			NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			"Cancel", GTK_RESPONSE_CANCEL,
			"Open",
			GTK_RESPONSE_ACCEPT,
			NULL
			);

	gint res = gtk_dialog_run(GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
		char *filename = gtk_file_chooser_get_filename(chooser);
		load_image(filename);
		g_free(filename);
	}

	gtk_widget_destroy(dialog);
}

/* Create a new surface of the appropriate size to store our scribbles */
static gboolean event_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
	(void)event;
	(void)data;

	if (surface) {
		cairo_surface_destroy(surface);
	}

	surface = gdk_window_create_similar_surface(gtk_widget_get_window(widget),
			CAIRO_CONTENT_COLOR,
			gtk_widget_get_allocated_width(widget),
			gtk_widget_get_allocated_height(widget));

	clear_surface();
	return TRUE;
}

/* Redraw the screen from the surface. Note that the ::draw signal receives a
ready-to-be-used cairo_t that is already clipped to only draw the exposed areas
of the widget. */
static gboolean event_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
	(void)widget;
	(void)data;
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_paint(cr);

	return FALSE;
}

static gboolean event_scroll(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	(void)widget;
	(void)event;
	(void)user_data;

	GdkScrollDirection direction;
	gboolean scroll_status = gdk_event_get_scroll_direction(event, &direction);
	if (scroll_status && ctrl_pressed) {
		if (direction == GDK_SCROLL_UP) {
			zoom++;
		} else {
			zoom--;
		}
		clear_surface();
		gtk_widget_queue_draw(widget);
		return TRUE;
	}

	return FALSE;
}

/* Handle button press events by either drawing a rectangle or clearing the
 * surface, depending on which button was pressed. The ::button-press signal
 * handler receives a GdkEventButton struct which contains this information.
 */
static gboolean event_button_press(GtkWidget *drawable, GdkEventButton *event, gpointer data) {
	(void)data;

	/* Paranoia check, in case we haven't gotten a configure event. */
	if (surface == NULL) {
		return FALSE;
	}

	if (event->button == GDK_BUTTON_PRIMARY) {
		if (anchors.count >= sizeof anchors.pixels / sizeof anchors.pixels[0]) {
			gtk_label_set_text(GTK_LABEL(status), "Max number of anchors reached.");
		} else {
			double z = zoom_value(zoom);
			coord x = (coord)(event->x / z);
			coord y = (coord)(event->y / z);
			size_t i;
			for (i = 0; i < anchors.count; i++) {
				if (anchors.pixels[i].x == x && anchors.pixels[i].y == y) {
					/* Pixel is already an anchor. */
					return TRUE;
				}
			}
			anchors.pixels[anchors.count].x = x;
			anchors.pixels[anchors.count].y = y;
			draw_brush(drawable, x, y);
			anchors.count++;
		}
	} else if (event->button == GDK_BUTTON_SECONDARY) {
		double z = zoom_value(zoom);
		coord x = (coord)(event->x / z);
		coord y = (coord)(event->y / z);
		coord radius = BRUSHSZ / 2 / z;

		/* We loop backward in case rectangles are stack, then last one gets erased
		* first. */
		if (anchors.count > 0) {
			size_t i = anchors.count - 1;
			do {
				if (x <= anchors.pixels[i].x + radius &&
					x >= anchors.pixels[i].x - radius &&
					y <= anchors.pixels[i].y + radius &&
					y >= anchors.pixels[i].y - radius) {
					anchors.count--;
					anchors.pixels[i] = anchors.pixels[anchors.count];
					clear_surface();
					gtk_widget_queue_draw(drawable);
					gtk_label_set_text(GTK_LABEL(status), "");
					break;
				}
			} while (i-- != 0);
		}
	}

	/* We have handled the event, stop processing. */
	return TRUE;
}

static gboolean event_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	(void)widget;
	(void)data;

	if (event->state & GDK_CONTROL_MASK) {
		ctrl_pressed = false;
	} else {
		ctrl_pressed = true;
	}

	return FALSE;
}


static gboolean event_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer coord_status) {
	(void)widget;

	if (surface == NULL) {
		return FALSE;
	}

	#define COORD_TEXT_LEN 128
	char text[COORD_TEXT_LEN];
	snprintf(text, COORD_TEXT_LEN, "(%i, %i)", (gint)event->x, (gint)event->y);
	gtk_label_set_text(GTK_LABEL(coord_status), text);

	return TRUE;
}

static void event_process(GtkWidget *widget, gpointer data) {
	(void)widget;
	(void)data;

	if (anchors.count != 4) {
		gtk_label_set_text(GTK_LABEL(status), "4 anchors required.");
		return;
	}

	cairo_format_t format = cairo_image_surface_get_format(bg);
	if (format != CAIRO_FORMAT_RGB24 && format != CAIRO_FORMAT_ARGB32) {
		gtk_label_set_text(GTK_LABEL(status), "Unsupported format.");
		return;
	}

	/* Flush to ensure all writing to the image was done */
	cairo_surface_flush(bg);

	/* Initialize sink. Width and height is the smallest rectangle containing */
	/* the anchors and fitting the ratio. */
	coord sink_width, sink_height;
	size_t i;
	coord minx = anchors.pixels[0].x;
	coord miny = anchors.pixels[0].y;
	coord maxx = anchors.pixels[0].x;
	coord maxy = anchors.pixels[0].y;
	for (i = 0; i < anchors.count; i++) {
		if (anchors.pixels[i].x < minx) {
			minx = anchors.pixels[i].x;
		}
		if (anchors.pixels[i].y < miny) {
			miny = anchors.pixels[i].y;
		}
		if (anchors.pixels[i].x > maxx) {
			maxx = anchors.pixels[i].x;
		}
		if (anchors.pixels[i].y > maxy) {
			maxy = anchors.pixels[i].y;
		}
	}
	/* The following cannot be negative. */
	sink_width = maxx - minx;
	sink_height = maxy - miny;

	errno = 0;
	double ratio_w = strtod(gtk_entry_get_text(GTK_ENTRY(ratio_width)), NULL);
	if (errno || ratio_w <= 0) {
		gtk_label_set_text(GTK_LABEL(status), "Wrong value for width.");
		return;
	}
	double ratio_h = strtod(gtk_entry_get_text(GTK_ENTRY(ratio_height)), NULL);
	if (errno || ratio_h <= 0) {
		gtk_label_set_text(GTK_LABEL(status), "Wrong value for height.");
		return;
	}

	double ratio = ratio_w / ratio_h;
	if (sink_height < sink_height * ratio) {
		sink_height = sink_width * ratio;
	} else if (sink_height > sink_height * ratio) {
		sink_width = sink_height * ratio_h / ratio_w;
	}

	sink = cairo_surface_create_similar_image(bg, CAIRO_FORMAT_ARGB32, sink_width, sink_height);
	unsigned char *sink_data = cairo_image_surface_get_data(sink);

	/* Background data. */
	unsigned char *bg_data = cairo_image_surface_get_data(bg);
	coord bg_width = cairo_image_surface_get_width(bg);
	coord bg_height = cairo_image_surface_get_height(bg);

	/* Modify the image. */
	bool ustatus = perspector((color *)sink_data, sink_width, sink_height, (color *)bg_data, bg_width, bg_height, &anchors);
	if (ustatus) {
		gtk_label_set_text(GTK_LABEL(status), "Transformation applied.");
	} else {
		gtk_label_set_text(GTK_LABEL(status), "Anchors configuration is not usable.");
		cairo_surface_destroy(sink);
		sink = NULL;
	}
}

static void event_write(GtkWidget *widget, gpointer data) {
	(void)widget;

	const char *outname = gtk_entry_get_text(data);
	cairo_status_t cstatus;
	if (!sink) {
		gtk_label_set_text(GTK_LABEL(status), "Apply transformation first.");
	} else {
		cstatus = cairo_surface_write_to_png(sink, outname);

		if (cstatus) {
			gtk_label_set_text(GTK_LABEL(status), cairo_status_to_string(cstatus));
		} else {
			cairo_surface_destroy(sink);
			sink = NULL;
			gtk_label_set_text(GTK_LABEL(status), "File succesfully written.");
		}
	}
}

static void event_close(void) {
	if (surface) {
		cairo_surface_destroy(surface);
	}
	if (bg) {
		cairo_surface_destroy(bg);
	}
	if (sink) {
		cairo_surface_destroy(sink);
	}

	gtk_main_quit();
}

/******************************************************************************/

static void usage(const char *cmdname) {
	printf("Usage: %s [IMAGE]\n", cmdname);
}

int main(int argc, char *argv[]) {
	if (argc >= 2 && strncmp(argv[1], "-h", 2) == 0) {
		usage(argv[0]);
		return 1;
	}
	gtk_init(&argc, &argv);

	GtkWidget *window;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), argv[0]);
	g_signal_connect(window, "destroy", G_CALLBACK(event_close), NULL);
	/* gtk_container_set_border_width (GTK_CONTAINER (window), 10); */
	g_signal_connect(window, "key-press-event", G_CALLBACK(event_key_press), NULL);
	g_signal_connect(window, "key-release-event", G_CALLBACK(event_key_press), NULL);
	gtk_widget_set_events(window, gtk_widget_get_events(window)
		| GDK_KEY_PRESS_MASK
		| GDK_KEY_RELEASE_MASK);

	drawable_area = gtk_drawing_area_new();

	GtkWidget *coord_display = gtk_label_new("(0, 0)");
	status = gtk_label_new("");

	/* Signals used to handle the backing surface */
	g_signal_connect(drawable_area, "draw", G_CALLBACK(event_draw), NULL);
	g_signal_connect(drawable_area, "configure-event", G_CALLBACK(event_configure), NULL);
	/* Event signals */
	g_signal_connect(drawable_area, "motion-notify-event", G_CALLBACK(event_motion_notify), coord_display);
	g_signal_connect(drawable_area, "button-press-event", G_CALLBACK(event_button_press), NULL);
	g_signal_connect(drawable_area, "scroll-event", G_CALLBACK(event_scroll), NULL);
	/* Ask to receive events the drawing area doesn't normally subscribe to. */
	gtk_widget_set_events(drawable_area, gtk_widget_get_events(drawable_area)
		| GDK_BUTTON_PRESS_MASK
		| GDK_POINTER_MOTION_MASK
		| GDK_SCROLL_MASK);

	GtkWidget *open = gtk_button_new_with_label("Open");
	g_signal_connect(open, "clicked", G_CALLBACK(event_open), NULL);
	GtkWidget *process = gtk_button_new_with_label("Process");
	g_signal_connect(process, "clicked", G_CALLBACK(event_process), NULL);
	GtkWidget *ratio_width_label = gtk_label_new("Width");
	ratio_width = gtk_entry_new();
	GtkWidget *ratio_height_label = gtk_label_new("Height");
	ratio_height = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(ratio_width), "1");
	gtk_entry_set_text(GTK_ENTRY(ratio_height), "1");
	out = gtk_entry_new();

	GtkWidget *write = gtk_button_new_with_label("Write");
	g_signal_connect(write, "clicked", G_CALLBACK(event_write), out);
	/* GtkWidget *exit = gtk_button_new_with_label ("Exit"); */
	/* g_signal_connect_swapped (exit, "clicked", G_CALLBACK (event_close), window); */

	GtkWidget *menubox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing(GTK_BOX(menubox), 2);
	box_prepend(menubox, open);
	box_prepend(menubox, ratio_width_label);
	box_prepend(menubox, ratio_width);
	/* TODO: Filename box should be the only one to extend. */
	/* gtk_widget_set_size_request (ratio_width, 50, -1); */
	box_prepend(menubox, ratio_height_label);
	box_prepend(menubox, ratio_height);
	box_prepend(menubox, process);
	gtk_box_pack_start(GTK_BOX(menubox), out, TRUE, TRUE, 0);
	box_prepend(menubox, write);
	/* box_prepend (menubox, exit); */

	GtkWidget *statusbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	/* gtk_widget_set_size_request (statusbox, -1, 20); */
	box_append(statusbox, coord_display);
	box_prepend(statusbox, status);

	GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
	/* TODO: Use screen resolution instead of using fixed values. */
	gtk_widget_set_size_request(scroll, -1, 512);
	gtk_container_add(GTK_CONTAINER(scroll), drawable_area);

	GtkWidget *mainbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), mainbox);
	box_prepend(mainbox, menubox);
	gtk_box_pack_start(GTK_BOX(mainbox), scroll, TRUE, TRUE, 0);
	box_prepend(mainbox, statusbox);

	gtk_widget_show_all(window);

	if (argc >= 2) {
		load_image(argv[1]);
	}

	gtk_main();

	return 0;
}
