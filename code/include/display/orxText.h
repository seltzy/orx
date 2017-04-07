/* Orx - Portable Game Engine
 *
 * Copyright (c) 2008-2017 Orx-Project
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *    1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 *    2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 *    3. This notice may not be removed or altered from any source
 *    distribution.
 */

/**
 * @file orxText.h
 * @date 02/12/2008
 * @author iarwain@orx-project.org
 *
 * @todo
 */

/**
 * @addtogroup orxText
 *
 * Text module
 * Module that handles texts
 *
 * @{
 */


#ifndef _orxTEXT_H_
#define _orxTEXT_H_

#include "orxInclude.h"

#include "display/orxFont.h"

/** Text marker types */
typedef enum __orxTEXT_MARKER_TYPE_t
{
  orxTEXT_MARKER_TYPE_POP = 0,
  orxTEXT_MARKER_TYPE_CLEAR,
  orxTEXT_MARKER_TYPE_FONT,
  orxTEXT_MARKER_TYPE_COLOR,
  orxTEXT_MARKER_TYPE_SCALE,
  orxTEXT_MARKER_TYPE_REVERT,
  orxTEXT_MARKER_TYPE_LINE_HEIGHT,
  orxTEXT_MARKER_TYPE_NUMBER,
  orxTEXT_MARKER_TYPE_NONE = orxENUM_NONE
} orxTEXT_MARKER_TYPE;

/** Text marker data structure */
typedef struct __orxTEXT_MARKER_DATA_t {
  orxTEXT_MARKER_TYPE    eType;
  union
  {
    const orxFONT       *pstFont;
    orxRGBA              stRGBA;
    orxVECTOR            vScale;
    orxFLOAT             fLineHeight;
    orxTEXT_MARKER_TYPE  eRevertType;
  };
} orxTEXT_MARKER_DATA;

/** Text marker structure */
typedef struct __orxTEXT_MARKER_t
{
  orxU32                 u32Index;
  orxTEXT_MARKER_DATA    stData;
} orxTEXT_MARKER;

/** Internal text structure */
typedef struct __orxTEXT_t                orxTEXT;

/** Setups the text module
 */
extern orxDLLAPI void orxFASTCALL         orxText_Setup();

/** Inits the text module
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL    orxText_Init();

/** Exits from the text module
 */
extern orxDLLAPI void orxFASTCALL         orxText_Exit();


/** Creates an empty text
 * @return      orxTEXT / orxNULL
 */
extern orxDLLAPI orxTEXT *orxFASTCALL     orxText_Create();

/** Creates a text from config
 * @param[in]   _zConfigID    Config ID
 * @return      orxTEXT / orxNULL
 */
extern orxDLLAPI orxTEXT *orxFASTCALL     orxText_CreateFromConfig(const orxSTRING _zConfigID);

/** Deletes a text
 * @param[in]   _pstText      Concerned text
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL    orxText_Delete(orxTEXT *_pstText);


/** Gets text size
 * @param[in]   _pstText      Concerned text
 * @param[out]  _pfWidth      Text's width
 * @param[out]  _pfHeight     Text's height
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL    orxText_GetSize(const orxTEXT *_pstText, orxFLOAT *_pfWidth, orxFLOAT *_pfHeight);

/** Gets text name
 * @param[in]   _pstText      Concerned text
 * @return      Text name / orxNULL
 */
extern orxDLLAPI const orxSTRING orxFASTCALL orxText_GetName(const orxTEXT *_pstText);


/** Gets text string
 * @param[in]   _pstText      Concerned text
 * @return      Text string / orxSTRING_EMPTY
 */
extern orxDLLAPI const orxSTRING orxFASTCALL orxText_GetString(const orxTEXT *_pstText);

/** Gets text font
 * @param[in]   _pstText      Concerned text
 * @return      Text font / orxNULL
 */
extern orxDLLAPI orxFONT *orxFASTCALL     orxText_GetFont(const orxTEXT *_pstText);

/** Gets number of markers
 * @param[in]   _pstText      Concerned text
 * @return      Text marker counter
 */
extern orxDLLAPI orxU32 orxFASTCALL orxText_GetMarkerCounter(const orxTEXT *_pstText);

/** Gets marker array
 * @param[in] _pstText  Concerned text
 * @return Pointer to orxTEXT_MARKER / orxNULL if no markers
 */
extern orxDLLAPI const orxTEXT_MARKER *orxFASTCALL orxText_GetMarkerArray(const orxTEXT *_pstText);


/** Sets text string
 * @param[in]   _pstText      Concerned text
 * @param[in]   _zString      String to contain
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL    orxText_SetString(orxTEXT *_pstText, const orxSTRING _zString);

/** Sets text font
 * @param[in]   _pstText      Concerned text
 * @param[in]   _pstFont      Font / orxNULL to use default
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
extern orxDLLAPI orxSTATUS orxFASTCALL    orxText_SetFont(orxTEXT *_pstText, orxFONT *_pstFont);

#endif /* _orxTEXT_H_ */

/** @} */
