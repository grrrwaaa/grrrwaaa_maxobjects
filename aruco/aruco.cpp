
#ifdef __cplusplus 
extern "C" {
#endif
	#include "ext.h"		
	#include "ext_obex.h"

	#include "jit.common.h"
	#include "jit.gl.h"
#ifdef __cplusplus 
}
#endif

#include <new>

t_class * aruco_class;

volatile int libcount = 0;

class t_aruco {
public:
	t_object	ob;			// the object itself (must be first)
	
	// many outlets:
	void *		outlet_q;
	void *		outlet_eye[2];
	void *		outlet_mesh[2];
	void *		outlet_c;
	
	t_aruco(int index = 0) {
		
		libcount++;
		if (libcount == 1 /*&& !ovr_Initialize()*/) {
			object_error(&ob, "LibOVR: failed to initialize library");
			return;
		}
		
	}

	~t_aruco() {
	
		// release library nicely
		libcount--;
		if (libcount == 0) {

		}
	}
	
	void bang() {
		
	}
};

void aruco_assist(t_aruco *x, void *b, long m, long a, char *s)
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
			sprintf(s, "HMD left eye properties (messages)"); 
		} else if (a == 2) {
			sprintf(s, "HMD left eye mesh (jit_matrix)"); 
		} else if (a == 3) {
			sprintf(s, "HMD right eye properties (messages)"); 
		} else if (a == 4) {
			sprintf(s, "HMD right eye mesh (jit_matrix)");  
		} else if (a == 5) {
			sprintf(s, "HMD properties (messages)");
		} else {
			sprintf(s, "I am outlet %ld", a); 
		}
	}
}

void aruco_free(t_aruco *x) {
	x->~t_aruco();

	// free resources associated with our obex entry
	//jit_ob3d_free(x);
	max_jit_object_free(x);
}

void *aruco_new(t_symbol *s, long argc, t_atom *argv)
{
	t_aruco *x = NULL;
	if (x = (t_aruco *)object_alloc(aruco_class)) {
		
		x->outlet_c = outlet_new(x, 0);
		x->outlet_mesh[1] = outlet_new(x, "jit_matrix");
		x->outlet_eye[1] = outlet_new(x, 0);
		x->outlet_mesh[0] = outlet_new(x, "jit_matrix");
		x->outlet_eye[0] = outlet_new(x, 0);
		x->outlet_q = listout(x);
		
		// initialize in-place:
		x = new (x) t_aruco();
		
		// apply attrs:
		attr_args_process(x, argc, argv);
	}
	return (x);
}


t_max_err aruco_notify(t_aruco *x, t_symbol *s, t_symbol *msg, void *sender, void *data) {
	t_symbol *attrname;
	if (msg == _sym_attr_modified) {       // check notification type
		attrname = (t_symbol *)object_method((t_object *)data, _sym_getname);
		object_post((t_object *)x, "changed attr name is %s", attrname->s_name);
	} else { 
		object_post((t_object *)x, "notify %s (self %d)", msg->s_name, sender == x);
	}
	return 0;
}

void aruco_bang(t_aruco * x) {
	x->bang();
}

int C74_EXPORT main(void) {	
	t_class *maxclass;
	common_symbols_init();
	
	maxclass = class_new("aruco", (method)aruco_new, (method)aruco_free, (long)sizeof(t_aruco), 
				  0L, A_GIMME, 0);
				  
	class_addmethod(maxclass, (method)aruco_assist, "assist", A_CANT, 0);
	class_addmethod(maxclass, (method)aruco_notify, "notify", A_CANT, 0);
	
	//class_addmethod(maxclass, (method)aruco_jit_matrix, "jit_matrix", A_GIMME, 0); 
	class_addmethod(maxclass, (method)aruco_bang, "bang", 0);
	
	class_register(CLASS_BOX, maxclass); 
	aruco_class = maxclass;
	return 0;
}