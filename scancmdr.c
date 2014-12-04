
/* ===============================================================================================
	SCANCMDR 
	--------

	Library for programmatic control of a digital signal processor (DSP) for driving a pair of
    galvanometer-controlled mirrors for patterned laser illumination and photo-stimulation.

	(See scancmdr.h or readme file for detailed instructions)

	Dependencies :
	------------

	scancmdr.dll : scancmdr.c scancmdr.h

	Author Information :
	------------------

	M.J.Russo 12/3/2014
	Siegelbaum/Axel Labs
	Department of Neuroscience
	Columbia University
   ============================================================================================== */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <complex.h>

#include "scancmdr.h"

#ifdef __WIN32__
#include <windows.h>
	BOOL WINAPI DllMain(HANDLE hModule,DWORD dwFunction,LPVOID lpNot){
	    return TRUE;
	}
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#ifdef __cplusplus
extern "C"{
#endif

/* FUNCTION DEFINITIONS ==========================================================================*/


/* PROTOCOL BUILDING FUNCTIONS */

// SINGLE SPOT .....................................................................................

EXPORT char* buildSpot(uint32_t Baseline,
                uint32_t TimeOn,
                uint16_t NumPulses,
                uint32_t ISI,
                uint32_t EpisodePeriod,
                uint16_t Reps,
                struct Coord* Pos,
                int64_t ScaleFactor,
                struct Coord* CenterOffset,
                enum Trigger* Trig){

    /* Time conversions */
	/* Coerce inter-stimulus interval (ISI) to pulse-width if pulse-width > ISI. */

    if(ISI < TimeOn){
		ISI = TimeOn;
	}

	/* Coerce episode length to stimulus train + baseline if necessary. */

    if(EpisodePeriod < (Baseline+NumPulses*ISI)){
		EpisodePeriod = (Baseline+NumPulses*ISI);
	}

	/* Convert time parameters from milliseconds to cycles */

	Baseline = Baseline * CYCLES_PER_MS;
    TimeOn = TimeOn * CYCLES_PER_MS;
	ISI = ISI * CYCLES_PER_MS;
	EpisodePeriod = EpisodePeriod * CYCLES_PER_MS;

	uint32_t Time0 = 0;											//Set start time
    uint32_t EndTime = Time0+(EpisodePeriod*Reps)+PROT_PERIOD;	//Calculate end time
    uint32_t PulseStart = Time0 + Baseline;						//Calculate pulse start

    /* Coordinate conversions - pixel-space to galvo-space */

    gCoord galvoCoord = convertCoord(Pos,ScaleFactor,CenterOffset,0);

    ScanProt* pSpotProt = createProtocol();						//Initialize protocol (linked list)

    appendMove(pSpotProt,X,Time0,galvoCoord.X);					//Move to position (X)
    appendMove(pSpotProt,Y,Time0,galvoCoord.Y);					//Move to position (Y)

    /* Start master loop */
	appendLoop(pSpotProt,START,Time0,Reps);

    switch(*Trig){												//Trigger before episode
        case T_NONE:
            break;												//Do nothing.
        case T_IN:
            appendTrigIn(pSpotProt,Time0,RISING);				//Wait for rising trigger (at T=0)
            break;
        case T_OUT:
            appendTrigOut(pSpotProt,Time0,TH_DL);				//Send trigger out
            appendTrigOut(pSpotProt,Time0+TRIG_LEN,TL_DL);
            break;
    }

	/* Add single pulse or pulse train */

    if (NumPulses == 1){
        appendTrigOut(pSpotProt,Time0+Baseline,TL_DH);
        appendTrigOut(pSpotProt,Time0+Baseline+TimeOn,TL_DL);
    }else{
        appendLoop(pSpotProt,START,PulseStart,NumPulses);
        appendTrigOut(pSpotProt,PulseStart,TL_DH);
        appendTrigOut(pSpotProt,PulseStart+TimeOn,TL_DL);
		//End time should be time for single loop:
        appendLoop(pSpotProt,END,PulseStart+ISI,NumPulses);
    }

	/* End master loop */

    appendLoop(pSpotProt,END,EndTime,Reps);

    char* protocolString = ProtToString(pSpotProt);
    clearProtocol(pSpotProt);
    return protocolString;

}

// GRID ............................................................................................

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
					double RotAngle){

	/* Time conversions */
    if(ISI < TimeOn){
		ISI = TimeOn;
	}
    if(EpisodePeriod < (Baseline+NumPulses*ISI)){
		EpisodePeriod = (Baseline+NumPulses*ISI);
	}

	Baseline = Baseline * CYCLES_PER_MS;
    TimeOn = TimeOn * CYCLES_PER_MS;
	ISI = ISI * CYCLES_PER_MS;
	EpisodePeriod = EpisodePeriod * CYCLES_PER_MS * Iterations;

	uint32_t Time0 = 0;
	uint32_t EpisodeStart = Time0 + TIME_OFFSET;
	uint32_t PulseStart = EpisodeStart + Baseline;
	uint32_t XMoveTime = EpisodeStart + EpisodePeriod;
	uint32_t YMoveTime = EpisodeStart + (Dims->X*EpisodePeriod);
	uint32_t EndTime = EpisodeStart + Dims->X*Dims->Y*EpisodePeriod + PROT_PERIOD;

	gCoord gStartPos;
	gCoord gSpacing;
	int64_t gDeltaX1;
	int64_t gDeltaX2;
	int64_t gDeltaY1;
	int64_t gDeltaY2;


	/*Adjust for non-zero rotation*/
	if (RotAngle == 0){
		gStartPos = convertCoord(StartPos,ScaleFactor,CenterOffset,0);
		gSpacing.X = Spacing->X * ScaleFactor;
		gSpacing.Y = Spacing->Y * ScaleFactor;

	}else{
		Coord gridBounds[4];
		gridBounds[0] = *StartPos;
		gridBounds[1].X = StartPos->X + Spacing->X*(Dims->X-1);
		gridBounds[1].Y = StartPos->Y;
		gridBounds[2].X = gridBounds[1].X;
		gridBounds[2].Y = StartPos->Y - Spacing->Y*(Dims->Y-1);
		gridBounds[3].X = StartPos->X;
		gridBounds[3].Y = gridBounds[2].Y;
		Coord gridCenter = getCentroid(4,gridBounds);


		Coord rStartPos = rotateCoord(StartPos,&gridCenter,RotAngle);
		rStartPos.X += gridCenter.X;
		rStartPos.Y += gridCenter.Y;

		double deltaX1 = (double)Spacing->X*cos(RotAngle);
		double deltaX2 = (double)Spacing->Y*sin(RotAngle);
		double deltaY1 = (double)Spacing->X*sin(RotAngle);
		double deltaY2 = (double)Spacing->Y*cos(RotAngle);

		gStartPos = convertCoord(&rStartPos,ScaleFactor,CenterOffset,0);
		gDeltaX1 = round(deltaX1) * ScaleFactor;
		gDeltaX2 = round(deltaX2) * ScaleFactor;
		gDeltaY1 = round(deltaY1) * ScaleFactor;
		gDeltaY2 = round(deltaY2) * ScaleFactor;

	}

	ScanProt* pGridProt = createProtocol();
	/*Initialize at T=0, move to start position */								//COMMAND LIST:
	appendLoop(pGridProt,START,Time0,Reps);										//START MASTER LOOP
	appendMove(pGridProt,X,Time0,gStartPos.X);									//MOVE START X
	appendMove(pGridProt,Y,Time0,gStartPos.Y);									//MOVE START Y

	appendLoop(pGridProt,START,EpisodeStart,Dims->Y);							//Y LOOP START
	appendLoop(pGridProt,START,EpisodeStart,Dims->X);							//X LOOP START

	if(Iterations > 1){
		appendLoop(pGridProt,START,EpisodeStart,Iterations);					//LOOP AT SPOT
	}

	switch(*Trig){   //Trigger before episode									//TRIGGER
        case T_NONE:
            break;	//Do nothing.
        case T_IN:
            appendTrigIn(pGridProt,EpisodeStart,RISING);						//TRIGGER IN
            break;
        case T_OUT:
            appendTrigOut(pGridProt,EpisodeStart,TH_DL);						//TRIGGER OUT START
			appendTrigOut(pGridProt,EpisodeStart+TRIG_LEN,TL_DL);				//TRIGGER OUT END
            break;
    }


	/* Single pulse or train of pulses *****************************/
	if(NumPulses == 1){
		appendTrigOut(pGridProt,PulseStart,TL_DH);								//SINGLE PULSE
		appendTrigOut(pGridProt,PulseStart+TimeOn,TL_DL);
	}else{
		appendLoop(pGridProt,START,PulseStart,NumPulses);						//PULSE LOOP
		appendTrigOut(pGridProt,PulseStart,TL_DH);								//PULSE START
		appendTrigOut(pGridProt,PulseStart+TimeOn,TL_DL);						//PULSE END
		appendLoop(pGridProt,END,PulseStart+ISI,NumPulses);			            //PULSE LOOP END
	}
	/* ************************************************************ */
	if(Iterations > 1){
		appendLoop(pGridProt,END,EpisodeStart + EpisodePeriod,Iterations);		//CLOSE PULSE LOOP
	}

	if(RotAngle == 0){
		appendRel(pGridProt,XMoveTime,X,-1*gSpacing.X);							//MOVE X
		appendLoop(pGridProt,END,XMoveTime,Dims->X);							//X LOOP END
		appendRel(pGridProt,YMoveTime,Y,gSpacing.Y);							//MOVE Y
		appendRel(pGridProt,YMoveTime,X,gSpacing.X*Dims->X);					//MOVE X BACK
		appendLoop(pGridProt,END,YMoveTime,Dims->Y);							//MOVE Y
	}else{
		appendRel(pGridProt,XMoveTime,X,-1*gDeltaX1);
		appendRel(pGridProt,XMoveTime,Y,-1*gDeltaX2);
		appendLoop(pGridProt,END,XMoveTime,Dims->X);
		appendRel(pGridProt,YMoveTime,X,-1*gDeltaY1);
		appendRel(pGridProt,YMoveTime,Y,gDeltaY2);
		appendRel(pGridProt,YMoveTime,X,gDeltaX1*Dims->X);
		appendRel(pGridProt,YMoveTime,Y,gDeltaX2*Dims->X);
		appendLoop(pGridProt,END,YMoveTime,Dims->Y);
    }
	appendLoop(pGridProt,END,EndTime,Reps);										//END MASTER LOOP

	char* protocolString = ProtToString(pGridProt);
    clearProtocol(pGridProt);
    return protocolString;
}

