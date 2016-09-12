#include <sys/time.h>
#include <unistd.h>
#include <GL/freeglut.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <memory.h>

// external interface
void InitWindow();
int GetSelectedTile();
void SelectUp();
void SelectDown();
void SelectLeft();
void SelectRight();

static bool drawgrid;

// simulation timestep in msecs
// eqv to 30 frames per second
#define SIM_TIMESTEP	32
#define TILE_SIZE	16

static unsigned int realtime;
static unsigned int simframe;
static unsigned int simtime;

// tileset info
static GLuint texobj[1];
static int tilew;
static int tileh;
static int imagew;
static int imageh;

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

static int FileSize(FILE *fp)
{
	int curpos = ftell(fp);
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, curpos, SEEK_SET);

	return size;
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

static void WriteFile(const char *filename, void *data, int numbytes)
{
	FILE *fp = fopen(filename, "wb");
	if(!fp)
		Error("Failed to open file \"%s\"\n", filename);

	fwrite(data, numbytes, 1, fp);
	fclose(fp);
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
	//k = k + strspn(k, " \t\n\v\r\f");
	k = k + strspn(k, "\x20\x09\x0a\x0b\x0c\x0d");

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
	imagew = tilew * TILE_SIZE;
	imageh = tileh * TILE_SIZE;

	// search for the data block
	while(buffer != end && memcmp("data", buffer, 4))
		buffer++;
	if (buffer == end)
		Error("Couldn't find data block");
	buffer += 4;

	FlipRasterOrder(imagew, imageh, buffer);
	MakeTexture(imagew, imageh, (void*)buffer);
}

//________________________________________________________________________________
// Graphics
// the magic 16 constants here are all the map dimensions

static int currentlayer;

// called to place a tile
static int layoutdata[1024];

static void WriteMapData()
{
	WriteFile("maptiles.bin", layoutdata, sizeof(int) * sizeof(layoutdata));
}

static void ReadMapData()
{
	void *buffer;
	ReadFile("maptiles.bin", &buffer);
	memcpy(layoutdata, buffer, sizeof(layoutdata));
}

static void PlaceClick(int x, int y)
{
	// 16 is the size of the map data
	// fixme: need to handle the coordinate systems better
	x /= 32;
	y /= 32;
	//printf("set tile x: %i, y: %i\n", x, y);
	layoutdata[(currentlayer * 16 * 16) + (y * 16) + x] = GetSelectedTile();
}

static int GetTileIndex(int layer, int tilenum)
{
	return layoutdata[(layer * 16 * 16) + tilenum];
}

static void ChangeLayer()
{
	currentlayer = (currentlayer + 1) % 4;
	printf("layer is %i\n", currentlayer);
}

// --------------------------------------------------------------------------------
// Input

enum keyaction_t
{
	ka_left,
	ka_right,
	ka_up,
	ka_down,
	ka_x,
	ka_y,
	NUM_KEY_ACTIONS
};

static bool keyactions[NUM_KEY_ACTIONS];

static void KeyDownFunc(unsigned char key, int x, int y)
{
	if (key == 'x')
		drawgrid = !drawgrid;
	if (key == ' ')
		ChangeLayer();

	if (key == 'o')
		ReadMapData();
	if (key == 'p')
		WriteMapData();

	if (key == 'j')
		SelectRight();
	if (key == 'y')
		SelectUp();
	if (key == 'g')
		SelectLeft();
	if (key == 'h')
		SelectDown();
}



static void KeyUpFunc(unsigned char key, int x, int y)
{
	if (key == 'a')
		keyactions[ka_left] = false;
	if (key == 'd')
		keyactions[ka_right] = false;
	if (key == 'w')
		keyactions[ka_up] = false;
	if (key == 's')
		keyactions[ka_down] = false;
	if (key == 'x')
		keyactions[ka_x] = false;
	if (key == 'z')
		keyactions[ka_y] = false;
}



