#include "prefix.h"

#include "core.h"
#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "mcio.h"

#include "uidc.h"
#include "util.h"
#include "objectstream.h"
#include "bitmapeffect.h"
#include "globals.h"
#include "execpt.h"
#include "paint.h"

#include "graphicscontext.h"
#include "graphics.h"

////////////////////////////////////////////////////////////////////////////////

static inline MCGRectangle MCRectangleToMCGRectangle(MCRectangle p_rect)
{
	MCGRectangle t_rect;
	t_rect . origin . x = (MCGFloat) p_rect . x;
	t_rect . origin . y = (MCGFloat) p_rect . y;
	t_rect . size . width = (MCGFloat) p_rect . width;
	t_rect . size . height = (MCGFloat) p_rect . height;
	return t_rect;
}

static inline MCGPoint MCPointToMCGPoint(MCPoint p_point)
{
	MCGPoint t_point;
	t_point . x = (MCGFloat) p_point . x;
	t_point . y = (MCGFloat) p_point . y;
	return t_point;
}

static inline MCGBlendMode MCBitmapEffectBlendModeToMCGBlendMode(MCBitmapEffectBlendMode p_blend_mode)
{
	switch(p_blend_mode)
	{
		case kMCBitmapEffectBlendModeNormal:
			return kMCGBlendModeSourceOver;
		case kMCBitmapEffectBlendModeMultiply:
			return kMCGBlendModeMultiply;
		case kMCBitmapEffectBlendModeScreen:
			return kMCGBlendModeScreen;
		case kMCBitmapEffectBlendModeOverlay:
			return kMCGBlendModeOverlay;
		case kMCBitmapEffectBlendModeDarken:
			return kMCGBlendModeDarken;
		case kMCBitmapEffectBlendModeLighten:
			return kMCGBlendModeLighten;
		case kMCBitmapEffectBlendModeColorDodge:
			return kMCGBlendModeColorDodge;
		case kMCBitmapEffectBlendModeColorBurn:
			kMCGBlendModeColorBurn;
		case kMCBitmapEffectBlendModeHardLight:
			kMCGBlendModeHardLight;
		case kMCBitmapEffectBlendModeSoftLight:
			return kMCGBlendModeSoftLight;
		case kMCBitmapEffectBlendModeDifference:
			return kMCGBlendModeDifference;
		case kMCBitmapEffectBlendModeExclusion:
			return kMCGBlendModeExclusion;
		case kMCBitmapEffectBlendModeHue:
			return kMCGBlendModeHue;
		case kMCBitmapEffectBlendModeSaturation:
			return kMCGBlendModeSaturation;
		case kMCBitmapEffectBlendModeColor:
			return kMCGBlendModeColor;
		case kMCBitmapEffectBlendModeLuminosity:
			return kMCGBlendModeLuminosity;
	}
}

static void MCGraphicsContextAngleAndDistanceToXYOffset(uint8_t p_angle, uint8_t p_distance, MCGFloat &r_x_offset, MCGFloat &r_y_offset)
{
	r_x_offset = floor(0.5f + p_distance * cos(p_angle * M_PI / 180.0));
	r_y_offset = floor(0.5f + p_distance * sin(p_angle * M_PI / 180.0));
}

////////////////////////////////////////////////////////////////////////////////

MCGraphicsContext::MCGraphicsContext(MCGContextRef p_context)
{
	m_gcontext = MCGContextRetain(p_context);
	m_pattern = nil;
}

MCGraphicsContext::MCGraphicsContext(uint32_t p_width, uint32_t p_height, bool p_alpha)
{
	/* UNCHECKED */ MCGContextCreate(p_width, p_height, p_alpha, m_gcontext);
	m_pattern = nil;
}

MCGraphicsContext::MCGraphicsContext(uint32_t p_width, uint32_t p_height, uint32_t p_stride, void *p_pixels, bool p_alpha)
{
	/* UNCHECKED */ MCGContextCreateWithPixels(p_width, p_height, p_stride, p_pixels, p_alpha, m_gcontext);
	m_pattern = nil;
}

MCGraphicsContext::~MCGraphicsContext()
{
	MCGImageRelease(m_pattern);
	MCGContextRelease(m_gcontext);
}

