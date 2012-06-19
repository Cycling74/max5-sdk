/**
 @file
 index~ - SDK example of an object which accesses an MSP buffer~
 
 updated 3/22/09 ajm: new API
 
 @ingroup	examples	
 */

#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h" // contains CLIP macro
#include "z_dsp.h"
#include "buffer.h"	// this defines our buffer's data structure and other goodies
#include "ext_atomic.h"

void *index_class;

typedef struct _index
{
    t_pxobject l_obj;
    t_symbol *l_sym;
    t_buffer *l_buf;
    long l_chan;
} t_index;

t_int *index_perform(t_int *w);
void index_dsp(t_index *x, t_signal **sp);
void index_set(t_index *x, t_symbol *s);
void *index_new(t_symbol *s, long chan);
void index_in1(t_index *x, long n);
void index_assist(t_index *x, void *b, long m, long a, char *s);
void index_dblclick(t_index *x);

t_symbol *ps_buffer;

int main(void)
{
	t_class *c;

	c = class_new("index~", (method)index_new, (method)dsp_free, (short)sizeof(t_index), 0L, 
		A_SYM, A_DEFLONG, 0);
	class_addmethod(c, (method)index_dsp, "dsp", A_CANT, 0);
	class_addmethod(c, (method)index_set, "set", A_SYM, 0);
	class_addmethod(c, (method)index_in1, "in1", A_LONG, 0);
	class_addmethod(c, (method)index_assist, "assist", A_CANT, 0);
	class_addmethod(c, (method)index_dblclick, "dblclick", A_CANT, 0);
	class_dspinit(c);
	class_register(CLASS_BOX, c);
	index_class = c;
	
	ps_buffer = gensym("buffer~");

	return 0;
}


t_int *index_perform(t_int *w)
{
    t_index *x = (t_index *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    int n = (int)(w[4]);
	t_buffer *b = x->l_buf;
	float *tab;
	double temp;
	double f;
	long index,chan,frames,nc;
	
	if (x->l_obj.z_disabled)
		goto out;
	if (!b)
		goto zero;
	ATOMIC_INCREMENT(&b->b_inuse);
	if (!b->b_valid) {
		ATOMIC_DECREMENT(&b->b_inuse);
		goto zero;
	}
	tab = b->b_samples;
	chan = x->l_chan;
	frames = b->b_frames;
	nc = b->b_nchans;
	while (n--) {
		temp = *in++;
		f = temp + 0.5;
		index = f;
		if (index < 0)
			index = 0;
		else if (index >= frames)
			index = frames - 1;
		if (nc > 1)
			index = index * nc + chan;
		*out++ = tab[index];
	}
	ATOMIC_DECREMENT(&b->b_inuse);
	return w + 5;
zero:
	while (n--) *out++ = 0.;
out:
	return w + 5;
}

// here's where we set the buffer~ we're going to access
void index_set(t_index *x, t_symbol *s)
{
	t_buffer *b;
	
	if (s) {
		x->l_sym = s;
		if ((b = (t_buffer *)(s->s_thing)) && ob_sym(b) == ps_buffer) {
			x->l_buf = b;
		} else {
			object_error((t_object *)x, "no buffer~ %s", s->s_name);
			x->l_buf = 0;
		}
	} else {
		// this will reappear every time the dsp is restarted; do we really want it?
		object_error((t_object *)x, "no buffer~ object specified");
	}
}

void index_in1(t_index *x, long n)
{
	if (n)
		x->l_chan = CLIP(n,1,4) - 1;
	else
		x->l_chan = 0;
}

void index_dsp(t_index *x, t_signal **sp)
{
    index_set(x,x->l_sym);
    dsp_add(index_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

// this lets us double-click on index~ to open up the buffer~ it references
void index_dblclick(t_index *x)
{
	t_buffer *b;
	
	if ((b = (t_buffer *)(x->l_sym->s_thing)) && ob_sym(b) == ps_buffer)
		mess0((t_object *)b,gensym("dblclick"));
}

void index_assist(t_index *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_OUTLET)
		sprintf(s,"(signal) Sample Value at Index");
	else {
		switch (a) {	
			case 0:	sprintf(s,"(signal) Sample Index");	break;
			case 1:	sprintf(s,"Audio Channel In buffer~");	break;
		}
	}
}

void *index_new(t_symbol *s, long chan)
{
	t_index *x = object_alloc(index_class);
	dsp_setup((t_pxobject *)x, 1);
	intin((t_object *)x,1);
	outlet_new((t_object *)x, "signal");
	x->l_sym = s;
	index_in1(x,chan);
	return (x);
}

