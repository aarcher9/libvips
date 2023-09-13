/* VIPS universal main program.
 *
 * J. Cupitt, 8/4/93.
 * 12/5/06
 * 	- use GOption. g_*_prgname()
 * 16/7/06
 * 	- hmm, was broken for function name as argv1 case
 * 11/7/06
 * 	- add "all" option to -l
 * 14/7/06
 * 	- ignore "--" arguments.
 * 2/9/06
 * 	- do less init ... im_init_world() does more now
 * 18/8/06
 * 	- use IM_EXEEXT
 * 16/10/06
 * 	- add --version
 * 17/10/06
 * 	- add --swig
 * 	- cleanups
 * 	- remove --swig again, sigh
 * 	- add throw() decls to C++ to help SWIG
 * 14/1/07
 * 	- add --list packages
 * 26/2/07
 * 	- add input *VEC arg types to C++ binding
 * 17/8/08
 * 	- add --list formats
 * 29/11/08
 * 	- add --list interpolators
 * 9/2/09
 * 	- and now we just have --list packages/classes/package-name
 * 13/11/09
 * 	- drop _f postfixes, drop many postfixes
 * 24/6/10
 * 	- less chatty error messages
 * 	- oops, don't rename "copy_set" as "copy_"
 * 6/2/12
 * 	- long arg names in decls to help SWIG
 * 	- don't wrap im_remainderconst_vec()
 * 31/12/12
 * 	- parse options in two passes (thanks Haida)
 * 26/11/17
 * 	- remove throw() decls, they are now deprecated everywhere
 * 18/6/20 kleisauke
 * 	- avoid using vips7 symbols
 * 	- remove deprecated vips7 C++ generator
 * 1/11/22
 * 	- add "-c" flag
 */

 /*

	 This file is part of VIPS.

	 VIPS is free software; you can redistribute it and/or modify
	 it under the terms of the GNU Lesser General Public License as published by
	 the Free Software Foundation; either version 2 of the License, or
	 (at your option) any later version.

	 This program is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU Lesser General Public License for more details.

	 You should have received a copy of the GNU Lesser General Public License
	 along with this program; if not, write to the Free Software
	 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
	 02110-1301  USA

  */

  /*

	  These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

   */

   /*
   #define DEBUG
   #define DEBUG_FATAL
	*/

	/* Need to disable these sometimes.
	#undef DEBUG_FATAL
	 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <glib/gi18n.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

#define VIPS_DISABLE_DEPRECATION_WARNINGS
#include <vips/vips.h>
#include <vips/internal.h>

static char* main_option_plugin = NULL;
static gboolean main_option_version;

static void*
list_class(GType type, void* user_data) {
	VipsObjectClass* class = VIPS_OBJECT_CLASS(g_type_class_ref(type));
	int depth = vips_type_depth(type);

	int i;

	if (class->deprecated)
		return NULL;
	if (VIPS_IS_OPERATION_CLASS(class) &&
		(VIPS_OPERATION_CLASS(class)->flags &
			VIPS_OPERATION_DEPRECATED))
		return NULL;

	for (i = 0; i < depth * 2; i++)
		printf(" ");
	vips_object_print_summary_class(
		VIPS_OBJECT_CLASS(g_type_class_ref(type)));

	return NULL;
}

static void*
test_class_name(GType type, void* data) {
	const char* name = (const char*)data;

	VipsObjectClass* class;

	/* Test the classname too.
	 */
	if ((class = VIPS_OBJECT_CLASS(g_type_class_ref(type))) &&
		strcmp(class->nickname, name) == 0)
		return class;
	if (strcmp(G_OBJECT_CLASS_NAME(class), name) == 0)
		return class;

	return NULL;
}

static gboolean
parse_main_option_list(const gchar* option_name, const gchar* value,
	gpointer data, GError** error) {
	VipsObjectClass* class;

	if (value &&
		(class = (VipsObjectClass*)vips_type_map_all(
			g_type_from_name("VipsObject"),
			test_class_name, (void*)value))) {
		vips_type_map_all(G_TYPE_FROM_CLASS(class),
			list_class, NULL);
	} else if (value) {
		vips_error(g_get_prgname(),
			_("'%s' is not the name of a vips class"), value);
		vips_error_g(error);

		return FALSE;
	} else {
		vips_type_map_all(g_type_from_name("VipsOperation"),
			list_class, NULL);
	}

	exit(0);
}

