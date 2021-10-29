#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "wlr-gamma-control-unstable-v1-client-protocol.h"

#define DEFAULT_TEMP  5600
#define MINIMUM_TEMP  1200
#define MAXIMUM_TEMP 20000

/* On SIGUSR1, the current temperature is multiplied by this value;
 * on SIGUSR2, the current temperature is divided by this value. */
#define STEP_MULTIPLIER 1.06

struct context;
struct output;
static const struct zwlr_gamma_control_v1_listener gamma_control_listener;
static const struct wl_registry_listener registry_listener;
static struct zwlr_gamma_control_manager_v1 *gamma_control_manager;
static int change_signal_fds[2];
static volatile int temp;
static double gamma_mod;

char* get_fifo_name(void);
void parse_input(char *input);
static int illuminant_d(double *x, double *y);
static int planckian_locus(double *x, double *y);
static double srgb_gamma(double value);
static double clamp(double value);
static void xyz_to_srgb(
	double x, double y, double z,
	double *r, double *g, double *b);
static void srgb_normalize(double *r, double *g, double *b);
void calc_whitepoint(double *rw, double *gw, double *bw);
static int create_anonymous_file(off_t size);
static int create_gamma_table(uint32_t ramp_size, uint16_t **table);
static void gamma_control_handle_gamma_size(
	void *data,
	struct zwlr_gamma_control_v1 *gamma_control,
	uint32_t ramp_size);
static void gamma_control_handle_failed(
	void *data,
	struct zwlr_gamma_control_v1 *gamma_control);
static void setup_output(struct output *output);
static void registry_handle_global(
	void *data,
	struct wl_registry *registry,
	uint32_t name,
	const char *interface,
	uint32_t version);
static void registry_handle_global_remove(
	void *data,
	struct wl_registry *registry,
	uint32_t name);
static void fill_gamma_table(
	uint16_t *table,
	uint32_t ramp_size,
	double rw, double gw, double bw);
static void set_temperature(struct wl_list *outputs);
static int display_dispatch(struct wl_display *display, int timeout);
static int wlrun(void);
void temp_increase(int ignored);
void temp_decrease(int ignored);
void calc_whitepoint(double *rw, double *gw, double *bw);
int main(int argc, char *argv[]);

struct context {
	bool new_output;
	struct wl_list outputs;
};

struct output {
	struct wl_list link;

	struct context *context;
	struct wl_output *wl_output;
	struct zwlr_gamma_control_v1 *gamma_control;

	int table_fd;
	uint32_t id;
	uint32_t ramp_size;
	uint16_t *table;
};

static const struct zwlr_gamma_control_v1_listener gamma_control_listener = {
	.gamma_size = gamma_control_handle_gamma_size,
	.failed = gamma_control_handle_failed,
};

static const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

static struct zwlr_gamma_control_manager_v1 *gamma_control_manager = NULL;

static const char usage[] = "usage: %s <temperature>\n";

/* Get filename for server FIFO;
 * first of XDG_RUNTIME_DIR, XDG_STATE_HOME, or HOME/.local/state,
 * in subdirectory redway, with FIFO named io;
 * the FIFO is created if it does not exist */
char* get_fifo_name(void) {
	static char name[FILENAME_MAX + 1] = { 0 };
	if (name[0])
		return &name[0];

	char sub_name_chars[] = "/.local/state/redway/io";
	char *dir_name = NULL;
	char *sub_name = NULL;
	int len = 0;

	dir_name = getenv("XDG_RUNTIME_DIR");
	if (!dir_name || !dir_name[0])
		dir_name = getenv("XDG_STATE_HOME");
	if (dir_name && dir_name[0]) {
		sub_name = strrchr(&sub_name_chars[0], 'r') - 1;
	} else {
		dir_name = getenv("HOME");
		sub_name = &sub_name_chars[0];
	}

	if (strlen(dir_name) + strlen(sub_name) > FILENAME_MAX)
		return NULL;

	len = strlen(dir_name);
	strncpy(&name[0], dir_name, len + 1);
	strncpy(&name[0] + len, sub_name, strlen(sub_name) + 1);

	/* ensure directory exists */
	char tree[FILENAME_MAX + 1] = { 0 };
	char *last_slash = NULL;
	strncpy(tree, name, strlen(name));
	last_slash = strrchr(tree, '/');
	/* remove /io */
	*last_slash = 0;
	struct stat fifo_stat;
	/* get parent directory that exists */
	int count = 0;
	while (stat(tree, &fifo_stat) == -1) {
		last_slash = strrchr(tree, '/');
		*last_slash = 0;
		count++;
	}
	/* build directory path */
	for (int i = 0; i < count; ++i) {
		*last_slash = '/';
		mkdir(tree, S_IRUSR | S_IWUSR | S_IXUSR);
		// TODO error handling
		last_slash = strchr(tree, 0);
	}

	/* create the FIFO */
	umask(0);
	mkfifo(name, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);
	// TODO error handling

	return &name[0];
}

