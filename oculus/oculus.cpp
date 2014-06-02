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

#include <new>

#include "OVR_CAPI.h"

#ifdef __APPLE__
	#include "CoreFoundation/CoreFoundation.h"
    #define MY_PLUGIN_BUNDLE_IDENTIFIER "com.cycling74.oculus"
#endif

t_class *oculus_class;
static t_symbol * ps_quat;

char bundle_path[MAX_PATH_CHARS];

volatile int libcount = 0;

class t_oculus {
public:
	t_object	ob;			// the object itself (must be first)
	void *		ob3d;		// OpenGL scene state
	
	void *		outlet_q;
	void *		outlet_c;
	void *		outlet_eye[2];
	void *		outlet_mesh[2];
	
	// the quaternion orientation of the HMD:
	t_atom		quat[4];
	
	ovrHmd		hmd;
	ovrHmdDesc	hmdDesc;
	unsigned int distortionCaps;
	
	ovrFovPort	fovPort[2];
	ovrEyeRenderDesc eyeDesc[2];
	ovrSizei	texDim[2];
	ovrDistortionMesh eyeMesh[2];
	
	t_oculus(int index = 0) {
		distortionCaps = 0;
		eyeMesh[0].pVertexData = 0;
		eyeMesh[1].pVertexData = 0;
		
		libcount++;
		if (libcount == 1 && !ovr_Initialize()) {
			object_error(&ob, "LibOVR: failed to initialize library");
			return;
		}
		
		int hmd_count = ovrHmd_Detect();
		object_post(&ob, "%d HMDs detected", hmd_count);
		if (hmd_count > 0 && index < hmd_count) {
			hmd = ovrHmd_Create(index);
		}
		hmd = ovrHmd_Create(index);
		if (hmd == 0) {
			object_warn(&ob, "LibOVR: HMD not detected, using offline DK1 simulator instead");
			hmd = ovrHmd_CreateDebug(ovrHmd_DK1);
		}
		ovrHmd_GetDesc(hmd, &hmdDesc);
		
		// start tracking:
		// supported capabilities -- use hmdDesc by default
		// required capabilities -- use none by default
		if (!ovrHmd_StartSensor(hmd, hmdDesc.SensorCaps, 0)) {
			object_error(&ob, "failed to start LibOVR sensor");
		}
	}

	~t_oculus() {
	
		if (hmd) {
			ovrHmd_StopSensor(hmd);
			ovrHmd_Destroy(hmd);
		}
				
		if (eyeMesh[0].pVertexData != 0) ovrHmd_DestroyDistortionMesh(&eyeMesh[0]);
		if (eyeMesh[1].pVertexData != 0) ovrHmd_DestroyDistortionMesh(&eyeMesh[1]);
		
		// release library nicely
		libcount--;
		if (libcount == 0) {
			ovr_Shutdown();
		}
	}
	
