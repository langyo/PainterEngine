#include "PX_Texture.h"

PX_TEXTURERENDER_BLEND PX_TEXTURERENDER_BLEND_BUILD(px_float hdr_R, px_float hdr_G, px_float hdr_B, px_float alpha)
{
	PX_TEXTURERENDER_BLEND blend;
	blend.alpha = alpha;
	blend.hdr_R = hdr_R;
	blend.hdr_G = hdr_G;
	blend.hdr_B = hdr_B;
	return blend;
}

px_bool PX_TextureCreate(px_memorypool *mp,px_texture *tex,px_int width,px_int height)
{
	return PX_SurfaceCreate(mp,width,height,tex);
}

px_bool PX_TextureCreateFromMemory(px_memorypool *mp,px_void *data,px_int size,px_texture *tex)
{
	px_int width;
	px_int height;
	if (PX_BitmapVerify(data,size))
	{
		width=PX_BitmapGetWidth(data);
		height=PX_BitmapGetHeight(data);
		if(PX_TextureCreate(mp,tex,width,height))
		{
			PX_BitmapRender(tex,data,size,0,0);
			return PX_TRUE;
		}
		else
		{
			return PX_FALSE;
		}

	}

	if (PX_TRawVerify(data,size))
	{
		width=PX_TRawGetWidth(data);
		height=PX_TRawGetHeight(data);
		if(PX_TextureCreate(mp,tex,width,height))
		{
			PX_TRawRender(tex,data,0,0);
			return PX_TRUE;
		}
		else
		{
			return PX_FALSE;
		}
	}

	if (PX_PngVerify((px_byte *)data,size,&width,&height,0))
	{
		if (PX_TextureCreate(mp, tex, width, height))
		{
			if (!PX_PngToRenderBuffer(mp, (px_byte*)data, size, tex))
				return PX_FALSE;
			return PX_TRUE;
		}
		else
		{
			return PX_FALSE;
		}
	}

	if (PX_JpgVerify((px_byte *)data, size))
	{
		PX_JpgDecoder decoder;
		if (PX_JpgDecoderInitialize(mp, &decoder, (px_byte*)data, size))
		{
			if (PX_TextureCreate(mp, tex, decoder.width, decoder.height))
			{
				PX_JpgDecoderRenderToSurface(&decoder, tex);
				PX_JpgDecoderFree(&decoder);
				return PX_TRUE;
			}
			PX_JpgDecoderFree(&decoder);
		}
	}

	//image-format not supported
	return PX_FALSE;
}

typedef struct  
{
	px_surface *psurface;
	px_texture *Server;
	px_int x,y,top,bottom,left,right;
	PX_ALIGN refPoint;
	PX_TEXTURERENDER_BLEND *blend;
}PX_TEXTURERENDER_PARALLEL_DATA;


px_void PX_TextureRenderClipMirror(px_surface* psurface, px_texture* tex, px_int x, px_int y, px_int clipx, px_int clipy, px_int clipw, px_int cliph, PX_ALIGN refPoint, PX_TEXTURERENDER_BLEND* blend, PX_TEXTURERENDER_MIRRROR_MODE mirrorMode)
{
	px_int i, j;
	px_int bR, bG, bB, bA;
	px_color* pdata;
	px_color clr;

	PX_ASSERTIF(tex == PX_NULL);
	PX_ASSERTIF(psurface == PX_NULL);
	PX_ASSERTIF(y - cliph / 2 > y);//y overflow
	PX_ASSERTIF(x - clipw / 2 > x);//x overflow
	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x -= clipw / 2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x -= clipw;
		break;
	case PX_ALIGN_LEFTMID:
		y -= cliph / 2;
		break;
	case PX_ALIGN_CENTER:
		y -= cliph / 2;
		x -= clipw / 2;
		break;
	case PX_ALIGN_RIGHTMID:
		y -= cliph / 2;
		x -= clipw;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y -= cliph;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y -= cliph;
		x -= clipw / 2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y -= cliph;
		x -= clipw;
		break;
	}

	//correct the param
	if (clipx < 0)
	{
		clipx = 0;
	}
	else if (clipx >= tex->width)
	{
		return;
	}

	if (clipy < 0)
	{
		clipy = 0;
	}
	else if (clipy >= tex->height)
	{
		return;
	}

	if (clipw <= 0)
	{
		return;
	}
	else if (clipx + clipw > tex->width)
	{
		clipw = tex->width - clipx;
	}

	if (cliph <= 0)
	{
		return;
	}
	else if (clipy + cliph > tex->height)
	{
		cliph = tex->height - clipy;
	}

	//refresh param
	switch (mirrorMode)
	{
	case PX_TEXTURERENDER_MIRRROR_MODE_NONE:
		if (x < psurface->limit_left)
		{
			clipx = clipx + psurface->limit_left - x;
		}
		if (y < psurface->limit_top)
		{
			clipy = clipy + psurface->limit_top - y;
		}
		break;
	case PX_TEXTURERENDER_MIRRROR_MODE_H:
		if (x+clipw > psurface->limit_right+1)
		{
			clipx = clipx + x + clipw - psurface->limit_right - 1;
		}
		if (y < psurface->limit_top)
		{
			clipy = clipy + psurface->limit_top - y;
		}
		break;
	case PX_TEXTURERENDER_MIRRROR_MODE_V:
		if (x < psurface->limit_left)
		{
			clipx = clipx + psurface->limit_left - x;
		}
		if (y + cliph > psurface->limit_bottom + 1)
		{
			clipy = clipy + y + cliph - psurface->limit_bottom - 1;
		}
		break;
	case PX_TEXTURERENDER_MIRRROR_MODE_HV:
		if (x + clipw > psurface->limit_right + 1)
		{
			clipx = clipx + x + clipw - psurface->limit_right - 1;
		}
		if (y + cliph > psurface->limit_bottom + 1)
		{
			clipy = clipy + y + cliph - psurface->limit_bottom - 1;
		}
		break;
	default:
		break;
	}

	pdata = (px_color*)tex->surfaceBuffer;

	if (x < psurface->limit_left)
	{
		if (x + clipw < psurface->limit_left)
		{
			return;
		}
		else
		{
			if (x + clipw <= psurface->limit_right + 1)
			{
				clipw = x + clipw - psurface->limit_left;
				x = psurface->limit_left;
			}
			else
			{
				clipw = psurface->limit_right - psurface->limit_left + 1;
				x = psurface->limit_left;
			}
		}

	}
	else if (x >= psurface->limit_left)
	{
		if (x + clipw > psurface->limit_right + 1)
			clipw = psurface->limit_right + 1-x;
	}
	else
	{
		return;
	}

	if (y < psurface->limit_top)
	{
		if (y + cliph < psurface->limit_top)
		{
			return;
		}
		else
		{
			if (y + cliph <= psurface->limit_bottom + 1)
			{
				cliph = y + cliph - psurface->limit_top;
				y = psurface->limit_top;
			}
			else
			{
				cliph = psurface->limit_bottom - psurface->limit_top + 1;
				y = psurface->limit_top;
			}
		}

	}
	else if (y >= psurface->limit_top)
	{
		if (y + cliph > psurface->limit_bottom + 1)
			cliph = psurface->limit_bottom + 1 - y;
	}
	else
	{
		return;
	}

	if (cliph <= 0 || clipw <= 0)
		return;

	if (blend)
	{
		px_int Ab = (px_int)(blend->alpha * 128);
		px_int Rb = (px_int)(blend->hdr_R * 128);
		px_int Gb = (px_int)(blend->hdr_G * 128);
		px_int Bb = (px_int)(blend->hdr_B * 128);

		switch (mirrorMode)
		{
		case PX_TEXTURERENDER_MIRRROR_MODE_NONE:
		{


#ifdef PX_GPU_ENABLE
			if (PX_GPU_isEnable()&& clipw>=32)
			{
				px_dword blend;
				Ab &= 0xff;
				Rb &= 0xff;
				Gb &= 0xff;
				Bb &= 0xff;
				blend = (Ab << 24) + (Rb << 16) + (Gb << 8) + Bb;
				
				PX_GPU_Render(pdata + clipy * tex->width + clipx, tex->width, clipw, cliph, psurface->surfaceBuffer + y * psurface->width + x, psurface->width, PX_COLOR_FORMAT, blend);
			}
			else
			{
				for (j = 0; j < cliph; j++)
				{
					for (i = 0; i < clipw; i++)
					{
						clr = pdata[(clipy + j) * tex->width + (clipx + i)];
						bA = (px_int)(clr._argb.a * Ab) >> 7;
						bR = (px_int)(clr._argb.r * Rb) >> 7;
						bG = (px_int)(clr._argb.g * Gb) >> 7;
						bB = (px_int)(clr._argb.b * Bb) >> 7;

						clr._argb.a = (px_uchar)bA;
						clr._argb.r = (px_uchar)bR;
						clr._argb.g = (px_uchar)bG;
						clr._argb.b = (px_uchar)bB;

						PX_SurfaceDrawPixelWithoutLimit(psurface, x + i, y + j, clr);
					}
				}
			}
			
#else
		for (j = 0; j < cliph; j++)
		{
			for (i = 0; i < clipw; i++)
			{
				clr = pdata[(clipy + j) * tex->width + (clipx + i)];
				bA = (px_int)(clr._argb.a * Ab) >> 7;
				bR = (px_int)(clr._argb.r * Rb) >> 7;
				bG = (px_int)(clr._argb.g * Gb) >> 7;
				bB = (px_int)(clr._argb.b * Bb) >> 7;

				clr._argb.a = (px_uchar)bA;
				clr._argb.r = (px_uchar)bR;
				clr._argb.g = (px_uchar)bG;
				clr._argb.b = (px_uchar)bB;

				PX_SurfaceDrawPixelWithoutLimit(psurface, x + i, y + j, clr);
			}
		}
#endif
		}
			break;
		case PX_TEXTURERENDER_MIRRROR_MODE_H:
			for (j = 0; j < cliph; j++)
			{
				for (i = 0; i < clipw; i++)
				{
					clr = pdata[(clipy + j) * tex->width + (clipx +i)];
					bA = (px_int)(clr._argb.a * Ab / 1000);
					bR = (px_int)(clr._argb.r * Rb / 1000);
					bG = (px_int)(clr._argb.g * Gb / 1000);
					bB = (px_int)(clr._argb.b * Bb / 1000);

					clr._argb.a = (px_uchar)bA;
					clr._argb.r = (px_uchar)bR;
					clr._argb.g = (px_uchar)bG;
					clr._argb.b = (px_uchar)bB;

					PX_SurfaceDrawPixelWithoutLimit(psurface, x + clipw - i-1, y + j, clr);
				}
			}
			break;
		case PX_TEXTURERENDER_MIRRROR_MODE_V:
			for (j = 0; j < cliph; j++)
			{
				for (i = 0; i < clipw; i++)
				{
					clr = pdata[(clipy + j) * tex->width + (clipx + i)];
					bA = (px_int)(clr._argb.a * Ab / 1000);
					bR = (px_int)(clr._argb.r * Rb / 1000);
					bG = (px_int)(clr._argb.g * Gb / 1000);
					bB = (px_int)(clr._argb.b * Bb / 1000);

					clr._argb.a = (px_uchar)bA;
					clr._argb.r = (px_uchar)bR;
					clr._argb.g = (px_uchar)bG;
					clr._argb.b = (px_uchar)bB;

					PX_SurfaceDrawPixelWithoutLimit(psurface, x + i, y +cliph-j-1, clr);
				}
			}
			break;
		case PX_TEXTURERENDER_MIRRROR_MODE_HV:
			for (j = 0; j < cliph; j++)
			{
				for (i = 0; i < clipw; i++)
				{
					clr = pdata[(clipy + j) * tex->width + (clipx + i)];
					bA = (px_int)(clr._argb.a * Ab / 1000);
					bR = (px_int)(clr._argb.r * Rb / 1000);
					bG = (px_int)(clr._argb.g * Gb / 1000);
					bB = (px_int)(clr._argb.b * Bb / 1000);

					clr._argb.a = (px_uchar)bA;
					clr._argb.r = (px_uchar)bR;
					clr._argb.g = (px_uchar)bG;
					clr._argb.b = (px_uchar)bB;

					PX_SurfaceDrawPixelWithoutLimit(psurface, x + clipw - i - 1, y + cliph - j - 1, clr);
				}
			}
			break;
		}
	}
	else
	{

		switch (mirrorMode)
		{
		case PX_TEXTURERENDER_MIRRROR_MODE_NONE:
			{
#ifdef PX_GPU_ENABLE
			if (PX_GPU_isEnable())
			{
				PX_GPU_Render(pdata + clipy * tex->width + clipx, tex->width, clipw, cliph, psurface->surfaceBuffer + y * psurface->width + x, psurface->width, PX_COLOR_FORMAT, 0x80808080);
			}
			else
			{
				for (j = 0; j < cliph; j++)
				{
					for (i = 0; i < clipw; i++)
					{
						clr = pdata[(clipy + j) * tex->width + (clipx + i)];
						PX_SurfaceDrawPixelWithoutLimit(psurface, x + i, y + j, clr);
					}
				}
			}
#else
			for (j = 0; j < cliph; j++)
			{
				for (i = 0; i < clipw; i++)
				{
					clr = pdata[(clipy + j) * tex->width + (clipx + i)];
					PX_SurfaceDrawPixelWithoutLimit(psurface, x + i, y + j, clr);
				}
			}
#endif
			}
			break;
		case PX_TEXTURERENDER_MIRRROR_MODE_H:
			for (j = 0; j < cliph; j++)
			{
				for (i = 0; i < clipw; i++)
				{
					clr = pdata[(clipy + j) * tex->width + (clipx + i)];
					PX_SurfaceDrawPixelWithoutLimit(psurface, x + clipw - i - 1, y + j, clr);
				}
			}
			break;
		case PX_TEXTURERENDER_MIRRROR_MODE_V:
			for (j = 0; j < cliph; j++)
			{
				for (i = 0; i < clipw; i++)
				{
					clr = pdata[(clipy + j) * tex->width + (clipx + i)];
					PX_SurfaceDrawPixelWithoutLimit(psurface, x+i, y + cliph - j - 1, clr);
				}
			}
			break;
		case PX_TEXTURERENDER_MIRRROR_MODE_HV:
			for (j = 0; j < cliph; j++)
			{
				for (i = 0; i < clipw; i++)
				{
					clr = pdata[(clipy + j) * tex->width + (clipx + i)];
					PX_SurfaceDrawPixelWithoutLimit(psurface, x + clipw - i - 1, y + cliph - j - 1, clr);
				}
			}
			break;
		}

	}

}

