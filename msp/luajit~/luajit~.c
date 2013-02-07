
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include "jit.common.h"
#include "jit.gl.h"

#include "luajit.h"
#include "lauxlib.h"
#include "lualib.h"

static t_class *luajit_class;

typedef struct {
    t_pxobject	x_obj;
	void *		ob3d;
	
	void *		lua_outlet;
	
	lua_State * L;
	t_symbol * filename;
	short filepath;
	void * filewatcher;
	long autowatch;
	
} t_luajit;


t_jit_err luajit_draw(t_luajit *x);
t_jit_err luajit_dest_closing(t_luajit *x);
t_jit_err luajit_dest_changed(t_luajit *x);

void luajit_assist(t_luajit *x, void *b, long m, long a, char *s);
void *luajit_new(t_symbol *s, long argc, t_atom *argv);
void luajit_free(t_luajit * x);

void luajit_dsp64(t_luajit *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);

void scale_perform64_method(t_luajit *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void luajit_perform64_method(t_luajit *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);

t_max_err luajit_filename_set(t_luajit *x, t_object *attr, long argc, t_atom *argv);
void luajit_doread(t_luajit *x);
void luajit_filechanged(t_luajit *x, char *filename, short path);
lua_State * luajit_newstate(t_luajit *x);
void luajit_dostring(t_luajit *x, C74_CONST char * text);
t_max_err luajit_notify(t_luajit *x, t_symbol *s, t_symbol *msg, void *sender, void *data);


int C74_EXPORT main(void)
{
	t_class * maxclass;
	void *ob3d;
	
	common_symbols_init();
	
	maxclass = class_new("luajit~", (method)luajit_new, (method)luajit_free, sizeof(t_luajit), 0L, A_GIMME, 0);

	class_addmethod(maxclass, (method)luajit_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(maxclass, (method)luajit_assist, "assist", A_CANT, 0);
	class_addmethod(maxclass, (method)luajit_notify, "notify", A_CANT, 0);
	class_addmethod(maxclass, (method)luajit_filechanged, "filechanged", A_CANT, 0);
	//class_setname("*~","luajit~"); // because the filename on disk is different from the object name in Max
	
	CLASS_ATTR_SYM(maxclass, "file", 0, t_luajit, filename);
	CLASS_ATTR_LABEL(maxclass,	"file",	0,	"Lua script file name to load");
	CLASS_ATTR_ACCESSORS(maxclass, "file", 0, luajit_filename_set);
	
	CLASS_ATTR_LONG(maxclass, "autowatch", 0, t_luajit, autowatch); 
	CLASS_ATTR_LABEL(maxclass,	"autowatch",	0,	"automatically reload file when changed on disk");
	
	CLASS_ATTR_OBJ(maxclass, "lua_outlet", 0, t_luajit, lua_outlet);
	CLASS_ATTR_INVISIBLE(maxclass, "lua_outlet", 0);
	
	class_dspinit(maxclass);

	// set up object extension for 3d object, customized with flags
	ob3d = jit_ob3d_setup(maxclass, 
				calcoffset(t_luajit, ob3d), 
				JIT_OB3D_NO_MATRIXOUTPUT);
				
	// define our OB3D draw method.  called in automatic mode by 
	// jit.gl.render or otherwise through ob3d when banged. this 
	// method is A_CANT because our draw setup needs to happen 
	// in the ob3d beforehand to initialize OpenGL state 
	jit_class_addmethod(maxclass, 
		(method)luajit_draw, "ob3d_draw", A_CANT, 0L);
	
	// define our dest_closing and dest_changed methods. 
	// these methods are called by jit.gl.render when the 
	// destination context closes or changes: for example, when 
	// the user moves the window from one monitor to another. Any 
	// resources your object keeps in the OpenGL machine 
	// (e.g. textures, display lists, vertex shaders, etc.) 
	// will need to be freed when closing, and rebuilt when it has 
	// changed. 
	jit_class_addmethod(maxclass, (method)luajit_dest_closing, "dest_closing", A_CANT, 0L);
	jit_class_addmethod(maxclass, (method)luajit_dest_changed, "dest_changed", A_CANT, 0L);

	// must register for ob3d use
	jit_class_addmethod(maxclass, (method)jit_object_register, "register", A_CANT, 0L);
	
	class_register(CLASS_BOX, maxclass);
	luajit_class = maxclass;

	return 0;
}

t_jit_err luajit_draw(t_luajit *x) {
	t_jit_err result = JIT_ERR_NONE;
	int debug_traceback;
	
	lua_getfield(x->L, LUA_REGISTRYINDEX, "debug.traceback");
	debug_traceback = lua_gettop(x->L);	
	lua_getglobal(x->L, "draw");
	if (lua_pcall(x->L, 0, 0, debug_traceback)) {
		object_error((t_object *)x, lua_tostring(x->L, -1));
	}
	
	// clear debug.traceback:
	lua_settop(x->L, 0);
	
	return result;
}

t_jit_err luajit_dest_closing(t_luajit *x) {
	return JIT_ERR_NONE;
}
t_jit_err luajit_dest_changed(t_luajit *x) {
	return JIT_ERR_NONE;
}

t_max_err luajit_filename_set(t_luajit *x, t_object *attr, long argc, t_atom *argv)
{
	x->filename = atom_getsym(argv);
	defer(x, (method)luajit_doread, 0, 0, 0);
	return 0;
}

void luajit_filechanged(t_luajit *x, char *filename, short path) {
	object_post((t_object *)x, "file changed, reloading: %s", filename);
	if (x->autowatch) {
		defer(x, (method)luajit_doread, 0, 0, 0);
	}
}

void luajit_doread(t_luajit *x) {
	char filename[MAX_PATH_CHARS];
	short path;
	t_fourcc filetype = 'TEXT';
	t_fourcc outtype;
	
	t_filehandle fh;
	char **texthandle;
	
	strcpy(filename, x->filename->s_name);    // must copy symbol before calling locatefile_extended
	if (locatefile_extended(filename, &path, &outtype, &filetype, 1)) { // non-zero: not found
		object_error((t_object *)x, "%s: not found", x->filename->s_name);
		return;
	}
	
	// we have a file:
	if (path_opensysfile(filename, path, &fh, READ_PERM)) {
		object_error((t_object *)x, "error opening %s", filename);
		return;
	}
	
	// store new path:
	x->filepath = path;
		
	// allocate some empty memory to receive text
	texthandle = sysmem_newhandle(0);
	if (sysfile_readtextfile(fh, texthandle, 0, TEXT_NULL_TERMINATE)) {
		object_error((t_object *)x, "failed to read %s", x->filename->s_name);
	} else {
	
		// now run it:
		if (luajit_newstate(x)) {
		
			// start filewatching:
			if (x->filewatcher) {
				filewatcher_stop(x->filewatcher);
				object_free(x->filewatcher);
			}
			x->filewatcher = filewatcher_new((t_object *)x, x->filepath, x->filename->s_name);
			filewatcher_start(x->filewatcher);	
		
			luajit_dostring(x, *texthandle);
		}
		
	}
	sysfile_close(fh);
	sysmem_freehandle(texthandle);
}

lua_State * luajit_newstate(t_luajit *x) {
	lua_State * L;
	char local_path[MAX_PATH_CHARS];
	
	L = lua_open();
	if (L == 0) {
		object_error((t_object *)x, "failed to allocate Lua");
	} else {
		// initialize L:
		luaL_openlibs(L);
		
		// cache debug.traceback:
		lua_getglobal(L, "debug");
		lua_getfield(L, -1, "traceback");
		lua_setfield(L, LUA_REGISTRYINDEX, "debug.traceback");
		
		// add the local path to the package path:
		if (path_toabsolutesystempath(x->filepath, "", local_path)) {
			object_warn((t_object *)x, "problem resolving package path");
		} else {	
			if (luaL_loadstring(L, "local path = ...; package.path = string.format('%s/?.lua;%s/?/init.lua;%s', path, path, package.path)")) {
				object_error((t_object *)x, lua_tostring(L, -1));
			} else {
				lua_pushstring(L, local_path);
				if (lua_pcall(L, 1, LUA_MULTRET, 0)) {
					object_error((t_object *)x, lua_tostring(L, -1));
				}
			}
		}	
		
		// TODO: push x as 'this'?
		lua_pushlightuserdata(L, x);
		lua_setglobal(L, "this");
		
		// leave it nice:
		lua_settop(L, 0);
		
		// swap states:
		if (x->L) lua_close(x->L);
		x->L = L;
	}
	
	return L;
}

void luajit_dostring(t_luajit *x, C74_CONST char * text) {
	if (luaL_dostring(x->L, text)) {
		object_error((t_object *)x, lua_tostring(x->L, -1));
	}
}

t_max_err luajit_notify(t_luajit *x, t_symbol *s, t_symbol *msg, void *sender, void *data) {
	t_symbol *attrname;
 
	if (msg == gensym("attr_modified")) {       // check notification type
		attrname = (t_symbol *)object_method((t_object *)data, gensym("getname"));
		object_post((t_object *)x, "changed attr name is %s",attrname->s_name);
	} else {
		object_post((t_object *)x, "notify %s (self %d)", msg->s_name, sender == x);
	}

	return 0;
}

void luajit_dsp64(t_luajit *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags) {
	dsp_add64(dsp64, (t_object*) x, (t_perfroutine64) luajit_perform64_method, 0, 0);
}

void luajit_perform64_method(t_luajit *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	t_double *in1 = ins[0];
	t_double *in2 = ins[1];
	t_double *out = outs[0];
	t_double ftmp;
	
//	if (IS_DENORM_DOUBLE(*in1) || IS_DENORM_DOUBLE(*in2)) {
//		static int counter = 0; 
//		post("luajit~ (%p): saw denorm (%d)", x, counter++); 
//	}

	while (sampleframes--) {
		ftmp = *in1++ * *in2++;
		FIX_DENORM_NAN_DOUBLE(ftmp);
		*out++ = ftmp;
	}
}


void luajit_assist(t_luajit *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_OUTLET)
		sprintf(s,"(Signal) Multiplication Result");
	else {
		switch (a) {	
		case 0:
			sprintf(s,"(Signal/Float) This * Right Inlet");
			break;
		case 1:
			sprintf(s,"(Signal/Float) Left Inlet * This");
			break;
		}
	}
}

void *luajit_new(t_symbol *s, long argc, t_atom *argv) {
    long attrstart;
	t_symbol *dest_name_sym = _jit_sym_nothing;
	t_luajit *x = (t_luajit*) object_alloc((t_class*) luajit_class);
	if (x) {
	
		// add a general purpose outlet (rightmost)
		//max_jit_obex_dumpout_set(x, outlet_new(x,NULL));
		
		x->lua_outlet = outlet_new(x, 0);
		
		// get first normal arg, the destination name
		attrstart = max_jit_attr_args_offset(argc,argv);
		if (attrstart && argv) {
			jit_atom_arg_getsym(&dest_name_sym, 0, attrstart, argv);
		}
		jit_ob3d_new(x, dest_name_sym);
		
		// configure audio:
		dsp_setup((t_pxobject *)x,2);
		outlet_new((t_pxobject *)x, "signal");
		
		x->filename = _sym_none;
		x->filepath = 0;
		x->autowatch = 1;
		
		if (luajit_newstate(x) == 0) {
			luajit_free(x);
			return 0;
		}
		
		attr_args_process(x, argc, argv);
	}
	return (x);
}


void luajit_free(t_luajit * x)
{
	dsp_free((t_pxobject *)x);
	
	if (x->L) lua_close(x->L);
	
	// free resources associated with our obex entry
	jit_ob3d_free(x);
	max_jit_object_free(x);
}



