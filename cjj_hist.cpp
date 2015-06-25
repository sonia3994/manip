/**********************************
**
**	Chris Jillings, August 8, 1995
**
**	see cjj_hist.h for comments
**
***********************************/
#include <stdio.h>
#include <math.h>
#include "doslib.h"

#ifndef HOME
#define HOME
#endif

#ifdef HOME
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#endif

#undef HOME

#ifndef TOM_NOT_HERE
#include "ioutilit.h"
#endif


#include "cjj.h"
#include "cjj_hist.h"


/**********************************/
int cjj_hist_new(struct cjj_hist** a,float x_low_in,
                              float x_high_in,int num_bins_in)
/**********************************/
{
	int i;
	int success_flag = 1;

	if (x_low_in >= x_high_in) success_flag = 0;

	(*a) = (struct cjj_hist*)malloc(sizeof(struct cjj_hist));
	if ((*a) == NULL) success_flag =0;

	if(success_flag==1)
	{
		(*a)->y = (float*)malloc(num_bins_in*sizeof(*((*a)->y)));
		(*a)->dy = (float*)malloc(num_bins_in*sizeof(*((*a)->dy)));
		(*a)->x = (float*)malloc(num_bins_in*sizeof(*((*a)->x)));
	}
	if (((*a)->y == NULL)||((*a)->dy == NULL)||((*a)->x==NULL)) success_flag = 0;
	


	if(success_flag ==1)
	{
		(*a)->label = NULL;
		(*a)->x_low = x_low_in;
		(*a)->x_high = x_high_in;
		(*a)->num_bins = num_bins_in;
		(*a)->over_range = 0.0;
		(*a)->under_range = 0.0;	
	
		(*a)->range = (*a)->x_high - (*a)->x_low;
		(*a)->bin_width = (*a)->range/((float)(*a)->num_bins);
		for(i=0;i<(*a)->num_bins;i++)
		{
			(*a)->y[i] = 0.0;
			(*a)->dy[i] = 0.0;
			(*a)->x[i] = x_low_in + (*a)->bin_width/2.0 + i*(*a)->bin_width;
		}
	}
	return success_flag;
}

/**********************************/
int cjj_hist_label(struct cjj_hist *a, char* lab)
/**********************************/
{
	int length;
	int success_flag = 1;

	length = strlen(lab) +1;
	a->label = (char *)malloc(length*sizeof(char));
	if (a->label == NULL) success_flag = 0;
	if(success_flag)
		strcpy(a->label,lab);
	return success_flag;
}

/**********************************/
void cjj_hist_inc(struct cjj_hist* a, float x)
/**********************************/
{
	int i;

	if (x < a->x_low) 
	{
		a->under_range += 1.0;
	}
	else
	{
		if (x > a->x_high)	
		{
			a->over_range += 1.0;
		}
		else
		{
			i = (int)floor((x - a->x_low)*(a->num_bins)/a->range);
			a->y[i] += 1.0;
		}
	}
}
	
/**********************************/
void cjj_hist_wipe(struct cjj_hist* a)
/**********************************/
{
	int i;
	
	for(i=0;i<a->num_bins;i++)
	{
		a->y[i] = 0.0;
		a->dy[i] = 0.0;
	}
}

/**********************************/
void cjj_hist_log(struct cjj_hist* a)
/**********************************/
{
	int i;
	
	for(i=0;i<a->num_bins;i++)
	{
		if(a->y[i] >0.0)
		{
			a->y[i] = log10(a->y[i]);
		}
		else
		{
			a->y[i] = 0.1;
		}
	}
}


/**********************************/
int cjj_hist_normal_err(struct cjj_hist* a)
/**********************************/
{
	int success_flag = 1;
	int i;
	for(i=0;i<a->num_bins;i++)	
	{
		if (a->y[i] >= 0.0)
		{
			a->dy[i] = sqrt(a->y[i]);
		}
		else
		{
			success_flag = 0;
		}
	}
  return success_flag;
}

/************************************/
int cjj_hist_add(struct cjj_hist* t,float c1, struct cjj_hist* a, 
                                    float c2, struct cjj_hist* b)
/************************************/
{
	int i;
	int success_flag = 1;
	
	/*	make sure a and b have the same number of bins and that t 
	  	is at least as long as a */
	if ((t==NULL) || (a->num_bins != b->num_bins)) success_flag = 0;
	if (t->num_bins < a->num_bins) success_flag = 0;

	if(success_flag == 1)
	{
		for(i=0;i<a->num_bins;i++)
		{
			t->y[i] = c1*a->y[i] + c2* b->y[i];
			t->dy[i] = sqrt((c1*a->dy[i])*(c1*a->dy[i]) + 
							(c2*b->dy[i])*(c2*b->dy[i]) );
		}
	}
	return success_flag;
}

