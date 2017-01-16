/*

The base of this code was taken from... (thanks again Nehe, RiP)
OpenGL 1.2 PPM (P3 only) viewer, no STL, no glut. Win32 o.0

 *		This Code Was Created By Jeff Molofee 2000
 *		A HUGE Thanks To Fredric Echols For Cleaning Up
 *		And Optimizing The Base Code, Making It More Flexible!
 *		If You've Found This Code Useful, Please Let Me Know.
 *		Visit My Site At nehe.gamedev.net
 */

#include <windows.h>		// Header File For Windows
#include <gl\gl.h>			// Header File For The OpenGL32 Library
#include <gl\glu.h>			// Header File For The GLu32 Library
#include <string.h>
#include <stdio.h>

#include <iostream>

HDC			hDC=NULL;		// Private GDI Device Context
HGLRC		hRC=NULL;		// Permanent Rendering Context
HWND		hWnd=NULL;		// Holds Our Window Handle
HINSTANCE	hInstance;		// Holds The Instance Of The Application

bool	keys[256];			// Array Used For The Keyboard Routine
bool	active=true;		// Window Active Flag Set To TRUE By Default
bool	fullscreen=false;	// Fullscreen Flag Set To Fullscreen Mode By Default
GLint   textureSizeGL = 64;
unsigned int bpp = 32;

char buffer[128];
char* ReadTextFileLineIgnoringComment(FILE* file)
{
    fgets(buffer, 128, file);
    while ( buffer[0] == '#' )
        fgets(buffer, 128, file);
    return feof(file) ? 0 : &buffer[0];
}

class PPMImage
{
public:
    PPMImage(LPSTR filename)
        :   data_(0), width_(0), height_(0)
    {
        FILE* ppmfile = fopen(filename, "r");
        if (ppmfile)
        {
            // First line should be magic
            ReadTextFileLineIgnoringComment(ppmfile);
            if (strncmp(buffer, "P3", 2) == 0)
            {
                // Next should be dimensions, so tokenise
                char* token = new char[10];
                token = strtok(ReadTextFileLineIgnoringComment(ppmfile), " ");
                for (unsigned int i = 0; i < 2; ++i)
                {
                    width_ ? height_ = atoi(token) : width_ = atoi(token);
                    token = strtok(NULL, " ");
                }
                delete token;

                // Next should be the range maximum for the channel.
                unsigned int imageChannelRange = atoi(ReadTextFileLineIgnoringComment(ppmfile));

                // And then the data...
                data_ = new unsigned char[width_ * height_ * 3];
                unsigned char* itr = data_;
                unsigned int gfgf = 0;
                while (ReadTextFileLineIgnoringComment(ppmfile))
                {
                    *itr = static_cast<unsigned char>(atoi(buffer));
                    ++itr;
                    ++gfgf;
                }
            }
            fclose(ppmfile);
        }
    }

    ~PPMImage()
    {
        if (data_)
            delete data_;
    }


    unsigned char* data_;

    unsigned int width_;
    unsigned int height_;
};

unsigned int tileCount_ = 0;

class TextureTile
{
public:
    TextureTile()
        : x_(0), y_(0), width_(0), height_(0), textureID_(0)
    {
    }

    TextureTile(unsigned int x, unsigned int y, unsigned int width, unsigned int height, GLuint textureID)
        : x_(x), y_(y), width_(width), height_(height), textureID_(textureID)
    {
    }

    void DrawTile()
    {
        glBindTexture(GL_TEXTURE_2D, textureID_);
        
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex3i(x_, y_, 0);
        glTexCoord2f(1.0f, 0); glVertex3i(x_ + width_, y_, 0);
        glTexCoord2f(1.0f, 1.0f); glVertex3i(x_ + width_, y_ + height_, 0);
        glTexCoord2f(0, 1.0f); glVertex3i(x_, y_ + height_, 0);
        glEnd();

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GLuint textureID_;
    unsigned int x_;
    unsigned int y_;
    unsigned int width_;
    unsigned int height_;
};

TextureTile* tiles_;



LRESULT	CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);	// Declaration For WndProc

GLvoid ReSizeGLScene(GLsizei width, GLsizei height)		// Resize And Initialize The GL Window
{
	if (height==0)										// Prevent A Divide By Zero By
	{
		height=1;										// Making Height Equal One
	}

	glViewport(0,0,width,height);						// Reset The Current Viewport

	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glLoadIdentity();									// Reset The Projection Matrix
    glOrtho(0, (GLfloat)width, 0, (GLfloat)height, 0.1, 2.0f);

	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glLoadIdentity();									// Reset The Modelview Matrix
}

