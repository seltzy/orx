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
 * @file orxText.c
 * @date 02/12/2008
 * @author iarwain@orx-project.org
 *
 */


#include "display/orxText.h"

#include "memory/orxMemory.h"
#include "core/orxConfig.h"
#include "core/orxEvent.h"
#include "core/orxLocale.h"
#include "core/orxResource.h"
#include "math/orxVector.h"
#include "object/orxStructure.h"
#include "utils/orxHashTable.h"

#ifdef __orxMSVC__

#include <malloc.h>
#pragma warning(disable : 4200)

#endif /* __orxMSVC__ */

/** Module flags
 */
#define orxTEXT_KU32_STATIC_FLAG_NONE         0x00000000  /**< No flags */

#define orxTEXT_KU32_STATIC_FLAG_READY        0x00000001  /**< Ready flag */

#define orxTEXT_KU32_STATIC_MASK_ALL          0xFFFFFFFF  /**< All mask */

/** orxTEXT flags / masks
 */
#define orxTEXT_KU32_FLAG_NONE                0x00000000  /**< No flags */

#define orxTEXT_KU32_FLAG_INTERNAL            0x10000000  /**< Internal structure handlign flag */

#define orxTEXT_KU32_MASK_ALL                 0xFFFFFFFF  /**< All mask */


/** Misc defines
 */
#define orxTEXT_KZ_CONFIG_STRING              "String"
#define orxTEXT_KZ_CONFIG_FONT                "Font"

#define orxTEXT_KC_LOCALE_MARKER              '$'

#define orxTEXT_KZ_MARKER_WARNING             "Invalid text marker [%c%s%s] in [%s]!"
#define orxTEXT_KC_MARKER_SYNTAX_START        '`'
#define orxTEXT_KC_MARKER_SYNTAX_OPEN         '('
#define orxTEXT_KC_MARKER_SYNTAX_CLOSE        ')'
#define orxTEXT_KZ_MARKER_TYPE_FONT           "font"
#define orxTEXT_KZ_MARKER_TYPE_COLOR          "color"
#define orxTEXT_KZ_MARKER_TYPE_SCALE          "scale"
#define orxTEXT_KZ_MARKER_TYPE_POP            "!"
#define orxTEXT_KZ_MARKER_TYPE_CLEAR          "*"

#define orxTEXT_KU32_BANK_SIZE                256         /**< Bank size */
#define orxTEXT_KU32_MARKER_CELL_BANK_SIZE    128         /**< Bank size */
#define orxTEXT_KU32_MARKER_DATA_BANK_SIZE    128         /**< Bank size */


/***************************************************************************
 * Structure declaration                                                   *
 ***************************************************************************/

/** Marker format data
 *  Capable of being shared between multiple markers.
 *  TODO: Either reuse these between multiple markers, or integrate with orxTEXT_MARKER_CELL. This might be accomplished by interpreting this struct as a string and using that as a key.
 */
typedef struct __orxTEXT_MARKER_DATA_t
{
  orxTEXT_MARKER_TYPE eType;
  union
  {
    const orxFONT       *pstFont;
    orxRGBA              stRGBA;
    orxVECTOR            vScale;
    orxFLOAT             fLineHeight;
    orxTEXT_MARKER_TYPE  eRevertType;
  };
} orxTEXT_MARKER_DATA;

/** Marker fallback data
 *  Used by the parser to maintain fallback data state
 */
typedef struct __orxTEXT_MARKER_FALLBACKS_t
{
  const orxTEXT_MARKER_DATA *pstFont;
  const orxTEXT_MARKER_DATA *pstColor;
  const orxTEXT_MARKER_DATA *pstScale;
} orxTEXT_MARKER_FALLBACKS;

/** Marker position data
 *  Where the marker resides in an orxTEXT string.
 */
typedef struct __orxTEXT_MARKER_CELL_t
{
  orxLINKLIST_NODE                       stNode;
  orxU32                                 u32Index;
  const orxTEXT_MARKER_DATA             *pstData;
} orxTEXT_MARKER_CELL;

/** Marker stack entry
 *  Used specifically for dry run of marker traversal
 */
typedef struct __orxTEXT_MARKER_STACK_ENTRY_t
{
  orxLINKLIST_NODE            stNode;
  const orxTEXT_MARKER_DATA  *pstData;
  const orxTEXT_MARKER_DATA  *pstFallbackData;
} orxTEXT_MARKER_STACK_ENTRY;

/** Text structure
 */
struct __orxTEXT_t
{
  orxSTRUCTURE      stStructure;                /**< Public structure, first structure member : 32 */
  orxFONT          *pstFont;                    /**< Font : 20 */
  orxBANK          *pstMarkerDatas;
  orxBANK          *pstMarkerCells;
  orxLINKLIST       stMarkers;
  const orxSTRING   zString;                    /**< String : 24 */
  orxFLOAT          fWidth;                     /**< Width : 28 */
  orxFLOAT          fHeight;                    /**< Height : 32 */
  const orxSTRING   zReference;                 /**< Config reference : 36 */
};

/** Static structure
 */
typedef struct __orxTEXT_STATIC_t
{
  orxU32        u32Flags;                       /**< Control flags : 4 */

} orxTEXT_STATIC;


/***************************************************************************
 * Module global variable                                                  *
 ***************************************************************************/

static orxTEXT_STATIC sstText;


/***************************************************************************
 * Private functions                                                       *
 ***************************************************************************/

/** Gets corresponding locale key
 * @param[in]   _pstText    Concerned text
 * @param[in]   _zProperty  Property to get
 * @return      orxSTRING / orxNULL
 */
static orxINLINE const orxSTRING orxText_GetLocaleKey(const orxTEXT *_pstText, const orxSTRING _zProperty)
{

  const orxSTRING zResult = orxNULL;

  /* Checks */
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has reference? */
  if(_pstText->zReference != orxNULL)
  {
    const orxSTRING zString;

    /* Pushes its section */
    orxConfig_PushSection(_pstText->zReference);

    /* Gets its string */
    zString = orxConfig_GetString(_zProperty);

    /* Valid? */
    if(zString != orxNULL)
    {
      /* Begins with locale marker? */
      if((*zString == orxTEXT_KC_LOCALE_MARKER) && (*(zString + 1) != orxTEXT_KC_LOCALE_MARKER))
      {
        /* Updates result */
        zResult = zString + 1;
      }
    }

    /* Pops config section */
    orxConfig_PopSection();
  }

  /* Done! */
  return zResult;
}

/** Sanity tests marker traversal, giving back the failed marker handle for debugging
 * @param[in]   _pstText      Concerned text
 * @param[out]  _phBadMarker  Failed marker
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
static orxSTATUS orxFASTCALL orxText_ValidateMarkers(const orxTEXT *_pstText, orxHANDLE *_phBadMarker)
{
  orxASSERT(_pstText != orxNULL);
  orxASSERT(_pstText->pstMarkerCells != orxNULL);
  orxASSERT(_pstText->pstMarkerDatas != orxNULL);
  orxSTATUS eResult = orxSTATUS_SUCCESS;
  orxU32 u32Index = 0;
  orxHANDLE hIterator;
  if (orxBank_GetCounter(_pstText->pstMarkerCells) == orxLinkList_GetCounter(&_pstText->stMarkers))
  {
    for (hIterator = orxText_FirstMarker(_pstText);
         (hIterator != orxNULL) && (hIterator != orxHANDLE_UNDEFINED);
         hIterator = orxText_NextMarker(hIterator))
    {
      orxU32 u32MarkerIndex = orxText_GetMarkerIndex(hIterator);

      /* Ensure marker has valid index */
      if (u32MarkerIndex == orxU32_UNDEFINED)
      {
        eResult = orxSTATUS_FAILURE;
        break;
      }

      /* Make sure markers are in order */
      if (u32MarkerIndex < u32Index)
      {
        eResult = orxSTATUS_FAILURE;
        break;
      }

      /* Check types */
      orxTEXT_MARKER_TYPE eType = orxText_GetMarkerType(hIterator);
      switch(eType)
      {
      case orxTEXT_MARKER_TYPE_COLOR:
      {
        orxRGBA stColor = {0};
        eResult = orxText_GetMarkerColor(hIterator, &stColor);
        break;
      }
      case orxTEXT_MARKER_TYPE_FONT:
      {
        const orxFONT *pstFont = orxNULL;
        eResult = orxText_GetMarkerFont(hIterator, &pstFont);
        break;
      }
      case orxTEXT_MARKER_TYPE_SCALE:
      {
        orxVECTOR vScale = {0};
        eResult = orxText_GetMarkerScale(hIterator, &vScale);
        break;
      }
      case orxTEXT_MARKER_TYPE_LINE_HEIGHT:
      {
        orxFLOAT fHeight = orxNULL;
        eResult = orxText_GetMarkerLineHeight(hIterator, &fHeight);
        break;
      }
      case orxTEXT_MARKER_TYPE_REVERT:
      {
        orxTEXT_MARKER_TYPE eRevertType = orxTEXT_MARKER_TYPE_NONE;
        eResult = orxText_GetMarkerRevertType(hIterator, &eRevertType);
        /* Make sure it's a valid revert type */
        if (eRevertType == orxTEXT_MARKER_TYPE_NONE ||
            eRevertType == orxTEXT_MARKER_TYPE_POP ||
            eRevertType == orxTEXT_MARKER_TYPE_CLEAR ||
            eRevertType == orxTEXT_MARKER_TYPE_LINE_HEIGHT)
        {
          eResult = orxSTATUS_FAILURE;
        }
        break;
      }
      default:
        /* Everything else should be impossible when this function is called */
        eResult = orxSTATUS_FAILURE;
      }
    }
  }
  else
  {
    eResult = orxSTATUS_FAILURE;
  }

  /* Store the failed marker if the user provided a valid handle pointer */
  if (_phBadMarker != orxNULL)
  {
    *_phBadMarker = hIterator;
  }

  /* Done! */
  return eResult;
}