MCContextType MCGraphicsContext::gettype() const
{
	return CONTEXT_TYPE_SCREEN;
}

////////////////////////////////////////////////////////////////////////////////

void MCGraphicsContext::begin(bool p_group)
{
	MCGContextBegin(m_gcontext);
}

bool MCGraphicsContext::begin_with_effects(MCBitmapEffectsRef p_effects, const MCRectangle &p_shape)
{
	MCGBitmapEffects t_effects;
	t_effects . has_drop_shadow = false;
	t_effects . has_outer_glow = false;
	t_effects . has_inner_glow = false;
	t_effects . has_inner_shadow = false;

	if ((p_effects -> mask & kMCBitmapEffectTypeColorOverlayBit) != 0)
	{
		t_effects . has_color_overlay = true;
		t_effects . color_overlay . color = p_effects -> effects[kMCBitmapEffectTypeColorOverlay] . layer . color;
		t_effects . color_overlay . blend_mode = MCBitmapEffectBlendModeToMCGBlendMode((MCBitmapEffectBlendMode) p_effects -> effects[kMCBitmapEffectTypeColorOverlay] . layer . blend_mode);

	}
	else
		t_effects . has_color_overlay = false;
	
	if ((p_effects -> mask & kMCBitmapEffectTypeInnerGlowBit) != 0)
	{
		t_effects . has_inner_glow = true;
		t_effects . inner_glow . color = p_effects -> effects[kMCBitmapEffectTypeInnerGlow] . glow . color;
		t_effects . inner_glow . blend_mode = MCBitmapEffectBlendModeToMCGBlendMode((MCBitmapEffectBlendMode) p_effects -> effects[kMCBitmapEffectTypeInnerGlow] . glow . blend_mode);
		t_effects . inner_glow . size = (MCGFloat) p_effects -> effects[kMCBitmapEffectTypeInnerGlow] . glow . size;
		t_effects . inner_glow . inverted = p_effects -> effects[kMCBitmapEffectTypeInnerGlow] . glow . source == kMCBitmapEffectSourceEdge;
	}
	else
		t_effects . has_inner_glow = false;

	if ((p_effects -> mask & kMCBitmapEffectTypeInnerShadowBit) != 0)
	{
		t_effects . has_inner_shadow = true;
		t_effects . inner_shadow . color = p_effects -> effects[kMCBitmapEffectTypeInnerShadow] . shadow . color;
		t_effects . inner_shadow . blend_mode = MCBitmapEffectBlendModeToMCGBlendMode((MCBitmapEffectBlendMode) p_effects -> effects[kMCBitmapEffectTypeInnerShadow] . shadow . blend_mode);
		t_effects . inner_shadow . size = (MCGFloat) p_effects -> effects[kMCBitmapEffectTypeInnerShadow] . shadow . size;
		
		MCGFloat t_x_offset, t_y_offset;
		MCGraphicsContextAngleAndDistanceToXYOffset(p_effects -> effects[kMCBitmapEffectTypeInnerShadow] . shadow . angle, p_effects -> effects[kMCBitmapEffectTypeInnerShadow] . shadow . distance,
													t_x_offset, t_y_offset);
		t_effects . inner_shadow . x_offset = t_x_offset;
		t_effects . inner_shadow . y_offset = t_y_offset;
	}
	else
		t_effects . has_inner_shadow = false;	
	
	if ((p_effects -> mask & kMCBitmapEffectTypeOuterGlowBit) != 0)
	{
		t_effects . has_outer_glow = true;
		t_effects . outer_glow . color = p_effects -> effects[kMCBitmapEffectTypeOuterGlow] . glow . color;
		t_effects . outer_glow . blend_mode = MCBitmapEffectBlendModeToMCGBlendMode((MCBitmapEffectBlendMode) p_effects -> effects[kMCBitmapEffectTypeOuterGlow] . glow . blend_mode);
		t_effects . outer_glow . size = (MCGFloat) p_effects -> effects[kMCBitmapEffectTypeOuterGlow] . glow . size;
	}
	else
		t_effects . has_outer_glow = false;
	
	if ((p_effects -> mask & kMCBitmapEffectTypeDropShadowBit) != 0)
	{
		t_effects . has_drop_shadow = true;
		t_effects . drop_shadow . color = p_effects -> effects[kMCBitmapEffectTypeDropShadow] . shadow . color;
		t_effects . drop_shadow . blend_mode = MCBitmapEffectBlendModeToMCGBlendMode((MCBitmapEffectBlendMode) p_effects -> effects[kMCBitmapEffectTypeDropShadow] . shadow . blend_mode);
		t_effects . drop_shadow . size = (MCGFloat) p_effects -> effects[kMCBitmapEffectTypeDropShadow] . shadow . size;
		
		MCGFloat t_x_offset, t_y_offset;
		MCGraphicsContextAngleAndDistanceToXYOffset(p_effects -> effects[kMCBitmapEffectTypeDropShadow] . shadow . angle, p_effects -> effects[kMCBitmapEffectTypeDropShadow] . shadow . distance,
													t_x_offset, t_y_offset);
		t_effects . drop_shadow . x_offset = t_x_offset;
		t_effects . drop_shadow . y_offset = t_y_offset;
	}
	else
		t_effects . has_drop_shadow = false;	
		
	MCGContextBeginWithEffects(m_gcontext, t_effects);
	return true;
}

