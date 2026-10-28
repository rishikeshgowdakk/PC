/*
 * PixelSprint: Race-Free Parallel Histogram Equalization with OpenMP
 *
 * BCS702 Parallel Computing mini project (Module 2 performance metrics and
 * Module 4 OpenMP).  This file intentionally keeps the algorithms small and
 * explicit so that the implementation can be explained in a viva.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -fopenmp src/pixelsprint.c -o pixelsprint
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <omp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef _OPENMP
#error "PixelSprint requires OpenMP. Compile with gcc -fopenmp."
#endif

/*
 * OpenMP 4.5 introduced array-section reductions.  The V5 implementation
 * below uses reduction(+:hist[:256]); older compilers must not pretend that
 * this variant was built or tested.
 */
#if _OPENMP < 201511
#error "PixelSprint requires OpenMP 4.5 or newer for reduction(+:hist[:256])."
#endif

#define MIN_DIMENSION 16
#define MAX_DIMENSION 8192
#define DEFAULT_DIMENSION 2048
#define HISTOGRAM_BINS 256
#define MAX_THREADS 256
#define PATH_BUFFER_SIZE 4096
#define BENCHMARK_REPETITIONS 7
#define MAX_BENCHMARK_ROWS 64
#define ASCII_BUCKETS 32
#define ASCII_BAR_WIDTH 40

typedef unsigned long long hist_t;

typedef struct {
    int width;
    int height;
    unsigned char *rgb; /* Interleaved R, G, B bytes. */
} Image;

typedef enum {
    VARIANT_SEQUENTIAL = 0,
    VARIANT_UNSAFE = 1,
    VARIANT_CRITICAL = 2,
    VARIANT_ATOMIC = 3,
    VARIANT_PRIVATE = 4,
    VARIANT_REDUCTION = 5
} HistVariant;

typedef enum {
    MODE_NORMAL = 0,
    MODE_DEMO = 1,
    MODE_BENCHMARK = 2
} RunMode;

typedef struct {
    RunMode mode;
    int size;
    bool size_set;
    int threads;
    bool threads_set;
    HistVariant variant;
    bool variant_set;
    char input_path[PATH_BUFFER_SIZE];
    bool input_set;
    char output_dir[PATH_BUFFER_SIZE];
    bool output_set;
    bool help;
} Options;

typedef struct {
    int requested_threads;
    int actual_threads;
    double median_ms;
    int differing_bins;
    hist_t total;
    bool all_match;
} HistogramMeasurement;

typedef struct {
    int variant;
    int requested_threads;
    int actual_threads;
    double median_ms;
    double speedup;
    double efficiency;
    bool has_speedup;
    bool efficiency_not_applicable;
    char correctness[48];
} BenchmarkRow;

static const char *const variant_cli_names[] = {
    "sequential", "unsafe", "critical", "atomic", "private", "reduction"
};

static const char *const variant_labels[] = {
    "V0 sequential", "V1 unsafe", "V2 critical",
    "V3 atomic", "V4 private", "V5 reduction"
};

static const char *const variant_csv_names[] = {
    "sequential", "unsafe", "critical", "atomic", "private", "reduction"
};

/* ------------------------------------------------------------------------- */
/* Small utilities                                                           */
/* ------------------------------------------------------------------------- */

static bool checked_pixel_count(int width, int height, size_t *pixel_count)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return false;
    }

    size_t count = (size_t)width * (size_t)height;
    if (count > SIZE_MAX / 3U) {
        return false;
    }

    *pixel_count = count;
    return true;
}

static bool parse_positive_int(const char *text, long maximum, int *value)
{
    if (text == NULL || *text == '\0') {
        return false;
    }

    errno = 0;
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }
    if (parsed <= 0 || parsed > maximum) {
        return false;
    }

    *value = (int)parsed;
    return true;
}

static bool parse_dimension(const char *text, int *value)
{
    if (!parse_positive_int(text, MAX_DIMENSION, value)) {
        return false;
    }
    return *value >= MIN_DIMENSION;
}

static bool parse_thread_count(const char *text, int *value)
{
    return parse_positive_int(text, MAX_THREADS, value);
}

static bool variant_from_name(const char *name, HistVariant *variant)
{
    for (int i = 0; i <= VARIANT_REDUCTION; i++) {
        if (strcmp(name, variant_cli_names[i]) == 0) {
            *variant = (HistVariant)i;
            return true;
        }
    }
    return false;
}

static bool join_path(char *destination, size_t capacity,
                      const char *directory, const char *file_name)
{
    int written = snprintf(destination, capacity, "%s/%s", directory, file_name);
    return written >= 0 && (size_t)written < capacity;
}

static bool ensure_output_directory(const char *path)
{
    struct stat status;
    if (stat(path, &status) == 0) {
        if (!S_ISDIR(status.st_mode)) {
            fprintf(stderr, "Error: output path '%s' exists but is not a directory.\n", path);
            return false;
        }
        return true;
    }

    if (errno != ENOENT) {
        fprintf(stderr, "Error: cannot inspect output directory '%s': %s\n",
                path, strerror(errno));
        return false;
    }

    if (mkdir(path, 0777) != 0) {
        fprintf(stderr, "Error: cannot create output directory '%s': %s\n",
                path, strerror(errno));
        return false;
    }
    return true;
}

static const char *openmp_version_name(int macro_value)
{
    switch (macro_value) {
    case 200505: return "2.5";
    case 200805: return "3.0";
    case 201107: return "3.1";
    case 201307: return "4.0";
    case 201511: return "4.5";
    case 201811: return "5.0";
    case 202011: return "5.1";
    case 202111: return "5.2";
    default: return "unknown";
    }
}

static bool read_cpu_model(char *buffer, size_t capacity)
{
    FILE *file = fopen("/proc/cpuinfo", "r");
    if (file == NULL) {
        return false;
    }

    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon == NULL) {
                continue;
            }
            char *start = colon + 1;
            while (*start == ' ' || *start == '\t') {
                start++;
            }
            char *end = start + strlen(start);
            while (end > start &&
                   (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ')) {
                end--;
            }
            *end = '\0';
            if (*start != '\0') {
                snprintf(buffer, capacity, "%s", start);
                found = true;
            }
            break;
        }
    }

    if (fclose(file) != 0 && found) {
        return false;
    }
    return found;
}

static int detect_actual_threads(int requested_threads)
{
    int actual_threads = 1;

    #pragma omp parallel num_threads(requested_threads)
    {
        #pragma omp single
        actual_threads = omp_get_num_threads();
    }

    return actual_threads > 0 ? actual_threads : 1;
}

static void print_environment(int width, int height, int runtime_max_threads,
                              const char *mode_text, const char *variant_text,
                              int requested_threads, int actual_threads)
{
    char cpu_model[256];
    if (!read_cpu_model(cpu_model, sizeof(cpu_model))) {
        snprintf(cpu_model, sizeof(cpu_model), "unavailable");
    }

    char openmp_text[64];
    snprintf(openmp_text, sizeof(openmp_text), "OpenMP %s (macro %d)",
             openmp_version_name(_OPENMP), _OPENMP);

    printf("PixelSprint environment\n");
    printf("  CPU model              : %s\n", cpu_model);
    printf("  Logical processors     : %d\n", omp_get_num_procs());
    printf("  Runtime max OpenMP thr : %d\n", runtime_max_threads);
    printf("  GCC version            : %s\n", __VERSION__);
    printf("  OpenMP support         : %s\n", openmp_text);
    printf("  Image dimensions       : %d x %d\n", width, height);
    printf("  Mode                   : %s\n", mode_text);
    printf("  Histogram variant      : %s\n", variant_text);
    if (requested_threads > 0) {
        printf("  Requested threads      : %d\n", requested_threads);
        printf("  Actual threads used    : %d\n", actual_threads);
        if (actual_threads != requested_threads) {
            printf("  Note: the OpenMP runtime provided %d threads, not %d.\n",
                   actual_threads, requested_threads);
        }
    }
    printf("\n");
}

/* ------------------------------------------------------------------------- */
/* Image generation and PPM input                                            */
/* ------------------------------------------------------------------------- */

static bool allocate_image(Image *image, int width, int height)
{
    size_t pixel_count = 0;
    if (!checked_pixel_count(width, height, &pixel_count)) {
        fprintf(stderr, "Error: invalid image dimensions %d x %d.\n", width, height);
        return false;
    }

    unsigned char *rgb = (unsigned char *)calloc(pixel_count, 3U);
    if (rgb == NULL) {
        fprintf(stderr,
                "Error: cannot allocate %zu bytes for a %d x %d RGB image.\n",
                pixel_count * 3U, width, height);
        return false;
    }

    image->width = width;
    image->height = height;
    image->rgb = rgb;
    return true;
}

static void free_image(Image *image)
{
    free(image->rgb);
    image->rgb = NULL;
    image->width = 0;
    image->height = 0;
}