static void SpecialDownFunc(int key, int x, int y)
{
	if (key == GLUT_KEY_LEFT)
		keyactions[ka_left] = true;
	if (key == GLUT_KEY_RIGHT)
		keyactions[ka_right] = true;
	if (key == GLUT_KEY_UP)
		keyactions[ka_up] = true;
	if (key == GLUT_KEY_DOWN)
		keyactions[ka_down] = true;
}



static void SpecialUpFunc(int key, int x, int y)
{
	if (key == GLUT_KEY_LEFT)
		keyactions[ka_left] = false;
	if (key == GLUT_KEY_RIGHT)
		keyactions[ka_right] = false;
	if (key == GLUT_KEY_UP)
		keyactions[ka_up] = false;
	if (key == GLUT_KEY_DOWN)
		keyactions[ka_down] = false;
}

static void MouseFunc(int button, int state, int x, int y)
{
	//printf("x: %i, y: %i\n", x, y);
	if (button == GLUT_LEFT_BUTTON && state == GLUT_UP)
		PlaceClick(x, 512 - y);
}

static void MouseMotionFunc(int x, int y)
{
	PlaceClick(x, 512 - y);
}

// --------------------------------------------------------------------------------
// Game logic

//
// Map
//

#define LEFT	0
#define RIGHT	1
#define BOTTOMC	2
#define BOTTOML 3
#define BOTTOMR 4
#define TOPC	5
#define TOPL	6
#define TOPR	7

static int offsets[][2] =
{
	{ -4,  0 },
	{  4,  0 },
	{  0, -4 },
	{ -4, -4 },
	{  4, -4 },
	{  0,  4 },
	{ -4,  4 },
	{  4,  4 }
};

// type flags
#define	SOLID	(1 << 0)
#define WATER	(1 << 1)
#define LADDER  (1 << 2)
#define FIELD   (1 << 3)

#if 0
static const char map[] =
"################" \
"#wwwwwwwwwwwwww#" \
"#wwwwwwwwwwwwww#" \
"###########..###" \
"#.............f#" \
"#.1....11.....f#" \
"#....111......f#" \
"#.............f#" \
"#.....111#####f#" \
"#......l......f#" \
"#......l......f#" \
"#......l......f#" \
"#######l##....f#" \
"#......l......f#" \
"#......l......f#" \
"################";
#endif

static const char map[] =
"################" \
"#wwwwwwwwwwwwww#" \
"#wwwwwwwwwwwwww#" \
"###########..###" \
"#.............f#" \
"#.1....11#####f#" \
"#....111######f#" \
"#.....11######f#" \
"#.....111#####f#" \
"#......l......f#" \
"#......l......f#" \
"#......l......f#" \
"#######l##....f#" \
"#......l......f#" \
"#......l......f#" \
"################";

// measured in tiles
static char Map_Tile(float x, float y)
{
	int xx = x / 16;
	int yy = y / 16;

	int addr = yy * 16 + xx;
	return map[addr];
}



// --------------------------------------------------------------------------------
// Rendering

static float *LookupColor(int x, int y)
{
	static float colors[6][3] =
	{
		{ 1, 0, 0 },
		{ 0, 0, 1 },
		{ 1, 1, 1 },
		{ 1, 1, 0 },
		{ 0, 1, 1 },
		{ 0.5, 0, 0 },
	};

	char tile = Map_Tile(x, y);
	if (tile == '#')
		return colors[0];
	else if (tile == 'w')
		return colors[1];
	else if (tile == 'l')
		return colors[3];
	else if (tile == 'f')
		return colors[4];
	else if (tile == '1')
		return colors[5];
	else
		return colors[2];
}

static void DrawCrosshair(int x, int y)
{
	static int s = 2;

	glBegin(GL_LINES);
	glColor3f(0, 0, 0);
	glVertex2f(x - s, y);
	glVertex2f(x + s, y);
	glVertex2f(x, y - s);
	glVertex2f(x, y + s);
	glEnd();
}

// coordinate system is in pixels
static void DrawGrid()
{
	if (!drawgrid)
		return;

	for (int x = 0; x <= 16; x++)
	{
		for (int y = 0; y <= 16; y++)
		{
			// convert from tile coordinates to screen coordinates
			DrawCrosshair(x * 16, y * 16);
		}
	}
}

