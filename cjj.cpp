#include "cjj.h"
#include "doslib.h"

void outofhere(void)
/************************************/
/* Normal exit message              */
/************************************/
{
	printw("\n ===== Do widzenia ===== \n\n");
	exit(-1);
}

#define STR_LN_CJ_HERW 100
int drange(double *low, double *high, char *variable)
/************************************************/
/* Prompts user for low/high values of a range  */
/* Returns 1 for high >low; 0 for high = low;   */
/* and -1 for high < low                        */
/************************************************/
{
	char instring[STR_LN_CJ_HERW];
	int return_variable;

	printw("Please enter the low value of %s.",variable);
	fgets(instring,STR_LN_CJ_HERW,stdin);	 
	*low = atof(instring);
	printw("\nPlease enter the high value of %s.",variable);
	fgets(instring,STR_LN_CJ_HERW,stdin);	 
	*high = atof(instring);
	printw("\n");
	
	if ((*high)>(*low)) return_variable = 1;
	else 	
		if ((*high) == (*low)) return_variable = 0;
		else return_variable = -1;

	return return_variable;
}  

int frange(float *low, float *high, char *variable)
/************************************************/
/* Prompts user for low/high values of a range  */
/* Returns 1 for high >low; 0 for high = low;   */
/* and -1 for high < low                        */
/************************************************/
{
	char instring[STR_LN_CJ_HERW];
	int return_variable;

	printw("Please enter the low value of %s.",variable);
	fgets(instring,STR_LN_CJ_HERW,stdin);	 
	*low = (float)(atof(instring));
	printw("\nPlease enter the high value of %s.",variable);
	fgets(instring,STR_LN_CJ_HERW,stdin);	 
	*high = (float)(atof(instring));
	printw("\n");
	
	if ((*high)>(*low)) return_variable = 1;
	else 	
		if ((*high) == (*low)) return_variable = 0;
		else return_variable = -1;

	return return_variable;
}  

#ifdef TOM_NOT_HERE
/*************************************/
FILE* opener(char* fname, char* status)
/*************************************/
/* Tom R¹s file opening routine      */
{
		 FILE* fptr;

		 if (NULL == (fptr = fopen(fname, status)))
		 {
			 printw("\n\n\n COULD NOT OPEN FILE: %s \n\n\n",fname);
			 exit(-1);
		 }

		 return(fptr);

}
#endif
/**************************************/
void string_from_user(char *string, int string_length, char *message)
/**************************************/
/* Chris Jillings -- June 1995 */
/* Prompts the user for a string */
{
	int good = 0;
	char* dummy;
	int i;
	char  tmp;

	dummy = (char*)malloc((string_length+1)*sizeof(char));
	if (dummy == NULL)
	{
		fprintf(stderr,"\nError in cjj library, string_from_user\n");
		string[0] = '\0';
		good = 1;
	}

	while (!good)
	{
		fflush(stdin);
		printw("%s\n",message);
		fgets(dummy,string_length,stdin);
		fflush(stdin);
		printw("You have entered %sOK(y/n)?\n",dummy);
		tmp = getc(stdin);
		if (toupper(tmp) == 'Y') 
		{	
			i = 0;
			while((dummy[i] != '\n') && (i<string_length))	i++;  /* Run through char string to \n*/
			if (i==string_length) 
			{
				printw("String is too long. It must have fewer ");
				printw("than %d characters.\n",string_length);
			}
			else
			{
				dummy[i] = '\0'; /* replace newline with null terminator */
				string[0] = '\0';  /* to make sure than after copying string */
								   /* is exactly dummy */
				strcpy(string,dummy);
				good = 1;
			}
		}
	}
}

