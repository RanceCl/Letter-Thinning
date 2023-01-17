
/*
	ECE 4310
	Lab 3
	Roderick "Rance" White
	
	This program 
	1) Reads a PPM image, msf image, and ground truth files.
	2) For a range of T, loop through: 
		a) Loop through the ground truth letter locations.
			i)		Check a 9 x 15 pixel area centered at the ground truth location. If
						any pixel in the msf image is greater than the threshold, consider
						the letter “detected”. If none of the pixels in the 9 x 15 area are
						greater than the threshold, consider the letter “not detected”.
			ii) 	If the letter is "not detected" continue to the next letter.
			iii)	Create a 9 x 15 pixel image that is a copy of the area centered at 
						the ground truth location (center of letter) from the original image.
			iv) 	Threshold this image at 128 to create a binary image.
			v) 		Thin the threshold image down to single-pixel wide components.
			vi) 	Check all remaining pixels to determine if they are branchpoints or 
						endpoints.
			vii) 	If there are not exactly 1 branchpoint and 1 endpoint, do not 
						further consider this letter (it becomes "not detected").
		b) Count up the number of FP (letters detected that are not 'e') and TP (number of 
			 letters detected that are 'e').
		c) Output the total FP and TP for each T. 


	The program also demonstrates how to time a piece of code.
	To compile, must link using -lrt  (man clock_gettime() function).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

unsigned char *Image_Read(char *FileName, char *header, int *r, int *c, int *b);
void Image_Write(unsigned char *image, char *FileName, int ROWS, int COLS);
void Find_Range_Boundaries(int RMin, int RMax, int *RangeLow, int *RangeHigh);
unsigned char *Image_Copy_Section(unsigned char *image, int ROWS, int COLS, 
	int CoordRow, int CoordCol, int WindRow, int WindCol);
unsigned char *Threshold_Image(unsigned char *image, int ROWS, int COLS, 
	int CoordRow, int CoordCol, int WindRow, int WindCol, int T, int *isDetected);
void Thinning_Algorithm(unsigned char *image, int ROWS, int COLS);
int Edge_Features(unsigned char *image, int ROWS, int COLS);
void ROC_Evalutation(char *FileName, char SearchedLetter, unsigned char *InImage, 
	unsigned char *MSF, int ROWS, int COLS, int TROWS, int TCOLS, int TMin, int TMax);
	

int main()
{
	unsigned char		*InputImage, *MSFImage;
//	unsigned char 	*NormalizedImage;
	char						header[320];
	int							ROWS,COLS,BYTES;						// The information for the input image
	int							MSFROWS,MSFCOLS,MSFBYTES;		// The information for the MSF image
	int 						RangeLow, RangeHigh;				// The boundaries of the range
	
	
//unsigned char 	*InputCharSect, *Matches;
//int isDetected;
	
	/* Read in the images */
	InputImage = Image_Read("parenthood.ppm", header, &ROWS, &COLS, &BYTES);
	MSFImage = Image_Read("msf_e.ppm", header, &MSFROWS, &MSFCOLS, &MSFBYTES);
	
	//Find_Range_Boundaries(0, 255, &RangeLow, &RangeHigh);
	RangeLow = 100; RangeHigh = 100;
	//printf("Threshold range of %i and %i.\n", RangeLow, RangeHigh);
	
	/* ROC Evaluation */
	ROC_Evalutation("parenthood_gt.txt", 'e', InputImage, MSFImage, ROWS, COLS, 15, 
		9, RangeLow, RangeHigh);
	
}



/* This function serves to read and open the image, given it's name. 
 * It will return the values within the file, the number of rows, the number of columns, and
 * the number of bytes.
 */