/************************************/
void cjj_hist_to_file(struct cjj_hist* a, char* filename)
/************************************/
{
	FILE* out;
	int i;
	int success_flag = 1;
	
	if (filename==NULL)
		out = fopen("CJJ_HIST.TMP","w");
	else
		out = fopen(filename,"w");
	if (out ==NULL)
	{
		printw("Unable to open file in cjj_hist_to_file.\n");
		success_flag = 0;
	}

	if (success_flag)
		for(i = 0;i< a->num_bins;i++)
			fprintf(out,"%e  %e  %e \n",a->x[i],
				a->y[i],a->dy[i]);
	
	fclose(out);

}

/******************************************************/
void cjj_hist_to_data(struct cjj_hist*a, char* filename)
/******************************************************/
{
	int i;
	FILE* fout;
	if (filename == NULL) fout = opener("HIST.DAT","w");
	else fout = opener(filename,"w");
	for(i=0;i<a->num_bins;i++)
		fprintf(fout,"%e\t%e\t%e\n",a->x[i],a->y[i],a->dy[i]);
	fclose(fout);
}

/************************************/
void cjj_hist_to_file1(struct cjj_hist* a, char* filename)
/************************************/
{
	FILE* out;
	int i;
	int success_flag = 1;
	
	if (filename==NULL)
		out = fopen("CJJ_HIST.TMP","w");
	else
		out = fopen(filename,"w");
	if (out ==NULL)
	{
		printw("Unable to open file in cjj_hist_to_file1.\n");
		success_flag = 0;
	}

	if (success_flag) {
	    fprintf(out,"December96_format\n");
	    if (a->label == NULL) fprintf(out,"no label\n");
	    else {
		fprintf(out,"%s",a->label);
		fprintf(out,"\n");
	    }
	    fprintf(out,"%d\tnum_bins\n",a->num_bins);
	    fprintf(out,"%f\tx_low\n",a->x_low);
	    fprintf(out,"%f\tx_high\n",a->x_high);
	    fprintf(out,"%f\tunder range\n",a->under_range);
	    fprintf(out,"%f\tover range\n",a->over_range);
	    for(i=0;i<a->num_bins;i++){
		fprintf(out,"%e\t%e\t%e\n",a->x[i],a->y[i],a->dy[i]);
	    }
	fclose(out);

	}
}

/*******************************/
int cjj_hist_from_file1(struct cjj_hist **a,char* filename)
/*******************************/
{
    FILE* fin;
    int i;
    char label[500];
    int num_bins;
    float x_low;
    float x_high;
    float under_range;
    float over_range;
    char fileline[100];
    int success_flag = 1;
    int december96_format = 0;
    float temp;

    fin = fopen(filename,"r");
    if (fin==NULL) success_flag = 0;
    if(success_flag) {
	fgets(label,499,fin);
	if(strstr(label,"December96_format")!=NULL) {
	    fgets(label,499,fin);
	    december96_format = 1;
        }	
	fgets(fileline,99,fin);
	sscanf(fileline,"%d ",&num_bins);
	fgets(fileline,99,fin);
	sscanf(fileline,"%f ",&x_low);
	fgets(fileline,99,fin);
	sscanf(fileline,"%f ",&x_high);
	fgets(fileline,99,fin);
	sscanf(fileline,"%f ",&under_range);
	fgets(fileline,99,fin);
	sscanf(fileline,"%f ",&over_range);
	success_flag = cjj_hist_new(a,x_low,x_high,num_bins);
	
	if (success_flag) {
	    for(i = 0;i<num_bins;i++){
	    	fgets(fileline,99,fin);
		    if(december96_format)
                	sscanf(fileline,"%f %f %f",&temp,&((*a)->y[i]),&((*a)->dy[i]));
		    else
			sscanf(fileline,"%f %f",&((*a)->y[i]),&((*a)->dy[i]));
	    }
	}
    }
    fclose(fin);
	return success_flag;
}


/************************************/
void cjj_hist_clean(struct cjj_hist* a)
/************************************/
{
	free(a->y);
	free(a->dy);
	free(a->x);	
	free(a);
	a = NULL;		
}

