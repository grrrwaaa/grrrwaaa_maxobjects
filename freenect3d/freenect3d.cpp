/**
	@file
	freenect3d - a max object
	
	
	Q: is it better to have a separate freenect_context per device?
	
*/

extern "C" {
	#include "ext.h"		
	#include "ext_obex.h"
	#include "ext_dictionary.h"
	#include "ext_dictobj.h"
	#include "ext_systhread.h"

	#include "z_dsp.h"

	#include "jit.common.h"
	#include "jit.gl.h"

	#include "libfreenect.h"
}


//#include <pthread.h>
#include <new>

#define DEPTH_WIDTH 640
#define DEPTH_HEIGHT 480

#ifdef __APPLE__
	#include "CoreFoundation/CoreFoundation.h"
    #define MY_PLUGIN_BUNDLE_IDENTIFIER "com.cycling74.freenect3d"
#endif

t_class *freenect3d_class;

freenect_context * f_ctx = NULL;
t_systhread capture_thread;	// Windows?
int capturing = 0;

char bundle_path[MAX_PATH_CHARS];

class t_freenect3d {
public:

	struct vec2i { int x, y; };
	struct vec2f { float x, y; };
	struct vec3f { float x, y, z; };

	t_object	ob;			// the object itself (must be first)
	
	void *		outlet_cloud;
	void *		outlet_rgb;
	void *		outlet_depth;
	void *		outlet_msg;
	
	// attributes:
	vec2f		depth_focal;
	vec2f		depth_center;
	float		depth_base, depth_offset;
	int			unique;
	
	// cloud matrix for output:
	void *		cloud_mat;
	void *		cloud_mat_wrapper;
	t_atom		cloud_name[1];
	vec3f *		cloud_back;
	
	// rgb matrix for raw output:
	void *		rgb_mat;
	void *		rgb_mat_wrapper;
	t_atom		rgb_name[1];
	uint8_t *	rgb_back;
	
	// depth matrix for raw output:
	void *		depth_mat;
	void *		depth_mat_wrapper;
	t_atom		depth_name[1];
	uint32_t *	depth_back;
	
	// freenect:
	freenect_device  *device;
	
	volatile char new_rgb_data;
	volatile char new_depth_data;
	volatile char new_cloud_data;
	
	// internal data:
	vec2i *		depth_map_data;
	uint16_t *	depth_data;
		