void MCGraphicsContext::end()
{
	MCGContextEnd(m_gcontext);
}

////////////////////////////////////////////////////////////////////////////////

void MCGraphicsContext::setprintmode(void)
{
}

bool MCGraphicsContext::changeopaque(bool p_value)
{
	return true;
}

void MCGraphicsContext::setclip(const MCRectangle& p_clip)
{
	m_clip = p_clip;
	MCGContextClipToRect(m_gcontext, MCRectangleToMCGRectangle(p_clip));
}

const MCRectangle& MCGraphicsContext::getclip(void) const
{
	return m_clip;
}

void MCGraphicsContext::clearclip(void)
{
}

void MCGraphicsContext::setorigin(int2 x, int2 y)
{
	MCGContextTranslateCTM(m_gcontext, -1.0f * x, -1.0f * y);
}

void MCGraphicsContext::clearorigin(void)
{
	MCGContextResetCTM(m_gcontext);
}

void MCGraphicsContext::setquality(uint1 p_quality)
{
	MCGContextSetShouldAntialias(m_gcontext, p_quality == QUALITY_SMOOTH);
}

void MCGraphicsContext::setfunction(uint1 p_function)
{
	m_function = p_function;
	MCGBlendMode t_blend_mode;
	t_blend_mode = kMCGBlendModeSourceOver;
	switch (p_function)
	{
		case GXblendClear:
		case GXclear:
			t_blend_mode = kMCGBlendModeClear;
			break;
		case GXblendSrc:
			t_blend_mode = kMCGBlendModeCopy;
			break;
		case GXblendDst:
			t_blend_mode = kMCGBlendModeClear;
			break;
		case GXblendSrcOver:
		case GXcopy:
			t_blend_mode = kMCGBlendModeSourceOver;
			break;
		case GXblendDstOver:
			t_blend_mode = kMCGBlendModeDestinationOver;
			break;
		case GXblendSrcIn:
			t_blend_mode = kMCGBlendModeSourceIn;
			break;
		case GXblendDstIn:
			t_blend_mode = kMCGBlendModeDestinationIn;
			break;
		case GXblendSrcOut:
			t_blend_mode = kMCGBlendModeSourceOut;
			break;
		case GXblendDstOut:
			t_blend_mode = kMCGBlendModeDestinationOut;
			break;
		case GXblendSrcAtop:
			t_blend_mode = kMCGBlendModeSourceAtop;
			break;
		case GXblendDstAtop:
			t_blend_mode = kMCGBlendModeDestinationAtop;
			break;
		case GXblendXor:
			t_blend_mode = kMCGBlendModeXor;
			break;
		case GXblendPlus:
			t_blend_mode = kMCGBlendModePlusLighter;
			break;
		case GXblendMultiply:
			t_blend_mode = kMCGBlendModeMultiply;
			break;
		case GXblendScreen:
			t_blend_mode = kMCGBlendModeScreen;
			break;
		case GXblendOverlay:
			t_blend_mode = kMCGBlendModeOverlay;
			break;
		case GXblendDarken:
			t_blend_mode = kMCGBlendModeDarken;
			break;
		case GXblendLighten:
			t_blend_mode = kMCGBlendModeLighten;
			break;
		case GXblendDodge:
			t_blend_mode = kMCGBlendModeColorDodge;
			break;
		case GXblendBurn:
			t_blend_mode = kMCGBlendModeColorBurn;
			break;
		case GXblendHardLight:
			t_blend_mode = kMCGBlendModeHardLight;
			break;
		case GXblendSoftLight:
			t_blend_mode = kMCGBlendModeSoftLight;
			break;
		case GXblendDifference:
			t_blend_mode = kMCGBlendModeDifference;
			break;
		case GXblendExclusion:
			t_blend_mode = kMCGBlendModeExclusion;
			break;
	}
	MCGContextSetBlendMode(m_gcontext, t_blend_mode);
}

