#include "assert.h"
#include "png.h"
#include "pnginfo.h"
#include <math.h>
#include <algorithm>
#include <string>
#include <vector>
#include <windows.h>

using namespace std;

typedef unsigned int uint;

class Vector2D
{
public:
	Vector2D()
	{
		x = 0;
		y = 0;
	}
	Vector2D(unsigned int x, unsigned int y)
	{
		this->x = x;
		this->y = y;
	}
	unsigned int x;
	unsigned int y;
};

class Rect
{
public:
	Rect()
	{
	}
	Rect(int tlx, int tly, int brx, int bry)
	{
		topLeft.x = tlx;
		topLeft.y = tly;
		bottomRight.x = brx;
		bottomRight.y = bry;
	}
	unsigned int Width() const
	{
		return (bottomRight.x - topLeft.x);
	}
	unsigned int Height() const
	{
		return (bottomRight.y - topLeft.y);
	}

	Vector2D topLeft;
	Vector2D bottomRight;
};

class Node
{
public:
	void Init()
	{
		child[0] = NULL;
		child[1] = NULL;
		occupied = false;
	}
	Node()
	{
		Init();
	}
	Node(Rect box)
	{
		Init();
		boundingBox = box;
	}

	bool IsLeaf()
	{
		return (child[0] == NULL && child[1] == NULL);
	}

	Node* Insert(const Vector2D* insertRect)
	{
		if (!IsLeaf()) {
			// insert below this node.  prefer left child over right.
			if ( insertRect->x <= child[0]->boundingBox.Width() && insertRect->y <= child[0]->boundingBox.Height() ) {
				Node* n = child[0]->Insert(insertRect);
				if (n != NULL) return n;
			}
			return child[1]->Insert(insertRect);
		}
		else {
			// leaf node. check if node is already occupied.
			if (occupied) return NULL;
			// check if rect will fit inside
			if (insertRect->x > boundingBox.Width() || insertRect->y > boundingBox.Height()) return NULL;
			// check if rect fits perfectly
			if (insertRect->x == boundingBox.Width() && insertRect->y == boundingBox.Height()) {
				occupied = true;
				return this;
			}
			// rect fits, but not perfectly.  partition the space.  try to make one partition as large as possible.
			int dw = boundingBox.Width() - insertRect->x;
			int dh = boundingBox.Height() - insertRect->y;
			if (dw > dh) {
				child[0] = new Node(Rect(boundingBox.topLeft.x, boundingBox.topLeft.y, boundingBox.topLeft.x + insertRect->x, boundingBox.bottomRight.y));
				child[1] = new Node(Rect(boundingBox.topLeft.x + insertRect->x, boundingBox.topLeft.y, boundingBox.bottomRight.x, boundingBox.bottomRight.y));
			}
			else {
				child[0] = new Node(Rect(boundingBox.topLeft.x, boundingBox.topLeft.y, boundingBox.bottomRight.x, boundingBox.topLeft.y + insertRect->y));
				child[1] = new Node(Rect(boundingBox.topLeft.x, boundingBox.topLeft.y + insertRect->y, boundingBox.bottomRight.x, boundingBox.bottomRight.y));
			}
			// insert node into first child
			return child[0]->Insert(insertRect);
		}
	}

	void Clear()
	{
		if (child[0]) {
			child[0]->Clear();
			delete child[0]; 
			child[0] = NULL;
		}
		if (child[1]) {
			child[1]->Clear();
			delete child[1];
			child[1] = NULL;
		}
		occupied = false;
	}

public:
	Rect  boundingBox;
	bool  occupied;
	Node* child[2];
};

//#define ERROR 1

enum PngIOState
{
	READ,
	WRITE,
};

struct PngFileInfo
{
	PngIOState State;
	png_structp png_ptr;
	png_infop info_ptr;
	FILE *fp;
};

void ClosePNG(PngFileInfo* PngInfo)
{
	/* Clean up after the read, and free any memory allocated - REQUIRED */
	if (PngInfo->png_ptr != NULL) {
		if (PngInfo->State == READ) {
			png_destroy_read_struct(&PngInfo->png_ptr, &PngInfo->info_ptr, NULL);
		}
		else {
			png_destroy_write_struct(&PngInfo->png_ptr, &PngInfo->info_ptr);
		}
	}

	/* Close the file */
	fclose(PngInfo->fp);
}

