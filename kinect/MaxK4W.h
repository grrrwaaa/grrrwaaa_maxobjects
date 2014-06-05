#include "MaxKinectBase.h"

class t_kinect : public MaxKinectBase {
public:
		
	t_kinect() {	
		
	}

	~t_kinect() {
		close();
	}
	
	void getdevlist() {
		t_atom a[8];
		
	}
	
	void open(t_symbol *s, long argc, t_atom *argv) {
	
		
	}
	
	void close() {
		
		// mark one less device running:
		capturing--;
	}
	
	void accel() {
		t_atom a[3];
		double x, y, z;
		
		
		atom_setfloat(a+0, x);
		atom_setfloat(a+1, y);
		atom_setfloat(a+2, z);
		outlet_anything(outlet_msg, gensym("accel"), 3, a);
	}
	
	void led(int option) {
		object_warn(&ob, "LED not yet implemented for Windows");
	}
	
	void bang() {
	
	}
	
	static void *capture_threadfunc(void *arg) {
		t_kinect *x = (t_kinect *)arg;
		
		capturing = 1;
		post("freenect starting processing");
		while (capturing > 0) {
			
			systhread_sleep(0);
		}
		post("freenect finished processing");
		
	out: 
		systhread_exit(NULL);
		return NULL;
	}
};