int InitGL(GLvoid)										// All Setup For OpenGL Goes Here
{
	glShadeModel(GL_SMOOTH);							// Enable Smooth Shading
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);				// Black Background
	glClearDepth(1.0f);									// Depth Buffer Setup
	glEnable(GL_DEPTH_TEST);							// Enables Depth Testing
	glDepthFunc(GL_LEQUAL);								// The Type Of Depth Testing To Do
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations
    
    glEnable(GL_TEXTURE_2D);
    

	return TRUE;										// Initialization Went OK
}

unsigned int GetNext2ToN(unsigned int value)
{
    unsigned int incr = 2^6;
    while (incr < value)
        incr *= 2;
    return incr;
}
bool Is2ToN(unsigned int value)
{
    for (unsigned int n = 0; n < 42; ++n)
    {
        unsigned int toCheck = 2 ^ n;
        if (value == toCheck)
            return true;
        else if (toCheck > value)
            return false;
    }
    return false;
}

void GetImageSubTexture(PPMImage* ppmimage, unsigned int u, unsigned int v, unsigned int textureSizeGL, unsigned char* resultBuffer)
{
    // Calculate the bottom left corner
    unsigned int blc = ((ppmimage->width_ * v) + u) * 3;
    unsigned char black[3] = { 0, 0, 0 };

    unsigned int uMax = (u + textureSizeGL > ppmimage->width_) ? ppmimage->width_ - u : textureSizeGL;
    unsigned int vMax = (v + textureSizeGL > ppmimage->height_) ? ppmimage->height_ - v : textureSizeGL;

    for ( unsigned int iv = 0; iv < textureSizeGL; ++iv )
    {
        for (unsigned int iu = 0; iu < textureSizeGL; ++iu)
        {
            unsigned int index = blc;
            unsigned char* pixel = (iv >= vMax) || (iu >= uMax) ? &black[0] : ppmimage->data_ + blc + (((iv * ppmimage->width_) + iu) * 3);

            for (unsigned int c = 0; c < 3; ++c)
                *(resultBuffer + c + (((iv * textureSizeGL) + iu) * 3)) = *(pixel + c);
        }
    }
}

void LoadGLTexture(PPMImage* ppmimage)
{
    // Check if the image dimensions fit into a single texture.
    if ((ppmimage->width_ <= (unsigned int)textureSizeGL) && (ppmimage->height_ <= (unsigned int)textureSizeGL))
    {
        // If our image is not 2^n scale it
        unsigned int newWidth = Is2ToN(ppmimage->width_) ? ppmimage->width_ : GetNext2ToN(ppmimage->width_);
        unsigned int newHeight = Is2ToN(ppmimage->height_) ? ppmimage->height_ : GetNext2ToN(ppmimage->height_);
        unsigned char* newImageBuffer = new unsigned char[newWidth * newHeight * 3];
        gluScaleImage(GL_RGB, ppmimage->width_, ppmimage->height_, GL_UNSIGNED_BYTE, ppmimage->data_, newWidth, newHeight, GL_UNSIGNED_BYTE, newImageBuffer);

        tileCount_ = 1;
        GLuint textureID = 0;
        glGenTextures(tileCount_, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, newWidth, newHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, newImageBuffer);
        
        delete newImageBuffer;

        tiles_ = new TextureTile(0, 0, ppmimage->width_, ppmimage->height_, textureID);
    }
    else
    {
        // Otherwise we need to split up the texture into tiles
        unsigned int uSize = (unsigned int)(ppmimage->width_ / textureSizeGL) + 1;
        unsigned int vSize = (unsigned int)(ppmimage->height_ / textureSizeGL) + 1;

        tileCount_ = uSize * vSize;
        
        
        tiles_ = new TextureTile[tileCount_];

        unsigned int current = 0;
        for (unsigned int v = 0; v < vSize; ++v)
        {
            for (unsigned int u = 0; u < uSize; ++u)
            {
                unsigned char* imageSubTexture = new unsigned char[textureSizeGL * textureSizeGL * 3];

                GLuint textureID;
                glGenTextures(1, &textureID);
                glBindTexture(GL_TEXTURE_2D, textureID);
                GetImageSubTexture(ppmimage, u * textureSizeGL, v * textureSizeGL, textureSizeGL, imageSubTexture);
                
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSizeGL, textureSizeGL, 0, GL_RGB, GL_UNSIGNED_BYTE, imageSubTexture);
                *(tiles_ + current) = TextureTile(u * textureSizeGL, ppmimage->height_ - (v * textureSizeGL), textureSizeGL, -textureSizeGL, textureID);

                glBindTexture(GL_TEXTURE_2D, 0);
                
                ++current;

                delete imageSubTexture;
            }
        }
    }

    glEnable(GL_TEXTURE_2D);
}

