/* See LICENSE file for copyright and license details. */
#include "common.h"

USAGE("matrix-stream")

static void
process(struct stream *input, struct stream *matrices)
{
	char *input_row, *output_frame;
	size_t x_in, y_in, x_out, y_out;
	ptrdiff_t input_offset, output_offset;
	double *matrix, x_center, y_center, x, y, a, b, c, d, e, f;

	// make sure there's enough memory to store one frame of each stream
	echeck_dimensions(input, WIDTH, NULL);
	echeck_dimensions(matrices, WIDTH | HEIGHT, NULL);
	// allocate a buffer to store the current frame of input and matrix streams
	input_row = emalloc(input->row_size);
	matrix = emalloc(matrices->frame_size);
	// allocate buffer for output frame and zero it
	output_frame = emalloc(input->frame_size);
	// determine center of image
	x_center = (double)(input->width) / 2.;
	y_center = (double)(input->height) / 2.;
	// read each matrix into the buffer
	while (eread_frame(matrices, matrix)) {
		a = matrix[0];
		b = matrix[4];
		c = matrix[8];
		d = matrix[12];
		e = matrix[16];
		f = matrix[20];
		// clear output frame
		memset(output_frame, 0, input->frame_size);
		// read each row from corresponding input frame
		for (y_in = 0; y_in < input->height; y_in += 1) {
			// stop if the read fails, meaning the input stream has run out of rows
			if (!eread_row(input, input_row)) break;
			// iterate over every pixel in input row
			for (x_in = 0; x_in < input->width; x_in += 1) {
				// normalize coordinates, putting center at 0, 0
				x = (double)(x_in) - x_center;
				y = (double)(y_in) - y_center;
				// determine destination coordinates, then denormalize them
				x_out = round(a * x + b * y + c + x_center);
				y_out = round(d * x + e * y + f + y_center);
				// in-memory location of destination pixel, as offset from st
				input_offset = x_in * input->pixel_size;
				output_offset = (y_out * input->width + x_out) * input->pixel_size;
				// ignore pixels that fall outside the frame
				if (output_offset >= 0 && output_offset < input->frame_size) {
					// copy source pixel to destination
					memcpy(output_frame + output_offset, input_row + input_offset, input->pixel_size);
				}
			}
		}
		// write output frame to stdout
		ewriteall(STDOUT_FILENO, output_frame, input->frame_size, "<stdout>");
	}
	// free the allocated buffers when done
	free(input_row);
	free(matrix);
	free(output_frame);
}

int
main(int argc, char *argv[])
{
	struct stream input; // stream of video frames to be transformed
	struct stream matrices; // stream of corresponding transform matrices to apply

	if (argc != 2) // require 1 positional argument - path to matrices
		usage();

	eopen_stream(&input, NULL);
	eopen_stream(&matrices, argv[1]);

	// validate format of matrix stream
	if (matrices.width != 3 || matrices.height != 3)
		eprintf("matrix stream does not have 3x3 geometry\n");
	if (strcmp(matrices.pixfmt, "xyza"))
		eprintf("pixel format of matrix stream must be xyza\n");

	fprint_stream_head(stdout, &input);
	efflush(stdout, "<stdout>");

	process(&input, &matrices);
	close(matrices.fd);
	return 0;
}
