
/* ===============================================================================================

	SCANCMDR
	--------

	Library for programmatic control of a digital signal processor (DSP) for driving galvanometer-
	directed laser illumination and photo-stimulation.
	
	Hardware Summary :
	----------------

	Scan Control DSP v1.7.0
	BioImaging Zentrum (LMU) / TILL Photonics

	The Scan-Control DSP generates the control signal for the SmartMove DC900 galvanometer driver
	boards, including an 8 MHz sample clock, 8 digital outputs and 4 analog output channels
	(16 bit resolution). These output devices can be controlled through the DSP via a set of commands
    which form a protocol.  The control protocol is uploaded to the DSP via RS232 (connection
    parameters listed below).

	A protocol comprises a list of commands.  Each command is a single line with several components:
	a single character control command, a single character scan control command, and the parameters
    of the scan control command (cycle, channel, and value).

		DSP Control Command		(single character/uppercase letter)
		Scan Command			(single character/uppercase letter)
		Cycle					(48 bit, long), 10-us cycle in which command is executed
		Channel					(24 bit, int), channel whose value is changed
		Value					(48 bit, long), value to set

	All command lines are terminated by a semicolon(';'), newline ('\n'), or return character ('\r').


	DSP Control Commands:
    *Indicates most commonly used commands for directed photostimulation.

		#	Do nothing. Keeps interface busy, can signal break of protocol execution. No arguments.
		R	Returns DSP information, firmware version. No arguments.
	*	C	Clear protocol.  No arguments, returns status/error code.
	*	A	Add command to scan command list. Specify scan command, cycle, channel, and value.
	*	X	Execute protocol.  No arguments, returns status/error code.
		L	Returns protocol in memory.
		B	Returns contents of the buffer for a specified channel.
		?	Returns current value of scan channel.
	*	V	Directly sets value of a scan channel.  Specify channel and value.
		O	Sets an offset to be added to all values of galvo position channels (i.e. channels 3,4)

	Scan Commands:

		0	Zero, do nothing.
	*	V	Set the value of an output channel.
	*	R	Set value relative to current value.
		I	Set value by which channel (e.g. position) is incremented every cycle.
		J   Set value by which increment is incremented every cycle.
		O	Switch offset on and off.
	*	S	Indicate start of loop.
	*	E	Indicate end of loop.
	*	U	Signal wait for rising trigger.
		D	Signal wait for falling trigger.

	Cycles (timing) and Channels:

		All input/output signals of the DSP are via time multiplexed serial interface (TMSI). The
		TMSI transmits 14 bytes of data each direction during a single time frame (cycle) of 10us.
		There are 14 timeslots of 8 bits each. Some of these timeslots are grouped into channels for
        multibyte data. The relevant channels for the 2 mirror system are as follows:

		Channel		Timeslot		Function
		   0			0			reserved
		   1			1			analog out configuration		[not used]
		   2			2			analog out value (high byte)	[not used]
						3			analog out value (low byte)		[not used]
           3			4			position galvo 0 (high byte)
						5			position galvo 0 (low byte)
		   4			6			position galvo 1 (high byte)
						7			position galvo 1 (low byte)
	       5			8			position galvo 2 (high byte)	[not used]
						9			position galvo 2 (low byte)		[not used]
           6			10			position galvo 3 (high byte)	[not used]
						11			position galvo 3 (low byte)		[not used]
           7			12			digital out (3 LSBs indicate which output)
           8			13			reserved

		  (9)						Loop start/end commands do not include a specific channel.
									It is helpful for debugging/readability to set the loop channel
									to 9.

		**Important Notes**

		-Due to a bug in the DSP firmware, triggering will not work as expected if it is set within 
		 cycle zero.
		-Loop timing.  Loops starting at time t0 with n iterations, and single iteration time of dt,
		 are entered as follows:

		 	t0 := start time
		 	dt := single iteration time
		 	n  := number of iterations

		 	tf := final time

		 	AS,t0,9,n\n
		 	AE,tf,9,n\n

		 	where tf = t0 + n * dt
		 	
		-The technical documents indicate that channel and value in a loop start/end command are not
		 read by the DSP.  However, for readability, the channel for all loop start/end commands is
		 entered as 9, and the value (no. of loop iterations) is included for both start AND end 
		 commands.

		 Example:
		 Simple loop, 1000 iterations, with a nested 10-cycle wait.

			AS,0,9,1000\n 			#Loop Start
			A0,10,0,0\n             #Wait 10 cycles
			AE,10000,9,1000\n       #End loop after 10*1000 = 10000 cycles (total loop time)
	Values:

		Position (Channels 3,4,5,6)

		The galvanometer mirror positions are defined as a signed
		36-bit integer, termed a microcount (ucount).  Thus, the range of galvo positions is -2^35
		to +2^35-1.  However, only the two most significant bytes are sent to the device, so the
	    actual resolution is defined by a 16-bit signed integer, termed simply a 'count'.  Counts
		are most relevant when setting the offset value, as this is set as number of counts.

		Relative Position (Channels 3,4,5,6)

		Position relative to the current position is defined with the scan command 'R'. The relative
        position is defined in terms of ucounts, just as for the position value.  For instance, if
		galvo0 is at position 10000 ucounts, it can be positioned at 14700 ucounts by specifying a
		relative change of +4700 ucounts.

		Digital Out (Channel 7)

		There are 2 digital outputs available (e.g. for delivering a TTL (5V) trigger to other devices).
		The digital output levels are specified in channel 7, with the least significant bits of
		the sent byte.  Simply send the base10 integer value that corresponds to the low-order bit
		values.

		BOTH LOW		000		'0' (Value to send).
		T-OUT HIGH		001		'2'
		D-OUT HIGH		010		'4'
		BOTH HIGH		100		'6'

		Loops ("Channel 9")

		Loops start commands require the number of iterations as a value.  Loop end commands do not
		require a specific value, just an integer place-holder.  However, it is helpful for
		debugging purposes to set the loop end value to the same iteration value as the corresponding
		loop start command.


	Important values/constants:

		Cycle: 10-us (therefore, 10^5 cycles per second)
		Position value range: -2^34 to +2^35-1 (microcounts)
		Maximum commands (lines) in protocol: 10000

	Serial Control (RS232) :
	----------------------

	Baud:		57600
	Data bits:	8
	Parity:		none
	Stop bits:  1
	Flow ctrl:  none

	Dependencies :
	------------

	scancmdr.dll : scancmdr.c scancmdr.h

	Author Information :
	------------------

	M.J.Russo, 7/5/2014
	Siegelbaum/Axel Labs
	Department of Neuroscience
	Columbia University

  =============================================================================================== */