#define DUMMY_STR_LN_ASXZ 100
/***************************************/
float float_from_user(char* message)
/***************************************/
/* Chris Jillings -- June 1995 */
/* prompts user for a float */
{
	char dummy_string[DUMMY_STR_LN_ASXZ]; /* used to get inputs from the user */
	float retvar;
	int good = 0;
	while (!good)
	{
		printw("\n%s\n",message);
		fflush(stdin);
		(void)fgets(dummy_string,DUMMY_STR_LN_ASXZ,stdin);
		retvar = (float)(atof((const char*)dummy_string));
		fflush(stdin);
		printw("You have entered %f. OK(y/n)?",retvar);
		if( toupper(getchar()) == 'Y' ) good = 1;
	}
	return retvar;
}

/***************************************/
double double_from_user(char* message)
/***************************************/
/* Chris Jillings -- June 1995 */
/* prompts user for a double */
{
	char dummy_string[DUMMY_STR_LN_ASXZ]; /* used to get inputs from the user */
	double retvar;
	int good = 0;
	while (!good)
	{
		printw("\n%s\n",message);
		fflush(stdin);
		fgets(dummy_string,DUMMY_STR_LN_ASXZ,stdin);
		retvar = atof((const char*)dummy_string);
		fflush(stdin);
		printw("You have entered %lf. OK(y/n)?",retvar);
		if( toupper(getchar()) == 'Y' ) good = 1;
	}
	return retvar;
}

/***************************************/
int int_from_user(char* message)
/***************************************/
/* Chris Jillings -- June 1995 */
{
	char dummy_string[DUMMY_STR_LN_ASXZ]; /* used to get inputs from the user */
	int retvar;
	int good = 0;
	while (!good)
	{
		printw("\n%s\n",message);
		fflush(stdin);
		fgets(dummy_string,DUMMY_STR_LN_ASXZ,stdin);
		retvar = atoi((const char*)dummy_string);
		fflush(stdin);
		printw("You have entered %d. OK(y/n)?",retvar);
		if( toupper(getchar()) == 'Y' ) good = 1;
	}
	return retvar;
}

/************************************/
char char_from_user(char* message)
/************************************/
/* Chris Jillings -- June 1995 */
{
	char retvar;

	printw("\n%s\n",message);
	fflush(stdin);
	retvar = getchar();
	fflush(stdin);
	return retvar;
}

/***************************************/
void init_float_array(int num_elements, float *a)
/***************************************/
/* Chris Jillings, June, 1995 */
{
	int i;

	for(i=0;i<num_elements;i++) a[i] = 0.0;
}


/***************************************/
void init_double_array(int num_elements, double *a)
/***************************************/
/* Chris Jillings, June, 1995 */
{
	int i;

	for(i=0;i<num_elements;i++) a[i] = 0.0;
}

/***************************************/
void init_int_array(int num_elements, int *a)
/***************************************/
/* Chris Jillings, June, 1995 */
{
	int i;

	for(i=0;i<num_elements;i++) a[i] = 0.0;
}

/***************************************/
float** allocate_2d_float(int i,int j)
/***************************************/
{
	int k;
	float **a;
	
	a = (float**)malloc( i*sizeof(float*) );
	if(a == NULL) {
		return NULL;
	}
	for(k = 0;k < i; k++) 	{
		a[k] = (float*)malloc(j*sizeof(float*));
		if (a[k] == NULL)	 {
			a = NULL;
			return NULL;
		}
	}
  return a;
}

/****************************************/
float dot_prod(float *a, float *b)
/****************************************/
/*	Chris Jillings, July, 1995 */
/*
**	Does a three-vector dot product   */
{
	return (a[X]*b[X] + a[Y]*b[Y] + a[Z]*b[Z]);
}

/****************************************/
void cross_prod(float* a,float* b,float* answer)
/*****************************************/
/*	Chris Jillings, July, 1995 */
/*
**	Does a three-vector cross product   */
/*	answer = a x b						*/
{
	answer[X] = a[Y]*b[Z] - b[Y]*a[Z];
	answer[Y] = a[Z]*b[X] - b[Z]*a[X];
	answer[Z] = a[X]*b[Y] - b[X]*a[Y];
}