static void*
list_operation(GType type, void* user_data) {
	VipsObjectClass* class = VIPS_OBJECT_CLASS(g_type_class_ref(type));

	if (G_TYPE_IS_ABSTRACT(type))
		return NULL;
	if (class->deprecated)
		return NULL;
	if (VIPS_OPERATION_CLASS(class)->flags & VIPS_OPERATION_DEPRECATED)
		return NULL;

	/* Complete on class names as well as nicknames -- "crop", for
	 * example, is a class name.
	 */
	printf("%s\n", class->nickname);
	printf("%s\n", G_OBJECT_CLASS_NAME(class));

	return NULL;
}

static void*
list_operation_arg(VipsObjectClass* object_class,
	GParamSpec* pspec, VipsArgumentClass* argument_class,
	void* _data1, void* _data2) {
	GType type = G_PARAM_SPEC_VALUE_TYPE(pspec);

	if (!(argument_class->flags & VIPS_ARGUMENT_CONSTRUCT) ||
		(argument_class->flags & VIPS_ARGUMENT_DEPRECATED))
		return NULL;

	/* We don't try to complete options, though maybe we should.
	 */
	if (!(argument_class->flags & VIPS_ARGUMENT_REQUIRED))
		return NULL;

	/* These are the pspecs that vips uses that have interesting values.
	 */
	if (G_IS_PARAM_SPEC_ENUM(pspec)) {
		GTypeClass* class = g_type_class_ref(type);

		GEnumClass* genum;
		int i;

		/* Should be impossible, no need to warn.
		 */
		if (!class)
			return NULL;

		genum = G_ENUM_CLASS(class);

		printf("word:");

		/* -1 since we always have a "last" member.
		 */
		for (i = 0; i < genum->n_values - 1; i++) {
			if (i > 0)
				printf("|");
			printf("%s", genum->values[i].value_nick);
		}

		printf("\n");
	} else if (G_IS_PARAM_SPEC_BOOLEAN(pspec))
		printf("word:true|false\n");
	else if (G_IS_PARAM_SPEC_DOUBLE(pspec)) {
		GParamSpecDouble* pspec_double = (GParamSpecDouble*)pspec;

		printf("word:%g\n", pspec_double->default_value);
	} else if (G_IS_PARAM_SPEC_INT(pspec)) {
		GParamSpecInt* pspec_int = (GParamSpecInt*)pspec;

		printf("word:%d\n", pspec_int->default_value);
	} else if (G_IS_PARAM_SPEC_OBJECT(pspec))
		/* Eg. an image input or output.
		 */
		printf("file\n");
	else
		/* We can offer no useful suggestion for eg. array_int etc.
		 */
		printf("none\n");

	return NULL;
}

static gboolean
parse_main_option_completion(const gchar* option_name, const gchar* value,
	gpointer data, GError** error) {
	VipsObjectClass* class;

	if (value &&
		(class = (VipsObjectClass*)vips_type_map_all(
			g_type_from_name("VipsOperation"),
			test_class_name, (void*)value)))
		vips_argument_class_map(class,
			(VipsArgumentClassMapFn)list_operation_arg,
			NULL, NULL);
	else if (value) {
		vips_error(g_get_prgname(),
			_("'%s' is not the name of a vips operation"),
			value);
		vips_error_g(error);

		return FALSE;
	} else {
		vips_type_map_all(g_type_from_name("VipsOperation"),
			list_operation, NULL);
	}

	exit(0);
}

static GOptionEntry main_option[] = {
	{ "list", 'l', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK,
		(GOptionArgFunc)parse_main_option_list,
		N_("list objects"),
		N_("BASE-NAME") },
	{ "plugin", 'p', 0, G_OPTION_ARG_FILENAME, &main_option_plugin,
		N_("load PLUGIN"),
		N_("PLUGIN") },
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &main_option_version,
		N_("print version"), NULL },
	{ "completion", 'c', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK,
		(GOptionArgFunc)parse_main_option_completion,
		N_("print completions"),
		N_("BASE-NAME") },
	{ NULL }
};

static int
print_list(int argc, char** argv) {
	if (!argv[0] || strcmp(argv[0], "classes") == 0)
		vips_type_map_all(g_type_from_name("VipsObject"),
			list_class, NULL);
	else if (g_type_from_name(argv[0]) &&
		g_type_is_a(g_type_from_name(argv[0]), VIPS_TYPE_OBJECT)) {
		vips_type_map_all(g_type_from_name(argv[0]),
			list_class, NULL);
	} else {
		vips_error_exit("unknown operation \"%s\"", argv[0]);
	}

	return 0;
}

static int
isvips(const char* name) {
	/* If we're running uninstalled we get the lt- prefix.
	 */
	if (vips_isprefix("lt-", name))
		name += 3;

	return vips_isprefix("vips", name);
}
static int
print_help(int argc, char** argv) {
	return 0;
}