uint1 MCGraphicsContext::getfunction(void)
{
	return m_function;
}

void MCGraphicsContext::setopacity(uint1 p_opacity)
{
	m_opacity = p_opacity;
	MCGContextSetOpacity(m_gcontext, p_opacity / 255.0f);
}

uint1 MCGraphicsContext::getopacity(void)
{
	/* TODO */
	return m_opacity;
}

////////////////////////////////////////////////////////////////////////////////

void MCGraphicsContext::setforeground(const MCColor& c)
{
	MCGContextSetFillRGBAColor(m_gcontext, (c . red >> 8) / 255.0f, (c . green >> 8) / 255.0f, (c . blue >> 8) / 255.0f, 1.0f);
	MCGContextSetStrokeRGBAColor(m_gcontext, (c . red >> 8) / 255.0f, (c . green >> 8) / 255.0f, (c . blue >> 8) / 255.0f, 1.0f);
}

void MCGraphicsContext::setbackground(const MCColor& c)
{

}

void MCGraphicsContext::setdashes(uint16_t p_offset, const uint8_t *p_dashes, uint16_t p_length)
{
	MCGFloat *t_lengths;
	/* UNCHECKED */ MCMemoryNewArray(p_length, t_lengths);
	for (uint32_t i = 0; i < p_length; i++)
		t_lengths[i] = (MCGFloat) p_dashes[i];
	
	MCGContextSetStrokeDashes(m_gcontext, (MCGFloat) p_offset, t_lengths, p_length);
	
	MCMemoryDeleteArray(t_lengths);	
}

void MCGraphicsContext::setfillstyle(uint2 style, MCGImageRef p, int2 x, int2 y)
{
	MCGImageRelease(m_pattern);
	m_pattern = nil;

	if (style == FillTiled && p != NULL)
	{
		MCGAffineTransform t_transform = MCGAffineTransformMakeTranslation(x, y);
		MCGContextSetFillPattern(m_gcontext, p, t_transform, kMCGImageFilterBilinear);
		m_pattern = MCGImageRetain(p);
		m_pattern_x = x;
		m_pattern_y = y;
	}
	else
	{
		MCGAffineTransform t_transform = MCGAffineTransformMakeIdentity();
		MCGContextSetFillPattern(m_gcontext, NULL, t_transform, kMCGImageFilterNearest);
	}
}

void MCGraphicsContext::getfillstyle(uint2& style, MCGImageRef& p, int2& x, int2& y)
{
	p = m_pattern;
	x = m_pattern_x;
	y = m_pattern_y;
}

void MCGraphicsContext::setlineatts(uint2 linesize, uint2 linestyle, uint2 capstyle, uint2 joinstyle)
{
	MCGContextSetStrokeWidth(m_gcontext, (MCGFloat) linesize);
	
	switch (capstyle)
	{
		case CapButt:
			MCGContextSetStrokeCapStyle(m_gcontext, kMCGCapStyleButt);
			break;
		case CapRound:
			MCGContextSetStrokeCapStyle(m_gcontext, kMCGCapStyleRound);
			break;
		case CapProjecting:
			MCGContextSetStrokeCapStyle(m_gcontext, kMCGCapStyleSquare);
			break;
	}
	
	switch (linestyle)
	{
		case LineOnOffDash:
		{
			break;
		}
		case LineDoubleDash:
		{
			break;
		}
	}
	
	
	switch (joinstyle)
	{
		case JoinRound:
			MCGContextSetStrokeJoinStyle(m_gcontext, kMCGJoinStyleRound);
			break;
		case JoinMiter:
			MCGContextSetStrokeJoinStyle(m_gcontext, kMCGJoinStyleMiter);
			break;
		case JoinBevel:
			MCGContextSetStrokeJoinStyle(m_gcontext, kMCGJoinStyleBevel);
			break;
	}	
}