unsigned char *Image_Read(char *FileName, char *header, int *r, int *c, int *b)
{
	FILE						*fpt;
	unsigned char		*image;
	int							ROWS,COLS,BYTES;
	
	/* read image */
	if ((fpt=fopen(FileName,"rb")) == NULL)
	{
		printf("Unable to open %s for reading\n", FileName);
		exit(0);
	}

	/* read image header (simple 8-bit greyscale PPM only) */
	fscanf(fpt,"%s %d %d %d",header,&COLS,&ROWS,&BYTES);	
	if (strcmp(header,"P5") != 0  ||  BYTES != 255)
	{
		printf("Not a greyscale 8-bit PPM image\n");
		exit(0);
	}
	
	/* allocate dynamic memory for image */
	image=(unsigned char *)calloc(ROWS*COLS,sizeof(unsigned char));
	header[0]=fgetc(fpt);	/* read white-space character that separates header */
	fread(image,1,COLS*ROWS,fpt);
	fclose(fpt);
	
	*r = ROWS;		//Return number of rows for the image
	*c = COLS;		//Return number of columns for the image
	*b = BYTES;
	
	return image;
}


/* This function serves to write the image to the appropriate file for more concise code */
void Image_Write(unsigned char *image, char *FileName, int ROWS, int COLS)
{
	FILE						*fpt;
	fpt=fopen(FileName,"w");
	fprintf(fpt,"P5 %d %d 255\n",COLS,ROWS);
	fwrite(image,COLS*ROWS,1,fpt);
	fclose(fpt);
}

/* Function for a user to input the lower and upper bounds of a range, given a range to pick from */
void Find_Range_Boundaries(int RMin, int RMax, int *RangeLow, int *RangeHigh)
{

	int RLow, RHigh, T;
	printf("Please two numbers between %i and %i to act as boundaries for the threshold: \n", 
		RMin, RMax);
	scanf("%i ", &RLow);
	scanf("%i", &RHigh);
	printf("\n");
	
	//Change minimum threshold if it is beyond possible range
	if(RLow<RMin || RLow>RMax)
	{
		printf("Minimum is not possible, defaulting to %i.\n", RMin);
		RLow=RMin;
	}
	
	//Change maximum threshold if it is beyond possible range
	if(RHigh<RMin || RHigh>RMax)
	{
		printf("Maximum is not possible, defaulting to %i.\n", RMax);
		RHigh=RMax;
	}
	
	//Switch the min and max if the min is greater than the max
	if(RLow > RHigh)
	{
		T=RLow;
		RLow=RHigh;
		RHigh=T;
	}
	
	*RangeLow = RLow;			//Return beginning of the range
	*RangeHigh = RHigh;		//Return the end of the range
	
}



/* This function serves to copy a portion of an image based on the centered coordinate and the
 * column and row width given.
 */
unsigned char *Image_Copy_Section(unsigned char *image, int ROWS, int COLS, 
	int CoordRow, int CoordCol, int WindRow, int WindCol)
{
	unsigned char *ImageSection;
	int r, c, Wr, Wc;

	Wr = WindRow/2;			//Half the size of the template rows.
	Wc = WindCol/2;			//Half the size of the template colunms.
	
	ImageSection=(unsigned char *)calloc(WindRow*WindCol,sizeof(unsigned char));
	
	//Read in the window of the image
	for(r=0; r<WindRow; r++)
	{
		for(c=0; c<WindCol; c++)
		{
			//Put in the values of the image at the corresponding coordinates
			ImageSection[r*WindCol+c] = image[(r+CoordRow-Wr)*COLS+(c+CoordCol-Wc)];
		}
	}
	return ImageSection;
}

/* Function to threshold an image */
unsigned char *Threshold_Image(unsigned char *image, int ROWS, int COLS, 
	int CoordRow, int CoordCol, int WindRow, int WindCol, int T, int *isDetected)
{
	unsigned char *Matches;
	int r, c, Wr, Wc;

	Wr = WindRow/2;			//Half the size of the template rows.
	Wc = WindCol/2;			//Half the size of the template colunms.
	
	Matches=(unsigned char *)calloc(ROWS*COLS,sizeof(unsigned char));
	*isDetected = 0; 		//Reset to 0
	
	//Check the 9 x 15 pixel area to check if any pixel in the msf image is > than the threshold
	for(r=0; r<WindRow; r++)
	{
		for(c=0; c<WindCol; c++)
		{
			//Indicate if the system response detects the letter
			//Only execute the following if a letter is detected
			if(image[(r+CoordRow-Wr)*COLS+(c+CoordCol-Wc)]>=T)
			{
				*isDetected = 1;
				Matches[r*WindCol+c] = 255;
			}
			else
				Matches[r*WindCol+c] = 0;
		}
	}
	return Matches;
}