// TARGET ..........................................................................................

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
				  double RotAngle){

	/* Time conversions */
	if(ISI < TimeOn){ ISI = TimeOn; }
    if(EpisodePeriod < (Baseline+NumPulses*ISI)){ EpisodePeriod = (Baseline+NumPulses*ISI); }
	Baseline = Baseline * CYCLES_PER_MS;
    TimeOn = TimeOn * CYCLES_PER_MS;
	ISI = ISI * CYCLES_PER_MS;
	EpisodePeriod = EpisodePeriod * CYCLES_PER_MS * Iterations;


	uint32_t Time0 = 0;
	uint32_t EpisodeStart = Time0 + TIME_OFFSET;
	uint32_t NextEpisode = 0;
	uint32_t NextPulse = 0;
	uint32_t EndTime = EpisodePeriod*NumPoints;

	Coord pCoordArr[NumPoints];
	gCoord gCoordArr[NumPoints];

	getCoords(TargetFile,NumPoints,pCoordArr);

	/*Apply rotation before converting to galvo coordinates*/
	if (RotAngle != 0){
		Coord centroid = getCentroid(NumPoints,pCoordArr);
		int i;
		for (i=0;i < NumPoints;i++){
			pCoordArr[i] = rotateCoord(&pCoordArr[i],&centroid,RotAngle);
		}
	}

	int j;
	for(j=0; j < NumPoints; j++){
		gCoordArr[j] = convertCoord(&pCoordArr[j],ScaleFactor,CenterOffset,0);
	}

	ScanProt* pTargetProt = createProtocol();

	/* Initialize at T=0 */
	appendLoop(pTargetProt,START,Time0,Reps);
	/* Loop through array of coordinates */
	int k;
	for(k = 0; k < NumPoints; k++){
		NextEpisode = EpisodeStart + (k*EpisodePeriod);
		NextPulse = NextEpisode + Baseline;
		appendMove(pTargetProt,X,NextEpisode,gCoordArr[k].X);
		appendMove(pTargetProt,Y,NextEpisode,gCoordArr[k].Y);
		if(Iterations > 1){
			appendLoop(pTargetProt,START,NextEpisode,Iterations);	//Open loop, iterations at spot
		}

		switch(*Trig){   //Trigger before episode
			case T_NONE:
				break;	//Do nothing.
			case T_IN:
				appendTrigIn(pTargetProt,NextEpisode,RISING);
				break;
			case T_OUT:
				appendTrigOut(pTargetProt,NextEpisode,TH_DL);
				appendTrigOut(pTargetProt,NextEpisode+TRIG_LEN,TL_DL);
				break;
		}

		/* Single pulse or train of pulses *****************************/
		if(NumPulses == 1){
			appendTrigOut(pTargetProt,NextPulse,TL_DH);
			appendTrigOut(pTargetProt,NextPulse+TimeOn,TL_DL);
		}else{
			appendLoop(pTargetProt,START,NextPulse,NumPulses);
			appendTrigOut(pTargetProt,NextPulse,TL_DH);
			appendTrigOut(pTargetProt,NextPulse+TimeOn,TL_DL);
			appendLoop(pTargetProt,END,NextPulse+(NumPulses*ISI),NumPulses);
		}
		/* *************************************************************/
		if(Iterations > 1){
			appendLoop(pTargetProt,END,NextEpisode + EpisodePeriod,Iterations);
			//Close loop, iterations at spot
		}
	} //for loop
    appendLoop(pTargetProt,END,EndTime,Reps);


	char* protocolString = ProtToString(pTargetProt);
    clearProtocol(pTargetProt);
    return protocolString;
}