	void configure() {
		t_atom a[4];
	
		if (!hmd) {
			object_warn(&ob, "No HMD to configure");
			return;
		}
		
		// distortion caps:
		//ovrDistortionCap_Chromatic	= 0x01,		//	Supports chromatic aberration correction.
		//ovrDistortionCap_TimeWarp	= 0x02,		//	Supports timewarp.
		//ovrDistortionCap_Vignette	= 0x08		//	Supports vignetting around the edges of the view.
		unsigned int distortionCaps = ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette;
	
		// configure each eye
		for ( int eyeNum = 0; eyeNum < 2; eyeNum++ ) {
			
			// derive eye configuration for given desired FOV:
			fovPort[eyeNum] = hmdDesc.DefaultEyeFov[eyeNum];	// or MaxEyeFov?
			eyeDesc[eyeNum] = ovrHmd_GetRenderDesc(hmd, (ovrEyeType)eyeNum, fovPort[eyeNum]);

			// Use ovrHmd_CreateDistortionMesh to generate distortion mesh.
			// Distortion capabilities will depend on 'distortionCaps' flags; user should rely on
			// appropriate shaders based on their settings.
			// re-use, reduce, recycle
			if (eyeMesh[eyeNum].pVertexData != 0) ovrHmd_DestroyDistortionMesh(&eyeMesh[eyeNum]);
			if (!ovrHmd_CreateDistortionMesh(hmd, (ovrEyeType)eyeNum, fovPort[eyeNum], distortionCaps, &eyeMesh[eyeNum])) {
				object_error(&ob, "LibOVR failed to create distortion mesh");
			}
			
			// export this distortion mesh to Jitter
			output_mesh(eyeNum);
			
			// get recommended texture dim
			// depends on the FOV and desired pixel quality
			// maybe want to scale this up to a POT texture dim?
			texDim[eyeNum] = ovrHmd_GetFovTextureSize(hmd, (ovrEyeType)eyeNum, fovPort[eyeNum], 1.0f);

			// Users should call ovrHmd_GetRenderScaleAndOffset to get uvScale and Offset values for rendering.			
			ovrVector2f uvScaleOffsetOut[2];
			ovrSizei textureSize;
			textureSize.w = 2048;
			textureSize.h = 2048;
			// since we are mapping each eye viewport to a texture, the dims are the same:
			ovrRecti renderViewport;
			renderViewport.Pos.x = 0;
			renderViewport.Pos.y = 0;
			renderViewport.Size.w = textureSize.w; //hmdDesc.Resolution.w/2;
			renderViewport.Size.h = textureSize.h; //hmdDesc.Resolution.h;
			ovrHmd_GetRenderScaleAndOffset(fovPort[eyeNum], textureSize, renderViewport, uvScaleOffsetOut);
			
			atom_setfloat(a+0, uvScaleOffsetOut[0].x);
			atom_setfloat(a+1, uvScaleOffsetOut[0].y);
			atom_setfloat(a+2, uvScaleOffsetOut[1].x);
			atom_setfloat(a+3, uvScaleOffsetOut[1].y);
			outlet_anything(outlet_eye[eyeNum], gensym("uvScaleAndOffset"), 4, a);
			
		}
		
		info();
	}	
	
	void output_mesh(int eyeNum) {
		t_jit_matrix_info info;
		t_atom a[1];
		
		// create index matrix:
		void * index_mat_wrapper = jit_object_new(gensym("jit_matrix_wrapper"), jit_symbol_unique(), 0, NULL);
		void * index_mat = jit_object_method(index_mat_wrapper, _jit_sym_getmatrix);
		
		// configure matrix:
		int indexCount = eyeMesh[eyeNum].IndexCount;
		jit_matrix_info_default(&info);
		info.flags |= JIT_MATRIX_DATA_PACK_TIGHT;
		info.planecount = 1;
		info.type = gensym("long");
		info.dimcount = 1;
		info.dim[0] = indexCount;
		jit_object_method(index_mat, _jit_sym_setinfo_ex, &info);
		
		// copy indices:
		int32_t * index_ptr;
		jit_object_method(index_mat, _jit_sym_getdata, &index_ptr);
		for (int i=0; i<indexCount; i++) {
			index_ptr[i] = eyeMesh[eyeNum].pIndexData[i];
		}
		
		// output index matrix for mesh:
		atom_setsym(a, jit_attr_getsym(index_mat_wrapper, _jit_sym_name));
		outlet_anything(outlet_mesh[eyeNum], gensym("index_matrix"), 1, a);
		
		// done with index matrix:
		object_release((t_object *)index_mat_wrapper);

		
		
		// create geometry matrix:
		void * mesh_mat_wrapper = jit_object_new(gensym("jit_matrix_wrapper"), jit_symbol_unique(), 0, NULL);
		void * mesh_mat = jit_object_method(mesh_mat_wrapper, _jit_sym_getmatrix);
		
		// configure matrix:
		int vertexCount = eyeMesh[eyeNum].VertexCount;		
		jit_matrix_info_default(&info);
		info.flags |= JIT_MATRIX_DATA_PACK_TIGHT;
		info.planecount = 12;
		info.type = gensym("float32");
		info.dimcount = 1;
		info.dim[0] = vertexCount;
		jit_object_method(mesh_mat, _jit_sym_setinfo_ex, &info);
		
		// copy & translate vertices to Jitter matrix format:
		float * mat_ptr;
		jit_object_method(mesh_mat, _jit_sym_getdata, &mat_ptr);
		for (int i=0; i<vertexCount; i++) {
			float * cell = mat_ptr + i*12;
			ovrDistortionVertex& vertex = eyeMesh[eyeNum].pVertexData[i];
			
			// planes 0,1,2: xyz
			cell[0] = vertex.Pos.x;
			cell[1] = vertex.Pos.y;
			cell[2] = 0.5; 	// unused
			
			// planes 3,4: texcoord0
			cell[3] = vertex.TexR.x;
			cell[4] = vertex.TexR.y;
			
			// planes 5,6,7: normal
			cell[5] = vertex.VignetteFactor; 
			cell[6] = vertex.TimeWarpFactor;
			cell[7] = 0.5;	// unused
			
			// planes 8,9,10,11: rgba
			// mapped to TexG and TexB (chromatic aberration)
			cell[8]  = vertex.TexG.x;
			cell[9]  = vertex.TexG.y;
			cell[10] = vertex.TexB.x;
			cell[11] = vertex.TexB.y;
		
		}
		
		// output geometry matrix:
		atom_setsym(a, jit_attr_getsym(mesh_mat_wrapper, _jit_sym_name));
		outlet_anything(outlet_mesh[eyeNum], _jit_sym_jit_matrix, 1, a);
		
		// done with geometry matrix:
		object_release((t_object *)mesh_mat_wrapper);
	}
	