px_void PX_TextureRenderClip(px_surface* psurface, px_texture* tex, px_int x, px_int y, px_int clipx, px_int clipy, px_int clipw, px_int cliph, PX_ALIGN refPoint, PX_TEXTURERENDER_BLEND* blend)
{
	PX_TextureRenderClipMirror(psurface, tex, x, y, clipx, clipy, clipw, cliph, refPoint, blend, PX_TEXTURERENDER_MIRRROR_MODE_NONE);
}
px_void PX_TextureRenderMirror(px_surface* psurface, px_texture* tex, px_int x, px_int y, PX_ALIGN refPoint, PX_TEXTURERENDER_BLEND* blend, PX_TEXTURERENDER_MIRRROR_MODE mirrorMode)
{
	PX_TextureRenderClipMirror(psurface, tex, x, y, 0, 0, tex->width, tex->height, refPoint, blend, mirrorMode);
}
px_void PX_TextureRender(px_surface* psurface, px_texture* tex, px_int x, px_int y, PX_ALIGN refPoint, PX_TEXTURERENDER_BLEND* blend)
{
	PX_TextureRenderMirror(psurface, tex, x, y, refPoint, blend, PX_TEXTURERENDER_MIRRROR_MODE_NONE);
}

px_void PX_TextureCover(px_surface *psurface,px_texture *tex,px_int x,px_int y,PX_ALIGN refPoint)
{
	px_int left,right,top,bottom,j;

	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=tex->width/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=tex->width;
		break;
	case PX_ALIGN_LEFTMID:
		y-=tex->height/2;
		break;
	case PX_ALIGN_CENTER:
		y-=tex->height/2;
		x-=tex->width/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=tex->height/2;
		x-=tex->width;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=tex->height;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=tex->height;
		x-=tex->width/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=tex->height;
		x-=tex->width;
		break;
	}


	if (x<-tex->width)
	{
		return;
	}
	if (x>psurface->width-1)
	{
		return;
	}
	if (y<-tex->height)
	{
		return;
	}
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}

	if (x+tex->width>psurface->width)
	{
		right=psurface->width-x-1;
	}
	else
	{
		right=tex->width-1;
	}

	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}

	if (y+tex->height>psurface->height)
	{
		bottom=psurface->height-y-1;
	}
	else
	{
		bottom=tex->height-1;
	}


	for (j=top;j<=bottom;j++)
	{
		PX_memcpy(&psurface->surfaceBuffer[x+left+psurface->width*(y+j)],&tex->surfaceBuffer[tex->width*(j)+left],sizeof(px_color)*(right-left+1));
	}
	

}

px_void PX_TextureGetVisibleRange(px_texture *ptexture,px_int *pLeft,px_int *pRight,px_int *pTop,px_int *pBottom)
{
	px_int x=0,y=0;
	px_int left=ptexture->width-1,right=0,top=ptexture->height-1,bottom=0;
	for (y=0;y<ptexture->height-1;y++)
	{
		for (x=0;x<ptexture->width-1;x++)
		{
			if (PX_SurfaceGetPixel(ptexture,x,y)._argb.a)
			{
				if (x<left)
				{
					left=x;
				}
				if (x>right)
				{
					right=x;
				}
				if (y<top)
				{
					top=y;
				}
				if (y>bottom)
				{
					bottom=y;
				}
			}
		}
	}
	*pLeft=left;
	*pRight=right;
	*pTop=top;
	*pBottom=bottom;
}

px_void PX_TextureRenderPixelShader(px_surface *psurface,px_texture *tex,px_int x,px_int y,PX_ALIGN refPoint,PX_TexturePixelShader shader,px_void *ptr)
{
	px_int left,right,top,bottom,i,j;
	px_color *pdata;
	px_color clr;

	pdata=(px_color *)tex->surfaceBuffer;
	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=tex->width/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=tex->width;
		break;
	case PX_ALIGN_LEFTMID:
		y-=tex->height/2;
		break;
	case PX_ALIGN_CENTER:
		y-=tex->height/2;
		x-=tex->width/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=tex->height/2;
		x-=tex->width;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=tex->height;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=tex->height;
		x-=tex->width/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=tex->height;
		x-=tex->width;
		break;
	}


	if (x<-tex->width)
	{
		return;
	}
	if (x>psurface->width-1)
	{
		return;
	}
	if (y<-tex->height)
	{
		return;
	}
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}

	if (x+tex->width>psurface->width)
	{
		right=psurface->width-x-1;
	}
	else
	{
		right=tex->width-1;
	}

	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}

	if (y+tex->height>psurface->height)
	{
		bottom=psurface->height-y-1;
	}
	else
	{
		bottom=tex->height-1;
	}


	for (j=top;j<=bottom;j++)
	{
		for (i=left;i<=right;i++)
		{
			clr=pdata[j*tex->width+i];
			shader(psurface, tex,x+i,y+j,i,j,ptr);
		}
	}
}

px_void PX_TextureRenderRotation(px_surface *psurface,px_texture *tex,px_int x,px_int y,PX_ALIGN refPoint,PX_TEXTURERENDER_BLEND *blend,px_int Angle)
{
	PX_TextureRenderRotation_sincos(psurface,tex,x,y,refPoint,blend,PX_sin_angle((px_float)Angle),PX_cos_angle((px_float)Angle));
}

px_void PX_TextureRenderRotation_vector(px_surface *psurface,px_texture *tex,px_int x,px_int y,PX_ALIGN refPoint,PX_TEXTURERENDER_BLEND *blend,px_point2D p_vector)
{
	if (p_vector.x|| p_vector.y)
	{
		PX_TextureRenderRotation_sincos(psurface, tex, x, y, refPoint, blend, PX_Point2D_sin(p_vector), PX_Point2D_cos(p_vector));
	}
	else
	{
		PX_TextureRender(psurface, tex, x, y, refPoint, blend);
	}
	
}

