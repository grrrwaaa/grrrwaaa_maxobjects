#include "aruco.h"
#include "cvdrawingutils.h"
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#ifdef __cplusplus 
extern "C" {
#endif
	#include "ext.h"		
	#include "ext_obex.h"

	#include "jit.common.h"
#ifdef __cplusplus 
}
#endif

#include <new>
#include <vector>


t_class * aruco_class;

volatile int libcount = 0;

class t_aruco {
public:
	t_object	ob;			// the object itself (must be first)
	
	// many outlets:
	void *		outlet_msg;
	void *		outlet_img;
	void *		outlet_c;
	
	// attrs:
	float		markersize;
	float		camera_distortion[5];
	float		camera_intrinsic[9];
	
	int			use_calibration;
	
	aruco::MarkerDetector MDetector;
	std::vector<aruco::Marker> Markers;
	
	cv::Mat cvIntrinsic, cvDistortion;
	
	t_aruco() {
		markersize = 0.1f;
		use_calibration = 1;
		
		cvIntrinsic = cv::Mat(3, 3, CV_32F, camera_intrinsic);
		cvIntrinsic = 0.f;
		cvIntrinsic.at<float>(0,0) = 640.f;				
		cvIntrinsic.at<float>(1,1) = 640.f;		
		cvIntrinsic.at<float>(0,2) = 320.f;				
		cvIntrinsic.at<float>(1,2) = 240.f;				
		cvIntrinsic.at<float>(2,2) = 1.f;	
		
		cvDistortion = cv::Mat(5, 1, CV_32F, camera_distortion);
		cvDistortion = 0.f;
	}

	~t_aruco() {
	
	}
	
	void bang() {
		t_atom a[8];
		
		for (unsigned int i=0;i<Markers.size();i++) {
			aruco::Marker& m = Markers[i];
		
			//cv::Mat Rvec,Tvec
			
			atom_setlong(a+0, i);
			atom_setlong(a+1, m.id);
			atom_setfloat(a+2, m.Tvec.at<float>(0));
			atom_setfloat(a+3, m.Tvec.at<float>(1));
			atom_setfloat(a+4, m.Tvec.at<float>(2));
			
			outlet_list(outlet_c, 0, 5, a);
		}
	}
	
	void jit_matrix(t_symbol * name) {
		t_jit_matrix_info in_info;
		char * in_bp;
		t_atom a[1];
		
		void * in_mat = jit_object_findregistered(name);
		
		// lock it:
		long in_savelock = (long)jit_object_method(in_mat, _jit_sym_lock, 1);
		
		// ensure data exists:
		jit_object_method(in_mat, _jit_sym_getdata, &in_bp);
		if (!in_bp) {
			jit_error_code(&ob, JIT_ERR_INVALID_INPUT);
			return;
		}
		
		// ensure the type is correct:
		jit_object_method(in_mat, _jit_sym_getinfo, &in_info);
		if (in_info.type != _jit_sym_char) {
			jit_error_code(&ob, JIT_ERR_MISMATCH_TYPE);
			return;
//		} else if (in_info.planecount > 1) {
//			jit_error_code(&ob, JIT_ERR_MISMATCH_PLANE);
//			return;
		} else if (in_info.dimcount != 2) {
			jit_error_code(&ob, JIT_ERR_MISMATCH_DIM);
			return;
		}
		
		// create CV mat wrapper around Jitter matrix data
		// (cv declares dim as numrows, numcols)
		cv::Mat InImage(in_info.dim[1], in_info.dim[0], CV_8UC(in_info.planecount), in_bp, in_info.dimstride[1]);

		cv::Mat grey;
		if ( InImage.type()==CV_8UC1) grey=InImage;
		else cv::cvtColor(InImage,grey,CV_RGBA2GRAY);
		
		try {
		
			bool setYPerpendicular = false;
		
			aruco::CameraParameters CParams(cvIntrinsic, cvDistortion, cv::Size(in_info.dim[0], in_info.dim[1]));
		
			if (use_calibration) {				
				MDetector.detect(grey, Markers, CParams, markersize, setYPerpendicular);
				
				//for each marker, draw info and its boundaries in the image
				for (unsigned int i=0;i<Markers.size();i++) {
				
					aruco::Marker& m = Markers[i];
				
					m.draw(InImage,cv::Scalar(0,0,255),2);
					aruco::CvDrawingUtils::draw3dCube(InImage, m, CParams, setYPerpendicular);
					
				}
				
			} else {
				MDetector.detect(grey, Markers);
				
				//for each marker, draw info and its boundaries in the image
				for (unsigned int i=0;i<Markers.size();i++) {
				
					aruco::Marker& m = Markers[i];
				
					m.draw(InImage,cv::Scalar(0,0,255),2);
					
				}
				
			}
			
		} catch (std::exception &ex) {
			object_error(&ob, "exception: %s", ex.what());
		}
		
		// restore matrix lock state:
		jit_object_method(in_mat, _jit_sym_lock, in_savelock);
		
		// output the count:
		atom_setlong(a, Markers.size());
		outlet_anything(outlet_msg, _sym_count, 1, a);
			
		// output the image:
		atom_setsym(a, name);
		outlet_anything(outlet_img, _jit_sym_jit_matrix, 1, a);
		
		// now output the markers:
		bang();
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
			sprintf(s, "modified image (jit_matrix)"); 
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
		
		x->outlet_msg = outlet_new(x, 0);
		x->outlet_img = outlet_new(x, "jit_matrix");
		x->outlet_c = listout(x);
		
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

void aruco_jit_matrix(t_aruco * x, t_symbol *s) {
	x->jit_matrix(s);
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
	
	class_addmethod(maxclass, (method)aruco_jit_matrix, "jit_matrix", A_SYM, 0); 
	class_addmethod(maxclass, (method)aruco_bang, "bang", 0);
	
	CLASS_ATTR_FLOAT(maxclass, "markersize", 0, t_aruco, markersize);
	
	CLASS_ATTR_FLOAT_ARRAY(maxclass, "camera_distortion", 0, t_aruco, camera_distortion, 5);

	CLASS_ATTR_FLOAT_ARRAY(maxclass, "camera_intrinsic", 0, t_aruco, camera_intrinsic, 9);
	
	CLASS_ATTR_LONG(maxclass, "use_calibration", 0, t_aruco, use_calibration);
	CLASS_ATTR_STYLE(maxclass, "use_calibration", 0, "onoff");
	
	
	class_register(CLASS_BOX, maxclass); 
	aruco_class = maxclass;
	return 0;
}