void parse_input(char *input) {
	int offset = 0;
	switch (input[0]) {
	case '+':
		temp = (int)( (double)temp * STEP_MULTIPLIER);
		return;
	case '-':
		temp = (int)( (double)temp / STEP_MULTIPLIER);
		return;
	case '0': case '1': case '2': case '3': case '4': /* fall through */
	case '5': case '6': case '7': case '8': case '9': /* fall through */
		temp = atoi(input);
		return;
	case 't': case 'T': /* fall through */
		while (*(input + offset) < '0' || *(input + offset) > '9')
			offset++;
		printf("input: %d (%d)\n", atoi(input + offset), offset);
		printf("%s\n", input);
		temp = atoi(input + offset);
		return;
	default:
		return;
	}
}

/*
 * Illuminant D, or daylight locus, is is a "standard illuminant" used to
 * describe natural daylight. It is on this locus that D65, the whitepoint used
 * by most monitors and assumed here, is defined.
 *
 * This approximation is strictly speaking only well-defined between 4000K and
 * 25000K.
 */
static int illuminant_d(double *x, double *y) {
	// https://en.wikipedia.org/wiki/Standard_illuminant#Illuminant_series_D
	*x = 0.237040 +
		0.24748e3 / temp +
		1.9018e6 / pow(temp, 2) -
		2.0064e9 / pow(temp, 3);
	*y = (-3 * pow(*x, 2)) + (2.870 * (*x)) - 0.275;
	return 0;
}

/*
 * Planckian locus, or black body locus, describes the color of a black body at
 * a certain temperatures. This is not entirely equivalent to daylight due to
 * atmospheric effects.
 */
static int planckian_locus(double *x, double *y) {
	// https://en.wikipedia.org/wiki/Planckian_locus#Approximation
	// Customized to taste from values appropriate for < 4,000K.
	*x = -0.2661239e9 / pow(temp, 3) -
		0.2343589e6 / pow(temp, 2) +
		0.93e3 / temp + // originally 0.8776956e3
		0.179910;
	*y = -0.9549476 * pow(*x, 3) -
		1.37418593 * pow(*x, 2) +
		2.095 * (*x) -// originally 2.09137015
		0.16748867;
	return 0;
}

static double srgb_gamma(double value) {
	// https://en.wikipedia.org/wiki/SRGB
	if (value <= 0.0031308)
		return 12.92 * value;
	else
		return pow(1.055 * value, 1.0/2.2) - 0.055;
}

static double clamp(double value) {
	if (value > 1.0)
		return 1.0;
	else if (value < 0.0)
		return 0.0;
	else
		return value;
}

static void xyz_to_srgb(double x, double y, double z, double *r, double *g, double *b) {
	// http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
	*r = srgb_gamma(clamp(3.2404542 * x - 1.5371385 * y - 0.4985314 * z));
	*g = srgb_gamma(clamp(-0.9692660 * x + 1.8760108 * y + 0.0415560 * z));
	*b = srgb_gamma(clamp(0.0556434 * x - 0.2040259 * y + 1.0572252 * z));
}

static void srgb_normalize(double *r, double *g, double *b) {
	double maxw = fmaxl(*r, fmaxl(*g, *b));
	*r /= maxw;
	*g /= maxw;
	*b /= maxw;
}

void calc_whitepoint(double *rw, double *gw, double *bw) {
	if (temp == 6500) {
		*rw = *gw = *bw = 1.0;
		return;
	}

	double x = 1.0, y = 1.0;
	if (temp >= 6500)
		illuminant_d(&x, &y);
	else
		planckian_locus(&x, &y);
	double z = 1.0 - x - y;

	xyz_to_srgb(x, y, z, rw, gw, bw);
	srgb_normalize(rw, gw, bw);
}