// RAPID GRID ......................................................................................

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
					 double RotAngle){

	//Time conversions from ms to cycles
    if(ISI < TimeOn){ ISI = TimeOn; }
    if(EpisodePeriod < (Baseline+(Dims->X*Dims->Y*ISI))){
		EpisodePeriod = (Baseline+(Dims->X*Dims->Y*ISI));
	}
	Baseline = Baseline * CYCLES_PER_MS;
    TimeOn = TimeOn * CYCLES_PER_MS;
	ISI = ISI * CYCLES_PER_MS;
	EpisodePeriod = EpisodePeriod * CYCLES_PER_MS;

	uint32_t Time0 = 0;
	uint32_t EpisodeStart = Time0 + TIME_OFFSET;
	uint32_t PulseStart = EpisodeStart + Baseline;
	uint32_t XMoveTime = EpisodeStart + ISI;
	uint32_t YMoveTime = EpisodeStart + (Dims->X*ISI);
	uint32_t EndTime = EpisodeStart + EpisodePeriod + PROT_PERIOD;

	gCoord gStartPos = convertCoord(StartPos,ScaleFactor,CenterOffset,0);
	gCoord gSpacing;
	gSpacing.X = -1 * Spacing->X * ScaleFactor;
	gSpacing.Y = -1 * Spacing->Y * ScaleFactor;

	ScanProt* pRapidGridProt = createProtocol();

	appendLoop(pRapidGridProt,START,Time0,Reps);
	appendMove(pRapidGridProt,X,Time0,gStartPos.X);
	appendMove(pRapidGridProt,Y,Time0,gStartPos.Y);

	switch(*Trig){   //Trigger before episode
        case T_NONE:
            break;	//Do nothing.
        case T_IN:
            appendTrigIn(pRapidGridProt,EpisodeStart,RISING);
            break;
        case T_OUT:
            appendTrigOut(pRapidGridProt,EpisodeStart,TH_DL);
			appendTrigOut(pRapidGridProt,EpisodeStart+TRIG_LEN,TL_DL);
            break;
    }

	appendLoop(pRapidGridProt,START,EpisodeStart,Dims->Y);			//Y Loop START
	appendLoop(pRapidGridProt,START,EpisodeStart,Dims->X);			//X Loop START
	appendTrigOut(pRapidGridProt,PulseStart,TL_DH);
	appendTrigOut(pRapidGridProt,PulseStart+TimeOn,TL_DL);

	appendRel(pRapidGridProt,XMoveTime,X,-1*gSpacing.X);			//Move X
	appendLoop(pRapidGridProt,END,XMoveTime,Dims->X);				//X Loop END
	appendRel(pRapidGridProt,YMoveTime,Y,gSpacing.Y);				//Move Y
	appendRel(pRapidGridProt,YMoveTime,X,gSpacing.X*Dims->X);		//Move X Back
	appendLoop(pRapidGridProt,END,YMoveTime,Dims->Y);				//Move Y
	appendLoop(pRapidGridProt,END,EndTime,Reps);					//End Master Loop

	char* protocolString = ProtToString(pRapidGridProt);
    clearProtocol(pRapidGridProt);
    return protocolString;

}