px_void PX_TextureRenderRotation_sincos(px_surface *psurface,px_texture *tex,px_int x,px_int y,PX_ALIGN refPoint,PX_TEXTURERENDER_BLEND *blend,px_float sinx,px_float cosx)
{
	px_int newheight,newwidth,i,j,resHeight,resWidth;
	px_point CornerPoint[4];
	px_matrix rotMat;
	px_double SampleX,SampleY,mapX,mapY;
	px_double mixa,mixr,mixg,mixb,Weight;
	px_float invCosAgl,invSinAgl;
	px_color sampleColor;
	px_int left,right,top,bottom;



	resHeight=tex->height;
	resWidth=tex->width;

	CornerPoint[0].x=-tex->width/2.f;
	CornerPoint[0].y=-tex->height/2.f;
	CornerPoint[0].z=0;

	CornerPoint[1].x=tex->width/2.f;
	CornerPoint[1].y=-tex->height/2.f;
	CornerPoint[1].z=0;

	CornerPoint[2].x=-tex->width/2.f;
	CornerPoint[2].y=tex->height/2.f;
	CornerPoint[2].z=0;

	CornerPoint[3].x=tex->width/2.f;
	CornerPoint[3].y=tex->height/2.f;
	CornerPoint[3].z=0;

	rotMat._11=cosx;		rotMat._12=sinx;			rotMat._13=0.0f;			rotMat._14=0.0f;
	rotMat._21=-sinx;		rotMat._22=cosx;			rotMat._23=0.0f;			rotMat._24=0.0f;
	rotMat._31=0.0f;		rotMat._32=0.0f;			rotMat._33=1.0f;			rotMat._34=0.0f;
	rotMat._41=0.0f;		rotMat._42=0.0f;			rotMat._43=0.0f;			rotMat._44=1.0f;


	CornerPoint[0]=PX_PointMulMatrix(CornerPoint[0],rotMat);
	CornerPoint[1]=PX_PointMulMatrix(CornerPoint[1],rotMat);
	CornerPoint[2]=PX_PointMulMatrix(CornerPoint[2],rotMat);
	CornerPoint[3]=PX_PointMulMatrix(CornerPoint[3],rotMat);

	newheight=0;
	newwidth=0;
	for (i=0;i<4;i++)
	{
		if (CornerPoint[i].y>newheight)
		{
			newheight=(px_int)CornerPoint[i].y;
		}
		if (CornerPoint[i].x>newwidth)
		{
			newwidth=(px_int)CornerPoint[i].x;
		}
	}
	newheight=PX_TRUNC(newheight*2+2);
	newwidth=PX_TRUNC(newwidth*2+2);


	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=newwidth/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=newwidth;
		break;
	case PX_ALIGN_LEFTMID:
		y-=newheight/2;
		break;
	case PX_ALIGN_CENTER:
		y-=newheight/2;
		x-=newwidth/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=newheight/2;
		x-=newwidth;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=newheight;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=newheight;
		x-=newwidth/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=newheight;
		x-=newwidth;
		break;
	}


	if (x<-newwidth)
	{
		return;
	}
	if (x>psurface->width-1)
	{
		return;
	}
	if (y<-newheight)
	{
		return;
	}
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}

	if (x+newwidth>psurface->width)
	{
		right=psurface->width-x-1;
	}
	else
	{
		right=newwidth-1;
	}

	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}

	if (y+newheight>psurface->height)
	{
		bottom=psurface->height-y-1;
	}
	else
	{
		bottom=newheight-1;
	}


	


	invCosAgl=cosx;
	invSinAgl=-sinx;


	for (j=top;j<bottom;j++)
	{
		for (i=left;i<right;i++)
		{
			SampleX=i-newwidth/2.0+0.5;
			SampleY=j-newheight/2.0+0.5;

			mapX=SampleX*invCosAgl-SampleY*invSinAgl+resWidth/2.f;
			mapY=SampleX*invSinAgl+SampleY*invCosAgl+resHeight/2.f;

			if (mapX<-0.5||mapX>resWidth+0.5)
			{
				continue;
			}
			if (mapY<-0.5||mapY>resHeight+0.5)
			{
				continue;
			}
			mixa=0;
			mixr=0;
			mixg=0;
			mixb=0;
			//Sample 4 points
			//lt

			SampleX=(mapX-0.5f);
			SampleY=(mapY-0.5f);

			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(tex,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=(1-PX_FRAC(SampleX))*(1-PX_FRAC(SampleY));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			SampleX=(mapX+0.5f);
			SampleY=(mapY-0.5f);
			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(tex,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=PX_FRAC(mapX+0.5f)*(1-PX_FRAC(mapY-0.5f));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			SampleX=(mapX-0.5f);
			SampleY=(mapY+0.5f);
			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(tex,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=(1-PX_FRAC(SampleX))*(PX_FRAC(SampleY));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			SampleX=(mapX+0.5f);
			SampleY=(mapY+0.5f);
			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(tex,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=(PX_FRAC(SampleX))*(PX_FRAC(SampleY));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			mixa>255?mixa=255:0;
			mixr>255?mixr=255:0;
			mixg>255?mixg=255:0;
			mixb>255?mixb=255:0;
			if (blend)
			{
				px_int bA,bR,bG,bB;
				px_color clr=PX_COLOR((px_uchar)mixa,(px_uchar)mixr,(px_uchar)mixg,(px_uchar)mixb);
				px_int Ab=(px_int)(blend->alpha*128);
				px_int Rb=(px_int)(blend->hdr_R*128);
				px_int Gb=(px_int)(blend->hdr_G*128);
				px_int Bb=(px_int)(blend->hdr_B*128);

				bA=(px_int)(clr._argb.a*Ab)>>7;
				bR=(px_int)(clr._argb.r*Rb)>>7;
				bG=(px_int)(clr._argb.g*Gb)>>7;
				bB=(px_int)(clr._argb.b*Bb)>>7;

				clr._argb.a = (px_uchar)(((bA > 255) * 0xff) | bA);
				clr._argb.r = (px_uchar)(((bR > 255) * 0xff) | bR);
				clr._argb.g = (px_uchar)(((bG > 255) * 0xff) | bG);
				clr._argb.b = (px_uchar)(((bB > 255) * 0xff) | bB);

				PX_SurfaceDrawPixel(psurface,i+x,j+y,clr);
			}
			else
			{
				PX_SurfaceDrawPixel(psurface,i+x,j+y,PX_COLOR((px_uchar)mixa,(px_uchar)mixr,(px_uchar)mixg,(px_uchar)mixb));
			}

		}
	}
	return;
}

px_void PX_TextureRenderMask(px_surface *psurface,px_texture *mask_tex,px_texture *map_tex,px_int x,px_int y,PX_ALIGN refPoint,PX_TEXTURERENDER_BLEND *blend)
{
	px_int left,right,top,bottom,i,j;
	px_int bR,bG,bB,bA;
	px_color *pmapdata,*pmaskdata;
	px_color clr;

	pmaskdata=(px_color *)mask_tex->surfaceBuffer;
	pmapdata=(px_color *)map_tex->surfaceBuffer;

	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=mask_tex->width/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=mask_tex->width;
		break;
	case PX_ALIGN_LEFTMID:
		y-=mask_tex->height/2;
		break;
	case PX_ALIGN_CENTER:
		y-=mask_tex->height/2;
		x-=mask_tex->width/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=mask_tex->height/2;
		x-=mask_tex->width;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=mask_tex->height;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=mask_tex->height;
		x-=mask_tex->width/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=mask_tex->height;
		x-=mask_tex->width;
		break;
	}

	if (mask_tex->width>map_tex->width)
	{
		if (x<-map_tex->width)
		{
			return;
		}
	}
	else
	{
		if (x<-mask_tex->width)
		{
			return;
		}
	}
	
	if (x>psurface->width-1)
	{
		return;
	}
	if (mask_tex->height>map_tex->height)
	{
		if (y<-map_tex->height)
		{
			return;
		}
	}
	else
	{
		if (y<-mask_tex->height)
		{
			return;
		}
	}
	
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}
	if (mask_tex->width>map_tex->width)
	{
		if (x+mask_tex->width>psurface->width)
		{
			right=psurface->width-x-1;
		}
		else
		{
			right=mask_tex->width-1;
		}
	}
	else
	{
		if (x+map_tex->width>psurface->width)
		{
			right=psurface->width-x-1;
		}
		else
		{
			right=map_tex->width-1;
		}
	}

	

	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}
	if (mask_tex->height>map_tex->height)
	{
		if (y+mask_tex->height>psurface->height)
		{
			bottom=psurface->height-y-1;
		}
		else
		{
			bottom=mask_tex->height-1;
		}
	}
	else
	{
		if (y+map_tex->height>psurface->height)
		{
			bottom=psurface->height-y-1;
		}
		else
		{
			bottom=map_tex->height-1;
		}
	}
	
	if (blend)
	{	
		px_int Ab=(px_int)(blend->alpha*128);
		px_int Rb=(px_int)(blend->hdr_R*128);
		px_int Gb=(px_int)(blend->hdr_G*128);
		px_int Bb=(px_int)(blend->hdr_B*128);

		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				clr=pmapdata[j*map_tex->width+i];
				clr._argb.a=1*clr._argb.a*pmaskdata[j*mask_tex->width+i]._argb.a/255;
				bA=(px_int)(clr._argb.a*Ab)>>7;
				bR=(px_int)(clr._argb.r*Rb)>>7;
				bG=(px_int)(clr._argb.g*Gb)>>7;
				bB=(px_int)(clr._argb.b*Bb)>>7;

				clr._argb.a = (px_uchar)(((bA > 255) * 0xff) | bA);
				clr._argb.r = (px_uchar)(((bR > 255) * 0xff) | bR);
				clr._argb.g = (px_uchar)(((bG > 255) * 0xff) | bG);
				clr._argb.b = (px_uchar)(((bB > 255) * 0xff) | bB);
				PX_SurfaceDrawPixel(psurface,x+i,y+j,clr);
			}
		}
	}
	else
	{
		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				clr=pmapdata[j*map_tex->width+i];
				clr._argb.a=1*clr._argb.a*pmaskdata[j*mask_tex->width+i]._argb.a/255;
				PX_SurfaceDrawPixel(psurface,x+i,y+j,clr);
			}
		}
	}

}