/** Checks type name against current spot in marker string, storing remainder of string after parsing
 * @param[in]   _zCheckTypeName Test marker type name
 * @param[in]   _zMarkerText    Pointer to string where marker type starts
 * @param[out]  _pzRemainder    Where to continue parsing from
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
static orxSTATUS orxFASTCALL orxText_CheckMarkerType(const orxSTRING _zCheckTypeName, const orxSTRING _zMarkerText, const orxSTRING *_pzRemainder)
{
  orxASSERT((_zCheckTypeName != orxNULL) && (_zCheckTypeName != orxSTRING_EMPTY) && (*_zCheckTypeName != orxCHAR_NULL));
  orxASSERT((_zMarkerText != orxNULL) && (_zMarkerText != orxSTRING_EMPTY) && (*_zMarkerText != orxCHAR_NULL)) ;
  orxASSERT(_pzRemainder != orxNULL);
  /* Set default output */
  orxSTATUS eResult = orxSTATUS_FAILURE;
  *_pzRemainder = _zMarkerText;
  /* See _zCheckTypeName matches the start of _zMarkerText */
  orxU32 u32TypeLength = orxString_GetLength(_zCheckTypeName);
  if (orxString_NCompare(_zMarkerText, _zCheckTypeName, u32TypeLength) == 0) {
    /* Update the next token to be the end of the type name in _zMarkerText */
    *_pzRemainder = (_zMarkerText + u32TypeLength);
    eResult = orxSTATUS_SUCCESS;
  }
  return eResult;
}

/** Checks the type and returns a pointer to the appropriate fallback data pointer
 *  This is used to manage marker processing state when pushing/popping marker stack entries
 * @param[in]  _eType         Concerned text
 * @param[in]  _pstFallbacks  Pointer to fallback structure (used for parser state)
 * @return     Matching orxTEXT_MARKER_DATA in _pstFallbacks / orxNULL
 */
static const orxTEXT_MARKER_DATA **orxFASTCALL orxText_GetMarkerFallbackPointer(orxTEXT_MARKER_TYPE _eType, orxTEXT_MARKER_FALLBACKS *_pstFallbacks)
{
  orxASSERT(_pstFallbacks != orxNULL);
  /* Get a pointer to the appropriate fallback data */
  const orxTEXT_MARKER_DATA **ppstResult = orxNULL;
  switch(_eType)
  {
  case orxTEXT_MARKER_TYPE_COLOR:
    ppstResult = &_pstFallbacks->pstColor;
    break;
  case orxTEXT_MARKER_TYPE_FONT:
    ppstResult = &_pstFallbacks->pstFont;
    break;
  case orxTEXT_MARKER_TYPE_SCALE:
    ppstResult = &_pstFallbacks->pstScale;
    break;
  default:
    ppstResult = orxNULL;
  }
  return ppstResult;
}

/** Create marker data of specified type with zeroed out data.
 * @return      orxTEXT_MARKER_DATA
 */
static orxTEXT_MARKER_DATA *orxFASTCALL orxText_CreateMarkerData(const orxTEXT *_pstText, orxTEXT_MARKER_TYPE _eType)
{
  orxASSERT(_pstText != orxNULL);
  orxASSERT(_eType != orxTEXT_MARKER_TYPE_NONE);
  /* Allocate and initialize marker data */
  orxTEXT_MARKER_DATA *pstResult = (orxTEXT_MARKER_DATA *) orxBank_Allocate(_pstText->pstMarkerDatas);
  orxASSERT(pstResult != orxNULL);
  orxMemory_Zero(pstResult, sizeof(orxTEXT_MARKER_DATA));
  pstResult->eType = _eType;
  return pstResult;
}

/** Create and push a marker stack entry with the specified data / fallback data.
 * @return      orxTEXT_MARKER_STACK_ENTRY
 */
static orxTEXT_MARKER_STACK_ENTRY *orxFASTCALL orxText_AddMarkerStackEntry(orxLINKLIST *_pstStack, orxBANK *_pstStackBank, const orxTEXT_MARKER_DATA *_pstData, const orxTEXT_MARKER_DATA *_pstFallbackData)
{
  orxASSERT(_pstStackBank != orxNULL);
  orxASSERT(_pstStack != orxNULL);
  orxASSERT(_pstData != orxNULL);
  /* Allocate and initialize marker stack entry */
  orxTEXT_MARKER_STACK_ENTRY *pstResult = (orxTEXT_MARKER_STACK_ENTRY *) orxBank_Allocate(_pstStackBank);
  orxASSERT(pstResult != orxNULL);
  pstResult->pstData = _pstData;
  pstResult->pstFallbackData = _pstFallbackData;
  orxLinkList_AddEnd(_pstStack, (orxLINKLIST_NODE *) pstResult);
  return pstResult;
}

/** Create a marker cell and add it to the marker list
 * @param[in] _pstText          Concerned text
 * @param[in] _u32Index         Index in final text string for the marker to use
 * @param[in] _pstData          Data for the marker to use
 * @param[in] _bSeekInsertion   Whether to insert based on _u32Index, or simply append
 * @return    orxTEXT_MARKER_CELL
 */
static orxTEXT_MARKER_CELL *orxFASTCALL orxText_AddMarkerCell(orxTEXT *_pstText, orxU32 _u32Index, const orxTEXT_MARKER_DATA *_pstData, orxBOOL _bSeekInsertion)
{
  orxASSERT(_pstText != orxNULL);
  orxASSERT(_u32Index != orxU32_UNDEFINED);

  /* Allocate and initialize marker call */
  orxTEXT_MARKER_CELL *pstResult = (orxTEXT_MARKER_CELL *) orxBank_Allocate(_pstText->pstMarkerCells);
  orxASSERT(pstResult != orxNULL);
  orxMemory_Zero(pstResult, sizeof(orxTEXT_MARKER_CELL));
  pstResult->u32Index = _u32Index;
  pstResult->pstData = _pstData;

  if (_bSeekInsertion && (orxLinkList_GetCounter(&_pstText->stMarkers) > 0))
  {
    orxHANDLE pstMarker = orxNULL;
    for (pstMarker = orxText_FirstMarker(_pstText)
           ; (pstMarker != orxNULL) && (pstMarker != orxHANDLE_UNDEFINED)
           ; pstMarker = orxText_NextMarker(pstMarker))
    {
      orxTEXT_MARKER_CELL *pstCell = (orxTEXT_MARKER_CELL *) pstMarker;
      if (_u32Index <= pstCell->u32Index)
      {
        break;
      }
    }
    if ((pstMarker != orxNULL) && (pstMarker != orxHANDLE_UNDEFINED))
    {
      orxASSERT(orxLinkList_AddBefore((orxLINKLIST_NODE *)pstMarker, (orxLINKLIST_NODE *)pstResult) == orxSTATUS_SUCCESS);
    }
    else
    {
      orxASSERT(orxLinkList_AddEnd(&_pstText->stMarkers, (orxLINKLIST_NODE *) pstResult) == orxSTATUS_SUCCESS);
    }
  }
  else
  {
    orxASSERT(orxLinkList_AddEnd(&_pstText->stMarkers, (orxLINKLIST_NODE *)pstResult) == orxSTATUS_SUCCESS);
  }

  return pstResult;
}

