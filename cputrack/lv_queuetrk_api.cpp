/*
Labview API for the functionality in QueuedTracker.h
*/
#include "std_incl.h"
#include "utils.h"
#include "labview.h"
#include "QueuedTracker.h"

#include "jpeglib.h"
#include "TeLibJpeg\jmemdstsrc.h"



#include "lv_prolog.h"
struct QueueImageParams {
	uint locType;
	uint frame;
	vector3f initialPos;
	uint zlutIndex; // or bead#
	uint zlutPlane; // for ZLUT building

	LocalizeType LocType()  { return (LocalizeType)locType; }
};
#include "lv_epilog.h"


MgErr FillErrorCluster(MgErr err, const char *message, ErrorCluster *error)
{
	if (err)
	{
		int msglen = strlen(message);
		error->status = LVBooleanTrue;
		error->code = err;
		err = NumericArrayResize(uB, 1, (UHandle*)&(error->message), msglen);
		if (!err)
		{
			MoveBlock(message, LStrBuf(*error->message), msglen);
			LStrLen(*error->message) = msglen;
		} 
	}
	return err;
}

void ArgumentErrorMsg(ErrorCluster* e, const std::string& msg) {
	FillErrorCluster(mgArgErr, msg.c_str(), e);
}

CDLL_EXPORT void DLL_CALLCONV qtrk_set_ZLUT(QueuedTracker* tracker, LVArray3D<float>** pZlut)
{
	LVArray3D<float>* zlut = *pZlut;

	int numLUTs = zlut->dimSizes[0];
	int planes = zlut->dimSizes[1];
	int res = zlut->dimSizes[2];

	dbgprintf("Setting ZLUT size: %d beads, %d planes, %d radialsteps\n", numLUTs, planes, res);
	tracker->SetZLUT(zlut->elem, numLUTs, planes, res);
}

CDLL_EXPORT void DLL_CALLCONV qtrk_get_ZLUT(QueuedTracker* tracker, LVArray3D<float>** pzlut)
{
	int dims[3];

	float* zlut = tracker->GetZLUT(&dims[0], &dims[1], &dims[2]);
	ResizeLVArray3D(pzlut, dims[0], dims[1], dims[2]);
	memcpy((*pzlut)->elem, zlut, sizeof(float)*(*pzlut)->numElem());
	delete[] zlut;
}

CDLL_EXPORT QueuedTracker* qtrk_create(QTrkSettings* settings)
{
	QueuedTracker* tracker = CreateQueuedTracker(settings);
	tracker->Start();
	return tracker;
}


CDLL_EXPORT void qtrk_destroy(QueuedTracker* qtrk)
{
	delete qtrk;
}

template<typename T>
bool CheckImageInput(QueuedTracker* qtrk, LVArray2D<T> **data, ErrorCluster  *error)
{
	if (!data) {
		ArgumentErrorMsg(error, "Image data array is empty");
		return false;
	} else if( (*data)->dimSizes[1] != qtrk->cfg.width || (*data)->dimSizes[0] != qtrk->cfg.height ) {
		ArgumentErrorMsg(error, SPrintf( "Image data array has wrong size (%d,%d). Should be: (%d,%d)", (*data)->dimSizes[1], (*data)->dimSizes[0], qtrk->cfg.width, qtrk->cfg.height));
		return false;
	}
	return true;
}

CDLL_EXPORT void qtrk_queue_u16(QueuedTracker* qtrk, ErrorCluster* error, LVArray2D<ushort>** data, QueueImageParams* params)
{
	if (CheckImageInput(qtrk, data, error))
		qtrk->ScheduleLocalization( (uchar*)(*data)->elem, sizeof(ushort)*(*data)->dimSizes[1], QTrkU16, params->LocType(), params->frame, &params->initialPos, params->zlutIndex, params->zlutPlane);
}

CDLL_EXPORT void qtrk_queue_u8(QueuedTracker* qtrk, ErrorCluster* error, LVArray2D<uchar>** data, QueueImageParams* params)
{
	if (CheckImageInput(qtrk, data, error))
		qtrk->ScheduleLocalization( (*data)->elem, sizeof(uchar)*(*data)->dimSizes[1], QTrkU8, params->LocType(), params->frame, &params->initialPos, params->zlutIndex, params->zlutPlane);
}

CDLL_EXPORT void qtrk_queue_float(QueuedTracker* qtrk, ErrorCluster* error, LVArray2D<float>** data, QueueImageParams* params)
{
	if (CheckImageInput(qtrk, data, error))
		qtrk->ScheduleLocalization( (uchar*) (*data)->elem, sizeof(float)*(*data)->dimSizes[1], QTrkFloat, params->LocType(), params->frame, &params->initialPos, params->zlutIndex, params->zlutPlane);
}


