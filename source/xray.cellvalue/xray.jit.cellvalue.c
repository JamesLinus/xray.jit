/* 
	xray.jit.cellminmax
	Wesley Smith
	wesley.hoke@gmail.com
	
	last modified: 12-7-2006
*/

#include "jit.common.h"

typedef struct _xray_jit_cellvalue  {
	t_object	ob;	
} t_xray_jit_cellvalue;

void *_xray_jit_cellvalue_class;

//jitter object/MOP methods
t_jit_err xray_jit_cellvalue_init(void);
t_xray_jit_cellvalue *xray_jit_cellvalue_new(void);
void xray_jit_cellvalue_free(t_xray_jit_cellvalue *x);
t_jit_err xray_jit_cellvalue_matrix_calc(t_xray_jit_cellvalue *x, void *inputs, void *outputs);

//data processing methods
void xray_jit_cellvalue_calculate_ndim(t_xray_jit_cellvalue *obj, long dimcount, long *dim, long planecount, 
		t_jit_matrix_info *in1_minfo, char *bip1, 
		t_jit_matrix_info *in2_minfo, char *bip2, 
		t_jit_matrix_info *out_minfo, char *bop);
	
t_jit_err xray_jit_cellvalue_init(void) 
{
	t_jit_object *mop,*o;
	t_atom a[1];
	
	_xray_jit_cellvalue_class = jit_class_new("xray_jit_cellvalue",(method)xray_jit_cellvalue_new,(method)xray_jit_cellvalue_free,
		sizeof(t_xray_jit_cellvalue),0L);

	//add mop - input 1 can be arbitrary 2D, input 2 is a 2-plane 2D float32 matrix
	//the output is of the same type and planecont as in1 but of the same dimension as in2
	mop = jit_object_new(_jit_sym_jit_mop,2,1);
	jit_mop_input_nolink(mop,2);
	o = jit_object_method(mop,_jit_sym_getinput,2);
	jit_attr_setlong(o,_jit_sym_dimlink,0);
	jit_attr_setlong(o,_jit_sym_typelink,0);
	jit_attr_setlong(o,_jit_sym_planelink,0);
	jit_object_method(o,_jit_sym_ioproc,jit_mop_ioproc_copy_adapt);
	jit_atom_setsym(a,_jit_sym_float32);
	jit_object_method(o,_jit_sym_types,1,a);
	
	o = jit_object_method(mop,_jit_sym_getoutput,1);
	jit_attr_setlong(o,_jit_sym_dimlink,0);
	
	jit_class_addadornment(_xray_jit_cellvalue_class,mop);

	//add methods
	jit_class_addmethod(_xray_jit_cellvalue_class, (method)xray_jit_cellvalue_matrix_calc, 		"matrix_calc", 		A_CANT, 0L);
	
	jit_class_register(_xray_jit_cellvalue_class);

	return JIT_ERR_NONE;
}

t_jit_err xray_jit_cellvalue_matrix_calc(t_xray_jit_cellvalue *x, void *inputs, void *outputs)
{
	t_jit_err err=JIT_ERR_NONE;
	long in1_savelock, in2_savelock, out_savelock;
	t_jit_matrix_info in1_minfo, in2_minfo, out_minfo;
	char *in1_bp, *in2_bp, *out_bp;
	long i,dimcount,planecount,dim[JIT_MATRIX_MAX_DIMCOUNT];
	void *in1_matrix, *in2_matrix, *out_matrix;
	
	in1_matrix 	= jit_object_method(inputs,_jit_sym_getindex,0);
	in2_matrix 	= jit_object_method(inputs,_jit_sym_getindex,1);
	out_matrix = jit_object_method(outputs,_jit_sym_getindex,0);

	if (x&&in1_matrix&&in2_matrix&&out_matrix) {
		in1_savelock = (long) jit_object_method(in1_matrix,_jit_sym_lock,1);
		in2_savelock = (long) jit_object_method(in2_matrix,_jit_sym_lock,1);
		out_savelock = (long) jit_object_method(out_matrix,_jit_sym_lock,1);
		
		jit_object_method(in1_matrix,_jit_sym_getinfo,&in1_minfo);
		jit_object_method(in2_matrix,_jit_sym_getinfo,&in2_minfo);
		jit_object_method(in1_matrix,_jit_sym_getdata,&in1_bp);
		jit_object_method(in2_matrix,_jit_sym_getdata,&in2_bp);
		
		if (!in1_bp || !in2_bp) { err=JIT_ERR_INVALID_INPUT; goto out;}	
			
		//get dimensions/planecount
		dimcount   = in1_minfo.dimcount;
		planecount = in1_minfo.planecount;			
		
		for (i=0;i<dimcount;i++) {
			dim[i] = in1_minfo.dim[i];
			if ((in1_minfo.dim[i]<dim[i])) dim[i] = in1_minfo.dim[i];
		}
		
		jit_object_method(out_matrix,_jit_sym_getinfo,&out_minfo);
		
		out_minfo.dim[0] = in2_minfo.dim[0];
		out_minfo.dim[1] = in2_minfo.dim[1];
		out_minfo.planecount = in1_minfo.planecount;
		out_minfo.type = in1_minfo.type;
		
		jit_object_method(out_matrix, _jit_sym_setinfo, &out_minfo);
		jit_object_method(out_matrix,_jit_sym_getinfo,&out_minfo);

		jit_object_method(out_matrix,_jit_sym_getdata,&out_bp);
		
		if (!out_bp) { err=JIT_ERR_INVALID_OUTPUT; goto out;}
		
		xray_jit_cellvalue_calculate_ndim(x, dimcount, dim, planecount, 
				&in1_minfo, in1_bp, 
				&in2_minfo, in2_bp, 
				&out_minfo, out_bp);
	}
	else {
		return JIT_ERR_INVALID_PTR;
	}
	
out:
	jit_object_method(out_matrix,gensym("lock"),out_savelock);
	jit_object_method(in2_matrix,gensym("lock"),in2_savelock);
	jit_object_method(in1_matrix,gensym("lock"),in1_savelock);
	return err;
}

