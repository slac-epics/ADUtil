#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#include <epicsExport.h>
#include <epicsTypes.h>

#define NR_END 1
#define FREE_ARG char*

#define MA 3
#define SPREAD 0.01
#define ASIZE  3072

int                     DEBUG_GFIT =0;
epicsExportAddress(int, DEBUG_GFIT);

void nrerror(char error_text[])
{
        if(!DEBUG_GFIT) return;

        printf("Numerical Recipes run-time error...\n");
        printf("%s\n",error_text);
}


#define SWAP(a,b) {swap=(a);(a)=(b);(b)=swap;}

static void covsrt(float covar[][MA+1], int ma, int ia[], int mfit)
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


#define SWAP(a,b) {temp=(a);(a)=(b);(b)=temp;}

static int gaussj(float a[][MA+1], int n, float b[][1+1], int m)
{
	int indxc[MA+1],indxr[MA+1],ipiv[MA+1];
	int i,icol,irow,j,k,l,ll;
	float big,dum,pivinv,temp;

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
					} else if (ipiv[k] > 1) { nrerror("gaussj: Singular Matrix-1"); return -1; }
				}
		++(ipiv[icol]);
		if (irow != icol) {
			for (l=1;l<=n;l++) SWAP(a[irow][l],a[icol][l])
			for (l=1;l<=m;l++) SWAP(b[irow][l],b[icol][l])
		}
		indxr[i]=irow;
		indxc[i]=icol;
		if (a[icol][icol] == 0.0) { nrerror("gaussj: Singular Matrix-2"); return -1; }
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

        return 0;
}
#undef SWAP


static void fgauss(float x, float a[], float *y, float dyda[], int na)
{
	int i;
	float fac,ex,arg;

	*y=0.0;
	for (i=1;i<=na-1;i+=3) {
		arg=(x-a[i+1])/a[i+2];
		ex=exp(-arg*arg);
		fac=a[i]*ex*2.0*arg;
		*y += a[i]*ex;
		dyda[i]=ex;
		dyda[i+1]=fac/a[i+2];
		dyda[i+2]=fac*arg/a[i+2];
	}
}


static void mrqcof(float x[], float y[], float sig[], int ndata, float a[], int ia[],
	int ma, float alpha[][MA+1], float beta[], float *chisq,
	void (*funcs)(float, float [], float *, float [], int))
{
	int i,j,k,l,m,mfit=0;
	float ymod,wt,sig2i,dy,dyda[MA+1];

	for (j=1;j<=ma;j++)
		if (ia[j]) mfit++;
	for (j=1;j<=mfit;j++) {
		for (k=1;k<=j;k++) alpha[j][k]=0.0;
		beta[j]=0.0;
	}
	*chisq=0.0;
	for (i=1;i<=ndata;i++) {
		(*funcs)(x[i],a,&ymod,dyda,ma);
		sig2i=1.0/(sig[i]*sig[i]);
		dy=y[i]-ymod;
		for (j=0,l=1;l<=ma;l++) {
			if (ia[l]) {
				wt=dyda[l]*sig2i;
				for (j++,k=0,m=1;m<=l;m++)
					if (ia[m]) alpha[j][++k] += wt*dyda[m];
				beta[j] += dy*wt;
			}
		}
		*chisq += dy*dy*sig2i;
	}
	for (j=2;j<=mfit;j++)
		for (k=1;k<j;k++) alpha[k][j]=alpha[j][k];
}