CDLL_EXPORT void qtrk_queue_pitchedmem(QueuedTracker* qtrk, uchar* data, int pitch, uint pdt, QueueImageParams* params)
{
	qtrk->ScheduleLocalization(data, pitch, (QTRK_PixelDataType)pdt, params->LocType(), params->frame, &params->initialPos, params->zlutIndex, params->zlutPlane);
}

CDLL_EXPORT void qtrk_queue_array(QueuedTracker* qtrk,  ErrorCluster* error,LVArray2D<uchar>** data,uint pdt,  QueueImageParams* params)
{
	uint pitch;

	if (pdt == QTrkFloat) 
		pitch = sizeof(float);
	else if(pdt == QTrkU16) 
		pitch = 2;
	else pitch = 1;

	if (!CheckImageInput(qtrk, data, error))
		return;

	pitch *= (*data)->dimSizes[1]; // LVArray2D<uchar> type works for ushort and float as well
//	dbgprintf("zlutindex: %d, zlutplane: %d\n", zlutIndex,zlutPlane);
	qtrk_queue_pitchedmem(qtrk, (*data)->elem, pitch, pdt, params);
}

CDLL_EXPORT void qtrk_clear_results(QueuedTracker* qtrk)
{
	qtrk->ClearResults();
}


CDLL_EXPORT int qtrk_hasfullqueue(QueuedTracker* qtrk) 
{
	return qtrk->IsQueueFilled() ? 1 : 0;
}

CDLL_EXPORT int qtrk_resultcount(QueuedTracker* qtrk)
{
	return qtrk->GetResultCount();
}

CDLL_EXPORT void qtrk_flush(QueuedTracker* qtrk)
{
	qtrk->Flush();
}

static bool compareResultsByID(const LocalizationResult& a, const LocalizationResult& b) {
	return a.id<b.id;
}

CDLL_EXPORT int qtrk_get_results(QueuedTracker* qtrk, LocalizationResult* results, int maxResults, int sortByID)
{
	int resultCount = qtrk->PollFinished(results, maxResults);

	if (sortByID) {
		std::sort(results, results+resultCount, compareResultsByID);
	}

	return resultCount;
}

CDLL_EXPORT int qtrk_idle(QueuedTracker* qtrk)
{
	return qtrk->IsIdle() ? 1 : 0;
}

CDLL_EXPORT void DLL_CALLCONV qtrk_generate_test_image(QueuedTracker* tracker, LVArray2D<ushort>** image, float xp, float yp, float size, float photoncount)
{
	int w=tracker->cfg.width, h =tracker->cfg.height;
	ResizeLVArray2D(image, h,w);
	
	float *d = new float[w*h];
	tracker->GenerateTestImage(d, xp, yp, size, photoncount );
	floatToNormalizedInt((*image)->elem, d, w,h, (ushort)((1<<16)-1));
	delete[] d;
}

CDLL_EXPORT void DLL_CALLCONV qtrk_generate_image_from_lut(LVArray2D<float>** image, LVArray2D<float>** lut, float LUTradius, vector2f* position, float z, float M, float photonCountPP)
{
	ImageData img((*image)->elem, (*image)->dimSizes[1], (*image)->dimSizes[0]);
	ImageData zlut((*lut)->elem, (*lut)->dimSizes[1], (*lut)->dimSizes[0]);

	GenerateImageFromLUT(&img, &zlut, LUTradius, *position, z, M);
	img.normalize();
	if(photonCountPP>0)
		ApplyPoissonNoise(img, photonCountPP);
}




struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */
};


CDLL_EXPORT int DLL_CALLCONV qtrk_read_jpeg_from_file(const char* filename, LVArray2D<uchar>** dstImage)
{
	int w,h;

	FILE *f = fopen(filename, "rb");

	fseek(f, 0, SEEK_END);
	int len = ftell(f);
	fseek(f, 0, SEEK_SET);

	uchar* buf=new uchar[len];
	fread(buf, 1,len, f);
	fclose(f);

  struct jpeg_decompress_struct cinfo;

  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */
  my_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jpeg_create_decompress(&cinfo);

  j_mem_src(&cinfo, buf, len);

  /* Step 3: read file parameters with jpeg_read_header() */
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);
  
  if (cinfo.output_components != 1) {
	  delete[] buf;
	  return 0;
  }

  w = cinfo.output_width;
  h = cinfo.output_height;
	if ( (*dstImage)->dimSizes[0] != h || (*dstImage)->dimSizes[1] != w )
		ResizeLVArray2D(dstImage, h, w);

  row_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
 // buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

  while (cinfo.output_scanline < cinfo.output_height) {
	uchar* jpeg_buf = & (*dstImage)->elem [cinfo.output_scanline * w];
    jpeg_read_scanlines(&cinfo, &jpeg_buf, 1);
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  delete[] buf;
  return 1;
}


CDLL_EXPORT void qtrk_dump_memleaks()
{
#ifdef USE_MEMDBG
	_CrtDumpMemoryLeaks();
#endif
}