void MCGraphicsContext::setmiterlimit(real8 p_limit)
{
	MCGContextSetStrokeMiterLimit(m_gcontext, (MCGFloat) p_limit);
}

void MCGraphicsContext::setgradient(MCGradientFill *p_gradient)
{
	if (p_gradient != NULL && p_gradient -> kind != kMCGradientKindNone)
	{	
		MCGGradientFunction t_function;
		t_function = kMCGGradientFunctionLinear;
		switch (p_gradient -> kind)
		{
			case kMCGradientKindLinear:
				t_function = kMCGGradientFunctionLinear;
				break;
			case kMCGradientKindRadial:
				t_function = kMCGGradientFunctionRadial;
				break;
			case kMCGradientKindConical:
				t_function = kMCGGradientFunctionConical;
				break;
			case kMCGradientKindDiamond:
				t_function = kMCGGradientFunctionSweep;
				break;
				/*kMCGradientKindSpiral,
				 kMCGradientKindXY,
				 kMCGradientKindSqrtXY*/
		}
		
		MCGImageFilter t_filter;
		t_filter = kMCGImageFilterNearest;
		switch (p_gradient -> quality)
		{
			case kMCGradientQualityNormal:
				t_filter = kMCGImageFilterNearest;
				break;
			case kMCGradientQualityGood:
				t_filter = kMCGImageFilterBilinear;
				break;
		}
		
		MCGFloat *t_stops;
		/* UNCHECKED */ MCMemoryNewArray(p_gradient -> ramp_length, t_stops);
		MCGColor *t_colors;
		/* UNCHECKED */ MCMemoryNewArray(p_gradient -> ramp_length, t_colors);
		for (uint32_t i = 0; i < p_gradient -> ramp_length; i++)
		{
			t_stops[i] = (MCGFloat) p_gradient -> ramp[i] . offset / STOP_INT_MAX;
			t_colors[i] = p_gradient -> ramp[i] . color;
		}
						
		MCGAffineTransform t_transform;
		t_transform . a = p_gradient -> primary . x - p_gradient -> origin . x;
		t_transform . b = p_gradient -> primary . y - p_gradient -> origin . y;
		t_transform . c = p_gradient -> secondary . x - p_gradient -> origin . x;
		t_transform . d = p_gradient -> secondary . y - p_gradient -> origin . y;
		t_transform . tx = p_gradient -> origin . x;
		t_transform . ty = p_gradient -> origin . y;

		MCGContextSetFillGradient(m_gcontext, t_function, t_stops, t_colors, p_gradient -> ramp_length, p_gradient -> mirror, p_gradient -> wrap, p_gradient -> repeat, t_transform, t_filter);
		MCGContextSetStrokeGradient(m_gcontext, t_function, t_stops, t_colors, p_gradient -> ramp_length, p_gradient -> mirror, p_gradient -> wrap, p_gradient -> repeat, t_transform, t_filter);
	}
}

////////////////////////////////////////////////////////////////////////////////

void MCGraphicsContext::drawline(int2 x1, int2 y1, int2 x2, int2 y2)
{
	MCGPoint t_start;
	t_start . x = (MCGFloat) x1;
	t_start . y = (MCGFloat) y1;
	
	MCGPoint t_finish;
	t_finish . x = (MCGFloat) x2;
	t_finish . y = (MCGFloat) y2;
	
	MCGContextBeginPath(m_gcontext);
	MCGContextAddLine(m_gcontext, t_start, t_finish);	
	MCGContextStroke(m_gcontext);
}

void MCGraphicsContext::drawlines(MCPoint *points, uint2 npoints, bool p_closed)
{
	MCGPoint *t_points;
	/* UNCHECKED */ MCMemoryNewArray(npoints, t_points);
	for (uint32_t i = 0; i < npoints; i++)
		t_points[i] = MCPointToMCGPoint(points[i]);

	MCGContextBeginPath(m_gcontext);
	if (p_closed)
		MCGContextAddPolygon(m_gcontext, t_points, npoints);
	else
		MCGContextAddPolyline(m_gcontext, t_points, npoints);	
	MCGContextStroke(m_gcontext);
	
	MCMemoryDeleteArray(t_points);
}