static uint32_t xorshift32(uint32_t *state)
{
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

/*
 * Generate a deterministic low-contrast image.  The RGB channels deliberately
 * carry the same value, so the grayscale conversion is exact and the target
 * range (approximately 80..160) is easy to reason about.
 */
static bool generate_image(Image *image, int size)
{
    if (size < MIN_DIMENSION || size > MAX_DIMENSION) {
        fprintf(stderr, "Error: generated image size must be between %d and %d.\n",
                MIN_DIMENSION, MAX_DIMENSION);
        return false;
    }
    if (!allocate_image(image, size, size)) {
        return false;
    }

    uint32_t random_state = 0x13579BDFu;
    int width = image->width;
    int height = image->height;
    int center1_x = width * 3 / 10;
    int center1_y = height * 3 / 10;
    int center2_x = width * 7 / 10;
    int center2_y = height * 4 / 10;
    int center3_x = width * 5 / 10;
    int center3_y = height * 7 / 10;
    int radius1 = width / 8;
    int radius2 = width / 7;
    int radius3 = width / 5;
    long long radius1_sq = (long long)radius1 * radius1;
    long long radius2_sq = (long long)radius2 * radius2;
    long long radius3_sq = (long long)radius3 * radius3;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int gradient = 96 + (x * 32) / (width - 1) + (y * 16) / (height - 1);
            int shape_delta = 0;

            long long dx = (long long)x - center1_x;
            long long dy = (long long)y - center1_y;
            if (dx * dx + dy * dy <= radius1_sq) {
                shape_delta += 18;
            }

            dx = (long long)x - center2_x;
            dy = (long long)y - center2_y;
            if (dx * dx + dy * dy <= radius2_sq) {
                shape_delta -= 20;
            }

            dx = (long long)x - center3_x;
            dy = (long long)y - center3_y;
            if (dx * dx + dy * dy <= radius3_sq) {
                shape_delta += 12;
            }

            /* A narrow diagonal band adds another simple geometric feature. */
            if (x + y > width && x + y < width + width / 12) {
                shape_delta -= 10;
            }

            int noise = (int)(xorshift32(&random_state) % 13U) - 6;
            int gray = gradient + shape_delta + noise;
            if (gray < 80) {
                gray = 80;
            } else if (gray > 160) {
                gray = 160;
            }

            size_t index = ((size_t)y * (size_t)width + (size_t)x) * 3U;
            image->rgb[index + 0] = (unsigned char)gray;
            image->rgb[index + 1] = (unsigned char)gray;
            image->rgb[index + 2] = (unsigned char)gray;
        }
    }

    return true;
}

/*
 * Read one PPM header token.  Whitespace and '#' comments are skipped before
 * the token.  The delimiter after the token is pushed back so that the caller
 * can enforce the single whitespace character that must follow maxval.
 */
static bool read_ppm_token(FILE *file, char *buffer, size_t capacity)
{
    int character;
    for (;;) {
        character = fgetc(file);
        if (character == EOF) {
            return false;
        }
        if (isspace((unsigned char)character)) {
            continue;
        }
        if (character == '#') {
            while ((character = fgetc(file)) != EOF && character != '\n') {
                /* Discard the comment. */
            }
            continue;
        }
        break;
    }

    size_t length = 0;
    while (character != EOF &&
           !isspace((unsigned char)character) &&
           character != '#') {
        if (length + 1U >= capacity) {
            return false;
        }
        buffer[length++] = (char)character;
        character = fgetc(file);
    }

    if (character != EOF) {
        if (ungetc(character, file) == EOF) {
            return false;
        }
    }
    buffer[length] = '\0';
    return length > 0;
}

static bool load_ppm_image(const char *path, Image *image)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: cannot open input file '%s': %s\n",
                path, strerror(errno));
        return false;
    }

    bool success = false;
    char token[64];
    int width = 0;
    int height = 0;
    int maximum_value = 0;
    unsigned char *rgb = NULL;

    if (!read_ppm_token(file, token, sizeof(token)) || strcmp(token, "P6") != 0) {
        fprintf(stderr, "Error: '%s' is not a binary PPM P6 file.\n", path);
        goto cleanup;
    }
    if (!read_ppm_token(file, token, sizeof(token)) ||
        !parse_positive_int(token, MAX_DIMENSION, &width)) {
        fprintf(stderr, "Error: invalid PPM width in '%s' (allowed 1..%d).\n",
                path, MAX_DIMENSION);
        goto cleanup;
    }
    if (!read_ppm_token(file, token, sizeof(token)) ||
        !parse_positive_int(token, MAX_DIMENSION, &height)) {
        fprintf(stderr, "Error: invalid PPM height in '%s' (allowed 1..%d).\n",
                path, MAX_DIMENSION);
        goto cleanup;
    }
    if (!read_ppm_token(file, token, sizeof(token)) ||
        !parse_positive_int(token, 255, &maximum_value) || maximum_value != 255) {
        fprintf(stderr, "Error: '%s' must use PPM maximum channel value 255.\n", path);
        goto cleanup;
    }

    /* The P6 format has exactly one whitespace byte after the maximum value. */
    int separator = fgetc(file);
    if (separator == EOF || !isspace((unsigned char)separator)) {
        fprintf(stderr, "Error: PPM header in '%s' is missing its final separator.\n", path);
        goto cleanup;
    }

    size_t pixel_count = 0;
    if (!checked_pixel_count(width, height, &pixel_count)) {
        fprintf(stderr, "Error: PPM dimensions %d x %d are too large or overflow.\n",
                width, height);
        goto cleanup;
    }

    long header_end = ftell(file);
    if (header_end < 0) {
        fprintf(stderr, "Error: cannot determine PPM data position: %s\n", strerror(errno));
        goto cleanup;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error: cannot inspect PPM file length: %s\n", strerror(errno));
        goto cleanup;
    }
    long file_size = ftell(file);
    if (file_size < 0) {
        fprintf(stderr, "Error: cannot determine PPM file length: %s\n", strerror(errno));
        goto cleanup;
    }
    if (fseek(file, header_end, SEEK_SET) != 0) {
        fprintf(stderr, "Error: cannot return to PPM raster data: %s\n", strerror(errno));
        goto cleanup;
    }

    unsigned long long required_bytes = (unsigned long long)pixel_count * 3ULL;
    long long remaining_bytes = (long long)file_size - (long long)header_end;
    if (remaining_bytes != (long long)required_bytes) {
        fprintf(stderr,
                "Error: PPM raster length mismatch in '%s': expected %llu bytes, found %lld.\n",
                path, required_bytes, remaining_bytes);
        goto cleanup;
    }

    if (!allocate_image(image, width, height)) {
        goto cleanup;
    }
    rgb = image->rgb;

    size_t bytes_read = fread(rgb, 1U, (size_t)required_bytes, file);
    if (bytes_read != (size_t)required_bytes) {
        fprintf(stderr, "Error: short read from PPM file '%s' (read %zu of %llu bytes).\n",
                path, bytes_read, required_bytes);
        free_image(image);
        rgb = NULL;
        goto cleanup;
    }
    if (ferror(file)) {
        fprintf(stderr, "Error: read failure in PPM file '%s'.\n", path);
        free_image(image);
        rgb = NULL;
        goto cleanup;
    }

    success = true;

cleanup:
    if (fclose(file) != 0 && success) {
        fprintf(stderr, "Error: could not close input file '%s': %s\n",
                path, strerror(errno));
        if (rgb != NULL) {
            free_image(image);
        }
        success = false;
    }
    return success;
}

static void convert_to_grayscale(const Image *image, unsigned char *gray,
                                 size_t pixel_count)
{
    for (size_t i = 0; i < pixel_count; i++) {
        unsigned int red = image->rgb[i * 3U + 0U];
        unsigned int green = image->rgb[i * 3U + 1U];
        unsigned int blue = image->rgb[i * 3U + 2U];
        gray[i] = (unsigned char)((299U * red + 587U * green + 114U * blue) / 1000U);
    }
}

/* ------------------------------------------------------------------------- */
/* Histogram implementations                                                 */
/* ------------------------------------------------------------------------- */

/*
 * V0: sequential reference.  The destination histogram is zeroed by the
 * caller before the timer/operation, and only one thread performs every
 * increment, so this loop is race-free.
 */
static void histogram_sequential(const unsigned char *gray, size_t pixel_count,
                                 hist_t *histogram)
{
    for (size_t i = 0; i < pixel_count; i++) {
        histogram[gray[i]]++;
    }
}

/*
 * V1: INTENTIONALLY UNSAFE shared histogram.
 *
 * Multiple OpenMP threads increment the same shared counters without any
 * synchronization.  hist[gray[i]]++ is a read-modify-write operation, so two
 * threads can read the same old count and overwrite each other's update.
 * This variant exists only to demonstrate a data race; it must never be used
 * for equalization or described as a correct speedup.
 */