/**********************************/
void cjj_hist_norm(struct cjj_hist* a)
/**********************************/
{
	int i;
	float tot = 0;

	for(i=0;i<a->num_bins;i++)
	{
		tot+=a->y[i];
	}
	if (tot >0.0)
		for(i=0;i<a->num_bins;i++)
		{
			a->y[i]/=tot;
		}
}

/*********************************************/
void cjj_hist_norm_spectrum(struct cjj_hist* a)
/*********************************************/
{
	int i;
	float tot = 0;

	for(i=0;i<a->num_bins;i++)
	{
		tot+=a->y[i];
	}
	tot *= a->bin_width;
	if(tot>0.0)
		for(i=0;i<a->num_bins;i++)
		{
			a->y[i]/=tot;
		}
	

}

/**********************************/
void cjj_hist_scale(struct cjj_hist* a,float scale_factor)
/**********************************/
{
	int i;

	for(i=0;i<a->num_bins;i++)
	{
		a->y[i]*=scale_factor;
		a->dy[i]*=scale_factor;
	}
}

/****************************************/
void cjj_hist_fill_bin(struct cjj_hist *a,int i,float y)
/****************************************/
{
	a->y[i] = y;
}

/****************************************/
float cjj_hist_get_bin(struct cjj_hist *a,int i)
/****************************************/
{
	return a->y[i];
}

/****************************************/
int cjj_hist_get_bin_index(struct cjj_hist *a, float x)
/* returns -1 if outside of range       */
/****************************************/
{
    int i=-1;
    if ((x > a->x_low) && (x < a->x_high))
	i = (int)floor( (x - a->x_low) * a->num_bins / a->range );
    return i;
}

/****************************************/
float cjj_hist_mean(struct cjj_hist *a)
/****************************************/
{
    float sum = 0.0;
    float entries = 0.0;
    float mean;
    int i;

    for(i = 0;i < a->num_bins; i++){
	sum += a->x[i]*a->y[i];
	entries += a->y[i];
    }
    if (entries <= 0.0) mean = 0.0;
    else mean = sum/entries;
    
    return mean;
}
/****************************************/
float cjj_hist_sigma(struct cjj_hist *a)
/****************************************/
{
    float sumsq = 0.0;
    float entries = 0.0;
    float mean;
    float sigma;
    int i;

    mean = cjj_hist_mean(a);  /* a is a pointer here */
    for(i = 0;i < a->num_bins; i++){
	sumsq += a->y[i] * (a->x[i] - mean) * (a->x[i] - mean);
	entries += a->y[i];
    }
    if (entries <= 0.0) sigma = 0.0;
    else sigma = sqrt(sumsq/(entries-1));
    
    return sigma;
}

/********************************************/
void cjj_hist_comb(struct cjj_hist *ans,struct cjj_hist *a,struct cjj_hist *b)
/* this more for integrating up neutrino spectra with cross-section data
** and a detector response than anything else
** No error checking: use at own risk
*********************************************/
{
    int i;
    for(i = 0;i < a->num_bins;i++){
	ans->y[i] = a->y[i]*b->y[i];
    }


}


/********************************************/
float cjj_hist_int(struct cjj_hist* a)
/* this is for integrating a real histo, it is NOT a
** a numerical integration of a set of equally spaced x,y
** points
*********************************************/
{
    int i;
    float value;

    value = 0.0;
    for(i = 0;i<a->num_bins;i++){
		value += a->y[i];
    }
    value *=a->bin_width;
    return value;
}

/********************************************/
float cjj_hist_sum_range(struct cjj_hist* a, float low, float high)
/* this is for integrating a real histo, it is NOT a
** a numerical integration of a set of equally spaced x,y
** points. The integration is between low and high inclusive
*********************************************/
{
    int i;
    int low_bin,high_bin;
    float value;
    int ok=1;

    value = 0.0;

    low_bin = cjj_hist_get_bin_index(a,low);
    if(low_bin<0) {
	if(low==a->x_low) low_bin = 0;
    	else ok = 0;
    }
    if (ok) {
      high_bin = cjj_hist_get_bin_index(a,high);
      if(high_bin<0) {
	if (high==a->x_high) high_bin = a->num_bins-1;
      	else ok = 0;
      }
    }
    if(ok) {
      for(i=low_bin;i<=high_bin;i++) value += a->y[i];
    }
    return value;
}


/********************************************/
float cjj_hist_int1(struct cjj_hist* a)
/* this is for integrating a spectrum, it is NOT a
** histogram.
*********************************************/
{
    int i;
    float value;

    value = 0.0;
    value = 0.5*(a->y[0]+a->y[a->num_bins]);
    for(i = 1;i<a->num_bins-1;i++){
		value += a->y[i];
    }
    value *=a->bin_width;
    return value;
}