static void DrawTileTextured(int layer, int x, int y, float color[3])
{
	// tilew, and tileh are the count of the tiles, not the tile size
	const float size = TILE_SIZE;
	const float tcsizex = 1.0f / tilew;
	const float tcsizey = 1.0f / tileh;

	int tileaddr = GetTileIndex(layer, y * 16 + x);
	float tcx = tileaddr % tilew;
	float tcy = tileaddr / tilew;
	tcx *= tcsizex;
	tcy *= tcsizey;

	if (tileaddr != 55)
		glColor3f(1, 1, 1);
	else
	{
		float color = (float)(simframe & 127) / 128.0f;
		color = 0.5f * sin(2.0f * 3.1415f * color) + 0.5f;
		//color += 0.5f;
		glColor3f(color, color, color);
	}

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texobj[0]);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	x *= size;
	y *= size;

	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2f(tcx, tcy);
	glVertex2f(x + 0.0f, y + 0.0f);

	glTexCoord2f(tcx + tcsizex, tcy);
	glVertex2f(x + size, y + 0.0f);

	glTexCoord2f(tcx, tcy + tcsizey);
	glVertex2f(x + 0.0f, y + size);

	glTexCoord2f(tcx + tcsizex, tcy + tcsizey);
	glVertex2f(x + size, y + size);
	glEnd();

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ZERO);
}

static void DrawTile(int layer, int x, int y, float color[3])
{
	DrawTileTextured(layer, x, y, color);
}

static void DrawLayer(int layer)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (layer == 0)
	{
		glBlendFunc(GL_ONE, GL_ZERO);
	}

	for (int i = 0; i < 256; i++)
	{
		int y = i / 16;
		int x = i % 16;

		float *c = LookupColor(x * 16, y * 16);

		DrawTile(layer, x, y, c);
	}
}

static void DrawTiles()
{
	for (int i = 0; i < 4; i++)
		DrawLayer(i);
}

static void ReshapeFunc(int w, int h)
{
	glMatrixMode(GL_PROJECTION);
	glOrtho(0, 256, 0, 256, -1, 1);

	glViewport(0, 0, w, h);
}



static void DisplayFunc()
{
	glClearColor(1, 1, 1, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	DrawTiles();

	DrawGrid();

	glutSwapBuffers();
}
// --------------------------------------------------------------------------------
// Main

unsigned int Sys_Milliseconds (void)
{
	struct timeval	tp;
	static int		secbase;
	static int		curtime;

	gettimeofday(&tp, NULL);

	if (!secbase)
	{
		secbase = tp.tv_sec;
	}

	curtime = (tp.tv_sec - secbase) * 1000 + tp.tv_usec / 1000;

	return curtime;
}



void Sys_Sleep(unsigned int msecs)
{
	usleep(msecs * 1000);
}


static void SimRunFrame()
{
	//printf("===== simrunframe =====\n");
	simframe++;
	simtime = simframe * SIM_TIMESTEP;
}



static void MainLoopFunc()
{
	unsigned int newtime = Sys_Milliseconds();

	// yield the thread if no time has advanced
	if (newtime == realtime)
	{
		Sys_Sleep(0);
		return;
	}

	realtime = newtime;

	// run the simulation code
	if (simtime < realtime)
		SimRunFrame();

	// signal a rendering update
	glutPostRedisplay();
}



int main(int argc, char *argv[])
{
	// glutmain
	glutInit(&argc, argv);
	glutInitWindowSize(512, 512);
	glutCreateWindow("test window");
	glutDisplayFunc(DisplayFunc);
	glutReshapeFunc(ReshapeFunc);
	glutIdleFunc(MainLoopFunc);
	glutKeyboardFunc(KeyDownFunc);
	glutKeyboardUpFunc(KeyUpFunc);
	glutSpecialFunc(SpecialDownFunc);
	glutSpecialUpFunc(SpecialUpFunc);
	glutMouseFunc(MouseFunc);
	glutMotionFunc(MouseMotionFunc);

	LoadTileset();

	// tile window
	InitWindow();

	glutMainLoop();

	return 0;
}


