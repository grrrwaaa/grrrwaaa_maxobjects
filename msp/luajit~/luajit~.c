
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

// #define WIN_SSE_INTRINSICS

#ifdef WIN_VERSION
#include <xmmintrin.h>
#include <emmintrin.h>
#endif

static t_class *luajit_class;

typedef struct {
    t_pxobject	x_obj;
    t_sample	x_val;
} t_luajit;


void luajit_assist(t_luajit *x, void *b, long m, long a, char *s);
void *luajit_new(double val);
void luajit_dsp64(t_luajit *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void luajit_float(t_luajit *x, double f);
void luajit_int(t_luajit *x, long n);
void scale_perform64_method(t_luajit *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void luajit_perform64_method(t_luajit *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);


int C74_EXPORT main(void)
{
	t_class *c;

	c = class_new("luajit~", (method)luajit_new, (method)dsp_free, sizeof(t_luajit), 0L, A_DEFFLOAT, 0);
	class_dspinit(c);

	class_addmethod(c, (method)luajit_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(c, (method)luajit_float, "float", A_FLOAT, 0);
	class_addmethod(c, (method)luajit_int, "int", A_LONG, 0);
	class_addmethod(c, (method)luajit_assist, "assist", A_CANT, 0);
	class_setname("*~","luajit~"); // because the filename on disk is different from the object name in Max
	
	class_register(CLASS_BOX, c);
	luajit_class = c;

	return 0;
}

// this routine covers both inlets. It doesn't matter which one is involved
void luajit_float(t_luajit *x, double f)
{
	x->x_val = f;
}


void luajit_int(t_luajit *x, long n)
{
	luajit_float(x,(double)n);
}


void luajit_dsp64(t_luajit *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	if (!count[1]) {
		dsp_add64(dsp64, (t_object*) x, (t_perfroutine64) scale_perform64_method, 0, 0); 
	}
	else if (!count[0]) {
		dsp_add64(dsp64, (t_object*) x, (t_perfroutine64) scale_perform64_method, 0, (void*) 1); 
	}
	else {
		dsp_add64(dsp64, (t_object*) x, (t_perfroutine64) luajit_perform64_method, 0, 0);
	}
}


void scale_perform64_method(t_luajit *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	int invec = (int) userparam;   // used to signal which one is the signal input (1 for right, 0 for left)
	t_double *in = ins[invec];
	t_double *out = outs[0];
	t_double val = x->x_val;
#if defined(WIN_VERSION) && defined(WIN_SSE_INTRINSICS)
	__m128d mm_in1;
	__m128d *mm_out1 = (__m128d*) out; 
	__m128d mm_in2;
	__m128d *mm_out2 = (__m128d*) out; 
	__m128d mm_val;
	__declspec(align(16)) t_double aligned_val = x->x_val;
	int i;

	mm_val = _mm_set1_pd(aligned_val);

	// rbs fix: this version will break if the SVS is smaller than 4
	C74_ASSERT(sampleframes >= 4);
	for (i=0; i < sampleframes; i+=4) {
		mm_in1 = _mm_load_pd(in+i); 
		mm_out1[i/2] = _mm_mul_pd(mm_in1, mm_val); 
		mm_in2 = _mm_load_pd(in+i+2); 
		mm_out2[i/2 + 1] = _mm_mul_pd(mm_in2, mm_val); 
	}

#else
	t_double ftmp;

//	if (IS_DENORM_DOUBLE(*in)) {
//		static int counter = 0; 
//		post("luajit~ (%p): saw denorm (%d)", x, counter++); 
//	}

	while (sampleframes--) {
		ftmp = val * *in++;
		FIX_DENORM_NAN_DOUBLE(ftmp);
		*out++ = ftmp;
	}
#endif
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

void *luajit_new(double val)
{
    t_luajit *x = (t_luajit*) object_alloc((t_class*) luajit_class);
    dsp_setup((t_pxobject *)x,2);
    outlet_new((t_pxobject *)x, "signal");
    x->x_val = val; // splatted in _dsp method if optimizations are on
    
    return (x);
}