/**********************************************************/
int cjj_hist_to_ford_fit(struct cjj_hist *h, float fxlow, float fxhigh,
                         char *filename, char type)
/* This takes a portion of a histogram and writes x y dy  */
/* to file for fitting in Richard's fitting code.         */
/* if fxlow = fxhigh = 0.0 then fit the entire thing      */
/* just takes into account a histogram ( x in the middle  */
/* of the bin) or a spectrum (x at the left edge of the   */
/* bin)                                                   */
/**********************************************************/
{
	int ok = 1;
	FILE *out;
	int low_bin;
	int high_bin;
	int i; /* looping */

	if((fxlow < h->x_low)||(fxlow> h->x_high)) ok = 0;
	if((fxhigh < h->x_low)||(fxhigh> h->x_high)) ok = 0;
	if(fxlow>fxhigh) ok = 0;
	if( (type!='s') && (type!='h') ) ok = 0;	
	
	if (filename == NULL) out = fopen("FORD_FIT.DAT","w");
	else fopen(filename,"w");
	if (out == NULL) ok = 0;

	low_bin = (int)floor((fxlow - h->x_low)*(h->num_bins)/h->range);
	high_bin = (int)floor((fxhigh - h->x_low)*(h->num_bins)/h->range);

	if (ok)	{
		for(i = low_bin;i<= high_bin;i++)	{
			if (type =='h')
  			  fprintf(out,"%f   %f   %f\n",(float)h->x[i],(float)h->y[i],(float)h->dy[i]);
			if (type =='s')
  			  fprintf(out,"%f   %f   %f\n",(float)(h->x[i]-h->bin_width/2.0),(float)h->y[i],(float)h->dy[i]);
		}
	}
	return ok;
}

#ifdef SQR
#undef SQR
#endif

#define SQR(x) ((x)*(x))

/***********************************************/
float cjj_hist_chisq(struct cjj_hist *exp,char et, struct cjj_hist *th, 
                      char tt, char type)
/* returns -1.0 if error occured               */
/* the spectra must have the same bin sizes,   */
/* ranges etc. If type is 'r' or 'R' the       */
/* reduced chi square is given.                */
/* et refers to the exp hist being a histo or  */
/* spectrum. tt refers to th hist as above     */
/***********************************************/
{
    float retvar = -1.0;
    int i = 0;
    int ok = 1;   /* my standard boolean */
    int bins_used = 0;
    float theory;
    float xx;
    int th_low_pt;
    int th_high_pt;

    if((toupper(tt) != 'H') && (toupper(tt) != 'S')) ok = 0;
    if((toupper(et) != 'H') && (toupper(et) != 'S')) ok = 0;

    i = 0;
    while ((ok)&&(retvar == -1.0)&&(i<=exp->num_bins)) {
	if (exp->dy[i] > 0.0) retvar = 0.0;
	i++;
    }
    if (retvar < 0.0) ok = 0;
    
    i = 0;
    while ( (i<exp->num_bins) &&(ok) ){
	if(toupper(et) == 'H') {
	    xx = exp->x[i];
	}
	else {
	    xx = exp->x[i] - exp->bin_width/2.0;
	}
	if(toupper(tt) == 'H') {
	    theory = th->y[cjj_hist_get_bin_index(th,xx)];
	}
	else {
	    th_low_pt = cjj_hist_get_bin_index(th,xx);
	    if (th_low_pt == th->num_bins) {
		ok = 0;                        /* shouldn't happen */ 
		break;
	    } 
	    th_high_pt = th_low_pt +1;
	    theory = th->y[th_low_pt] + (xx - th->x[th_low_pt]) *
		    (th->y[th_high_pt] - th->y[th_low_pt]); 
	    if(exp->dy[i] > 0.0) retvar += 
                  SQR(theory - exp->y[i]) / SQR(exp->dy[i]);
	}	    
	i++;
    }
    if(toupper(type) =='R') retvar /= exp->num_bins;
    if(!ok) retvar = -1.0;
    return retvar;

}
#undef SQR