	t_freenect3d() {
	
		device = 0;
		unique = 1;
		
		new_rgb_data = 0;
		new_depth_data = 0;
		new_cloud_data = 0;
		
		// these should be attributes?
		// or should freenect3d accept a dict?
		depth_base = 0.085;
		depth_offset = 0.0011;
		depth_center.x = 314;
		depth_center.y = 241;
		depth_focal.x = 597;
		depth_focal.y = 597;
		
		/*
		T 0.025 0.000097 0.00558
		R1 1 0 0
		R2 0 1 0
		R3 0 0 1
		*/
		
		// create matrices:
		t_jit_matrix_info info;
		
		rgb_mat_wrapper = jit_object_new(gensym("jit_matrix_wrapper"), jit_symbol_unique(), 0, NULL);
		rgb_mat = jit_object_method(rgb_mat_wrapper, _jit_sym_getmatrix);
		// create the internal data:
		jit_matrix_info_default(&info);
		info.flags |= JIT_MATRIX_DATA_PACK_TIGHT;
		info.planecount = 3;
		info.type = gensym("char");
		info.dimcount = 2;
		info.dim[0] = DEPTH_WIDTH;
		info.dim[1] = DEPTH_HEIGHT;
		jit_object_method(rgb_mat, _jit_sym_setinfo_ex, &info);
		jit_object_method(rgb_mat, _jit_sym_clear);
		jit_object_method(rgb_mat, _jit_sym_getdata, &rgb_back);
		// cache name:
		atom_setsym(rgb_name, jit_attr_getsym(rgb_mat_wrapper, _jit_sym_name));
		
		depth_mat_wrapper = jit_object_new(gensym("jit_matrix_wrapper"), jit_symbol_unique(), 0, NULL);
		depth_mat = jit_object_method(depth_mat_wrapper, _jit_sym_getmatrix);
		// create the internal data:
		jit_matrix_info_default(&info);
		info.flags |= JIT_MATRIX_DATA_PACK_TIGHT;
		info.planecount = 1;
		info.type = gensym("long");
		info.dimcount = 2;
		info.dim[0] = DEPTH_WIDTH;
		info.dim[1] = DEPTH_HEIGHT;
		jit_object_method(depth_mat, _jit_sym_setinfo_ex, &info);
		jit_object_method(depth_mat, _jit_sym_clear);
		jit_object_method(depth_mat, _jit_sym_getdata, &depth_back);
		// cache name:
		atom_setsym(depth_name, jit_attr_getsym(depth_mat_wrapper, _jit_sym_name));
		
		cloud_mat_wrapper = jit_object_new(gensym("jit_matrix_wrapper"), jit_symbol_unique(), 0, NULL);
		cloud_mat = jit_object_method(cloud_mat_wrapper, _jit_sym_getmatrix);
		// create the internal data:
		jit_matrix_info_default(&info);
		info.flags |= JIT_MATRIX_DATA_PACK_TIGHT;
		info.planecount = 3;
		info.type = gensym("float32");
		info.dimcount = 1;
		info.dim[0] = DEPTH_WIDTH * DEPTH_HEIGHT;
		jit_object_method(cloud_mat, _jit_sym_setinfo_ex, &info);
		jit_object_method(cloud_mat, _jit_sym_clear);
		jit_object_method(cloud_mat, _jit_sym_getdata, &cloud_back);
		// cache name:
		atom_setsym(cloud_name, jit_attr_getsym(cloud_mat_wrapper, _jit_sym_name));
			
		// depth buffer doesn't use a jit_matrix, because uint16_t is not a Jitter type:
		depth_data = (uint16_t *)sysmem_newptr(DEPTH_WIDTH*DEPTH_HEIGHT * sizeof(uint16_t));
		depth_map_data = (vec2i *)sysmem_newptr(DEPTH_WIDTH*DEPTH_HEIGHT * sizeof(vec2i));
		
		// init depth_map with default data:
		for (int i=0, y=0; y<DEPTH_HEIGHT; y++) {
			for (int x=0; x<DEPTH_WIDTH; x++, i++) {
				depth_map_data[i].x = x;
				depth_map_data[i].y = y;
			}
		}
	}

	~t_freenect3d() {
		close();
		
		if (rgb_mat_wrapper) {
			object_free(rgb_mat_wrapper);
			rgb_mat_wrapper = NULL;
		}
		if (depth_mat_wrapper) {
			object_free(depth_mat_wrapper);
			depth_mat_wrapper = NULL;
		}
		if (cloud_mat_wrapper) {
			object_free(cloud_mat_wrapper);
			cloud_mat_wrapper = NULL;
		}
		
		sysmem_freeptr(depth_data);
		sysmem_freeptr(depth_map_data);
	}
	
	void getdevlist() {
		t_atom a[8];
		int i, flags;
		struct freenect_device_attributes* attribute_list;
		struct freenect_device_attributes* attribute;
	
		// list devices:
		if (!f_ctx) return;
		
		int num_devices = freenect_list_device_attributes(f_ctx, &attribute_list);
		
		i = 0;
		attribute = attribute_list;
		while (attribute) {
			atom_setsym(a+i, gensym(attribute->camera_serial));
			attribute = attribute->next;
		}
		outlet_anything(outlet_msg, gensym("devlist"), num_devices, a);
		
		freenect_free_device_attributes(attribute_list);
		
		//post("supported flags: %d", freenect_supported_subdevices());
	}
	