static  int mrqmin(float x[], float y[], float sig[], int ndata, float a[], int ia[],
	int ma, float covar[][MA+1], float alpha[][MA+1], float *chisq,
	void (*funcs)(float, float [], float *, float [], int), float *alamda)
{
	void covsrt(float covar[][MA+1], int ma, int ia[], int mfit);
	int  gaussj(float a[][MA+1], int n, float b[][1+1], int m);
	void mrqcof(float x[], float y[], float sig[], int ndata, float a[],
		int ia[], int ma, float alpha[][MA+1], float beta[], float *chisq,
		void (*funcs)(float, float [], float *, float [], int));
	int j,k,l,m;
	int mfit;
	float ochisq,atry[MA+1], beta[MA+1],da[MA+1],oneda[MA+1][1+1];

	if (*alamda < 0.0) {
		for (mfit=0,j=1;j<=ma;j++)
			if (ia[j]) mfit++;
		*alamda=0.001;
		mrqcof(x,y,sig,ndata,a,ia,ma,alpha,beta,chisq,funcs);
		ochisq=(*chisq);
		for (j=1;j<=ma;j++) atry[j]=a[j];
	}
	for (j=0,l=1;l<=ma;l++) {
		if (ia[l]) {
			for (j++,k=0,m=1;m<=ma;m++) {
				if (ia[m]) {
					k++;
					covar[j][k]=alpha[j][k];
				}
			}
			covar[j][j]=alpha[j][j]*(1.0+(*alamda));
			oneda[j][1]=beta[j];
		}
	}
	if(gaussj(covar,mfit,oneda,1)) return -1;
	for (j=1;j<=mfit;j++) da[j]=oneda[j][1];
	if (*alamda == 0.0) {
		covsrt(covar,ma,ia,mfit);
		return 0;
	}
	for (j=0,l=1;l<=ma;l++)
		if (ia[l]) atry[l]=a[l]+da[++j];
	mrqcof(x,y,sig,ndata,atry,ia,ma,covar,da,chisq,funcs);
	if (*chisq < ochisq) {
		*alamda *= 0.1;
		ochisq=(*chisq);
		for (j=0,l=1;l<=ma;l++) {
			if (ia[l]) {
				for (j++,k=0,m=1;m<=ma;m++) {
					if (ia[m]) {
						k++;
						alpha[j][k]=covar[j][k];
					}
				}
				beta[j]=da[j];
				a[l]=atry[l];
			}
		}
	} else {
		*alamda *= 10.0;
		*chisq=ochisq;
	}


        return 0;
}



int  gfit(int n, double *v, double *fv, double *amplitude, double *position, double *width, double centroid, double sigma)
{
        int i,ia[MA+1],iter,itst,j,k,mfit=MA;
        float alamda,chisq,ochisq,x[ASIZE+1],y[ASIZE+1],sig[ASIZE+1],covar[MA+1][MA+1],alpha[MA+1][MA+1];
        float a[MA+1];

        float max = -1.E+6;


        for (i=1;i<=n;i++) {
                x[i] = i;
                y[i] = v[i-1];
                sig[i]=SPREAD*0.1;
                if(max<y[i]) max = y[i];
        }

        a[1] = max;
        a[2] = centroid + 1.;
        a[3] = sigma;

        for (i=1;i<=mfit;i++) ia[i]=1;
        for (iter=1;iter<=1;iter++) {
                alamda = -1;
                if(mrqmin(x,y,sig,n,a,ia,MA,covar,alpha,&chisq,fgauss,&alamda)) return -1;
                k=1;
                itst=0;
                for (;;) {
                        k++;
                        ochisq=chisq;
                        if(mrqmin(x,y,sig,n,a,ia,MA,covar,alpha,&chisq,fgauss,&alamda)) return -1;
                        if (chisq > ochisq)
                                itst=0;
                        else if (fabs(ochisq-chisq) < 0.1)
                                itst++;
                        if (itst < 4) continue;
                        alamda=0.0;
                        if(mrqmin(x,y,sig,n,a,ia,MA,covar,alpha,&chisq,fgauss,&alamda)) return -1;
                        break;
                }
        }

        for(i=0;i<n;i++) {
            double arg = (x[i+1] - a[2])/ a[3];
            fv[i] = a[1] * exp (-arg *arg);
        }

        *amplitude = (double) a[1];
        *position  = (double) a[2]-1.;
        *width     = (double) a[3];

        return 0;

}

