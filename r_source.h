/********************************************************************/
/*                                                                  */
/* The rotating source class (r_source) controls the aimable source */
/* for pmt verification.                                            */
/*                                                                  */
/* The class contains two motor objects, which control the theta    */
/* and phi orientaions of the source respectively.                  */
/*                                                                  */
/* The basic functionality of the class is as follows:              */
/*                                                                  */
/* 	1)	It can be interrogated as to the current orientation of     */
/*			the source through the get_orientation member.              */
/*                                                                  */
/*	2)	The orientation can be set through the aim_at memeber.      */
/*                                                                  */
/*	3)	The source can be set to rotate in great circles (i.e.      */
/*			complete circle in theta, then step in phi, and so on )     */
/*			at a given rate through the rotate member.                  */
/*                                                                  */
/********************************************************************/

#ifndef __R_SOURCE_H
#define __R_SOURCE_H

#include "polyaxis.h"

#define R_SOURCE_COMMAND_NUMBER 3

class LockedMotor;

class R_Source:public PolyAxis
{
private:
	Motor *theta;
	LockedMotor *phi;
	float current_theta;
	float current_phi;
	float last_theta;
	float	last_phi;
	long	stepsPerRevolution;

public:

	R_Source(char *objectName, AV *av, FILE *fp);
	~R_Source();

	virtual void	monitor(char *outString);
	virtual int 	stop(int i=0);
	virtual void	rampDown();

	char* 	getOrientation(void);
	char* 	setOrientation(char* inString);
	void 		aimAt(float dest_theta, float dest_phi);
	char* 	aimAt(char* inString);
	void 		rotate(float angle_rate);
	char* 	rotate(char* inString);
	void		stopRotation();
	void 		poll();

private:
	void 		positionUpdate();
	int			checkPhi(float test_phi);

	static class SetOrientation: public Command {
		public:
			SetOrientation(void):Command("setOrientation",kAccessCode2) { }
			char* subCommand(Hardware *hw, char* inString)
				{return ((R_Source*) hw)->setOrientation(inString);}
	} cmd_setorientation;

	static class AimAt: public Command {
		public:
			AimAt(void):Command("aimAt",kAccessCode2){;}
			char* subCommand(Hardware *hw, char* inString)
				{return ((R_Source*)hw)->aimAt(inString);}
	} cmd_aimat;

	static class Rotate: public Command {
		public:
			Rotate(void):Command("rotate",kAccessCode2){;}
			char* subCommand(Hardware *hw, char* inString)
				{return ((R_Source*)hw)->rotate(inString);}
	} cmd_rotate;

protected:
	virtual Command *getCommand(int num);
	virtual int 		getNumCommands();

private:
	static Command	*commandList[R_SOURCE_COMMAND_NUMBER];	// list of commands

};

#endif // ifndef __R_SOURCE_H