static void orxFASTCALL orxText_ClearMarkers(orxTEXT *_pstText, orxU32 _u32Index, const orxTEXT_MARKER_FALLBACKS *_pstFallbacks, orxLINKLIST *_pstStack, orxBANK *_pstBank)
{
  /* When clearing, we only want to revert to each type once, and only if necessary. */
  /* Create a temporary fallbacks structure to keep track of what has already been reverted */
  orxTEXT_MARKER_FALLBACKS stFallbacksReverted = {orxNULL, orxNULL, orxNULL};
  /* Pop stack until it's empty */
  while (orxLinkList_GetCounter(_pstStack) > 0)
  {
    /* Inspect top of stack for what type needs to be reverted */
    orxTEXT_MARKER_STACK_ENTRY *pstTop = (orxTEXT_MARKER_STACK_ENTRY *) orxLinkList_GetLast(_pstStack);
    /* Pop the stack */
    orxTEXT_MARKER_STACK_ENTRY *pstPoppedEntry = (orxTEXT_MARKER_STACK_ENTRY *) orxLinkList_GetLast(_pstStack);
    orxASSERT(pstPoppedEntry != orxNULL);
    orxLinkList_Remove((orxLINKLIST_NODE *) pstPoppedEntry);
    const orxTEXT_MARKER_DATA *pstNewData = orxNULL;
    /* Default values are unknown to orxTEXT, so we put a placeholder marker that identifies its data type */
    if (pstPoppedEntry->pstData->eType == orxTEXT_MARKER_TYPE_REVERT)
    {
      /* The popped entry was already a revert? Someone isn't keeping track of how much they clear/pop the stack. */
      pstNewData = pstPoppedEntry->pstData;
    }
    else
    {
      /* Allocate a new revert */
      orxTEXT_MARKER_DATA *pstData = orxText_CreateMarkerData(_pstText, orxTEXT_MARKER_TYPE_REVERT);
      pstData->eRevertType = pstPoppedEntry->pstData->eType;
      pstNewData = pstData;
    }
    /* Delete the popped entry */
    orxBank_Free(_pstBank, pstPoppedEntry);
    if (pstNewData != orxNULL)
    {
      /* Get pointer to the revert data of this type in the temp fallbacks structure */
      const orxTEXT_MARKER_DATA **ppstStoreFallback = orxText_GetMarkerFallbackPointer(pstNewData->eRevertType, &stFallbacksReverted);
      /* Valid place to store the fallback? */
      if (ppstStoreFallback != orxNULL)
      {
        /* Nothing there? Hasn't been reverted yet, so add a revert marker for it and flag it as reverted. */
        if (*ppstStoreFallback == orxNULL)
        {
          orxText_AddMarkerCell(_pstText, _u32Index, pstNewData, orxFALSE);
          *ppstStoreFallback = pstNewData;
        }
      }
    }
  }
}

/** Pop a marker stack entry from the stack, adding a new marker to the marker list
 * Popping a marker represents adding a new marker of the same type, but with the data of what came before it
 * When a stack entry is pushed, its data becomes the fallback data for the next pushed marker of its type
 * When a stack entry is popped, its fallback data is added as a new marker (which makes its data the new current fallback of that type)
 * If the popped marker stack entry has no fallback data (i.e. was the first of its type), a revert marker with that type is allocated/placed instead
 * @param[in]      _pstText           Concerned text
 * @param[in]      _u32Index          Index to use for a fallback marker cell
 * @param[in]      _pstFallbacks      Pointer to fallback structure (used for parser state)
 * @param[in]      _pstStack          Stack to pop from
 * @param[in]      _pstBank           Bank used by the stack for deleting popped stack entries
 */
static void orxFASTCALL orxText_PopMarker(orxTEXT *_pstText, orxU32 _u32Index, orxBOOL _bClear, orxTEXT_MARKER_FALLBACKS *_pstFallbacks, orxLINKLIST *_pstStack, orxBANK *_pstBank)
{
  orxASSERT(_pstText != orxNULL);
  orxASSERT(_u32Index != orxU32_UNDEFINED);
  orxASSERT(_pstFallbacks != orxNULL);
  orxASSERT(_pstStack != orxNULL);
  orxASSERT(_pstBank != orxNULL);
  orxASSERT(orxLinkList_GetCounter(_pstStack) > 0);

  /* Inspect top of stack for what type needs to be rolled back */
  orxTEXT_MARKER_STACK_ENTRY *pstTop = (orxTEXT_MARKER_STACK_ENTRY *) orxLinkList_GetLast(_pstStack);
  /* Pop the stack */
  orxTEXT_MARKER_STACK_ENTRY *pstPoppedEntry = (orxTEXT_MARKER_STACK_ENTRY *) orxLinkList_GetLast(_pstStack);
  orxASSERT(pstPoppedEntry != orxNULL);
  orxLinkList_Remove((orxLINKLIST_NODE *) pstPoppedEntry);

  /* The fallback data of the popped entry will serve as the data for a new marker */
  const orxTEXT_MARKER_DATA *pstNewData = pstPoppedEntry->pstFallbackData;
  /* If that fallback data is null, it means we're reverting to a default value. */
  if (pstNewData == orxNULL)
  {
    /* Default values are unknown to orxTEXT, so we put a placeholder marker that identifies its data type */
    if (pstPoppedEntry->pstData->eType == orxTEXT_MARKER_TYPE_REVERT)
    {
      /* The popped entry was already a revert? Someone isn't keeping track of how much they clear/pop the stack. */
      pstNewData = pstPoppedEntry->pstData;
    }
    else
    {
      /* Allocate a new revert */
      orxTEXT_MARKER_DATA *pstData = orxText_CreateMarkerData(_pstText, orxTEXT_MARKER_TYPE_REVERT);
      pstData->eRevertType = pstPoppedEntry->pstData->eType;
      pstNewData = pstData;
    }
  }

  /* Delete the popped entry */
  orxBank_Free(_pstBank, pstPoppedEntry);
  /* Add a new marker using fallback data */
  orxText_AddMarkerCell(_pstText, _u32Index, pstNewData, orxFALSE);

  /* Update the processors fallback data to be the newly added marker's data */
  /* Get a pointer to the appropriate fallback data */
  const orxTEXT_MARKER_DATA **ppstFallbackData = orxText_GetMarkerFallbackPointer(pstNewData->eRevertType, _pstFallbacks);
  if (ppstFallbackData != orxNULL)
  {
    /* This is either the popped marker's fallback data, or a placeholder (revert) for the user to interpret */
    *ppstFallbackData = pstNewData;
  }
}

/** Parses marker value string
 * @param[in]      _pstText             Concerned text
 * @param[in]      _eType               Expected marker data type
 * @param[in]      _zString             Whole unparsed string
 * @param[in]      _u32Offset           Offset in _zString to the start of the marker value
 * @param[out]     _pzRemainder         Where to store pointer to remainder of string
 * @return         orxTEXT_MARKER_DATA  / orxNULL
 */