void MCGraphicsContext::fillpolygon(MCPoint *points, uint2 npoints)
{
	MCGPoint *t_points;
	/* UNCHECKED */ MCMemoryNewArray(npoints, t_points);
	for (uint32_t i = 0; i < npoints; i++)
		t_points[i] = MCPointToMCGPoint(points[i]);
	
	MCGContextBeginPath(m_gcontext);
	MCGContextAddPolygon(m_gcontext, t_points, npoints);	
	MCGContextFill(m_gcontext);
	
	MCMemoryDeleteArray(t_points);	
}

void MCGraphicsContext::drawarc(const MCRectangle& rect, uint2 start, uint2 angle)
{
	MCGPoint t_center;
	t_center . x = (MCGFloat) (rect . x + 0.5f * rect . width);
	t_center . y = (MCGFloat) (rect . y + 0.5f * rect . height);
	
	MCGSize t_radii;
	t_radii . width = (MCGFloat) rect . width;
	t_radii . height = (MCGFloat) rect . height;

	MCGContextBeginPath(m_gcontext);
	MCGContextAddArc(m_gcontext, t_center, t_radii, 0.0f, (MCGFloat) (360 - (start + angle)), (MCGFloat) (360 - start));
	MCGContextStroke(m_gcontext);
}

void MCGraphicsContext::fillarc(const MCRectangle& rect, uint2 start, uint2 angle)
{
	MCGPoint t_center;
	t_center . x = (MCGFloat) (rect . x + 0.5f * rect . width);
	t_center . y = (MCGFloat) (rect . y + 0.5f * rect . height);
	
	MCGSize t_radii;
	t_radii . width = (MCGFloat) rect . width;
	t_radii . height = (MCGFloat) rect . height;
	
	MCGContextBeginPath(m_gcontext);
	if (angle != 0 && angle % 360 == 0)
		MCGContextAddArc(m_gcontext, t_center, t_radii, 0.0f, (MCGFloat) (360 - (start + angle)), (MCGFloat) (360 - start));
	else
		MCGContextAddSegment(m_gcontext, t_center, t_radii, 0.0f, (MCGFloat) (360 - (start + angle)), (MCGFloat) (360 - start));
	MCGContextFill(m_gcontext);
}

void MCGraphicsContext::drawsegment(const MCRectangle& rect, uint2 start, uint2 angle)
{
	MCGPoint t_center;
	t_center . x = (MCGFloat) (rect . x + 0.5f * rect . width);
	t_center . y = (MCGFloat) (rect . y + 0.5f * rect . height);
	
	MCGSize t_radii;
	t_radii . width = (MCGFloat) rect . width;
	t_radii . height = (MCGFloat) rect . height;
	
	MCGContextBeginPath(m_gcontext);
	MCGContextAddSegment(m_gcontext, t_center, t_radii, 0.0f, (MCGFloat) (360 - (start + angle)), (MCGFloat) (360 - start));
	MCGContextStroke(m_gcontext);
}

void MCGraphicsContext::drawsegments(MCSegment *segments, uint2 nsegs)
{
	MCGContextBeginPath(m_gcontext);
	for (uint32_t i = 0; i < nsegs; i++)
	{
		MCGPoint t_point;
		t_point . x = (MCGFloat) segments[i] . x1;
		t_point . y = (MCGFloat) segments[i] . y1;
		MCGContextMoveTo(m_gcontext, t_point);		
		t_point . x = (MCGFloat) segments[i] . x2;
		t_point . y = (MCGFloat) segments[i] . y2;
		MCGContextLineTo(m_gcontext, t_point);		
	}
	MCGContextStroke(m_gcontext);
}

void MCGraphicsContext::drawrect(const MCRectangle& rect)
{
	MCGContextBeginPath(m_gcontext);
	MCGContextAddRectangle(m_gcontext, MCRectangleToMCGRectangle(rect));
	MCGContextStroke(m_gcontext);
}