int DrawGLScene(PPMImage* image)									// Here's Where We Do All The Drawing
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear Screen And Depth Buffer

	glLoadIdentity();									// Reset The Current Modelview Matrix
	glTranslatef(0,0,-1.0f);						// Move Left 1.5 Units And Into The Screen 6.0
    glColor3f(1.0f, 1.0f, 1.0f);
   
    
    for (unsigned int tile = 0; tile < tileCount_; ++tile)
        (tiles_+tile)->DrawTile();
	
	return TRUE;										
}

GLvoid KillGLWindow(GLvoid)								// Properly Kill The Window
{
	if (fullscreen)										// Are We In Fullscreen Mode?
	{
		ChangeDisplaySettings(NULL,0);					// If So Switch Back To The Desktop
		ShowCursor(TRUE);								// Show Mouse Pointer
	}

	if (hRC)											// Do We Have A Rendering Context?
	{
		if (!wglMakeCurrent(NULL,NULL))					// Are We Able To Release The DC And RC Contexts?
		{
			MessageBox(NULL,"Release Of DC And RC Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		}

		if (!wglDeleteContext(hRC))						// Are We Able To Delete The RC?
		{
			MessageBox(NULL,"Release Rendering Context Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		}
		hRC=NULL;										// Set RC To NULL
	}

	if (hDC && !ReleaseDC(hWnd,hDC))					// Are We Able To Release The DC
	{
		MessageBox(NULL,"Release Device Context Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hDC=NULL;										// Set DC To NULL
	}

	if (hWnd && !DestroyWindow(hWnd))					// Are We Able To Destroy The Window?
	{
		MessageBox(NULL,"Could Not Release hWnd.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hWnd=NULL;										// Set hWnd To NULL
	}

	if (!UnregisterClass("OpenGL",hInstance))			// Are We Able To Unregister Class
	{
		MessageBox(NULL,"Could Not Unregister Class.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hInstance=NULL;									// Set hInstance To NULL
	}
}

BOOL CreateGLWindow(char* title, int width, int height, int bits, bool fullscreenflag)
{
	GLuint		PixelFormat;			// Holds The Results After Searching For A Match
	WNDCLASS	wc;						// Windows Class Structure
	DWORD		dwExStyle;				// Window Extended Style
	DWORD		dwStyle;				// Window Style
	RECT		WindowRect;				// Grabs Rectangle Upper Left / Lower Right Values
	WindowRect.left=(long)0;			// Set Left Value To 0
	WindowRect.right=(long)width;		// Set Right Value To Requested Width
	WindowRect.top=(long)0;				// Set Top Value To 0
	WindowRect.bottom=(long)height;		// Set Bottom Value To Requested Height

	fullscreen=fullscreenflag;			// Set The Global Fullscreen Flag

	hInstance			= GetModuleHandle(NULL);				// Grab An Instance For Our Window
	wc.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC;	// Redraw On Size, And Own DC For Window.
	wc.lpfnWndProc		= (WNDPROC) WndProc;					// WndProc Handles Messages
	wc.cbClsExtra		= 0;									// No Extra Window Data
	wc.cbWndExtra		= 0;									// No Extra Window Data
	wc.hInstance		= hInstance;							// Set The Instance
	wc.hIcon			= LoadIcon(NULL, IDI_WINLOGO);			// Load The Default Icon
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);			// Load The Arrow Pointer
	wc.hbrBackground	= NULL;									// No Background Required For GL
	wc.lpszMenuName		= NULL;									// We Don't Want A Menu
	wc.lpszClassName	= "OpenGL";								// Set The Class Name

	if (!RegisterClass(&wc))									// Attempt To Register The Window Class
	{
		MessageBox(NULL,"Failed To Register The Window Class.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;											// Return FALSE
	}
	
	if (fullscreen)												// Attempt Fullscreen Mode?
	{
		DEVMODE dmScreenSettings;								// Device Mode
		memset(&dmScreenSettings,0,sizeof(dmScreenSettings));	// Makes Sure Memory's Cleared
		dmScreenSettings.dmSize=sizeof(dmScreenSettings);		// Size Of The Devmode Structure
		dmScreenSettings.dmPelsWidth	= width;				// Selected Screen Width
		dmScreenSettings.dmPelsHeight	= height;				// Selected Screen Height
		dmScreenSettings.dmBitsPerPel	= bits;					// Selected Bits Per Pixel
		dmScreenSettings.dmFields=DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;

		// Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
		if (ChangeDisplaySettings(&dmScreenSettings,CDS_FULLSCREEN)!=DISP_CHANGE_SUCCESSFUL)
		{
			// If The Mode Fails, Offer Two Options.  Quit Or Use Windowed Mode.
			if (MessageBox(NULL,"The Requested Fullscreen Mode Is Not Supported By\nYour Video Card. Use Windowed Mode Instead?","Uh oh",MB_YESNO|MB_ICONEXCLAMATION)==IDYES)
			{
				fullscreen=FALSE;		// Windowed Mode Selected.  Fullscreen = FALSE
			}
			else
			{
				// Pop Up A Message Box Letting User Know The Program Is Closing.
				MessageBox(NULL,"Uh oh, good bye.","ERROR",MB_OK|MB_ICONSTOP);
				return FALSE;									// Return FALSE
			}
		}
	}

	if (fullscreen)												// Are We Still In Fullscreen Mode?
	{
		dwExStyle=WS_EX_APPWINDOW;								// Window Extended Style
		dwStyle=WS_POPUP;										// Windows Style
		ShowCursor(FALSE);										// Hide Mouse Pointer
	}
	else
	{
		dwExStyle=WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;			// Window Extended Style
		dwStyle= (WS_OVERLAPPEDWINDOW | WS_SYSMENU) & ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME);							// Windows Style
	}

	AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);		// Adjust Window To True Requested Size

	// Create The Window
	if (!(hWnd=CreateWindowEx(	dwExStyle,							// Extended Style For The Window
								"OpenGL",							// Class Name
								title,								// Window Title
								dwStyle |							// Defined Window Style
								WS_CLIPSIBLINGS |					// Required Window Style
								WS_CLIPCHILDREN,					// Required Window Style
								0, 0,								// Window Position
								WindowRect.right-WindowRect.left,	// Calculate Window Width
								WindowRect.bottom-WindowRect.top,	// Calculate Window Height
								NULL,								// No Parent Window
								NULL,								// No Menu
								hInstance,							// Instance
								NULL)))								// Dont Pass Anything To WM_CREATE
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Window Creation Error.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	static	PIXELFORMATDESCRIPTOR pfd=				// pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),				// Size Of This Pixel Format Descriptor
		1,											// Version Number
		PFD_DRAW_TO_WINDOW |						// Format Must Support Window
		PFD_SUPPORT_OPENGL |						// Format Must Support OpenGL
		PFD_DOUBLEBUFFER,							// Must Support Double Buffering
		PFD_TYPE_RGBA,								// Request An RGBA Format
		bits,										// Select Our Color Depth
		0, 0, 0, 0, 0, 0,							// Color Bits Ignored
		0,											// No Alpha Buffer
		0,											// Shift Bit Ignored
		0,											// No Accumulation Buffer
		0, 0, 0, 0,									// Accumulation Bits Ignored
		16,											// 16Bit Z-Buffer (Depth Buffer)  
		0,											// No Stencil Buffer
		0,											// No Auxiliary Buffer
		PFD_MAIN_PLANE,								// Main Drawing Layer
		0,											// Reserved
		0, 0, 0										// Layer Masks Ignored
	};
	
	if (!(hDC=GetDC(hWnd)))							// Did We Get A Device Context?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Create A GL Device Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	if (!(PixelFormat=ChoosePixelFormat(hDC,&pfd)))	// Did Windows Find A Matching Pixel Format?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Find A Suitable PixelFormat.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	if(!SetPixelFormat(hDC,PixelFormat,&pfd))		// Are We Able To Set The Pixel Format?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Set The PixelFormat.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	if (!(hRC=wglCreateContext(hDC)))				// Are We Able To Get A Rendering Context?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Create A GL Rendering Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	if(!wglMakeCurrent(hDC,hRC))					// Try To Activate The Rendering Context
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Activate The GL Rendering Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	ShowWindow(hWnd,SW_SHOW);						// Show The Window
	SetForegroundWindow(hWnd);						// Slightly Higher Priority
	SetFocus(hWnd);									// Sets Keyboard Focus To The Window
	ReSizeGLScene(width, height);					// Set Up Our Perspective GL Screen

	if (!InitGL())									// Initialize Our Newly Created GL Window
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Initialization Failed.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	return TRUE;									// Success
}

LRESULT CALLBACK WndProc(	HWND	hWnd,			// Handle For This Window
							UINT	uMsg,			// Message For This Window
							WPARAM	wParam,			// Additional Message Information
							LPARAM	lParam)			// Additional Message Information
{
	switch (uMsg)									// Check For Windows Messages
	{
		case WM_ACTIVATE:							// Watch For Window Activate Message
		{
			if (!HIWORD(wParam))					// Check Minimization State
			{
				active=TRUE;						// Program Is Active
			}
			else
			{
				active=FALSE;						// Program Is No Longer Active
			}

			return 0;								// Return To The Message Loop
		}

		case WM_SYSCOMMAND:
		{
			switch (wParam)
			{
				case SC_SCREENSAVE:
				case SC_MONITORPOWER:
					return 0;
			}
			break;
		}

		case WM_CLOSE:								// Did We Receive A Close Message?
		{
			PostQuitMessage(0);						// Send A Quit Message
			return 0;								// Jump Back
		}

		case WM_KEYDOWN:							// Is A Key Being Held Down?
		{
			keys[wParam] = TRUE;					// If So, Mark It As TRUE
			return 0;								// Jump Back
		}

		case WM_KEYUP:								// Has A Key Been Released?
		{
			keys[wParam] = FALSE;					// If So, Mark It As FALSE
			return 0;								// Jump Back
		}

		case WM_SIZE:								// Resize The OpenGL Window
		{
			ReSizeGLScene(LOWORD(lParam),HIWORD(lParam));  // LoWord=Width, HiWord=Height
			return 0;								// Jump Back
		}
	}

	// Pass All Unhandled Messages To DefWindowProc
	return DefWindowProc(hWnd,uMsg,wParam,lParam);
}

int WINAPI WinMain(	HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG		msg;									// Windows Message Structure
	bool finished = false;
    
    PPMImage* ppmimage = 0;
    char* fn = 0;

    // split command line into tokens.
    char seps[] = " ,\t\n";
    
    // Establish string and get the first token:  
    char* token;
    token = strtok(lpCmdLine, seps); // C4996  
    while (token)
    {
        if (*token == '/')
        {
            if (strncmp(token, "/fullscreen", 12) == 0)
                fullscreen = true;
            else if (strncmp(token, "/bpp=32", 7) == 0)
                bpp = 32;
            else if (strncmp(token, "/bpp=24", 7) == 0)
                bpp = 24;
            else if (strncmp(token, "/bpp=16", 7) == 0)
                bpp = 16;
        }
        else
        {
            fn = new char[strlen(token)];
            strcpy(fn, token);
        }
            
        // Get next token:   
        token = strtok(NULL, seps);
    }

    if (fn)
        ppmimage = new PPMImage(fn);
        
    if (!ppmimage)
    {
        MessageBox(NULL, "No PPM(P3) image specified.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
        return 0;
    }

	// Create Our OpenGL Window 16bpp might be more 9x compliant?
	if (!CreateGLWindow("GL PPM",ppmimage->width_,ppmimage->height_,bpp,fullscreen))
	{
        if (ppmimage)
            delete ppmimage;
		return 0;									// Quit If Window Was Not Created
	}

    if (ppmimage)
    {
        LoadGLTexture(ppmimage);
        delete ppmimage;
    }
        
	while(!finished)									// Loop That Runs While done=FALSE
	{
		if (PeekMessage(&msg,NULL,0,0,PM_REMOVE))	// Is There A Message Waiting?
		{
            if (msg.message == WM_QUIT)				// Have We Received A Quit Message?
                finished = true;
			else									// If Not, Deal With Window Messages
			{
				TranslateMessage(&msg);				// Translate The Message
				DispatchMessage(&msg);				// Dispatch The Message
			}
		}
		else										// If There Are No Messages
		{
			// Draw The Scene.  Watch For ESC Key And Quit Messages From DrawGLScene()
            if ((active && !DrawGLScene(ppmimage)) || keys[VK_ESCAPE])	// Active?  Was There A Quit Received?
                finished = true;
			else									// Not Time To Quit, Update Screen
			{
				SwapBuffers(hDC);					// Swap Buffers (Double Buffering)
			}
		}
	}

    if (tileCount_)
        delete tiles_;
    
	// Shutdown
	KillGLWindow();									// Kill The Window
	return (msg.wParam);							// Exit The Program
}
