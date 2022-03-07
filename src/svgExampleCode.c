#include <stdio.h>
#include "coursework.h"
#include "linkedlist.h"

void printHeadersSVG();
void printProcessSVG(int iCPUId, struct process *pProcess, struct timeval oStartTime, struct timeval oEndTime);
void printPrioritiesSVG();
void printRasterSVG();
void printFootersSVG();

struct timeval oBaseTime; //declare as a global in main file

void printHeadersSVG()
{
	printf("SVG: <!DOCTYPE html>\n");
	printf("SVG: <html>\n");
	printf("SVG: <body>\n");
	printf("SVG: <svg width=\"10000\" height=\"2000\">\n"); //can change height from 1100 to 2000
}

void printProcessSVG(int iCPUId, struct process *pProcess, struct timeval oStartTime, struct timeval oEndTime)
{
	int iXOffset = getDifferenceInMilliSeconds(oBaseTime, oStartTime) + 30;
	int iYOffsetPriority = (pProcess->iPriority + 1) * 16 - 12;
	int iYOffsetCPU = (iCPUId - 1) * (480 + 50);
	int iWidth = getDifferenceInMilliSeconds(oStartTime, oEndTime);
	printf("SVG: <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"8\" style=\"stroke:rgb(255, 255, 255); stroke-width:1; fill:rgb(%d, 0, %d);\"/>\n", iXOffset /* x */, iYOffsetCPU + iYOffsetPriority /* y */, iWidth, *(pProcess->pPID) - 1 /* rgb */, *(pProcess->pPID) - 1 /* rgb */);
}

void printPrioritiesSVG()
{
	for(int iCPU = 1; iCPU <= NUMBER_OF_CPUS; iCPU++)
	{
		for(int iPriority = 0; iPriority < MAX_PRIORITY; iPriority++)
		{
			int iYOffsetPriority = (iPriority + 1) * 16 - 4;
			int iYOffsetCPU = (iCPU - 1) * (480 + 50);
			printf("SVG: <text x=\"0\" y=\"%d\" fill=\"black\">%d</text>\n", iYOffsetCPU + iYOffsetPriority, iPriority);
		}
	}
}

void printRasterSVG()
{
	for(int iCPU = 1; iCPU <= NUMBER_OF_CPUS; iCPU++)
	{
		for(int iPriority = 0; iPriority < MAX_PRIORITY; iPriority++)
		{
			int iYOffsetPriority = (iPriority + 1) * 16 - 8;
			int iYOffsetCPU = (iCPU - 1) * (480 + 50);
			printf("SVG: <line x1=\"%d\" y1=\"%d\" x2=\"10000\" y2=\"%d\" style=\"stroke:rgb(125, 125, 125); stroke-width:1;\"/>\n", 16, iYOffsetCPU + iYOffsetPriority, iYOffsetCPU + iYOffsetPriority);
		}
	}
}

void printFootersSVG()
{
	printf("SVG: Sorry, your browser does not support inline SVG.\n");
	printf("SVG: </svg>\n");
	printf("SVG: </body>\n");
	printf("SVG: </html>\n");
}
