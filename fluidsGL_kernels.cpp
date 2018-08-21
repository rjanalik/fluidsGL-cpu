/*
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fftw3.h>
#include "defines.h"
#include <math.h>
#include <complex>


// OpenGL Graphics includes
#define HELPERGL_EXTERN_GL_FUNC_IMPLEMENTATION


// FluidsGL CUDA kernel definitions
#include "fluidsGL_kernels.h"


// Texture pitch
extern size_t tPitch;
cData *vxfield = NULL;
cData *vyfield = NULL;


// These are the external function calls necessary for launching fluid simulation
	extern "C"
void addForces(cData *v, int dx, int dy, int spx, int spy, float fx, float fy, int r)
{
	
	int tx, ty;
 	
	for (int y = 0; y < 2*r+1; ++y){
                for (int x = 0; x < 2*r+1; ++x){
                                 
                                cData* fj = &v[(y+spy)*dx + x + spx];
                                cData vterm = *fj;
                                
                                tx = x - r;
                                ty = y - r;
                                float s = 1.f / (1.f + tx*tx*tx*tx + ty*ty*ty*ty);
                                vterm.x += s * fx;
                                vterm.y += s * fy;
                                
                                *fj = vterm;
                                
                }
        }

}

	extern "C"
void advectVelocity(cData *v, float *vx, float *vy, int dx, int pdx, int dy, float dt)
{
	
	cData vterm, ploc ;
	float vxterm, vyterm;
	
	
	for (int y = 0; y < dy; ++y){
		for (int x = 0; x < dx; ++x){
			int fj = y * dx + x;
			vterm = v[fj];
			 
			ploc.x = x  - dt * vterm.x * dx;
                	ploc.y = y  - dt * vterm.y * dy;
			
			int posxl, posxu, posyl, posyu;
			posxl = int(ploc.x);
			posxu = posxl + 1;
			posyl = int(ploc.y);
			posyu = posyl + 1;
			posxl = posxl % dx;
			posxu = posxu % dx;
			posyl = posyl % dy;
			posyu = posyu % dy;

			if(posxl < 0) {
				posxl += dx;
			}

			if(posxu < 0) {
            			 posxu += dx;
			}

            		if(posyl < 0) {
            			posyl += dy ;
			}

			if(posyu < 0) {
				posyu += dy;
			}
			
			cData p11, p12, p21, p22;
			p11 = v[posyl * dx + posxl];
			p12 = v[posyl * dx + posxu];
			p21 = v[posyu * dx + posxl];
			p22 = v[posyu * dx + posxu];
			
			//calculate weights

			float fx = ploc.x - int(ploc.x);
			float fy = ploc.y - int(ploc.y);
			float fx1 = 1.0 - fx;
			float fy1 = 1.0 - fy;
			
			float w1 = fx1 * fy1;
 			float w2 = fx  * fy1;
 			float w3 = fx1 * fy;
 			float w4 = fx  * fy;
			
			// update velocity with weighted neighbours
			fj = y * pdx + x;
			vx[fj] = w4*p11.x + w3 * p12.x + w2 * p21.x + w1 * p22.x ;
			vy[fj] = w4*p11.y + w3 * p12.y + w2 * p21.y + w1 * p22.y ;
		}
	}
}

	extern "C"
void diffuseProject(cData *vx, cData *vy, int dx, int dy, float dt, float visc)
{
	
	fftwf_plan plan;

	plan = fftwf_plan_dft_r2c_2d(DIM, DIM, (float *) vx, (fftwf_complex *) vx, 0);
	fftwf_execute(plan);
	fftwf_destroy_plan(plan);
	plan = fftwf_plan_dft_r2c_2d(DIM, DIM, (float*) vy,  (fftwf_complex *) vy, 0);
	fftwf_execute(plan);
	fftwf_destroy_plan(plan);

		
	cData xterm, yterm;

	
	for (int y = 0; y < dy; ++y){
		for (int x = 0; x < dx; ++x){
			int fj = y * dx + x;
			xterm = vx[fj];
			yterm = vy[fj];
			
			// Compute the index of the wavenumber based on the
			// data order produced by a standard NN FFT.

			int iix = x;
			int iiy = (y>dy/2)?(y-(dy)):y;
			
			// Velocity diffusion
			float kk = (float)(iix * iix + iiy * iiy); // k^2
			float diff = 1.f / (1.f + visc * dt * kk);
			xterm.x *= diff;
			xterm.y *= diff;
			yterm.x *= diff;
			yterm.y *= diff;

				// Velocity projection
			if (kk > 0.f){
				float rkk = 1.f / kk;
				// Real portion of velocity projection
				float rkp = (iix * xterm.x + iiy * yterm.x);
				// Imaginary portion of velocity projection
				float ikp = (iix * xterm.y + iiy * yterm.y);
				xterm.x -= rkk * rkp * iix;
				xterm.y -= rkk * ikp * iix;
				yterm.x -= rkk * rkp * iiy;
				yterm.y -= rkk * ikp * iiy;
			}

			vx[fj] = xterm;
			vy[fj] = yterm;
		}
	}

	plan = fftwf_plan_dft_c2r_2d(DIM, DIM, (float (*)[2]) vx, (float*) vx, 0);
	fftwf_execute(plan);
	fftwf_destroy_plan(plan);
	plan = fftwf_plan_dft_c2r_2d(DIM, DIM, (float (*)[2]) vy, (float*) vy, 0);
	fftwf_execute(plan);
	fftwf_destroy_plan(plan);

}


	extern "C"
void updateVelocity(cData *v, float *vx, float *vy, int dx, int pdx, int dy)
{

	float vxterm, vyterm;
	cData nvterm;
	
	for (int y = 0; y < dy; ++y){
		for (int x = 0; x < dx; ++x){
			int fjr = y * pdx + x;
			vxterm = vx[fjr];
			vyterm = vy[fjr];
			
			// Normalize the result of the inverse FFT
			float scale = 1.f / (dx * dy);
			nvterm.x = vxterm * scale;
			nvterm.y = vyterm * scale;

			cData* fj = &v[ y*dx + x];
			*fj = nvterm;

	

				
		}
	}

}

	extern "C"
void advectParticles(cData *ptcl , cData *v, int dx, int dy, float dt)
{

	cData pterm, vterm;

	for (int y = 0; y < dy; ++y){
		for (int x = 0; x < dx; ++x){
			
			int fj = y * dx + x;
			pterm = ptcl[fj];
			
			int xvi = ((int)(pterm.x * dx));
			int yvi = ((int)(pterm.y * dy));
			
			int vi = yvi*dx + xvi;
			vterm = v[vi];


			pterm.x += dt * vterm.x;
			pterm.x = pterm.x - (int)pterm.x;
			pterm.x += 1.f;
			pterm.x = pterm.x - (int)pterm.x;
			pterm.y += dt * vterm.y;
			pterm.y = pterm.y - (int)pterm.y;
			pterm.y += 1.f;
			pterm.y = pterm.y - (int)pterm.y;

			ptcl[fj] = pterm;
			
		}

	}
}