/***********************************************/
void cjj_hist_conv_dr(struct cjj_hist *a,struct cjj_hist *b)
/* for spectra. Can be dangerous. Use with care */
/************************************************/
{
	int i,j,d;
	float sigma;
	float sisq; /* sigma * sigma */
	for(i = 1; i< a->num_bins;i++) {
		sigma = 1.4*sqrt(i/(10.0*a->bin_width));
		sisq = sigma*sigma;
		for(j = 0;j<b->num_bins;j++)	{
			d = i-j;
			b->y[j] += a->y[i]*exp(-d*d/(2*sisq))
					      /  (sqrt(2*PI)*sigma);
		}
	}				
}
	
/************************************************************/
float cjj_hist_gety(struct cjj_hist* h, float ix)
/************************************************************/
{
	int bin;
	if((ix-h->x_low)*(ix-h->x_high) <= 0.0) { /* valid range */
		bin = floor((ix-h->x_low)/h->bin_width);
		return h->y[bin];
	}
	else {
		return 1.0E-30;
	}
}
/***********************************************************************/
void cjj_hist_scale_x(struct cjj_hist *h, float zero_offset, float slope) 
/***********************************************************************/
{
	int i;
	h->range *= slope;
	h->bin_width *=slope;
	h->x_low = zero_offset + slope*(h->x_low);
	h->x_high = zero_offset + slope*(h->x_high);
	for(i=0;i<h->num_bins;i++) h->x[i] = h->x_low + (i+0.5)*(h->bin_width);

}

/*************************************************************************/
/*************************************************************************/
/* **		2D HISTOS					      ** */
/*************************************************************************/
/*************************************************************************/


/**********************************/
int cjj_2dhist_new(struct cjj_2dhist** a,float x_low_in,float x_high_in,
			int x_num_bins_in,
			float y_low_in,float y_high_in,
			int y_num_bins_in)
/**********************************/
{
	int i,j;
	int success_flag = 1;

	if (  (x_low_in >= x_high_in) || (x_low_in >= x_high_in) ) success_flag = 0;

	(*a) = (struct cjj_2dhist*)malloc(sizeof(struct cjj_2dhist));
	if ((*a) == NULL) success_flag =0;

	if(success_flag==1)
	{
		(*a)->z  = (float**)malloc(x_num_bins_in*sizeof(*((*a)->z )));
		(*a)->dz = (float**)malloc(x_num_bins_in*sizeof(*((*a)->dz)));
	}
	if (((*a)->z == NULL)||((*a)->dz == NULL)) success_flag = 0;
	
	if(success_flag ==1)
	{
		(*a)->z[0]  = (float*)malloc(x_num_bins_in*y_num_bins_in*sizeof(*((*a)->z[0] )));
		(*a)->dz[0] = (float*)malloc(x_num_bins_in*y_num_bins_in*sizeof(*((*a)->dz[0])));
		for(i = 1;i < x_num_bins_in;i++)
		{
			(*a)->z[i]  = (*a)->z[0]  + i*y_num_bins_in;
			(*a)->dz[i] = (*a)->dz[0] + i*y_num_bins_in;
		}
	}
	

	if(success_flag ==1)
	{
		(*a)->label = NULL;
		(*a)->x_low = x_low_in;
		(*a)->x_high = x_high_in;
		(*a)->num_bins_x = x_num_bins_in;
		(*a)->y_low = y_low_in;
		(*a)->y_high = y_high_in;
		(*a)->num_bins_y = y_num_bins_in;
		(*a)->out_of_range = 0.0;
	
		(*a)->range_x = (*a)->x_high - (*a)->x_low;
		(*a)->range_y = (*a)->y_high - (*a)->y_low;
		(*a)->bin_width_x = (*a)->range_x/((float)(*a)->num_bins_x);
		(*a)->bin_width_y = (*a)->range_y/((float)(*a)->num_bins_y);
	}

	

	if(success_flag ==1)
	{
		for(i=0;i<(*a)->num_bins_x;i++)
			for(j=0;j<(*a)->num_bins_y;j++)
			{
				(*a)->z[i][j] = 0.0;
				(*a)->dz[i][j] = 0.0;
			}
	}
	return success_flag;
}


/**********************************/
void cjj_2dhist_inc(struct cjj_2dhist* a, float x, float y)
/**********************************/
{
	int i,j;

	if ((x < a->x_low)||(x > a->x_high)||(y < a->y_low)||(y > a->y_high)) 
	{
		a->out_of_range ++;
	}
	else
	{
		i = (int)floor((x - a->x_low)*(a->num_bins_x)/a->range_x);
		j = (int)floor((y - a->y_low)*(a->num_bins_y)/a->range_y);
		a->z[i][j] += 1.0;
	}
}

