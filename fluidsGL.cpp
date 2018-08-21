// Includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <chrono>

#if defined(__APPLE__) || defined(MACOSX)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <GLUT/glut.h>
//#include <freeglut.h>
//#include <glew.h>
#ifndef glutCloseFunc
#define glutCloseFunc glutWMCloseFunc
#endif
#else
#include <GL/freeglut.h>
#endif

#include "defines.h"
#include "fluidsGL_kernels.h"

#include <algorithm>

const char *sSDKname = "fluidsGL";

void cleanup(void);
void reshape(int x, int y);

static cData *vxfield = NULL;
static cData *vyfield = NULL;

cData *hvfield = NULL;

static int wWidth  = std::max(512, DIM);
static int wHeight = std::max(512, DIM);


static int clicked  = 0;
static int fpsCount = 0;
static int fpsLimit = 1;
static std::chrono::high_resolution_clock::time_point t1 ;
static std::chrono::high_resolution_clock::time_point t2;
static std::chrono::duration<double> time_span = std::chrono::milliseconds::zero();

GLuint vbo = 0; 
// Particle data
cData* buffer = 0;                 // OpenGL vertex buffer object
static cData *particles = NULL; // particle positions in host memory
static int lastx = 0, lasty = 0;

// Texture pitch
size_t tPitch = 0; // Now this is compatible with gcc in 64-bit

char *ref_file         = NULL;
bool g_bQAAddTestForce = true;
int  g_iFrameToCompare = 100;
int  g_TotalErrors     = 0;

bool g_bExitESC = false;


void autoTest(char **);

extern "C" void addForces(cData *v, int dx, int dy, int spx, int spy, float fx, float fy, int r);
extern "C" void advectVelocity(cData *v, float *vx, float *vy, int dx, int pdx, int dy, float dt);
extern "C" void diffuseProject(cData *vx, cData *vy, int dx, int dy, float dt, float visc);
extern "C" void updateVelocity(cData *v, float *vx, float *vy, int dx, int pdx, int dy);
extern "C" void advectParticles(cData *partBuffer, cData *v, int dx, int dy, float dt);

void simulateFluids(void)
{
	// simulate fluid
	advectVelocity(hvfield, (float *)vxfield, (float *)vyfield, DIM, RPADW, DIM, DT);
	diffuseProject(vxfield, vyfield, CPADW, DIM, DT, VIS);
	updateVelocity(hvfield, (float *)vxfield, (float *)vyfield, DIM, RPADW, DIM);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	buffer = (cData*) glMapBuffer(GL_ARRAY_BUFFER,GL_READ_WRITE);
	glBindBuffer(GL_ARRAY_BUFFER, 0); 

	advectParticles(buffer, hvfield, DIM, DIM, DT);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glUnmapBuffer(GL_ARRAY_BUFFER);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	
}

void display(void)
{
	
	
	if (!ref_file)
	{
		t1 = std::chrono::high_resolution_clock::now();
		simulateFluids();
		

	}

	// render points from vertex buffer

	glClear(GL_COLOR_BUFFER_BIT);
	// particle color:
	glColor4f(0,1,0,0.5f);
	glPointSize(1);
	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexPointer(2, GL_FLOAT, 0, NULL);
	glDrawArrays(GL_POINTS, 0, DS);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisable(GL_TEXTURE_2D);

	if (ref_file)
	{
		return;
	}

	// Finish timing before swap buffers to avoid refresh sync
	t2 = std::chrono::high_resolution_clock::now(); 
	time_span += std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
	glutSwapBuffers();

	fpsCount++;
	
	if (fpsCount == fpsLimit)
	{
		char fps[256];
		float ifps = 1.f / ((time_span).count()/ fpsCount);
		sprintf(fps, "Cuda/GL Stable Fluids (%d x %d): %3.1f fps", DIM, DIM, ifps);
		glutSetWindowTitle(fps);
		fpsCount = 0;
		time_span = time_span.zero();
		fpsLimit = (int)std::max(ifps, 1.f);
		//sdkResetTimer(&timer);
	}
	
	glutPostRedisplay();
}


float myrand(void)
{
	static int seed = 72191;
	char sq[22];

	if (ref_file)
	{
		seed *= seed;
		sprintf(sq, "%010d", seed);
		// pull the middle 5 digits out of sq
		sq[8] = 0;
		seed = atoi(&sq[3]);

		return seed/99999.f;
	}
	else
	{
		return rand()/(float)RAND_MAX;
	}
}