px_bool PX_TextureCreateScale(px_memorypool *mp,px_texture *resTexture,px_int newWidth,px_int newHeight,px_texture *out)
{
	if (newWidth<=0||newHeight<=0)
	{
		PX_ASSERT();
		return PX_FALSE;
	}
	if (!PX_TextureCreate(mp,out,newWidth,newHeight))
	{
		return PX_FALSE;
	}

	if (!PX_TextureScaleToTexture(resTexture, out))
	{
		PX_TextureFree(out);
		return PX_FALSE;
	}
	return PX_TRUE;
}


px_bool PX_TextureCreateRotationRadian(px_memorypool *mp,px_texture *resTexture,px_float radian,px_texture *out)
{
    return PX_TextureCreateRotationAngle(mp,resTexture,(px_float)(radian*180/PX_PI),out);
}

px_bool PX_TextureCreateRotationAngle(px_memorypool *mp,px_texture *resTexture,px_float Angle,px_texture *out)
{
	px_int height,width,i,j,resHeight,resWidth;
	px_point CornerPoint[4];
	px_matrix rotMat;
	px_double SampleX,SampleY,mapX,mapY;
	px_double mixa,mixr,mixg,mixb,Weight;
	px_float invCosAgl,invSinAgl;
	px_color sampleColor;

	resHeight=resTexture->height;
	resWidth=resTexture->width;
	CornerPoint[0].x=-resTexture->width/2.f;
	CornerPoint[0].y=-resTexture->height/2.f;
	CornerPoint[0].z=0;

	CornerPoint[1].x=resTexture->width/2.f;
	CornerPoint[1].y=-resTexture->height/2.f;
	CornerPoint[1].z=0;

	CornerPoint[2].x=-resTexture->width/2.f;
	CornerPoint[2].y=resTexture->height/2.f;
	CornerPoint[2].z=0;

	CornerPoint[3].x=resTexture->width/2.f;
	CornerPoint[3].y=resTexture->height/2.f;
	CornerPoint[3].z=0;

	PX_MatrixRotateZ(&rotMat,Angle);
	CornerPoint[0]=PX_PointMulMatrix(CornerPoint[0],rotMat);
	CornerPoint[1]=PX_PointMulMatrix(CornerPoint[1],rotMat);
	CornerPoint[2]=PX_PointMulMatrix(CornerPoint[2],rotMat);
	CornerPoint[3]=PX_PointMulMatrix(CornerPoint[3],rotMat);

	height=0;
	width=0;
	for (i=0;i<4;i++)
	{
		if (CornerPoint[i].y>height)
		{
			height=(px_int)CornerPoint[i].y;
		}
		if (CornerPoint[i].x>width)
		{
			width=(px_int)CornerPoint[i].x;
		}
	}
	height=PX_TRUNC(height*2+2);
	width=PX_TRUNC(width*2+2);
	if (!PX_TextureCreate(mp,out,width,height))
	{
		return PX_FALSE;
	}
	invCosAgl=PX_cos_angle(-Angle);
	invSinAgl=PX_sin_angle(-Angle);
	for (j=0;j<height;j++)
	{
		for (i=0;i<width;i++)
		{
			SampleX=i-width/2.0+0.5;
			SampleY=j-height/2.0+0.5;

			mapX=SampleX*invCosAgl-SampleY*invSinAgl+resWidth/2.f;
			mapY=SampleX*invSinAgl+SampleY*invCosAgl+resHeight/2.f;

			if (mapX<-0.5||mapX>resWidth+0.5)
			{
				continue;
			}
			if (mapY<-0.5||mapY>resHeight+0.5)
			{
				continue;
			}
			mixa=0;
			mixr=0;
			mixg=0;
			mixb=0;
			//Sample 4 points
			//lt
		
			SampleX=(mapX-0.5f);
			SampleY=(mapY-0.5f);

			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(resTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=(1-PX_FRAC(SampleX))*(1-PX_FRAC(SampleY));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			SampleX=(mapX+0.5f);
			SampleY=(mapY-0.5f);
			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(resTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=PX_FRAC(mapX+0.5f)*(1-PX_FRAC(mapY-0.5f));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			SampleX=(mapX-0.5f);
			SampleY=(mapY+0.5f);
			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(resTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=(1-PX_FRAC(SampleX))*(PX_FRAC(SampleY));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			SampleX=(mapX+0.5f);
			SampleY=(mapY+0.5f);
			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(resTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=(PX_FRAC(SampleX))*(PX_FRAC(SampleY));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			mixa>255?mixa=255:0;
			mixr>255?mixr=255:0;
			mixg>255?mixg=255:0;
			mixb>255?mixb=255:0;
			 
			PX_SurfaceSetPixel(out,i,j,PX_COLOR((px_uchar)mixa,(px_uchar)mixr,(px_uchar)mixg,(px_uchar)mixb));

		}
	}
	return PX_TRUE;
}

px_bool PX_TextureCreateMirrior(px_memorypool* mp, px_texture* resTexture, PX_TEXTURERENDER_MIRRROR_MODE mirror, px_texture* out)
{
	if (PX_TextureCreate(mp, out, resTexture->width, resTexture->height))
		return PX_FALSE;

	PX_TextureRenderMirror( out, resTexture,0,0,PX_ALIGN_LEFTTOP,0, mirror);
	return PX_TRUE;
}
px_bool PX_TextureRotationAngleToTexture(px_texture *resTexture,px_float Angle,px_texture *out)
{
	px_int height,width,i,j,resHeight,resWidth;
	px_double SampleX,SampleY,mapX,mapY;
	px_double mixa,mixr,mixg,mixb,Weight;
	px_float invCosAgl,invSinAgl;
	px_color sampleColor;

	resHeight=resTexture->height;
	resWidth=resTexture->width;
	
	height=out->height;
	width=out->width;
	
	invCosAgl=PX_cos_angle(-Angle);
	invSinAgl=PX_sin_angle(-Angle);
	for (j=0;j<height;j++)
	{
		for (i=0;i<width;i++)
		{
			SampleX=i-width/2.0+0.5;
			SampleY=j-height/2.0+0.5;

			mapX=SampleX*invCosAgl-SampleY*invSinAgl+resWidth/2.f;
			mapY=SampleX*invSinAgl+SampleY*invCosAgl+resHeight/2.f;

			if (mapX<-0.5||mapX>resWidth+0.5)
			{
				continue;
			}
			if (mapY<-0.5||mapY>resHeight+0.5)
			{
				continue;
			}
			mixa=0;
			mixr=0;
			mixg=0;
			mixb=0;
			//Sample 4 points
			//lt

			SampleX=(mapX-0.5f);
			SampleY=(mapY-0.5f);

			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(resTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=(1-PX_FRAC(SampleX))*(1-PX_FRAC(SampleY));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			SampleX=(mapX+0.5f);
			SampleY=(mapY-0.5f);
			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(resTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=PX_FRAC(mapX+0.5f)*(1-PX_FRAC(mapY-0.5f));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			SampleX=(mapX-0.5f);
			SampleY=(mapY+0.5f);
			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(resTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=(1-PX_FRAC(SampleX))*(PX_FRAC(SampleY));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			SampleX=(mapX+0.5f);
			SampleY=(mapY+0.5f);
			if (SampleX>0&&(SampleX)<resWidth&&SampleY>0&&(SampleY)<resHeight)
			{
				//Sample color from resTexture
				sampleColor=PX_SURFACECOLOR(resTexture,PX_TRUNC(SampleX),PX_TRUNC(SampleY));
				Weight=(PX_FRAC(SampleX))*(PX_FRAC(SampleY));
				mixa+=sampleColor._argb.a*Weight;
				mixr+=sampleColor._argb.r*Weight;
				mixg+=sampleColor._argb.g*Weight;
				mixb+=sampleColor._argb.b*Weight;
			}

			mixa>255?mixa=255:0;
			mixr>255?mixr=255:0;
			mixg>255?mixg=255:0;
			mixb>255?mixb=255:0;

			PX_SurfaceSetPixel(out,i,j,PX_COLOR((px_uchar)mixa,(px_uchar)mixr,(px_uchar)mixg,(px_uchar)mixb));

		}
	}
	return PX_TRUE;
}

px_void PX_TextureFree(px_texture *tex)
{
	PX_SurfaceFree(tex);
}

px_int PX_TextureGetRenderRange(px_texture* tex, px_int x, px_int y, px_int clipw, px_int cliph, px_recti* render_range)
{
	px_int i, j;
	px_int left=tex->width, right=0, top=tex->height, bottom=0;
	PX_memset(render_range, 0, sizeof(px_recti));
	
	if (x<0)
	{
		x = 0;
	}
	if (x>=tex->width)
	{
		return 0;
	}
	if (y < 0)
	{
		y=0;
	}
	if (y >= tex->height)
	{
		return 0;
	}
	if (x+clipw>tex->width)
	{
		clipw = tex->width - x;
	}

	if (y + cliph > tex->height)
	{
		cliph  =tex->height-y;
	}

	for (j = y; j < cliph; j++)
	{
		for (i = x; i < clipw; i++)
		{
			if (PX_SurfaceGetPixel(tex, x, y)._argb.a)
			{
				if (i < left)
				{
					left = i;
				}
				if (i > right)
				{
					right = i;
				}
				if (j < top)
				{
					top = j;
				}
				if (j > bottom)
				{
					bottom = j;
				}
			}
		}
	}
	if (right < left)
	{
		return 0;
	}
	else
	{
		render_range->x = left;
		render_range->y = top;
		render_range->width = right-left+1;
		render_range->height = bottom-top+1;
		return render_range->width * render_range->height;
	}
}



px_bool PX_TextureDiffZip(px_memorypool* mp, px_texture* srctex, px_texture* ziptex, px_memory* mem)
{
	px_int i,count;
	px_byte* p1, * p2,*p3; if (srctex->width != ziptex->width || srctex->height != ziptex->height)
	{
		return PX_FALSE;
	}
	p1 = (px_byte*)srctex->surfaceBuffer;
	p2 = (px_byte*)ziptex->surfaceBuffer;
	count = srctex->width * srctex->height;
	p3 = (px_byte*)MP_Malloc(mp, count*4);
	for (i = 0; i < count; i++)
	{
		p3[i] = p2[i*4] - p1[i*4];
		p3[i + count] = p2[i*4 + 1] - p1[i*4 + 1];
		p3[i + count * 2] = p2[i*4 + 2] - p1[i*4 + 2];
		p3[i + count * 3] = p2[i*4 + 3] - p1[i*4 + 3];
	}
	if(!PX_ArleCompress(p3, count*4,mem))
	{
		MP_Free(mp, p3);
		return PX_FALSE;
	}

	MP_Free(mp, p3);
	return PX_TRUE;
}

px_bool PX_TextureDiffUnzip(px_memorypool* mp,px_void *buffer,px_int size, px_texture* srctex, px_texture* tex)
{
	px_int i, count;
	px_byte* p1, * p2,*p3;
	px_memory rleout;
	if (srctex->width != tex->width || srctex->height != tex->height)
	{
		return PX_FALSE;
	}
	p1 = (px_byte*)srctex->surfaceBuffer;
	p2 = (px_byte*)tex->surfaceBuffer;
	count = srctex->width * srctex->height;
	PX_MemoryInitialize(mp, &rleout);
	if (!PX_ArleDecompress((px_byte *)buffer, size, &rleout))
	{
		PX_MemoryFree(&rleout);
		return PX_FALSE;
	}
	if (rleout.usedsize!= count * 4)
	{
		PX_MemoryFree(&rleout);
		return PX_FALSE;
	}
	p3 = (px_byte*)rleout.buffer;
	for (i = 0; i < count; i++)
	{
		p2[i*4] = p1[i*4] + p3[i];
		p2[i*4 + 1] = p1[i*4 + 1] + p3[i + count];
		p2[i*4 + 2] = p1[i*4 + 2] + p3[i + count * 2];
		p2[i*4 + 3] = p1[i*4 + 3] + p3[i + count * 3];
	}
	
	PX_MemoryFree(&rleout);
	return PX_TRUE;
}


px_void PX_SurfaceRender(px_surface *psurface,px_surface *surface,px_int x,px_int y,PX_ALIGN refPoint,PX_TEXTURERENDER_BLEND *blend)
{
	PX_TextureRender(psurface,surface,x,y,refPoint,blend);
}

px_void PX_SurfaceCover(px_surface *pdestSurface,px_surface *pResSurface,px_int x,px_int y,PX_ALIGN refPoint)
{
	PX_TextureCover(pdestSurface,pResSurface,x,y,refPoint);
}

px_void PX_SurfaceSetRect(px_surface *psurface, px_int left, px_int top, px_int right, px_int bottom,px_color color)
{
	px_int i,mid;

	if (left>right)
	{
		mid=left;
		left=right;
		right=mid;
	}
	if (top>bottom)
	{
		mid=top;
		top=bottom;
		bottom=mid;
	}
	if (left<0)
	{
		left=0;
	}
	if (top<0)
	{
		top=0;
	}
	if (left>psurface->width-1)
	{
		return;
	}

	if (bottom>psurface->height-1)
	{
		return;
	}

	if (right>psurface->width-1)
	{
		right=psurface->width-1;
	}

	if (bottom>psurface->height-1)
	{
		right=psurface->height-1;
	}

	for (i=top;i<=bottom;i++)
	{
		PX_memdwordset(psurface->surfaceBuffer+i*psurface->width+left,color._argb.ucolor,right-left+1);
	}
}



px_void PX_TextureRenderEx(px_surface *psurface,px_texture *resTexture,px_int x,px_int y,PX_ALIGN refPoint,PX_TEXTURERENDER_BLEND *blend,px_float scale,px_float Angle)
{
	px_int newWidth,newHeight;
	
	px_int i,j,resHeight,resWidth;

	px_float invCosAgl,invSinAgl;

	px_double ref_x,ref_y;
	px_int mapX,mapY;

	px_color clr;
	px_int bA,bR,bG,bB;

	//////////////////////////////////////////////////////////////////////////
	px_int left,right,top,bottom;

	if (scale==1&& Angle==0)
	{
		PX_TextureRender(psurface, resTexture, x, y, refPoint, blend);
		return;
	}

	//////////////////////////////////////////////////////////////////////////

	resHeight=resTexture->height;
	resWidth=resTexture->width;

	newHeight=PX_TRUNC(resHeight*scale);
	newWidth=PX_TRUNC(resWidth*scale);

	//////////////////////////////////////////////////////////////////////////
	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=newWidth;
		break;
	case PX_ALIGN_LEFTMID:
		y-=newHeight/2;
		break;
	case PX_ALIGN_CENTER:
		y-=newHeight/2;
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=newHeight/2;
		x-=newWidth;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=newHeight;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=newHeight;
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=newHeight;
		x-=newWidth;
		break;
	}


	if (x<-newWidth)
	{
		return;
	}
	if (x>psurface->width-1)
	{
		return;
	}
	if (y<-newHeight)
	{
		return;
	}
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}

	if (x+newWidth>psurface->width)
	{
		right=psurface->width-x-1;
	}
	else
	{
		right=newWidth-1;
	}

	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}

	if (y+newHeight>psurface->height)
	{
		bottom=psurface->height-y-1;
	}
	else
	{
		bottom=newHeight-1;
	}
	//////////////////////////////////////////////////////////////////////////

	

	invCosAgl=PX_cos_angle(-Angle);
	invSinAgl=PX_sin_angle(-Angle);

	if (blend)
	{	
		px_int Ab=(px_int)(blend->alpha*128);
		px_int Rb=(px_int)(blend->hdr_R*128);
		px_int Gb=(px_int)(blend->hdr_G*128);
		px_int Bb=(px_int)(blend->hdr_B*128);

		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				ref_x = (i - newWidth / 2.0);
				ref_y = (j - newHeight / 2.0);

				mapX = (px_int)((ref_x * invCosAgl - ref_y * invSinAgl) / scale + resWidth / 2);
				mapY = (px_int)((ref_x * invSinAgl + ref_y * invCosAgl) / scale + resHeight / 2);

				if (mapX<0||mapX>=resWidth)
				{
					continue;
				}

				if (mapY<0||mapY>=resHeight)
				{
					continue;
				}

				clr=PX_SURFACECOLOR(resTexture,mapX,mapY);
				bA=(px_int)(clr._argb.a*Ab)>>7;
				bR=(px_int)(clr._argb.r*Rb)>>7;
				bG=(px_int)(clr._argb.g*Gb)>>7;
				bB=(px_int)(clr._argb.b*Bb)>>7;

				clr._argb.a = (px_uchar)(((bA > 255) * 0xff) | bA);
				clr._argb.r = (px_uchar)(((bR > 255) * 0xff) | bR);
				clr._argb.g = (px_uchar)(((bG > 255) * 0xff) | bG);
				clr._argb.b = (px_uchar)(((bB > 255) * 0xff) | bB);


				PX_SurfaceDrawPixel(psurface,x+i,y+j,clr);
			}

		}

	}
	else
	{
		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				ref_x = (i - newWidth / 2.0);
				ref_y=  (j - newHeight / 2.0);

				mapX=(px_int)((ref_x*invCosAgl-ref_y*invSinAgl)/scale+resWidth/2);
				mapY=(px_int)((ref_x*invSinAgl+ref_y*invCosAgl)/scale+resHeight/2);

				if (mapX<0||mapX>=resWidth)
				{
					continue;
				}

				if (mapY<0||mapY>=resHeight)
				{
					continue;
				}

				PX_SurfaceDrawPixel(psurface,x+i,y+j,PX_SURFACECOLOR(resTexture,mapX,mapY));
			}

		}
	}
}