#ifndef SCANCMDR_H_
#define SCANCMDR_H_

#include <inttypes.h>
#ifdef __WIN32__
#include <windows.h>
#endif

/* PREPROCESSOR ==================================================================================*/

#define CYCLE_LEN 10          //Cycle length, microseconds
#define CYCLES_PER_MS 100	  //Cyles per millisecond
#define TRIG_LEN 10           //Trigger length, cycles (default: 100 us)
#define MAX_CMD_LEN 100		  //Maximum length of command line, in characters
#define STOPCHAR '\n'		  //Options, '\n, '\r' or ';' may be redundant with FORMAT specifier
#define	CLEAR "C\n"			  //Clear command
#define EXECUTE "X\n"		  //Execute command
#define START 'S'			  //Loop start character
#define END 'E'				  //Loop end character
#define MOVE_TIME 140         //Smart-move time + jump time for galvos (in cycles)
#define TIME_OFFSET 10	      //Offset, cycles (x10 microseconds)
#define PROT_PERIOD 50         //Wait time after each complete protocol repetition (in cycles)

#ifdef __WIN32__
#define FORMAT "%c%c,%I32u,%i,%I64d\n"				//WINDOWS format specifier
#define SCANFORMAT "%lf\t%lf\t%lf\t%lf\n"
#else
#define FORMAT "%c%c,%" PRIu32 ",%i,%" PRId64 "\n"	//POSIX format specifier
#define SCANFORMAT "%lf\t%lf\t%lf\t%lf\n"
#endif