// RAPID TARGET ....................................................................................

EXPORT char* buildRapidTarget(const char* TargetFile,
					   uint32_t Baseline,
					   uint32_t TimeOn,
					   uint32_t ISI,
					   uint32_t EpisodePeriod,
					   uint16_t Reps,
					   uint16_t NumPoints,
					   int64_t ScaleFactor,
                       struct Coord* CenterOffset,
					   enum Trigger*Trig,
					   double RotAngle){

	/* Time conversions */
	if(ISI < TimeOn){ ISI = TimeOn; }
    if(EpisodePeriod < (Baseline+NumPoints*ISI)){ EpisodePeriod = (Baseline+NumPoints*ISI); }
	Baseline = Baseline * CYCLES_PER_MS;
    TimeOn = TimeOn * CYCLES_PER_MS;
	ISI = ISI * CYCLES_PER_MS;
	EpisodePeriod = EpisodePeriod * CYCLES_PER_MS;

	uint32_t Time0 = 0;
	uint32_t EpisodeStart = Time0 + TIME_OFFSET;
	uint32_t PulseStart = EpisodeStart + Baseline;
	uint32_t NextPulse = 0;
	uint32_t EndTime = EpisodePeriod;

	Coord pCoordArr[NumPoints];
	gCoord gCoordArr[NumPoints];
	getCoords(TargetFile,NumPoints,pCoordArr);

	if (RotAngle != 0){
		Coord centroid = getCentroid(NumPoints,pCoordArr);
		int i;
		for (i=0;i < NumPoints;i++){
			pCoordArr[i] = rotateCoord(&pCoordArr[i],&centroid,RotAngle);
		}
	}

	int j;
	for(j=0; j < NumPoints; j++){
		gCoordArr[j] = convertCoord(&pCoordArr[j],ScaleFactor,CenterOffset,0);
	}

	ScanProt* pRapidTargetProt = createProtocol();

	/* Initialize at T=0 */
	appendLoop(pRapidTargetProt,START,Time0,Reps);

	switch(*Trig){   //Trigger before episode
		case T_NONE:
			break;	//Do nothing.
		case T_IN:
			appendTrigIn(pRapidTargetProt,EpisodeStart,RISING);
			break;
		case T_OUT:
			appendTrigOut(pRapidTargetProt,EpisodeStart,TH_DL);
			appendTrigOut(pRapidTargetProt,EpisodeStart+TRIG_LEN,TL_DL);
			break;
	}

	/* Loop through array of coordinates */
	int m;
	for(m = 0; m < NumPoints; m++){
		NextPulse = PulseStart + m*ISI;
		appendMove(pRapidTargetProt,X,NextPulse,gCoordArr[m].X);
		appendMove(pRapidTargetProt,Y,NextPulse,gCoordArr[m].Y);
		appendTrigOut(pRapidTargetProt,NextPulse,TL_DH);
		appendTrigOut(pRapidTargetProt,NextPulse+TimeOn,TL_DL);
	} //for loop
    appendLoop(pRapidTargetProt,END,EndTime,Reps);

	char* protocolString = ProtToString(pRapidTargetProt);
    clearProtocol(pRapidTargetProt);
    return protocolString;
}