/*****************************************/
void three_vec_norm(float *a)
/*****************************************/
/*	Chris Jillings July 1995
**
**	Norms a three vec to unity				*/
/*	A zero vector is returned unchanged     */
{
	float length;
	length = sqrt(a[X]*a[X] + a[Y]*a[Y] + a[Z]*a[Z]);
	if (length != 0.0)
	{
		*a /=length;
		*(a+1) /=length;
		*(a+2) /=length;
	}
}
	


/********************************************************/
/*                                                      */
/*      Converts direction cosines into Euler angles    */
/*      that will either transform u into the z-axis    */
/*      or transform the z-axis into u.                 */
/*                                                      */
/*      imatdef generates matrix for u -> z-axis        */
/*       matdef generates matrix for z-axis -> u        */
/*                                                      */
/*                              -- written 93-10-23 tjr */
/********************************************************/
void eulerCosine(float * u, float *phi, float *theta, float *psi)
{
        float   ephi,etheta,epsi;

        if (fabs(u[Z]) == 1.f)
        {
          etheta = 0.f;
          ephi   = 0.f;
        }
        else 
        {
          etheta = acos(u[Z]);           /*  find Euler angles */
          ephi   = -57.29577951*atan2(u[X]/sin(etheta),u[Y]/sin(etheta));
          etheta = -57.29577951*etheta;
        }
        epsi = 0.f;   /*  never rotate about new z-axis */

        *phi   = ephi;
        *theta = etheta;
        *psi   = epsi;

}

/********************************************************/
/*                                                      */
/*      Defines inverse rotation matrix.  See Goldstein,*/
/*      p. 128 ff.                                      */
/*                                                      */
/*                -- modified from FORTRAN 92-07-30 tjr */
/*                                                      */
/*      Modified November, 1995				*/
/*	Input angles in degrees			        */
/*                -- CJJ				*/
/********************************************************/
void imatdef(float ieuler[3][3],float phi, float theta, float psi)
{
        float cphi,sphi;
        float ctheta,stheta;
        float cpsi,spsi;

        cphi   = (float) cos(TO_RAD*phi); 
        sphi   = (float) sin(TO_RAD*phi); 
        ctheta = (float) cos(TO_RAD*theta); 
        stheta = (float) sin(TO_RAD*theta); 
        cpsi   = (float) cos(TO_RAD*psi); 
        spsi   = (float) sin(TO_RAD*psi); 

        ieuler[0][0] = cphi*cpsi - ctheta*sphi*spsi;       
        ieuler[0][1] = cpsi*sphi + cphi*ctheta*spsi; 
        ieuler[0][2] = spsi*stheta; 

        ieuler[1][0] = -cpsi*ctheta*sphi - cphi*spsi;       
        ieuler[1][1] = cphi*cpsi*ctheta - sphi*spsi; 
        ieuler[1][2] = cpsi*stheta; 

        ieuler[2][0] = sphi*stheta;
        ieuler[2][1] = -cphi*stheta; 
        ieuler[2][2] = ctheta;

} 
/********************************************************/
/*                                                      */
/*      Multiplies a 3X3 matrix by a column vector.     */
/*                                                      */
/*                -- modified from FORTRAN 92-07-30 tjr */
/*                                                      */
/********************************************************/
void mmult(float matrix[3][3], float * u)
{
        int i,j;                /*  loop counters */
        float  vector[3];    /*  dummy variable */

        for( i = 0; i < 3; i++)
        { 
          vector[i]=0.0F;

          for( j = 0; j < 3; j++)
          { 
            vector[i]=vector[i]+matrix[i][j]*u[j];

          }

        }  


        for( i = 0; i < 3; i++) u[i] = vector[i]; 

}

