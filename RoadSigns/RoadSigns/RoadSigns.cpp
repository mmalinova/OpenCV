// RoadSigns.cpp : Defines the entry point for the application.
//
/**
 * Simple shape detector program.
 * It loads an image and tries to find simple shapes (rectangle, triangle, circle, etc) in it.
 */

#include "framework.h"
#include "RoadSigns.h"
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <iostream>

//////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <Windows.h>
#include <vector>

#include <windef.h>
#include "atlbase.h"
#include "atlstr.h"
#include "comutil.h"
#include "commdlg.h"
#include <Wingdi.h>
#include <gdiplus.h>
//////////////////////
using namespace cv;
using namespace std;
#define MAX_LOADSTRING 100

OPENFILENAME ofn;		//конструктор за четене/писане на файл по име
char FileName[128];		//име на входно изображение
char FileNameOut[128];	//име на изходен файл
char path[128];			//директория на входното изображение
char pathOut[128];		//директория на изходен файл
char Title[128];
wchar_t szFilter[] = _TEXT("All Files(*.*)\0*.*\0\0");

// Global Variables:
const char *names[16] = { "00.png", "01.png", "02.png", "03.png", "04.png", "05.png", "06.png", "07.png", "08.png", "09.png", "10.png", "11.png", "12.png", "13.png", "14.png", "15.png" };
int index = 0;
wchar_t text_buffer[20]; //temporary buffer

HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // the title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name


HBITMAP hbmHBITMAP1 = NULL;			// манипулатор към входното изображение
LPBITMAPINFOHEADER m_lpBMIH1 = NULL;// заглаван част на входното изображение 
BYTE* image1;						// байтов масив на входното изображение 

char* imname;			//указател към име на входен файл
float width, height;	//размери на дъщерен прозорец
Mat image;				//глобален Mat обект за изображение
//променливи за GDIPlus
GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;


/**
 * Helper function to find a cosine of angle between vectors
 * from pt0->pt1 and pt0->pt2
 */
static double angle(cv::Point pt1, cv::Point pt2, cv::Point pt0)
{
	double dx1 = pt1.x - pt0.x;
	double dy1 = pt1.y - pt0.y;
	double dx2 = pt2.x - pt0.x;
	double dy2 = pt2.y - pt0.y;
	return (dx1 * dx2 + dy1 * dy2) / sqrt((dx1 * dx1 + dy1 * dy1) * (dx2 * dx2 + dy2 * dy2) + 1e-10);
}

/**
 * Helper function to display text in the center of a contour
 */
void setLabel(cv::Mat& im, const std::string label, std::vector<cv::Point>& contour)
{
	int fontface = cv::FONT_HERSHEY_SIMPLEX;
	double scale = 0.4;
	int thickness = 1;
	int baseline = 0;

	cv::Size text = cv::getTextSize(label, fontface, scale, thickness, &baseline);
	cv::Rect r = cv::boundingRect(contour);

	cv::Point pt(r.x + ((r.width - text.width) / 2), r.y + ((r.height + text.height) / 2));
	cv::rectangle(im, pt + cv::Point(0, baseline), pt + cv::Point(text.width, -text.height), CV_RGB(255, 255, 255), cv::FILLED);
	cv::putText(im, label, pt, fontface, scale, CV_RGB(0, 0, 0), thickness, 8);
}


long Datasize(LPBITMAPINFOHEADER m_lpBMIH)
{//помощна функция за определяне на размера на масива на изображението
	long sizeImage;
	int p, dump;
	p = 0;
	if (m_lpBMIH->biBitCount == 8) p = 1;
	if (m_lpBMIH->biBitCount == 24) p = 3;
	dump = (m_lpBMIH->biWidth * p) % 4;
	if (dump != 0) dump = 4 - dump;
	sizeImage = m_lpBMIH->biHeight * (m_lpBMIH->biWidth + dump);
	return sizeImage;
}

int Mask(int iSliderValue)
{ //помощна функция за определяне на размер на маска след избор по рулер
	int MaskSize;
	if (iSliderValue < 4)  MaskSize = 3; else
		if (iSliderValue < 6) MaskSize = 5; else
			if (iSliderValue < 8) MaskSize = 7; else
				if (iSliderValue < 10) MaskSize = 9; else MaskSize = 11;
	return MaskSize;
}