/**********************************/
void cjj_2dhist_wipe(struct cjj_2dhist* a)
/**********************************/
{
	int i,j;
	
	for(i=0;i<a->num_bins_x;i++)
		for(j=0;j<a->num_bins_y;j++)
		{
			a->z[i][j] = 0.0;
			a->dz[i][j] = 0.0;
		}
}


/**********************************/
int cjj_2dhist_normal_err(struct cjj_2dhist* a)
/**********************************/
{
	int success_flag = 1;
	int i,j;
	for(i=0;i<a->num_bins_x;i++)	
		for(j=0;j<a->num_bins_y;j++)	
		{
			if (a->z[i][j] >= 0.0)
			{
				a->dz[i][j] = sqrt(a->z[i][j]);
			}
			else
			{
				success_flag = 0;
			}
		}
	return success_flag;
}

/************************************/
void cjj_2dhist_clean(struct cjj_2dhist* a)
/************************************/
{
	free(a->z);
	free(a->dz);
	free(a);
	a = NULL;		
}


/************************************/
int cjj_2dhist_add(struct cjj_2dhist* t,float c1, struct cjj_2dhist* a, 
                                    float c2, struct cjj_2dhist* b)
/************************************/
{
	int i,j;
	int success_flag = 1;
	
	/*	make sure a and b have the same number of bins and that t 
	  	is at least as long as a */
	if ((t==NULL) || (a->num_bins_x != b->num_bins_x) || (a->num_bins_y != b->num_bins_y)) 
		success_flag = 0;
	if (t->num_bins_x < a->num_bins_x) success_flag = 0;
	if (t->num_bins_y < a->num_bins_y) success_flag = 0;
	
	if(success_flag == 1)
	{
		for(i=0;i<a->num_bins_x;i++)
			for(j=0;j<a->num_bins_y;j++)
			{
				t->z[i][j] = c1*a->z[i][j] + c2* b->z[i][j];
				t->dz[i][j] = sqrt((c1*a->dz[i][j])*(c1*a->dz[i][j]) + 
						(c2*b->dz[i][j])*(c2*b->dz[i][j]) );
			}
	}
	return success_flag;
}

/************************************/
void cjj_2dhist_to_file1(struct cjj_2dhist* a, char* filename)
/************************************/
{
	FILE* out;
	int i,j;
	int success_flag = 1;
	
	if (filename==NULL)
		out = fopen("CJJ_2DHIST1.TMP","w");
	else
		out = fopen(filename,"w");
	if (out ==NULL)
	{
		printw("Unable to open file in cjj_hist_to_file.\n");
		success_flag = 0;
	}

	fprintf(out,"%d %d\n",a->num_bins_x,a->num_bins_y);
	fprintf(out,"%f %f %f %f\n",a->x_low,a->x_high,a->y_low,a->y_high);
	if (success_flag)
	{
		for(i = 0;i< a->num_bins_x;i++)
			for(j = 0;j< a->num_bins_y;j++)
				fprintf(out,"%f\n",a->z[i][j]);
	}
	fclose(out);

}

/************************************/
int cjj_2dhist_from_file1(struct cjj_2dhist** a, char* filename)
/************************************/
{
	void cjj_2dhist_fill_bin(struct cjj_2dhist *a,int i, int j, float z);
	FILE* fin;
	char fileline[100];
	int num_x, num_y;
	float low_x,low_y,high_x,high_y;
//	int check;
	int success_flag = 1;
	int i,j;
	float z;

	fin = fopen(filename,"r");
	

	if(fin == NULL) success_flag = 0;
	if(success_flag)
		if(fgets(fileline,99,fin) == NULL) success_flag = 0;

	if(success_flag)
	{
		sscanf(fileline,"%d %d",&num_x,&num_y);
		fgets(fileline,99,fin);
		sscanf(fileline,"%f %f %f %f",&low_x,&high_x,&low_y,&high_y);
		success_flag = cjj_2dhist_new(a,low_x,high_x,num_x,low_y,high_y,num_y);
	}
	if(success_flag)
	{
		for(i=0;i<num_x;i++)
			for(j=0;j<num_y;j++)
			{
				fgets(fileline,99,fin);
				z = atof(fileline);
				cjj_2dhist_fill_bin(*a,i,j,z);	
			}

	}	

	
	return(success_flag);


}