/********************************************************/
/*                                                      */
/*      Defines rotation matrix.  See Goldstein,        */
/*      p. 128 ff.                                      */
/*                                                      */
/*                -- modified from FORTRAN 92-07-30 tjr */
/*      Input angles now in degrees, CJJ, Nov 1995      */
/********************************************************/
void matdef(float euler[3][3],float phi, float theta, float psi)
{
        float cphi,sphi;
        float ctheta,stheta;
        float cpsi,spsi;

        cphi   = (float) cos(TO_RAD*phi); 
        sphi   = (float) sin(TO_RAD*phi); 
        ctheta = (float) cos(TO_RAD*theta); 
        stheta = (float) sin(TO_RAD*theta); 
        cpsi   = (float) cos(TO_RAD*psi); 
        spsi   = (float) sin(TO_RAD*psi); 


        euler[0][0] = cphi*cpsi-ctheta*spsi*sphi;       
        euler[0][1] = -cphi*spsi-ctheta*cpsi*sphi; 
        euler[0][2] = sphi*stheta; 

        euler[1][0] = sphi*cpsi+ctheta*spsi*cphi;       
        euler[1][1] = -spsi*sphi+ctheta*cphi*cpsi; 
        euler[1][2] = -cphi*stheta; 

        euler[2][0] = stheta*spsi;
        euler[2][1] = stheta*cpsi; 
        euler[2][2] = ctheta;

} 



#define MAX_JACOBI_ITS 20
#define ROTATE_EIG(a,i,j,k,l) {g=a[i][j];h=a[k][l];a[i][j]=g-s*(h+g*tau);\
						  a[k][l]=h+s*(g-h*tau);}
