/**
	@file
	freenect3d - a max object
*/

extern "C" {
	#include "ext.h"		
	#include "ext_obex.h"

	#include "z_dsp.h"

	#include "jit.common.h"
	#include "jit.gl.h"

	#include "libfreenect.h"
}

#include <pthread.h>
#include <new>

#ifdef __APPLE__
	#include "CoreFoundation/CoreFoundation.h"
    #define MY_PLUGIN_BUNDLE_IDENTIFIER "com.cycling74.freenect3d"
#endif

t_class *freenect3d_class;

freenect_context * f_ctx = NULL;
pthread_t capture_thread;	// Windows?
int capturing = 0;

char bundle_path[MAX_PATH_CHARS];

class t_freenect3d {
public:
	t_object	ob;			// the object itself (must be first)
	void *		ob3d;		// OpenGL scene state
	
	void *		outlet_rgb;
	
	void *		rgb_mat;
	void *		rgb_mat_wrapper;
	t_atom		rgb_name[1];
	
	freenect_device  *device;
	uint8_t *	rgb_back;
	uint16_t *	depth_back;
		
	t_freenect3d() {
		device = 0;
		info();
		
		rgb_mat_wrapper = jit_object_new(gensym("jit_matrix_wrapper"), jit_symbol_unique(), 0, NULL);
		rgb_mat = jit_object_method(rgb_mat_wrapper, _jit_sym_getmatrix);
		
		// create the internal data:
		t_jit_matrix_info info;
		jit_matrix_info_default(&info);
		info.flags |= JIT_MATRIX_DATA_PACK_TIGHT;
		info.planecount = 3;
		info.type = gensym("char");
		info.dimcount = 2;
		info.dim[0] = 640;
		info.dim[1] = 480;
		jit_object_method(rgb_mat, _jit_sym_setinfo_ex, &info);
		
		jit_object_method(rgb_mat, _jit_sym_clear);
		//jit_object_method(rgb_mat, _jit_sym_getinfo, &info); // for dimstride
		jit_object_method(rgb_mat, _jit_sym_getdata, &rgb_back);
		
		// cache name:
		atom_setsym(rgb_name, jit_attr_getsym(rgb_mat_wrapper, _jit_sym_name));
			
		depth_back = (uint16_t *)sysmem_newptr(640*480 * sizeof(uint16_t));
	}

	~t_freenect3d() {
		close();
		
		
		if (rgb_mat_wrapper) {
			object_free(rgb_mat_wrapper);
			rgb_mat_wrapper = NULL;
		}
		
		/*
		if (rgb_mat) {
			object_free(rgb_mat);
			rgb_mat = NULL;
		}
		*/
		
		free(depth_back);
	}
	
	void info() {
		
	}
	
	void open(t_symbol *s, long argc, t_atom *argv) {
	
		if(device){
			object_post(&ob, "A device is already open.");
			return;
		}
		
		// mark one additional device:
		capturing++;
		
		if(!f_ctx){
			if (pthread_create(&capture_thread, NULL, capture_threadfunc, this)) {
				object_error(&ob, "Failed to create capture thread.");
				capturing = 0;
				return;
			}
			while(!f_ctx){
				sleep(0);
			}
		}
		
		int ndevices = freenect_num_devices(f_ctx);
		if(!ndevices){
			object_post(&ob, "Could not find any connected Kinect device. Are you sure the power cord is plugged-in?");
			capturing = 0;
			return;
		}
		
		if (argc > 0 && atom_gettype(argv) == A_SYM) {
			const char * serial = atom_getsym(argv)->s_name;
			object_post(&ob, "opening device %s", serial);
			if (freenect_open_device_by_camera_serial(f_ctx, &device, serial) < 0) {
				object_error(&ob, "failed to open device %s", serial);
				device = NULL;
			}
		} else {
			int devidx = 0;
			if (argc > 0 && atom_gettype(argv) == A_LONG) devidx = atom_getlong(argv);
			
			object_post(&ob, "opening device %d", devidx);
			if (freenect_open_device(f_ctx, &device, devidx) < 0) {
				object_error(&ob, "failed to open device %d", devidx);
				device = NULL;
			}
		}
		
		if (!device) {
			// failed to create device:
			capturing--;
			return;
		}
		
	
		freenect_set_user(device, this);
		freenect_set_depth_callback(device, depth_callback);
		freenect_set_video_callback(device, rgb_callback);
		
		freenect_set_video_buffer(device, rgb_back);
		freenect_set_depth_buffer(device, depth_back);
		
		freenect_set_video_mode(device, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
		freenect_set_depth_mode(device, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT));
		
		freenect_set_led(device,LED_RED);
		
		freenect_start_depth(device);
		freenect_start_video(device);
	}
	