static void histogram_unsafe(const unsigned char *gray, size_t pixel_count,
                             hist_t *histogram)
{
    /*
     * WARNING: several threads can update the same bin at the same time.
     * Each increment is a read-modify-write, so concurrent updates can be
     * lost.  This is deliberately left unprotected for the demonstration.
     */
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < pixel_count; i++) {
        histogram[gray[i]]++; /* INTENTIONALLY UNSAFE: no protection. */
    }
}

/*
 * V2: critical section.  The critical region protects the read-modify-write
 * operation from concurrent updates, so the histogram is correct.  However,
 * all threads must take the same lock for every pixel, which serializes the
 * counting work and can be slow when contention is high.
 */
static void histogram_critical(const unsigned char *gray, size_t pixel_count,
                               hist_t *histogram)
{
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < pixel_count; i++) {
        #pragma omp critical
        {
            histogram[gray[i]]++;
        }
    }
}

/*
 * V3: atomic update.  The atomic directive makes each individual increment
 * indivisible, so no increments are lost.  The whole loop body is not a
 * critical section, but many pixels with the same intensity still contend
 * for the same cache line and atomic operation.
 */
static void histogram_atomic(const unsigned char *gray, size_t pixel_count,
                             hist_t *histogram)
{
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < pixel_count; i++) {
        #pragma omp atomic
        histogram[gray[i]]++;
    }
}

/*
 * V4: one private 256-bin histogram per actual OpenMP thread.  A thread only
 * writes its own region of the preallocated workspace, so pixel counting has
 * no shared-counter race.  After the parallel region, one thread merges the
 * private histograms sequentially into the caller's histogram.  The merge is
 * race-free because it is outside the parallel region.  The workspace and its
 * zeroing are prepared outside the timed counting step by the caller.
 */
static void histogram_private(const unsigned char *gray, size_t pixel_count,
                              hist_t *histogram, hist_t *workspace,
                              int workspace_threads, int *actual_threads)
{
    *actual_threads = 1;

    #pragma omp parallel num_threads(workspace_threads)
    {
        int thread_id = omp_get_thread_num();
        hist_t *private_histogram =
            workspace + (size_t)thread_id * (size_t)HISTOGRAM_BINS;

        #pragma omp single
        *actual_threads = omp_get_num_threads();

        #pragma omp for schedule(static)
        for (size_t i = 0; i < pixel_count; i++) {
            private_histogram[gray[i]]++;
        }
    }

    /* Sequential merge: no other thread is touching histogram here. */
    for (int thread = 0; thread < *actual_threads; thread++) {
        const hist_t *private_histogram =
            workspace + (size_t)thread * (size_t)HISTOGRAM_BINS;
        for (int bin = 0; bin < HISTOGRAM_BINS; bin++) {
            histogram[bin] += private_histogram[bin];
        }
    }
}

/*
 * V5: OpenMP array-section reduction.  Each thread receives a private 256-bin
 * copy, increments it without contention, and the runtime combines the copies
 * with an addition reduction.  The destination histogram must be zeroed by
 * the caller before the reduction.  This construct needs OpenMP 4.5 (checked
 * at the top of this file) and a compiler that supports array reductions.
 */
static void histogram_reduction(const unsigned char *gray, size_t pixel_count,
                                hist_t *histogram)
{
    #pragma omp parallel for schedule(static) reduction(+:histogram[:HISTOGRAM_BINS])
    for (size_t i = 0; i < pixel_count; i++) {
        histogram[gray[i]]++;
    }
}

static void run_histogram_once(HistVariant variant, const unsigned char *gray,
                               size_t pixel_count, hist_t *histogram,
                               hist_t *workspace, int workspace_threads,
                               int *actual_threads)
{
    switch (variant) {
    case VARIANT_SEQUENTIAL:
        histogram_sequential(gray, pixel_count, histogram);
        break;
    case VARIANT_UNSAFE:
        histogram_unsafe(gray, pixel_count, histogram);
        break;
    case VARIANT_CRITICAL:
        histogram_critical(gray, pixel_count, histogram);
        break;
    case VARIANT_ATOMIC:
        histogram_atomic(gray, pixel_count, histogram);
        break;
    case VARIANT_PRIVATE:
        histogram_private(gray, pixel_count, histogram, workspace,
                          workspace_threads, actual_threads);
        break;
    case VARIANT_REDUCTION:
        histogram_reduction(gray, pixel_count, histogram);
        break;
    }
}

static double timed_histogram_once(HistVariant variant, const unsigned char *gray,
                                   size_t pixel_count, hist_t *histogram,
                                   hist_t *workspace, int workspace_threads,
                                   int *actual_threads)
{
    double start = omp_get_wtime();
    run_histogram_once(variant, gray, pixel_count, histogram, workspace,
                       workspace_threads, actual_threads);
    double end = omp_get_wtime();
    return (end - start) * 1000.0;
}

static int compare_histograms(const hist_t *reference, const hist_t *candidate,
                              size_t pixel_count, int *different_bins,
                              hist_t *candidate_total)
{
    int differences = 0;
    hist_t total = 0;

    for (int bin = 0; bin < HISTOGRAM_BINS; bin++) {
        total += candidate[bin];
        if (candidate[bin] != reference[bin]) {
            differences++;
        }
    }

    *different_bins = differences;
    *candidate_total = total;
    return differences == 0 && total == (hist_t)pixel_count;
}

/* ------------------------------------------------------------------------- */
/* Equalization, ASCII histograms, and BMP output                            */
/* ------------------------------------------------------------------------- */

/*
 * The CDF is inherently sequential: cdf[v] depends on cdf[v - 1].  There are
 * only 256 bins, so this tiny loop is not worth parallelizing in this project.
 */
static bool build_lookup_table(const hist_t *histogram,
                               unsigned long long total_pixels,
                               unsigned char lookup[256], bool *identity_mapping)
{
    hist_t cdf[HISTOGRAM_BINS];
    hist_t running = 0;

    for (int bin = 0; bin < HISTOGRAM_BINS; bin++) {
        running += histogram[bin];
        cdf[bin] = running;
    }

    if (total_pixels == 0) {
        fprintf(stderr, "Error: cannot equalize an image with zero pixels.\n");
        return false;
    }
    if (cdf[HISTOGRAM_BINS - 1] != total_pixels) {
        fprintf(stderr,
                "Error: histogram total %llu does not match image pixel count %llu.\n",
                cdf[HISTOGRAM_BINS - 1], total_pixels);
        return false;
    }

    int first_nonzero_bin = 0;
    while (first_nonzero_bin < HISTOGRAM_BINS &&
           histogram[first_nonzero_bin] == 0) {
        first_nonzero_bin++;
    }
    if (first_nonzero_bin == HISTOGRAM_BINS) {
        fprintf(stderr, "Error: histogram is empty; cannot build a lookup table.\n");
        return false;
    }

    hist_t cdf_min = cdf[first_nonzero_bin];
    unsigned long long denominator = total_pixels - cdf_min;

    if (denominator == 0) {
        /*
         * All pixels have the same intensity (or only one intensity occurs).
         * The formula divides by zero in this case, so preserve the original
         * grayscale values.  This is the documented uniform-image behavior.
         */
        for (int value = 0; value < HISTOGRAM_BINS; value++) {
            lookup[value] = (unsigned char)value;
        }
        *identity_mapping = true;
        return true;
    }

    *identity_mapping = false;
    for (int value = 0; value < HISTOGRAM_BINS; value++) {
        /* Check before subtracting: cdf/v and cdf_min are unsigned values. */
        unsigned long long shifted =
            cdf[value] >= cdf_min ? cdf[value] - cdf_min : 0ULL;
        double scaled = ((double)shifted * 255.0) / (double)denominator;

        if (scaled < 0.0) {
            scaled = 0.0;
        } else if (scaled > 255.0) {
            scaled = 255.0;
        }

        int rounded = (int)(scaled + 0.5); /* Consistent round-half-up rule. */
        if (rounded < 0) {
            rounded = 0;
        } else if (rounded > 255) {
            rounded = 255;
        }
        lookup[value] = (unsigned char)rounded;
    }

    return true;
}

/*
 * Each iteration reads its own input pixel, reads the read-only lookup table,
 * and writes a distinct output pixel.  There is no shared writable state, so
 * this loop is race-free.
 */
static void apply_lookup_table(const unsigned char *input, unsigned char *output,
                               size_t pixel_count, const unsigned char lookup[256])
{
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < pixel_count; i++) {
        output[i] = lookup[input[i]];
    }
}

static void print_intensity_statistics(const char *label,
                                       const unsigned char *pixels,
                                       size_t pixel_count)
{
    unsigned int minimum = 255;
    unsigned int maximum = 0;
    unsigned long long sum = 0;

    for (size_t i = 0; i < pixel_count; i++) {
        if (pixels[i] < minimum) {
            minimum = pixels[i];
        }
        if (pixels[i] > maximum) {
            maximum = pixels[i];
        }
        sum += pixels[i];
    }

    double mean = pixel_count > 0 ? (double)sum / (double)pixel_count : 0.0;
    printf("%s: min=%u max=%u range=%u mean=%.2f\n", label, minimum, maximum,
           maximum - minimum, mean);
}