int eigen_symm(float **a, int n, float* lambda, float **v)
/*******************************************************************/
/*  Calculates Eigenvalues/vectors using the Jacobi rotations method
**  Chris Jillings, July, 1995
********************************************************************/
/*
**	a is an n x n SYMMETRIC, REAL matrix
**	If a is not symmetric and real, the routine returns -1.
**	If the routine cannot diagonalize the matrix after
**	MAX_JACOBI_ITS (set at 50 if you don't change it) the 
**	routine value is -2. If the routine returns successfully,
**	the return value is the number of iterations taken to diagonalize
**	the matrix. If the matrix is already diagonal, the routine
**  returns 0.
**
**	Please note, the values of a above the diagonal are changed 
**	by the routine, but the values on and below are not.
**
**	The routine is based on NR in C. I have not used unit offset 
**	arrays as NR in C does. I have changes a few variable names.
**	See section 11.1 and the routine jacobi for reference
**
********************************************************************/
/*
**	ARGS:
**		a[][]:		the  real, symmetric matrix
**		n:			the dimension
**		lambda[]:	the eigenvalues
**		v[][]:		the matrix whose columns are normed
**					eigenvectors
**
********************************************************************/
{
	int i;	/* Loop control */
	int j;	/* Loop control */
	int ip; /* The index of one of the dimensions in the 2d rot. */
	int iq;	/* The other dimension in the 2d rotation */
	int num_its = 0; /* the number of iterations made */

	float s;     /* the sine of the rotation angle */
	float c;     /* the cosine of the rotation angle */
	float t;     /* the tan of the rot angle */
	float tau;   /* s/(1+c)  eq 11.1.18 in NR in C */  
	float theta; /* (c*c - s*s)/(2sc) eq 11.1.8 in NR in C */
	
	float thresh;  /* the min size to cause the routine to rotate */
	float sum_off_d; /* the sum of the mags of off-diag terms of a */
	float h;     /* temp storage */
	float g;     /* temp storage */
	
	float *b;
	float *z;    /* t*a_{pq} in eq 11.1.14 in NR in C */

	int done=0;  /* A boolean to exit the main loop */

	/***************************/
	/* check for symmetry of a */
	for (i=0;i<n;i++)
		for(j=0;j<n;j++)
			if( a[i][j] != a[j][i])
			{
				done = 1;
				num_its = -1;
			}
	/***************************/

	if (done == 0)
	{
		/* Create space for b,z and initialize b,z,d,v */				
		b = (float *)malloc(n*sizeof(*b));
		z = (float *)malloc(n*sizeof(*z));
		for(i=0;i<n;i++)
		{
			for(j=0;j<n;j++)
			{
				v[i][j] = 0.0;
			}	
			v[i][i] = 1.0;
			b[i] = a[i][i];
			lambda[i] = a[i][i];
			z[i] = 0.0;
		}
	}
	/* Main Loop */
	while(done == 0)
	{
		/* calc the sum of off diag terms. If zero, return */
		sum_off_d = 0.0;
		for (i=0;i< n-1 ;i++)	
		{
			for(j=i+1;j<n;j++)	
			{
				sum_off_d += (a[i][j]);
			}
		}
		if (sum_off_d == 0.0)
		{
			free(z);
			free(b);
			done = 1;
			break;  /* get out of the loop */
		}
		
		/* set the thresh for rotations */
		if(num_its < 3)
		{
			thresh = 0.2*sum_off_d/(n*n);
		}
		else
		{
			thresh = 0.0;
		}
		
		/* LOOP over ip, iq */
		for(ip = 0;ip < n-1; ip++)
		{
			for(iq = ip+1;iq < n; iq++)
			{
				g = 100.0 * abs((int)a[ip][iq]);
				if( (num_its>3) &&
				    ( (float)(abs((int)lambda[ip])+g) == (float)abs((int)lambda[ip]) ) &&
				    ( (float)(abs((int)lambda[iq])+g) == (float)abs((int)lambda[iq]) )    )
				{
					a[ip][iq] = 0.0;
				} 
				else
				{ /* A */
					if(abs((int)a[ip][iq]) > thresh)
					{ /* B */
						h = lambda[iq] - lambda[ip];
						/* Check for large value of theta */
						if( (float)(abs((int)h) + g) == (float)abs((int)h) )					
						{
							t = a[ip][iq]/h;	/* t = 1/(2*theta) */
						}
						else  /* small  value of theta */
						{
							theta = 0.5*h/(a[ip][iq]);		/* eq 11.1.10 */
							t = 1.0/(abs((int)theta) + sqrt(1.0 + theta*theta));					
						}
						/* Get rotation angles */
						c = 1.0/sqrt(1+t*t);
						s = t*c;
						tau = s/(1.0+c);

						h = t*a[ip][iq];
						z[ip] -= h;
						z[iq] += h;
						lambda[ip] -= h;
						lambda[iq] += h;
						a[ip][iq] = 0.0;
						
						for(j=0;j < ip-1; j++)  
						{
							ROTATE_EIG(a,j,ip,j,iq) /* MACRO: see def above*/
						}							/* routine */
						for(j=ip;j < iq-1; j++) 
						{
							ROTATE_EIG(a,j,ip,j,iq)
						}
						for(j=iq;j < n; j++)    
						{
							ROTATE_EIG(a,j,ip,j,iq)
						}
						for(j=0;j < n; j++)
					    {
					    	ROTATE_EIG(a,j,ip,j,iq)
					    }
					} /* B */
				} /* A */
			} /* iq loop */	
		} /* ip loop */
		
		for(ip = 0;ip<n;ip++)		
		{
			b[ip] += z[ip];
			lambda[ip] = b[ip];
			z[ip] = 0.0;
		}
		
		num_its++;
		if(num_its > MAX_JACOBI_ITS)	{
			num_its = -2;
			done = 1;
		}
	}/* end while loop */
	
	return num_its;

}	

void filename_append(char* infilename, int length, char* suffix, 
						char* outfilename)
{
	int i;

        for (i=0;i<length;i++)
        {
		outfilename[i] = infilename[i];
                if(infilename[i] == '\0')
                {
                        outfilename[i]   = '.' ;
                        outfilename[i+1] = suffix[0] ;
                        outfilename[i+2] = suffix[1] ;
                        outfilename[i+3] = suffix[2] ;
                        outfilename[i+4] = '\0';
                        break;
                }
        }
}