	void close() {
		if(!device)return;
		
		freenect_set_led(device,LED_BLINK_GREEN);
		freenect_close_device(device);
		device = NULL;
		
		// mark one less device running:
		capturing--;
	}
	
	static void rgb_callback(freenect_device *dev, void *pixels, uint32_t timestamp){
		t_freenect3d *x = (t_freenect3d *)freenect_get_user(dev);
		if(!x)return;
		
	}
	
	static void depth_callback(freenect_device *dev, void *pixels, uint32_t timestamp){
		t_freenect3d *x = (t_freenect3d *)freenect_get_user(dev);
		if(!x)return;
		
	}
	
	static void *capture_threadfunc(void *arg) {
		t_freenect3d *x = (t_freenect3d *)arg;
		
		freenect_context *context;		
		if(!f_ctx){
			if (freenect_init(&context, NULL) < 0) {
				error("freenect_init() failed");
				goto out;
			}
		}
		f_ctx = context;
		
		capturing = 1;
		post("freenect starting processing");
		while (capturing > 0) {
			//if(f_ctx->first){
				if(freenect_process_events(f_ctx) < 0){
					error("Freenect could not process events.");
					break;
				}
			//}
			sleep(0);
		}
		post("freenect finished processing");
		
	out: 
		freenect_shutdown(f_ctx);
		f_ctx = NULL;
		pthread_exit(NULL);
		return NULL;
	}
};

void freenect3d_bang(t_freenect3d * x) {
	// output rgb and depth raw data as jitter matrices.
	
	// TODO: don't output if @unique 1 and the timestamp hasn't changed
	
	outlet_anything(x->outlet_rgb, _jit_sym_jit_matrix, 1, x->rgb_name);
}

t_jit_err freenect3d_draw(t_freenect3d *x) {
	t_jit_err result = JIT_ERR_NONE;
	return result;
}

t_jit_err freenect3d_dest_closing(t_freenect3d *x) {
	return JIT_ERR_NONE;
}
t_jit_err freenect3d_dest_changed(t_freenect3d *x) {
	return JIT_ERR_NONE;
}

// object_notify
t_max_err freenect3d_notify(t_freenect3d *x, t_symbol *s, t_symbol *msg, void *sender, void *data) {
	t_symbol *attrname;
 
	if (msg == gensym("attr_modified")) {       // check notification type
		attrname = (t_symbol *)object_method((t_object *)data, gensym("getname"));
		object_post((t_object *)x, "changed attr name is %s",attrname->s_name);
	} else {
		object_post((t_object *)x, "notify %s (self %d)", msg->s_name, sender == x);
	}

	return 0;
}

void freenect3d_info(t_freenect3d *x) {
	x->info();
}


void freenect3d_jit_matrix(t_freenect3d *x, t_symbol *s, long argc, t_atom *argv)
{
	post("got matrix");
	
	/*
	long i;
	t_atom_long jump[JIT_MATRIX_MAX_PLANECOUNT];
	void *o, *p;

	o = max_jit_obex_jitob_get(x);
	jit_attr_getlong_array(o, gensym("jump"), JIT_MATRIX_MAX_PLANECOUNT, jump);
	
	for(i=0; i < x->outlets; i++) {
		p=max_jit_mop_getoutput(x,i+1);
		jit_attr_setlong(p,gensym("minplanecount"),jump[i]);
		jit_attr_setlong(p,gensym("maxplanecount"),jump[i]);
	}
	
	max_jit_mop_jit_matrix(x,s,argc,argv);
	*/
}

void freenect3d_assist(t_freenect3d *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) { // inlet
		if (a == 0) {
			sprintf(s, "messages in, bang to report orientation");
		} else {
			sprintf(s, "I am inlet %ld", a);
		}
	} else {	// outlet
		if (a == 0) {
			sprintf(s, "HMD orientation quaternion (list)"); 
		} else if (a == 1) {
			sprintf(s, "HMD properties (messages)"); 
		} else {
			sprintf(s, "I am outlet %ld", a); 
		}
	}
}


