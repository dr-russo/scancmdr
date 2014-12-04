/* ===============================================================================================
	
	SCTEST
	------

	Test script for scancmdr shared library.  Link to scancmdr.dll, modify parameters below, and 
	run.  Script will calculate scaling factor from a set of calibration coordinates, and will 
	output text commands for single spot illumination, targeted illumination, and grid patterned 
	illumination. 
	
	Dependencies :
	------------

	sctest.c : scancmdr.dll : scancmdr.c scancmdr.h
	(also requires targets.coord and calibration.coord)

	Author Information :
	------------------

	M.J.Russo, 7/5/2014
	Siegelbaum/Axel Labs
	Department of Neuroscience
	Columbia University

  =============================================================================================== */


#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>

#ifdef __WIN32__
#include <windows.h>
#endif


#include "scancmdr.h"

int main(){
	
	/*Enter protocol type to output
	 	(1) Single Spot
	 	(2) Grid pattern
	 	(3) Targeted pattern */
	const int type = 2;


	//For grid:
	struct Coord dims;
	dims.X = 5;
	dims.Y = 5;
	struct Coord startpos;
	startpos.X = 40;
	startpos.Y = 320;
	struct Coord spacing;
	spacing.X = 50;
	spacing.Y = 50;
	struct Coord position;
	position.X = 450;
	position.Y = 400;
	

	//Enter timing parameters
	uint32_t baseline = 400;
	uint32_t timeon = 200;
	uint16_t numpulses = 5;
	uint32_t isi = 400;
	uint32_t iterations = 10;
	uint32_t episodeperiod = 2000;
	uint16_t reps = 1;
	uint16_t numpoints = 256;

	struct Coord centeroffset;
	centeroffset.X = 716;
	centeroffset.Y = 206;
	enum Trigger trig = T_OUT;
	double theta = 10;

	const char* coordFile = "test-targets.coord";
	const char* calibrationFile = "test-calibration.coord";
	

	int64_t scalefactor = calcScaling(8,calibrationFile);
	fprintf(stdout,"Scale Factor:\t%" PRId64 "\n",scalefactor);
	
	char* spotprotocol = buildSpot(baseline,
		timeon,
		numpulses,
		isi,
		episodeperiod,
		reps,
		&position,
		scalefactor,
		&centeroffset,
		&trig);
	
	char* targetedprotocol = buildTarget(coordFile,
		baseline,
		timeon,
		numpulses,
		isi,
		iterations,
		episodeperiod,
		reps,
		numpoints,
		scalefactor,
		&centeroffset,
		&trig,
		theta);
	char* gridprotocol = buildGrid(baseline,
		timeon,
		numpulses,
		isi,
		iterations,
		episodeperiod,
		reps,
		&dims,
		&startpos,
		&spacing,
		scalefactor,
		&centeroffset,
		&trig,
		theta);

	switch (type){
		case 3 :
			fprintf(stdout,"TARGETED SEQUENCE:\n%s\n",targetedprotocol);
			break;
		case 2 :
			fprintf(stdout,"GRID PROTOCOL:\n%s\n",gridprotocol);
			break;
		case 1 :
			fprintf(stdout,"SINGLE SPOT PROTOCOL:\n\%s\n",spotprotocol);
			break;
	}
	
    return 0;
}