char* Uni2char(char* FileNameOut)
{ //функция за трансформация към Уникод
	char* imname0;
	size_t origsize = wcslen((LPWSTR)FileNameOut) + 1;
	size_t convertedChars = 0;
	char strConcat[] = " (char *)";
	size_t strConcatsize = (strlen(strConcat) + 1) * 2;
	const size_t newsize = origsize * 2;
	imname0 = new char[newsize + strConcatsize];
	int errrr = wcstombs_s(&convertedChars, imname0, newsize, (LPWSTR)FileNameOut, _TRUNCATE);
	if (errrr == 0) return imname0; else return 0;
}
void saveimage(HWND hWnd, Mat imResult)
{// помощна процедура за запис на файл с четене на име и преобразуване в уникод
	char* imnameOut;
	wchar_t defex[] = __TEXT("*.*");
	*FileNameOut = 0;
	RtlZeroMemory(&ofn, sizeof ofn);
	ofn.lStructSize = sizeof ofn;
	ofn.hwndOwner = hWnd;
	ofn.hInstance = hInst;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
	ofn.lpstrFilter = (LPWSTR)szFilter;
	ofn.lpstrDefExt = (LPWSTR)defex;
	ofn.lpstrCustomFilter = (LPWSTR)szFilter;


	ofn.lpstrFile = (LPWSTR)FileNameOut;
	ofn.nMaxFile = sizeof FileNameOut;

	ofn.lpstrInitialDir = (LPCWSTR)pathOut;
	if (GetSaveFileName(&ofn) != 0)
	{
		size_t origsize = wcslen((LPWSTR)FileNameOut) + 1;
		size_t convertedChars = 0;
		char strConcat[] = " (char*)";
		size_t strConcatsize = (strlen(strConcat) + 1) * 2;
		const size_t newsize = origsize * 2;
		imnameOut = new char[newsize + strConcatsize];
		int errrr = wcstombs_s(&convertedChars, imnameOut, newsize, (LPWSTR)FileNameOut, _TRUNCATE);
		if (errrr == 0)	imwrite(imnameOut, imResult);
		delete[] imnameOut;
	}
}