/* All our built-in actions.
 */

typedef int (*Action)(int argc, char** argv);

typedef struct _ActionEntry {
	char* name;
	char* description;
	GOptionEntry* group;
	Action action;
} ActionEntry;

static GOptionEntry empty_options[] = {
	{ NULL }
};

static ActionEntry actions[] = {
	{ "list", N_("list classes|all|operation-name"),
		&empty_options[0], print_list },

	{ "help", N_("list possible actions"),
		&empty_options[0], print_help },
};

static void
parse_options(GOptionContext* context, int* argc, char** argv) {
	char txt[1024];
	VipsBuf buf = VIPS_BUF_STATIC(txt);
	GError* error = NULL;
	int i, j;

#ifdef DEBUG
	printf("parse_options:\n");
	for (i = 0; i < *argc; i++)
		printf("%d) %s\n", i, argv[i]);
#endif /*DEBUG*/

	vips_buf_appendf(&buf, "%7s - %s\n",
		"OPER", _("execute vips operation OPER"));
	g_option_context_set_summary(context, vips_buf_all(&buf));

#ifdef G_OS_WIN32
	if (!g_option_context_parse_strv(context, &argv, &error))
#else  /*!G_OS_WIN32*/
	if (!g_option_context_parse(context, argc, &argv, &error))
#endif /*G_OS_WIN32*/
	{
		if (error) {
			fprintf(stderr, "%s\n", error->message);
			g_error_free(error);
		}

		vips_error_exit(NULL);
	}

	/* On Windows, argc will not have been updated by
	 * g_option_context_parse_strv().
	 */
	for (*argc = 0; argv[*argc]; (*argc)++)
		;

	/* Remove any "--" argument. If one of our arguments is a negative
	 * number, the user will need to have added the "--" flag to stop
	 * GOption parsing. But "--" is still passed down to us and we need to
	 * ignore it.
	 */
	for (i = 1; i < *argc; i++)
		if (strcmp(argv[i], "--") == 0) {
			for (j = i; j < *argc; j++)
				argv[j] = argv[j + 1];

			*argc -= 1;
		}
}

static GOptionGroup*
add_operation_group(GOptionContext* context, VipsOperation* user_data) {
	GOptionGroup* group;

	group = g_option_group_new("operation",
		_("Operation"), _("Operation help"), user_data, NULL);
	g_option_group_set_translation_domain(group, GETTEXT_PACKAGE);
	g_option_context_add_group(context, group);

	return group;
}

/* VIPS universal main program.
 */