static void print_ascii_histogram(const char *title, const unsigned char *pixels,
                                  size_t pixel_count)
{
    hist_t buckets[ASCII_BUCKETS];
    memset(buckets, 0, sizeof(buckets));

    for (size_t i = 0; i < pixel_count; i++) {
        buckets[pixels[i] / 8]++;
    }

    hist_t maximum_count = 0;
    for (int bucket = 0; bucket < ASCII_BUCKETS; bucket++) {
        if (buckets[bucket] > maximum_count) {
            maximum_count = buckets[bucket];
        }
    }

    printf("\n%s\n", title);
    printf(" range       count  bar\n");
    for (int bucket = 0; bucket < ASCII_BUCKETS; bucket++) {
        int low = bucket * 8;
        int high = low + 7;
        int bar_length = 0;
        if (maximum_count > 0) {
            bar_length = (int)((buckets[bucket] * (hist_t)ASCII_BAR_WIDTH) /
                               maximum_count);
        }
        if (buckets[bucket] > 0 && bar_length == 0) {
            bar_length = 1;
        }

        printf("%3d-%3d %10llu | ", low, high, buckets[bucket]);
        for (int bar = 0; bar < bar_length; bar++) {
            putchar('#');
        }
        putchar('\n');
    }
}

static void put_u16_le(unsigned char *destination, uint16_t value)
{
    destination[0] = (unsigned char)(value & 0xFFU);
    destination[1] = (unsigned char)((value >> 8) & 0xFFU);
}

static void put_u32_le(unsigned char *destination, uint32_t value)
{
    destination[0] = (unsigned char)(value & 0xFFU);
    destination[1] = (unsigned char)((value >> 8) & 0xFFU);
    destination[2] = (unsigned char)((value >> 16) & 0xFFU);
    destination[3] = (unsigned char)((value >> 24) & 0xFFU);
}

/*
 * Write an uncompressed 24-bit BMP.  The grayscale value is duplicated into
 * B, G, and R.  BMP rows are padded to four bytes, stored bottom-up, and use
 * BGR byte order.  Header fields are written explicitly in little-endian form
 * so the file format does not depend on the host struct layout.
 */
static bool write_grayscale_bmp(const char *path, const unsigned char *pixels,
                                int width, int height)
{
    size_t pixel_count = 0;
    if (!checked_pixel_count(width, height, &pixel_count)) {
        fprintf(stderr, "Error: invalid BMP dimensions %d x %d.\n", width, height);
        return false;
    }
    (void)pixel_count;

    if ((size_t)width > (SIZE_MAX - 3U) / 3U) {
        fprintf(stderr, "Error: BMP row size overflows for width %d.\n", width);
        return false;
    }
    size_t row_stride = ((size_t)width * 3U + 3U) & ~(size_t)3U;
    if ((size_t)height > (SIZE_MAX - 54U) / row_stride) {
        fprintf(stderr, "Error: BMP file size overflows for %d x %d.\n", width, height);
        return false;
    }
    size_t image_size = row_stride * (size_t)height;
    size_t file_size = 54U + image_size;
    if (file_size > UINT32_MAX) {
        fprintf(stderr, "Error: BMP file would exceed the 32-bit BMP size limit.\n");
        return false;
    }

    unsigned char header[54];
    memset(header, 0, sizeof(header));

    header[0] = 'B';
    header[1] = 'M';
    put_u32_le(header + 2, (uint32_t)file_size);
    put_u32_le(header + 6, 0U); /* Reserved. */
    put_u32_le(header + 10, 54U); /* Pixel data offset. */

    put_u32_le(header + 14, 40U); /* BITMAPINFOHEADER size. */
    put_u32_le(header + 18, (uint32_t)width);
    put_u32_le(header + 22, (uint32_t)height);
    put_u16_le(header + 26, 1U); /* Planes. */
    put_u16_le(header + 28, 24U); /* Bits per pixel. */
    put_u32_le(header + 30, 0U); /* BI_RGB, no compression. */
    put_u32_le(header + 34, (uint32_t)image_size);
    put_u32_le(header + 38, 2835U); /* 72 DPI in pixels/metre, approximate. */
    put_u32_le(header + 42, 2835U);
    put_u32_le(header + 46, 0U); /* No palette. */
    put_u32_le(header + 50, 0U); /* All colours important. */

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "Error: cannot open BMP file '%s' for writing: %s\n",
                path, strerror(errno));
        return false;
    }

    bool success = false;
    unsigned char *row = NULL;

    if (fwrite(header, 1U, sizeof(header), file) != sizeof(header)) {
        fprintf(stderr, "Error: failed to write BMP header to '%s'.\n", path);
        goto cleanup;
    }

    row = (unsigned char *)calloc(row_stride, 1U);
    if (row == NULL) {
        fprintf(stderr, "Error: cannot allocate a BMP row buffer (%zu bytes).\n",
                row_stride);
        goto cleanup;
    }

    for (int y = height - 1; y >= 0; y--) {
        memset(row, 0, row_stride);
        for (int x = 0; x < width; x++) {
            size_t source_index = (size_t)y * (size_t)width + (size_t)x;
            unsigned char gray = pixels[source_index];
            size_t destination_index = (size_t)x * 3U;
            row[destination_index + 0U] = gray; /* B */
            row[destination_index + 1U] = gray; /* G */
            row[destination_index + 2U] = gray; /* R */
        }
        if (fwrite(row, 1U, row_stride, file) != row_stride) {
            fprintf(stderr, "Error: failed to write BMP pixel data to '%s'.\n", path);
            goto cleanup;
        }
    }

    success = true;

cleanup:
    free(row);
    if (fclose(file) != 0 && success) {
        fprintf(stderr, "Error: failed to close BMP file '%s': %s\n",
                path, strerror(errno));
        success = false;
    }
    if (!success) {
        remove(path);
    }
    return success;
}