static int create_anonymous_file(off_t size) {
	char template[] = "/tmp/redway-shared-XXXXXX";
	int fd = mkstemp(template);
	if (fd < 0)
		return -1;

	int ret;
	do {
		errno = 0;
		ret = ftruncate(fd, size);
	} while (errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}

	unlink(template);
	return fd;
}

static int create_gamma_table(uint32_t ramp_size, uint16_t **table) {
	size_t table_size = ramp_size * 3 * sizeof(uint16_t);
	int fd = create_anonymous_file(table_size);
	if (fd < 0) {
		fprintf(stderr, "failed to create anonymous file\n");
		return -1;
	}

	void *data = mmap(
		NULL,
		table_size,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		fd,
		0
	);
	if (data == MAP_FAILED) {
		fprintf(stderr, "failed to mmap()\n");
		close(fd);
		return -1;
	}

	*table = data;
	return fd;
}

static void gamma_control_handle_gamma_size(void *data,
		struct zwlr_gamma_control_v1 *gamma_control, uint32_t ramp_size) {
	(void)gamma_control;
	struct output *output = data;
	output->ramp_size = ramp_size;
	if (output->table_fd != -1)
		close(output->table_fd);
	output->table_fd = create_gamma_table(ramp_size, &output->table);
	output->context->new_output = true;
	if (output->table_fd < 0) {
		fprintf(
			stderr,
			"could not create gamma table for output %d\n",
			output->id
		);
		exit(EXIT_FAILURE);
	}
}

static void gamma_control_handle_failed(void *data,
		struct zwlr_gamma_control_v1 *gamma_control) {
	(void)gamma_control;
	struct output *output = data;
	fprintf(
		stderr,
		"gamma control of output %d failed\n",
		output->id
	);
	zwlr_gamma_control_v1_destroy(output->gamma_control);
	output->gamma_control = NULL;
	if (output->table_fd != -1) {
		close(output->table_fd);
		output->table_fd = -1;
	}
}

static void setup_output(struct output *output) {
	if (output->gamma_control != NULL)
		return;
	if (gamma_control_manager == NULL) {
		fprintf(
			stderr,
			"skipping setup of output %d: "
			"gamma_control_manager missing\n",
			output->id
		);
		return;
	}
	output->gamma_control = zwlr_gamma_control_manager_v1_get_gamma_control(
		gamma_control_manager,
		output->wl_output
	);
	zwlr_gamma_control_v1_add_listener(
		output->gamma_control,
		&gamma_control_listener,
		output
	);
}

static void registry_handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	(void)version;
	struct context *ctx = (struct context *)data;
	if (strcmp(interface, wl_output_interface.name) == 0) {
		fprintf(stderr, "registry: adding output %d\n", name);
		struct output *output = calloc(1, sizeof(struct output));
		output->id = name;
		output->wl_output = wl_registry_bind(
			registry,
			name,
			&wl_output_interface,
			1
		);
		output->table_fd = -1;
		output->context = ctx;
		wl_list_insert(&ctx->outputs, &output->link);
		setup_output(output);
	} else if (
		strcmp(
			interface,
			zwlr_gamma_control_manager_v1_interface.name
		) == 0
	) {
		gamma_control_manager = wl_registry_bind(
			registry,
			name,
			&zwlr_gamma_control_manager_v1_interface,
			1
		);
	}
}

static void registry_handle_global_remove(void *data,
		struct wl_registry *registry, uint32_t name) {
	(void)registry;
	struct context *ctx = (struct context *)data;
	struct output *output, *tmp;
	wl_list_for_each_safe(output, tmp, &ctx->outputs, link) {
		if (output->id == name) {
			fprintf(
				stderr,
				"registry: removing output %d\n",
				name
			);
			wl_list_remove(&output->link);
			if (output->gamma_control != NULL)
				zwlr_gamma_control_v1_destroy(
					output->gamma_control
				);
			if (output->table_fd != -1)
				close(output->table_fd);
			free(output);
			break;
		}
	}
}

static void fill_gamma_table(uint16_t *table, uint32_t ramp_size, double rw,
		double gw, double bw) {
	uint16_t *r = table;
	uint16_t *g = table + ramp_size;
	uint16_t *b = table + 2 * ramp_size;
	for (uint32_t i = 0; i < ramp_size; ++i) {
		double val = (double)i / (ramp_size - 1);
		r[i] = (uint16_t)(UINT16_MAX * pow(val * rw, gamma_mod));
		g[i] = (uint16_t)(UINT16_MAX * pow(val * gw, gamma_mod));
		b[i] = (uint16_t)(UINT16_MAX * pow(val * bw, gamma_mod));
	}
}