int
main(int argc, char** argv) {
	char* action;
	GOptionContext* context;
	GOptionGroup* main_group;
	GOptionGroup* group;
	VipsOperation* operation;
	int i, j;
	gboolean handled;

	GError* error = NULL;

	if (VIPS_INIT(argv[0]))
		vips_error_exit(NULL);

#ifdef ENABLE_NLS
	textdomain(GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */
	setlocale(LC_ALL, "");

	{
		char* basename;

		basename = g_path_get_basename(argv[0]);
		g_set_prgname(basename);
		g_free(basename);
	}

	/* On Windows, argv is ascii-only .. use this to get a utf-8 version of
	 * the args.
	 */
#ifdef G_OS_WIN32
	argv = g_win32_get_command_line();
#endif /*G_OS_WIN32*/

#ifdef DEBUG_FATAL
	/* Set masks for debugging ... stop on any problem.
	 */
	g_log_set_always_fatal(
		G_LOG_FLAG_RECURSION |
		G_LOG_FLAG_FATAL |
		G_LOG_LEVEL_ERROR |
		G_LOG_LEVEL_CRITICAL |
		G_LOG_LEVEL_WARNING);
#endif /*!DEBUG_FATAL*/

	context = g_option_context_new(_("[ACTION] [OPTIONS] [PARAMETERS] - "
		"VIPS driver program"));

	/* Add and parse the outermost options: the ones this program uses.
	 * For example, we need
	 * to be able to spot that in the case of "--plugin ./poop.plg" we
	 * must remove two args.
	 */
	main_group = g_option_group_new(NULL, NULL, NULL, NULL, NULL);
	g_option_group_add_entries(main_group, main_option);
	vips_add_option_entries(main_group);
	g_option_group_set_translation_domain(main_group, GETTEXT_PACKAGE);
	g_option_context_set_main_group(context, main_group);

	/* We add more options later, for example as options to vips8
	 * operations. Ignore any unknown options in this first parse.
	 */
	g_option_context_set_ignore_unknown_options(context, TRUE);

	/* "vips" with no arguments does "vips --help".
	 */
	if (argc == 1) {
		char* help;

		help = g_option_context_get_help(context, TRUE, NULL);
		printf("%s", help);
		g_free(help);

		exit(0);
	}

	/* Also disable help output: we want to be able to display full help
	 * in a second pass after all options have been created.
	 */
	g_option_context_set_help_enabled(context, FALSE);

#ifdef G_OS_WIN32
	if (!g_option_context_parse_strv(context, &argv, &error))
#else  /*!G_OS_WIN32*/
	if (!g_option_context_parse(context, &argc, &argv, &error))
#endif /*G_OS_WIN32*/
	{
		if (error) {
			fprintf(stderr, "%s\n", error->message);
			g_error_free(error);
		}

		vips_error_exit(NULL);
	}

	/* On Windows, argc will not have been updated by
	 * g_option_context_parse_strv().
	 */
	for (argc = 0; argv[argc]; argc++)
		;

	if (main_option_plugin) {
#if ENABLE_MODULES
		GModule* module;

		module = g_module_open(main_option_plugin, G_MODULE_BIND_LAZY);
		if (!module) {
			vips_error_exit(_("unable to load \"%s\" -- %s"),
				main_option_plugin, g_module_error());
		}
#else  /*!ENABLE_MODULES*/
		g_warning("%s", _("plugin load disabled: "
			"libvips built without modules support"));
#endif /*ENABLE_MODULES*/
	}

	if (main_option_version)
		printf("vips-%s\n", vips_version_string());

	/* Re-enable help and unknown option detection ready for the second
	 * option parse.
	 */
	g_option_context_set_ignore_unknown_options(context, FALSE);
	g_option_context_set_help_enabled(context, TRUE);

	/* Try to find our action.
	 */
	handled = FALSE;
	action = NULL;

	/* Should we try to run the thing we are named as?
	 */
	if (!isvips(g_get_prgname()))
		action = argv[0];

	if (!action) {
		/* Look for the first non-option argument, if any, and make
		 * that our action. The parse above will have removed most of
		 * them, but --help (for example) could still remain.
		 */
		for (i = 1; i < argc; i++)
			if (argv[i][0] != '-') {
				action = argv[i];

				/* Remove the action from argv.
				 */
				for (j = i; j < argc; j++)
					argv[j] = argv[j + 1];
				argc -= 1;

				break;
			}
	}

	/* Could be one of our built-in actions.
	 */
	if (action)
		for (i = 0; i < VIPS_NUMBER(actions); i++)
			if (strcmp(action, actions[i].name) == 0) {
				group = add_operation_group(context, NULL);
				g_option_group_add_entries(group,
					actions[i].group);
				parse_options(context, &argc, argv);

				if (actions[i].action(argc - 1, argv + 1))
					vips_error_exit("%s", action);

				handled = TRUE;
				break;
			}

	/* Could be a vips8 VipsOperation.
	 */
	if (action &&
		!handled &&
		(operation = vips_operation_new(action))) {
		group = add_operation_group(context, operation);
		vips_call_options(group, operation);
		parse_options(context, &argc, argv);

		if (vips_call_argv(operation, argc - 1, argv + 1)) {
			if (argc == 1)
				vips_operation_class_print_usage(
					VIPS_OPERATION_GET_CLASS(operation));

			vips_object_unref_outputs(VIPS_OBJECT(operation));
			g_object_unref(operation);

			if (argc == 1)
				/* We don't exit with an error for something
				 * like "vips fitsload" failing, we use it to
				 * decide if an optional component has been
				 * configured. If we've been built without
				 * fits support, fitsload will fail to find
				 * the operation and we'll error with "unknown
				 * action" below.
				 */
				exit(0);
			else
				vips_error_exit(NULL);
}

		vips_object_unref_outputs(VIPS_OBJECT(operation));
		g_object_unref(operation);

		handled = TRUE;
	}

	/* vips_operation_new() sets an error msg for unknown operation.
	 */
	if (action &&
		!handled)
		vips_error_clear();

	if (action &&
		!handled) {
		vips_error_exit(_("unknown action \"%s\""), action);
	}

	/* Still not handled? We may not have called parse_options(), so
	 * --help args may not have been processed.
	 */
	if (!handled)
		parse_options(context, &argc, argv);

	g_option_context_free(context);

#ifdef G_OS_WIN32
	g_strfreev(argv);
#endif /*G_OS_WIN32*/

	vips_shutdown();

	return 0;
}