/*************************************/
int cjj_2dhist_coarse_rebin(struct cjj_2dhist* a,struct cjj_2dhist **b,int fac_x,int fac_y)
/*************************************/
/* b is created and is a version of a with coarser bins */
/* The numbins in each must be integer multiples of each other */
{
	void cjj_2dhist_fill_bin(struct cjj_2dhist *a,int i, int j, float z);
	float cjj_2dhist_get_bin(struct cjj_2dhist *a,int i,int j);
	int num_x,num_y;
	float low_x,high_x,low_y,high_y;
	int i,j,k,l;
	float total;

	int success_flag=1;

	if( (a->num_bins_x%fac_x != 0) || (a->num_bins_y%fac_y != 0) ) success_flag =0;
	
	if(success_flag)
	{
		num_x = a->num_bins_x/fac_x;
		num_y = a->num_bins_y/fac_y;
		low_x = a->x_low;
		high_x = a->x_high;
		low_y = a->y_low;
		high_y = a->y_high;
		success_flag = cjj_2dhist_new(b,low_x,high_x,num_x,low_y,high_y,num_y);
	}
	if(success_flag)
	{
		for(i=0;i<num_x;i++)
			for(j=0;j<num_y;j++)
			{
				total = 0.0;				
				for(k=0;k<fac_x;k++)
					for(l=0;l<fac_y;l++)
					{
						total += cjj_2dhist_get_bin(a,k+fac_x*i,l+j*fac_y);
					}			
				cjj_2dhist_fill_bin(*b,i,j,total);
			}
	}
	(*b)->out_of_range = a->out_of_range;
	return success_flag;
}

/************************************/
void cjj_2dhist_to_file(struct cjj_2dhist* a, char* filename)
/************************************/
{
	FILE* out;
	int i,j;
	int success_flag = 1;
	
	if (filename==NULL)
		out = fopen("CJJ_2DHIST.TMP","w");
	else
		out = fopen(filename,"w");
	if (out ==NULL)
	{
		printw("Unable to open file in cjj_hist_to_file.\n");
		success_flag = 0;
	}

	if (success_flag)
	{
		for(i = 0;i< a->num_bins_x;i++)
			{
			for(j = 0;j< a->num_bins_y;j++)
				fprintf(out,"%f ",a->z[i][j]);
			fprintf(out,"\n");
			}
	}
	fclose(out);
}



/**********************************/
void cjj_2dhist_norm(struct cjj_2dhist* a)
/**********************************/
{
	int i,j;
	float tot = 0.0;

	for(i=0;i<a->num_bins_x;i++)
		for(j=0;j<a->num_bins_y;j++)
		{
			tot += a->z[i][j];
		}
	for(i=0;i<a->num_bins_x;i++)
		for(j=0;j<a->num_bins_y;j++)
		{
			a->z[i][j]/=tot;
			a->dz[i][j]/=tot;
		}
}



/**********************************/
void cjj_2dhist_polar(struct cjj_2dhist* a,char unit)
/**********************************/
{
        int i,j;
        float theta;
	float stheta; 

        if ((toupper(unit) != 'R')&&(toupper(unit) != 'D'))     {
                printw("Warning: cjj_2dhist_norm_polar does not know what ");
		printw("unit to use.\n");
                printw("Assuming degrees.\n");
	        unit = 'd';  /* this change won't last past the end of the function. */
        }
 	
        for(i=0;i<a->num_bins_x;i++) {
                theta = a->x_low + a->bin_width_x *(i+0.5);
                if (toupper(unit) == 'D') {
			printw("Using degrees\t");
			theta *= TO_RAD;
			printw("theta = %f\n",theta);
		}
		stheta = sin(theta);
                for(j=0;j<a->num_bins_y;j++)
                {
                        a->z[i][j]*=stheta;
                }
        }

}
  








/**********************************/
void cjj_2dhist_scale(struct cjj_2dhist* a,float scale_factor)
/**********************************/
{
	int i,j;

	for(i=0;i<a->num_bins_x;i++)
		for(j=0;j<a->num_bins_y;j++)
		{
			a->z[i][j]*=scale_factor;
			a->dz[i][j]*=scale_factor;
		}
}

/****************************************/
void cjj_2dhist_fill_bin(struct cjj_2dhist *a,int i, int j, float z)
/****************************************/
{
	a->z[i][j] = z;
}

/****************************************/
float cjj_2dhist_get_bin(struct cjj_2dhist *a,int i,int j)
/****************************************/
{
	return a->z[i][j];
}

/******************************************************/
int cjj_2dhist_get_x_index(struct cjj_2dhist *a,float x)
/******************************************************/
{
	int i;
	i = (int)floor( (x-a->x_low) * a->num_bins_x/(a->x_high - a->x_low) );
	return i;
}

