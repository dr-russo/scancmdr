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