// PATTERN .........................................................................................

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
						  double RotAngle){

	uint16_t NumPoints = (uint16_t)getNumPoints(PatternFile);

	/* Time conversions */
	if(ISI < TimeOn){ ISI = TimeOn; }
    if(EpisodePeriod < (Baseline+NumPulses*ISI)){ EpisodePeriod = (Baseline+NumPulses*ISI); }
	Baseline = Baseline * CYCLES_PER_MS;
    TimeOn = TimeOn * CYCLES_PER_MS;
	ISI = ISI * CYCLES_PER_MS;
	EpisodePeriod = EpisodePeriod * CYCLES_PER_MS * Iterations;


	uint32_t Time0 = 0;
	uint32_t EpisodeStart = Time0 + TIME_OFFSET;
	uint32_t NextEpisode = 0;
	uint32_t NextPulse = 0;
	uint32_t EndTime = EpisodePeriod*NumPoints;

	Coord pCoordArr[NumPoints];
	gCoord gCoordArr[NumPoints];

	getPattern(PatternFile,NumPoints,pCoordArr,StartPos,Spacing);

	/*Apply rotation before converting to galvo coordinates*/
	if (RotAngle != 0){
		Coord centroid = getCentroid(NumPoints,pCoordArr);
		int i;
		for (i=0;i < NumPoints;i++){
			pCoordArr[i] = rotateCoord(&pCoordArr[i],&centroid,RotAngle);
		}
	}

	int j;
	for(j=0; j < NumPoints; j++){
		gCoordArr[j] = convertCoord(&pCoordArr[j],ScaleFactor,CenterOffset,0);
	}

	ScanProt* pTargetProt = createProtocol();

	/* Initialize at T=0 */
	appendLoop(pTargetProt,START,Time0,Reps);
	/* Loop through array of coordinates */
	int k;
	for(k = 0; k < NumPoints; k++){
		NextEpisode = EpisodeStart + (k*EpisodePeriod);
		NextPulse = NextEpisode + Baseline;
		appendMove(pTargetProt,X,NextEpisode,gCoordArr[k].X);
		appendMove(pTargetProt,Y,NextEpisode,gCoordArr[k].Y);
		if(Iterations > 1){
			appendLoop(pTargetProt,START,NextEpisode,Iterations);	//Open loop, iterations at spot
		}

		switch(*Trig){   //Trigger before episode
			case T_NONE:
				break;	//Do nothing.
			case T_IN:
				appendTrigIn(pTargetProt,NextEpisode,RISING);
				break;
			case T_OUT:
				appendTrigOut(pTargetProt,NextEpisode,TH_DL);
				appendTrigOut(pTargetProt,NextEpisode+TRIG_LEN,TL_DL);
				break;
		}

		/* Single pulse or train of pulses *****************************/
		if(NumPulses == 1){
			appendTrigOut(pTargetProt,NextPulse,TL_DH);
			appendTrigOut(pTargetProt,NextPulse+TimeOn,TL_DL);
		}else{
			appendLoop(pTargetProt,START,NextPulse,NumPulses);
			appendTrigOut(pTargetProt,NextPulse,TL_DH);
			appendTrigOut(pTargetProt,NextPulse+TimeOn,TL_DL);
			appendLoop(pTargetProt,END,NextPulse+(NumPulses*ISI),NumPulses);
		}
		/* *************************************************************/
		if(Iterations > 1){
			appendLoop(pTargetProt,END,NextEpisode + EpisodePeriod,Iterations);
			//Close loop, iterations at spot
		}
	} //for loop
    appendLoop(pTargetProt,END,EndTime,Reps);


	char* protocolString = ProtToString(pTargetProt);
    clearProtocol(pTargetProt);
    return protocolString;
}

/* HELPER FUNCTIONS ==============================================================================*/

