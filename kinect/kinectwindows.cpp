#include "MaxK4W.h"

t_class *kinect_class;

void kinect_depth_map(t_kinect *x, t_symbol *s, long argc, t_atom *argv) {
	if (argc > 0) {
		// grab the last argument for the matrix name:
		x->depth_map(atom_getsym(&argv[argc-1]));
	}
}

void kinect_getdevlist(t_kinect *x) {
	x->getdevlist();
}

void kinect_led(t_kinect *x, int option) {
	x->led(option);
}

void kinect_accel(t_kinect *x) {
	x->accel();
}

void kinect_dictionary(t_kinect *x, t_symbol *s, long argc, t_atom *argv) { 
	x->dictionary(s, argc, argv); 
}

void kinect_open(t_kinect *x, t_symbol *s, long argc, t_atom * argv) {
	x->open(s, argc, argv);
}

void kinect_close(t_kinect *x) {
	x->close();
}

void kinect_bang(t_kinect *x) {
	x->bang();
}

// object_notify
t_max_err kinect_notify(t_kinect *x, t_symbol *s, t_symbol *msg, void *sender, void *data) {
	t_symbol *attrname;
 
	if (msg == gensym("attr_modified")) {       // check notification type
		attrname = (t_symbol *)object_method((t_object *)data, gensym("getname"));
		object_post((t_object *)x, "changed attr name is %s",attrname->s_name);
	} else {
		object_post((t_object *)x, "notify %s (self %d)", msg->s_name, sender == x);
	}

	return 0;
}

void kinect_assist(t_kinect *x, void *b, long m, long a, char *s)
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

void kinect_free(t_kinect *x)
{
	x->~t_kinect();
}

void *kinect_new(t_symbol *s, long argc, t_atom *argv)
{
	t_kinect *x = NULL;
    if (x = (t_kinect *)object_alloc(kinect_class)) {
	
		// initialize in-place:
		x = new ((void *)x) t_kinect();
		
		// add a general purpose outlet (rightmost)
		x->outlet_msg = outlet_new(x, 0);
		x->outlet_rgb = outlet_new(x, "jit_matrix");
		x->outlet_depth = outlet_new(x, "jit_matrix");
		x->outlet_cloud = outlet_new(x, "jit_matrix");
		
		// apply attrs:
		attr_args_process(x, (short)argc, argv);
		
		// default attrs:
		kinect_getdevlist(x);
	}
	return (x);
}

int C74_EXPORT main(void)
{	
	common_symbols_init();
	
	t_class * maxclass = class_new("kinectwindows", (method)kinect_new, (method)kinect_free, (long)sizeof(t_kinect), 0L, A_GIMME, 0);
	
	class_addmethod(maxclass, (method)kinect_assist, "assist", A_CANT, 0);  
	class_addmethod(maxclass, (method)kinect_open, "open", A_GIMME, 0);  
	class_addmethod(maxclass, (method)kinect_close, "close", 0); 
	class_addmethod(maxclass, (method)kinect_bang, "bang", 0); 

	class_addmethod(maxclass, (method)kinect_notify, "notify", A_CANT, 0);
	class_addmethod(maxclass, (method)kinect_getdevlist, "getdevlist", 0);
	class_addmethod(maxclass, (method)kinect_led, "led", A_LONG, 0);
	class_addmethod(maxclass, (method)kinect_accel, "accel", 0);
	
	class_addmethod(maxclass, (method)kinect_depth_map, "depth_map", A_GIMME, 0);
	class_addmethod(maxclass, (method)kinect_dictionary, "dictionary", A_SYM, 0);
	
	CLASS_ATTR_LONG(maxclass, "unique", 0, t_kinect, unique);
	CLASS_ATTR_STYLE_LABEL(maxclass, "unique", 0, "onoff", "output frame only when new data is received");
	
	CLASS_ATTR_FLOAT_ARRAY(maxclass, "depth_focal", 0, t_kinect, depth_focal, 2);
	CLASS_ATTR_FLOAT_ARRAY(maxclass, "depth_center", 0, t_kinect, depth_center, 2);
	CLASS_ATTR_FLOAT(maxclass, "depth_base", 0, t_kinect, depth_base);
	CLASS_ATTR_FLOAT(maxclass, "depth_offset", 0, t_kinect, depth_offset);

	CLASS_ATTR_LONG(maxclass, "device_count", 0, t_kinect, device_count);
	CLASS_ATTR_LONG(maxclass, "near_mode", 0, t_kinect, near_mode);
	CLASS_ATTR_STYLE_LABEL(maxclass, "near_mode", 0, "onoff", "Windows only!");
	CLASS_ATTR_LONG(maxclass, "player", 0, t_kinect, player);
	CLASS_ATTR_STYLE_LABEL(maxclass, "player", 0, "onoff", "Windows only! Enable detection of player IDs");

	class_register(CLASS_BOX, maxclass);
	kinect_class = maxclass;
	return 0;
}
