#include "jsi.h"
#include "jsparse.h"
#include "jscompile.h"
#include "jsvalue.h"
#include "jsrun.h"
#include "jsbuiltin.h"

static void *js_defaultalloc(void *actx, void *ptr, unsigned int size)
{
	if (size == 0) {
		free(ptr);
		return NULL;
	}
	if (!ptr)
		return malloc(size);
	return realloc(ptr, size);
}

static void js_defaultpanic(js_State *J)
{
	fprintf(stderr, "libjs: uncaught exception: %s\n", js_tostring(J, -1));
	/* return to javascript to abort */
}

int js_ploadstring(js_State *J, const char *filename, const char *source)
{
	if (js_try(J))
		return 1;
	js_loadstring(J, filename, source);
	js_endtry(J);
	return 0;
}

int js_ploadfile(js_State *J, const char *filename)
{
	if (js_try(J))
		return 1;
	js_loadfile(J, filename);
	js_endtry(J);
	return 0;
}

void js_loadstring(js_State *J, const char *filename, const char *source)
{
	js_Ast *P;
	js_Function *F;

	if (js_try(J)) {
		jsP_freeparse(J);
		js_throw(J);
	}

	P = jsP_parse(J, filename, source);
	F = jsC_compile(J, P);
	jsP_freeparse(J);
	js_newscript(J, F);

	js_endtry(J);
}

void js_loadfile(js_State *J, const char *filename)
{
	FILE *f;
	char *s;
	int n, t;

	f = fopen(filename, "rb");
	if (!f) {
		js_error(J, "cannot open file: '%s'", filename);
	}

	if (fseek(f, 0, SEEK_END) < 0) {
		fclose(f);
		js_error(J, "cannot seek in file: '%s'", filename);
	}
	n = ftell(f);
	fseek(f, 0, SEEK_SET);

	s = js_malloc(J, n + 1); /* add space for string terminator */
	if (!s) {
		fclose(f);
		js_error(J, "cannot allocate storage for file contents: '%s'", filename);
	}

	t = fread(s, 1, n, f);
	if (t != n) {
		js_free(J, s);
		fclose(f);
		js_error(J, "cannot read data from file: '%s'", filename);
	}

	s[n] = 0; /* zero-terminate string containing file data */

	if (js_try(J)) {
		js_free(J, s);
		fclose(f);
		js_throw(J);
	}

	js_loadstring(J, filename, s);

	js_free(J, s);
	fclose(f);
	js_endtry(J);
}

int js_dostring(js_State *J, const char *source, int report)
{
	if (js_try(J)) {
		fprintf(stderr, "libjs: %s\n", js_tostring(J, -1));
		js_pop(J, 1);
		return 1;
	}
	js_loadstring(J, "(string)", source);
	js_pushglobal(J);
	js_call(J, 0);
	if (report)
		printf("%s\n", js_tostring(J, -1));
	js_pop(J, 1);
	js_endtry(J);
	return 0;
}

int js_dofile(js_State *J, const char *filename)
{
	if (js_try(J)) {
		fprintf(stderr, "libjs: %s\n", js_tostring(J, -1));
		js_pop(J, 1);
		return 1;
	}
	js_loadfile(J, filename);
	js_pushglobal(J);
	js_call(J, 0);
	js_pop(J, 1);
	js_endtry(J);
	return 0;
}

js_Panic js_atpanic(js_State *J, js_Panic panic)
{
	js_Panic old = J->panic;
	J->panic = panic;
	return old;
}

js_State *js_newstate(js_Alloc alloc, void *actx)
{
	js_State *J;

	if (!alloc)
		alloc = js_defaultalloc;

	J = alloc(actx, NULL, sizeof *J);
	if (!J)
		return NULL;
	memset(J, 0, sizeof(*J));
	J->actx = actx;
	J->alloc = alloc;

	J->panic = js_defaultpanic;

	J->stack = alloc(actx, NULL, JS_STACKSIZE * sizeof *J->stack);
	if (!J->stack) {
		alloc(actx, NULL, 0);
		return NULL;
	}

	J->gcmark = 1;
	J->nextref = 0;

	J->R = jsV_newobject(J, JS_COBJECT, NULL);
	J->G = jsV_newobject(J, JS_COBJECT, NULL);
	J->E = jsR_newenvironment(J, J->G, NULL);
	J->GE = J->E;

	jsB_init(J);

	return J;
}
