/*
 * Focused extraction of Blender 3.6.23 Legacy GP Build modifier's deterministic
 * concurrent generation algorithm. The desktop generateStrokes() callback
 * requires Depsgraph/Scene evaluation; Project Grease intentionally does not
 * port that desktop graph.
 */
#include <algorithm>
#include <cmath>
#include <cstring>
#include "MEM_guardedalloc.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BKE_deform.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"

static void pg_clear_stroke(bGPDframe *gpf, bGPDstroke *gps)
{
  BLI_remlink(&gpf->strokes, gps); BKE_gpencil_free_stroke(gps);
}
static void pg_reduce(bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps, int n, eBuildGpencil_Transition transition)
{
  if (n == 0 || !gps->points) { pg_clear_stroke(gpf, gps); return; }
  bGPDspoint *points=(bGPDspoint*)MEM_callocN(sizeof(bGPDspoint)*n,__func__);
  MDeformVert *dvert=gps->dvert?(MDeformVert*)MEM_callocN(sizeof(MDeformVert)*n,__func__):nullptr;
  if (transition==GP_BUILD_TRANSITION_VANISH) {
    const int offset=gps->totpoints-n; memcpy(points,gps->points+offset,sizeof(bGPDspoint)*n);
    if(dvert){memcpy(dvert,gps->dvert+offset,sizeof(MDeformVert)*n);for(int i=0;i<offset;i++)BKE_gpencil_free_point_weights(&gps->dvert[i]);}
  } else {
    memcpy(points,gps->points,sizeof(bGPDspoint)*n);
    if(dvert){memcpy(dvert,gps->dvert,sizeof(MDeformVert)*n);for(int i=n;i<gps->totpoints;i++)BKE_gpencil_free_point_weights(&gps->dvert[i]);}
  }
  MEM_SAFE_FREE(gps->points); MEM_SAFE_FREE(gps->dvert);
  gps->points=points; gps->dvert=dvert; gps->totpoints=n;
  BKE_gpencil_stroke_geometry_update(gpd,gps);
}
static void pg_fade(bGPDstroke *gps,int a,int b,float wa,float wb,eBuildGpencil_Transition transition,float ts,float os)
{
  int range=b-a;if(!range)range=1;
  if(transition!=GP_BUILD_TRANSITION_GROW&&transition!=GP_BUILD_TRANSITION_SHRINK&&transition!=GP_BUILD_TRANSITION_VANISH)return;
  for(int i=a;i<=b;i++){
    float w=interpf(wb,wa,float(i-a)/range);
    if(ts>1e-5f)gps->points[i].pressure*=interpf(w,1.0f,ts);
    if(os>1e-5f)gps->points[i].strength*=interpf(w,1.0f,os);
  }
}
extern "C" bool project_grease_legacy_build_apply(bGPdata *gpd,bGPDframe *gpf,BuildGpencilModifierData *mmd,float factor)
{
  if(!gpd||!gpf||!mmd||BLI_listbase_is_empty(&gpf->strokes)){}
  if(!gpd||!gpf||!mmd||BLI_listbase_is_empty(&gpf->strokes))return false;
  const bool reverse=mmd->transition!=GP_BUILD_TRANSITION_GROW;
  const bool fading=(mmd->flag&GP_BUILD_USE_FADING)!=0;
  const float fadefac=fading?mmd->fade_fac:0.0f;
  const float fac=std::clamp(factor,0.0f,1.0f);
  float use=interpf(1.0f+fadefac,0.0f,fac); if(reverse)use-=fadefac;
  int maxp=0; LISTBASE_FOREACH(bGPDstroke*,gps,&gpf->strokes)maxp=std::max(maxp,gps->totpoints);
  if(!maxp)return false; const int fadepoints=int(fadefac*maxp);
  for(bGPDstroke*gps=(bGPDstroke*)gpf->strokes.first,*next;gps;gps=next){
    next=gps->next; const float rel=float(gps->totpoints)/float(maxp); int n=0;
    if(mmd->time_alignment==GP_BUILD_TIMEALIGN_START){
      float scaled=use/std::max(rel,1e-8f); n=int(std::round((reverse?1.0f-scaled:scaled)*gps->totpoints));
    } else if(mmd->time_alignment==GP_BUILD_TIMEALIGN_END){
      float scaled=(use-(1.0f-rel))/std::max(rel,1e-8f); n=int(std::round((reverse?1.0f-scaled:scaled)*gps->totpoints));
    } else return false;
    if(n<=0){pg_clear_stroke(gpf,gps);continue;}
    int more=std::clamp(n-gps->totpoints,0,fadepoints+1);
    float maxw=std::clamp(float(n+more)/std::max(float(fadepoints),1.0f),0.0f,1.0f);
    int a,b;float wa,wb;
    if(mmd->transition==GP_BUILD_TRANSITION_VANISH){a=gps->totpoints-n-more;b=gps->totpoints-n+fadepoints-more;wa=float(more)/std::max(float(fadepoints),1.0f);wb=maxw;}
    else {a=n-1-fadepoints+more;b=n-1+more;wa=maxw;wb=float(more)/std::max(float(fadepoints),1.0f);}
    a=std::clamp(a,0,gps->totpoints-1);b=std::clamp(b,0,gps->totpoints-1);
    if(fading)pg_fade(gps,a,b,wa,wb,mmd->transition,mmd->fade_thickness_strength,mmd->fade_opacity_strength);
    if(n<gps->totpoints)pg_reduce(gpd,gpf,gps,n,mmd->transition);
  }
  return true;
}