/* Exported */
EXPORT int64_t calcScaling(uint16_t NumPoints, const char* calibrationFile){

	double* pixelXArr = calloc(NumPoints,sizeof(double));
	double* pixelYArr = calloc(NumPoints,sizeof(double));
	double* galvoXArr = calloc(NumPoints,sizeof(double));
	double* galvoYArr = calloc(NumPoints,sizeof(double));

	FILE* fp = fopen(calibrationFile,"r");
	if(fp == NULL){
		fprintf(stderr,"Failed to open calibration file: %s\n",calibrationFile);
	}

	double gX,gY,pX,pY;

	int n = 0;
	while((ftell(fp) != EOF) && (n < NumPoints)){
		fscanf(fp,SCANFORMAT,&gX,&gY,&pX,&pY);
		galvoXArr[n] = gX;
		galvoYArr[n] = gY;
		pixelXArr[n] = pX;
		pixelYArr[n] = pY;
		n++;
	}

	fclose(fp);

	double scalefactor;

    /* Calculates scalefactor from every combination of points */
	int Nmax = 2*NumPoints*(NumPoints-1);
	double* scaleArr = calloc(Nmax,sizeof(double));
	int N = 0;
	int i,j;
	for(i=0;i < NumPoints; i++){
        for(j=i+1;j < NumPoints; j++){
            if(i != j){
                if (galvoXArr[i] != galvoXArr[j]){
					scaleArr[N] = fabs(galvoXArr[i] - galvoXArr[j])/fabs(pixelXArr[i] - pixelXArr[j]);
					N++;
				}
                if (galvoYArr[i] != galvoYArr[j]){
                    scaleArr[N] = fabs(galvoYArr[i] - galvoYArr[j])/fabs(pixelYArr[i] - pixelYArr[j]);
					N++;
				}
			}
        }
    }

	scalefactor = scaleArr[0];
	int k;
	for(k=1; k < Nmax; k++){
		if (scaleArr[k] != 0){
			scalefactor = (scalefactor + scaleArr[k])/2;
		}
	}
	free(pixelXArr);
	free(pixelYArr);
	free(galvoXArr);
	free(galvoYArr);


	free(scaleArr);
    return (int64_t)round(scalefactor);
}

EXPORT struct gCoord convertCoord(struct Coord* pixelCoord, int64_t ScaleFactor, struct Coord* CenterOffset, double RotAngle){

    static gCoord galvoCoord = {0,0};
    long double complex cCoord = (double)pixelCoord->X + (double)pixelCoord->Y * I;
    long double complex offset = (double)CenterOffset->X + (double)CenterOffset->Y * I;

    long double rotTransform = ScaleFactor * cexp(RotAngle*I);

    cCoord = rotTransform*(cCoord - offset);
    galvoCoord.X = (int64_t)(-1*creal(cCoord));
    galvoCoord.Y = (int64_t)(-1*cimag(cCoord));

    return galvoCoord;
}

EXPORT struct Coord rotateCoord(struct Coord* pixelCoord, struct Coord* axisCenter, double RotAngle){

	Coord rotCoord = {0,0};
	Coord normCoord;
	normCoord.X = pixelCoord->X - axisCenter->X;
	normCoord.Y = pixelCoord->Y - axisCenter->Y;

	/*Cast to double to use trigonmetric functions*/
	double XX = (double)normCoord.X;
	double YY = (double)normCoord.Y;
	double rXX = XX*cos(RotAngle) - YY*sin(RotAngle);
	double rYY = XX*sin(RotAngle) + YY*cos(RotAngle);

	rotCoord.X = (int)round(rXX);
	rotCoord.Y = (int)round(rYY);

	return rotCoord;
}

//..................................................................................................


void getCoords(const char* CoordFile, uint16_t NumPoints, struct Coord CoordArr[NumPoints]){
/*Read target coordinates from file */
	FILE* fp = fopen(CoordFile,"r");
	if(fp == NULL){
		fprintf(stderr,"Failed to open coordinate file: %s\n",CoordFile);
	}

    int xcoord = 0;
	int ycoord = 0;
	int i = 0;
	while((ftell(fp) != EOF) && (i < NumPoints)){
		fscanf(fp,"%d\t%d\n",&xcoord,&ycoord);
		//fprintf(stdout,"%d\t%d\n",xcoord,ycoord);
		CoordArr[i].X = xcoord;
		CoordArr[i].Y = ycoord;
		i++;
	}
	fclose(fp);
}

void getPattern(const char* PatternFile, uint16_t NumPoints, struct Coord CoordArr[NumPoints], struct Coord* StartPos, struct Coord* Spacing){
/*Read pattern sequence from file - scale to the specified grid position and spacing*/
	FILE *fp = fopen(PatternFile,"r");
	if (fp==NULL){
		fprintf(stderr,"Failed to open pattern file: %s\n",PatternFile);
	}

	int nPoints;
	int xcoord = 0;
	int ycoord = 0;
	int i = 0;
	fscanf(fp,"%d\n",&nPoints);
	while((ftell(fp) != EOF) && (i < NumPoints)){
		fscanf(fp,"%d\t%d\n",&xcoord,&ycoord);
		fprintf(stdout,"%d\t%d\n",xcoord,ycoord);
		CoordArr[i].X = StartPos->X + (xcoord-1)*Spacing->X;
		CoordArr[i].Y = StartPos->Y - (ycoord-1)*Spacing->Y;
		fprintf(stdout,"%d\t%d\n",CoordArr[i].X,CoordArr[i].Y);
		i++;
	}
	fclose(fp);
}