static int equalize_and_save(const Options *options, int width, int height,
                             const unsigned char *gray, size_t pixel_count,
                             const hist_t *histogram, const char *histogram_source)
{
    omp_set_num_threads(options->threads);

    printf("\nEqualization using the %s histogram\n", histogram_source);
    unsigned char lookup[256];
    bool identity_mapping = false;
    if (!build_lookup_table(histogram, (unsigned long long)pixel_count,
                            lookup, &identity_mapping)) {
        return 1;
    }
    if (identity_mapping) {
        printf("LUT note: the histogram has a single occupied intensity; the denominator is\n");
        printf("          zero, so the original grayscale values are preserved.\n");
    }

    unsigned char *equalized = (unsigned char *)malloc(pixel_count);
    if (equalized == NULL) {
        fprintf(stderr, "Error: cannot allocate %zu bytes for the equalized image.\n",
                pixel_count);
        return 1;
    }

    apply_lookup_table(gray, equalized, pixel_count, lookup);

    printf("\nContrast check\n");
    print_intensity_statistics("  Before", gray, pixel_count);
    print_intensity_statistics("  After ", equalized, pixel_count);

    print_ascii_histogram("Histogram before equalization (32 buckets)",
                          gray, pixel_count);
    print_ascii_histogram("Histogram after equalization (32 buckets)",
                          equalized, pixel_count);

    char before_path[PATH_BUFFER_SIZE];
    char after_path[PATH_BUFFER_SIZE];
    if (!join_path(before_path, sizeof(before_path), options->output_dir,
                   "before.bmp") ||
        !join_path(after_path, sizeof(after_path), options->output_dir,
                   "after.bmp")) {
        fprintf(stderr, "Error: output path is too long.\n");
        free(equalized);
        return 1;
    }

    if (!write_grayscale_bmp(before_path, gray, width, height)) {
        free(equalized);
        return 1;
    }
    if (!write_grayscale_bmp(after_path, equalized, width, height)) {
        free(equalized);
        return 1;
    }

    printf("\nSaved grayscale images\n");
    printf("  %s\n", before_path);
    printf("  %s\n", after_path);

    free(equalized);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Modes                                                                     */
/* ------------------------------------------------------------------------- */

static void print_v1_outcome(const char *prefix, int attempt,
                             const hist_t *reference,
                             const hist_t *unsafe_histogram,
                             size_t pixel_count)
{
    int different_bins = 0;
    hist_t total = 0;
    bool matches = compare_histograms(reference, unsafe_histogram, pixel_count,
                                      &different_bins, &total);

    if (attempt > 0) {
        printf("  Attempt %d: pixels=%zu total=%llu differing_bins=%d",
               attempt, pixel_count, total, different_bins);
    } else {
        printf("%s: pixels=%zu total=%llu differing_bins=%d",
               prefix, pixel_count, total, different_bins);
    }

    if (matches) {
        printf(" status=NO RACE OBSERVED THIS RUN — RACES ARE TIMING-DEPENDENT\n");
        return;
    }

    printf(" status=RACE OBSERVED");
    if (total < (hist_t)pixel_count) {
        printf(", lost=%llu", (hist_t)pixel_count - total);
    } else if (total > (hist_t)pixel_count) {
        printf(", excess=%llu", total - (hist_t)pixel_count);
    } else {
        printf(", total_matches_pixel_count (differing bins only; lost counts not inferred)");
    }
    printf("\n");
}

static int run_demo(const Options *options, int width, int height,
                    const unsigned char *gray, size_t pixel_count,
                    int runtime_max_threads)
{
    omp_set_num_threads(options->threads);
    int actual_threads = detect_actual_threads(options->threads);

    print_environment(width, height, runtime_max_threads, "demo",
                      variant_labels[options->variant], options->threads,
                      actual_threads);

    hist_t histograms[6][HISTOGRAM_BINS];
    memset(histograms, 0, sizeof(histograms));

    /* V0 is the correctness reference. */
    histogram_sequential(gray, pixel_count, histograms[VARIANT_SEQUENTIAL]);
    int reference_differences = 0;
    hist_t reference_total = 0;
    bool reference_ok = compare_histograms(histograms[VARIANT_SEQUENTIAL],
                                           histograms[VARIANT_SEQUENTIAL],
                                           pixel_count, &reference_differences,
                                           &reference_total);
    printf("V0 sequential reference: pixels=%zu total=%llu status=%s\n",
           pixel_count, reference_total, reference_ok ? "PASS" : "FAIL");
    if (!reference_ok) {
        fprintf(stderr, "Error: sequential reference histogram is inconsistent.\n");
        return 1;
    }

    printf("\nV1 unsafe shared histogram: UNSAFE — EXPECTED TO BE WRONG\n");
    printf("  The unprotected increments below can lose updates; a match is not proof of safety.\n");
    for (int attempt = 1; attempt <= 3; attempt++) {
        memset(histograms[VARIANT_UNSAFE], 0,
               sizeof(histograms[VARIANT_UNSAFE]));
        histogram_unsafe(gray, pixel_count, histograms[VARIANT_UNSAFE]);
        print_v1_outcome(NULL, attempt, histograms[VARIANT_SEQUENTIAL],
                         histograms[VARIANT_UNSAFE], pixel_count);
    }

    bool protected_failure = false;
    for (int variant = VARIANT_CRITICAL; variant <= VARIANT_REDUCTION; variant++) {
        hist_t *histogram = histograms[variant];
        memset(histogram, 0, sizeof(histograms[variant]));

        int variant_threads = actual_threads;
        hist_t *workspace = NULL;
        int workspace_threads = 0;

        if (variant == VARIANT_PRIVATE) {
            workspace_threads = omp_get_max_threads();
            if (workspace_threads < 1) {
                workspace_threads = 1;
            }
            workspace = (hist_t *)calloc(
                (size_t)workspace_threads * (size_t)HISTOGRAM_BINS,
                sizeof(hist_t));
            if (workspace == NULL) {
                fprintf(stderr,
                        "Error: cannot allocate private histogram workspace (%d threads).\n",
                        workspace_threads);
                return 1;
            }
        }

        run_histogram_once((HistVariant)variant, gray, pixel_count, histogram,
                           workspace, workspace_threads, &variant_threads);
        free(workspace);

        int different_bins = 0;
        hist_t total = 0;
        bool matches = compare_histograms(histograms[VARIANT_SEQUENTIAL],
                                          histogram, pixel_count,
                                          &different_bins, &total);
        printf("%s: pixels=%zu total=%llu differing_bins=%d actual_threads=%d status=%s\n",
               variant_labels[variant], pixel_count, total, different_bins,
               variant_threads, matches ? "PASS" : "FAIL");
        if (!matches) {
            protected_failure = true;
        }
    }

    if (protected_failure) {
        fprintf(stderr,
                "\nError: at least one protected histogram variant failed; images were not written.\n");
        return 1;
    }

    hist_t chosen[HISTOGRAM_BINS];
    const char *source = variant_labels[options->variant];
    if (options->variant == VARIANT_UNSAFE) {
        fprintf(stderr,
                "\nError: the unsafe histogram is not thread-safe and will not be used for equalization.\n");
        fprintf(stderr,
                "Falling back to the sequential reference (V0) for the equalization histogram.\n");
        memcpy(chosen, histograms[VARIANT_SEQUENTIAL], sizeof(chosen));
        source = "V0 sequential fallback";
    } else {
        memcpy(chosen, histograms[options->variant], sizeof(chosen));
    }

    return equalize_and_save(options, width, height, gray, pixel_count,
                             chosen, source);
}

static int run_normal(const Options *options, int width, int height,
                      const unsigned char *gray, size_t pixel_count,
                      int runtime_max_threads)
{
    omp_set_num_threads(options->threads);
    int actual_threads = detect_actual_threads(options->threads);

    print_environment(width, height, runtime_max_threads, "single image processing",
                      variant_labels[options->variant], options->threads,
                      actual_threads);

    hist_t reference[HISTOGRAM_BINS];
    hist_t selected[HISTOGRAM_BINS];
    memset(reference, 0, sizeof(reference));
    memset(selected, 0, sizeof(selected));

    histogram_sequential(gray, pixel_count, reference);
    int reference_differences = 0;
    hist_t reference_total = 0;
    bool reference_ok = compare_histograms(reference, reference, pixel_count,
                                           &reference_differences,
                                           &reference_total);
    printf("%s: pixels=%zu total=%llu status=%s\n",
           variant_labels[VARIANT_SEQUENTIAL], pixel_count, reference_total,
           reference_ok ? "PASS" : "FAIL");
    if (!reference_ok) {
        fprintf(stderr, "Error: sequential reference histogram is inconsistent.\n");
        return 1;
    }

    const hist_t *chosen = reference;
    const char *source = "V0 sequential";

    if (options->variant == VARIANT_SEQUENTIAL) {
        printf("Selected variant is the sequential reference; no parallel histogram is run.\n");
    } else if (options->variant == VARIANT_UNSAFE) {
        printf("\nV1 unsafe shared histogram: UNSAFE — EXPECTED TO BE WRONG\n");
        histogram_unsafe(gray, pixel_count, selected);
        print_v1_outcome("V1 unsafe", 0, reference, selected, pixel_count);
        fprintf(stderr,
                "\nError: the unsafe histogram is not thread-safe and will not be used for equalization.\n");
        fprintf(stderr,
                "Falling back to the sequential reference (V0) for the equalization histogram.\n");
        source = "V0 sequential fallback";
    } else {
        hist_t *workspace = NULL;
        int workspace_threads = 0;
        int variant_threads = actual_threads;

        if (options->variant == VARIANT_PRIVATE) {
            workspace_threads = omp_get_max_threads();
            if (workspace_threads < 1) {
                workspace_threads = 1;
            }
            workspace = (hist_t *)calloc(
                (size_t)workspace_threads * (size_t)HISTOGRAM_BINS,
                sizeof(hist_t));
            if (workspace == NULL) {
                fprintf(stderr,
                        "Error: cannot allocate private histogram workspace (%d threads).\n",
                        workspace_threads);
                return 1;
            }
        }

        run_histogram_once(options->variant, gray, pixel_count, selected,
                           workspace, workspace_threads, &variant_threads);
        free(workspace);

        int different_bins = 0;
        hist_t total = 0;
        bool matches = compare_histograms(reference, selected, pixel_count,
                                          &different_bins, &total);
        printf("%s: pixels=%zu total=%llu differing_bins=%d actual_threads=%d status=%s\n",
               variant_labels[options->variant], pixel_count, total,
               different_bins, variant_threads, matches ? "PASS" : "FAIL");
        if (!matches) {
            fprintf(stderr,
                    "Error: selected protected histogram failed validation; images were not written.\n");
            return 1;
        }
        chosen = selected;
        source = variant_labels[options->variant];
    }

    return equalize_and_save(options, width, height, gray, pixel_count,
                             chosen, source);
}

static int compare_doubles(const void *left, const void *right)
{
    double a = *(const double *)left;
    double b = *(const double *)right;
    return (a > b) - (a < b);
}

static bool median_of_times(const double times[BENCHMARK_REPETITIONS],
                            double *median)
{
    double sorted[BENCHMARK_REPETITIONS];
    memcpy(sorted, times, sizeof(sorted));
    qsort(sorted, BENCHMARK_REPETITIONS, sizeof(sorted[0]), compare_doubles);
    *median = sorted[BENCHMARK_REPETITIONS / 2];
    return *median > 0.0;
}

static bool measure_histogram(HistVariant variant, const unsigned char *gray,
                              size_t pixel_count, const hist_t *reference,
                              int requested_threads, hist_t *workspace,
                              int workspace_threads, HistogramMeasurement *result)
{
    hist_t histogram[HISTOGRAM_BINS];
    double times[BENCHMARK_REPETITIONS];
    /* This helper region is outside all timers; it records the runtime's answer. */
    int actual_threads = detect_actual_threads(requested_threads);
    bool have_failure_details = false;

    memset(result, 0, sizeof(*result));
    result->requested_threads = requested_threads;
    result->all_match = true;

    /* One warm-up run, not included in the median. */
    memset(histogram, 0, sizeof(histogram));
    if (workspace != NULL) {
        memset(workspace, 0,
               (size_t)workspace_threads * (size_t)HISTOGRAM_BINS * sizeof(hist_t));
    }
    timed_histogram_once(variant, gray, pixel_count, histogram, workspace,
                         workspace_threads, &actual_threads);

    int different_bins = 0;
    hist_t total = 0;
    if (!compare_histograms(reference, histogram, pixel_count, &different_bins, &total)) {
        result->all_match = false;
        result->differing_bins = different_bins;
        result->total = total;
        have_failure_details = true;
    }

    for (int repetition = 0; repetition < BENCHMARK_REPETITIONS; repetition++) {
        memset(histogram, 0, sizeof(histogram));
        if (workspace != NULL) {
            memset(workspace, 0,
                   (size_t)workspace_threads * (size_t)HISTOGRAM_BINS * sizeof(hist_t));
        }
        times[repetition] = timed_histogram_once(
            variant, gray, pixel_count, histogram, workspace,
            workspace_threads, &actual_threads);

        if (!compare_histograms(reference, histogram, pixel_count,
                                &different_bins, &total)) {
            result->all_match = false;
            if (!have_failure_details) {
                result->differing_bins = different_bins;
                result->total = total;
                have_failure_details = true;
            }
        }
    }

    result->actual_threads = actual_threads > 0 ? actual_threads : 1;
    if (!median_of_times(times, &result->median_ms)) {
        return false;
    }
    return true;
}

static bool compute_speedup(double baseline_ms, double measured_ms,
                            double *speedup)
{
    if (baseline_ms <= 0.0 || measured_ms <= 1.0e-12) {
        return false;
    }
    *speedup = baseline_ms / measured_ms;
    return true;
}

static int find_thread_index(const int *threads, int thread_count, int requested)
{
    for (int i = 0; i < thread_count; i++) {
        if (threads[i] == requested) {
            return i;
        }
    }
    return -1;
}

static bool write_results_csv(const char *path, const BenchmarkRow *rows,
                              int row_count, const int *threads,
                              int thread_count, const double *lut_ms,
                              const double *lut_speedup,
                              const double *lut_efficiency,
                              const bool *lut_has_speedup)
{
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "Error: cannot open CSV file '%s' for writing: %s\n",
                path, strerror(errno));
        return false;
    }

    bool success = true;
    if (fprintf(file,
                "variant,requested_threads,actual_threads,median_histogram_ms,"
                "speedup,efficiency,correctness,median_lut_apply_ms,"
                "lut_speedup,lut_efficiency\n") < 0) {
        success = false;
    }

    for (int row = 0; success && row < row_count; row++) {
        int lut_index = find_thread_index(threads, thread_count,
                                          rows[row].requested_threads);
        if (lut_index < 0) {
            success = false;
            break;
        }

        if (fprintf(file, "%s,%d,%d,%.6f,",
                    variant_csv_names[rows[row].variant],
                    rows[row].requested_threads, rows[row].actual_threads,
                    rows[row].median_ms) < 0) {
            success = false;
            break;
        }

        if (rows[row].has_speedup) {
            if (fprintf(file, "%.6f,", rows[row].speedup) < 0) {
                success = false;
                break;
            }
        } else if (fprintf(file, "N/A,") < 0) {
            success = false;
            break;
        }

        if (rows[row].efficiency_not_applicable) {
            if (fprintf(file, "N/A,") < 0) {
                success = false;
                break;
            }
        } else if (fprintf(file, "%.6f,", rows[row].efficiency) < 0) {
            success = false;
            break;
        }

        if (fprintf(file, "%s,%.6f,", rows[row].correctness,
                    lut_ms[lut_index]) < 0) {
            success = false;
            break;
        }

        if (lut_has_speedup[lut_index]) {
            if (fprintf(file, "%.6f,%.6f\n", lut_speedup[lut_index],
                        lut_efficiency[lut_index]) < 0) {
                success = false;
                break;
            }
        } else if (fprintf(file, "N/A,N/A\n") < 0) {
            success = false;
            break;
        }
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "Error: failed to close CSV file '%s': %s\n",
                path, strerror(errno));
        success = false;
    }
    if (!success) {
        fprintf(stderr, "Error: failed while writing CSV file '%s'.\n", path);
        remove(path);
    }
    return success;
}

