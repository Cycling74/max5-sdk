/**
 @file
 times~ - the *~ signal operator
 SDK example to illustrate platform-safe Altivec optimization
 
 updated 3/22/09 ajm: new API
 
 @ingroup	examples	
 */

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

/*

The idea here is that we can create source code that works on multiple
platforms at compile time, and works on both Altivec- and non-Altivec
PowerPC machines at runtime. We do this by isolating Altivec-specific
code inside #ifdefs and using the routines sys_optimize and sys_altivec
that return whether the user can and wants to optimize
to protect non-Altivec machines from crashing.

The conditions for sys_optimize() to return true:

1. Altivec-capable machine
2. User has checked Optimize in the DSP Status window
3. Signal Vector size is a multiple of 4

Another thing is that assuming you are going to want to decide whether to
use optimized code or not, you should only call sys_optimize() in your DSP
method. Then you can assume that the value will not change until your DSP
method is called again. In other words, the change of the user optimization
state is a "triggering event" for a DSP chain recompilation. sys_optimize()
CANNOT be called in your perform routine because it may allocate memory,
and in any case it is SLOW, much slower than storing its current value
in your object or, as is done in the example below, simply putting different
functions (optimized vs. non-optimized) on the DSP call chain depending
on what its value is in your DSP method.

When you want to use something that is specific to the Altivec compiler,
you need to put it within two things:

#ifdef __ALTIVEC__

and

#pragma altivec_model on

The first one is true is the compiler is capable of generating altivec
code. This could be true WHETHER OR NOT the actual machine running the
compiler has Altivec instructions! The second tells the compiler to start 
generating these instructions (and therefore, understanding the use of
the word vector etc.). By leaving the
altivec code generation OFF by default in the project settings, we eliminate
the risk that in the future, Altivec optimization might be automatically
performed by the compiler that would crash non-Altivec machines.

ALSO note that the only include file you need to write altivec code etc. is
z_dsp.h, which includes z_altivec.h. See z_altivec.h for more obscurity...

*/

void *times_class;


typedef struct _times
{
    t_pxobject x_obj;
    t_float x_val;
#ifdef __ALTIVEC__
#pragma altivec_model on
	vector float x_vecVal;
#pragma altivec_model off
#endif // __ALTIVEC__
} t_times;

t_int *times_perform(t_int *w);
t_int *scale_perform(t_int *w);
void times_float(t_times *x, double f);
void times_int(t_times *x, long n);
#ifdef __ALTIVEC__			// these are our altivec-specific routines
void times_floatAV(t_times *x, double f);
t_int *times_performAV(t_int *w);
t_int *scale_performAV(t_int *w);
#endif 
void times_dsp(t_times *x, t_signal **sp, short *count);
void times_assist(t_times *x, void *b, long m, long a, char *s);
void *times_new(double val);

int main(void)
{
	t_class *c;

	c = class_new("times~", (method)times_new, (method)dsp_free, (short)sizeof(t_times), 0L, A_DEFFLOAT, 0);
	class_dspinit(c);
	class_addmethod(c, (method)times_dsp, "dsp", A_CANT, 0);
	class_addmethod(c, (method)times_float, "float", A_FLOAT, 0);
	class_addmethod(c, (method)times_int, "int", A_LONG, 0);
	class_addmethod(c, (method)times_assist, "assist", A_CANT, 0);
	class_setname("*~","times~"); // because the filename on disk is different from the object name in Max
	class_register(CLASS_BOX, c);
	times_class = c;

	return 0;
}

// this routine covers both inlets. It doesn't matter which one is involved
void times_float(t_times *x, double f)
{
	x->x_val = f;
 	if (DSP_SIMPLE_OPTIMIZE_TEST_PARAM) 
#ifdef __ALTIVEC__
		times_floatAV(x,f);
#else
		;
#endif
}

void times_int(t_times *x, long n)
{
	times_float(x,(double)n);
}

// here are the two non-vector-optimized perform routines

t_int *times_perform(t_int *w)
{
    t_float *in1,*in2,*out;
    int n;
#ifdef DENORM_WANT_FIX				// if we want to fix denormalized floating-point numbers (Windows XP)
	float ftmp;
#endif

    if (*(long *)(w[1]))
	    goto out;
	in1 = (t_float *)(w[2]);
	in2 = (t_float *)(w[3]);
	out = (t_float *)(w[4]);
	n = (int)(w[5]);

#ifdef DENORM_WANT_FIX				// if we want to fix denormalized floating-point numbers (Windows XP)
	while (n--) {
		ftmp = *in1++ * *in2++;
		FIX_DENORM_NAN_FLOAT(ftmp);
		*out++ = ftmp;
	}
#else
	while (n--) *out++ = *in1++ * *in2++; 
#endif

out:
	return (w + 6);
}