HBITMAP LoadFileImage(wchar_t* FileName);			//функция за зареждане на външно изображение
void getDataHBITMAP(HBITMAP hbmHBITMAP);			//процедура за извличане на данните за изображението
BOOL DisplayBmpJPG(HWND hWnd, HBITMAP hbmHBITMAP);  //функция за визуализация на изображението в основния екран


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_ROADSIGNS, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ROADSIGNS));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ROADSIGNS));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_ROADSIGNS);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
			///////////////////////////////////
		case ID_FILE_INPUTIMAGE:
		{//зареждане на изображение след избор на име
			wchar_t defex[] = __TEXT("*.*");
			*FileName = 0;
			RtlZeroMemory(&ofn, sizeof ofn);
			ofn.lStructSize = sizeof ofn;
			ofn.hwndOwner = hWnd;
			ofn.hInstance = hInst;
			ofn.lpstrFile = (LPWSTR)FileName;
			ofn.nMaxFile = sizeof FileName;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
			ofn.lpstrFilter = (LPWSTR)szFilter;
			ofn.lpstrDefExt = (LPWSTR)defex;
			ofn.lpstrCustomFilter = (LPWSTR)szFilter;
			ofn.lpstrInitialDir = (LPCWSTR)path;

			if (GetOpenFileName(&ofn) != 0)
			{
				size_t origsize = wcslen((LPWSTR)FileName) + 1;
				size_t convertedChars = 0;
				char strConcat[] = " (char *)";
				size_t strConcatsize = (strlen(strConcat) + 1) * 2;
				const size_t newsize = origsize * 2;
				imname = new char[newsize + strConcatsize];
				wcstombs_s(&convertedChars, imname, newsize, (LPWSTR)FileName, _TRUNCATE);


				GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
				hbmHBITMAP1 = LoadFileImage((LPWSTR)FileName);
				getDataHBITMAP(hbmHBITMAP1);
				GdiplusShutdown(gdiplusToken);
				image = imread(imname, 1);
				delete[] imname;
			}
			else {}

		}

		break;
		case ID_IMPROC_AVERAGE:
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{

				Mat imResult;  //масив за резултата от обработката

				namedWindow("Averaged", 0); //генериране на прозорец
				resizeWindow("Averaged", int(width), int(height));//ограничаване на размера на прозореца
				//Create trackbar to change masksize
				int iSliderValue1 = 1;
				createTrackbar("kernelSize", "Averaged", &iSliderValue1, 11);
				while (true)
				{
					int MaskSize = Mask(iSliderValue1); //определяне на размера на маската

					blur(image, imResult, Size2i(MaskSize, MaskSize));//филтрация
					imshow("Averaged", imResult);//визуализация на резултат

					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, imResult);
						if (iKey == 27) break;

					}
				}
			}
			break;
		case ID_IMPROC_MEDIAN:
			if (image.dims == 2)// проверка за прехвърлен масив в Mat масив
			{
				Mat imResult; // масив за резултата от обработката

				namedWindow("Median", 0); // генериране на прозорец
				resizeWindow("Median", int(width), int(height));//ограничаване на размера на прозореца
				//Create trackbar to change kernelsize
				int iSliderValue1 = 1;
				createTrackbar("kernelSize", "Median", &iSliderValue1, 11);
				while (true)
				{
					int MaskSize = Mask(iSliderValue1);//определяне на размера на маската
					medianBlur(image, imResult, MaskSize); //филтрация
					imshow("Median", imResult);//визуализация на реултат

					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, imResult);//запис на резултата
						if (iKey == 27) break;

					}
				}

			}
			break;

		case ID_IMPROC_GAUSS:
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{
				Mat imResult;// масив за резултата от обработката
									//		
				namedWindow("GaussBlur", 0);// генериране на прозорец
				resizeWindow("GaussBlur", int(width), int(height));//ограничаване на размера на прозореца
				//Create trackbar to change brightness
				int iSliderValue1 = 1;

				createTrackbar("kernelSize", "GaussBlur", &iSliderValue1, 11);
				while (true)
				{
					int MaskSize = Mask(iSliderValue1);//определяне на размера на маската
					GaussianBlur(image, imResult, Size2i(MaskSize, MaskSize), 0, 0);//филтрация
					imshow("GaussBlur", imResult);//визуализация на реултат

					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, imResult);//запис на резултата
						if (iKey == 27) break;

					}
				}
			}
			break;

		case ID_IMPROC_SOBEL:
			if (image.dims == 2)// проверка за прехвърлен масив в Mat масив
			{
				Mat imResult;// масив за резултата от обработката

				namedWindow("Sobel", 0);// генериране на прозорец
				resizeWindow("Sobel", int(width), int(height));//ограничаване на размера на прозореца
				int b = 1;
				createTrackbar("scale", "Sobel", &b, 100);

				while (true)
				{
					Sobel(image, imResult, image.depth(), 1, 1, 3, b, 0, BORDER_DEFAULT);//диференциране

					imshow("Sobel", imResult);//визуализация на реултат

					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, imResult);//запис на резултата
						if (iKey == 27) break;

					}
				}
			}

			break;

		case ID_IMPROC_LAPLAS:
			if (image.dims == 2)// проверка за прехвърлен масив в Mat масив
			{
				Mat imResult;// масив за резултата от обработката

				namedWindow("Laplacian", 0);// генериране на прозорец
				resizeWindow("Laplacian", int(width), int(height));//ограничаване на размера на прозореца
				int b = 1;
				createTrackbar("scale", "Laplacian", &b, 100);



				while (true)
				{
					Laplacian(image, imResult, image.depth(), 1, b, 0, BORDER_DEFAULT);//диференциране

					imshow("Laplacian", imResult);//визуализация на реултат
					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, imResult);//запис на резултата
						if (iKey == 27) break;

					}
				}
			}
			break;
		case ID_HISTOGRAMS_CALC:
			if (image.dims == 2)// проверка за прехвърлен масив в Mat масив
			{
				int histSize = 256; //задаване на размер на масива на хистограмата
				Mat imResult;  // буферно изображение
				MatND Bh;   //масив за самата хистограма
				image.copyTo(imResult);
				namedWindow("Histogram", 0);   // генериране на прозорец
				int channels[] = { 0, 1, 2, 3 }; // дефиниране на каналите за 32-битово изображение
				calcHist(&imResult, 1, channels, Mat(), Bh, 1, &histSize, 0); //пресмятане на хистограма за първи канал
				Mat histImage = Mat::ones(200, 320, CV_8U) * 255; //ограничаване на матрица за визуализация на хистограмата
				normalize(Bh, Bh, 0, histImage.rows, NORM_MINMAX, CV_32F);//нормализация на данните в хистограмата
				histImage = Scalar::all(255);  //преобразуванв в скаларна стойност
				int binW = cvRound((double)histImage.cols / histSize);//определяне на позиция за визуализация
				for (int i = 0; i < histSize; i++)
					rectangle(histImage, Point2i(i * binW, histImage.rows),
						Point2i((i + 1) * binW, histImage.rows - cvRound(Bh.at<float>(i))),
						Scalar::all(0), -1, 8, 0);
				imshow("Histogram", histImage);//визуализация на хистограмата
				while (true)
				{
					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, histImage);//запис на хистограмата
						if (iKey == 27) break;

					}
				}

			}
			break;
		case ID_HISTOGRAMS_EQUILIZE:
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{
				Mat imResult, img_hist_equalized;  // работни масиви
				cvtColor(image, imResult, COLOR_BGR2GRAY); //преобразуване в сиво изображение
				equalizeHist(imResult, img_hist_equalized);//еквилизация на хистограмата
				namedWindow("Equilized image", 0);//създаване на прозорец
				resizeWindow("Equilized image", int(width), int(height));//ограничаване на прозореца
				imshow("Equilized image", img_hist_equalized);//визуализация на резултата
				while (true)
				{
					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, img_hist_equalized);//запис на резултата
						if (iKey == 27) break;

					}
				}
			}

			break;


		case ID_HISTOGRAMS_COMPARE:
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{
				Mat imResult; //работен масив
				image.copyTo(imResult);
				//избое на име на второто изображение			
				char* imnameOut;
				*FileNameOut = 0;
				ofn.lpstrFile = (LPWSTR)FileNameOut;
				ofn.nMaxFile = sizeof FileNameOut;

				ofn.lpstrInitialDir = (LPCWSTR)pathOut;
				if (GetOpenFileName(&ofn) == 0)
				{
					break;
				}
				else {

					imnameOut = Uni2char(FileNameOut);  //преобразуване на иматео в уникод
					Mat secondIm;    // масив за второто изображение
					secondIm = imread(imnameOut, 1);// зареждане на второто изибражение
					namedWindow("Second Image", 0);  //екран за визуализация на второто изображение
					resizeWindow("Second Image", int(width), int(height));//ограничаване на размера на екрана 
																		//по размерите на първото изображеение

					imshow("Second Image", secondIm);//визуализация на второто изображение
					delete imnameOut;

					int histSize = 256;  //ограничаване на размера на масивите на хистограмите

					float ranges[] = { 0, 256 };

					MatND hist1, hist2;//задаване на масиви за хистограмите

					int channels[] = { 0, 1, 2, 3 };//дефиниране на каналите
					//пресмятане на хистограмите по първия канал

					calcHist(&imResult, 1, channels, Mat(), hist1, 1, &histSize, 0);

					calcHist(&secondIm, 1, channels, Mat(), hist2, 1, &histSize, 0);

					// избор на име на файл за съхраняване на информацията
					errno_t err;
					FILE* fstr;
					*FileNameOut = 0;
					ofn.lpstrFile = (LPWSTR)FileNameOut;
					ofn.nMaxFile = sizeof FileNameOut;
					ofn.lpstrInitialDir = (LPCWSTR)pathOut;
					if (GetSaveFileName(&ofn) != 0)
					{
						char* imname = Uni2char(FileNameOut);

						err = fopen_s(&fstr, imname, "w+");
						if (err == 0)
						{//пресмятане на коефициентите и запис на резултатите
							double self = compareHist(hist1, hist1, HISTCMP_CORREL);// CV_COMP_CORREL);
							double comp = compareHist(hist1, hist2, HISTCMP_CORREL);// CV_COMP_CORREL);
							fprintf_s(fstr, " Method Compare corell: self = %f comp= %f \n", self, comp);
							self = compareHist(hist1, hist1, HISTCMP_CHISQR);// CV_COMP_CHISQR);
							comp = compareHist(hist1, hist2, HISTCMP_CHISQR);// CV_COMP_CHISQR);
							fprintf_s(fstr, " Method Compare chsqr: self = %f comp= %f \n", self, comp);
							self = compareHist(hist1, hist1, HISTCMP_INTERSECT);// CV_COMP_INTERSECT);
							comp = compareHist(hist1, hist2, HISTCMP_INTERSECT);// CV_COMP_INTERSECT);
							fprintf_s(fstr, " Method Compare intersect: self = %f comp= %f \n", self, comp);
							self = compareHist(hist1, hist1, HISTCMP_BHATTACHARYYA);// CV_COMP_BHATTACHARYYA);
							comp = compareHist(hist1, hist2, HISTCMP_BHATTACHARYYA);// CV_COMP_BHATTACHARYYA);
							fprintf_s(fstr, " Method Compare bhattacharyya: self = %f comp= %f \n", self, comp);
							fclose(fstr);
							delete imname;
						}
					}
				}
			}
			break;


		case ID_MORPHOLOGY_ERODE:
			if (image.dims == 2)// проверка за прехвърлен масив в Mat масив
			{
				Mat imResult; // работен масив за резултата от обработката
				image.copyTo(imResult);

				namedWindow("Erode", 0); // генериране на прозорец
				resizeWindow("Erode", int(width), int(height));//ограничаване на размера на прозореца
				//задаване на структурен елемент 
				Mat element = getStructuringElement(MORPH_RECT, Size2i(3, 3), Point2i(1, 1));

				erode(image, imResult, element);//ерозия
				imshow("Erode", imResult);//визуализация на резултата
				while (true)
				{
					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, imResult);//запис на резултата
						if (iKey == 27) break;

					}
				}
			}

			break;
		case ID_MORPHOLOGY_DILATE:
			if (image.dims == 2)// проверка за прехвърлен масив в Mat масив
			{
				Mat imResult;// масив за резултата от обработката
				image.copyTo(imResult);

				namedWindow("Dilate", 0);// генериране на прозорец
				resizeWindow("Dilate", int(width), int(height));//ограничаване на размера на прозореца
				//задаване на структурен елемент 
				Mat element = getStructuringElement(MORPH_RECT, Size2i(3, 3), Point2i(1, 1));

				dilate(image, imResult, element);//дилатация
				imshow("Dilate", imResult);//визуализация на резултата
				while (true)
				{
					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, imResult);//запис на резултата
						if (iKey == 27) break;

					}
				}
			}

			break;

		case ID_MORPHOLOGY_OPEN:
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{
				Mat imResult;// работен масив за резултата от обработката
				image.copyTo(imResult);

				namedWindow("Open", 0);// генериране на прозорец
				resizeWindow("Open", int(width), int(height));//ограничаване на размера на прозореца
				//задаване на структурен елемент
				Mat element = getStructuringElement(MORPH_RECT, Size2i(3, 3), Point2i(1, 1));

				morphologyEx(image, imResult, MORPH_OPEN, element);// операция "отваряне"

				imshow("Open", imResult);
				while (true)
				{
					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, imResult);// визуализация на резултата
						if (iKey == 27) break;

					}
				}
			}
			break;

		case ID_MORPHOLOGY_CLOSE:
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{
				Mat imResult;// работен масив за резултата от обработката
				image.copyTo(imResult);

				namedWindow("Close", 0);// генериране на прозорец
				resizeWindow("Close", int(width), int(height));//ограничаване на размера на прозореца
				//задаване на структурен елемент
				Mat element = getStructuringElement(MORPH_RECT, Size2i(3, 3), Point2i(1, 1));

				morphologyEx(image, imResult, MORPH_CLOSE, element);// опреация по затваряне
				imshow("Close", imResult);// визуализация на резултата
				while (true)
				{
					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, imResult);// визуализация на резултата
						if (iKey == 27) break;

					}
				}
			}
			break;

		case ID_TRANSFORMS_MAKEBINARY:
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{
				Mat imGrayScale, imResult;// работни масиви 
				image.copyTo(imResult);
				cvtColor(imResult, imGrayScale, COLOR_RGB2GRAY);//преобразуване в сиво изображение
				namedWindow("Binary", 0);// генериране на прозорец
				resizeWindow("Binary", int(width), int(height));//ограничаване на размера на прозореца


				//thresholding the grayscale image to get better results
				int treshold = 127;
				createTrackbar("treshold", "Binary", &treshold, 254);// генериране на рулер за праг
				while (true)
				{
					threshold(imGrayScale, imResult, treshold, 255, THRESH_BINARY);// бинаризация
					imshow("Binary", imResult);// визуализация на резултата


					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, imResult);// запис на резултата
						if (iKey == 27) break;

					}
				}
			}
			break;

		case ID_TRANSFORMS_MAKEGRAY:
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{
				Mat gray_image; // работен масив за резултата от обработката
				cvtColor(image, gray_image, COLOR_RGB2GRAY);//преобразуване в сиво изображение

				namedWindow("Gray image", 0);// генериране на прозорец
				resizeWindow("Gray image", int(width), int(height));//ограничаване на размера на прозореца
				imshow("Gray image", gray_image);//визуализация на резултата
				while (true)
				{
					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, gray_image);//запис на резултата
						if (iKey == 27) break;

					}
				}
			}

			break;

		case ID_TRANSFORMS_FOURIER:
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{
				Mat gray_image; // работен масив за резултата от обработката

				cvtColor(image, gray_image, COLOR_RGB2GRAY);//преобразуване в сиво изображение
				namedWindow("Gray image", 0);// генериране на прозорец
				resizeWindow("Gray image", int(width), int(height));//ограничаване на размера на прозореца
				imshow("Gray image", gray_image);//визуализация на резултата
				int M = getOptimalDFTSize(gray_image.rows);
				int N = getOptimalDFTSize(gray_image.cols);
				Mat padded; //създаване на масив с подходящ размер за DFT
				copyMakeBorder(gray_image, padded, 0, M - gray_image.rows, 0, N - gray_image.cols, BORDER_CONSTANT, Scalar::all(0));

				Mat planes[] = { Mat_<float>(padded), Mat::zeros(padded.size(), CV_32F) };
				Mat complexImg;//задаване на комплексен масив
				merge(planes, 2, complexImg);//преобразува няколко едноканални масива в един комплексен

				dft(complexImg, complexImg);//Фурие трансформация

				// compute log(1 + sqrt(Re(DFT(img))**2 + Im(DFT(img))**2))
				split(complexImg, planes); //разделя комплесния масив няколко едноканални масива
				magnitude(planes[0], planes[1], planes[0]); //пресмятане на квадрат на амплитудата (за визуализация)
				Mat mag = planes[0];
				mag += Scalar::all(1);
				log(mag, mag);

				// crop the spectrum, if it has an odd number of rows or columns
				mag = mag(Rect2i(0, 0, mag.cols & -2, mag.rows & -2));

				int cx = mag.cols / 2;
				int cy = mag.rows / 2;

				// rearrange the quadrants of Fourier image
				// so that the origin is at the image center
				//центриране на спектъра в полето за визуализация
				Mat tmp;
				Mat q0(mag, Rect2i(0, 0, cx, cy));
				Mat q1(mag, Rect2i(cx, 0, cx, cy));
				Mat q2(mag, Rect2i(0, cy, cx, cy));
				Mat q3(mag, Rect2i(cx, cy, cx, cy));

				q0.copyTo(tmp);
				q3.copyTo(q0);
				tmp.copyTo(q3);

				q1.copyTo(tmp);
				q2.copyTo(q1);
				tmp.copyTo(q2);

				normalize(mag, mag, 0, 1, NORM_MINMAX);//нормализация на данните
				namedWindow("spectrum magnitude", 0);// генериране на прозорец
				imshow("spectrum magnitude", mag);//визуализация на резултата

			}
			break;

		case ID_DETECTION_CANNYEDGE:
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{
				Mat dst, cdst;// работни масиви
				namedWindow("Canny edge detected", 0);// генериране на прозорец
				resizeWindow("Canny edge detected", int(width), int(height));//ограничаване на размера на прозореца
				int treshold_low = 1;
				createTrackbar("tr_low", "Canny edge detected", &treshold_low, 521);//генериране на рулер за долен праг
				int treshold_up = 255;
				createTrackbar("tr_up", "Canny edge detected", &treshold_up, 522);//генериране на рулер за горен праг
				while (true)
				{

					Canny(image, dst, treshold_low, treshold_up, 3);//оконтуряване
					imshow("Canny edge detected", dst);//визуализация на резултата

					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, dst);//запис на резултата
						if (iKey == 27) break;

					}
				}

			}
			break;

		case ID_DETECTION_HOUGHLINES:
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{
				Mat dst, cdst;// работни масиви
				namedWindow("edge detected lines", 0);// генериране на прозорец
				resizeWindow("edge detected lines", int(width), int(height));//ограничаване на размера на прозореца
				int treshold_low = 1;
				createTrackbar("tr_low", "edge detected lines", &treshold_low, 254);//генериране на рулер за долен праг
				int treshold_up = 255;
				createTrackbar("tr_up", "edge detected lines", &treshold_up, 255);//генериране на рулер за горен праг
				while (true)
				{

					Canny(image, dst, treshold_low, treshold_up, 3);// оконтуряване
					imshow("edge detected lines", dst);
					int iKey = waitKey(50);
					if (iKey == 27)
					{
						break;
					}
				}


				cvtColor(dst, cdst, COLOR_GRAY2BGR);//преобразуване в сиво изображение

				namedWindow("detected lines", 0);// генериране на прозорец
				resizeWindow("detected lines", int(width), int(height));//ограничаване на размера на прозореца

				vector<Vec4i> lines;
				int treshold = 1;
				createTrackbar("treshold", "detected lines", &treshold, 254);//генериране на рулер за праг
				int LL = 255;
				createTrackbar("Line", "detected lines", &LL, 255);//генериране на рулер за дължина

				while (true)
				{
					HoughLinesP(dst, lines, 1, CV_PI / 180, treshold, LL, 5);//Хаф трансформация
					for (size_t i = 0; i < lines.size(); i++)
					{//намиране на търсените елементи
						Vec4i l = lines[i];
						line(cdst, Point2i(l[0], l[1]), Point2i(l[2], l[3]), Scalar(0, 0, 255), 3, LINE_AA);
					}

					imshow("detected lines", cdst);//виизуализация на резултата

					int	iKey = waitKey(50);

					if (iKey == 119) //w
						saveimage(hWnd, cdst);//запис на резултата
					if (iKey == 27) break;


				}

			}
			break;

		case ID_DETECTION_HOUGH32797:
			//откриване на окръжности
			if (image.dims == 2) // проверка за прехвърлен масив в Mat масив
			{
				Mat imResult, dst;// работни масиви
				//		
				cvtColor(image, imResult, COLOR_RGB2GRAY);//преобразуване в сиво изображение
				medianBlur(imResult, imResult, 5);//филтрация чрез усредняване 5х5

				Mat cimg;//работен масив

				cvtColor(imResult, cimg, COLOR_GRAY2BGR);//преобразуване в сиво изображение
				namedWindow("detected circles", 0);// генериране на прозорец
				resizeWindow("detected circles", int(width), int(height));//ограничаване на размера на прозореца
				vector<Vec3f> circles; //вектор на търсените окръжности

				int min_r = 1120;
				createTrackbar("min_r", "detected circles", &min_r, 1120);//генериране на рулер за минимален радиус
				int max_r = 1150;
				createTrackbar("max_r", "detected circles", &max_r, 1150);//генериране на рулер за максимален радиус

				while (true)
				{
					//визуализация на резултата
					if (min_r <= max_r)
					{
						HoughCircles(imResult, circles, HOUGH_GRADIENT, 1, 10, 100, 30, min_r, max_r); //хаф трансформация

						for (size_t i = 0; i < circles.size(); i++)
						{//откриване на търсените елементи
							Vec3i c = circles[i];
							circle(cimg, Point2i(c[0], c[1]), c[2], Scalar(0, 0, 255), 2, LINE_AA);
							circle(cimg, Point2i(c[0], c[1]), 2, Scalar(0, 255, 0), 2, LINE_AA);
						}

						imshow("detected circles", cimg);//визуализация на резултата
					}
					int	iKey = waitKey(50);
					if ((iKey == 27) || (iKey == 119))
					{
						if (iKey == 119) //w
							saveimage(hWnd, imResult);//запис на резултата
						if (iKey == 27) break;

					}
				}

			}
			break;
			/* VideoCapture cap(0);//конструктор за видео */
		case ID_SHAPE_DETECT:
		{
			if (image.empty())
				return -1;

			// Convert to grayscale
			cv::Mat gray;
			cv::cvtColor(image, gray, COLOR_RGB2GRAY);

			// Convert to binary image using Canny
			// use Canny operator rather than thresholding since Canny is able to catch squares with gradient shading
			cv::Mat bw;
			cv::Canny(gray, bw, 0, 50, 5);

			// Find contours
			std::vector<std::vector<cv::Point> > contours;
			cv::findContours(bw.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

			// The array for storing the approximation curve
			std::vector<cv::Point> approx;

			// We'll put the labels in this destination image
			cv::Mat dst = image.clone();

			// Loop through all the contours and get the approximate polygonal curves for each contour
			for (int i = 0; i < contours.size(); i++)
			{
				// Approximate contour with accuracy proportional
				// to the contour perimeter
				cv::approxPolyDP(
					cv::Mat(contours[i]),
					approx,
					cv::arcLength(cv::Mat(contours[i]), true) * 0.02,
					true
				);

				// Skip small or non-convex objects 
				if (std::fabs(cv::contourArea(contours[i])) < 100 || !cv::isContourConvex(approx))
					continue;

				if (approx.size() == 3)
					setLabel(dst, "TRIANGLE", contours[i]);    // Triangles

				else
				{
					// Number of vertices of polygonal curve
					int vtc = approx.size();

					// Get the degree (in cosines) of all corners
					std::vector<double> cos;
					for (int j = 2; j < vtc + 1; j++)
						cos.push_back(angle(approx[j % vtc], approx[j - 2], approx[j - 1]));

					// Sort ascending the corner degree values
					std::sort(cos.begin(), cos.end());

					// Get the lowest and the highest degree
					double mincos = cos.front();
					double maxcos = cos.back();

					// Use the degrees obtained above and the number of vertices
					// to determine the shape of the contour
					if (vtc == 4 && mincos >= -0.1 && maxcos <= 0.3)
					{
						// Detect rectangle or square
						cv::Rect r = cv::boundingRect(contours[i]);
						double ratio = std::abs(1 - (double)r.width / r.height);

						setLabel(dst, ratio <= 0.02 ? "SQUARE" : "RECTANGLE", contours[i]);
					}
					else if (vtc == 5 && mincos >= -0.34 && maxcos <= -0.27)
						setLabel(dst, "PENTAGON", contours[i]);
					else if (vtc == 6 && mincos >= -0.55 && maxcos <= -0.45)
						setLabel(dst, "HEXAGON", contours[i]);
					else {
						vector<vector<cv::Point>> contours_poly(contours.size());

						for (int i = 0; i < contours.size(); i++)
							approxPolyDP(Mat(contours[i]), contours_poly[i], 3, true);

						Mat stopsign_contours = Mat::zeros(image.size(), CV_8UC3);

						
							if (8 == contours_poly[i].size())
								setLabel(dst, "OCTAGON", contours[i]);
							else {
								// Detect and label circles
								double area = cv::contourArea(contours[i]);
								cv::Rect r = cv::boundingRect(contours[i]);
								int radius = r.width / 2;

								if (std::abs(1 - ((double)r.width / r.height)) <= 0.2 &&
									std::abs(1 - (area / (CV_PI * std::pow(radius, 2)))) <= 0.2)
								{
									setLabel(dst, "CIRCLE", contours[i]);
								}
							}
						
					}
				}
				cv::imwrite(names[index], dst);
				index++;
				if (index > 15) index = 0;
			} // end of for() loop

			cv::imshow("src", image);
			cv::imshow("dst", dst);
			cv::waitKey(0);
			
		}
		break;

		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		/*       PAINTSTRUCT ps;
			   HDC hdc = BeginPaint(hWnd, &ps);
			   // TODO: Add any drawing code that uses hdc here...

			  Mat image = Mat::zeros(300, 600, CV_8UC3);
			   circle(image, Point(250, 150), 100, Scalar(0, 255, 128), -100);
			   circle(image, Point(350, 150), 100, Scalar(255, 255, 255), -100);
			   imshow("Display Window", image);
			   EndPaint(hWnd, &ps);*/
		{	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
		if (hbmHBITMAP1 != 0)	DisplayBmpJPG(hWnd, hbmHBITMAP1);
		GdiplusShutdown(gdiplusToken);
		}
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}


