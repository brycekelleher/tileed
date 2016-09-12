#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/freeglut.h>

#define TILE_SIZE	16

// ==============================================
// errors and warnings

static void Error(const char *error, ...)
{
	va_list valist;
	char buffer[2048];

	va_start(valist, error);
	vsprintf(buffer, error, valist);
	va_end(valist);

	fprintf(stderr, "\x1b[31m");
	fprintf(stderr, "Error: %s", buffer);
	fprintf(stderr, "\x1b[0m");
	exit(1);
}

static void Warning(const char *warning, ...)
{
	va_list valist;
	char buffer[2048];

	va_start(valist, warning);
	vsprintf(buffer, warning, valist);
	va_end(valist);
	
	fprintf(stderr, "\x1b[33m");
	fprintf(stderr, "Warning: %s", buffer);
	fprintf(stderr, "\x1b[0m");
}

// ==============================================
// Files

static FILE *FileOpen(const char *filename, const char* mode)
{
	FILE *fp = fopen(filename, mode);
	if(!fp)
		Error("Failed to open file \"%s\"\n", filename);

	return fp;
}

static bool FileExists(const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if(!fp)
		return false;
	
	fclose(fp);
	return true;
}

static int FileSize(FILE *fp)
{
	int curpos = ftell(fp);
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, curpos, SEEK_SET);
	
	return size;
}

static void FileClose(FILE *fp)
{
	fclose(fp);
}


static int ReadFile(const char* filename, void **data)
{
	FILE *fp = fopen(filename, "rb");
	if(!fp)
		Error("Failed to open file \"%s\"\n", filename);

	int size = FileSize(fp);
	
	*data = malloc(size);
	fread(*data, size, 1, fp);
	fclose(fp);
	
	return size;
}

static void WriteBytes(void *data, int numbytes, FILE *fp)
{
	fwrite(data, numbytes, 1, fp);
}

// ________________________________________________________________________________ 

// interface
// in keypress
// in mouse click

// out current tile

static GLuint texobj[1];
static int selectedtile;
static int tilew;
static int tileh;
static int windoww;
static int windowh;


// fixme: might be better to draw a texture quad for each tile?
static void DrawTexturedQuad()
{
	float sizex = tilew * TILE_SIZE;
	float sizey = tileh * TILE_SIZE;

	glColor3f(1, 1, 1);
	glBegin(GL_TRIANGLE_STRIP);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(0.0f, 0.0f);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(sizex, 0.0f);

	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(0.0f, sizey);

	glTexCoord2f(1.0f, 1.0f);
	glVertex2f(sizex, sizey);

	glEnd();
}

static void DrawSelectedTile()
{
	int x = selectedtile % tilew;
	int y = selectedtile / tilew;

	float xl = (x + 0) * TILE_SIZE;
	float xr = (x + 1) * TILE_SIZE;
	float yl = (y + 0) * TILE_SIZE;
	float yr = (y + 1) * TILE_SIZE;

	glColor3f(0, 0, 0);

	glBegin(GL_LINE_LOOP);
	glVertex2f(xl, yl);
	glVertex2f(xr, yl);
	glVertex2f(xr, yr);
	glVertex2f(xl, yr);
	glEnd();
}

#if 0
static void DrawSelectedTile()
{
	int x = selectedtile % tilew;
	int y = selectedtile / tilew;

	float xl = (x + 0) * TILE_SIZE;
	float xr = (x + 1) * TILE_SIZE;
	float yl = (y + 0) * TILE_SIZE;
	float yr = (y + 1) * TILE_SIZE;

	float color = 0.5f;
	glColor3f(color, color, color);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glBegin(GL_TRIANGLE_STRIP);
	glVertex2f(xl, yl);
	glVertex2f(xr, yl);
	glVertex2f(xl, yr);
	glVertex2f(xr, yr);
	glEnd();
	
	glDisable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ZERO);
}
#endif

