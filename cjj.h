#ifndef __CJJ_H
#define __CJJ_H

#include <stdio.h>
#include <math.h>

#ifndef HOME
#define HOME
#endif

#ifdef HOME
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#endif

#undef HOME



#define PI 3.14159265358979323846264338328
#define TWOPI 6.28318530717958647692528677
#define TO_RAD 0.017453292521
#define TO_DEG 57.29577951
#define X 0  	/* for handling 3-vecs */
#define Y 1
#define Z 2



void outofhere(void);
int frange(float *low, float *high, char *variable);
int drange(double *low, double *high, char *variable);
//FILE* opener(char* filename, char* status);
void string_from_user(char *string, int string_length, char *message);
int int_from_user(char* message);
double double_from_user(char* message);
float float_from_user(char* message);
char char_from_user(char* message);
void init_float_array(int num_elements, float *a);
void init_double_array(int num_elements, double *a);
void init_int_array(int num_elements, int *a);
float** allocate_2d_float(int i, int j);
float dot_prod(float *a, float *b);
void cross_prod(float* a,float* b,float* answer);
void three_vec_norm(float *a);
void eulerCosine(float * u, float *phi, float *theta, float *psi);
void imatdef(float ieuler[3][3],float phi, float theta, float psi);
void mmult(float matrix[3][3], float * u);
void matdef(float euler[3][3],float phi, float theta, float psi);
int eigen_symm(float **a, int n, float* lambda, float **v);
void filename_append(char* infilename, int length, char* suffix,
                     char* outfilename);

#endif



