#ifndef PX_OBJECT_ANIMATION_H
#define PX_OBJECT_ANIMATION_H
#include "PX_Object.h"

typedef enum
{
	PX_OBJECT_ANIMATION_STATE_PLAY,
	PX_OBJECT_ANIMATION_STATE_PAUSE
}PX_OBJECT_ANIMATION_STATE;

typedef struct
{
	PX_OBJECT_ANIMATION_STATE state;
	PX_Animation animation;
}PX_Object_Animation;

PX_Object* PX_Object_AnimationAttachObject( PX_Object* AttachObject, px_int attachIndex, px_int x, px_int y, PX_AnimationLibrary* lib);
PX_Object *PX_Object_AnimationCreate(px_memorypool *mp,PX_Object *Parent,px_int x,px_int y,PX_AnimationLibrary *lib);
px_void PX_Object_AnimationSetLibrary(PX_Object *pObject,PX_AnimationLibrary *lib);
PX_Object_Animation *PX_Object_GetAnimation(PX_Object *pObject);
px_void	   PX_Object_AnimationPlay(PX_Object* pObject, const px_char name[]);
px_void	   PX_Object_AnimationPause(PX_Object* pObject);
px_void	   PX_Object_AnimationResume(PX_Object* pObject);
#endif

