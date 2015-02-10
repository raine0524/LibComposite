/*
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __VAAPI_ALLOCATOR_H__
#define __VAAPI_ALLOCATOR_H__


#include <stdlib.h>
#include <va/va.h>

#include "base_allocator.h"

// VAAPI Allocator internal Mem ID
struct vaapiMemId
{
    VASurfaceID* m_surface;
    VAImage      m_image;
    // variables for VAAPI Allocator inernal color convertion
    unsigned int m_fourcc;
    mfxU8*       m_sys_buffer;
    mfxU8*       m_va_buffer;
};
/*
struct vaapiAllocatorParams : mfxAllocatorParams
{
    VADisplay m_dpy;
};
*/
class vaapiFrameAllocator: public BaseFrameAllocator
{
public:
    vaapiFrameAllocator();
    virtual ~vaapiFrameAllocator();

    virtual mfxStatus Init(VADisplay *dpy);
    virtual mfxStatus Close();
protected:
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);

    virtual mfxStatus CheckRequestType(mfxFrameAllocRequest *request);
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

    VADisplay m_dpy;
};


#endif // __VAAPI_ALLOCATOR_H__