int getNumPoints(const char* PatternFile){
	FILE* fp = fopen(PatternFile,"r");
	int NumPoints;
	int xDims;
	int yDims;
	fscanf(fp,"%d\t%d\t%d\n",&NumPoints,&xDims,&yDims);
	return NumPoints;
}

struct Coord getCentroid(uint16_t NumPoints, struct Coord CoordArr[NumPoints]){

	Coord coordSum = {0,0};

	int i;
	for(i = 0; i < NumPoints;i++){
		coordSum.X += CoordArr[i].X;
		coordSum.Y += CoordArr[i].Y;
	}

	Coord centroid;
	centroid.X = round(coordSum.X/NumPoints);
	centroid.Y = round(coordSum.Y/NumPoints);

	return centroid;
}

int NumCmds(ScanProt* protocol){
	int numCmds = 0;
	CmdLine* pLoop = protocol->pFirst;
	while(pLoop != NULL){
		numCmds++;
		pLoop = pLoop->pNext;
	}
	return numCmds;
}

char* ProtToString(ScanProt* protocol){

	//Allocate a protocol string
	int NCmds = NumCmds(protocol)+1;
	char* StrProtocol = (char*)calloc((NCmds*MAX_CMD_LEN),sizeof(char));
	if (StrProtocol == NULL){
		fprintf(stderr,"Failure to allocate memory block for protocol string.\n");
		return NULL;
	}

	StrProtocol[0] = 'C';
	StrProtocol[1] = '\n';				//Append CLEAR command at start
	CmdLine* pLoop = protocol->pFirst;

	while(pLoop != NULL){
		char* NextLine = (char*)calloc(MAX_CMD_LEN,sizeof(char));
		if (NextLine == NULL){
			fprintf(stderr,"Failure to allocate memory block for command line string.\n");
			return NULL;
		}
		sprintf(NextLine,FORMAT,pLoop->DSPCmd,pLoop->ScanCmd,pLoop->Cycle,pLoop->Channel,pLoop->Value);
		strcat(StrProtocol,NextLine);
	    pLoop = pLoop->pNext;
		free(NextLine);
	}

	//Resize the protocol string based on actual size of lines
	int ProtLen = strlen(StrProtocol)+1;
    char* newStr = (char*)realloc(StrProtocol,(ProtLen*sizeof(char)));
	if (newStr != NULL){
		StrProtocol = newStr;
	} else {
		fprintf(stderr,"Failure to reallocate memory block.\n");
		return NULL;
	}

	return StrProtocol;
}

struct Coord expandGridCoords(struct Coord* Dims, struct Coord* StartPos, struct Coord* Spacing){

	uint16_t NumPoints = Dims->X*Dims->Y;
	Coord* pCoordArr;
	pCoordArr = (Coord*)calloc(NumPoints,sizeof(Coord));
	int StartX = StartPos->X;
	int StartY = StartPos->Y;
	int deltaX = Spacing->X;
	int deltaY = Spacing->Y;
	int i,j,k;
	for (j=0; j < Dims->Y; j++){
		for (i=0; i < Dims->X; i++){
			k = j*Dims->X + (i+1);
			pCoordArr[k].X = StartX + i*deltaX;
			pCoordArr[k].Y = StartY + j*deltaY;
		}
	}
	return *pCoordArr;
}


/* SCAN COMMAND FUNCTIONS =========================================================================*/

void reassignPointers(ScanProt* pParentList, CmdLine* pChildNode){	  //Generalized function
	if(pParentList->pFirst == NULL){								  //for reassigning pointers
		pParentList->pFirst = pChildNode;							  //as list grows
		pParentList->pLast = pChildNode;
		}
	else {
	    pParentList->pLast->pNext = pChildNode;
		pChildNode->pPrev = pParentList->pLast;
		pParentList->pLast = pChildNode;
		}
}

ScanProt* createProtocol(){							//Initialize a protocol (allocate heap memory);
    ScanProt* pScanProt = calloc(1,sizeof(ScanProt));
	if (pScanProt == NULL){
		perror("Failure to create protocol (allocation error) - ");
	}
	return pScanProt;
}

void clearProtocol(ScanProt* pProtocol){							//Free all nodes from protocol list
	CmdLine* pCmdLine;
	CmdLine* pNextCmd;
	for(pCmdLine = pProtocol->pFirst; pCmdLine != NULL; pCmdLine = pNextCmd){
		pNextCmd = pCmdLine->pNext;
		free(pCmdLine);
		}
		pProtocol->pFirst = pProtocol->pLast = NULL;
}

int appendMove(ScanProt* pProtocol, int channel, const uint32_t cycle, const int64_t position){
    //Appends a move command for single channel at end of list
	CmdLine* pCmdLine = calloc(1,sizeof(CmdLine));
	if (pCmdLine == NULL) {
		perror("Failure to create line at appendMove - ");
		return -1;
	}

	pCmdLine->DSPCmd = 'A';
	pCmdLine->ScanCmd = 'V';
	pCmdLine->Cycle = cycle;
	pCmdLine->Channel = channel;
	pCmdLine->Value = position;

	reassignPointers(pProtocol,pCmdLine);
	return 0;
}

