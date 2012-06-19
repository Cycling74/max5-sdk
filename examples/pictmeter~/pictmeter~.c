/**
	@file
	pictmeter~ - audio meter that works by resizing an image

	@ingroup	examples
*/

#include "ext.h"							// standard Max include, always required
#include "ext_obex.h"						// required for new style Max object
#include "jpatcher_api.h"
#include "jgraphics.h"
#include "z_dsp.h"							// should be after jpatcher_api.h
#include "ext_drag.h"

typedef struct _pictmeter 
{
	t_pxjbox	p_obj;
	t_jsurface *p_surface;
	void *p_clock;
	double p_value;
	double p_max;
	char p_startclock;
} t_pictmeter;

void *pictmeter_new(t_symbol *s, long argc, t_atom *argv);
void pictmeter_free(t_pictmeter *x);
void pictmeter_assist(t_pictmeter *x, void *b, long m, long a, char *s);
void pictmeter_paint(t_pictmeter *x, t_object *patcherview);
void pictmeter_read(t_pictmeter *x, t_symbol *s);
void pictmeter_doread(t_pictmeter *x, t_symbol *s, long argc, t_atom *argv);
long pictmeter_acceptsdrag_unlocked(t_pictmeter *x, t_object *drag, t_object *view);
void pictmeter_dsp(t_pictmeter *x, t_signal **sp, short *count);
t_int *pictmeter_perform(t_int *w);
void pictmeter_tick(t_pictmeter *x);

static t_class *s_pictmeter_class;

int main(void)
{	
	t_class *c;
		
	c = class_new("pictmeter~", (method)pictmeter_new, (method)pictmeter_free, sizeof(t_pictmeter), 0L, A_GIMME, 0);

	c->c_flags |= CLASS_FLAG_NEWDICTIONARY;
	jbox_initclass(c, 0);
	class_dspinitjbox(c);

	class_addmethod(c, (method)pictmeter_dsp, "dsp", A_CANT, 0);
	class_addmethod(c, (method)pictmeter_paint, "paint",	A_CANT, 0);
	class_addmethod(c, (method)pictmeter_assist,			"assist",		A_CANT, 0);  
	class_addmethod(c, (method)pictmeter_acceptsdrag_unlocked, "acceptsdrag_unlocked", A_CANT, 0);
	class_addmethod(c, (method)pictmeter_acceptsdrag_unlocked, "acceptsdrag_locked", A_CANT, 0);
	class_addmethod(c, (method)pictmeter_read, "read", A_DEFSYM, 0);
	
	CLASS_ATTR_DEFAULT(c,"patching_rect",0, "0. 0. 128. 128.");
	
	class_register(CLASS_BOX, c);
	s_pictmeter_class = c;
	return 0;
}

void pictmeter_assist(t_pictmeter *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET)	//inlet
		sprintf(s, "(signal) Audio Input");
}

void pictmeter_paint(t_pictmeter *x, t_object *patcherview)
{
	t_rect src, dst, rect;
	t_jgraphics *g = (t_jgraphics*) patcherview_get_jgraphics(patcherview);		// obtain graphics context
	jbox_get_rect_for_view((t_object *)x, patcherview, &rect);
	
	if (!x->p_surface)
		return;

	// draw if the value is non zero
	if (x->p_value) {
		src.width = jgraphics_image_surface_get_width(x->p_surface);
		src.height = jgraphics_image_surface_get_height(x->p_surface);
		src.x = 0;
		src.y = 0;
		dst.width = rect.width * x->p_value;
		dst.height = rect.height * x->p_value;
		if (dst.width < 1 || dst.height < 1)
			return;
		dst.x = 0 + ((rect.width * 0.5) - (dst.width * 0.5));
		dst.y = 0 + ((rect.height * 0.5) - (dst.height * 0.5));
		
		jgraphics_image_surface_draw(g, x->p_surface, src, dst);
	}
}

void pictmeter_read(t_pictmeter *x, t_symbol *s)
{
	defer((t_object *)x, (method)pictmeter_doread, s, 0, NULL);
}

