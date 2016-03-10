/*
 * This file is part of the DXX-Rebirth project <http://www.dxx-rebirth.com/>.
 * It is copyright by its individual contributors, as recorded in the
 * project's Git history.  See COPYING.txt at the top level for license
 * terms and a link to the Git history.
 */
/*
 * 
 * Private (internal) header for 3d library
 * 
 */

#pragma once

#include "maths.h"
#include "fwd-vecmat.h"

struct g3s_point;

namespace dcx {

extern int Canvas_width,Canvas_height;	//the actual width & height
extern fix Canv_w2,Canv_h2;				//fixed-point width,height/2

#ifdef __powerc
extern double fCanv_w2, fCanv_h2;
#endif

extern vms_vector Window_scale;

extern fix View_zoom;
extern vms_vector View_position,Matrix_scale;
extern vms_matrix View_matrix,Unscaled_matrix;
}

//vertex buffers for polygon drawing and clipping
//list of 2d coords