HBITMAP LoadFileImage(wchar_t* FileName)
{
	// въвъежда изображение от файл с помощта на GDI+

	Bitmap* pbmp = Bitmap::FromFile(FileName);
	HBITMAP JPGImage;
	if (pbmp == NULL) JPGImage = NULL;
	else
		if (Ok != pbmp->GetHBITMAP(Color::Black, &JPGImage))
			JPGImage = NULL;

	delete pbmp;
	return JPGImage;
}

void getDataHBITMAP(HBITMAP hbmHBITMAP)
{
	//извличане на описанието и матрицата на изображението 
	LPBYTE m_lpImage;
	DIBSECTION ds;

	::GetObject(hbmHBITMAP, sizeof(ds), &ds);
	if (m_lpBMIH1 != NULL) delete[] m_lpBMIH1;
	m_lpBMIH1 = (LPBITMAPINFOHEADER) new char[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
	memcpy(m_lpBMIH1, &ds.dsBmih, sizeof(BITMAPINFOHEADER));
	if (m_lpBMIH1->biSizeImage == 0)
		m_lpBMIH1->biSizeImage = Datasize(m_lpBMIH1);

	// копиране на матрицата на изображението
	m_lpImage = (LPBYTE)ds.dsBm.bmBits;
	// масив с изображението
	image1 = m_lpImage; //на всеки индекс от масива има компонента B, G, R или „прозрачност“
}

BOOL DisplayBmpJPG(HWND hWnd, HBITMAP hbmHBITMAP)
{  //визуализация с GDI на HBITMAP 

	RECT rcDest;
	rcDest.top = 0;
	rcDest.left = 0;
	rcDest.right = m_lpBMIH1->biWidth;
	rcDest.bottom = m_lpBMIH1->biHeight;

	Graphics g(::GetDC(hWnd));

	RECT rc;
	::GetClientRect(hWnd, &rc);

	float scalex = float(rc.right) / float(rcDest.right);
	float scaley = float(rc.bottom) / float(rcDest.bottom);

	width = float(rcDest.right);
	height = float(rcDest.bottom);

	if ((scalex >= 1) && (scaley >= 1)) {
		if (scalex > scaley) { width *= scaley; height *= scaley; }
		if (scalex <= scaley) { width *= scalex; height *= scalex; }
	}
	if ((scalex <= 1) && (scaley > 1)) { width = width * scalex; height = height * scalex; }
	if ((scalex > 1) && (scaley <= 1)) { width = width * scaley; height = height * scaley; }
	if ((scalex < 1) && (scaley < 1)) {
		if (scalex > scaley) { width *= scaley; height *= scaley; }
		if (scalex <= scaley) { width *= scalex; height *= scalex; }
	}
	Bitmap* pb = Bitmap::FromHBITMAP(hbmHBITMAP, NULL);


	//изчертаване на изображението в границите на прозореца
	g.DrawImage(pb, float(rc.left), float(rc.top), width, height);

	delete pb;
	return true;
}