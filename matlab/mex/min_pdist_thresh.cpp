#include <mex.h>

#include <cmath>
#include <vector>
#include <iostream>
#include <limits>

// #include "mex_eigen_utils.hpp"
#include "StringBuilder.h"

void mexPrintString(const std::string &s)
{
    mexPrintf(s.c_str());
}

#define MEX_PRINT_STRING(x) mexPrintString(UTIL_STR(x));
#define DEBUG_SHOW_TAB(x) MEX_PRINT_STRING(#x << " = " << (x) << "\t");
#define DEBUG_SHOW(x) MEX_PRINT_STRING(#x << " = " << (x) << "\n");

// X : nx x d
// Y : ny x d
// [mindists, minidxs] = min_pdist_thresh(X, Y, dthresh)
//   minidxs: ny x 1
//   mindists: ny x 1

// compute squared euclidean distance
double eucldist2(double *px, double *py, int dims, int deltapx, int deltapy)
{
    int d;
    double dist2 = 0.;
    
    for(d = 0; d < dims; ++d) {
        dist2 += (*px - *py) * (*px - *py);
        px += deltapx;
        py += deltapy;
    }
    
    return dist2;
}

double eucldist2(double *px, double *py, int dims, int deltapx, int deltapy, double bestdist2)
{
    int d;
    double dist2 = 0.;
    
    for(d = 0; (d < dims) && (dist2 < bestdist2); ++d) {
        dist2 += (*px - *py) * (*px - *py);
        px += deltapx;
        py += deltapy;
    }
    
    return dist2;
}

double cityblockdist2(double *px, double *py, int dims, int deltapx, int deltapy)
{
    int d;
    double dist = 0.;
    
    for(d = 0; d < dims; ++d) {
        dist += std::abs(*px - *py);
        px += deltapx;
        py += deltapy;
    }
    
    return dist * dist;
}

bool getMexScalar(const mxArray *mxarr, double &out)
{
    size_t mrows, ncols;
    mrows = mxGetM(mxarr);
    ncols = mxGetN(mxarr);

    if (!mxIsDouble(mxarr) || mxIsComplex(mxarr) || !(mrows == 1 && ncols == 1)) {
        return false;
    }

    out = *mxGetPr(mxarr);
    return true;
}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    // declare variables
    const mxArray *mX, *mY;
    const mxArray *mThresh;
    mxArray *mMinDists, *mMinIdxs;
            
    const mwSize *dimX, *dimY;
    int nx, ny, D;
    double *pX, *pY, *pMinDists, *pMinIdxs, *pWidths;
    
    // check for proper number of arguments
    if (nrhs != 3) { 
        mexErrMsgTxt("Three input argument required."); 
    }  
    if (nlhs != 2) {
        mexErrMsgTxt("Two outputs required."); 
    } 

    // setup inputs
    mX = prhs[0]; mY = prhs[1];
    dimX = mxGetDimensions(mX);
    dimY = mxGetDimensions(mY);
    
    nx = (int)dimX[0];
    ny = (int)dimY[0];
    D = (int)dimX[1];
    
    if (D != dimY[1]) {
        mexErrMsgTxt("Different number of arguments"); 
    }

    pX = mxGetPr(mX);
    pY = mxGetPr(mY);
    
    mThresh = prhs[2];
    size_t nthresh = mxGetNumberOfElements(mThresh);
    if ((nthresh != 1) && (nthresh != nx)) {
        mexErrMsgTxt("Threshold must be a scalar, or must have a value for each row in X"); 
    }
    
    // get squared distance threshold for all X
    std::vector<double> dthresh2(nx);
    if (nthresh == 1)
    {
        double dthresh; // distance threshold
        if (!getMexScalar(mThresh, dthresh)) {
            mexErrMsgTxt("Unknown error loading scalar threshold value"); 
        }
        
        dthresh = dthresh * dthresh; // squared distance
        for(size_t xj = 0; xj < nx; xj++) {
            dthresh2[xj] = dthresh;
        }
    }
    else
    {
        // nthresh == nx
        double * pThresh = mxGetPr(mThresh);
        for(size_t xj = 0; xj < nx; xj++, pThresh++) {
            dthresh2[xj] = (*pThresh) * (*pThresh); // squared distance
        }        
    }
    
    // setup outputs
    mMinIdxs = plhs[0] = mxCreateDoubleMatrix(ny, 1, mxREAL);
    mMinDists = plhs[1] = mxCreateDoubleMatrix(ny, 1, mxREAL);
    
    pMinDists = mxGetPr(mMinDists);
    pMinIdxs = mxGetPr(mMinIdxs);
    
    // compute pair-wise distances between all x,
    //  such that we can later prune test points
    std::vector<std::vector<double>> interdist(nx);
    for(int xj = 0; xj < nx; xj++) {
        interdist[xj].resize(nx);
        interdist[xj][xj] = 0.;
    }
    for(int xj = 0; xj < nx; xj++)
    {
        for(int xi = xj+1; xi < nx; xi++)
        {
            double dist2 = eucldist2(pX+xj, pX+xi, D, nx, nx);
            interdist[xj][xi] = dist2;
            interdist[xi][xj] = interdist[xj][xi];
        }
    }
    
    // find closest x for all y
    int offset = 0;
    //int nskip = 0, nnoskip = 0; // DEBUG statistics
    for(int yj = 0; yj < ny; yj++)
    {
        int bestxj = std::numeric_limits<int>::max();
        double bestdist2 = std::numeric_limits<int>::max();

        for(int xj2 = 0; xj2 < nx; xj2++)
        {
            // start xj search at 'offset' index, since
            //  that was the best xj for the previous
            //  yj (i.e. assume local smoothness)
            int xj = (xj2 + offset) % nx;
            
            // test if we can prune this xj : if its distance to the best
            // xj is larger than twice the distance of test point yj
            // to the best xj, then we know yj cannot be closer to this xj.
            if (bestxj < std::numeric_limits<int>::max())
            {
                double testdist = interdist[bestxj][xj];                
                if (testdist > (4*bestdist2)) {
                    //nskip++;
                    continue;
                }

                //nnoskip++;
            }
            
            double this_dthreshj = dthresh2[xj];
                    
            double dist2 = eucldist2(pX+xj, pY+yj, D, nx, ny, bestdist2);
            if (dist2 > this_dthreshj)
            {
                // should be within threshold
                continue;
            }
                
            if (dist2 < bestdist2)
            {
                bestdist2 = dist2;
                bestxj = xj;
            }            
        }
         
        if (bestxj < std::numeric_limits<int>::max()) {
            *pMinDists = std::sqrt(bestdist2);
            *pMinIdxs = bestxj+1;
            offset = bestxj;
        } else {
            *pMinDists = std::numeric_limits<double>::quiet_NaN();
            *pMinIdxs = std::numeric_limits<double>::quiet_NaN();
        }
        pMinDists++;
        pMinIdxs++;
    }
    //DEBUG_SHOW_TAB(nskip);
    //DEBUG_SHOW(nnoskip);
}