px_void PX_TextureRenderMaskEx(px_surface *psurface,px_texture *mask_tex,px_texture *map_tex,px_int x,px_int y,PX_ALIGN refPoint,PX_TEXTURERENDER_BLEND *blend,px_float scale,px_float Angle)
{
	px_int newWidth,newHeight;

	px_int i,j,resHeight,resWidth;
	px_int bA,bB,bG,bR;
	px_float invCosAgl,invSinAgl;

	px_double ref_x,ref_y;
	px_int mapX,mapY;

	px_color *pmapdata,*pmaskdata;
	px_color clr;


	//////////////////////////////////////////////////////////////////////////
	px_int left,right,top,bottom;


	//////////////////////////////////////////////////////////////////////////


	pmaskdata=(px_color *)mask_tex->surfaceBuffer;
	pmapdata=(px_color *)map_tex->surfaceBuffer;

	resHeight=mask_tex->height;
	resWidth=mask_tex->width;

	newHeight=PX_TRUNC(resHeight*scale);
	newWidth=PX_TRUNC(resWidth*scale);

	//////////////////////////////////////////////////////////////////////////
	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=newWidth;
		break;
	case PX_ALIGN_LEFTMID:
		y-=newHeight/2;
		break;
	case PX_ALIGN_CENTER:
		y-=newHeight/2;
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=newHeight/2;
		x-=newWidth;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=newHeight;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=newHeight;
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=newHeight;
		x-=newWidth;
		break;
	}


	if (x<-newWidth)
	{
		return;
	}
	if (x>psurface->width-1)
	{
		return;
	}
	if (y<-newHeight)
	{
		return;
	}
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}

	if (newWidth>map_tex->width)
	{
		if (x+map_tex->width>psurface->width)
		{
			right=psurface->width-x-1;
		}
		else
		{
			right=map_tex->width-1;
		}
	}
	else
	{
		if (x+newWidth>psurface->width)
		{
			right=psurface->width-x-1;
		}
		else
		{
			right=newWidth-1;
		}
	}


	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}

	if (newHeight>map_tex->height)
	{
		if (y+newHeight>psurface->height)
		{
			bottom=psurface->height-y-1;
		}
		else
		{
			bottom=newHeight-1;
		}
	}
	else
	{
		if (y+newHeight>psurface->height)
		{
			bottom=psurface->height-y-1;
		}
		else
		{
			bottom=newHeight-1;
		}
	}

	//////////////////////////////////////////////////////////////////////////


	invCosAgl=PX_cos_angle(-Angle);
	invSinAgl=PX_sin_angle(-Angle);

	if (blend)
	{	
		px_int Ab=(px_int)(blend->alpha*128);
		px_int Rb=(px_int)(blend->hdr_R*128);
		px_int Gb=(px_int)(blend->hdr_G*128);
		px_int Bb=(px_int)(blend->hdr_B*128);

		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				ref_x = (i - newWidth / 2.0);
				ref_y = (j - newHeight / 2.0);

				mapX = (px_int)((ref_x * invCosAgl - ref_y * invSinAgl) / scale + resWidth / 2);
				mapY = (px_int)((ref_x * invSinAgl + ref_y * invCosAgl) / scale + resHeight / 2);

				if (mapX<0||mapX>=resWidth)
				{
					continue;
				}

				if (mapY<0||mapY>=resHeight)
				{
					continue;
				}

				clr=pmapdata[j*map_tex->width+i];
				clr._argb.a=1*clr._argb.a*pmaskdata[j*mask_tex->width+i]._argb.a/255;

				bA=(px_int)(clr._argb.a*Ab)>>7;
				bR=(px_int)(clr._argb.r*Rb)>>7;
				bG=(px_int)(clr._argb.g*Gb)>>7;
				bB=(px_int)(clr._argb.b*Bb)>>7;

				clr._argb.a = (px_uchar)(((bA > 255) * 0xff) | bA);
				clr._argb.r = (px_uchar)(((bR > 255) * 0xff) | bR);
				clr._argb.g = (px_uchar)(((bG > 255) * 0xff) | bG);
				clr._argb.b = (px_uchar)(((bB > 255) * 0xff) | bB);
				PX_SurfaceDrawPixel(psurface,x+i,y+j,clr);
			}

		}

	}
	else
	{
		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				ref_x = (i - newWidth / 2.0);
				ref_y = (j - newHeight / 2.0);

				mapX = (px_int)((ref_x * invCosAgl - ref_y * invSinAgl) / scale + resWidth / 2);
				mapY = (px_int)((ref_x * invSinAgl + ref_y * invCosAgl) / scale + resHeight / 2);

				if (mapX<0||mapX>=resWidth)
				{
					continue;
				}

				if (mapY<0||mapY>=resHeight)
				{
					continue;
				}
				clr=pmapdata[j*map_tex->width+i];
				clr._argb.a=1*clr._argb.a*pmaskdata[j*mask_tex->width+i]._argb.a/255;

				PX_SurfaceDrawPixel(psurface,x+i,y+j,clr);
			}

		}
	}

}

