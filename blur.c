/*
 * Copyright Â© 2008 Kristian HÃ¸gsberg
 * Copyright Â© 2009 Chris Wilson
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <cairo.h>
#include <stdlib.h>

/* Performs a simple 2D Gaussian blur of radius @radius on surface @surface. */

uint32_t* boxesForGauss(double sigma, double n)  // standard deviation, number of boxes
{
	uint32_t wl = sqrt(12*sigma*sigma/n+1.);  // Ideal averaging filter width  // floor
	if(wl%2==0)
		wl--;

	uint32_t wu = wl+2;
	uint32_t m = (12*sigma*sigma - n*wl*wl - 4*n*wl - 3*n)/(-4*wl - 4); // round
				
	uint32_t* sizes = (uint32_t*) malloc(n*sizeof(uint32_t));
	for(uint32_t i=0; i<n; i++)
		sizes[i] = (i<m?wl:wu);

	return sizes;
}
/*
function boxBlurH_4 (scl, tcl, w, h, r) {
    var iarr = 1 / (r+r+1);
    for(var i=0; i<h; i++) {
        var ti = i*w, li = ti, ri = ti+r;
        var fv = scl[ti], lv = scl[ti+w-1], val = (r+1)*fv;
        for(var j=0; j<r; j++) val += scl[ti+j];
        for(var j=0  ; j<=r ; j++) { val += scl[ri++] - fv       ;   tcl[ti++] = Math.round(val*iarr); }
        for(var j=r+1; j<w-r; j++) { val += scl[ri++] - scl[li++];   tcl[ti++] = Math.round(val*iarr); }
        for(var j=w-r; j<w  ; j++) { val += lv        - scl[li++];   tcl[ti++] = Math.round(val*iarr); }
    }
}
*/
void boxBlurH_4 (uint8_t *scl, uint8_t *tcl, int w, int h, int r, int off) {
	double iarr = 1. / (double)(r+r+1);
	for(int i=0; i<h; i++) {
		int ti = i*w, 
			li = ti, 
			ri = ti+r;

		long double fv = scl[ti*4+off],
			lv = scl[(ti+w-1)*4+off],
			val = (r+1)*(fv);

		for(int j=0; j<r; j++) {
			val += scl[(ti+j)*4+off];
		}
		for(int j=0  ; j<=r ; j++) {
			val += scl[ri++*4+off] - fv;
			tcl[ti++*4+off] = val * iarr;
		}
		for(int j=r+1; j<w-r; j++) {
			val += scl[ri++*4+off] - scl[li++*4+off];
			tcl[ti++*4+off] = val * iarr;
		}
		for(int j=w-r; j<w; j++) {
			val += lv - scl[li++*4+off]; 
			tcl[ti++*4+off] = val * iarr;
		}
	}
}
/*
function boxBlurT_4 (scl, tcl, w, h, r) {
    var iarr = 1 / (r+r+1);
    for(var i=0; i<w; i++) {
        var ti = i, li = ti, ri = ti+r*w;
        var fv = scl[ti], lv = scl[ti+w*(h-1)], val = (r+1)*fv;
        for(var j=0; j<r; j++) val += scl[ti+j*w];
        for(var j=0  ; j<=r ; j++) { val += scl[ri] - fv     ;  tcl[ti] = Math.round(val*iarr);  ri+=w; ti+=w; }
        for(var j=r+1; j<h-r; j++) { val += scl[ri] - scl[li];  tcl[ti] = Math.round(val*iarr);  li+=w; ri+=w; ti+=w; }
        for(var j=h-r; j<h  ; j++) { val += lv      - scl[li];  tcl[ti] = Math.round(val*iarr);  li+=w; ti+=w; }
    }
}
*/
void boxBlurT_4 (uint8_t *scl, uint8_t *tcl, int w, int h, int r, int off) {
	double iarr = 1. / (double)(r+r+1);
	for(int i=0; i<w; i++) {
		int ti = i, 
			li = ti, 
			ri = ti+r*w;

		long double fv = scl[ti*4+off],
			lv = scl[(ti+w*(h-1))*4+off],
			val = (r+1)*(fv);

		for(int j=0; j<r; j++) {
			val += scl[(ti+j*w)*4+off];
		}
		for(int j=0  ; j<=r ; j++) {
			val += scl[ri*4+off] - fv;
			tcl[ti*4+off] = val * iarr;
			ri += w; ti += w;
		}
		for(int j=r+1; j<h-r; j++) {
			val += scl[ri*4+off] - scl[li*4+off];
			tcl[ti*4+off] = val * iarr;
			ri += w; li += w; ti += w;
		}
		for(int j=h-r; j<h; j++) {
			val += lv - scl[li*4+off]; 
			tcl[ti*4+off] = val * iarr;
			li += w; ti += w;
		}
	}
}
void boxBlur_4 (uint8_t *scl, uint8_t *tcl, int w, int h, int r, char off) {
	memcpy(tcl, scl, h*w*4);
	// for(int i=0; i<h*w*4; i++)
	// 	tcl[i] = scl[i];
	boxBlurH_4(scl, tcl, w, h, r, off);
	boxBlurT_4(tcl, scl, w, h, r, off);
	// memcpy(scl, tcl, h*w*4);
}
void gaussBlur_4 (uint8_t *scl, uint8_t *tcl, int w, int h, int r) {
	uint32_t* bxs = boxesForGauss(r, 3);
	for (char off = 0; off < 3; off++) {
		boxBlur_4 (scl, tcl, w, h, (bxs[0]-1)/2, off);
		boxBlur_4 (scl, tcl, w, h, (bxs[1]-1)/2, off);
		boxBlur_4 (scl, tcl, w, h, (bxs[2]-1)/2, off);
	}
	free(bxs);
}


void blur_image_surface (cairo_surface_t *surface, int radius)
{
	cairo_surface_t *tmp;
	int width, height;
	int src_stride, dst_stride;
	// int x, y, z, w;
	uint8_t *src, *dst;
	// uint32_t *s, *d, a, p;
	// int i, j, k;
	// const int size = radius;
	// const int half = size / 2;

	if (cairo_surface_status (surface))
		return;

	width = cairo_image_surface_get_width (surface);
	height = cairo_image_surface_get_height (surface);

	switch (cairo_image_surface_get_format (surface)) {
		case CAIRO_FORMAT_A1:
		default:
			/* Don't even think about it! */
			return;

		case CAIRO_FORMAT_A8:
			/* Handle a8 surfaces by effectively unrolling the loops by a
			 * factor of 4 - this is safe since we know that stride has to be a
			 * multiple of uint32_t. */
			width /= 4;
			break;

		case CAIRO_FORMAT_RGB24:
		case CAIRO_FORMAT_ARGB32:
			break;
	}

	tmp = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status (tmp))
		return;

	src = cairo_image_surface_get_data (surface);
	src_stride = cairo_image_surface_get_stride (surface);

	dst = cairo_image_surface_get_data (tmp);
	dst_stride = cairo_image_surface_get_stride (tmp);
	if (src_stride != dst_stride) {
		cairo_surface_destroy (tmp);
		cairo_surface_flush (surface);
		cairo_surface_mark_dirty (surface);
	}

	gaussBlur_4(src, dst, width, height, radius); 

	cairo_surface_destroy (tmp);
	cairo_surface_flush (surface);
	cairo_surface_mark_dirty (surface);
}

