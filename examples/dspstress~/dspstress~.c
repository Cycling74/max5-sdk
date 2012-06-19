/**
 @file
 dspstress~ - very simple msp object that does nothing except eat up a specified % of processor time 
 
 updated 6/5/09 rbs: initial
 
 @ingroup	examples	
 */

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

void *dspstress_class;

typedef struct _dspstress
{
    t_pxobject	x_obj;
    double		x_cpuusagetarget;	// how much cpu to burn (0 - 100)
	double		x_sr;				
	double		x_svs;		
	double		x_svtime_ms;		// how long one signal vector takes in ms
} t_dspstress;

void *dspstress_new(double val);
t_int *dspstress_perform(t_int *w);
void dspstress_float(t_dspstress *x, double f);
void dspstress_int(t_dspstress *x, long n);
void dspstress_dsp(t_dspstress *x, t_signal **sp, short *count);
void dspstress_assist(t_dspstress *x, void *b, long m, long a, char *s);

int main(void)
{
    t_class *c;

	c = class_new("dspstress~", (method)dspstress_new, (method)dsp_free, (short)sizeof(t_dspstress), 0L, A_DEFFLOAT, 0);
    
    class_addmethod(c, (method)dspstress_dsp, "dsp", A_CANT, 0); 	// respond to the dsp message 
    																// (sent to MSP objects when audio is turned on/off)
    class_addmethod(c, (method)dspstress_float, "float", A_FLOAT, 0);
    class_addmethod(c, (method)dspstress_int, "int", A_LONG, 0);
    class_addmethod(c, (method)dspstress_assist,"assist",A_CANT,0);
    class_dspinit(c);								// must call this function for MSP object classes

	class_register(CLASS_BOX, c);
	dspstress_class = c;
	return 0;
}

void dspstress_assist(t_dspstress *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) {
		switch (a) {	
		case 0:
			strcpy(s,"Specify cpu usage % here");
			break;
		}
	}
}

void *dspstress_new(double val)
{
    t_dspstress *x = object_alloc(dspstress_class);
    dsp_setup((t_pxobject *)x,1);					// set up DSP for the instance and create signal inlet
	x->x_sr = 0; 
	x->x_svs = 0; 
	dspstress_float(x, val); 
    return x;
}

void dspstress_float(t_dspstress *x, double f)				// the float and int routines cover both inlets. 
{															// It doesn't matter which one is involved
	x->x_cpuusagetarget = f;
}

void dspstress_int(t_dspstress *x, long n)
{
	dspstress_float(x, (double) n); 
}

t_int *dspstress_perform(t_int *w)						// our perform method 
{
	t_dspstress *x = (t_dspstress *)(w[1]);
	float spintime;
	double intime; 
	double outtime; 
	unsigned long spincounter = 0;

	if (x->x_obj.z_disabled)
		goto out;
	
	spintime = x->x_svtime_ms * x->x_cpuusagetarget / 100.; 

	intime = systimer_gettime(); 
	outtime = intime + spintime; 
	while (systimer_gettime() < outtime)
	{
		// tra la la
		spincounter++;  // how high can we count? 
	}

out:
    return (w+2);
}

void dspstress_dsp(t_dspstress *x, t_signal **sp, short *count)	// method called when dsp is turned on
{
	dsp_add(dspstress_perform, 1, x);
	x->x_sr = sp[0]->s_sr; 
	x->x_svs = sp[0]->s_n;  
	x->x_svtime_ms = x->x_svs / x->x_sr * 1000.; 
}

