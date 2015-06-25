#ifndef __DYE_H
#define __DYE_H

#include "string.h"
#include "hwio.h"
#include "datain.h"
#include "global.h"
#include "ieee_num.h"
#include "motor.h"
#include "encoder.h"
#include "loadcell.h"


#define DYE_COMMAND_NUMBER 11
#define NUM_DYE_CELLS 10

enum DyeMode {		// monitor() routine must be updated if modes are changed
	kDyeIdle,			  // mirror is idle
	kDyeStep,       // mirror moving in step mode
	kDyeSelect,			// mirror is running to a dye cell
	kDyeLimit,		  // mirror is running to a limit switch
	kDyeProblem     // failure due to large encoder error
};


class Dye:public Controller
{
private:
	int   i;							//loop counter
	int   sign(float x){if(x<0.0) return -1; else return 1;} //sign function
	int   initAtLimitFlag; // set by findzero, flag to init at the limit switch

	double time;         	// current time in seconds
	double lastTime;		 	// time on last poll
	double lastDatabaseTime;
	double lastCheckProblemTime;

	DyeMode	dyeMode;	   	// control mode

	int   numReversals;
	int   reverseFlag;
	int   desiredDye;     // desired dye cell (0 - NUM_DYE_CELLS-1)
	float desiredPosition;// position in cm along lead screw (from start pos)
	float	startPosition;	// where it was at start of motion
	float presentPosition;// last known position
	int		presentCell;		// last known cell number
	float oldPosition;    // position at last poll
	float dbasePosition;  // data base position
	float limitStart;     // Pos of forward limit switch
	float limitEnd;       // Pos of end limit switch
	float cellPosition[NUM_DYE_CELLS]; // Positions of dye cells
	float neutralPosition; // Position of mirror for beam to be absorbed

	float cellWavelength[NUM_DYE_CELLS]; // frequencies of dye cells

	float	lastEncoderError;	// last encoder error (for time-average lagging)

	Motor     *motor;						  // Dyecell Mirror Hardware components
	Encoder   *encoder;
	HardwareIO* dyeLaserLimitSwitch1;
	HardwareIO* dyeLaserLimitSwitch2;

	float			encoderStep;				// encoder calibration (cm per step)

	void	decommand(void);        // force Hardware subunits to refuse commands
	void	recommand(void);        // enable Hardware subunit command processing

	void	moveMotor(float dist);	// move motor by specified distance
	void	initDist();					  	// initialize encoders with new distance

public:

	Dye(char* dyeName);        // constructor from database

	~Dye(void);                 // deletes Dye object and subunits

	virtual void	monitor(char *outString);
	virtual void  updateDatabase(void); 	// helper function to update database
	virtual int		needUpdate()	{ return(dbasePosition!=presentPosition);	}

	void			setDyeMode(DyeMode mode);	// set dye mirror control mode
	DyeMode 	getDyeMode() 				 { return dyeMode;	   }

	int				okToMoveMotor();
	float     getPosition();
	int       getCellNumber();
	float     getWavelength(int cellNum){
				if((cellNum>=0)&&(cellNum<=NUM_DYE_CELLS))
					 return(cellWavelength[cellNum]);
				else
					 return -1.0;}
	void      scaleSpeed(float scale);
	float 		getEncoderError();
	void			resetEncoderError();	// set encoder error to zero
	int				isEncoderError();			// is error too large?

	void 			poll();                // polls dye mirror and controls it

	void      setDesiredPosition(float L) {desiredPosition = L;}
	float	  	getDesiredPosition() { return desiredPosition;	}
	void      setStartPosition(float L) {startPosition = L;}
	float     getStartPosition() {return startPosition;}
	void    	setMaxSpeed(char*);     // set dye motor speed limit

	int				isRunning()		 { return(motor->isRunning());	}

	void      initPosition(float position);
	void      wavelength(float lambda);
	void      cell(int cell);
	void      forward(float distance);
	void      backward(float distance);
	void      tostart(void);
	void      toend(void);
	void      toneutral(void);
	void			stop(){ motor->stop(); }
	void      calcell(int cell);
	void      findzero(void);
	void      calzero(void);

private:        // classes for converting member functions to Commands

	class Init: public Command
	{public:
		Init(void):Command("init",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			float arg = atof(inString);
			((Dye*) hw)->initPosition(arg);
			return "HW_ERR_OK";
		}
	};
	class Wavelength: public Command
	{public:
		Wavelength(void):Command("wavelength",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			float arg = atof(inString);
			((Dye*) hw)->wavelength(arg);
			return "HW_ERR_OK";
		}
	};
	class Cell: public Command
	{public:
		Cell(void):Command("cell",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			int arg = atoi(inString);
			((Dye*) hw)->cell(arg);
			return "HW_ERR_OK";
		}
	};

	class Forward: public Command
	{public:
		Forward(void):Command("forward",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			float arg = atof(inString);
			((Dye*) hw)->forward(arg);
			return "HW_ERR_OK";
		}
	};
	class Backward: public Command
	{public:
		Backward(void):Command("backward",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			float arg = atof(inString);
			((Dye*) hw)->backward(arg);
			return "HW_ERR_OK";
		}
	};
	class Tostart: public Command
	{public:
		Tostart(void):Command("tostart",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{((Dye*) hw)->tostart();return "HW_ERR_OK";}
	};
	class Toend: public Command
	{public:
		Toend(void):Command("toend",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{((Dye*) hw)->toend();return "HW_ERR_OK";}
	};
	class Toneutral: public Command
	{public:
		Toneutral(void):Command("toneutral",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{((Dye*) hw)->toneutral();return "HW_ERR_OK";}
	};
	class Calcell: public Command
	{public:
		Calcell(void):Command("calcell",kAccessCode1){	; }
		char* subCommand(Hardware* hw, char* inString)
		{
			int arg = atoi(inString);
			((Dye*) hw)->calcell(arg);
			return "HW_ERR_OK";
		}
	};
	class Findzero: public Command
	{public:
		Findzero(void):Command("findzero",kAccessCode1){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{((Dye*) hw)->findzero();return "HW_ERR_OK";}
	};
	class Stop: public Command
	{public:
		Stop(void):Command("stop",kAccessAlways){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{((Dye*) hw)->stop();return "HW_ERR_OK";}
	};

protected:
	virtual Command *getCommand(int num)	{ return commandList[num]; }
	virtual int 		getNumCommands() 			{ return DYE_COMMAND_NUMBER; }

private:
	static Command	*commandList[DYE_COMMAND_NUMBER];	// list of commands
};

#endif