	void dictionary(t_symbol *s, long argc, t_atom *argv) {
		
		t_dictionary	*d = dictobj_findregistered_retain(s);
		long			numkeys = 0;
		t_symbol		**keys = NULL;
		int				i;
		int				size;

		if (!d) {
			object_error(&ob, "unable to reference dictionary named %s", s);
			return;
		}


		dictionary_getkeys_ordered(d, &numkeys, &keys);
		for (i=0; i<numkeys; i++) {
			long	argc = 0;
			t_atom	*argv = NULL;
			t_symbol * key = keys[i];
			
			post("key %s", keys[i]->s_name);
			
			/*
			key R
			key T
			key depth_base_and_offset
			key depth_distortion
			key depth_intrinsics
			key depth_size
			key raw_depth_size
			key raw_rgb_size
			key rgb_distortion
			key rgb_intrinsics
			key rgb_size
			*/
			
			if (key == gensym("depth_base_and_offset")) {
				
			}

			/*
			
			dictionary_getatoms(d, keys[i], &argc, &argv);
			if (argc == 1 && atom_gettype(argv) == A_OBJ && object_classname(atom_getobj(argv)) == _sym_dictionary) {
				t_atom			a[2];
				t_dictionary	*d2 = (t_dictionary *) atom_getobj(argv);
				t_symbol		*d2_name = dictobj_namefromptr(d2);

				// if we have a dictionary, but it doesn't have a name,
				// then we register it so that it can be passed out and accessed by other dict objects
				// we don't want to leak this memory, however, so we track it with a linklist and clear it when we are done
				//
				// this means that operations on the output from dict.iter must be synchronous
				// the dict world generally presupposes synchronous operation, so maybe it's not that bad?

				if (!subdictionaries) {
					subdictionaries = linklist_new();
					linklist_flags(subdictionaries, OBJ_FLAG_DATA);
				}

				if (!d2_name) {
					d2 = dictobj_register(d2, &d2_name);
					linklist_append(subdictionaries, d2);
				}

				atom_setsym(a, _sym_dictionary);
				atom_setsym(a+1, d2_name);
				outlet_anything(x->outlet, keys[i], 2, a);
			}
			else
				outlet_anything(x->outlet, keys[i], argc, argv);
			*/
		}

		dictobj_release(d);
		if (keys)
			sysmem_freeptr(keys);

		
	}
	