static void format_thread_list(char *buffer, size_t capacity,
                               const int *threads, int thread_count)
{
    size_t used = 0;
    buffer[0] = '\0';
    for (int i = 0; i < thread_count; i++) {
        int written = snprintf(buffer + used, capacity - used, "%s%d",
                               i == 0 ? "" : ", ", threads[i]);
        if (written < 0 || (size_t)written >= capacity - used) {
            break;
        }
        used += (size_t)written;
    }
}

static int run_benchmark(const Options *options, int width, int height,
                         const unsigned char *gray, size_t pixel_count,
                         int runtime_max_threads)
{
    int threads[5];
    int thread_count = 0;
    int candidates[5];
    int candidate_count = 0;

    candidates[candidate_count++] = 1;
    candidates[candidate_count++] = 2;
    candidates[candidate_count++] = 4;
    candidates[candidate_count++] = runtime_max_threads;
    if (options->threads_set) {
        candidates[candidate_count++] = options->threads;
    }

    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i] <= 0) {
            continue;
        }
        bool duplicate = false;
        for (int j = 0; j < thread_count; j++) {
            if (threads[j] == candidates[i]) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            threads[thread_count++] = candidates[i];
        }
    }

    char thread_text[128];
    format_thread_list(thread_text, sizeof(thread_text), threads, thread_count);

    print_environment(width, height, runtime_max_threads, "benchmark",
                      "all variants", 0, 0);
    printf("Benchmark thread set: %s\n", thread_text);
    if (options->threads_set) {
        printf("Note: --benchmark always keeps the standard set (1, 2, 4, runtime maximum);\n");
        printf("      the explicitly requested value %d is added when it is distinct.\n",
               options->threads);
    }
    printf("Histogram timing: 1 warm-up run + %d measured repetitions per case; median shown.\n",
           BENCHMARK_REPETITIONS);
    printf("Only histogram counting is timed.  Allocation, zeroing, comparison, and I/O are outside the timer.\n");
    printf("The sequential V0 measurement is the speedup baseline.\n\n");

    /* Build the correctness reference before measuring anything. */
    hist_t reference[HISTOGRAM_BINS];
    memset(reference, 0, sizeof(reference));
    histogram_sequential(gray, pixel_count, reference);
    int reference_differences = 0;
    hist_t reference_total = 0;
    bool reference_ok = compare_histograms(reference, reference, pixel_count,
                                           &reference_differences,
                                           &reference_total);
    if (!reference_ok) {
        fprintf(stderr, "Error: sequential reference histogram is inconsistent.\n");
        return 1;
    }

    BenchmarkRow rows[MAX_BENCHMARK_ROWS];
    int row_count = 0;
    memset(rows, 0, sizeof(rows));

    omp_set_num_threads(1);
    HistogramMeasurement baseline;
    if (!measure_histogram(VARIANT_SEQUENTIAL, gray, pixel_count, reference, 1,
                           NULL, 0, &baseline)) {
        fprintf(stderr, "Error: could not obtain a valid sequential baseline time.\n");
        return 1;
    }
    if (!baseline.all_match) {
        fprintf(stderr, "Error: sequential baseline histogram failed its own check.\n");
        return 1;
    }
    double baseline_ms = baseline.median_ms;
    if (baseline_ms <= 0.0) {
        fprintf(stderr, "Error: sequential baseline timing was not usable.\n");
        return 1;
    }

    printf("%-14s %9s %7s %14s %10s %10s %s\n",
           "Variant", "Requested", "Actual", "Median (ms)", "Speedup",
           "Efficiency", "Correctness");
    printf("%-14s %9d %7d %14.4f %10s %10s %s\n",
           "V0 sequential", 1, 1, baseline_ms, "1.000", "N/A", "PASS");

    rows[row_count].variant = VARIANT_SEQUENTIAL;
    rows[row_count].requested_threads = 1;
    rows[row_count].actual_threads = 1;
    rows[row_count].median_ms = baseline_ms;
    rows[row_count].speedup = 1.0;
    rows[row_count].has_speedup = true;
    rows[row_count].efficiency_not_applicable = true;
    snprintf(rows[row_count].correctness, sizeof(rows[row_count].correctness),
             "PASS");
    row_count++;

    bool protected_failure = false;

    for (int variant = VARIANT_UNSAFE; variant <= VARIANT_REDUCTION; variant++) {
        for (int thread_index = 0; thread_index < thread_count; thread_index++) {
            int requested_threads = threads[thread_index];
            omp_set_num_threads(requested_threads);

            hist_t *workspace = NULL;
            int workspace_threads = 0;
            if (variant == VARIANT_PRIVATE) {
                workspace_threads = omp_get_max_threads();
                if (workspace_threads < 1) {
                    workspace_threads = 1;
                }
                workspace = (hist_t *)calloc(
                    (size_t)workspace_threads * (size_t)HISTOGRAM_BINS,
                    sizeof(hist_t));
                if (workspace == NULL) {
                    fprintf(stderr,
                            "Error: cannot allocate private histogram workspace (%d threads).\n",
                            workspace_threads);
                    return 1;
                }
            }

            HistogramMeasurement measurement;
            bool measured = measure_histogram(
                (HistVariant)variant, gray, pixel_count, reference,
                requested_threads, workspace, workspace_threads, &measurement);
            free(workspace);

            if (!measured) {
                fprintf(stderr,
                        "Error: no usable timing was obtained for %s with %d requested threads.\n",
                        variant_labels[variant], requested_threads);
                return 1;
            }

            bool safe_variant = variant != VARIANT_UNSAFE;
            bool has_speedup = false;
            double speedup = 0.0;
            double efficiency = 0.0;

            if (safe_variant) {
                has_speedup = compute_speedup(baseline_ms, measurement.median_ms,
                                              &speedup);
                if (has_speedup && measurement.actual_threads > 0) {
                    efficiency = speedup / (double)measurement.actual_threads;
                }
            }

            char status[48];
            if (variant == VARIANT_UNSAFE) {
                if (measurement.all_match) {
                    snprintf(status, sizeof(status), "NO_RACE_OBSERVED");
                } else {
                    snprintf(status, sizeof(status), "RACE");
                }
            } else if (measurement.all_match) {
                snprintf(status, sizeof(status), "PASS");
            } else {
                snprintf(status, sizeof(status), "FAIL");
                protected_failure = true;
            }

            if (variant == VARIANT_UNSAFE) {
                printf("%-14s %9d %7d %14.4f %10s %10s %s\n",
                       variant_labels[variant], requested_threads,
                       measurement.actual_threads, measurement.median_ms,
                       "N/A", "N/A", status);
            } else {
                char speedup_text[32];
                char efficiency_text[32];
                if (has_speedup) {
                    snprintf(speedup_text, sizeof(speedup_text), "%.3f", speedup);
                    snprintf(efficiency_text, sizeof(efficiency_text), "%.3f",
                             efficiency);
                } else {
                    snprintf(speedup_text, sizeof(speedup_text), "N/A");
                    snprintf(efficiency_text, sizeof(efficiency_text), "N/A");
                }
                printf("%-14s %9d %7d %14.4f %10s %10s %s\n",
                       variant_labels[variant], requested_threads,
                       measurement.actual_threads, measurement.median_ms,
                       speedup_text, efficiency_text, status);
            }

            if (variant == VARIANT_UNSAFE && !measurement.all_match) {
                printf("  (unsafe result differs from V0: differing_bins=%d total=%llu; not a valid speedup)\n",
                       measurement.differing_bins, measurement.total);
            }
            if (!safe_variant) {
                printf("  (V1 timing is shown for demonstration only; it is not a correct algorithm.)\n");
            }

            if (row_count >= MAX_BENCHMARK_ROWS) {
                fprintf(stderr, "Error: benchmark table row limit reached.\n");
                return 1;
            }
            BenchmarkRow *row = &rows[row_count];
            row->variant = variant;
            row->requested_threads = requested_threads;
            row->actual_threads = measurement.actual_threads;
            row->median_ms = measurement.median_ms;
            row->speedup = speedup;
            row->efficiency = efficiency;
            row->has_speedup = has_speedup;
            row->efficiency_not_applicable = variant == VARIANT_UNSAFE;
            snprintf(row->correctness, sizeof(row->correctness), "%s", status);
            row_count++;
        }
    }

    printf("\nV1 is intentionally unsafe.  Its timing is not a valid speedup, and its\n");
    printf("correctness status is based on the warm-up plus all seven timed repetitions.\n");
    printf("A match on one machine or run does not make V1 thread-safe.\n");

    /* Measure LUT application separately for every benchmark thread count. */
    unsigned char lookup[256];
    bool identity_mapping = false;
    if (!build_lookup_table(reference, (unsigned long long)pixel_count,
                            lookup, &identity_mapping)) {
        return 1;
    }

    unsigned char *lut_output = (unsigned char *)malloc(pixel_count);
    if (lut_output == NULL) {
        fprintf(stderr, "Error: cannot allocate %zu bytes for the LUT output buffer.\n",
                pixel_count);
        return 1;
    }

    double lut_ms[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double lut_speedup[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double lut_efficiency[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    bool lut_has_speedup[5] = {false, false, false, false, false};
    int lut_actual_threads[5] = {0, 0, 0, 0, 0};

    for (int thread_index = 0; thread_index < thread_count; thread_index++) {
        omp_set_num_threads(threads[thread_index]);
        lut_actual_threads[thread_index] =
            detect_actual_threads(threads[thread_index]);

        apply_lookup_table(gray, lut_output, pixel_count, lookup);
        for (size_t i = 0; i < pixel_count; i++) {
            if (lut_output[i] != lookup[gray[i]]) {
                fprintf(stderr, "Error: LUT application verification failed.\n");
                free(lut_output);
                return 1;
            }
        }

        double times[BENCHMARK_REPETITIONS];
        for (int repetition = 0; repetition < BENCHMARK_REPETITIONS; repetition++) {
            double start = omp_get_wtime();
            apply_lookup_table(gray, lut_output, pixel_count, lookup);
            double end = omp_get_wtime();
            times[repetition] = (end - start) * 1000.0;
        }

        if (!median_of_times(times, &lut_ms[thread_index])) {
            fprintf(stderr, "Error: LUT timing was not usable for %d threads.\n",
                    threads[thread_index]);
            free(lut_output);
            return 1;
        }
    }
    free(lut_output);

    for (int thread_index = 0; thread_index < thread_count; thread_index++) {
        if (thread_index == 0) {
            lut_speedup[thread_index] = 1.0;
            lut_efficiency[thread_index] = 1.0;
            lut_has_speedup[thread_index] = true;
            continue;
        }
        if (lut_ms[0] > 0.0 && lut_ms[thread_index] > 1.0e-12) {
            lut_speedup[thread_index] =
                lut_ms[0] / lut_ms[thread_index];
            if (lut_actual_threads[thread_index] > 0) {
                lut_efficiency[thread_index] =
                    lut_speedup[thread_index] /
                    (double)lut_actual_threads[thread_index];
            }
            lut_has_speedup[thread_index] = true;
        }
    }

    printf("\nLUT application benchmark (separate from histogram timing)\n");
    printf("Baseline is the 1-thread OpenMP LUT pass.  Output buffers are allocated before timing.\n");
    printf("%-14s %9s %7s %14s %10s %10s\n",
           "Step", "Requested", "Actual", "Median (ms)", "Speedup", "Efficiency");
    for (int thread_index = 0; thread_index < thread_count; thread_index++) {
        char speedup_text[32];
        char efficiency_text[32];
        if (lut_has_speedup[thread_index]) {
            snprintf(speedup_text, sizeof(speedup_text), "%.3f",
                     lut_speedup[thread_index]);
            snprintf(efficiency_text, sizeof(efficiency_text), "%.3f",
                     lut_efficiency[thread_index]);
        } else {
            snprintf(speedup_text, sizeof(speedup_text), "N/A");
            snprintf(efficiency_text, sizeof(efficiency_text), "N/A");
        }
        printf("%-14s %9d %7d %14.4f %10s %10s\n",
               "LUT apply", threads[thread_index],
               lut_actual_threads[thread_index], lut_ms[thread_index],
               speedup_text, efficiency_text);
    }

    char csv_path[PATH_BUFFER_SIZE];
    if (!join_path(csv_path, sizeof(csv_path), options->output_dir,
                   "results.csv")) {
        fprintf(stderr, "Error: output path is too long.\n");
        return 1;
    }
    if (!write_results_csv(csv_path, rows, row_count, threads, thread_count,
                           lut_ms, lut_speedup, lut_efficiency,
                           lut_has_speedup)) {
        return 1;
    }

    printf("\nCSV written: %s\n", csv_path);
    printf("CSV note: histogram speedup/efficiency are N/A for V0 (baseline) and V1 (unsafe).\n");

    if (protected_failure) {
        fprintf(stderr,
                "\nError: at least one protected histogram variant failed correctness.\n");
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Command-line interface                                                    */
/* ------------------------------------------------------------------------- */

static void print_help(const char *program_name)
{
    printf("PixelSprint: Race-Free Parallel Histogram Equalization with OpenMP\n\n");
    printf("Usage:\n");
    printf("  %s [options]\n\n", program_name);
    printf("Modes (default: process one image and write output/before.bmp and after.bmp):\n");
    printf("  --demo                 Run the correctness demonstration and create images.\n");
    printf("  --benchmark            Benchmark all histogram variants and write results.csv.\n\n");
    printf("Options:\n");
    printf("  --size N               Generated square image size, %d..%d (default %d).\n",
           MIN_DIMENSION, MAX_DIMENSION, DEFAULT_DIMENSION);
    printf("  --threads T            Requested OpenMP threads, 1..%d (default: runtime maximum).\n",
           MAX_THREADS);
    printf("  --variant NAME         Histogram for normal processing/demo equalization:\n");
    printf("                         sequential, unsafe, critical, atomic, private, reduction.\n");
    printf("                         Default: reduction.\n");
    printf("  --input FILE.ppm       Read a binary PPM P6 image instead of generating one.\n");
    printf("  --output DIR           Output directory (default: output/).\n");
    printf("  --help                 Show this help and exit successfully.\n\n");
    printf("Examples:\n");
    printf("  %s --demo\n", program_name);
    printf("  %s --demo --size 256 --threads 4\n", program_name);
    printf("  %s --benchmark --size 2048\n", program_name);
    printf("  %s --input photo.ppm --variant atomic --output my_results\n\n", program_name);
    printf("Notes:\n");
    printf("  --size and --input cannot be combined.\n");
    printf("  --demo and --benchmark cannot be combined.\n");
    printf("  --variant cannot be combined with --benchmark (all variants are tested).\n");
    printf("  --benchmark always tests 1, 2, 4, and the runtime maximum thread count;\n");
    printf("  an explicit --threads value is added if it is distinct.\n");
    printf("  Selecting unsafe for image processing still validates it, but equalization\n");
    printf("  falls back to the safe sequential reference after a clear warning.\n");
}

static bool parse_options(int argc, char **argv, Options *options)
{
    memset(options, 0, sizeof(*options));
    options->mode = MODE_NORMAL;
    options->size = DEFAULT_DIMENSION;
    options->threads = 0;
    options->threads_set = false;
    options->variant = VARIANT_REDUCTION;
    options->variant_set = false;
    options->input_set = false;
    options->output_set = false;
    options->help = false;
    snprintf(options->output_dir, sizeof(options->output_dir), "output");

    bool seen_size = false;
    bool seen_threads = false;
    bool seen_variant = false;
    bool seen_input = false;
    bool seen_output = false;
    bool seen_demo = false;
    bool seen_benchmark = false;
    bool seen_help = false;

    for (int argument = 1; argument < argc; argument++) {
        const char *option = argv[argument];

        if (strcmp(option, "--help") == 0) {
            if (seen_help) {
                fprintf(stderr, "Error: duplicate option '%s'.\n", option);
                return false;
            }
            seen_help = true;
            options->help = true;
        } else if (strcmp(option, "--demo") == 0) {
            if (seen_demo) {
                fprintf(stderr, "Error: duplicate option '%s'.\n", option);
                return false;
            }
            seen_demo = true;
            if (options->mode == MODE_BENCHMARK) {
                fprintf(stderr, "Error: --demo and --benchmark cannot be combined.\n");
                return false;
            }
            options->mode = MODE_DEMO;
        } else if (strcmp(option, "--benchmark") == 0) {
            if (seen_benchmark) {
                fprintf(stderr, "Error: duplicate option '%s'.\n", option);
                return false;
            }
            seen_benchmark = true;
            if (options->mode == MODE_DEMO) {
                fprintf(stderr, "Error: --demo and --benchmark cannot be combined.\n");
                return false;
            }
            options->mode = MODE_BENCHMARK;
        } else if (strcmp(option, "--size") == 0) {
            if (seen_size) {
                fprintf(stderr, "Error: duplicate option '%s'.\n", option);
                return false;
            }
            seen_size = true;
            if (argument + 1 >= argc) {
                fprintf(stderr, "Error: --size requires a value.\n");
                return false;
            }
            const char *value = argv[++argument];
            if (!parse_dimension(value, &options->size)) {
                fprintf(stderr,
                        "Error: --size must be an integer from %d to %d.\n",
                        MIN_DIMENSION, MAX_DIMENSION);
                return false;
            }
            options->size_set = true;
        } else if (strcmp(option, "--threads") == 0) {
            if (seen_threads) {
                fprintf(stderr, "Error: duplicate option '%s'.\n", option);
                return false;
            }
            seen_threads = true;
            if (argument + 1 >= argc) {
                fprintf(stderr, "Error: --threads requires a value.\n");
                return false;
            }
            const char *value = argv[++argument];
            if (!parse_thread_count(value, &options->threads)) {
                fprintf(stderr,
                        "Error: --threads must be an integer from 1 to %d.\n",
                        MAX_THREADS);
                return false;
            }
            options->threads_set = true;
        } else if (strcmp(option, "--variant") == 0) {
            if (seen_variant) {
                fprintf(stderr, "Error: duplicate option '%s'.\n", option);
                return false;
            }
            seen_variant = true;
            if (argument + 1 >= argc) {
                fprintf(stderr, "Error: --variant requires a value.\n");
                return false;
            }
            const char *value = argv[++argument];
            if (!variant_from_name(value, &options->variant)) {
                fprintf(stderr,
                        "Error: invalid variant '%s'. Use sequential, unsafe, critical, atomic, private, or reduction.\n",
                        value);
                return false;
            }
            options->variant_set = true;
        } else if (strcmp(option, "--input") == 0) {
            if (seen_input) {
                fprintf(stderr, "Error: duplicate option '%s'.\n", option);
                return false;
            }
            seen_input = true;
            if (argument + 1 >= argc) {
                fprintf(stderr, "Error: --input requires a file name.\n");
                return false;
            }
            const char *value = argv[++argument];
            if (*value == '\0') {
                fprintf(stderr, "Error: --input file name cannot be empty.\n");
                return false;
            }
            if (strlen(value) >= sizeof(options->input_path)) {
                fprintf(stderr, "Error: --input path is too long.\n");
                return false;
            }
            snprintf(options->input_path, sizeof(options->input_path), "%s", value);
            options->input_set = true;
        } else if (strcmp(option, "--output") == 0) {
            if (seen_output) {
                fprintf(stderr, "Error: duplicate option '%s'.\n", option);
                return false;
            }
            seen_output = true;
            if (argument + 1 >= argc) {
                fprintf(stderr, "Error: --output requires a directory name.\n");
                return false;
            }
            const char *value = argv[++argument];
            if (*value == '\0') {
                fprintf(stderr, "Error: --output directory cannot be empty.\n");
                return false;
            }
            if (strlen(value) >= sizeof(options->output_dir)) {
                fprintf(stderr, "Error: --output path is too long.\n");
                return false;
            }
            snprintf(options->output_dir, sizeof(options->output_dir), "%s", value);
            options->output_set = true;
        } else {
            fprintf(stderr, "Error: unknown argument '%s'. Use --help for usage.\n",
                    option);
            return false;
        }
    }

    if (options->mode == MODE_BENCHMARK && options->variant_set) {
        fprintf(stderr,
                "Error: --variant cannot be combined with --benchmark; the benchmark tests every variant.\n");
        return false;
    }
    if (options->input_set && options->size_set) {
        fprintf(stderr,
                "Error: --size cannot be combined with --input; the PPM header supplies the dimensions.\n");
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    Options options;
    if (!parse_options(argc, argv, &options)) {
        return EXIT_FAILURE;
    }

    if (options.help) {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    }

    omp_set_dynamic(0);
    int runtime_max_threads = omp_get_max_threads();
    if (runtime_max_threads < 1) {
        runtime_max_threads = 1;
    }
    if (!options.threads_set) {
        options.threads = runtime_max_threads;
    }
    if (options.threads < 1) {
        options.threads = 1;
    }

    if (!ensure_output_directory(options.output_dir)) {
        return EXIT_FAILURE;
    }

    Image image;
    memset(&image, 0, sizeof(image));
    if (options.input_set) {
        if (!load_ppm_image(options.input_path, &image)) {
            return EXIT_FAILURE;
        }
    } else {
        if (!generate_image(&image, options.size)) {
            return EXIT_FAILURE;
        }
    }

    size_t pixel_count = 0;
    if (!checked_pixel_count(image.width, image.height, &pixel_count)) {
        fprintf(stderr, "Error: loaded image dimensions are invalid.\n");
        free_image(&image);
        return EXIT_FAILURE;
    }

    unsigned char *gray = (unsigned char *)malloc(pixel_count);
    if (gray == NULL) {
        fprintf(stderr, "Error: cannot allocate %zu bytes for the grayscale image.\n",
                pixel_count);
        free_image(&image);
        return EXIT_FAILURE;
    }
    convert_to_grayscale(&image, gray, pixel_count);

    int result = EXIT_FAILURE;
    if (options.mode == MODE_DEMO) {
        result = run_demo(&options, image.width, image.height, gray,
                          pixel_count, runtime_max_threads);
    } else if (options.mode == MODE_BENCHMARK) {
        result = run_benchmark(&options, image.width, image.height, gray,
                               pixel_count, runtime_max_threads);
    } else {
        result = run_normal(&options, image.width, image.height, gray,
                            pixel_count, runtime_max_threads);
    }

    free(gray);
    free_image(&image);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