/******************************************************/
int cjj_2dhist_get_y_index(struct cjj_2dhist *a,float y)
/******************************************************/
{
	int j;
	j = (int)floor( (y-a->y_low) * a->num_bins_y/(a->y_high - a->y_low) );
	return j;
}

/****************************************/
int cjj_2dhist_projx(struct cjj_2dhist *twod, struct cjj_hist **oned)
/****************************************/
/* sums over y and projects onto x */
{
	int i,j; /*loop control */
	int success_flag;
	float total;
	
	success_flag = cjj_hist_new(oned,twod->x_low,twod->x_high,twod->num_bins_x);
	if(success_flag)
	{
		for(i = 0;i<twod->num_bins_x;i++)
		{
			total = 0.0;
			for(j = 0;j<twod->num_bins_y;j++)
			{
				total += cjj_2dhist_get_bin(twod,i,j);
			}
			cjj_hist_fill_bin((*oned),i,total);
		}
	}	
	return success_flag;
}
/**********************************/
int cjj_2dhist_label(struct cjj_2dhist *a, char* lab)
/**********************************/
{
	int length;
	int success_flag = 1;

	length = strlen(lab) +1;
	a->label = (char *)malloc(length*sizeof(char));
	if (a->label == NULL) success_flag = 0;
	if(success_flag)
		strcpy(a->label,lab);
	return success_flag;
}
/**********************************/
void cjj_2dhist_log(struct cjj_2dhist* a)
/**********************************/
{
	int i,j;
	
	for(i=0;i<a->num_bins_x;i++) 
		for(j = 0;j<a->num_bins_y;j++)
		{
			if(a->z[i][j] >0.0)
			{	
				a->z[i][j] = log10(a->z[i][j]);
			}
			else
			{
				a->z[i][j] = 0.1;
			}
		}
}

/****************************************/
int cjj_2dhist_projxtoy(struct cjj_2dhist *a, struct cjj_hist **pro)
/****************************************/
{
	int i; /* x index */
	int j; /* y index */
	int ok; /* Boolean checking */

	ok = cjj_hist_new(pro,a->y_low,a->y_high,a->num_bins_y);
	if(ok)	{
		for(j=0;j<a->num_bins_y;j++)	{
			for(i = 0;i<a->num_bins_x;i++)	{
				(*pro)->y[j] += a->z[i][j];
			}
		}
	}
	return ok;		
}

/***************************************/
int cjj_2dhist_projytox(struct cjj_2dhist *a, struct cjj_hist **pro)
/***************************************/
{
	int i;
	int j;
	int ok;
	
	ok = cjj_hist_new(pro,a->x_low,a->x_high,a->num_bins_x);
	if (ok)	{
		for(i=0;i<a->num_bins_x;i++)	{
			for(j=0;j<a->num_bins_y;j++)	{
				(*pro)->y[i] += a->z[i][j];
			}
		}
	}
	return ok;
}


/****************************************************************/
void cjj_2dhist_part(struct cjj_2dhist** d, struct cjj_2dhist* s,
                      float xl, float xh, float yl, float yh)
/****************************************************************/
{
	
	int ok=1;
	int i,j;
	int nxl,nxh,nyl,nyh;
	int nx,ny;
	float mid_bin;

	/* Do a bunch of error checking to make sure everything "fits" */
	if(xl < s->x_low || xh > s->x_high) ok = 0;
	if(yl < s->y_low || yh > s->y_high) ok = 0;
	
	if (ok) {
		/* make sure that xl etc are at the left (right) edge of bins */
		nxl = cjj_2dhist_get_x_index(s,xl);
		nxh = cjj_2dhist_get_x_index(s,xh);
		nx = nxh-nxl; /* correct ! The high bin is one too high ! */
	
		nyl = cjj_2dhist_get_y_index(s,yl);
		nyh = cjj_2dhist_get_y_index(s,yh);
		ny = nyh-nyl; /* correct ! The high bin is one too high ! */
	
		cjj_2dhist_new(d,xl,xh,nx,yl,yh,ny);
	 	if(s->bin_width_x != (*d)->bin_width_x) ok = 0;
	 	if(s->bin_width_y != (*d)->bin_width_y) ok = 0;
	}
	if (!ok) {
		printw("Problem in cjj_2dhist_part()...\n");
		outofhere();
	}
	for(i=nxl;i<nxh;i++) for(j=nyl;j<nyh;j++) {
		(*d)->z[i][j] = s->z[i][j];
		(*d)->dz[i][j] = s->dz[i][j];
	}
		
}







