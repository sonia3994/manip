#ifndef __CJJ_HIST_H
#define __CJJ_HIST_H

struct cjj_hist {

	/* variables you should care about */

	float *y;	/* the values if each bin */
	float *dy;  /* the error in y */
	float *x;  /* the x-value of the center of each bin */
	float x_low;  /* the low and high x values respectively */
	float x_high;
	float under_range; /* the number of entries below the x-range */
	float over_range; /* the number of counts above the range in x */
	int num_bins;  /* the number of bins */
	char* label;
	/* variables to ignore */

	float bin_width;  /* used internally: user is not to alter */
	float range;      /* used internally: user is not to alter */
};


 int cjj_hist_new(struct cjj_hist** a,
			float x_low,float x_high,int num_bins);
 void cjj_hist_inc(struct cjj_hist* a, float x);
 void cjj_hist_wipe(struct cjj_hist* a);
 int cjj_hist_normal_err(struct cjj_hist* a);
 void cjj_hist_plot(struct cjj_hist* a);
 void cjj_hist_plot_erry(struct cjj_hist* a);
 void cjj_hist_clean(struct cjj_hist* a);
 int cjj_hist_add(struct cjj_hist *t,float c1,struct cjj_hist* a, float c2, struct cjj_hist* b);
 void cjj_hist_to_file(struct cjj_hist *a,char *filename);
 void cjj_hist_norm(struct cjj_hist *a);
 void cjj_hist_scale(struct cjj_hist *a, float  scale_factor);
 void cjj_hist_fill_bin(struct cjj_hist *a,int i,float y);
 float cjj_hist_get_bin(struct cjj_hist *a,int i);
 int cjj_hist_label(struct cjj_hist *a,char *label);
 void cjj_hist_log(struct cjj_hist *a);
 float cjj_hist_mean(struct cjj_hist *a);
 float cjj_hist_sigma(struct cjj_hist *a);
void cjj_hist_to_data(struct cjj_hist*a, char* filename);
 void cjj_hist_to_file1(struct cjj_hist *a,char* filename);
 int cjj_hist_from_file1(struct cjj_hist **a,char* filename);
 void cjj_hist_comb(struct cjj_hist *ans,struct cjj_hist *a,struct cjj_hist *b);
 float cjj_hist_int(struct cjj_hist *a);
 float cjj_hist_sum_range(struct cjj_hist *a,float low,float high);
 float cjj_hist_int1(struct cjj_hist *a);
 void cjj_hist_conv_dr(struct cjj_hist *a,struct cjj_hist *b);
 int cjj_hist_to_ford_fit(struct cjj_hist *h, float fxlow,
																float fxhigh,char *filename, char type);
 int cjj_hist_get_bin_index(struct cjj_hist *a, float x);
 float cjj_hist_chisq(struct cjj_hist *exp, char et,
														 struct cjj_hist *th, char tt, char type);
float cjj_hist_gety(struct cjj_hist *h, float x);
void cjj_hist_scale_x(struct cjj_hist* a, float zero_offset, float slope);


struct cjj_2dhist {

	/* variable to care about */
	float **z; 	/* the value of each bin */
	float **dz;	/* the error on each bin */
	float x_low;  /* the low and high x values respectively */
				float x_high;
				float y_low;  /* the low and high y values respectively */
				float y_high;
	float out_of_range; /*	the number of counts out of range in x or y */
	int num_bins_x;
	int num_bins_y;
	char * label;

	/* variable to ignore */
				float bin_width_x;  /* used internally: user is not to alter */
				float range_x;      /* used internally: user is not to alter */
				float bin_width_y;  /* used internally: user is not to alter */
				float range_y;      /* used internally: user is not to alter */

};

 int cjj_2dhist_new(struct cjj_2dhist** a,
			float x_low,float x_high, int num_bins_x,
			float y_low,float y_high, int num_bins_y);
 void cjj_2dhist_inc(struct cjj_2dhist* a, float x, float y);
 void cjj_2dhist_wipe(struct cjj_2dhist* a);
 int cjj_2dhist_normal_err(struct cjj_2dhist* a);
 void cjj_2dhist_clean(struct cjj_2dhist* a);
 int cjj_2dhist_add(struct cjj_2dhist *t,float c1,struct cjj_2dhist* a, float c2, struct cjj_2dhist* b);
 void cjj_2dhist_to_file(struct cjj_2dhist *a,char *filename);
 void cjj_2dhist_to_file1(struct cjj_2dhist *a,char *filename);
 void cjj_2dhist_norm(struct cjj_2dhist *a);
 void cjj_2dhist_scale(struct cjj_2dhist *a, float  scale_factor);
 void cjj_2dhist_fill_bin(struct cjj_2dhist *a,int i, int j,float z);
 float cjj_2dhist_get_bin(struct cjj_2dhist *a,int i, int j);
 int cjj_2dhist_from_file1(struct cjj_2dhist** a, char* filename);
 int cjj_2dhist_coarse_rebin(struct cjj_2dhist* old, struct cjj_2dhist **newh, int fac_x, int fac_y);
 int cjj_hist_get_x_index(struct cjj_2dhist *a, float x);
 int cjj_hist_get_y_index(struct cjj_2dhist *a, float y);
 int cjj_2dhist_projxtoy(struct cjj_2dhist* twod,struct cjj_hist **oned);
 int cjj_2dhist_projytox(struct cjj_2dhist* twod,struct cjj_hist **oned);
 int cjj_2dhist_label(struct cjj_2dhist *a,char *label);
 void cjj_2dhist_log(struct cjj_2dhist *a);
 void cjj_2dhist_polar(struct cjj_2dhist* a, char unit);
 void cjj_2dhist_part(struct cjj_2dhist** d, struct cjj_2dhist* s,
											float xl, float xh, float yl, float yh);

#endif