void pictmeter_doread(t_pictmeter *x, t_symbol *s, long argc, t_atom *argv)
{
	char filename[MAX_PATH_CHARS];
	long *type,ntype,outtype;
	t_max_err err;
	char alloc;
	short path;
	t_jsurface *surface;
	
	jgraphics_getfiletypes(x, &ntype, &type, &alloc);
	if (s == gensym("")) {
		err = open_dialog(filename, &path,(void *)&outtype, (void *)type, ntype);
		if (err)
			return;
	} else {		
		strcpy(filename,s->s_name);
		err = locatefile_extended(filename, &path, &outtype, type, ntype);
		if (err)
			return;
	}
	surface = jgraphics_image_surface_create_referenced(filename, path); 
	if (surface)
		x->p_surface = surface;
	if (alloc)
		sysmem_freeptr((char *)type);
}

long pictmeter_acceptsdrag_unlocked(t_pictmeter *x, t_object *drag, t_object *view)
{
	if (jdrag_matchdragrole(drag, gensym("imagefile"), 0)) {
		jdrag_box_add(drag, (t_object *)x, gensym("read"));
		return true;
	}
	return false;
}

void pictmeter_dsp(t_pictmeter *x, t_signal **sp, short *count)
{
	x->p_value = 0.;
	x->p_max = 0.;
	// only put perf func on dsp chain if sig is connected
	if (count[0]) {
		dsp_add(pictmeter_perform, 3, x, sp[0]->s_vec, sp[0]->s_n);
		x->p_startclock = true;
	}
}

t_int *pictmeter_perform(t_int *w)
{
	t_pictmeter *x = (t_pictmeter *)(w[1]);
    t_float *in = (t_float *)(w[2]);
	int n = (int)(w[3]);
	double xn, max;
		
	if (x->p_obj.z_disabled)
		goto out;
		
	max = x->p_max;
	
	while (n--) {
		xn = *in++;
		if (xn < 0)
			xn = -xn;
		if (xn > max)
			max = xn;
	}
	x->p_max = max;
		
	if (x->p_startclock) {
		x->p_startclock = 0;
		clock_delay(x->p_clock,0);
	}
out:
    return w + 4;
}

void pictmeter_tick(t_pictmeter *x)
{
	// for the astute student of the Max SDK:
	//
	// this method is called by the scheduler thread
	// x->p_max is also accessed by the perform method in the audio thread
	// we could use a mutex or critical region to protect the following block of code from having the value of x->p_max modified during its execution.
	// however, mutexes and critical regions carry a performance penalty.
	//
	// in this case, due to the nature of what we are doing (drawing something every tenth of second showing the history of the previous samples),
	// the mutex or critical region will not add anything to the object, or protect us from crashes, and it carries a performance penalty.
	// so we have made a conscious decision to not use the aforementioned thread locking mechanisms.
	
	if (x->p_value != x->p_max) {
		x->p_value = x->p_max;
		if (x->p_value > 1.)
			x->p_value = 1.;
		x->p_max = 0;
		jbox_redraw((t_jbox *)x);
	}
	
	if (sys_getdspstate())	// if the dsp is still on, schedule a next pictmeter_tick() call
		clock_fdelay(x->p_clock, 100);
}

void pictmeter_free(t_pictmeter *x)
{
	dsp_freejbox((t_pxjbox *)x);
	freeobject((t_object *)x->p_clock);
	jbox_free((t_jbox *)x);
}

void *pictmeter_new(t_symbol *s, long argc, t_atom *argv)
{
	t_pictmeter *x = NULL;
 	t_dictionary *d=NULL;
	long boxflags;
	
	if (!(d=object_dictionaryarg(argc,argv)))
		return NULL;
   
	x = (t_pictmeter *)object_alloc(s_pictmeter_class);
	boxflags = 0 
			| JBOX_DRAWFIRSTIN 
			| JBOX_NODRAWBOX
			| JBOX_DRAWINLAST
			| JBOX_TRANSPARENT	
	//		| JBOX_NOGROW
	//		| JBOX_GROWY
			| JBOX_GROWBOTH
	//		| JBOX_HILITE
	//		| JBOX_BACKGROUND
	//		| JBOX_DRAWBACKGROUND
	//		| JBOX_NOFLOATINSPECTOR
	//		| JBOX_TEXTFIELD
	//		| JBOX_MOUSEDRAGDELTA
	//		| JBOX_TEXTFIELD
			;

	jbox_new((t_jbox *)x, boxflags, argc, argv);
	x->p_obj.z_box.b_firstin = (void *)x;
	dsp_setupjbox((t_pxjbox *)x,1);
	x->p_clock = clock_new(x,(method)pictmeter_tick);
	x->p_value = x->p_max = 0;
	x->p_startclock = false;
	jbox_ready((t_jbox *)x);
	return x;
}