void initParticles(cData *p, int dx, int dy)
{
	int i, j;

	for (i = 0; i < dy; i++)
	{
		for (j = 0; j < dx; j++)
		{
			p[i*dx+j].x = (j+0.5f+(myrand() - 0.5f))/dx;
			p[i*dx+j].y = (i+0.5f+(myrand() - 0.5f))/dy;
		}
	}
}



void keyboard(unsigned char key, int x, int y)
{
	switch (key)
	{
		case 27:
			g_bExitESC = true;
#if defined (__APPLE__) || defined(MACOSX)
			exit(EXIT_SUCCESS);
#else
			glutDestroyWindow(glutGetWindow());
			return;
#endif
			break;

		case 'r':
			memset(hvfield, 0, sizeof(cData) * DS);
			initParticles(particles, DIM, DIM);

			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(cData) * DS,
					particles, GL_DYNAMIC_DRAW_ARB);
			glBindBuffer(GL_ARRAY_BUFFER, 0);


			break;

		default:
			break;
	}
}

void click(int button, int updown, int x, int y)
{
	lastx = x;
	lasty = y;
	clicked = !clicked;
}

void motion(int x, int y)
{
	// Convert motion coordinates to domain
	float fx = (lastx / (float)wWidth);
	float fy = (lasty / (float)wHeight);
	int nx = (int)(fx * DIM);
	int ny = (int)(fy * DIM);

	if (clicked && nx < DIM-FR && nx > FR-1 && ny < DIM-FR && ny > FR-1)
	{
		int ddx = x - lastx;
		int ddy = y - lasty;
		fx = ddx / (float)wWidth;
		fy = ddy / (float)wHeight;
		int spy = ny-FR;
		int spx = nx-FR;
		addForces(hvfield, DIM, DIM, spx, spy, FORCE * DT * fx, FORCE * DT * fy, FR);
		lastx = x;
		lasty = y;
	}

	glutPostRedisplay();
}

void reshape(int x, int y)
{
	wWidth = x;
	wHeight = y;
	glViewport(0, 0, x, y);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glutPostRedisplay();
}

void cleanup(void)
{


    // Free all host and device resources
    free(hvfield);
    free(particles);
    free(vxfield);
    free(vyfield);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vbo);

}

int initGL(int *argc, char **argv)
{
	glutInit(argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutInitWindowSize(wWidth, wHeight);
	glutCreateWindow("Compute Stable Fluids");
	// background color
	glClearColor(0, 0, 0, 1.f);
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutMouseFunc(click);
	glutMotionFunc(motion);
	glutReshapeFunc(reshape);


	return true;
}

int main(int argc, char **argv)
{
	int devID;
#if defined(__linux__)
	setenv ("DISPLAY", ":0", 0);
#endif

	printf("%s Starting...\n\n", sSDKname);
	// First initialize OpenGL context, so we can properly set the GL for CUDA.
	// This is necessary in order to achieve optimal performance with OpenGL/CUDA interop.
	if (false == initGL(&argc, argv))
	{
		exit(EXIT_SUCCESS);
	}

	GLint bsize;

	hvfield = (cData *)malloc(sizeof(cData) * DS);
	memset(hvfield, 0, sizeof(cData) * DS);
	vxfield = (cData *)malloc(sizeof(cData) * PDS);
	vyfield = (cData *)malloc(sizeof(cData) * PDS);


	particles = (cData *)malloc(sizeof(cData) * DS);
	memset(particles, 0, sizeof(cData) * DS);


	initParticles(particles, DIM, DIM);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cData) * DS,
			particles, GL_DYNAMIC_DRAW_ARB);

	glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bsize);

	if (bsize != (sizeof(cData) * DS))
		goto EXTERR;

	glBindBuffer(GL_ARRAY_BUFFER, 0);


	#if defined (__APPLE__) || defined(MACOSX)
        atexit(cleanup);
#else
        glutCloseFunc(cleanup);
#endif

	 glutMainLoop();


	return 0;
	
EXTERR:
    printf("Failed to initialize GL extensions.\n");

    exit(EXIT_FAILURE);

}