void xray_jit_cellvalue_calculate_ndim(t_xray_jit_cellvalue *obj, long dimcount, long *dim, long planecount, 
		t_jit_matrix_info *in1_minfo, char *bip1, 
		t_jit_matrix_info *in2_minfo, char *bip2, 
		t_jit_matrix_info *out_minfo, char *bop)
{
	long i, j, k, width, height;
	long width2, height2;
	long in1planecount, in1colspan, in1rowspan;
	long in2planecount, in2rowspan;
	long outplanecount, outrowspan;
	float *fip1, *fip2, *fop;
	uchar *cip1, *cop;
	long *lip1, *lop;
	double *dip1, *dop;
	
	if (dimcount<1) return; //safety
	
	switch(dimcount) {
	case 1:
		dim[1]=1;
	case 2:
		width  = dim[0];
		height = dim[1];
		
		width2 = in2_minfo->dim[0];
		height2 = in2_minfo->dim[1];
		
		in1planecount = in1_minfo->planecount;
		in1colspan = in1_minfo->dimstride[0];
		in1rowspan = in1_minfo->dimstride[1];
		
		in2planecount = in2_minfo->planecount;
		in2rowspan = in2_minfo->dimstride[1];
		
		outplanecount = out_minfo->planecount;
		outrowspan = out_minfo->dimstride[1];
		
		if (out_minfo->type==_jit_sym_char) {
			for(i=0; i < height2; i++) {
				fip2 = (float *)(bip2 + i*in2rowspan);
				cop = (uchar *)(bop + i*outrowspan);
				
				for(j=0; j < width2; j++) {
					cip1 = (uchar *)(bip1 + (long)(fip2[0])*in1colspan + (long)(fip2[1])*in1rowspan);

					for(k=0; k < in1planecount; k++) {
						cop[k] = cip1[k];
					}
					
					fip2 += in2planecount;
					cop += outplanecount;
				}
			}
		}
		else if (out_minfo->type==_jit_sym_long) {
			for(i=0; i < height2; i++) {
				fip2 = (float *)(bip2 + i*in2rowspan);
				lop = (long *)(bop + i*outrowspan);
				
				for(j=0; j < width2; j++) {
					lip1 = (long *)(bip1 + (long)(fip2[0])*in1colspan + (long)(fip2[1])*in1rowspan);

					for(k=0; k < in1planecount; k++) {
						lop[k] = lip1[k];
					}
					
					fip2 += in2planecount;
					lop += outplanecount;
				}
			}
		}
		else if (out_minfo->type==_jit_sym_float32) {			
			for(i=0; i < height2; i++) {
				fip2 = (float *)(bip2 + i*in2rowspan);
				fop = (float *)(bop + i*outrowspan);
				
				for(j=0; j < width2; j++) {
					fip1 = (float *)(bip1 + (long)(fip2[0])*in1colspan + (long)(fip2[1])*in1rowspan);
					
					for(k=0; k < in1planecount; k++) {
						fop[k] = fip1[k];
					}
					
					fip2 += in2planecount;
					fop += outplanecount;
				}
			}
		}
		else if (out_minfo->type==_jit_sym_float64) {
			for(i=0; i < height2; i++) {
				fip2 = (float *)(bip2 + i*in2rowspan);
				dop = (double *)(bop + i*outrowspan);
				
				for(j=0; j < width2; j++) {
					dip1 = (double *)(bip1 + (long)(fip2[0])*in1colspan + (long)(fip2[1])*in1rowspan);

					for(k=0; k < in1planecount; k++) {
						dop[k] = dip1[k];
					}
					
					fip2 += in2planecount;
					dop += outplanecount;
				}
			}
		}
		break;
	default:
		;
	}
}

t_xray_jit_cellvalue *xray_jit_cellvalue_new(void)
{
	t_xray_jit_cellvalue *x;
		
	if (x=(t_xray_jit_cellvalue *)jit_object_alloc(_xray_jit_cellvalue_class)) {
		//nothing to do
	}
	else {
		x = NULL;
	}	
	return x;
}

void xray_jit_cellvalue_free(t_xray_jit_cellvalue *x)
{
	//nothing to do
}