px_bool PX_TextureFillTestColor(px_color c1, px_color c2, px_float range)
{
	px_float  r, g, b;

	if (c1._argb.ucolor == c2._argb.ucolor)
	{
		return PX_TRUE;
	}

	if (range==0)
	{
		return c1._argb.ucolor == c2._argb.ucolor;
	}
	else
	{
		r = (c1._argb.r - c2._argb.r) * (c1._argb.r - c2._argb.r) / 65535.f;
		g = (c1._argb.g - c2._argb.g) * (c1._argb.g - c2._argb.g) / 65535.f;
		b = (c1._argb.b - c2._argb.b) * (c1._argb.b - c2._argb.b) / 65535.f;

		if (r<=range&& g <= range && b <= range && c1._argb.a <= range * 255)
		{
			return PX_TRUE;
		}
		else
		{
			return PX_FALSE;
		}
	}
	
}

px_void PX_TextureFill(px_memorypool *mp,px_texture *ptexture,px_int x,px_int y,px_color test_color,px_float testRange0_1,px_color fill_color)
{
	typedef struct
	{
		px_int x,y;
	}__PX_POINT;
	px_vector pstack;

	if (fill_color._argb.ucolor==test_color._argb.ucolor)
	{
		return;
	}

	PX_VectorInitialize(mp,&pstack,sizeof(__PX_POINT),8);
	
	do 
	{
		__PX_POINT pt;
		pt.x=x;
		pt.y=y;
		PX_VectorPushback(&pstack,&pt);
	} while (0);

	while (pstack.size)
	{
		px_bool up_newnode,down_newnode;
		__PX_POINT retPt=*PX_VECTORLAST(__PX_POINT,&pstack);
		PX_VectorPop(&pstack);

		x=retPt.x;
		y=retPt.y;

		//xo mark
		while (x>0&&PX_TextureFillTestColor(PX_SurfaceGetPixel(ptexture,x-1,y),test_color, testRange0_1)&& (PX_SurfaceGetPixel(ptexture, x + 1, y)._argb.ucolor != fill_color._argb.ucolor))
		{
			x--;
		}

		up_newnode = PX_TRUE;
		down_newnode = PX_TRUE;
		for (; x< ptexture->width; x++)
		{
			PX_SurfaceSetPixel(ptexture, x, y, fill_color);
			//up
			if (y > 0)
			{
				px_color targettestcolor = PX_SurfaceGetPixel(ptexture, x, y - 1);

				if (PX_TextureFillTestColor(targettestcolor, test_color, testRange0_1)&& targettestcolor._argb.ucolor!= fill_color._argb.ucolor)
				{
					if (up_newnode)
					{
						__PX_POINT pt;
						pt.x = x;
						pt.y = y - 1;
						PX_VectorPushback(&pstack, &pt);
						up_newnode = PX_FALSE;
					}
					
				}
				else
				{
					up_newnode = PX_TRUE;
				}
			}

			//down
			if (y < ptexture->height - 1)
			{
				px_color targettestcolor = PX_SurfaceGetPixel(ptexture, x, y + 1);

				if (PX_TextureFillTestColor(PX_SurfaceGetPixel(ptexture, x, y + 1), test_color, testRange0_1) && targettestcolor._argb.ucolor != fill_color._argb.ucolor)
				{
					if (down_newnode)
					{
						__PX_POINT pt;
						pt.x = x;
						pt.y = y + 1;
						PX_VectorPushback(&pstack, &pt);
						down_newnode = PX_FALSE;
					}

				}
				else
				{
					down_newnode = PX_TRUE;
				}
			}
			//right


			if (!PX_TextureFillTestColor(PX_SurfaceGetPixel(ptexture, x+1, y ), test_color, testRange0_1)||\
				(PX_SurfaceGetPixel(ptexture, x+1, y)._argb.ucolor == fill_color._argb.ucolor))
			{
				break;
			}
		}
	}
	PX_VectorFree(&pstack);
}

px_void PX_TextureRegionRender(px_surface *psurface,px_texture *resTexture,px_int x,px_int y,px_int oft_left,px_int oft_top,px_int oft_right,px_int oft_bottom,PX_ALIGN refPoint,PX_TEXTURERENDER_BLEND *blend)
{
	px_int left,right,top,bottom,i,j;
	px_int bR,bG,bB,bA;
	px_color *pdata;
	px_color clr;
	px_int width=oft_right-oft_left+1;
	px_int height=oft_bottom-oft_top+1;

	pdata=(px_color *)resTexture->surfaceBuffer;

	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=width/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=width;
		break;
	case PX_ALIGN_LEFTMID:
		y-=height/2;
		break;
	case PX_ALIGN_CENTER:
		y-=height/2;
		x-=width/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=height/2;
		x-=width;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=height;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=height;
		x-=width/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=height;
		x-=width;
		break;
	}


	if (x<-width)
	{
		return;
	}
	if (x>psurface->width-1)
	{
		return;
	}
	if (y<-height)
	{
		return;
	}
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}

	if (x+width>psurface->width)
	{
		right=psurface->width-x-1;
	}
	else
	{
		right=width-1;
	}

	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}

	if (y+height>psurface->height)
	{
		bottom=psurface->height-y-1;
	}
	else
	{
		bottom=height-1;
	}


	if (blend)
	{	
		px_int Ab=(px_int)(blend->alpha*128);
		px_int Rb=(px_int)(blend->hdr_R*128);
		px_int Gb=(px_int)(blend->hdr_G*128);
		px_int Bb=(px_int)(blend->hdr_B*128);

		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				clr=pdata[(j+oft_top)*resTexture->width+i+oft_left];
				bA=(px_int)(clr._argb.a*Ab)>>7;
				bR=(px_int)(clr._argb.r*Rb)>>7;
				bG=(px_int)(clr._argb.g*Gb)>>7;
				bB=(px_int)(clr._argb.b*Bb)>>7;

				clr._argb.a = (px_uchar)(((bA > 255) * 0xff) | bA);
				clr._argb.r = (px_uchar)(((bR > 255) * 0xff) | bR);
				clr._argb.g = (px_uchar)(((bG > 255) * 0xff) | bG);
				clr._argb.b = (px_uchar)(((bB > 255) * 0xff) | bB);
				PX_SurfaceDrawPixel(psurface,x+i,y+j,clr);
			}
		}
	}
	else
	{
		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				clr=pdata[(j+oft_top)*resTexture->width+i+oft_left];
				PX_SurfaceDrawPixel(psurface,x+i,y+j,clr);
			}
		}
	}
}
px_void PX_TextureRegionCopy(px_surface *psurface,px_texture *resTexture,px_int x,px_int y,px_int oft_left,px_int oft_top,px_int oft_right,px_int oft_bottom,PX_ALIGN refPoint,PX_TEXTURERENDER_BLEND *blend)
{
	px_int left,right,top,bottom,i,j;
	px_int bR,bG,bB,bA;
	px_color *pdata;
	px_color clr;
	px_int width=oft_right-oft_left+1;
	px_int height=oft_bottom-oft_top+1;

	pdata=(px_color *)resTexture->surfaceBuffer;

	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=width/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=width;
		break;
	case PX_ALIGN_LEFTMID:
		y-=height/2;
		break;
	case PX_ALIGN_CENTER:
		y-=height/2;
		x-=width/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=height/2;
		x-=width;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=height;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=height;
		x-=width/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=height;
		x-=width;
		break;
	}


	if (x<-width)
	{
		return;
	}
	if (x>psurface->width-1)
	{
		return;
	}
	if (y<-height)
	{
		return;
	}
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}

	if (x+width>psurface->width)
	{
		right=psurface->width-x-1;
	}
	else
	{
		right=width-1;
	}

	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}

	if (y+height>psurface->height)
	{
		bottom=psurface->height-y-1;
	}
	else
	{
		bottom=height-1;
	}


	if (blend)
	{	
		px_int Ab=(px_int)(blend->alpha*128);
		px_int Rb=(px_int)(blend->hdr_R*128);
		px_int Gb=(px_int)(blend->hdr_G*128);
		px_int Bb=(px_int)(blend->hdr_B*128);

		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				clr=pdata[(j+oft_top)*resTexture->width+i+oft_left];
				bA=(px_int)(clr._argb.a*Ab)>>7;
				bR=(px_int)(clr._argb.r*Rb)>>7;
				bG=(px_int)(clr._argb.g*Gb)>>7;
				bB=(px_int)(clr._argb.b*Bb)>>7;

				clr._argb.a = (px_uchar)(((bA > 255) * 0xff) | bA);
				clr._argb.r = (px_uchar)(((bR > 255) * 0xff) | bR);
				clr._argb.g = (px_uchar)(((bG > 255) * 0xff) | bG);
				clr._argb.b = (px_uchar)(((bB > 255) * 0xff) | bB);
				PX_SurfaceSetPixel(psurface,x+i,y+j,clr);
			}
		}
	}
	else
	{
		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				clr=pdata[(j+oft_top)*resTexture->width+i+oft_left];
				PX_SurfaceSetPixel(psurface,x+i,y+j,clr);
			}
		}
	}
}

