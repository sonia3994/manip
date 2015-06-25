#ifndef __CJJ_RAN_H
#define __CJJ_RAN_H

extern float throw_exp(float mean); 
extern float cjj_ran(long* cjj_iseed);
extern int cjj_hist_ran(struct cjj_hist* source,struct cjj_hist* dest,
                 int number, char type);
extern float fgauss(float sigma);
extern float get_mott_angle(float cutoff, float beta);
extern float mott_mean_length(float cutoff,float beta,float gamma);

#endif