int appendLoop(ScanProt* pProtocol, const char StartOrEnd, const uint32_t cycle, const int64_t repetitions){
    //Append a loop start command at end of list.
	//Start or end are specified as 'S' or 'E' characters
	CmdLine* pCmdLine = calloc(1,sizeof(CmdLine));
	if (pCmdLine == NULL) {
		perror("Failure to create line at appendLoop - ");
		return -1;
	}

	pCmdLine->DSPCmd = 'A';
	switch (StartOrEnd){
		case 'S':
		    pCmdLine->ScanCmd = START;
            pCmdLine->Value = repetitions;
			break;
		case 'E':
		    pCmdLine->ScanCmd = END;
            pCmdLine->Value = repetitions;
			break;
		default: fputs("Start(S) or End(E) not indicated for loop command line\n", stderr);
	}
	pCmdLine->Cycle = cycle;
	pCmdLine->Channel = LOOP;
    //Note: no limit-checking on repetitions

	reassignPointers(pProtocol,pCmdLine);
	return 0;
}

int appendTrigOut(ScanProt* pProtocol, const uint32_t cycle, enum TrigCfg trigger){

	CmdLine* pCmdLine = calloc(1,sizeof(CmdLine));
	if (pCmdLine == NULL) {
		perror("Failure to create line at appendTrig - ");
		return -1;
	}

	pCmdLine->DSPCmd = 'A';
	pCmdLine->ScanCmd = 'V';
	pCmdLine->Cycle = cycle;
	pCmdLine->Channel = TRIG;
	pCmdLine->Value = trigger;

	reassignPointers(pProtocol,pCmdLine);
	return 0;
}

int appendIncr(ScanProt* pProtocol,const uint32_t cycle, const int channel, const int64_t increment){
    //Increment of increment to be implemented later.
	CmdLine* pCmdLine = calloc(1,sizeof(CmdLine));
	if (pCmdLine == NULL) {
		perror("Failure to create line at appendIncr - ");
		return -1;
	}

	pCmdLine->DSPCmd = 'A';
	pCmdLine->ScanCmd = 'I';
	pCmdLine->Cycle = cycle;
	pCmdLine->Channel = channel;
	pCmdLine->Value = increment;

	reassignPointers(pProtocol,pCmdLine);
	return 0;
}

int appendWait(ScanProt* pProtocol, const int64_t waitTime){

	CmdLine* pCmdLine = calloc(1,sizeof(CmdLine));
	if (pCmdLine == NULL){
		perror("Failure to create line at appendWait - ");
		return -1;
	}

	pCmdLine->DSPCmd = 'A';
	pCmdLine->ScanCmd = '0';
	pCmdLine->Cycle = waitTime;
	pCmdLine->Channel = 0;
	pCmdLine->Value = 0;

	reassignPointers(pProtocol,pCmdLine);
	return 0;
}

int appendRel(ScanProt* pProtocol,  const uint32_t cycle, const int channel, const int64_t deltaValue){

	CmdLine* pCmdLine = calloc(1,sizeof(CmdLine));
	if (pCmdLine == NULL){
		perror("Failure to create line at appendRel - ");
		return -1;
	}

	pCmdLine->DSPCmd = 'A';
	pCmdLine->ScanCmd = 'R';
	pCmdLine->Cycle = cycle;
	pCmdLine->Channel = channel;
	pCmdLine->Value = deltaValue;

	reassignPointers(pProtocol,pCmdLine);
	return 0;
}

int appendOffset(ScanProt* pProtocol, const uint32_t cycle, const int channel, const int64_t offsetValue){

	CmdLine* pCmdLine = calloc(1,sizeof(CmdLine));
	if (pCmdLine == NULL){
		perror("Failure to create line at appendOffset - ");
		return -1;
	}

	pCmdLine->DSPCmd = 'A';
	pCmdLine->ScanCmd = 'O';
	pCmdLine->Cycle = cycle;
	pCmdLine->Channel = channel;
	pCmdLine->Value = offsetValue;

	reassignPointers(pProtocol,pCmdLine);
	return 0;
}

int appendTrigIn(ScanProt* pProtocol, const uint32_t cycle, enum TrigIn risingFalling){

		CmdLine* pCmdLine = calloc(1,sizeof(CmdLine));
	if (pCmdLine == NULL){
		perror("Failure to create line at appendTrigIn - ");
		return -1;
	}

	pCmdLine->DSPCmd = 'A';
	switch (risingFalling){
		case 1:
			pCmdLine->ScanCmd = 'U';
			break;
		case 2:
			pCmdLine->ScanCmd = 'D';
			break;
		default:
			pCmdLine->ScanCmd = 'U';
	}
	pCmdLine->Cycle = cycle;
	pCmdLine->Channel = TRIG;
	pCmdLine->Value = 0;

	reassignPointers(pProtocol,pCmdLine);
	return 0;
}

//..................................................................................................

#ifdef __cplusplus
}
#endif




