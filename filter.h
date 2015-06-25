#ifndef __FILTER_H
#define __FILTER_H

#include "hwio.h"
#include "datain.h"
#include "global.h"
//#include "IEEE_num.h"
#include "motor.h"
#include "encoder.h"
#include "loadcell.h"


#define FILTER_COMMAND_NUMBER 12
#define NUM_STOPS 8

enum FilterMode {		// monitor() routine must be updated if modes are changed
	kFilterIdle,			  // filter wheel is idle
	kFilterStep,       // filter wheel moving in step mode
	kFilterSelect,			// filter wheel is running to a stop pos
	kFilterTab,		    // filter wheel is running to the tab switch
	kFilterProblem    // signal problem with the motor
};


class Filter:public Controller
{
private:
	int   i;              //loop counter
	int   sign(float x){if(x<0.0) return -1; else return 1;} //sign function
	int   initAtTabFlag; // set by findTab, flag to init at the limit switch

	double time;         	// current time in seconds
	double lastTime;		 	// time on last poll
	double lastDatabaseTime;
	double lastCheckProblemTime;
	double startedRunningTime;

	FilterMode	filterMode;	   	// control mode

	float desiredPosition;// position in degrees (from tab pos)
	float	startPosition;	// where it was at start of motion
	float presentPosition;// last known position
	float oldPosition;    // position at last poll
	float dbasePosition;  // data base position
	float tabPosition;    // Position of optical switch
	float stopPosition[NUM_STOPS+1]; // Positions of dye cells
	float lastEncoderError; //lagged encoder error used in isEncoderError()

	float neutralDensity[NUM_STOPS+1]; // frequencies of dye cells

	Motor     *motor;						  // Filter wheel Hardware components
	Encoder   *encoder;
	HardwareIO* filterTabSwitch;

	float			encoderStep;				// encoder calibration (degree per step)

	void	decommand(void);        // force Hardware subunits to refuse commands
	void	recommand(void);        // enable Hardware subunit command processing

	void  moveMotor(float angle);

	float remdeg(float angle){  //returns a positive remainder
				if(angle>=0.0)
					 return (angle - 360.0*((int)(angle/360.0)));
				else
					 return (angle - 360.0*((int)(angle/360.0)-1));}
public:

	Filter(char* filterName);        // constructor from database

	~Filter(void);                 // deletes Filter object and subunits

	virtual void	monitor(char *outString);
	virtual void  updateDatabase(void); 	// helper function to update database
	virtual int		needUpdate()	{ return(dbasePosition!=getPosition());	}

	void			 setFilterMode(FilterMode mode);	// set filter control mode
	FilterMode getFilterMode() 				 { return filterMode;	   }

	float     getPosition();
	float     getNDvalue(int stopNum){
				if((stopNum>=0)&&(stopNum<=NUM_STOPS))
					 return(neutralDensity[stopNum]);
				else
					 return -1.0;}
	int       getStopNumber();
	int       getNumStops(){ return NUM_STOPS;}
	void      scaleSpeed(float scale);
	float 		getEncoderError();
	void			resetEncoderError();	// set encoder error to zero
	int       isEncoderError();

	void 			poll();                // polls dye mirror and controls it

	void      setDesiredPosition(float L) {desiredPosition = L;}
	float	  	getDesiredPosition() { return desiredPosition;	}
	void      setStartPosition(float L) {startPosition = L;}
	float     getStartPosition() {return startPosition;}
	void    	setMaxSpeed(char*);     // set dye motor speed limit

	int				isRunning()		 { return(motor->isRunning());	}

	void      initPosition(float position);
	void      reSynchPos(void);
	void      cwd(float angle);
	void      ccwd(float angle);
	void      cws(int stops);
	void      ccws(int stops);
	void      position(int position);
	void      toTab(void);
	void      calTab(void);
	void			stop(){ motor->stop(); }
	void      nd(float ndvalue);
	void      calPosition(int position);
	void      findTab(void);

private:        // classes for converting member functions to Commands

	class Init: public Command
	{public:
		Init(void):Command("init"){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			float arg = atof(inString);
			((Filter*) hw)->initPosition(arg);
			return "HW_ERR_OK";
		}
	};
	class CWD: public Command
	{public:
		CWD(void):Command("cwd"){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			float arg = atof(inString);
			((Filter*) hw)->cwd(arg);
			return "HW_ERR_OK";
		}
	};

	class CCWD: public Command
	{public:
		CCWD(void):Command("ccwd"){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			float arg = atof(inString);
			((Filter*) hw)->ccwd(arg);
			return "HW_ERR_OK";
		}
	};

	class CWS: public Command
	{public:
		CWS(void):Command("cws"){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			int arg = atoi(inString);
			((Filter*) hw)->cws(arg);
			return "HW_ERR_OK";
		}
	};

	class CCWS: public Command
	{public:
		CCWS(void):Command("ccws"){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			int arg = atoi(inString);
			((Filter*) hw)->ccws(arg);
			return "HW_ERR_OK";
		}
	};

	class Position: public Command
	{public:
		Position(void):Command("position"){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			int arg = atoi(inString);
			((Filter*) hw)->position(arg);
			return "HW_ERR_OK";
		}
	};

	class ToTab: public Command
	{public:
		ToTab(void):Command("totab"){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{((Filter*) hw)->toTab();return "HW_ERR_OK";}
	};

	class CalTab: public Command
	{public:
		CalTab(void):Command("caltab"){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{((Filter*) hw)->calTab();return "HW_ERR_OK";}
	};

	class ND: public Command
	{public:
		ND(void):Command("nd"){	; }
		char* subCommand(Hardware* hw, char* inString)
		{
			float arg = atof(inString);
			((Filter*) hw)->nd(arg);
			return "HW_ERR_OK";
		}
	};

	class FindTab: public Command
	{public:
		FindTab(void):Command("findtab"){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{((Filter*) hw)->findTab();return "HW_ERR_OK";}
	};

	class Stop: public Command
	{public:
		Stop(void):Command("stop",kAccessAlways){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{((Filter*) hw)->stop();return "HW_ERR_OK";}
	};

	class CalPosition: public Command
	{public:
		CalPosition(void):Command("calposition"){ ; }
		char* subCommand(Hardware* hw, char* inString)
		{
			int arg = atoi(inString);
			((Filter*) hw)->calPosition(arg);
			return "HW_ERR_OK";
		}
	};

protected:
	virtual Command *getCommand(int num)	{ return commandList[num]; }
	virtual int 		getNumCommands() 			{ return FILTER_COMMAND_NUMBER; }

private:
	static Command	*commandList[FILTER_COMMAND_NUMBER];	// list of commands
};

#endif