static orxTEXT_MARKER_DATA *orxFASTCALL orxText_ParseMarkerValue(const orxTEXT *_pstText, orxTEXT_MARKER_TYPE _eType, const orxSTRING _zString, orxU32 _u32Offset, const orxSTRING *_pzRemainder)
{
  orxSTRUCTURE_ASSERT(_pstText);
  orxASSERT(_eType != orxTEXT_MARKER_TYPE_NONE);
  orxASSERT((_zString != orxNULL) && (_zString != orxSTRING_EMPTY));
  orxASSERT(_u32Offset != orxU32_UNDEFINED);
  orxASSERT(_pzRemainder != orxNULL);

  const orxSTRING zValueStart = _zString + _u32Offset;
  orxASSERT((zValueStart != orxNULL) && (zValueStart != orxSTRING_EMPTY) && (*zValueStart == orxTEXT_KC_MARKER_SYNTAX_OPEN));

  orxTEXT_MARKER_DATA *pstResult;

  /* Figure out where the value ends */
  orxS32 s32EndIndex = orxString_SearchCharIndex(zValueStart, orxTEXT_KC_MARKER_SYNTAX_CLOSE, 1);

  /* No end? Bad marker! */
  if (s32EndIndex < 0)
  {
    pstResult = orxNULL;
    *_pzRemainder = zValueStart;
  }
  else
  {
    /* Allocate some marker data */
    pstResult = orxText_CreateMarkerData(_pstText, _eType);
    orxU32 u32ValueStringSize = (s32EndIndex + 2);
    /* Set remainder pointer */
    *_pzRemainder = zValueStart + u32ValueStringSize;

    /* Make a temporary string to hold the value alone */
#ifdef __orxMSVC__
    orxCHAR *zValueString = (orxCHAR *)alloca(u32ValueStringSize * sizeof(orxCHAR));
#else /* __orxMSVC__ */
    orxCHAR zValueString[u32ValueStringSize];
#endif /* __orxMSVC__ */

    orxString_NCopy(zValueString, zValueStart, u32ValueStringSize);
    zValueString[u32ValueStringSize - 1] = orxCHAR_NULL;

    /* Check style values - if something is invalid, fall through to default */
    switch(_eType)
    {
      /* Attempt to store font style */
    case orxTEXT_MARKER_TYPE_FONT:
    {
      orxCHAR cPrevValue = zValueString[u32ValueStringSize - 2];
      /* Modify the value string to exclude surrounding chars */
      zValueString[u32ValueStringSize - 2] = orxCHAR_NULL;
      /* Try and get the font */
      const orxFONT *pstFont = orxFont_CreateFromConfig(zValueString + 1);
      /* EDGE CASE: Handle invalid/missing font */
      if (pstFont != orxNULL)
      {
        /* If everything checks out, store the font and continue */
        pstResult->pstFont = pstFont;
        break;
      }
      /* Reset value close char for error message */
      zValueString[u32ValueStringSize - 2] = cPrevValue;
      /* Fall through */
    }
    /* Attempt to store color style */
    case orxTEXT_MARKER_TYPE_COLOR:
    {
      orxVECTOR vColor = {0};
      /* EDGE CASE: Handle invalid/missing color */
      /* TODO: We may want to use _pzRemaining for parsing an alpha value, if we choose to add it that way */
      if (orxString_ToVector(zValueString, &vColor, orxNULL) == orxSTATUS_SUCCESS)
      {
        orxVector_Mulf(&vColor, &vColor, orxCOLOR_NORMALIZER);
        orxCOLOR stColor = {vColor, 1.0f};
        pstResult->stRGBA = orxColor_ToRGBA(&stColor);
        break;
      }
      /* Fall through */
    }
    /* Attempt to store scale style */
    case orxTEXT_MARKER_TYPE_SCALE:
    {
      orxVECTOR vScale = {0};
      /* EDGE CASE: Handle invalid/missing scale */
      if (orxString_ToVector(zValueString, &vScale, orxNULL) == orxSTATUS_SUCCESS)
      {
        orxVector_Copy(&(pstResult->vScale), &vScale);
        break;
      }
      /* Fall through */
    }
    /* Handle invalid values/types */
    default:
    {
      /* Get type name */
      const orxSTRING zTypeName;
      switch(_eType)
      {
      case orxTEXT_MARKER_TYPE_FONT:
        zTypeName = orxTEXT_KZ_MARKER_TYPE_FONT;
        break;
      case orxTEXT_MARKER_TYPE_COLOR:
        zTypeName = orxTEXT_KZ_MARKER_TYPE_COLOR;
        break;
      case orxTEXT_MARKER_TYPE_SCALE:
        zTypeName = orxTEXT_KZ_MARKER_TYPE_SCALE;
        break;
      default:
        zTypeName = "none";
      }
      /* Delete allocated data */
      orxMemory_Free(pstResult);
      /* Set results accordingly */
      pstResult = orxNULL;
      *_pzRemainder = zValueStart + u32ValueStringSize;
      /* Log warning */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, orxTEXT_KZ_MARKER_WARNING, orxTEXT_KC_MARKER_SYNTAX_START, zTypeName, zValueString, _zString);
    }
    }
  }
  return pstResult;
}

/** Parses marker type
 * @param[in]      _zString             Whole unparsed string
 * @param[in]      _u32Offset           Offset in _zString to the start of the marker type
 * @param[out]     _pzRemainder         Where to store pointer to remainder of string
 * @return         orxTEXT_MARKER_TYPE
 */
static orxTEXT_MARKER_TYPE orxFASTCALL orxText_ParseMarkerType(const orxSTRING _zString, orxU32 _u32Offset, const orxSTRING *_pzRemainder)
{
  orxASSERT((_zString != orxNULL) && (_zString != orxSTRING_EMPTY));
  orxASSERT(_u32Offset != orxU32_UNDEFINED);
  orxASSERT(_pzRemainder != orxNULL);

  orxTEXT_MARKER_TYPE eResult = orxTEXT_MARKER_TYPE_NONE;

  const orxSTRING zTypeStart = _zString + _u32Offset;

  /* Find marker type */
  if (orxText_CheckMarkerType(orxTEXT_KZ_MARKER_TYPE_FONT, zTypeStart, _pzRemainder) == orxSTATUS_SUCCESS)
  {
    eResult = orxTEXT_MARKER_TYPE_FONT;
  }
  else if (orxText_CheckMarkerType(orxTEXT_KZ_MARKER_TYPE_COLOR, zTypeStart, _pzRemainder) == orxSTATUS_SUCCESS)
  {
    eResult = orxTEXT_MARKER_TYPE_COLOR;
  }
  else if (orxText_CheckMarkerType(orxTEXT_KZ_MARKER_TYPE_SCALE, zTypeStart, _pzRemainder) == orxSTATUS_SUCCESS)
  {
    eResult = orxTEXT_MARKER_TYPE_SCALE;
  }
  else if (orxText_CheckMarkerType(orxTEXT_KZ_MARKER_TYPE_POP, zTypeStart, _pzRemainder) == orxSTATUS_SUCCESS)
  {
    eResult = orxTEXT_MARKER_TYPE_POP;
  }
  else if (orxText_CheckMarkerType(orxTEXT_KZ_MARKER_TYPE_CLEAR, zTypeStart, _pzRemainder) == orxSTATUS_SUCCESS)
  {
    eResult = orxTEXT_MARKER_TYPE_CLEAR;
  }
  else
  {
    eResult = orxTEXT_MARKER_TYPE_NONE;
  }

  /* if (eResult != orxTEXT_MARKER_TYPE_NONE) */
  {
    /* Ensure the next char is valid */
    switch(eResult)
    {
    /* Stack modifiers don't have any special chars after them */
    case orxTEXT_MARKER_TYPE_POP:
    case orxTEXT_MARKER_TYPE_CLEAR:
      break;

    /* Marker types with a value are expected to be followed by a value opener char */
    case orxTEXT_MARKER_TYPE_COLOR:
    case orxTEXT_MARKER_TYPE_FONT:
    case orxTEXT_MARKER_TYPE_SCALE:
      if (**_pzRemainder == orxTEXT_KC_MARKER_SYNTAX_OPEN)
      {
        break;
      }
      /* If invalid char was found after marker type, fall through */

    /* Anything else is considered invalid */
    default:
      eResult = orxTEXT_MARKER_TYPE_NONE;
      /* Skip to next whitespace character */
      const orxSTRING zNextWhiteSpace = zTypeStart;
      while ((*zNextWhiteSpace != ' ') && (*zNextWhiteSpace != '\t') &&
             (*zNextWhiteSpace != orxCHAR_CR) && (*zNextWhiteSpace != orxCHAR_LF) &&
             (*zNextWhiteSpace != orxCHAR_NULL))
      {
        zNextWhiteSpace++;
      }
      orxU32 u32TypeStringSize = (orxU32)(zNextWhiteSpace - zTypeStart + 1);
      /* Make a temporary string to hold the bad value */
#ifdef __orxMSVC__
      orxCHAR *zTypeString = (orxCHAR *)alloca(u32TypeStringSize * sizeof(orxCHAR));
#else /* __orxMSVC__ */
      orxCHAR zTypeString[u32TypeStringSize];
#endif /* __orxMSVC__ */
      orxString_NCopy(zTypeString, zTypeStart, u32TypeStringSize - 1);
      zTypeString[u32TypeStringSize - 1] = orxCHAR_NULL;
      /* Log warning */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, orxTEXT_KZ_MARKER_WARNING, orxTEXT_KC_MARKER_SYNTAX_START, zTypeString, orxSTRING_EMPTY, _zString);
      /* Advance next token to whitespace */
      *_pzRemainder = zNextWhiteSpace;
    }
  }

  /* Done */
  return eResult;
}

/** Process markers out of the text string, storing the markers in a list and returning an unmarked string
 * @param[in] _pstText    Concerned text
 * @param[in] _zString    Unprocessed string
 * @return    orxSTRING / orxSTRING_EMPTY
 */
