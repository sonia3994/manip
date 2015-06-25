#include "random.h"
#include "hwutil.h"		// for quit()
#include "display.h"

float randm(int* idum)          /* RNG hook for C code          */
{
#ifdef PORTABLE_RNG
  return(ran2(idum));
#else
  return(ran1(idum));
#endif
}

#ifndef PORTABLE_RNG
float ran1(int* idum)
{
        static unsigned short xseed[3];

        if (*idum < 0)
        {
          xseed[0] = (0x0000ffff & *idum);
          xseed[1] = (0xffff0000 & *idum)>>16;
          xseed[2] = (0x00ffff00 & *idum)>>8;
          *idum = 1;
        }

        return(((float) erand48(xseed)));

}

#else

#define M1 259200L
#define IA1 7141L
#define IC1 54773L
#define RM1 (1.0/M1)
#define M2 134456L
#define IA2 8121L
#define IC2 28411L
#define RM2 (1.0/M2)
#define M3 243000L
#define IA3 4561L
#define IC3 51349L

float ran2(int *idum)
{
				static long ix1,ix2,ix3;
				static float r[98];
				float temp;
				static int iff=0;
				int j;
				if (*idum < 0 || iff == 0) {
								iff=1;
								ix1=(IC1-(*idum)) % M1;
								ix1=(IA1*ix1+IC1) % M1;
								ix2=ix1 % M2;
								ix1=(IA1*ix1+IC1) % M1;
								ix3=ix1 % M3;
								for (j=1;j<=97;j++) {
												ix1=(IA1*ix1+IC1) % M1;
												ix2=(IA2*ix2+IC2) % M2;
												r[j]= (float) ((ix1+ix2*RM2)*RM1);
								}
								*idum=1;
				}
				ix1=(IA1*ix1+IC1) % M1;
				ix2=(IA2*ix2+IC2) % M2;
				ix3=(IA3*ix3+IC3) % M3;
				j = (int) (1 + ((97*ix3)/M3));
				if (j > 97 || j < 1)
				{
					qprintf("RAN2: This cannot happen.\n");
					qprintf("j = %d *idum = %d\n",j,*idum);
					quit();
				}
				temp=r[j];
				r[j] = (float) ((ix1+ix2*RM2)*RM1);
				return temp;
}
#undef M1
#undef IA1
#undef IC1
#undef RM1
#undef M2
#undef IA2
#undef IC2
#undef RM2
#undef M3
#undef IA3
#undef IC3

#endif