	void open(t_symbol *s, long argc, t_atom *argv) {
	
		if (device){
			object_post(&ob, "A device is already open.");
			return;
		}
		
		// mark one additional device:
		capturing++;
		
		if(!f_ctx){
			long priority = 0; // maybe increase?
			if (systhread_create((method)&capture_threadfunc, this, 0, priority, 0, &capture_thread)) {
				object_error(&ob, "Failed to create capture thread.");
				capturing = 0;
				return;
			}
			while(!f_ctx){
				systhread_sleep(0);
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
		freenect_set_depth_buffer(device, depth_data);
		
		freenect_set_video_mode(device, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
		freenect_set_depth_mode(device, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT));
		
		freenect_set_led(device,LED_RED);
		
		freenect_start_depth(device);
		freenect_start_video(device);
	}
	
	void close() {
		if(!device) return;
		
		freenect_set_led(device,LED_BLINK_GREEN);
		freenect_close_device(device);
		device = NULL;
		
		// mark one less device running:
		capturing--;
	}
	
	void accel() {
		t_atom a[3];
		double x, y, z;
		
		
		if (!device) return;
		
		freenect_update_tilt_state(device);
		freenect_raw_tilt_state * state = freenect_get_tilt_state(device);
		
		freenect_get_mks_accel(state, &x, &y, &z);
		atom_setfloat(a+0, x);
		atom_setfloat(a+1, y);
		atom_setfloat(a+2, z);
		outlet_anything(outlet_msg, gensym("accel"), 3, a);
	}
	
	void led(int option) {
		if (!device) return;
		
//		LED_OFF              = 0, /**< Turn LED off */
//		LED_GREEN            = 1, /**< Turn LED to Green */
//		LED_RED              = 2, /**< Turn LED to Red */
//		LED_YELLOW           = 3, /**< Turn LED to Yellow */
//		LED_BLINK_GREEN      = 4, /**< Make LED blink Green */
//		// 5 is same as 4, LED blink Green
//		LED_BLINK_RED_YELLOW = 6, /**< Make LED blink Red/Yellow */
		freenect_set_led(device, (freenect_led_options)option);
	}
	
	void depth_map(t_symbol * name) {
		t_jit_matrix_info in_info;
		long in_savelock;
		char * in_bp;
		t_jit_err err = 0;
		
		// get matrix from name:
		void * in_mat = jit_object_findregistered(name);
		if (!in_mat) {
			object_error(&ob, "failed to acquire matrix");
			err = JIT_ERR_INVALID_INPUT;
			goto out;
		}
		
		// lock it:
		in_savelock = (long)jit_object_method(in_mat, _jit_sym_lock, 1);
		
		// first ensure the type is correct:
		jit_object_method(in_mat, _jit_sym_getinfo, &in_info);
		jit_object_method(in_mat, _jit_sym_getdata, &in_bp);
		if (!in_bp) {
			err = JIT_ERR_INVALID_INPUT;
			goto unlock;
		}
		
		if (in_info.planecount != 2) {
			err = JIT_ERR_MISMATCH_PLANE;
			goto unlock;
		}
		
		if (in_info.type != _jit_sym_float32) {
			err = JIT_ERR_MISMATCH_TYPE;
			goto unlock;
		}
		
		if (in_info.dimcount != 2 || in_info.dim[0] != DEPTH_WIDTH || in_info.dim[1] != DEPTH_HEIGHT) {
			err = JIT_ERR_MISMATCH_DIM;
			goto unlock;
		}
		//
//		// create the internal data:
//		jit_matrix_info_default(&info);
//		info.flags |= JIT_MATRIX_DATA_PACK_TIGHT;
//		info.planecount = 3;
//		info.type = gensym("float32");
//		info.dimcount = 1;
//		info.dim[0] = DEPTH_WIDTH * DEPTH_HEIGHT;
//		jit_object_method(cloud_mat, _jit_sym_clear);

		// copy matrix data into depth map:
		for (int i=0, y=0; y<DEPTH_HEIGHT; y++) {
			// get row pointer:
			char * ip = in_bp + y*in_info.dimstride[1];
			
			for (int x=0; x<DEPTH_WIDTH; x++, i++) {
				
				// convert column pointer to vec2f:
				const vec2f& v = *(vec2f *)(ip);
				
				//post("%i %i: %i %f %f", x, y, i, v.x, v.y);
			
				// scale up to depth dim and store:
				// TODO: should there be a +0.5 for rounding?
				depth_map_data[i].x = v.x * DEPTH_WIDTH;
				depth_map_data[i].y = v.y * DEPTH_HEIGHT;
				
				// move to next column:
				ip += in_info.dimstride[0];
			}
		}
		
	unlock:
		// restore matrix lock state:
		jit_object_method(in_mat, _jit_sym_lock, in_savelock);
	out:
		if (err) {
			jit_error_code(&ob, err);
		}
	}
	
	void output_matrices() {
		outlet_anything(outlet_rgb  , _jit_sym_jit_matrix, 1, rgb_name  );
		outlet_anything(outlet_depth, _jit_sym_jit_matrix, 1, depth_name);
		outlet_anything(outlet_cloud, _jit_sym_jit_matrix, 1, cloud_name);
	}
	
	void bang() {
		if (unique) {
			if (new_rgb_data) {
				new_rgb_data = 0;
				outlet_anything(outlet_rgb  , _jit_sym_jit_matrix, 1, rgb_name  );
			}
			if (new_depth_data) {
				new_depth_data = 0;
				outlet_anything(outlet_depth, _jit_sym_jit_matrix, 1, depth_name);
			}
			if (new_cloud_data) {
				new_cloud_data = 0;
				outlet_anything(outlet_cloud, _jit_sym_jit_matrix, 1, cloud_name);
			}
		} else {
			outlet_anything(outlet_rgb  , _jit_sym_jit_matrix, 1, rgb_name  );
			outlet_anything(outlet_depth, _jit_sym_jit_matrix, 1, depth_name);
			outlet_anything(outlet_cloud, _jit_sym_jit_matrix, 1, cloud_name);
		}

	}
	
	void depth_process() {
		float inv_depth_focal_x = 1./depth_focal.x;
		float inv_depth_focal_y = 1./depth_focal.y;
	
		// for each cell:
		for (int i=0, y=0; y<DEPTH_HEIGHT; y++) {
			for (int x=0; x<DEPTH_WIDTH; x++, i++) {
				
				// cache raw, unrectified depth in output:
				// (casts uint16_t to uint32_t)
				depth_back[i] = depth_data[i];
				
				// remove the effects of lens distortion
				// (lookup into distortion map)
				vec2i di = depth_map_data[i];
				uint16_t d = depth_data[di.x + di.y*DEPTH_WIDTH];
				
				// Of course this isn't optimal; actually we should be doing the reverse, 
				// i.e. converting cell index to match the distortion. 
				// But it's not trivial to invert the lens distortion.
				
				//if (d < 2047) {
					// convert pixel coordinate to NDC depth plane intersection
					float uv_x = (x - depth_center.x) * inv_depth_focal_x;
					float uv_y = (y - depth_center.y) * inv_depth_focal_y;
					
					// convert Kinect depth to Z
					// NOTE: this should be cached into a lookup table
					// TODO: what are these magic numbers? the result is meters
					float z = 540 * 8 * depth_base / (depth_offset - d);
					
					// and scale according to depth (projection)
					uv_x = uv_x * z;
					uv_y = uv_y * z;
					
					// flip for GL:
					cloud_back[i].x = uv_x;
					cloud_back[i].y = -uv_y;
					cloud_back[i].z = -z;
					
				//} else {
//				
//					cloud_back[i].x = 0;
//					cloud_back[i].y = 0;
//					cloud_back[i].z = 0;
//				}
			}
		}
	}
	
	static void rgb_callback(freenect_device *dev, void *pixels, uint32_t timestamp){
		t_freenect3d *x = (t_freenect3d *)freenect_get_user(dev);
		if(!x)return;
		
		x->new_rgb_data = 1;
	}
	
	static void depth_callback(freenect_device *dev, void *pixels, uint32_t timestamp){
		t_freenect3d *x = (t_freenect3d *)freenect_get_user(dev);
		if(!x)return;
		
		// consider moving this to another thread to avoid dropouts?
		x->depth_process();
		
		x->new_depth_data = 1;
		x->new_cloud_data = 1;
	}
	
	static void log_cb(freenect_context *dev, freenect_loglevel level, const char *msg) {
		post(msg);
	}
	
	static void *capture_threadfunc(void *arg) {
		t_freenect3d *x = (t_freenect3d *)arg;
		
		// create the freenect context:
		freenect_context *context;		
		if(!f_ctx){
			if (freenect_init(&context, NULL) < 0) {
				error("freenect_init() failed");
				goto out;
			}
		}
		f_ctx = context;

		int devs = freenect_num_devices(f_ctx);
		post("%d devices detected", devs);
				
		freenect_set_log_callback(f_ctx, log_cb);
//		FREENECT_LOG_FATAL = 0,     /**< Log for crashing/non-recoverable errors */
//		FREENECT_LOG_ERROR,         /**< Log for major errors */
//		FREENECT_LOG_WARNING,       /**< Log for warning messages */
//		FREENECT_LOG_NOTICE,        /**< Log for important messages */
//		FREENECT_LOG_INFO,          /**< Log for normal messages */
//		FREENECT_LOG_DEBUG,         /**< Log for useful development messages */
//		FREENECT_LOG_SPEW,          /**< Log for slightly less useful messages */
//		FREENECT_LOG_FLOOD,         /**< Log EVERYTHING. May slow performance. */
		freenect_set_log_level(f_ctx, FREENECT_LOG_WARNING);
		
		
		capturing = 1;
		post("freenect starting processing");
		while (capturing > 0) {
			int err = freenect_process_events(f_ctx);
			//int err = freenect_process_events_timeout(f_ctx);
			if(err < 0){
				error("Freenect could not process events.");
				break;
			}
			systhread_sleep(0);
		}
		post("freenect finished processing");
		
	out: 
		freenect_shutdown(f_ctx);
		f_ctx = NULL;
		systhread_exit(NULL);
		return NULL;
	}
};

void freenect3d_bang(t_freenect3d * x) {
	x->bang();
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

void freenect3d_getdevlist(t_freenect3d *x) {
	x->getdevlist();
}

void freenect3d_led(t_freenect3d *x, int option) {
	x->led(option);
}

void freenect3d_accel(t_freenect3d *x) {
	x->accel();
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
			sprintf(s, "3D point cloud (matrix)"); 
		} else if (a == 1) {
			sprintf(s, "depth data (matrix)"); 
		} else if (a == 2) {
			sprintf(s, "RGB data (matrix)"); 
		} else {
			sprintf(s, "messages out"); 
		}
	}
}

void freenect3d_depth_map(t_freenect3d *x, t_symbol *s, long argc, t_atom *argv) {
	if (argc > 0) {
		// grab the last argument for the matrix name:
		x->depth_map(atom_getsym(&argv[argc-1]));
	}
}

void freenect3d_dictionary(t_freenect3d *x, t_symbol *s, long argc, t_atom *argv) {
	x->dictionary(s, argc, argv);
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
    if (x = (t_freenect3d *)object_alloc(freenect3d_class)) {
	
		// initialize in-place:
		x = new ((void *)x) t_freenect3d();
		
		// add a general purpose outlet (rightmost)
		x->outlet_msg = outlet_new(x, 0);
		x->outlet_rgb = outlet_new(x, "jit_matrix");
		x->outlet_depth = outlet_new(x, "jit_matrix");
		x->outlet_cloud = outlet_new(x, "jit_matrix");
		
		// default attrs:
		//freenect3d_getdevlist(x);
		
		// apply attrs:
		attr_args_process(x, argc, argv);
	}
	return (x);
}

void freenect3d_free(t_freenect3d *x) {
	x->~t_freenect3d();
}

int C74_EXPORT main(void) {	
	t_class *maxclass;
	
	common_symbols_init();
	
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
	
	maxclass = class_new("freenect3d", (method)freenect3d_new, (method)freenect3d_free, (long)sizeof(t_freenect3d), 0L, A_GIMME, 0);
				  
	class_addmethod(maxclass, (method)freenect3d_assist, "assist", A_CANT, 0);
	class_addmethod(maxclass, (method)freenect3d_notify, "notify", A_CANT, 0);
	
	class_addmethod(maxclass, (method)freenect3d_jit_matrix, "jit_matrix", A_GIMME, 0); 
	class_addmethod(maxclass, (method)freenect3d_bang, "bang", 0);
	class_addmethod(maxclass, (method)freenect3d_getdevlist, "getdevlist", 0);
	class_addmethod(maxclass, (method)freenect3d_led, "led", A_LONG, 0);
	class_addmethod(maxclass, (method)freenect3d_accel, "accel", 0);
	class_addmethod(maxclass, (method)freenect3d_open, "open", A_GIMME, 0);
	class_addmethod(maxclass, (method)freenect3d_close, "close", 0);
	
	class_addmethod(maxclass, (method)freenect3d_depth_map, "depth_map", A_GIMME, 0);
	class_addmethod(maxclass, (method)freenect3d_dictionary, "dictionary", A_SYM, 0);
	
	CLASS_ATTR_LONG(maxclass, "unique", 0, t_freenect3d, unique);
	CLASS_ATTR_STYLE_LABEL(maxclass, "unique", 0, "onoff", "output frame only when new data is received");
	
	CLASS_ATTR_FLOAT_ARRAY(maxclass, "depth_focal", 0, t_freenect3d, depth_focal, 2);
	CLASS_ATTR_FLOAT_ARRAY(maxclass, "depth_center", 0, t_freenect3d, depth_center, 2);
	CLASS_ATTR_FLOAT(maxclass, "depth_base", 0, t_freenect3d, depth_base);
	CLASS_ATTR_FLOAT(maxclass, "depth_offset", 0, t_freenect3d, depth_offset);
	
	
	
	class_register(CLASS_BOX, maxclass); /* CLASS_NOBOX */
	freenect3d_class = maxclass;
	return 0;
}
