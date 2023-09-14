#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vips/vips.h>
#include "unary.h"

typedef VipsUnary VipsWand;
typedef VipsUnaryClass VipsWandClass;

G_DEFINE_TYPE(VipsWand, vips_wand, VIPS_TYPE_UNARY);

#define LOOP(TYPE, L) \
	{ \
		TYPE *restrict p = (TYPE *) in[0]; \
		TYPE *restrict q = (TYPE *) out; \
\
		for (x = 0; x < sz; x++) \
			q[x] = (L) -p[x]; \
	}

#define LOOPN(TYPE) \
	{ \
		TYPE *restrict p = (TYPE *) in[0]; \
		TYPE *restrict q = (TYPE *) out; \
\
		for (x = 0; x < sz; x++) \
			q[x] = -1 * p[x]; \
	}

#define LOOPC(TYPE) \
	{ \
		TYPE *restrict p = (TYPE *) in[0]; \
		TYPE *restrict q = (TYPE *) out; \
\
		for (x = 0; x < sz; x++) { \
			q[0] = -1 * p[0]; \
			q[1] = p[1]; \
\
			p += 2; \
			q += 2; \
		} \
	}

static void
vips_wand_buffer(VipsArithmetic* arithmetic,
	VipsPel* out, VipsPel** in, int width) {
	VipsImage* im = arithmetic->ready[0];
	const int sz = width * vips_image_get_bands(im);

	int x;

	switch (vips_image_get_format(im)) {
	case VIPS_FORMAT_UCHAR:
		LOOP(unsigned char, UCHAR_MAX);
		break;
	case VIPS_FORMAT_CHAR:
		LOOPN(signed char);
		break;
	case VIPS_FORMAT_USHORT:
		LOOP(unsigned short, USHRT_MAX);
		break;
	case VIPS_FORMAT_SHORT:
		LOOPN(signed short);
		break;
	case VIPS_FORMAT_UINT:
		LOOP(unsigned int, UINT_MAX);
		break;
	case VIPS_FORMAT_INT:
		LOOPN(signed int);
		break;

	case VIPS_FORMAT_FLOAT:
		LOOPN(float);
		break;
	case VIPS_FORMAT_DOUBLE:
		LOOPN(double);
		break;

	case VIPS_FORMAT_COMPLEX:
		LOOPC(float);
		break;
	case VIPS_FORMAT_DPCOMPLEX:
		LOOPC(double);
		break;

	default:
		g_assert_not_reached();
	}
}

/* Save a bit of typing.
 */
#define UC VIPS_FORMAT_UCHAR
#define C VIPS_FORMAT_CHAR
#define US VIPS_FORMAT_USHORT
#define S VIPS_FORMAT_SHORT
#define UI VIPS_FORMAT_UINT
#define I VIPS_FORMAT_INT
#define F VIPS_FORMAT_FLOAT
#define X VIPS_FORMAT_COMPLEX
#define D VIPS_FORMAT_DOUBLE
#define DX VIPS_FORMAT_DPCOMPLEX

static const VipsBandFormat vips_wand_format_table[10] = {
	/* Band format:  UC  C  US  S  UI  I  F  X  D  DX */
	/* Promotion: */ UC, C, US, S, UI, I, F, X, D, DX
};

static void
vips_wand_class_init(VipsWandClass* class) {
	VipsObjectClass* object_class = (VipsObjectClass*) class;
	VipsArithmeticClass* aclass = VIPS_ARITHMETIC_CLASS(class);

	object_class->nickname = "wand";
	object_class->description = _("do a magic trick!");

	aclass->process_line = vips_wand_buffer;

	vips_arithmetic_set_format_table(aclass, vips_wand_format_table);
}

static void
vips_wand_init(VipsWand* wand) {}

int
vips_wand(VipsImage* in, VipsImage** out, ...) {
	va_list ap;
	int result;

	va_start(ap, out);
	result = vips_call_split("wand", ap, in, out);
	va_end(ap);

	return result;
}