px_bool PX_ShapeCreate(px_memorypool *mp,px_shape *shape,px_int width,px_int height)
{
	px_void *p=MP_Malloc(mp,height*width*sizeof(px_uchar));
	if (p!=PX_NULL)
	{
		shape->height=height;
		shape->width=width;
		shape->alpha=(px_uchar *)p;
		shape->MP=mp;
		PX_memset(p,0,height*width);
		return PX_TRUE;
	}
	return PX_FALSE;
}



px_bool PX_ShapeCreateFromTexture(px_memorypool *mp,px_shape *shape,px_texture *texture)
{
	px_int x,y;
	px_void *p=MP_Malloc(mp,texture->height*texture->width*sizeof(px_uchar));
	if (p!=PX_NULL)
	{
		shape->height=texture->height;
		shape->width=texture->width;
		shape->alpha=(px_uchar *)p;
		shape->MP=mp;
		PX_memdwordset(p,0,texture->height*texture->width);

		//Map texture to shape
		for (y=0;y<texture->height;y++)
		{
			for (x=0;x<texture->width;x++)
			{
				shape->alpha[y*shape->width+x]=((px_color *)texture->surfaceBuffer)[y*texture->width+x]._argb.a;
			}
		}
		return PX_TRUE;
	}
	return PX_FALSE;
}

px_void PX_ShapeFree(px_shape *shape)
{
	MP_Free(shape->MP,shape->alpha);
}

px_void PX_ShapeSetPixel(px_shape* shape, px_int x, px_int y, px_uchar value)
{
	px_uchar* pdata;
	pdata = (px_uchar*)shape->alpha;
	if (x>=0&&y>=0&&x<shape->width&&y<shape->height)
	{
		pdata[y * shape->width + x] = value;
	}
}

px_uchar PX_ShapeGetPixel(px_shape* shape, px_int x, px_int y)
{
 	px_uchar *pdata;
	pdata  =(px_uchar *)shape->alpha;
	if (x >= 0 && y >= 0 && x < shape->width && y < shape->height)
	{
		return pdata[y  *shape->width+x];
	}
	return 0;
}

px_void PX_ShapeRender(px_surface *psurface,px_shape *shape,px_int x,px_int y,PX_ALIGN refPoint,px_color blendColor)
{
	px_int left,right,top,bottom,i,j;
	px_uchar *pdata;
	px_color clr;

	pdata=(px_uchar *)shape->alpha;

	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=shape->width/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=shape->width;
		break;
	case PX_ALIGN_LEFTMID:
		y-=shape->height/2;
		break;
	case PX_ALIGN_CENTER:
		y-=shape->height/2;
		x-=shape->width/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=shape->height/2;
		x-=shape->width;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=shape->height;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=shape->height;
		x-=shape->width/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=shape->height;
		x-=shape->width;
		break;
	}


	if (x<-shape->width)
	{
		return;
	}
	if (x>psurface->width-1)
	{
		return;
	}
	if (y<-shape->height)
	{
		return;
	}
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}

	if (x+shape->width>psurface->width)
	{
		right=psurface->width-x-1;
	}
	else
	{
		right=shape->width-1;
	}

	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}

	if (y+shape->height>psurface->height)
	{
		bottom=psurface->height-y-1;
	}
	else
	{
		bottom=shape->height-1;
	}

	for (j=top;j<=bottom;j++)
	{
		for (i=left;i<=right;i++)
		{
			if (pdata[j*shape->width+i]!=0)
			{
				clr=blendColor;
				clr._argb.a=1*clr._argb.a*pdata[j*shape->width+i]/255;
				PX_SurfaceDrawPixel(psurface,x+i,y+j,clr);
			}

		}
	}

}



px_void PX_ShapeRenderEx(px_surface *psurface,px_shape *shape,px_int x,px_int y,PX_ALIGN refPoint,px_color blendColor,px_float scale,px_float Angle)
{
	px_int newWidth,newHeight;

	px_int i,j,resHeight,resWidth;

	px_float invCosAgl,invSinAgl;

	px_double ref_x,ref_y;
	px_int mapX,mapY;

	px_color clr;

	//////////////////////////////////////////////////////////////////////////
	px_int left,right,top,bottom;


	//////////////////////////////////////////////////////////////////////////

	resHeight=shape->height;
	resWidth=shape->width;

	newHeight=PX_TRUNC(resHeight*scale);
	newWidth=PX_TRUNC(resWidth*scale);

	//////////////////////////////////////////////////////////////////////////
	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=newWidth;
		break;
	case PX_ALIGN_LEFTMID:
		y-=newHeight/2;
		break;
	case PX_ALIGN_CENTER:
		y-=newHeight/2;
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=newHeight/2;
		x-=newWidth;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=newHeight;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=newHeight;
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=newHeight;
		x-=newWidth;
		break;
	}


	if (x<-newWidth)
	{
		return;
	}
	if (x>psurface->width-1)
	{
		return;
	}
	if (y<-newHeight)
	{
		return;
	}
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}

	if (x+newWidth>psurface->width)
	{
		right=psurface->width-x-1;
	}
	else
	{
		right=newWidth-1;
	}

	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}

	if (y+newHeight>psurface->height)
	{
		bottom=psurface->height-y-1;
	}
	else
	{
		bottom=newHeight-1;
	}
	//////////////////////////////////////////////////////////////////////////



	invCosAgl=PX_cos_angle(-Angle);
	invSinAgl=PX_sin_angle(-Angle);

	for (j=top;j<=bottom;j++)
	{
		for (i=left;i<=right;i++)
		{
			ref_x = (i - newWidth / 2.0);
			ref_y = (j - newHeight / 2.0);

			mapX = (px_int)((ref_x * invCosAgl - ref_y * invSinAgl) / scale + resWidth / 2);
			mapY = (px_int)((ref_x * invSinAgl + ref_y * invCosAgl) / scale + resHeight / 2);

			if (mapX<0||mapX>=resWidth)
			{
				continue;
			}

			if (mapY<0||mapY>=resHeight)
			{
				continue;
			}
			clr=blendColor;
			clr._argb.a=1*clr._argb.a*shape->alpha[mapY*shape->width+mapX]/255;
			PX_SurfaceDrawPixel(psurface,x+i,y+j,clr);
		}

	}

}

px_bool PX_ShapeCreateFromMemory(px_memorypool *mp,px_void *data,px_int size,px_shape *shape)
{
	px_int width;
	px_int height;
	if (PX_BitmapVerify(data,size))
	{
		return PX_FALSE;
	}

	if (PX_TRawVerify(data,size))
	{
		width=PX_TRawGetWidth(data);
		height=PX_TRawGetHeight(data);
		if(PX_ShapeCreate(mp,shape,width,height))
		{
			px_int i,j;
			px_color  *pdataColor=(px_color *)((px_char *)data+sizeof(PX_TRaw_Header));

			for (i=0;i<height;i++)
			{
				for (j=0;j<width;j++)
				{
					shape->alpha[width*i+j]=pdataColor[width*i+j]._argb.a;
				}
			}
		}
		else
		{
			return PX_FALSE;
		}
	}


	if (PX_PngVerify((px_byte*)data, size, &width, &height, 0))
	{
		if (PX_ShapeCreate(mp, shape, width, height))
		{
			if (!PX_PngToShapeBuffer(mp, (px_byte*)data, size, shape))
				return PX_FALSE;
			return PX_TRUE;
		}
		else
		{
			return PX_FALSE;
		}
	}


	return PX_TRUE;
}

px_void PX_ShapeRenderEx_sincos(px_surface *psurface,px_shape *shape,px_int x,px_int y,PX_ALIGN refPoint,px_color blendColor,px_float scale,px_float sinx,px_float cosx)
{
	px_int newWidth,newHeight;

	px_int i,j,resHeight,resWidth;

	px_float invCosAgl,invSinAgl;

	px_double ref_x,ref_y;
	px_int mapX,mapY;

	px_color clr;

	//////////////////////////////////////////////////////////////////////////
	px_int left,right,top,bottom;


	//////////////////////////////////////////////////////////////////////////

	resHeight=shape->height;
	resWidth=shape->width;

	newHeight=PX_TRUNC(resHeight*scale);
	newWidth=PX_TRUNC(resWidth*scale);

	//////////////////////////////////////////////////////////////////////////
	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=newWidth;
		break;
	case PX_ALIGN_LEFTMID:
		y-=newHeight/2;
		break;
	case PX_ALIGN_CENTER:
		y-=newHeight/2;
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=newHeight/2;
		x-=newWidth;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=newHeight;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=newHeight;
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=newHeight;
		x-=newWidth;
		break;
	}


	if (x<-newWidth)
	{
		return;
	}
	if (x>psurface->width-1)
	{
		return;
	}
	if (y<-newHeight)
	{
		return;
	}
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}

	if (x+newWidth>psurface->width)
	{
		right=psurface->width-x-1;
	}
	else
	{
		right=newWidth-1;
	}

	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}

	if (y+newHeight>psurface->height)
	{
		bottom=psurface->height-y-1;
	}
	else
	{
		bottom=newHeight-1;
	}
	//////////////////////////////////////////////////////////////////////////



	invCosAgl=cosx;
	invSinAgl=sinx;

	for (j=top;j<=bottom;j++)
	{
		for (i=left;i<=right;i++)
		{
			ref_x = (i - newWidth / 2.0);
			ref_y = (j - newHeight / 2.0);

			mapX = (px_int)((ref_x * invCosAgl - ref_y * invSinAgl) / scale + resWidth / 2);
			mapY = (px_int)((ref_x * invSinAgl + ref_y * invCosAgl) / scale + resHeight / 2);

			if (mapX<0||mapX>=resWidth)
			{
				continue;
			}

			if (mapY<0||mapY>=resHeight)
			{
				continue;
			}
			clr=blendColor;
			clr._argb.a=1*clr._argb.a*shape->alpha[shape->width*mapY+mapX]/255;
			PX_SurfaceDrawPixel(psurface,x+i,y+j,clr);
		}

	}
}