void MCGraphicsContext::fillrect(const MCRectangle& rect)
{
	MCGContextBeginPath(m_gcontext);
	MCGContextAddRectangle(m_gcontext, MCRectangleToMCGRectangle(rect));
	MCGContextFill(m_gcontext);
}

void MCGraphicsContext::fillrects(MCRectangle *rects, uint2 nrects)
{
	MCGContextBeginPath(m_gcontext);
	for (uint2 i = 0; i < nrects; i++)
		MCGContextAddRectangle(m_gcontext, MCRectangleToMCGRectangle(rects[i]));
	MCGContextFill(m_gcontext);
}

void MCGraphicsContext::drawroundrect(const MCRectangle& rect, uint2 radius)
{
	MCGSize t_corner_radii;
	t_corner_radii . width = radius;
	t_corner_radii . height = radius;
	
	MCGContextBeginPath(m_gcontext);	
	MCGContextAddRoundedRectangle(m_gcontext, MCRectangleToMCGRectangle(rect), t_corner_radii);	
	MCGContextStroke(m_gcontext);
}

void MCGraphicsContext::fillroundrect(const MCRectangle& rect, uint2 radius)
{
	MCGSize t_corner_radii;
	t_corner_radii . width = radius;
	t_corner_radii . height = radius;
	
	MCGContextBeginPath(m_gcontext);
	MCGContextAddRoundedRectangle(m_gcontext, MCRectangleToMCGRectangle(rect), t_corner_radii);	
	MCGContextFill(m_gcontext);
}

////////////////////////////////////////////////////////////////////////////////

void MCGraphicsContext::drawpath(MCPath *path)
{

}

void MCGraphicsContext::fillpath(MCPath *path, bool p_evenodd)
{

}

////////////////////////////////////////////////////////////////////////////////

void MCGraphicsContext::drawpict(uint1 *data, uint4 length, bool embed, const MCRectangle& drect, const MCRectangle& crect)
{
}

void MCGraphicsContext::draweps(real8 sx, real8 sy, int2 angle, real8 xscale, real8 yscale, int2 tx, int2 ty,
								const char *prolog, const char *psprolog, uint4 psprologlength, const char *ps, uint4 length,
								const char *fontname, uint2 fontsize, uint2 fontstyle, MCFontStruct *font, const MCRectangle& trect)
{
}

void MCGraphicsContext::drawimage(const MCImageDescriptor& p_image, int2 sx, int2 sy, uint2 sw, uint2 sh, int2 dx, int2 dy)
{
	MCImageBitmap *t_bits = p_image.bitmap;

	MCGRaster t_raster;
	t_raster . width = t_bits -> width;
	t_raster . height = t_bits -> height;
	t_raster . stride = t_bits -> stride;
	t_raster . pixels = t_bits -> data;
	t_raster . format = t_bits -> has_transparency ? kMCGRasterFormat_ARGB : kMCGRasterFormat_xRGB;

	MCGRectangle t_clip;
	t_clip . origin . x = dx;
	t_clip . origin . y = dy;
	t_clip . size . width = sw;
	t_clip . size . height = sh;

	MCGRectangle t_dest;
	t_dest.origin.x = dx - sx;
	t_dest.origin.y = dy - sy;
	t_dest.size.width = t_raster.width;
	t_dest.size.height = t_raster.height;

	MCGContextSave(m_gcontext);
	MCGContextClipToRect(m_gcontext, t_clip);

	if (p_image.has_transform)
	{
		MCGAffineTransform t_transform = MCGAffineTransformMakeTranslation(-t_dest.origin.x, -t_dest.origin.y);
		t_transform = MCGAffineTransformConcat(p_image.transform, t_transform);
		t_transform = MCGAffineTransformTranslate(t_transform, t_dest.origin.x, t_dest.origin.y);

		MCGContextConcatCTM(m_gcontext, t_transform);
	}

	MCGContextDrawPixels(m_gcontext, t_raster, t_dest, p_image.filter);
	MCGContextRestore(m_gcontext);
}

////////////////////////////////////////////////////////////////////////////////

void MCGraphicsContext::drawtheme(MCThemeDrawType p_type, MCThemeDrawInfo* p_info)
{
	MCThemeDraw(m_gcontext, p_type, p_info);
}