static const orxSTRING orxFASTCALL orxText_ProcessMarkedString(orxTEXT *_pstText, const orxSTRING _zString)
{
  const orxSTRING zMarkedString = orxNULL;
  const orxSTRING zResult       = orxSTRING_EMPTY;
  orxSTRING zCleanedString      = orxNULL;
  orxU32 u32CleanedSize = 0, u32CleanedSizeUsed = 0;

  /* These are used for keeping track of marker type fallbacks */
  orxTEXT_MARKER_FALLBACKS stFallbacks = {orxNULL, orxNULL, orxNULL};

  /* Used for a dry run of marker traversal */
  orxBANK      *pstDryRunBank = orxNULL;
  orxLINKLIST   stDryRunStack = {0};

  /* Clear banks */
  orxBank_Clear(_pstText->pstMarkerCells);
  orxBank_Clear(_pstText->pstMarkerDatas);
  orxLinkList_Clean(&_pstText->stMarkers);

  /* If string is invalid, return it. */
  if (_zString == orxNULL || _zString == orxSTRING_EMPTY)
  {
    return _zString;
  }

  /* Initialize string traversal/storage variables */

  zMarkedString    = _zString;
  u32CleanedSize   = orxString_GetLength(zMarkedString) + 1;
  u32CleanedSizeUsed = 0;

  zCleanedString   = (orxSTRING) orxMemory_Allocate(u32CleanedSize * sizeof(orxCHAR), orxMEMORY_TYPE_MAIN);
  orxASSERT(zCleanedString != orxNULL);
  orxMemory_Zero(zCleanedString, u32CleanedSize * sizeof(orxCHAR));

  pstDryRunBank    = orxBank_Create(orxTEXT_KU32_MARKER_DATA_BANK_SIZE, sizeof(orxTEXT_MARKER_STACK_ENTRY),
                                    orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);

  /* Parse the string using zMarkedString as a pointer to our current position in _zString */
  while ((zMarkedString != orxNULL) && (zMarkedString != orxSTRING_EMPTY) && (*zMarkedString != orxCHAR_NULL))
  {
    /* Start of marker? */
    if (*zMarkedString != orxTEXT_KC_MARKER_SYNTAX_START)
    {
      /* Non-marker text */
      zCleanedString[u32CleanedSizeUsed] = *zMarkedString;
      u32CleanedSizeUsed++;
      zMarkedString++;
    }
    else
    {
      /* Marker text? Let's find out. */
      zMarkedString++;

      /* Is escape? */
      if (*zMarkedString == orxTEXT_KC_MARKER_SYNTAX_START)
      {
        /* Store escaped char */
        zCleanedString[u32CleanedSizeUsed] = *zMarkedString;
        u32CleanedSizeUsed++;
        zMarkedString++;
      }
      else
      {
        /* Parse marker type */
        orxTEXT_MARKER_TYPE eType = orxText_ParseMarkerType(_zString, (orxU32)(zMarkedString - _zString), &zMarkedString);
        /* Is type valid? */
        if (eType == orxTEXT_MARKER_TYPE_NONE)
        {
          zCleanedString[u32CleanedSizeUsed] = *zMarkedString;
          u32CleanedSizeUsed++;
          zMarkedString++;
        }
        else if (eType == orxTEXT_MARKER_TYPE_POP)
        {
          /* We can't pop the stack if it's already empty */
          if (orxLinkList_GetCounter(&stDryRunStack) > 0)
          {
            /* Pop the stack, updating what pstFallbackData points to */
            orxText_PopMarker(_pstText, u32CleanedSizeUsed, orxTRUE, &stFallbacks, &stDryRunStack, pstDryRunBank);
          }

          /* Continue parsing */
        }
        else if (eType == orxTEXT_MARKER_TYPE_CLEAR)
        {
          /* Clear out the stack */
          while (orxLinkList_GetCounter(&stDryRunStack) > 0)
          {
            /* Pop the stack, updating what pstFallbackData points to */
            orxText_PopMarker(_pstText, u32CleanedSizeUsed, orxFALSE, &stFallbacks, &stDryRunStack, pstDryRunBank);
          }

          /* Clear storage */
          orxLinkList_Clean(&stDryRunStack);
          orxBank_Clear(pstDryRunBank);

          /* Let's see if this cleared out properly */
          orxASSERT(stFallbacks.pstColor == orxNULL || (stFallbacks.pstColor != orxNULL) && (stFallbacks.pstColor->eType == orxTEXT_MARKER_TYPE_REVERT));
          orxASSERT(stFallbacks.pstFont == orxNULL || (stFallbacks.pstFont != orxNULL) && (stFallbacks.pstFont->eType == orxTEXT_MARKER_TYPE_REVERT));
          orxASSERT(stFallbacks.pstScale == orxNULL || (stFallbacks.pstScale != orxNULL) && (stFallbacks.pstScale->eType == orxTEXT_MARKER_TYPE_REVERT));

          /* Continue parsing */
        }
        else
        {
          /* This marker has data associated with it */
          orxASSERT(*zMarkedString == orxTEXT_KC_MARKER_SYNTAX_OPEN)
          orxTEXT_MARKER_DATA *pstData = orxNULL;
          /* Parse the marker value into marker data */
          pstData = orxText_ParseMarkerValue(_pstText, eType, _zString, (orxU32)(zMarkedString - _zString), &zMarkedString);
          zMarkedString--;
          /* Add/Push marker */
          if (pstData != orxNULL)
          {
            /* The type we plan to store will determine which fallback pointer needs to be updated */
            const orxTEXT_MARKER_DATA **ppstFallbackData = orxNULL;
            ppstFallbackData = orxText_GetMarkerFallbackPointer(eType, &stFallbacks);
            /* Fallback data cannot be null if the data was of a valid type */
            orxASSERT(ppstFallbackData);

            /* Push data to stack with fallback data */
            orxTEXT_MARKER_STACK_ENTRY *pstStackEntry = orxText_AddMarkerStackEntry(&stDryRunStack, pstDryRunBank, pstData, *ppstFallbackData);
            /* Add a marker cell (implicitly represents final traversal order )*/
            orxTEXT_MARKER_CELL *pstMarker = orxText_AddMarkerCell(_pstText, u32CleanedSizeUsed, pstData, orxFALSE);
            /* Update the fallback data pointer */
            *ppstFallbackData = pstData;
          }
          else
          {
            
          }
          /* Continue parsing */
        }
      }
    }
  }

  /* Free the dry run bank full of any remaining stack entries */
  orxBank_Delete(pstDryRunBank);

  /* TODO: There's probably a better way to represent this edge case */
  if (*(zCleanedString + u32CleanedSizeUsed) != orxCHAR_NULL)
  {
    /* Terminate cleaned string - just to be safe */
    zCleanedString[u32CleanedSizeUsed - 1] = orxCHAR_NULL;
  }

  /* Has new string? */
  if((zCleanedString != orxNULL) && (zCleanedString != orxSTRING_EMPTY))
  {
    /* Stores a duplicate */
    zResult = orxString_Store(zCleanedString);
  }

  /* Since the string is now stored internally, we can safely free it of its mortal coil */
  orxMemory_Free(zCleanedString);

  /* TODO: Remove redundant tags from the bank/list and compress it. */

  /* Done! */
  return zResult;
}

static orxSTATUS orxFASTCALL orxText_ProcessConfigData(orxTEXT *_pstText)
{
  const orxSTRING zString;
  const orxSTRING zName;
  orxSTATUS       eResult = orxSTATUS_FAILURE;

  /* Pushes its config section */
  orxConfig_PushSection(_pstText->zReference);

  /* Gets font name */
  zName = orxConfig_GetString(orxTEXT_KZ_CONFIG_FONT);

  /* Begins with locale marker? */
  if(*zName == orxTEXT_KC_LOCALE_MARKER)
  {
    /* Gets its locale value */
    zName = (*(zName + 1) == orxTEXT_KC_LOCALE_MARKER) ? zName + 1 : orxLocale_GetString(zName + 1);
  }

  /* Valid? */
  if((zName != orxNULL) && (zName != orxSTRING_EMPTY))
  {
    orxFONT *pstFont;

    /* Creates font */
    pstFont = orxFont_CreateFromConfig(zName);

    /* Valid? */
    if(pstFont != orxNULL)
    {
      /* Stores it */
      if(orxText_SetFont(_pstText, pstFont) != orxSTATUS_FAILURE)
      {
        /* Sets its owner */
        orxStructure_SetOwner(pstFont, _pstText);

        /* Updates flags */
        orxStructure_SetFlags(_pstText, orxTEXT_KU32_FLAG_INTERNAL, orxTEXT_KU32_FLAG_NONE);
      }
      else
      {
        /* Logs message */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't set font (%s) for text (%s).", zName, _pstText->zReference);

        /* Sets default font */
        orxText_SetFont(_pstText, orxFONT(orxFont_GetDefaultFont()));
      }
    }
    else
    {
      /* Logs message */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't create font (%s) for text (%s).", zName, _pstText->zReference);

      /* Sets default font */
      orxText_SetFont(_pstText, orxFONT(orxFont_GetDefaultFont()));
    }
  }
  else
  {
    /* Sets default font */
    orxText_SetFont(_pstText, orxFONT(orxFont_GetDefaultFont()));
  }

  /* Gets its string */
  zString = orxConfig_GetString(orxTEXT_KZ_CONFIG_STRING);

  /* Begins with locale marker? */
  if(*zString == orxTEXT_KC_LOCALE_MARKER)
  {
    /* Stores its locale value */
    eResult = orxText_SetString(_pstText, (*(zString + 1) == orxTEXT_KC_LOCALE_MARKER) ? zString + 1 : orxLocale_GetString(zString + 1));
  }
  else
  {
    /* Stores raw text */
    eResult = orxText_SetString(_pstText, zString);
  }

  /* Pops config section */
  orxConfig_PopSection();

  /* Done! */
  return eResult;
}