	void info() {
		t_atom a[4];
		
		if (hmd) {
			#define HMD_CASE(T) case T: { \
				atom_setsym(a, gensym( #T )); \
				outlet_anything(outlet_c, gensym("hmdType"), 1, a); \
				break; \
			}
			switch(hmdDesc.Type) {
				HMD_CASE(ovrHmd_DK1)
				HMD_CASE(ovrHmd_DKHD)
				HMD_CASE(ovrHmd_CrystalCoveProto)
				HMD_CASE(ovrHmd_DK2)
				default: {
					atom_setsym(a, gensym("unknown")); 
					outlet_anything(outlet_c, gensym("hmdType"), 1, a);
				}
			}
			#undef HMD_CASE
			
			atom_setsym(a, gensym(hmdDesc.Manufacturer)); 
			outlet_anything(outlet_c, gensym("Manufacturer"), 1, a);
			atom_setsym(a, gensym(hmdDesc.ProductName)); 
			outlet_anything(outlet_c, gensym("ProductName"), 1, a);
			atom_setsym(a, gensym(hmdDesc.DisplayDeviceName)); 
			outlet_anything(outlet_c, gensym("DisplayDeviceName"), 1, a);
			atom_setlong(a, hmdDesc.Resolution.w); 
			atom_setlong(a+1, hmdDesc.Resolution.h); 
			outlet_anything(outlet_c, gensym("Resolution"), 2, a);

			
			// capabilities:
			//hmdDesc.HmdCaps
			//hmdDesc.SensorCaps
			//hmdDesc.DistortionCaps
			// hmdDesc.EyeRenderOrder
			
			for ( int eyeNum = 0; eyeNum < 2; eyeNum++ ) {
				atom_setlong(a  , texDim[eyeNum].w);
				atom_setlong(a+1, texDim[eyeNum].h);
				outlet_anything(outlet_eye[eyeNum], gensym("texdim"), 2, a);
						
				atom_setlong(a+0, eyeDesc[eyeNum].DistortedViewport.Pos.x);
				atom_setlong(a+1, eyeDesc[eyeNum].DistortedViewport.Pos.y);
				atom_setlong(a+2, eyeDesc[eyeNum].DistortedViewport.Size.w);
				atom_setlong(a+3, eyeDesc[eyeNum].DistortedViewport.Size.h);
				outlet_anything(outlet_eye[eyeNum], gensym("DistortedViewport"), 4, a);
				
				atom_setfloat(a+0, eyeDesc[eyeNum].ViewAdjust.x);
				atom_setfloat(a+1, eyeDesc[eyeNum].ViewAdjust.y);
				atom_setfloat(a+2, eyeDesc[eyeNum].ViewAdjust.z);
				outlet_anything(outlet_eye[eyeNum], gensym("ViewAdjust"), 3, a);
				
				/*
				// Field Of View (FOV) in tangent of the angle units.
				// As an example, for a standard 90 degree vertical FOV, we would 
				// have: { UpTan = tan(90 degrees / 2), DownTan = tan(90 degrees / 2) }.
				typedef struct ovrFovPort_
				{
					float UpTan;
					float DownTan;
					float LeftTan;
					float RightTan;
				} ovrFovPort;
				*/
				
				atom_setfloat(a+0, -eyeDesc[eyeNum].Fov.LeftTan);
				atom_setfloat(a+1,  eyeDesc[eyeNum].Fov.RightTan);
				atom_setfloat(a+2, -eyeDesc[eyeNum].Fov.DownTan);
				atom_setfloat(a+3,  eyeDesc[eyeNum].Fov.UpTan);
				outlet_anything(outlet_eye[eyeNum], gensym("FOV"), 4, a);
			}
		}
	}
	
	
	
	void bang() {
		if (hmd) {
		
			//  3. Use ovrHmd_BeginFrameTiming, ovrHmd_GetEyePose and ovrHmd_BeginFrameTiming
			//     in the rendering loop to obtain timing and predicted view orientation for
			//     each eye.
			//      - If relying on timewarp, use ovr_WaitTillTime after rendering+flush, followed
			//        by ovrHmd_GetEyeTimewarpMatrices to obtain timewarp matrices used 
			//        in distortion pixel shader to reduce latency.
			
			/*
			To facilitate prediction, ovrHmd_GetSensorState takes absolute time, in seconds, as a second argument. This is the same value as reported by the ovr_GetTimeInSeconds global function. 
			In a production application, however, you should use one of the real-time computed values returned by ovrHmd_BeginFrame or ovrHmd_BeginFrameTiming. Prediction is covered in more detail in the section on Frame Timing.*/
			
			
			
			ovrSensorState ss = ovrHmd_GetSensorState(hmd, ovr_GetTimeInSeconds());
			
			ovrPoseStatef predicted = ss.Predicted;
			ovrQuatf orient = predicted.Pose.Orientation;
			
			atom_setfloat(quat  , orient.x);
			atom_setfloat(quat+1, orient.y);
			atom_setfloat(quat+2, orient.z);
			atom_setfloat(quat+3, orient.w);
			outlet_list(outlet_q, 0L, 4, quat); 
			
			/*
			for ( int eyeNum = 0; eyeNum < 2; eyeNum++ ) {
				ovrEyeType eye = hmdDesc.EyeRenderOrder[eyeNum];
				ovrPosef eyePose = ovrHmd_BeginEyeRender(hmd, eye);
				
				// projection matrix only depends on this:
				// fov := eyeDesc[eye].Fov
				
				// quat := eyePose.Orientation
				// is this different to orient?
				
				// worldview, handed by jit.anim.node instead.
				//Matrix4f view = Matrix4f(quat.Inverted()) * Matrix4f::Translation(-EyePos);
			}
			*/
			/*
			ovrFrameTiming timing = ovrHmd_BeginFrameTiming(hmd, 0);
			
			ovrPosef leftPose = ovrHmd_GetEyePose(hmd, ovrEye_Left);
			ovrPosef rightPose = ovrHmd_GetEyePose(hmd, ovrEye_Right);
			*/
			
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
	x->bang();
	//x->SFusion.GetOrientation();
	/*
	OVR::Quatf orientation = x->SFusion.GetPredictedOrientation();
	atom_setfloat(x->quat  , orientation.x);
	atom_setfloat(x->quat+1, orientation.y);
	atom_setfloat(x->quat+2, orientation.z);
	atom_setfloat(x->quat+3, orientation.w);
	outlet_list(x->outlet_q, 0L, 4, x->quat); 
	*/
}

void oculus_info(t_oculus *x) {
	x->info();
}

void oculus_configure(t_oculus *x) {
	x->configure();
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


void oculus_free(t_oculus *x) {
	x->~t_oculus();

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
	
		// TODO: does oculus need to be an ob3d?
		// get first normal arg, the destination name
		attrstart = max_jit_attr_args_offset(argc,argv);
		if (attrstart && argv) {
			jit_atom_arg_getsym(&dest_name_sym, 0, attrstart, argv);
		}
		jit_ob3d_new(x, dest_name_sym);
		
		// add a general purpose outlet (rightmost)
		max_jit_obex_dumpout_set(x, outlet_new(x,NULL));
		x->outlet_c = outlet_new(x, 0);
		x->outlet_mesh[1] = outlet_new(x, "jit_matrix");
		x->outlet_eye[1] = outlet_new(x, 0);
		x->outlet_mesh[0] = outlet_new(x, "jit_matrix");
		x->outlet_eye[0] = outlet_new(x, 0);
		x->outlet_q = listout(x);
		
		// default attrs:
		// initialize in-place:
		x = new (x) t_oculus();
		
		// apply attrs:
		attr_args_process(x, argc, argv);
		
		// now configure:
		x->configure();
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
	class_addmethod(maxclass, (method)oculus_info, "info", 0);
	class_addmethod(maxclass, (method)oculus_configure, "configure", 0);
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
