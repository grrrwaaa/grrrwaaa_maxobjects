/**
	@file
	kinect - a max object
	
	
	Q: is it better to have a separate freenect_context per device?
	
*/

extern "C" {
	#include "ext.h"		
	#include "ext_obex.h"
	#include "ext_dictionary.h"
	#include "ext_dictobj.h"
	#include "ext_systhread.h"

	#include "jit.common.h"
	#include "jit.gl.h"
}

#include <new>

#define DEPTH_WIDTH 640
#define DEPTH_HEIGHT 480

t_systhread capture_thread;	
int capturing = 0;

class MaxKinectBase {
public:

	struct vec2i { int x, y; };
	struct vec2f { float x, y; };
	struct vec3f { float x, y, z; };

	t_object	ob;			// the object itself (must be first)

	void *		outlet_cloud;
	void *		outlet_rgb;
	void *		outlet_depth;
	void *		outlet_msg;
	
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
	
	// cloud matrix for output:
	void *		cloud_mat;
	void *		cloud_mat_wrapper;
	t_atom		cloud_name[1];
	vec3f *		cloud_back;
	
	// attributes:
	vec2f		depth_focal;
	vec2f		depth_center;
	float		depth_base, depth_offset;
	int			unique;
	
	vec2i *		depth_map_data;
	
	MaxKinectBase() {
		unique = 1;
		
		// can we accept a dict?
		depth_base = 0.085;
		depth_offset = 0.0011;
		depth_center.x = 314;
		depth_center.y = 241;
		depth_focal.x = 597;
		depth_focal.y = 597;
		
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
		
		// init depth_map with default data:
		depth_map_data = (vec2i *)sysmem_newptr(DEPTH_WIDTH*DEPTH_HEIGHT * sizeof(vec2i));
		for (int i=0, y=0; y<DEPTH_HEIGHT; y++) {
			for (int x=0; x<DEPTH_WIDTH; x++, i++) {
				depth_map_data[i].x = x;
				depth_map_data[i].y = y;
			}
		}
	}
	
	~MaxKinectBase() {
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
		sysmem_freeptr(depth_map_data);
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
};