void Thinning_Algorithm(unsigned char *image, int ROWS, int COLS)
{

	int r, c, ir, ic, i;
	int Transitions, Neighbors, Surrounded, Erased;
	unsigned char 	*Eraser;
	unsigned char 	*Pixels;													//The 3x3 array of pixels
	int PixelIndex[] = {0, 1, 2, 5, 8, 7, 6, 3, 0};		//For the clockwise order of the pixels
// 		0 1 2
//		3 4 5
//		6 7 8

	Eraser=(unsigned char *)calloc(ROWS*COLS,sizeof(unsigned char));
	
	/* Thin the edges until no more pixels are erased */
	while(1)
	{
		Erased = 0; 			//Reset every loop through the image
		for(c=0;c<COLS;c++)
		{
			for(r=0;r<ROWS;r++)
			{
				/* Look at each pixel that is on */
				if(image[r*COLS+c] == 0)
				{
					Pixels = Image_Copy_Section(image, ROWS, COLS, r, c, 3, 3);
					
					//Reset tests
					Transitions = 0;
					Neighbors = 0;
					Surrounded = 1;
					
					/* Place the indexes of the pixels in the correct order */
					for(ic=0;ic<3;ic++)
					{
						for(ir=0;ir<3;ir++)
						{
							//Out of bounds spaces are off 
							if(((ic==0&&c==0) || (ic==2&&c==COLS-1)) || ((ir==0&&r==0) || (ir==2&&r==ROWS-1)))
								Pixels[ir*3+ic]=255;
						}
					}
					
					/* Compare the different pixels to determine edge values */
					for(i=0;i<8;i++)
					{
						//Edge to non-edge transition occurs when an on value transitions to an off value
						if(Pixels[PixelIndex[i]] < Pixels[PixelIndex[i+1]]) Transitions++;
						
						//Edge neighbors occur when a pixel is on next to the middle pixel
						if(Pixels[PixelIndex[i]] == Pixels[4]) Neighbors++;
					}
					
					//Check if at least one of North, East, or (West and South) are not edges
					if(Pixels[1]!=Pixels[4] || Pixels[5]!=Pixels[4] || (Pixels[7]!=Pixels[4] 
						&& Pixels[3]!=Pixels[4])) Surrounded=0;
					
					//Determine if the pixel should be erased
					if(Transitions==1 && 2<=Neighbors && Neighbors<=6 && Surrounded==1)
					{
						Erased++;
						Eraser[r*COLS+c] = 1;
					}
					else
						Eraser[r*COLS+c] = 0;
				}
			}
		}
		if(Erased == 0) break;		//Break out of the loop if no more pixels are erased

		/* Erase the pixels */
		for(c=0;c<COLS;c++) for(r=0;r<ROWS;r++) if(Eraser[r*COLS+c]==1) image[r*COLS+c]=255;
	}
}






int Edge_Features(unsigned char *image, int ROWS, int COLS)
{

	int r, c, ir, ic, i;
	int Transitions, Endings=0, Branches=0;
	unsigned char 	*Pixels;													//The 3x3 array of pixels
	int PixelIndex[] = {0, 1, 2, 5, 8, 7, 6, 3, 0};		//For the clockwise order of the pixels
// 		0 1 2
//		3 4 5
//		6 7 8

	for(c=0;c<COLS;c++)
	{
		for(r=0;r<ROWS;r++)
		{
			/* Look at each pixel that is on */
			if(image[r*COLS+c] == 0)
			{
				Pixels = Image_Copy_Section(image, ROWS, COLS, r, c, 3, 3);
				Transitions = 0;

				/* Place the indexes of the pixels in the correct order */
				for(ic=0;ic<3;ic++)
					for(ir=0;ir<3;ir++)
						//Out of bounds spaces are off 
						if(((ic==0&&c==0) || (ic==2&&c==COLS-1)) || ((ir==0&&r==0) || (ir==2&&r==ROWS-1)))
							Pixels[ir*3+ic]=255;
				/* Checks how many transitions are present */
				for(i=0;i<8;i++)
					//Edge to non-edge transition occurs when an on value transitions to an off value
					if(Pixels[PixelIndex[i]] < Pixels[PixelIndex[i+1]]) Transitions++;
				
				if(Transitions == 1) Endings++;	//If there is only one transition, it is an endpoint
				if(Transitions > 2) Branches++;	//If there is more than one transition, it is a branchpoint
			}
		}
	}
		if(Endings==1 && Branches==1) return 1;			//Consider the letter, only if 1 end and branch
		else return 0;
}