/** Event handler
 * @param[in]   _pstEvent                     Sent event
 * @return      orxSTATUS_SUCCESS if handled / orxSTATUS_FAILURE otherwise
 */
static orxSTATUS orxFASTCALL orxText_EventHandler(const orxEVENT *_pstEvent)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Locale? */
  if(_pstEvent->eType == orxEVENT_TYPE_LOCALE)
  {
    /* Select language event? */
    if(_pstEvent->eID == orxLOCALE_EVENT_SELECT_LANGUAGE)
    {
      orxTEXT *pstText;

      /* For all texts */
      for(pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));
          pstText != orxNULL;
          pstText = orxTEXT(orxStructure_GetNext(pstText)))
      {
        const orxSTRING zLocaleKey;

        /* Gets its corresponding locale string */
        zLocaleKey = orxText_GetLocaleKey(pstText, orxTEXT_KZ_CONFIG_STRING);

        /* Valid? */
        if(zLocaleKey != orxNULL)
        {
          const orxSTRING zText;

          /* Gets its localized value */
          zText = orxLocale_GetString(zLocaleKey);

          /* Valid? */
          if(*zText != orxCHAR_NULL)
          {
            /* Updates text */
            orxText_SetString(pstText, zText);
          }
        }

        /* Gets its corresponding locale font */
        zLocaleKey = orxText_GetLocaleKey(pstText, orxTEXT_KZ_CONFIG_FONT);

        /* Valid? */
        if(zLocaleKey != orxNULL)
        {
          orxFONT *pstFont;

          /* Creates font */
          pstFont = orxFont_CreateFromConfig(orxLocale_GetString(zLocaleKey));

          /* Valid? */
          if(pstFont != orxNULL)
          {
            /* Updates text */
            if(orxText_SetFont(pstText, pstFont) != orxSTATUS_FAILURE)
            {
              /* Sets its owner */
              orxStructure_SetOwner(pstFont, pstText);

              /* Updates flags */
              orxStructure_SetFlags(pstText, orxTEXT_KU32_FLAG_INTERNAL, orxTEXT_KU32_FLAG_NONE);
            }
            else
            {
              /* Sets default font */
              orxText_SetFont(pstText, orxFONT(orxFont_GetDefaultFont()));
            }
          }
        }
      }
    }
  }
  /* Resource */
  else
  {
    /* Checks */
    orxASSERT(_pstEvent->eType == orxEVENT_TYPE_RESOURCE);

    /* Add or update? */
    if((_pstEvent->eID == orxRESOURCE_EVENT_ADD) || (_pstEvent->eID == orxRESOURCE_EVENT_UPDATE))
    {
      orxRESOURCE_EVENT_PAYLOAD *pstPayload;

      /* Gets payload */
      pstPayload = (orxRESOURCE_EVENT_PAYLOAD *)_pstEvent->pstPayload;

      /* Is config group? */
      if(pstPayload->u32GroupID == orxString_ToCRC(orxCONFIG_KZ_RESOURCE_GROUP))
      {
        orxTEXT *pstText;

        /* For all texts */
        for(pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));
            pstText != orxNULL;
            pstText = orxTEXT(orxStructure_GetNext(pstText)))
        {
          /* Match origin? */
          if(orxConfig_GetOriginID(pstText->zReference) == pstPayload->u32NameID)
          {
            /* Re-processes its config data */
            orxText_ProcessConfigData(pstText);
          }
        }
      }
    }
  }

  /* Done! */
  return eResult;
}

/** Updates text size and inserts line-height markers
 * @param[in]   _pstText      Concerned text
 */
static void orxFASTCALL orxText_UpdateSize(orxTEXT *_pstText)
{
  /* Checks */
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has string and font? */
  if((_pstText->zString != orxNULL) && (_pstText->zString != orxSTRING_EMPTY) && (_pstText->pstFont != orxNULL))
  {

    orxFLOAT        fWidth, fMaxWidth, fHeight, fMaxLineHeight, fCharacterHeight;
    orxU32          u32CharacterCodePoint, u32CharacterIndex, u32LineStartIndex;
    orxVECTOR       vScale;
    const orxFONT  *pstFont;
    orxHANDLE       hIterator;
    orxTEXT_MARKER_DATA *pstLineStartMarkerData;
    const orxCHAR  *pc;

    /* Initialize marker values */
    hIterator = orxNULL;
    vScale = orxVECTOR_1;
    pstFont = _pstText->pstFont;

    /* Gets character height */
    fCharacterHeight = orxFont_GetCharacterHeight(_pstText->pstFont);

    /* Insert marker to identify max line height for rendering */
    u32LineStartIndex = 0;
    pstLineStartMarkerData =  orxText_CreateMarkerData(_pstText, orxTEXT_MARKER_TYPE_LINE_HEIGHT);
    orxText_AddMarkerCell(_pstText, u32LineStartIndex, pstLineStartMarkerData, orxTRUE);

    /* For all characters */
    for(u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(_pstText->zString, &pc), u32CharacterIndex = 0, fHeight = fMaxLineHeight = fCharacterHeight, fWidth = fMaxWidth = orxFLOAT_0;
        u32CharacterCodePoint != orxCHAR_NULL;
        u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(pc, &pc))
    {
      /* Apply marker font/scale modifications */
      for ( hIterator = ((hIterator != orxNULL) ? hIterator : orxText_FirstMarker(_pstText));
           (hIterator != orxHANDLE_UNDEFINED) && (orxText_GetMarkerIndex(hIterator) == u32CharacterIndex);
            hIterator = orxText_NextMarker(hIterator) )
      {
        orxTEXT_MARKER_TYPE eType = orxText_GetMarkerType(hIterator);
        /* New scale */
        if (eType == orxTEXT_MARKER_TYPE_SCALE)
        {
          orxASSERT(orxText_GetMarkerScale(hIterator, &vScale) == orxSTATUS_SUCCESS);
        }
        /* New font */
        else if (eType == orxTEXT_MARKER_TYPE_FONT)
        {
          orxASSERT(orxText_GetMarkerFont(hIterator, &pstFont) == orxSTATUS_SUCCESS);
        }
        /* Revert to default values */
        else if ((eType == orxTEXT_MARKER_TYPE_REVERT) && (orxText_GetMarkerRevertType(hIterator, &eType) == orxSTATUS_SUCCESS))
        {
          if (eType == orxTEXT_MARKER_TYPE_SCALE)
          {
            vScale = orxVECTOR_1;
          }
          else if (eType == orxTEXT_MARKER_TYPE_FONT)
          {
            pstFont = _pstText->pstFont;
          }
        }
        /* Update font */
        pstFont = (pstFont != orxNULL) ? pstFont : _pstText->pstFont;
        /* Update character height based on font */
        fCharacterHeight = orxFont_GetCharacterHeight(pstFont);
        /* Update max line height using font character height and scale */
        pstLineStartMarkerData->fLineHeight = fMaxLineHeight = orxMAX(fMaxLineHeight, fCharacterHeight * vScale.fY);
      }

      /* Depending on character */
      switch(u32CharacterCodePoint)
      {
        case orxCHAR_CR:
        {
          /* Half EOL? */
          if(*pc == orxCHAR_LF)
          {
            /* Updates pointer */
            pc++;
            /* Increment character index */
            u32CharacterIndex++;
          }

          /* Fall through */
        }

        case orxCHAR_LF:
        {
          /* Create line height marker for next line */
          pstLineStartMarkerData = orxText_CreateMarkerData(_pstText, orxTEXT_MARKER_TYPE_LINE_HEIGHT);
          u32LineStartIndex = u32CharacterIndex + 1;
          orxText_AddMarkerCell(_pstText, u32LineStartIndex, pstLineStartMarkerData, orxTRUE);

          /* Updates height */
          fHeight += fMaxLineHeight;

          /* Updates max width */
          fMaxWidth = orxMAX(fMaxWidth, fWidth);

          /* Resets width */
          fWidth = orxFLOAT_0;

          /* Resets max line height to whatever the current scaled glyph height is */
          fMaxLineHeight = fCharacterHeight * vScale.fY;
          /* Default the line height value for the next line in case it's the last line */
          pstLineStartMarkerData->fLineHeight = fMaxLineHeight;

          break;
        }

        default:
        {
          /* Updates width */
          fWidth += orxFont_GetCharacterWidth(pstFont, u32CharacterCodePoint) * vScale.fX;

          break;
        }
      }

      /* Increment character index */
      u32CharacterIndex++;
    }

    /* Stores values */
    _pstText->fWidth  = orxMAX(fWidth, fMaxWidth);
    _pstText->fHeight = fHeight;
  }
  else
  {
    /* Clears values */
    _pstText->fWidth = _pstText->fHeight = orxFLOAT_0;
  }

  /* Done! */
  return;
}

/** Deletes all texts
 */