static void set_temperature(struct wl_list *outputs) {
	double rw, gw, bw;
	calc_whitepoint(&rw, &gw, &bw);
	fprintf(stderr, "setting temperature to %d K\n", temp);

	struct output *output;
	wl_list_for_each(output, outputs, link) {
		if (
			output->gamma_control == NULL ||
			output->table_fd == -1
		) {
			continue;
		}
		fill_gamma_table(
			output->table,
			output->ramp_size,
			rw, gw, bw
		);
		lseek(output->table_fd, 0, SEEK_SET);
		zwlr_gamma_control_v1_set_gamma(
			output->gamma_control,
			output->table_fd
		);
	}
}

static int display_dispatch(struct wl_display *display, int timeout) {
	if (wl_display_prepare_read(display) == -1)
		return wl_display_dispatch_pending(display);

	struct pollfd pfd[2];
	pfd[0].fd = wl_display_get_fd(display);
	pfd[1].fd = change_signal_fds[0];

	pfd[0].events = POLLOUT;
	while (wl_display_flush(display) == -1) {
		if (errno != EAGAIN && errno != EPIPE) {
			wl_display_cancel_read(display);
			return -1;
		}

		// We only poll the wayland fd here
		while (poll(pfd, 1, timeout) == -1) {
			if (errno != EINTR) {
				wl_display_cancel_read(display);
				return -1;
			}
		}
	}

	pfd[0].events = POLLIN;
	pfd[1].events = POLLIN;
	while (poll(pfd, 2, timeout) == -1) {
		if (errno != EINTR) {
			wl_display_cancel_read(display);
			return -1;
		}
	}

	if (pfd[1].revents & POLLIN) {
		// Empty signal fd
		char input[PIPE_BUF];
		if (
			read(
				change_signal_fds[0],
				&input,
				sizeof input
			) == -1 &&
			errno != EAGAIN
		) {
			return -1;
		}
		parse_input(input);
	}

	if ((pfd[0].revents & POLLIN) == 0) {
		wl_display_cancel_read(display);
		return 0;
	}

	if (wl_display_read_events(display) == -1)
		return -1;

	return wl_display_dispatch_pending(display);
}

static int wlrun(void) {
	int set_temp = 0;
	struct context ctx;
	wl_list_init(&ctx.outputs);

	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, &ctx);
	wl_display_roundtrip(display);

	if (gamma_control_manager == NULL) {
		fprintf(
			stderr,
			"compositor doesn't support "
			"wlr-gamma-control-unstable-v1\n"
		);
		return EXIT_FAILURE;
	}

	struct output *output;
	wl_list_for_each(output, &ctx.outputs, link) {
		setup_output(output);
	}
	wl_display_roundtrip(display);

	set_temperature(&ctx.outputs);

	while (display_dispatch(display, -1) != -1) {
		if (temp < MINIMUM_TEMP)
			temp = MINIMUM_TEMP;
		else if (temp > MAXIMUM_TEMP)
			temp = MAXIMUM_TEMP;
		if (temp != set_temp) {
			set_temperature(&ctx.outputs);
			set_temp = temp;
		}
	}

	return EXIT_SUCCESS;
}

void temp_increase(int ignored) {
	if (ignored) {}
	write(change_signal_fds[1], "+\0", 2);
}
void temp_decrease(int ignored) {
	if (ignored) {}
	write(change_signal_fds[1], "-\0", 2);
}

int main(int argc, char *argv[]) {
	/* validate input */
	if (2 <= argc && ('0' > argv[1][0] || '9' < argv[1][0])) {
		fprintf(stderr, usage, basename(argv[0]));
		return 0;
	}
	if (2 == argc)
		temp = strtol(argv[1], NULL, 0);
	else
		temp = DEFAULT_TEMP;

	/* set gamma_mod */
	gamma_mod = 1.0;

	/* make/open FIFO */
	const char *fifo_name = get_fifo_name();

	/* signal handlers */
	change_signal_fds[0] = open(fifo_name, O_RDONLY | O_NONBLOCK);
	change_signal_fds[1] = open(fifo_name, O_WRONLY | O_NONBLOCK);
	struct sigaction increase = { .sa_handler = temp_increase };
	sigaction(SIGUSR1, &increase, NULL);
	struct sigaction decrease = { .sa_handler = temp_decrease };
	sigaction(SIGUSR2, &decrease, NULL);

	return wlrun();
}