void ROC_Evalutation(char *FileName, char SearchedLetter, unsigned char *InImage, 
	unsigned char *MSF, int ROWS, int COLS, int TROWS, int TCOLS, int TMin, int TMax)
{
	FILE	*fpt, *ROCTable;
//	int *Matches;
	int	i,tp,tn,fp,fn;
	int	isDetected;
	char gt;
	int GTRow, GTCol;
	unsigned char		*InputCharSect, *BinaryImage, *Matches;
	
	
	int Wr, Wc, T;
	Wr = TROWS/2;		//Half the size of the template rows.
	Wc = TCOLS/2;		//Half the size of the template colunms.

	//Open a spreadsheet for the ROC curve inputs
	ROCTable=fopen("ROCTable.csv","w");
	fprintf(ROCTable, "Threshold,TP,TN,FP,FN,TPR,FPR,PPV,sensitivity,specificity\n");

	/* Loop through threshold T values to find the best */
	for(T=TMin; T<=TMax; T++)
	{	
	tp=tn=fp=fn=0;
	
	/* read image */
	if ((fpt=fopen(FileName,"r")) == NULL)
	{
		printf("Unable to open %s for reading\n", FileName);
		exit(0);
	}
	
	/* Create truth table for if a letter is detected */
	while (1)
	{
		i=fscanf(fpt,"%s %d %d",&gt, &GTCol, &GTRow);		//Scan the letter and coordinates of each line
		if (i != 3)
			break;																				//Break out of while loop if end is reached
		//Check the 9 x 15 pixel area to check if any pixel in the msf image is > than the threshold
		BinaryImage = Threshold_Image(InImage, ROWS, COLS, GTRow, GTCol, 
			TROWS, TCOLS, T, &isDetected);

		//Only execute the following if a letter is detected
		if(isDetected==1)
		{
			InputCharSect = Image_Copy_Section(InImage, ROWS, COLS, GTRow, GTCol, TROWS, TCOLS);
			Matches = Threshold_Image(InputCharSect, TROWS, TCOLS, Wr, Wc, TROWS, TCOLS, 128, &isDetected);
			Thinning_Algorithm(Matches, 15, 9);
			isDetected=Edge_Features(Matches, 15, 9);
		}
		 
		if (gt == SearchedLetter  &&  isDetected == 1) tp++;
		else if (gt != SearchedLetter  &&  isDetected == 0) tn++;
		else if (gt == SearchedLetter  &&  isDetected == 0) fn++;
		else if (gt != SearchedLetter  &&  isDetected == 1) fp++;
	}

	fclose(fpt);
	//printf("TP\tTN\tFP\tFN\tTPR\t\tFPR\t\tPPV\n");
	fprintf(ROCTable, "%d,%d,%d,%d,%d,%lf,%lf,%lf,%lf,%lf\n",T,tp,tn,fp,fn,
		(double)tp/(double)(tp+fn),(double)fp/(double)(fp+tn),(double)fp/(double)(tp+fp),
		(double)tp/(double)(tp+fn),1.0-(double)fp/(double)(fp+tn));
	}
	fclose(ROCTable);
	Image_Write(InputCharSect, "1_ImageSection.ppm", TROWS, TCOLS);
	Image_Write(BinaryImage, "2_MSFWindow.ppm", TROWS, TCOLS);
	Image_Write(Matches, "3_BinaryWindow.ppm", TROWS, TCOLS);

}