static orxINLINE void orxText_DeleteAll()
{
  orxTEXT *pstText;

  /* Gets first text */
  pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));

  /* Non empty? */
  while(pstText != orxNULL)
  {
    /* Deletes text */
    orxText_Delete(pstText);

    /* Gets first text */
    pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));
  }

  return;
}


/***************************************************************************
 * Public functions                                                        *
 ***************************************************************************/

/** Setups the text module
 */
void orxFASTCALL orxText_Setup()
{
  /* Adds module dependencies */
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_MEMORY);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_CONFIG);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_EVENT);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_FONT);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_LOCALE);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_STRUCTURE);

  return;
}

/** Inits the text module
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_Init()
{
  orxSTATUS eResult;

  /* Not already Initialized? */
  if(!(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY))
  {
    /* Cleans static controller */
    orxMemory_Zero(&sstText, sizeof(orxTEXT_STATIC));

    /* Adds event handler */
    eResult = orxEvent_AddHandler(orxEVENT_TYPE_LOCALE, orxText_EventHandler);

    /* Valid? */
    if(eResult != orxSTATUS_FAILURE)
    {
      /* Registers structure type */
      eResult = orxSTRUCTURE_REGISTER(TEXT, orxSTRUCTURE_STORAGE_TYPE_LINKLIST, orxMEMORY_TYPE_MAIN, orxTEXT_KU32_BANK_SIZE, orxNULL);

      /* Success? */
      if(eResult != orxSTATUS_FAILURE)
      {
        /* Updates flags for screen text creation */
        sstText.u32Flags = orxTEXT_KU32_STATIC_FLAG_READY;

        /* Adds event handler for resources */
        orxEvent_AddHandler(orxEVENT_TYPE_RESOURCE, orxText_EventHandler);
      }
      else
      {
        /* Removes event handler */
        orxEvent_RemoveHandler(orxEVENT_TYPE_LOCALE, orxText_EventHandler);
      }
    }
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Tried to initialize text module when it was already initialized.");

    /* Already initialized */
    eResult = orxSTATUS_SUCCESS;
  }

  /* Not initialized? */
  if(eResult == orxSTATUS_FAILURE)
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Initializing text module failed.");

    /* Updates Flags */
    sstText.u32Flags &= ~orxTEXT_KU32_STATIC_FLAG_READY;
  }

  /* Done! */
  return eResult;
}

/** Exits from the text module
 */
void orxFASTCALL orxText_Exit()
{
  /* Initialized? */
  if(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY)
  {
    /* Deletes text list */
    orxText_DeleteAll();

    /* Removes event handlers */
    orxEvent_RemoveHandler(orxEVENT_TYPE_RESOURCE, orxText_EventHandler);
    orxEvent_RemoveHandler(orxEVENT_TYPE_LOCALE, orxText_EventHandler);

    /* Unregisters structure type */
    orxStructure_Unregister(orxSTRUCTURE_ID_TEXT);

    /* Updates flags */
    sstText.u32Flags &= ~orxTEXT_KU32_STATIC_FLAG_READY;
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Tried to exit text module when it wasn't initialized.");
  }

  return;
}

/** Creates an empty text
 * @return      orxTEXT / orxNULL
 */
orxTEXT *orxFASTCALL orxText_Create()
{
  orxTEXT *pstResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);

  /* Creates text */
  pstResult = orxTEXT(orxStructure_Create(orxSTRUCTURE_ID_TEXT));

  /* Created? */
  if(pstResult != orxNULL)
  {
    /* Inits it */
    pstResult->zString    = orxNULL;
    pstResult->pstFont    = orxNULL;
    pstResult->pstMarkerDatas  = orxBank_Create(orxTEXT_KU32_MARKER_DATA_BANK_SIZE, sizeof(orxTEXT_MARKER_DATA),
                                           orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);
    pstResult->pstMarkerCells = orxBank_Create(orxTEXT_KU32_MARKER_CELL_BANK_SIZE, sizeof(orxTEXT_MARKER_CELL),
                                           orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);
    orxMemory_Zero(&pstResult->stMarkers, sizeof(orxLINKLIST));

    /* Inits flags */
    orxStructure_SetFlags(pstResult, orxTEXT_KU32_FLAG_NONE, orxTEXT_KU32_MASK_ALL);

    /* Increases counter */
    orxStructure_IncreaseCounter(pstResult);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Failed to create structure for text.");
  }

  /* Done! */
  return pstResult;
}

/** Creates a text from config
 * @param[in]   _zConfigID    Config ID
 * @return      orxTEXT / orxNULL
 */
orxTEXT *orxFASTCALL orxText_CreateFromConfig(const orxSTRING _zConfigID)
{
  orxTEXT *pstResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxASSERT((_zConfigID != orxNULL) && (_zConfigID != orxSTRING_EMPTY));

  /* Pushes section */
  if((orxConfig_HasSection(_zConfigID) != orxFALSE)
  && (orxConfig_PushSection(_zConfigID) != orxSTATUS_FAILURE))
  {
    /* Creates text */
    pstResult = orxText_Create();

    /* Valid? */
    if(pstResult != orxNULL)
    {
      /* Stores its reference key */
      pstResult->zReference = orxString_Store(orxConfig_GetCurrentSection());

      /* Processes its config data */
      if(orxText_ProcessConfigData(pstResult) == orxSTATUS_FAILURE)
      {
        /* Logs message */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't process config data for text <%s>.", _zConfigID);

        /* Deletes it */
        orxText_Delete(pstResult);

        /* Updates result */
        pstResult = orxNULL;
      }
    }

    /* Pops previous section */
    orxConfig_PopSection();
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't find config section named (%s).", _zConfigID);

    /* Updates result */
    pstResult = orxNULL;
  }

  /* Done! */
  return pstResult;
}

/** Deletes a text
 * @param[in]   _pstText      Concerned text
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_Delete(orxTEXT *_pstText)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Decreases counter */
  orxStructure_DecreaseCounter(_pstText);

  /* Not referenced? */
  if(orxStructure_GetRefCounter(_pstText) == 0)
  {
    /* Removes string */
    orxText_SetString(_pstText, orxNULL);

    /* Removes font */
    orxText_SetFont(_pstText, orxNULL);

    /* Deletes markers */
    orxBank_Delete(_pstText->pstMarkerCells);

    /* Deletes styles */
    orxBank_Delete(_pstText->pstMarkerDatas);

    /* Deletes structure */
    orxStructure_Delete(_pstText);
  }
  else
  {
    /* Referenced by others */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Gets text size
 * @param[in]   _pstText      Concerned text
 * @param[out]  _pfWidth      Text's width
 * @param[out]  _pfHeight     Text's height
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_GetSize(const orxTEXT *_pstText, orxFLOAT *_pfWidth, orxFLOAT *_pfHeight)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);
  orxASSERT(_pfWidth != orxNULL);
  orxASSERT(_pfHeight != orxNULL);

  /* Updates result */
  *_pfWidth   = _pstText->fWidth;
  *_pfHeight  = _pstText->fHeight;

  /* Done! */
  return eResult;
}

/** Gets text name
 * @param[in]   _pstText      Concerned text
 * @return      Text name / orxNULL
 */
const orxSTRING orxFASTCALL orxText_GetName(const orxTEXT *_pstText)
{
  const orxSTRING zResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Updates result */
  zResult = (_pstText->zReference != orxNULL) ? _pstText->zReference : orxSTRING_EMPTY;

  /* Done! */
  return zResult;
}

/** Gets text string
 * @param[in]   _pstText      Concerned text
 * @return      Text string / orxSTRING_EMPTY
 */
const orxSTRING orxFASTCALL orxText_GetString(const orxTEXT *_pstText)
{
  const orxSTRING zResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has string? */
  if(_pstText->zString != orxNULL)
  {
    /* Updates result */
    zResult = _pstText->zString;
  }
  else
  {
    /* Updates result */
    zResult = orxSTRING_EMPTY;
  }

  /* Done! */
  return zResult;
}

/** Gets text font
 * @param[in]   _pstText      Concerned text
 * @return      Text font / orxNULL
 */
orxFONT *orxFASTCALL orxText_GetFont(const orxTEXT *_pstText)
{
  orxFONT *pstResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Updates result */
  pstResult = _pstText->pstFont;

  /* Done! */
  return pstResult;
}