// Open the PNG for reading or writing and store info in provided struct
bool OpenPNG(const char* Filename, PngFileInfo* PngInfo, PngIOState IOState )
{
	if ((PngInfo->fp = fopen(Filename, IOState == READ ? "rb" : "wb")) == NULL) {
		printf("Unable to open file");
		return false;
	}

	PngInfo->State = IOState;

	// allocated read/write struct
	if (IOState == READ) {
		PngInfo->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	}
	else {
		PngInfo->png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,  NULL, NULL, NULL);
	}

	if (PngInfo->png_ptr == NULL) {
		fclose(PngInfo->fp);
		return false;
	}

	// Allocate info struct
	PngInfo->info_ptr = png_create_info_struct(PngInfo->png_ptr);
	if (PngInfo->info_ptr == NULL) {
		ClosePNG(PngInfo);
		return false;
	}

	/* Set error handling if you are using the setjmp/longjmp method (this is
	* the normal method of doing things with libpng).  REQUIRED unless you
	* set up your own error handlers in the png_create_read_struct() earlier.
	*/
	if (setjmp(png_jmpbuf(PngInfo->png_ptr))) {
		ClosePNG(PngInfo);
		return false;
	}

	png_init_io(PngInfo->png_ptr, PngInfo->fp);

	return true;
}