#ifdef __WIN32__
	BOOL WINAPI DllMain(HANDLE hModule,DWORD dwFunction,LPVOID lpNot);
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#ifdef __cplusplus
extern "C"{
#endif

/* CUSTOM DATA TYPES ==============================================================================*/

typedef struct Coord{           //pixels/microns
    int X;
    int Y;
}Coord;

typedef struct gCoord{				//For galvo coordinates (microcounts)
    int64_t X;
	int64_t Y;
}gCoord;

enum Trigger{
    T_NONE = 0,
    T_IN = 1,
    T_OUT = 2
};

/* Scan commands */
typedef struct CmdLine{				//Nodes in the doubly linked protocol lists
	char DSPCmd;
	char ScanCmd;
	uint32_t Cycle;
	int Channel;
	int64_t Value;

	struct CmdLine* pPrev;
	struct CmdLine* pNext;
} CmdLine;

typedef struct ScanProt{			//This will serve as the master protocol list:
	struct CmdLine* pFirst;			//Doubly linked list of command lines (nodes)
	struct CmdLine* pLast;
} ScanProt;

enum { X = 4,						//Constants defining the channels to be used (for readability)
       Y = 3,
	TRIG = 7,
	LOOP = 9
};

enum TrigCfg{						//Trigger out configurations
	TL_DL = 0,						//T-OUT is trigger to another device
	TH_DL = 2,						//D-OUT is shutter control of laser
	TL_DH = 4,
	TH_DH = 6
}TrigCfg;

enum TrigIn{
	RISING = 1,
	FALLING = 2
}TrigIn;


/* FUNCTION PROTOTYPES ===========================================================================*/

/* Exported functions */
EXPORT char* buildSpot(uint32_t Baseline,
                uint32_t TimeOn,
                uint16_t NumPulses,
                uint32_t ISI,
                uint32_t EpisodePeriod,
                uint16_t Reps,
                struct Coord* Pos,
                int64_t ScaleFactor,
                struct Coord* CenterOffset,
                enum Trigger* Trig);

EXPORT char* buildGrid(uint32_t Baseline,
				uint32_t TimeOn,
				uint16_t NumPulses,
				uint32_t ISI,
				uint32_t Iterations,
				uint32_t EpisodePeriod,
				uint16_t Reps,
				struct Coord* Dims,
				struct Coord* StartPos,
				struct Coord* Spacing,
				int64_t ScaleFactor,
                struct Coord* CenterOffset,
				enum Trigger* Trig,
			    double RotAngle);

EXPORT char* buildTarget(const char* TargetFile,
				uint32_t Baseline,
				uint32_t TimeOn,
				uint16_t NumPulses,
				uint32_t ISI,
				uint32_t Iterations,
				uint32_t EpisodePeriod,
				uint16_t Reps,
				uint16_t NumPoints,
				int64_t ScaleFactor,
				struct Coord* CenterOffset,
				enum Trigger* Trig,
				double RotAngle);

EXPORT char* buildRapidGrid(uint32_t Baseline,
				uint32_t TimeOn,
				uint32_t ISI,
				uint32_t EpisodePeriod,
				uint16_t Reps,
				struct Coord* Dims,
				struct Coord* StartPos,
				struct Coord* Spacing,
				int64_t ScaleFactor,
				struct Coord* CenterOffset,
				enum Trigger* Trig,
				double RotAngle);

EXPORT char* buildRapidTarget(const char* TargetFile,
				uint32_t Baseline,
				uint32_t TimeOn,
				uint32_t ISI,
				uint32_t EpisodePeriod,
				uint16_t Reps,
				uint16_t NumPoints,
				int64_t ScaleFactor,
				struct Coord* CenterOffset,
				enum Trigger* Trig,
				double RotAngle);

EXPORT char* buildPattern(const char* PatternFile,
				uint32_t Baseline,
				uint32_t TimeOn,
				uint16_t NumPulses,
				uint32_t ISI,
				uint32_t Iterations,
				uint32_t EpisodePeriod,
				uint16_t Reps,
				struct Coord* StartPos,
				struct Coord* Spacing,
				int64_t ScaleFactor,
				struct Coord* CenterOffset,
				enum Trigger* Trig,
				double RotAngle);

/* Protocol helper functions */

EXPORT int64_t calcScaling(uint16_t NumPoints, const char* calibrationFile);

EXPORT struct gCoord convertCoord(struct Coord* pixelCoord, int64_t ScaleFactor, struct Coord* CenterOffset, double RotAngle);

EXPORT struct Coord rotateCoord(struct Coord* pixelCoord, struct Coord* axisCenter, double RotAngle);

void getCoords(const char* CoordFile, uint16_t NumPoints, struct Coord CoordArr[NumPoints]);

void getPattern(const char* PatternFile, uint16_t NumPoints, struct Coord CoordArr[NumPoints],struct Coord* StartPos, struct Coord* Spacing);

int getNumPoints(const char* PatternFile);

struct Coord getCentroid(uint16_t NumPoints, struct Coord CoordArr[NumPoints]);

int NumCmds(ScanProt* protocol);

char* ProtToString(ScanProt* protocol);

struct Coord expandGridCoords(struct Coord* Dims, struct Coord* StartPos, struct Coord* Spacing);


/* Scan command functions */
ScanProt* createProtocol();

void clearProtocol(ScanProt* pProtocol);

void reassignPointers(ScanProt* pParentList, CmdLine* pChildNode);

int appendMove(ScanProt* pProtocol, int channel, const uint32_t cycle, const int64_t position);

int appendLoop(ScanProt* pProtocol, const char StartOrEnd, const uint32_t cycle, const int64_t repetitions);

int appendTrigOut(ScanProt* pProtocol, const uint32_t cycle, enum TrigCfg trigger);

int appendIncr(ScanProt* pProtocol, const uint32_t cycle, const int channel, const int64_t increment);

int appendWait(ScanProt* pProtocol, const int64_t waitTime);

int appendRel(ScanProt* pProtocol,  const uint32_t cycle, const int channel, const int64_t deltaValue);

int appendOffset(ScanProt* pProtocol, const uint32_t cycle, const int channel, const int64_t offsetValue);

int appendTrigIn(ScanProt* pProtocol, const uint32_t cycle, enum TrigIn risingFalling);


#ifdef __cplusplus
}
#endif
#endif //SCANCMDR_H_