t_int *scale_perform(t_int *w)
{
    t_float *in, *out;
    float val;
    int n;
#ifdef DENORM_WANT_FIX				// if we want to fix denormalized floating-point numbers (Windows XP)
	float ftmp;
#endif
    
	t_times *x = (t_times *)(w[3]);
	if (x->x_obj.z_disabled)
		goto out;
    in = (t_float *)(w[1]);
    out = (t_float *)(w[2]);
	val = x->x_val;
	n = (int)(w[4]);

#ifdef DENORM_WANT_FIX				// if we want to fix denormalized floating-point numbers (Windows XP)
	while (n--) {
		ftmp = val * *in++;
		FIX_DENORM_NAN_FLOAT(ftmp);
		*out++ = ftmp;
	}
#else	
	while (n--) *out++ = val * *in++; 
#endif
out:
    return (w + 5);
} 		

#ifdef __ALTIVEC__
#pragma altivec_model on  // turn AltiVec code generation on												

void times_floatAV(t_times *x, double f)
{
	vec_splat_float(x->x_vecVal, f); // copy the new scalar to the vector
}

// here is an Altivec-optimized routine that multiples two signals together, producing a third signal
// vec_madd(a,b,c) does result = a * (b + c), so we need a zero vector that we initialize locally

t_int *times_performAV(t_int *w)
{
	vector float *v_in1, *v_in2, *v_out, zero;
	unsigned int n = (int)(w[5]);			
	
    if (*(long *)(w[1]))
	    goto out;
	zero = (vector float) (0, 0, 0, 0);
	v_in1 = (vector float *)(w[2]);
	v_in2 = (vector float *)(w[3]);
	v_out = (vector float *)(w[4]);
	while (n--) {
		*v_out++ = vec_madd(*v_in1++, *v_in2++, zero);
	}
out:
	return (w + 6);
}

// here is an Altivec-optimized routine that multiples a signal by a scalar
// vec_madd is used as above, but the val argument refers to the vector
// we stored in our object

t_int *scale_performAV(t_int *w)
{	
	t_times *x = (t_times *)(w[3]);
	int n = (int)(w[4]);	
	vector float *v_in, *v_out, val, zero;		
	floatToVector foo;
	
	if (x->x_obj.z_disabled) 
		goto out;
		
    zero = (vector float) (0, 0, 0, 0);
 	v_in =  (vector float *)(w[1]);
	v_out = (vector float *)(w[2]);
	val = x->x_vecVal;
	while (n--) {
		*v_out++ = vec_madd(*v_in++, val, zero);
	}
	
out:
    return (w + 5);
} 

#pragma altivec_model off // turn AltiVec code generation off
#endif // __ALTIVEC__


void times_dsp(t_times *x, t_signal **sp, short *count)
{
	if (DSP_SIMPLE_OPTIMIZE_TEST(sp[0])) {
#ifdef __ALTIVEC__
#pragma altivec_model on
			// we need to do this in case the scalar was stored with optimization off
			// and we are just now turning optimization on. See I told you using this
			// vector stuff was tricky...
		times_floatAV(x,x->x_val);	// splat the vector
			// if optimization is on, we are going to use one of our two optimized
			// perform routines. We pick the proper one based on whether signals
			// are connected to both inlets. If they are, we use times_performAV,
			// otherwise we will be multiplying by a scalar
		if (!count[1]) 
			dsp_add(scale_performAV, 4, sp[0]->s_vec, sp[2]->s_vec, x, (sp[0]->s_n / 4));
		else if (!count[0]) 
			dsp_add(scale_performAV, 4, sp[1]->s_vec, sp[2]->s_vec, x, (sp[0]->s_n / 4));
		else {
			dsp_add(times_performAV, 5, &x->x_obj.z_disabled, sp[0]->s_vec, sp[1]->s_vec,
				sp[2]->s_vec, (sp[0]->s_n / 4));
		}
#pragma altivec_model off
#else
			// this error would be generated at runtime if the code were to be compiled on
			// a compiler or machine without ALTIVEC and for some reason sys_optimize returned true
		object_error((t_object *)x, "no optimizations available"); // in theory this should never happen...
#endif	// __ALTIVEC__
	} else {
		if (!count[1])
			dsp_add(scale_perform, 4, sp[0]->s_vec, sp[2]->s_vec, x, sp[0]->s_n);
		else if (!count[0])
			dsp_add(scale_perform, 4, sp[1]->s_vec, sp[2]->s_vec, x, sp[0]->s_n);
		else
			dsp_add(times_perform, 5, &x->x_obj.z_disabled, sp[0]->s_vec, sp[1]->s_vec,
				sp[2]->s_vec, sp[0]->s_n);
	}
}

void times_assist(t_times *x, void *b, long m, long a, char *s)
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

void *times_new(double val)
{
    t_times *x = object_alloc(times_class);
    dsp_setup((t_pxobject *)x,2);
    outlet_new((t_pxobject *)x, "signal");
    x->x_val = val; // splatted in _dsp method if optimizations are on
    
    return (x);
}

