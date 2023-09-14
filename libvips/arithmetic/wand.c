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

static void
vips_wand_buffer(VipsArithmetic* arithmetic,
	VipsPel* out, VipsPel** in, int width) {

}

static const VipsBandFormat vips_wand_format_table[0] = {};

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
