/**
	@file
	oculus - a max object
*/

extern "C" {
	#include "ext.h"		
	#include "ext_obex.h"

	#include "z_dsp.h"

	#include "jit.common.h"
	#include "jit.gl.h"
}

#include "OVR.h"

#ifdef __APPLE__
	#include "CoreFoundation/CoreFoundation.h"
    #define MY_PLUGIN_BUNDLE_IDENTIFIER "com.cycling74.oculus"
#endif

t_class *oculus_class;
static t_symbol * ps_quat;

char bundle_path[MAX_PATH_CHARS];

class t_oculus {
public:
	t_object	ob;			// the object itself (must be first)
	void *		ob3d;		// OpenGL scene state
	
	void *		outlet_q;
	
	// the quaternion orientation of the HMD:
	t_atom		quat[4];
	
	OVR::System sys;
	OVR::Ptr<OVR::DeviceManager> pManager;
	OVR::Ptr<OVR::HMDDevice>    pHMD;
	OVR::Ptr<OVR::SensorDevice> pSensor;
	OVR::SensorFusion			SFusion;
	OVR::HMDInfo				hmd;
	
	t_oculus() {
		
		if (!OVR::System::IsInitialized()) {
			OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));
		}
		
		pManager = *OVR::DeviceManager::Create();
		pHMD     = *pManager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();
		
		if (pHMD == 0) {
			error("no HMD found");
		} else {
			pHMD->GetDeviceInfo(&hmd);
			
			// get a reference to the sensor:
			pSensor = *pHMD->GetSensor();
			SFusion.AttachToSensor(pSensor);
			
			
			post("prediction delta: %f", SFusion.GetPredictionDelta());
			
			if (SFusion.HasMagCalibration()) {
				post("using magnetometer for yaw correction");
				SFusion.SetYawCorrectionEnabled(true);
			}
		}
	}

};

t_jit_err oculus_draw(t_oculus *x) {
	t_jit_err result = JIT_ERR_NONE;
	return result;
}

t_jit_err oculus_dest_closing(t_oculus *x) {
	return JIT_ERR_NONE;
}
t_jit_err oculus_dest_changed(t_oculus *x) {
	return JIT_ERR_NONE;
}

// object_notify
t_max_err oculus_notify(t_oculus *x, t_symbol *s, t_symbol *msg, void *sender, void *data) {
	t_symbol *attrname;
 
	if (msg == gensym("attr_modified")) {       // check notification type
		attrname = (t_symbol *)object_method((t_object *)data, gensym("getname"));
		object_post((t_object *)x, "changed attr name is %s",attrname->s_name);
	} else {
		object_post((t_object *)x, "notify %s (self %d)", msg->s_name, sender == x);
	}

	return 0;
}

// SFusion.SetPrediction(0.03, true);

void oculus_bang(t_oculus * x) {
	
	//x->SFusion.GetOrientation();
	OVR::Quatf orientation = x->SFusion.GetPredictedOrientation();
	atom_setfloat(x->quat  , orientation.x);
	atom_setfloat(x->quat+1, orientation.y);
	atom_setfloat(x->quat+2, orientation.z);
	atom_setfloat(x->quat+3, orientation.w);
	
	outlet_anything(x->outlet_q, ps_quat, 4, x->quat); 
}

void oculus_anything(t_oculus *x, t_symbol *s, long argc, t_atom *argv)
{
	//object_post( (t_object*)x, "This method was invoked by sending the '%s' message to this object.", s->s_name);
	// argc and argv are the arguments, as described in above.
}

void oculus_jit_matrix(t_oculus *x, t_symbol *s, long argc, t_atom *argv)
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

void oculus_assist(t_oculus *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) { // inlet
		sprintf(s, "I am inlet %ld", a);
	} 
	else {	// outlet
		sprintf(s, "I am outlet %ld", a); 			
	}
}


void oculus_free(t_oculus *x) {
	// free resources associated with our obex entry
	jit_ob3d_free(x);
	max_jit_object_free(x);
}

void *oculus_new(t_symbol *s, long argc, t_atom *argv)
{
	t_oculus *x = NULL;
    //long i;
	long attrstart;
	t_symbol *dest_name_sym = _jit_sym_nothing;
	
	if (x = (t_oculus *)object_alloc(oculus_class)) {
	
		// initialize in-place:
		x = new (x) t_oculus();
		
		// get first normal arg, the destination name
		attrstart = max_jit_attr_args_offset(argc,argv);
		if (attrstart && argv) {
			jit_atom_arg_getsym(&dest_name_sym, 0, attrstart, argv);
		}
		jit_ob3d_new(x, dest_name_sym);
		
		// add a general purpose outlet (rightmost)
		max_jit_obex_dumpout_set(x, outlet_new(x,NULL));
	
		x->outlet_q = outlet_new(x, 0);
		
		// default attrs:
		
		// apply attrs:
		attr_args_process(x, argc, argv);
	}
	return (x);
}

int C74_EXPORT main(void) {	
	t_class *maxclass;
	void *ob3d;
	
	ps_quat = gensym("quat");
	
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
	
	maxclass = class_new("oculus", (method)oculus_new, (method)oculus_free, (long)sizeof(t_oculus), 
				  0L, A_GIMME, 0);
				  
	class_addmethod(maxclass, (method)oculus_assist, "assist", A_CANT, 0);
	class_addmethod(maxclass, (method)oculus_notify, "notify", A_CANT, 0);
	
	class_addmethod(maxclass, (method)oculus_jit_matrix, "jit_matrix", A_GIMME, 0); 
	class_addmethod(maxclass, (method)oculus_bang, "bang", 0);
	class_addmethod(maxclass, (method)oculus_anything, "anything", A_GIMME, 0);

	// set up object extension for 3d object, customized with flags
	ob3d = jit_ob3d_setup(maxclass, 
				calcoffset(t_oculus, ob3d), 
				JIT_OB3D_NO_MATRIXOUTPUT);
				
	// define our OB3D draw method.  called in automatic mode by 
	// jit.gl.render or otherwise through ob3d when banged. this 
	// method is A_CANT because our draw setup needs to happen 
	// in the ob3d beforehand to initialize OpenGL state 
	jit_class_addmethod(maxclass, 
		(method)oculus_draw, "ob3d_draw", A_CANT, 0L);
	
	// define our dest_closing and dest_changed methods. 
	// these methods are called by jit.gl.render when the 
	// destination context closes or changes: for example, when 
	// the user moves the window from one monitor to another. Any 
	// resources your object keeps in the OpenGL machine 
	// (e.g. textures, display lists, vertex shaders, etc.) 
	// will need to be freed when closing, and rebuilt when it has 
	// changed. 
	jit_class_addmethod(maxclass, (method)oculus_dest_closing, "dest_closing", A_CANT, 0L);
	jit_class_addmethod(maxclass, (method)oculus_dest_changed, "dest_changed", A_CANT, 0L);
	
	// must register for ob3d use
	jit_class_addmethod(maxclass, (method)jit_object_register, "register", A_CANT, 0L);
	
	class_register(CLASS_BOX, maxclass); /* CLASS_NOBOX */
	oculus_class = maxclass;
	return 0;
}