/** Sets text string
 * @param[in]   _pstText      Concerned text
 * @param[in]   _zString      String to contain
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_SetString(orxTEXT *_pstText, const orxSTRING _zString)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has current string? */
  if((_pstText->zString != orxNULL) && (_pstText->zString != orxSTRING_EMPTY))
  {
    /* Cleans it */
    _pstText->zString = orxNULL;
  }

  /* Process markers out of the string */
  _zString = orxText_ProcessMarkedString(_pstText, _zString);

  /* Has new string? */
  if((_zString != orxNULL) && (_zString != orxSTRING_EMPTY))
  {
    /* Stores a duplicate */
    _pstText->zString = orxString_Store(_zString);
  }

  /* Updates text size */
  orxText_UpdateSize(_pstText);

  /* Validate final marker list */
  /* TODO: Should this be an assert, or return the result? It is more of a sanity test. */
  orxASSERT(orxText_ValidateMarkers(_pstText, orxNULL) == orxSTATUS_SUCCESS);

  /* Done! */
  return eResult;
}

/** Sets text font
 * @param[in]   _pstText      Concerned text
 * @param[in]   _pstFont      Font / orxNULL to use default
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_SetFont(orxTEXT *_pstText, orxFONT *_pstFont)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Different? */
  if(_pstText->pstFont != _pstFont)
  {
    /* Has current font? */
    if(_pstText->pstFont != orxNULL)
    {
      /* Updates structure reference counter */
      orxStructure_DecreaseCounter(_pstText->pstFont);

      /* Internally handled? */
      if(orxStructure_TestFlags(_pstText, orxTEXT_KU32_FLAG_INTERNAL))
      {
        /* Removes its owner */
        orxStructure_SetOwner(_pstText->pstFont, orxNULL);

        /* Deletes it */
        orxFont_Delete(_pstText->pstFont);

        /* Updates flags */
        orxStructure_SetFlags(_pstText, orxTEXT_KU32_FLAG_NONE, orxTEXT_KU32_FLAG_INTERNAL);
      }

      /* Cleans it */
      _pstText->pstFont = orxNULL;
    }

    /* Has new font? */
    if(_pstFont != orxNULL)
    {
      /* Stores it */
      _pstText->pstFont = _pstFont;

      /* Updates its reference counter */
      orxStructure_IncreaseCounter(_pstFont);
    }

    /* Updates text's size */
    orxText_UpdateSize(_pstText);
  }

  /* Done! */
  return eResult;
}

/** Get handle of first marker for iteration
 * @param[in] _pstText  Concerned text
 * @return orxHANDLE / orxHANDLE_UNDEFINED
 */
orxHANDLE orxFASTCALL orxText_FirstMarker(const orxTEXT *_pstText)
{
  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  orxTEXT_MARKER_CELL *pstCell;
  orxHANDLE hResult;
  if ((orxBank_GetCounter(_pstText->pstMarkerCells) > 0) && (orxLinkList_GetCounter(&_pstText->stMarkers) > 0))
  {
    pstCell = (orxTEXT_MARKER_CELL *) orxLinkList_GetFirst(&_pstText->stMarkers);
    hResult = (orxHANDLE) pstCell;
  }
  else
  {
    hResult = orxHANDLE_UNDEFINED;
  }
  return hResult;
}

/** Get next marker handle
 * @param[in] _hIterator  Iterator from previous search
 * @return Iterator for next element if an element has been found, orxHANDLE_UNDEFINED otherwise
 */
orxHANDLE orxFASTCALL orxText_NextMarker(orxHANDLE _hIterator)
{
  orxTEXT_MARKER_CELL *pstCell;
  orxHANDLE hResult;
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    hResult = (orxHANDLE) orxLinkList_GetNext((orxLINKLIST_NODE *) pstCell);
  }
  else
  {
    hResult = orxHANDLE_UNDEFINED;
  }
  return hResult;
}

/** Gets marker index (position) in the string it's a part of
 * @param[in] _hIterator  Marker handle
 * @return orxU32 index / orxU32_UNDEFINED for invalid marker handle
 */
orxU32 orxFASTCALL orxText_GetMarkerIndex(orxHANDLE _hIterator)
{
  orxTEXT_MARKER_CELL *pstCell;
  orxU32 u32Result;
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    u32Result = pstCell->u32Index;
  }
  else
  {
    u32Result = orxU32_UNDEFINED;
  }
  return u32Result;
}

/** Get marker type
 * @param[in]   _hIterator    Marker handle
 * @return      Marker type
 */
orxTEXT_MARKER_TYPE orxFASTCALL orxText_GetMarkerType(orxHANDLE _hIterator)
{
  orxTEXT_MARKER_CELL *pstCell;
  orxTEXT_MARKER_TYPE eResult;
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    if (pstCell->pstData != orxNULL)
    {
      eResult = pstCell->pstData->eType;
    }
    else
    {
      eResult = orxTEXT_MARKER_TYPE_NONE;
    }
  }
  else
  {
    eResult = orxTEXT_MARKER_TYPE_NONE;
  }
  return eResult;
}

/** Get marker font
 * @param[in]   _hIterator    Marker handle
 * @param[out]  _ppstFont     Marker font pointer / orxNULL
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_GetMarkerFont(orxHANDLE _hIterator, orxFONT const **_ppstFont)
{
  orxTEXT_MARKER_CELL *pstCell = orxNULL;
  orxSTATUS eResult = orxSTATUS_FAILURE;
  orxASSERT(_ppstFont != orxNULL);
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    if ((pstCell->pstData != orxNULL) && (pstCell->pstData->eType == orxTEXT_MARKER_TYPE_FONT))
    {
      *_ppstFont = pstCell->pstData->pstFont;
      eResult = orxSTATUS_SUCCESS;
    }
  }
  else
  {
    *_ppstFont = orxNULL;
  }
  return eResult;
}

/** Get marker color
 * @param[in]   _hIterator    Marker handle
 * @param[out]  _pstColor     Marker color / White
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_GetMarkerColor(orxHANDLE _hIterator, orxRGBA *_pstColor)
{
  orxTEXT_MARKER_CELL *pstCell = orxNULL;
  orxSTATUS eResult = orxSTATUS_FAILURE;
  orxASSERT(_pstColor != orxNULL);
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    if ((pstCell->pstData != orxNULL) && (pstCell->pstData->eType == orxTEXT_MARKER_TYPE_COLOR))
    {
      *_pstColor = pstCell->pstData->stRGBA;
      eResult = orxSTATUS_SUCCESS;
    }
  }
  else
  {
    *_pstColor = orx2RGBA(255, 255, 255, 255);
  }
  return eResult;
}

/** Get marker scale
 * @param[in]   _hIterator    Marker handle
 * @param[out]  _pvScale      Marker scale / orxVECTOR_1
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_GetMarkerScale(orxHANDLE _hIterator, orxVECTOR *_pvScale)
{
  orxTEXT_MARKER_CELL *pstCell = orxNULL;
  orxSTATUS eResult = orxSTATUS_FAILURE;
  orxASSERT(_pvScale != orxNULL);
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    if ((pstCell->pstData != orxNULL) && (pstCell->pstData->eType == orxTEXT_MARKER_TYPE_SCALE))
    {
      *_pvScale = pstCell->pstData->vScale;
      eResult = orxSTATUS_SUCCESS;
    }
  }
  else
  {
    *_pvScale = orxVECTOR_1;
  }
  return eResult;
}

/** Get marker line height
 * @param[in]   _hIterator    Marker handle
 * @param[out]  _ppstScale    Marker line height / orxFLOAT_0
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_GetMarkerLineHeight(orxHANDLE _hIterator, orxFLOAT *_pfHeight)
{
  orxTEXT_MARKER_CELL *pstCell = orxNULL;
  orxSTATUS eResult = orxSTATUS_FAILURE;
  orxASSERT(_pfHeight != orxNULL);
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    if ((pstCell->pstData != orxNULL) && (pstCell->pstData->eType == orxTEXT_MARKER_TYPE_LINE_HEIGHT))
    {
      *_pfHeight = pstCell->pstData->fLineHeight;
      eResult = orxSTATUS_SUCCESS;
    }
  }
  else
  {
    *_pfHeight = orxFLOAT_0;
  }
  return eResult;
}

/** Get marker revert type
 * @param[in]   _hIterator    Marker handle
 * @param[out]  _ppstScale    Marker revert type / orxTEXT_MARKER_TYPE_NONE
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_GetMarkerRevertType(orxHANDLE _hIterator, orxTEXT_MARKER_TYPE *_peType)
{
  orxTEXT_MARKER_CELL *pstCell = orxNULL;
  orxSTATUS eResult = orxSTATUS_FAILURE;
  orxASSERT(_peType != orxNULL);
  if ((_hIterator != orxNULL) && (_hIterator != orxHANDLE_UNDEFINED))
  {
    pstCell = (orxTEXT_MARKER_CELL *) _hIterator;
    if ((pstCell->pstData != orxNULL) && (pstCell->pstData->eType == orxTEXT_MARKER_TYPE_REVERT))
    {
      *_peType = pstCell->pstData->eRevertType;
      eResult = orxSTATUS_SUCCESS;
    }
  }
  else
  {
    *_peType = orxTEXT_MARKER_TYPE_NONE;
  }
  return eResult;
}

