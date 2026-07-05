// [library:3d] Link stubs for the software rasterizer, which is excluded from portable
// builds (hand-written x86 assembly and largely unimplemented in the decompilation).
// GolD3DRenderDevice embeds a GolSoftwareRenderer, so its constructor/destructor must
// exist and be harmless; the rendering entry points are unreachable because miniwin
// never advertises a software 3D device.

#include "golerror.h"
#include "render/golsoftwarerenderer.h"

GolSoftwareRenderer::GolSoftwareRenderer()
{
}

GolSoftwareRenderer::~GolSoftwareRenderer()
{
}

LegoBool GolSoftwareRenderer::Initialize(PixelFormat p_pixelFormat, LegoS32 p_nodeCapacity)
{
	GOL_FATALERROR_MESSAGE("The software renderer is not available in racers-portable");
	return FALSE;
}

void GolSoftwareRenderer::SetupPipeline(RasterizerPipeline* p_buffer, GolMaterial* p_material, LegoU32 p_index)
{
	GOL_FATALERROR_MESSAGE("The software renderer is not available in racers-portable");
}

void GolSoftwareRenderer::FUN_100417a0(TriangleCommand* p_cmds, LegoU32 p_count, LegoFloat)
{
	GOL_FATALERROR_MESSAGE("The software renderer is not available in racers-portable");
}

void GolSoftwareRenderer::FUN_100417c0(TriangleCommand* p_cmds, LegoU32 p_count)
{
	GOL_FATALERROR_MESSAGE("The software renderer is not available in racers-portable");
}

void GolSoftwareRenderer::FUN_10041830(LegoS32 p_count, LegoBool p_sort)
{
	GOL_FATALERROR_MESSAGE("The software renderer is not available in racers-portable");
}

void GolSoftwareRenderer::FUN_10041a20(LegoBool p_sort)
{
	GOL_FATALERROR_MESSAGE("The software renderer is not available in racers-portable");
}
