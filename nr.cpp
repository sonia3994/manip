#include "nr.h"
#include "nrutil.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "hwutil.h"		// for quit()
#include "doslib.h"

#define SWAP(a,b) {swap=(a); (a)=(b); (b)=swap;}
void gaussj(float **a, int n, float **b, int m)
{
				int i,icol,irow,j,k,l,ll;
				float big,dum,pivinv;
				float swap;

				int* indxc;
				indxc = ivector(1,n);
				int* indxr;
				indxr = ivector(1,n);
				int* ipiv;
				ipiv = ivector(1,n);
				for (j=1;j<=n;j++) ipiv[j]=0;
				for (i=1;i<=n;i++) {
					big=0.0;
					for (j=1;j<=n;j++)
						if (ipiv[j] != 1)
							for (k=1;k<=n;k++) {
								if (ipiv[k] == 0) {
									if (fabs(a[j][k]) >= big) {
										big=fabs(a[j][k]);
										irow=j;
										icol=k;
									}
								 } else if (ipiv[k] > 1) {
													printw("Something bad just happened in gaussj().\n");
													quit();
												}

							}
					++(ipiv[icol]);

					if (irow != icol) {
						for (l=1;l<=n;l++) SWAP(a[irow][l],a[icol][l])
						for (l=1;l<=m;l++) SWAP(b[irow][l],b[icol][l])
					}
					indxr[i]=irow;
					indxc[i]=icol;
					if (a[icol][icol] == 0.0) {
						printw("Pivot error. Don't trust a thing...\n");
						return;
					}
					pivinv=1.0/a[icol][icol];
					a[icol][icol]=1.0;
					for (l=1;l<=n;l++) a[icol][l] *= pivinv;
					for (l=1;l<=m;l++) b[icol][l] *= pivinv;
					for (ll=1;ll<=n;ll++)
					if (ll != icol) {
						dum=a[ll][icol];
						a[ll][icol]=0.0;
						for (l=1;l<=n;l++) a[ll][l] -= a[icol][l]*dum;
						for (l=1;l<=m;l++) b[ll][l] -= b[icol][l]*dum;
					}
				}
				for (l=n;l>=1;l--) {
								if (indxr[l] != indxc[l])
												for (k=1;k<=n;k++)
																SWAP(a[k][indxr[l]],a[k][indxc[l]]);
				}
				free_ivector(ipiv,1,n);
				free_ivector(indxr,1,n);
				free_ivector(indxc,1,n);


}
#undef SWAP

#define SWAP(a,b) {swap=(a);(a)=(b);(b)=swap;}

void covsrt(float **covar, int ma, int ia[], int mfit)
{
	int i,j,k;
	float swap;

	for (i=mfit+1;i<=ma;i++)
		for (j=1;j<=i;j++) covar[i][j]=covar[j][i]=0.0;
	k=mfit;
	for (j=ma;j>=1;j--) {
		if (ia[j]) {
			for (i=1;i<=ma;i++) SWAP(covar[i][k],covar[i][j])
			for (i=1;i<=ma;i++) SWAP(covar[k][i],covar[j][i])
			k--;
		}
	}
}
#undef SWAP