static void MakeTexture(int imagew, int imageh, void *pixels)
{
	glGenTextures(1, texobj);
	glBindTexture(GL_TEXTURE_2D, texobj[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imagew, imageh, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static int KeyInt(const char *data, const char *key)
{
	// find the key
	const char *k = strstr(data, key);
	if (!k)
		Error("Couldn't find key %s\n", key);

	// skip past it
	k = k + strlen(key);
	
	// eat whitespace up to the value
	k = k + strspn(k, " \t\n");

	return atoi(k);
}

static void FlipRasterOrder(int imagew, int imageh, char* pixels)
{
	int start = 0;
	int end = imageh - 1;

	while (start < end)
	{
		// swap the two lines
		for (int i = 0; i < imagew; i++)
		{
			// swap a single pixel
			// swap_pixel(src, dst)
			int addra = start * imagew + i;
			int addrb = end   * imagew + i;
			addra *= 4;
			addrb *= 4;

			// swap the components of the pixel
			// fixme: swap_pixel_data(src, dst, numbytes)
			for (int j = 0; j < 4; j++)
			{
				int temp = pixels[addra + j];
				pixels[addra + j] = pixels[addrb + j];
				pixels[addrb + j] = temp;
			}
		}

		start++;
		end--;
	}
}

static void LoadTileset()
{
	char *buffer = NULL, *end = NULL;
	
	// read the file
	int numbytes = ReadFile("tiles", (void**)&buffer);
	end = buffer + numbytes;

	// decode the key data
	const char *string = (const char*)buffer;
	tilew = KeyInt(string, "tilew");
	tileh = KeyInt(string, "tileh");

	//printf("tilew: %i, tileh: %i\n", tilew, tileh);

	int imagew = tilew * TILE_SIZE;
	int imageh = tileh * TILE_SIZE;

	// search for the data block
	while(buffer != end && memcmp("data", buffer, 4))
		buffer++;
	if (buffer == end)
		Error("Couldn't find data block");
	buffer += 4;

	FlipRasterOrder(imagew, imageh, buffer);
	MakeTexture(imagew, imageh, (void*)buffer);
}


// ________________________________________________________________________________ 
// external interface
// the window is twice the size which automatically scales the texture

void InitWindow();
int GetSelectedTile();
int GetTileIndex(int tilenum);
void SelectClick(int x, int y);
void SelectUp();
void SelectDown();
void SelectLeft();
void SelectRight();




int ClampSelected(int newtile, int oldtile)
{
	//if (newtile < 0 || newtile > tilew * tileh)
	//	return oldtile;

	return newtile;
}

// called to set the current tile in the palette
void SelectClick(int x, int y)
{
	// flip the y coordinate
	y = windowh - y;
	printf("%d, %d\n", x, y);

	// scale x and y since the tile palette is scaled up
	x /= 2;
	y /= 2;

	// convert to tile x, y
	x /= TILE_SIZE;
	y /= TILE_SIZE;

	// convert to tile addr
	selectedtile = y * tilew + x;
	
	printf("tilex: %i, tiley %i, tilenum %i\n", x, y, selectedtile);
	glutPostRedisplay();
}

void SelectUp()
{
	selectedtile = ClampSelected(selectedtile + tilew, selectedtile);
}

void SelectDown()
{
	selectedtile = ClampSelected(selectedtile - tilew, selectedtile);
}

void SelectLeft()
{
	selectedtile = ClampSelected(selectedtile - 1, selectedtile);
}

void SelectRight()
{
	selectedtile = ClampSelected(selectedtile + 1, selectedtile);
}

int GetSelectedTile()
{
	return selectedtile;
}


// ________________________________________________________________________________ 
// GLUT glue functions

static void ReshapeFunc(int w, int h)
{
	windoww = w;
	windowh = h;

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, tilew * TILE_SIZE, 0, tileh * TILE_SIZE, -1, 1);

	glViewport(0, 0, w, h);
}

static void DisplayFunc()
{
	glClearColor(0.3, 0.3, 0.3, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texobj[0]);
	DrawTexturedQuad();
	glDisable(GL_TEXTURE_2D);

	DrawSelectedTile();

	glutSwapBuffers();
	glutPostRedisplay();
}

static void KeyDownFunc(unsigned char key, int x, int y)
{
}

static void KeyUpFunc(unsigned char key, int x, int y)
{
}

static void MouseFunc(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON && state == GLUT_UP)
		SelectClick(x, y);
}

void InitWindow()
{
	LoadTileset();

	glutInitWindowSize(2 * tilew * TILE_SIZE, 2 * tileh * TILE_SIZE);
	glutCreateWindow("tile window");
	glutDisplayFunc(DisplayFunc);
	glutReshapeFunc(ReshapeFunc);
	glutKeyboardFunc(KeyDownFunc);
	glutKeyboardUpFunc(KeyUpFunc);
	glutMouseFunc(MouseFunc);

	// need to load again to get the texture
	LoadTileset();
}


//int main(int argc, char *argv[])
//{
//	LoadTileset();
//}