px_void PX_ShapeRenderEx_vector(px_surface *psurface,px_shape *shape,px_int x,px_int y,PX_ALIGN refPoint,px_color blendColor,px_float scale,px_point2D p_vector)
{
	PX_ShapeRenderEx_sincos(psurface,shape,x,y,refPoint,blendColor,scale,PX_Point2D_sin(p_vector),PX_Point2D_cos(p_vector));
}

px_void PX_TextureRenderEx_sincos(px_surface *psurface,px_texture *resTexture,px_int x,px_int y,PX_ALIGN refPoint,PX_TEXTURERENDER_BLEND *blend,px_float scale,px_float sinx,px_float cosx)
{
	px_int newWidth,newHeight;

	px_int i,j,resHeight,resWidth;

	px_float invCosAgl,invSinAgl;

	px_double ref_x,ref_y;
	px_int mapX,mapY;

	px_color clr;
	px_int bA,bR,bG,bB;

	//////////////////////////////////////////////////////////////////////////
	px_int left,right,top,bottom;


	//////////////////////////////////////////////////////////////////////////

	resHeight=resTexture->height;
	resWidth=resTexture->width;

	newHeight=PX_TRUNC(resHeight*scale);
	newWidth=PX_TRUNC(resWidth*scale);

	//////////////////////////////////////////////////////////////////////////
	switch (refPoint)
	{
	case PX_ALIGN_LEFTTOP:
		break;
	case PX_ALIGN_MIDTOP:
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTTOP:
		x-=newWidth;
		break;
	case PX_ALIGN_LEFTMID:
		y-=newHeight/2;
		break;
	case PX_ALIGN_CENTER:
		y-=newHeight/2;
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTMID:
		y-=newHeight/2;
		x-=newWidth;
		break;
	case PX_ALIGN_LEFTBOTTOM:
		y-=newHeight;
		break;
	case PX_ALIGN_MIDBOTTOM:
		y-=newHeight;
		x-=newWidth/2;
		break;
	case PX_ALIGN_RIGHTBOTTOM:
		y-=newHeight;
		x-=newWidth;
		break;
	}


	if (x<-newWidth)
	{
		return;
	}
	if (x>psurface->width-1)
	{
		return;
	}
	if (y<-newHeight)
	{
		return;
	}
	if (y>psurface->height-1)
	{
		return;
	}

	if (x<0)
	{
		left=-x;
	}
	else
	{
		left=0;
	}

	if (x+newWidth>psurface->width)
	{
		right=psurface->width-x-1;
	}
	else
	{
		right=newWidth-1;
	}

	if (y<0)
	{
		top=-y;
	}
	else
	{
		top=0;
	}

	if (y+newHeight>psurface->height)
	{
		bottom=psurface->height-y-1;
	}
	else
	{
		bottom=newHeight-1;
	}
	//////////////////////////////////////////////////////////////////////////



	invCosAgl=cosx;
	invSinAgl=-sinx;

	if (blend)
	{	
		px_int Ab=(px_int)(blend->alpha*128);
		px_int Rb=(px_int)(blend->hdr_R*128);
		px_int Gb=(px_int)(blend->hdr_G*128);
		px_int Bb=(px_int)(blend->hdr_B*128);

		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				ref_x = (i - newWidth / 2.0);
				ref_y = (j - newHeight / 2.0);

				mapX = (px_int)((ref_x * invCosAgl - ref_y * invSinAgl) / scale + resWidth / 2);
				mapY = (px_int)((ref_x * invSinAgl + ref_y * invCosAgl) / scale + resHeight / 2);

				if (mapX<0||mapX>=resWidth)
				{
					continue;
				}

				if (mapY<0||mapY>=resHeight)
				{
					continue;
				}

				clr=PX_SURFACECOLOR(resTexture,mapX,mapY);
				bA=(px_int)(clr._argb.a*Ab)>>7;
				bR=(px_int)(clr._argb.r*Rb)>>7;
				bG=(px_int)(clr._argb.g*Gb)>>7;
				bB=(px_int)(clr._argb.b*Bb)>>7;

				clr._argb.a = (px_uchar)(((bA > 255) * 0xff) | bA);
				clr._argb.r = (px_uchar)(((bR > 255) * 0xff) | bR);
				clr._argb.g = (px_uchar)(((bG > 255) * 0xff) | bG);
				clr._argb.b = (px_uchar)(((bB > 255) * 0xff) | bB);
				PX_SurfaceDrawPixel(psurface,x+i,y+j,clr);
			}

		}

	}
	else
	{
		for (j=top;j<=bottom;j++)
		{
			for (i=left;i<=right;i++)
			{
				ref_x = (i - newWidth / 2.0);
				ref_y = (j - newHeight / 2.0);

				mapX = (px_int)((ref_x * invCosAgl - ref_y * invSinAgl) / scale + resWidth / 2);
				mapY = (px_int)((ref_x * invSinAgl + ref_y * invCosAgl) / scale + resHeight / 2);

				if (mapX<0||mapX>=resWidth)
				{
					continue;
				}

				if (mapY<0||mapY>=resHeight)
				{
					continue;
				}

				PX_SurfaceDrawPixel(psurface,x+i,y+j,PX_SURFACECOLOR(resTexture,mapX,mapY));
			}

		}
	}
}


px_void PX_TextureRenderEx_vector(px_surface *psurface,px_texture *resTexture,px_int x,px_int y,PX_ALIGN refPoint,PX_TEXTURERENDER_BLEND *blend,px_float scale,px_point2D p_vector)
{
	PX_TextureRenderEx_sincos(psurface,resTexture,x,y,refPoint,blend,scale,PX_Point2D_sin(p_vector),PX_Point2D_cos(p_vector));
}



px_bool PX_TextureCopy(px_memorypool *mp,px_texture *restexture,px_texture *dest)
{
	if(!PX_TextureCreate(mp,dest,restexture->width,restexture->height)) return PX_FALSE;
	PX_memcpy(dest->surfaceBuffer,restexture->surfaceBuffer,restexture->width*restexture->height*sizeof(px_color));
	return PX_TRUE;
}

px_bool PX_TextureScaleToTexture( px_texture* resTexture,  px_texture* out)
{
	px_int newWidth = out->width;
	px_int newHeight= out->height;
	px_float SampleWidth = (px_float)(resTexture->width) / (newWidth);
	px_float SampleHeight = (px_float)(resTexture->height) / (newHeight);
	px_float SampleX, SampleY, SampleArea, u, v, cellw, cellh, mixa, mixr, mixg, mixb;
	px_color* Dst, * Src = (px_color*)resTexture->surfaceBuffer;
	px_color SampleColor, MixColor;
	px_int xoft, yoft, horz, vcl;

	if (newWidth < 0 || newHeight < 0)
	{
		return PX_FALSE;
	}
	if (!(newHeight && newWidth))
	{
		return PX_FALSE;
	}

	Dst = (px_color*)out->surfaceBuffer;

	SampleArea = SampleHeight * SampleWidth;

	for (yoft = 0; yoft < newHeight; yoft++)
	{
		for (xoft = 0; xoft < newWidth; xoft++)
		{
			//reset sample color
			MixColor._argb.ucolor = 0;

			SampleX = xoft * SampleWidth;
			SampleY = yoft * SampleHeight;
			mixa = 0;
			mixr = 0;
			mixg = 0;
			mixb = 0;

			u = SampleX;
			for (horz = PX_TRUNC(SampleX); horz <= PX_TRUNC(SampleX + SampleWidth);)
			{
				v = SampleY;
				if (PX_TRUNC(u) == PX_TRUNC(SampleX + SampleWidth))
				{
					cellw = SampleX + SampleWidth - u;
				}
				else
				{
					cellw = horz + 1 - u;
				}

				for (vcl = PX_TRUNC(SampleY); vcl <= PX_TRUNC(SampleY + SampleHeight);)
				{
					if (PX_TRUNC(v) == PX_TRUNC(SampleY + SampleHeight))
					{
						cellh = SampleY + SampleHeight - v;
					}
					else
					{
						cellh = vcl + 1 - v;
					}
					if (horz < resTexture->width&& vcl < resTexture->height)
						SampleColor = PX_SURFACECOLOR(resTexture, horz, vcl);
					else
						SampleColor._argb.ucolor = 0;

					mixa += SampleColor._argb.a * cellh * cellw / SampleArea;
					mixr += SampleColor._argb.r * cellh * cellw / SampleArea;
					mixg += SampleColor._argb.g * cellh * cellw / SampleArea;
					mixb += SampleColor._argb.b * cellh * cellw / SampleArea;


					vcl++;
					v = (px_float)vcl;
				}
				horz++;
				u = (px_float)horz;
			}
			mixa > 255 ? mixa = 255 : 0;
			mixr > 255 ? mixr = 255 : 0;
			mixg > 255 ? mixg = 255 : 0;
			mixb > 255 ? mixb = 255 : 0;
			Dst[xoft + yoft * newWidth] = PX_COLOR((px_uchar)PX_APO(mixa), (px_uchar)mixr, (px_uchar)mixg, (px_uchar)mixb);;
		}
	}
	return PX_TRUE;
}