MCRegionRef MCGraphicsContext::computemaskregion(void)
{
	return NULL;
}

void MCGraphicsContext::clear(void)
{
}

void MCGraphicsContext::clear(const MCRectangle* rect)
{
}

void MCGraphicsContext::applywindowshape(MCWindowShape *p_mask, uint4 p_u_width, uint4 p_u_height)
{
}

////////////////////////////////////////////////////////////////////////////////

void MCGraphicsContext::setfont(const char *fontname, uint2 fontsize, uint2 fontstyle, MCFontStruct *font)
{
}

void MCGraphicsContext::drawtext(int2 x, int2 y, const char *s, uint2 length, MCFontStruct *f, Boolean image, bool p_unicode_override)
{
	/*MCExecPoint ep;
	ep . setsvalue(MCString(s, length));	
	if (f -> unicode || p_unicode_override)
		ep . utf16toutf8();
	else
		ep . nativetoutf8();	
	const MCString &t_utf_string = ep . getsvalue();	
	
	MCGPoint t_loc;
	t_loc . x = x;
	t_loc . y = y;
	
	MCGContextDrawText(m_gcontext, t_utf_string . getstring(), t_utf_string . getlength(), t_loc, f -> size);*/
	
	bool t_success;
	t_success = true;
	
	MCGFloat t_tx, t_ty;
	MCGMaskRef t_mask;
	t_mask = nil;
	if (t_success)
	{		
		MCGRectangle t_gclip;
		t_gclip = MCGContextGetDeviceClipBounds(m_gcontext);
		
		MCRectangle t_clip;		
		t_clip . x = floor(t_gclip . origin . x);
		t_clip . y = floor(t_gclip . origin . y);
		t_clip . width = ceil(t_gclip . origin . x + t_gclip . size . width) - t_clip . x;
		t_clip . height = ceil(t_gclip . origin . y + t_gclip . size . height) - t_clip . y;		
		
		MCGAffineTransform t_gtransform;
		t_gtransform = MCGContextGetDeviceTransform(m_gcontext);

		MCGPoint t_text_origin;
		t_text_origin = MCGPointApplyAffineTransform(MCGPointMake(x, y), t_gtransform);

		MCGFloat t_offset_x, t_offset_y;
		MCGFloat t_temp;
		t_offset_x = modff(t_text_origin . x, &t_tx);
		t_offset_y = modff(t_text_origin . y, &t_ty);
		t_gtransform . tx = t_offset_x;
		t_gtransform . ty = t_offset_y;
		
		//t_tx -= t_clip . x;
		//t_ty -= t_clip . y;

		t_success = MCscreen -> textmask(f, s, length, p_unicode_override, t_clip, t_gtransform, t_mask);
	}
	
	if (t_success && t_mask!= nil)
	{
		if (image)
		{
		}
		MCGContextDrawDeviceMask(m_gcontext, t_mask, t_tx, t_ty);	
		MCGMaskRelease(t_mask);
	}
}

void MCGraphicsContext::drawlink(const char *link, const MCRectangle& region)
{
}

int4 MCGraphicsContext::textwidth(MCFontStruct *f, const char *s, uint2 length, bool p_unicode_override)
{
	MCExecPoint ep;
	ep . setsvalue(MCString(s, length));	
	if (f -> unicode || p_unicode_override)
		ep . utf16toutf8();
	else
		ep . nativetoutf8();	
	const MCString &t_utf_string = ep . getsvalue();
	
	return MCGContextMeasureText(m_gcontext, t_utf_string . getstring(), t_utf_string . getlength(), f -> size);
}

////////////////////////////////////////////////////////////////////////////////

uint2 MCGraphicsContext::getdepth(void) const
{
	return 32;
}

const MCColor& MCGraphicsContext::getblack(void) const
{
	return MCscreen->black_pixel;
}

const MCColor& MCGraphicsContext::getwhite(void) const
{
	return MCscreen->white_pixel;
}

const MCColor& MCGraphicsContext::getgray(void) const
{
	return MCscreen->gray_pixel;
}

const MCColor& MCGraphicsContext::getbg(void) const
{
	return MCscreen->background_pixel;
}

////////////////////////////////////////////////////////////////////////////////