void freenect3d_free(t_freenect3d *x) {
	x->~t_freenect3d();

	// free resources associated with our obex entry
	jit_ob3d_free(x);
	max_jit_object_free(x);
}

void freenect3d_open(t_freenect3d *x, t_symbol *s, long argc, t_atom *argv) {
	x->open(s, argc, argv);
}

void freenect3d_close(t_freenect3d *x) {
	x->close();
}

void *freenect3d_new(t_symbol *s, long argc, t_atom *argv)
{
	t_freenect3d *x = NULL;
    //long i;
	long attrstart;
	t_symbol *dest_name_sym = _jit_sym_nothing;
	
	if (x = (t_freenect3d *)object_alloc(freenect3d_class)) {
	
		// initialize in-place:
		x = new ((void *)x) t_freenect3d();
		
		// get first normal arg, the destination name
		attrstart = max_jit_attr_args_offset(argc,argv);
		if (attrstart && argv) {
			jit_atom_arg_getsym(&dest_name_sym, 0, attrstart, argv);
		}
		jit_ob3d_new(x, dest_name_sym);
		
		// add a general purpose outlet (rightmost)
		max_jit_obex_dumpout_set(x, outlet_new(x,NULL));
		//x->outlet_c = outlet_new(x, 0);
		//x->outlet_q = listout(x);
		
		x->outlet_rgb = outlet_new(x, "jit_matrix");
		
		// default attrs:
		
		// apply attrs:
		attr_args_process(x, argc, argv);
	}
	return (x);
}

int C74_EXPORT main(void) {	
	t_class *maxclass;
	void *ob3d;
	
	#ifdef __APPLE__
		CFBundleRef mainBundle = CFBundleGetBundleWithIdentifier(CFSTR(MY_PLUGIN_BUNDLE_IDENTIFIER));
		CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
		if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)bundle_path, MAX_PATH_CHARS))
		{
			// error!
		}
		CFRelease(resourcesURL);
		
		printf("bundle path %s\n", bundle_path);
	#endif
	
	common_symbols_init();
	
	maxclass = class_new("freenect3d", (method)freenect3d_new, (method)freenect3d_free, (long)sizeof(t_freenect3d), 
				  0L, A_GIMME, 0);
				  
	class_addmethod(maxclass, (method)freenect3d_assist, "assist", A_CANT, 0);
	class_addmethod(maxclass, (method)freenect3d_notify, "notify", A_CANT, 0);
	
	class_addmethod(maxclass, (method)freenect3d_jit_matrix, "jit_matrix", A_GIMME, 0); 
	class_addmethod(maxclass, (method)freenect3d_bang, "bang", 0);
	class_addmethod(maxclass, (method)freenect3d_info, "info", 0);
	class_addmethod(maxclass, (method)freenect3d_open, "open", A_GIMME, 0);
	class_addmethod(maxclass, (method)freenect3d_close, "close", 0);
	
	// set up object extension for 3d object, customized with flags
	ob3d = jit_ob3d_setup(maxclass, 
				calcoffset(t_freenect3d, ob3d), 
				JIT_OB3D_NO_MATRIXOUTPUT);
				
	// define our OB3D draw method.  called in automatic mode by 
	// jit.gl.render or otherwise through ob3d when banged. this 
	// method is A_CANT because our draw setup needs to happen 
	// in the ob3d beforehand to initialize OpenGL state 
	jit_class_addmethod(maxclass, 
		(method)freenect3d_draw, "ob3d_draw", A_CANT, 0L);
	
	// define our dest_closing and dest_changed methods. 
	// these methods are called by jit.gl.render when the 
	// destination context closes or changes: for example, when 
	// the user moves the window from one monitor to another. Any 
	// resources your object keeps in the OpenGL machine 
	// (e.g. textures, display lists, vertex shaders, etc.) 
	// will need to be freed when closing, and rebuilt when it has 
	// changed. 
	jit_class_addmethod(maxclass, (method)freenect3d_dest_closing, "dest_closing", A_CANT, 0L);
	jit_class_addmethod(maxclass, (method)freenect3d_dest_changed, "dest_changed", A_CANT, 0L);
	
	// must register for ob3d use
	jit_class_addmethod(maxclass, (method)jit_object_register, "register", A_CANT, 0L);
	
	class_register(CLASS_BOX, maxclass); /* CLASS_NOBOX */
	freenect3d_class = maxclass;
	return 0;
}
