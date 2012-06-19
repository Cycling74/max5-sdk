/**
 @file
 plussz~ - a very simple example of a basic MSP object 
 
 updated 3/22/09 ajm: new API
 
 @ingroup	examples	
 */

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

void *plussz_class;

typedef struct _plus
{
    t_pxobject x_obj;
    t_float x_val;
} t_plus;

void *plus_new(double val);
t_int *offset_perform(t_int *w);
t_int *plus2_perform(t_int *w);
void plus_float(t_plus *x, double f);
void plus_int(t_plus *x, long n);
void plus_dsp(t_plus *x, t_signal **sp, short *count);
void plus_assist(t_plus *x, void *b, long m, long a, char *s);

int main(void)
{
    t_class *c;

	c = class_new("plussz~", (method)plus_new, (method)dsp_free, (short)sizeof(t_plus), 0L, A_DEFFLOAT, 0);
    
    class_addmethod(c, (method)plus_dsp, "dsp", A_CANT, 0); 	// respond to the dsp message 
    												// (sent to MSP objects when audio is turned on/off)
    class_addmethod(c, (method)plus_float, "float", A_FLOAT, 0);
    class_addmethod(c, (method)plus_int, "int", A_LONG, 0);
    class_addmethod(c, (method)plus_assist,"assist",A_CANT,0);
    class_dspinit(c);								// must call this function for MSP object classes

	class_register(CLASS_BOX, c);
	plussz_class = c;
	return 0;
}

void plus_assist(t_plus *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) {
		switch (a) {	
		case 0:
			sprintf(s,"(Signal) This + Right Inlet");
			break;
		case 1:
			sprintf(s,"(Signal) Left Inlet + This");
			break;
		}
	} else
		sprintf(s,"(Signal) Addition Result");
}

void *plus_new(double val)
{
    t_plus *x = object_alloc(plussz_class);
    dsp_setup((t_pxobject *)x,2);					// set up DSP for the instance and create signal inlets
    outlet_new((t_pxobject *)x, "signal");			// signal outlets are created like this
    x->x_val = val;
    return (x);
}

void plus_float(t_plus *x, double f)				// the float and int routines cover both inlets. 
{													// It doesn't matter which one is involved
	x->x_val = f;
}

void plus_int(t_plus *x, long n)
{
	x->x_val = (float)n;
}

t_int *offset_perform(t_int *w)						// our perform method if one signal inlet is connected
{
    t_float *in = (t_float *)(w[1]);
    t_float *out = (t_float *)(w[2]);
	t_plus *x = (t_plus *)(w[3]);
	float val = x->x_val;
	int n = (int)(w[4]);
	
	if (x->x_obj.z_disabled)
		goto out;
	
    while (n--) *out++ = val + *in++; 
out:
    return (w+5);
}

t_int *plus2_perform(t_int *w)						// our perform method if both signal inlets are connected
{
	t_float *in1,*in2,*out;
	int n;

	if (*(long *)(w[1]))							// just doing this a different way for some variation
	    goto out;

	in1 = (t_float *)(w[2]);
	in2 = (t_float *)(w[3]);
	out = (t_float *)(w[4]);
	n = (int)(w[5]);
	
	while (n--) *out++ = *in1++ + *in2++;
out:
	return (w+6);
}		

void plus_dsp(t_plus *x, t_signal **sp, short *count)	// method called when dsp is turned on
{
	if (!count[0])
		dsp_add(offset_perform, 4, sp[1]->s_vec, sp[2]->s_vec, x, sp[0]->s_n);
	else if (!count[1])
		dsp_add(offset_perform, 4, sp[0]->s_vec, sp[2]->s_vec, x, sp[0]->s_n);
	else
		dsp_add(plus2_perform, 5, &x->x_obj.z_disabled, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