// Load the entire image into memory
bool ReadPNG(PngFileInfo* PngInfo)
{
	assert(PngInfo->State == READ);

	// Load the entire image into memory
	png_read_png(PngInfo->png_ptr, PngInfo->info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	return true;
}

static void GetDirFileList(string dir, string filePattern, vector<string>* fileList)
{
	WIN32_FIND_DATA findData;
	HANDLE  findList;
	string searchPattern;
	searchPattern.append(dir);
	searchPattern.append(filePattern);
	findList = FindFirstFile(searchPattern.c_str(), &findData);
	do {
		string filename;
		filename.append(dir);
		filename.append(findData.cFileName);
		fileList->push_back(filename);

	} while (FindNextFile(findList, &findData));

	FindClose(findList);
}

struct ImageInfo
{
	string filename;
	uint width;
	uint height;
};
bool sortAreaGT (ImageInfo i,ImageInfo j) { return (i.width + i.height > j.width + j.height); }
bool sortAreaLT (ImageInfo i,ImageInfo j) { return (i.width + i.height < j.width + j.height); }
bool sortWidthGT (ImageInfo i,ImageInfo j) { return (i.width > j.width); }
bool sortWidthLT (ImageInfo i,ImageInfo j) { return (i.width < j.width); }
bool sortHeightGT (ImageInfo i,ImageInfo j) { return (i.height > j.height); }
bool sortHeightLT (ImageInfo i,ImageInfo j) { return (i.height < j.height); }

int main(int argc, char* argv[])
{
	// Get a list of files
	vector<string> fileList;
	vector<ImageInfo> images;
	GetDirFileList(".\\pack\\", "*.png", &fileList);

	int outBitDepth, outColorType, outInterlaceMethod, outCompressionMethod, outFilterMethod;
	png_colorp outPalette = NULL;
	int outNumPaletteColors;
	png_bytep outTransAlpha = NULL;
	png_color_16p outTransColor = NULL;
	int outNumTrans;
	//png_fixed_point outGamma;

	// Save width and height for each file
	for (uint i=0; i<fileList.size(); i++) {
		PngFileInfo fileInfo;
		bool opened = OpenPNG(fileList[i].c_str(), &fileInfo, READ);
		assert(opened);
		ReadPNG(&fileInfo);

		png_uint_32 width, height;
		int bitDepth, colorType, interlaceMethod, compressionMethod, filterMethod;
		png_get_IHDR(fileInfo.png_ptr, fileInfo.info_ptr, &width, &height, &bitDepth, &colorType, 
			&interlaceMethod, &compressionMethod, &filterMethod);

		// Save off info from first file for use in writing the output file
		if (i==0) {
			// get header info
			outBitDepth = bitDepth;
			outColorType = colorType;
			outInterlaceMethod = interlaceMethod;
			outCompressionMethod = compressionMethod;
			outFilterMethod = filterMethod;

			// get palette
			if (colorType == PNG_COLOR_TYPE_PALETTE) {
				png_colorp palette;
				png_get_PLTE(fileInfo.png_ptr, fileInfo.info_ptr, &palette, &outNumPaletteColors);
				if (outNumPaletteColors > 0) {
					int paletteSizeInBytes = outNumPaletteColors * png_sizeof(png_color);
					outPalette = (png_colorp)malloc(paletteSizeInBytes);
					memcpy(outPalette, palette, paletteSizeInBytes);
				}
			}
			
			png_bytep transAlpha = NULL;
			png_color_16p transColor = NULL;
			png_get_tRNS(fileInfo.png_ptr, fileInfo.info_ptr, &transAlpha, &outNumTrans, &transColor);
			outTransAlpha = (png_bytep)malloc(outNumTrans * sizeof(png_byte));
			outTransColor = (png_color_16p)malloc(outNumTrans * sizeof(png_color_16));
			memcpy(outTransAlpha, transAlpha, outNumTrans * sizeof(png_byte));
			memcpy(outTransColor, transColor, outNumTrans * sizeof(png_color_16));

			//png_get_gAMA_fixed(fileInfo.png_ptr, fileInfo.info_ptr, &outGamma);
		}

		ImageInfo image;
		image.filename = fileList[i];
		image.width = width;
		image.height = height;
		images.push_back(image);

		ClosePNG(&fileInfo);
	}

	// Find sum of areas and max width/height
	uint minArea = 0;
	uint maxWidth = 0;
	uint maxHeight = 0;
	for (uint i=0; i<images.size(); i++) {
		minArea += images[i].width * images[i].height;
		if (images[i].width > maxWidth) maxWidth = images[i].width;
		if (images[i].height > maxHeight) maxHeight = images[i].height;
	}

	// Find the smallest width/height combination to pack into
	Node rootNode;
	rootNode.boundingBox.topLeft.x = 0;
	rootNode.boundingBox.topLeft.y = 0;
	
	int area = minArea;
	bool fitFound = false;
	do {
		// try all integer width/height combinations of the area
		for (uint i=maxWidth; (area/i) >= maxHeight && !fitFound; i++) {
			if (area%i != 0) continue;

			rootNode.boundingBox.bottomRight.x = i;
			rootNode.boundingBox.bottomRight.y = area / i;
		
			// Try six different sorted orderings of images
			for (uint j=0; j<6 && !fitFound; j++) {
				switch (j)
				{
				case 0: sort (images.begin(), images.end(), sortAreaGT); break;
				case 1: sort (images.begin(), images.end(), sortAreaLT); break;
				case 2: sort (images.begin(), images.end(), sortHeightGT); break;
				case 3: sort (images.begin(), images.end(), sortHeightLT); break;
				case 4: sort (images.begin(), images.end(), sortWidthGT); break;
				case 5: sort (images.begin(), images.end(), sortWidthLT); break;
				}
				
				// Insert each piece into the partition tree.  If any piece doesn't fit then bail out.
				fitFound = true;
				for (uint i=0; i<images.size(); i++) {
					Vector2D dim(images[i].width, images[i].height);
					Node* node = rootNode.Insert(&dim);
					if (node == NULL) {
						fitFound = false;
						break;
					}
				}
				//printf("W: %d, H: %d, Area: %d, %s\n", 
				//	rootNode.boundingBox.bottomRight.x, rootNode.boundingBox.bottomRight.y,
				//	rootNode.boundingBox.bottomRight.x * rootNode.boundingBox.bottomRight.y,
				//	fitFound ? "FIT" : "TOO SMALL");

				rootNode.Clear();
			}
		}

		area += 1;

	} while (!fitFound);

	uint outWidth = rootNode.boundingBox.bottomRight.x;
	uint outHeight = rootNode.boundingBox.bottomRight.y;
	
	// Open output file
	PngFileInfo outFileInfo;
	bool opened = OpenPNG("packed.png", &outFileInfo, WRITE);
	assert(opened);

	// Set the output image header info
	png_set_IHDR(outFileInfo.png_ptr, outFileInfo.info_ptr, outWidth, outHeight,
		outBitDepth, outColorType, outInterlaceMethod, outCompressionMethod, outFilterMethod);

	if (outPalette != NULL) {
		png_set_PLTE(outFileInfo.png_ptr, outFileInfo.info_ptr, outPalette, outNumPaletteColors);
	}

	if (outNumTrans > 0) {
		png_set_tRNS(outFileInfo.png_ptr, outFileInfo.info_ptr, outTransAlpha, outNumTrans, outTransColor);
	}

	//png_set_gAMA_fixed(outFileInfo.png_ptr, outFileInfo.info_ptr, outGamma);

	printf("Output image: Width: %d\nHeight: %d\nBitDepth: %d\nColorType: %d\nIsPalette: %d\nNumPaletteColors: %d\n", 
		outWidth, outHeight, outBitDepth, outColorType, outNumPaletteColors > 0, outNumPaletteColors);

	printf("Pack into rectangle: %d x %d, Area: %d, Minimum Area: %d, Wasted space: %.2f percent\n", 
		outWidth, outHeight, outWidth * outHeight, 
		minArea, ((outWidth * outHeight) / (float)minArea) * 100 - 100);

	// allocate memory for output pixels
	int bytesPerPixel = 1;
	png_bytep outPixels = (png_bytep)malloc(outHeight * outWidth * bytesPerPixel);
	png_bytepp outRowPointers = (png_bytepp)malloc(outHeight * sizeof(png_bytep));
	memset(outPixels, outHeight * outWidth * bytesPerPixel, 0);

	if (outHeight > PNG_UINT_32_MAX/png_sizeof(png_bytep)) {
		printf("Image is too tall to process in memory");
	}

	// initialize row pointers for output image
	for (uint i = 0; i < outHeight; i++) {
		outRowPointers[i] = outPixels + i*outWidth*bytesPerPixel;
	}

	// We've found the smallest rectangle possible. Pack the images into that rectange.
	for (uint i=0; i<images.size(); i++) {
		PngFileInfo inFileInfo;
		opened = OpenPNG(images[i].filename.c_str(), &inFileInfo, READ);
		assert(opened);
		ReadPNG(&inFileInfo);

		Vector2D dim(images[i].width, images[i].height);
		Node* node = rootNode.Insert(&dim);
		if (node == NULL) {
			printf("\"%s\" NOT inserted\n", images[i].filename.c_str());
		}
		else {
			printf("\"%s\" inserted at: {%d,%d}, {%d,%d}\n", images[i].filename.c_str(),
				node->boundingBox.topLeft.x, node->boundingBox.topLeft.y,
				node->boundingBox.bottomRight.x, node->boundingBox.bottomRight.y);

			// Write the pixels from input image to output image
			int destCol = max(0, node->boundingBox.topLeft.x);
			int destRow = max(0, node->boundingBox.topLeft.y);
			png_bytepp fromRowPointers = png_get_rows(inFileInfo.png_ptr, inFileInfo.info_ptr);
			for (uint row=0; row<images[i].height; row++) {
				memcpy(outRowPointers[row+destRow]+destCol, fromRowPointers[row], images[i].width);
			}
		}

		ClosePNG(&inFileInfo);
	}
	rootNode.Clear();

	// write out the data to the packed file
	png_write_info(outFileInfo.png_ptr, outFileInfo.info_ptr);
	png_write_image(outFileInfo.png_ptr, outRowPointers);
	png_write_end(outFileInfo.png_ptr, outFileInfo.info_ptr);

	ClosePNG(&outFileInfo);

	if (outPalette != NULL) {
		free(outPalette);
	}
	if (outTransAlpha != NULL) {
		free(outTransAlpha);
	}
	if (outTransColor != NULL) {
		free(outTransColor);
	}
	free(outPixels);
	free(outRowPointers);

	return 0;
}

