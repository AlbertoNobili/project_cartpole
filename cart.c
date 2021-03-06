#include <math.h>
#include <stdio.h>
#include "ase_ace.h"

// Physical constants
#define LEFT_LIMIT	-3.0	// position of left block [m]
#define RIGHT_LIMIT	3.0		// position of right block [m]
#define THETA_LIMIT	0.2		// 0.2 rad = 11.5 deg
#define CART_MASS	1.0		// mass of the cart [kg]
#define POLE_MASS	0.1		// mass of the pole [kg]
#define POLE_LEN	0.5		// length of the pole [m]
#define FORCE		10		// force level [N]
#define GRAVITY		9.8		// [m/s^2]

// Simulation constants
#define MAXDUR		40000000L	// max duration
#define MAXFAIL		10000000L	// max # of failures
#define NBOXES		162			// # of state boxes

// Cart structure
typedef struct{
	float pos;
	float speed;
	float theta;
	float omega;
} state;
static state s;

// Variables manipulated by graphics.c
int view;	// to know when display the cart evolution
int stop;	// to know when to stop

//Auxiliary functions
void set_state(float x, float v, float t, float w)
{
	s.pos = x;
	s.speed = v;
	s.theta = t;
	s.omega = w;
}

int out_limits()
{
	if ((s.pos > RIGHT_LIMIT) || (s.pos < LEFT_LIMIT) || 
		(fabs(s.theta) > THETA_LIMIT))	return 1;
	return 0;
}

// Update state
int update_state(float force)
{
	float ct, st, dt;		// sin, cos, integration step
	float x_acc, t_acc;		// linear & angular acceleration

		ct = cos(s.theta);
		st = sin(s.theta);
		t_acc = ((CART_MASS + POLE_MASS)*GRAVITY*st - 
					(force+POLE_MASS*POLE_LEN*s.omega*s.omega*st)*ct)/
					(4/3*(CART_MASS+POLE_MASS)*POLE_LEN-POLE_MASS*POLE_LEN*ct*ct);
		x_acc = (force + POLE_MASS*POLE_LEN*(s.omega*s.omega*st-t_acc*ct))/(POLE_MASS+CART_MASS);
		dt = 0.01;			// integration step of 10 ms
		s.pos   += s.speed*dt;
		s.speed += x_acc*dt;
		s.theta += s.omega*dt;
		s.omega += t_acc*dt;
		return out_limits();
}

// Decode state
#define XL	0.8
#define VL	0.5
#define T1	0.01745
#define T6	0.10472
#define W50	0.87266

int decode_state()
{
int box;

	if 		(s.pos < -XL)	box = 0;
	else if (s.pos < XL)	box = 1;
	else 					box = 2;

	if 		(s.speed < -VL)	;
	else if (s.speed < VL)	box +=3;
	else					box += 6;	

	if 		(s.theta < -T6)	;
	else if (s.theta <-T1)	box += 9;
	else if (s.theta < 0)	box += 18;
	else if (s.theta < T1)	box += 27;
	else if (s.theta < T6)	box += 36;
	else 					box += 45;

	if 		(s.omega < -W50);
	else if (s.omega < W50)	box += 54;
	else					box += 108;

	return box;
}

// External graphic functions
extern void init_graphics();
extern void display_cart(state s);
extern void terminate_graphics();
extern void read_key();
extern void update_info(long epoch, long maxd);

// Learning loop
int main()
{
long total_steps;	// total # of steps
long duration;		// # steps pole balanced
long maxd;			// max duration 
long failures;		// failure counter (epoch counter)
int box;			// decoded state region
int y;				// ASE output
float p;			// ACE output
int r;				// primary reinforce
float sr;			// secondary reinforce
int fail;			// failure flag
float force;		// applied force to the cart	

	init_graphics();
	total_steps = 0;
	duration = 0;
	maxd = 0;
	failures = 0;
	init_net(NBOXES);
	clear_eligibilities_traces();
	set_state(0, 0, 0, 0);
	box = decode_state();
	while (1){
	// Nota che failures corrisponde al numero di epoche 
	// e quindi impostare un limite massimo MAXFAIL equivale
	// a impostare un numero massimo di epoche.
	// Dare un limite MAXDUR invece significa fermarsi quando 
	// la rete ha imparato a tenere in equilibrio il palo 
	// per un tempo sufficientemente lungo
		read_key();
		if(stop) break;
		if(view)
			display_cart(s);
		duration++;
		total_steps++;

		y = ase_output(box);
		update_eligibilities_traces(box, y);
		force = FORCE*y;
		fail = update_state(force);
		box = decode_state();
		if (fail){
			r = -1;
			p = 0;	
			failures++;
			if (duration > maxd)
				maxd = duration;
			update_info(failures, maxd);
			duration = 0;
			set_state(0, 0, 0, 0);
			box = decode_state();
		} else {
			r = 0;
			p = ace_output(box);
		}
		sr = secondary_reinforce(r);
		update_weights(sr);
		if (fail) clear_eligibilities_traces();
		else decay_eligibilities_traces();
	}
	if (duration > maxd)
		maxd = duration;	
	terminate_graphics();
	printf("Main termintao.\nfailures = %ld; max duration = %ld\n", failures, maxd);
}

/*
// second option for state decoding
int decode_x (float x)
{
	if (x < -XL)	return 0;
	if (x <  XL)	return 1;
	else			return 2;
}
int decode_v (float v)
{
	if (v < -VL)	return 0;
	if (v <  VL)	return 1;
	else			return 2;
}
int decode_t (float t)
{
	if (t < -T6)	return 0;
	if (t < -T1)	return 1;
	if (t < 0)		return 2;
	if (t < T1)		return 3;
	if (t < T6)		return 4;
	else			return 5;
}
int decode_w (float w)
{
	if (w < -W50)	return 0;
	if (w < W50)	return 1;
	else			return 2;
}
#define NBX	3
#define NBV	3
#define NBT	6
#define NBW	3
int decode_state()
{
int box;
	box = decode_x(s.pos) + 
			decode_v(s.speed)*NBX +
			decode_t(s.theta)*NBX*NBV + 
			decode_w(s.omega)*NBX*NBV*NBT;
	return box;
} */
