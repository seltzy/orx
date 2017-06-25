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

/** Marker node
 *  Used specifically for dry run of marker traversal
 */
typedef struct __orxTEXT_MARKER_NODE_t
{
  orxLINKLIST_NODE           stNode;
  const orxTEXT_MARKER_DATA *pstData;
  const orxTEXT_MARKER_DATA *pstFallbackData;
} orxTEXT_MARKER_NODE;

/** Text structure
 */
struct __orxTEXT_t
{
  orxSTRUCTURE          stStructure;                /**< Public structure, first structure member : 32 */
  orxFONT              *pstFont;                    /**< Font : 20 */
  orxTEXT_MARKER       *pstMarkers;
  orxU32                u32MarkerCounter;
  const orxSTRING       zString;                    /**< String : 24 */
  orxFLOAT              fWidth;                     /**< Width : 28 */
  orxFLOAT              fHeight;                    /**< Height : 32 */
  const orxSTRING       zReference;                 /**< Config reference : 36 */
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

static orxTEXT_MARKER *orxFASTCALL orxText_CreateMarker(orxBANK *_pstMarkerBank, orxU32 _u32Index, const orxTEXT_MARKER_DATA *_pstData)
{
  orxTEXT_MARKER *pstResult;

  if (_pstData == orxNULL)
  {
    pstResult = orxNULL;
  }
  else
  {
    /* Checks */
    orxASSERT(_pstMarkerBank != orxNULL);
    orxASSERT(_u32Index != orxU32_UNDEFINED);
    orxASSERT(_pstData != orxNULL);
    orxASSERT(_pstData->eType != orxTEXT_MARKER_TYPE_NONE);
    orxASSERT(_pstData->eType < orxTEXT_MARKER_TYPE_NUMBER);

    /* Update result */
    pstResult = (orxTEXT_MARKER *) orxBank_Allocate(_pstMarkerBank);
    orxASSERT(pstResult != orxNULL);
    pstResult->u32Index = _u32Index;
    orxMemory_Copy(&(pstResult->stData), _pstData, sizeof(orxTEXT_MARKER_DATA));
  }

  /* Done! */
  return pstResult;
}

/** Create and push a marker stack entry with the specified data / fallback data.
 * @return      orxTEXT_MARKER_NODE
 */
static const orxTEXT_MARKER_NODE *orxFASTCALL orxText_AddMarkerStackEntry(orxLINKLIST *_pstStack, orxBANK *_pstStackBank, const orxTEXT_MARKER_DATA *_pstData, const orxTEXT_MARKER_DATA *_pstFallbackData)
{
  orxASSERT(_pstStackBank != orxNULL);
  orxASSERT(_pstStack     != orxNULL);
  orxASSERT(_pstData      != orxNULL);
  orxASSERT(_pstData->eType != orxTEXT_MARKER_TYPE_NONE);
  orxASSERT(_pstData->eType < orxTEXT_MARKER_TYPE_NUMBER_PARSED);
  /* Allocate and initialize marker stack entry */
  orxTEXT_MARKER_NODE *pstResult = (orxTEXT_MARKER_NODE *) orxBank_Allocate(_pstStackBank);
  orxASSERT(pstResult != orxNULL);
  pstResult->pstData         = _pstData;
  pstResult->pstFallbackData = _pstFallbackData;
  orxLinkList_AddEnd(_pstStack, (orxLINKLIST_NODE *) pstResult);
  return pstResult;
}

/** Returns the fallback for _pstNewData, and set _pstNewData to the new fallback of its type.
 *  Non-revertable types will cause the program to abort.
 * @param[in]  _pstData       Pointer to marker data to update fallback struct with
 * @param[in]  _pstFallbacks  Pointer to fallback structure
 * @return     Pointer to orxTEXT_MARKER_DATA that _pstNewData can fall back to / orxNULL if no fallbacks exist yet.
 */
static const orxTEXT_MARKER_DATA *orxFASTCALL orxText_UpdateMarkerFallback(const orxTEXT_MARKER_DATA *_pstNewData, const orxTEXT_MARKER_DATA *_pstFallbacks[])
{
  /* TODO: This function always has an associated call to CreateMarker. Perhaps this functionality should be integrated? */
  /* TODO: While debugging an assert in this function, it occurred to me that it would be easier to identify program state if I were passing orxTEXT_MARKERs instead of their contextless data. Consider doing that instead. There are few cases where data doesn't have a marker associated with it (e.g. parsing/validating the data before committing to a full marker allocation). */
  /* TODO: This function is sorta awkward - its return value is not always used, and does two related things that are individually too small for their own function */
  const orxTEXT_MARKER_DATA *pstResult;
  orxTEXT_MARKER_TYPE        eType;
  /* Argument checks */
  orxASSERT(_pstFallbacks != orxNULL);
  orxASSERT(_pstNewData   != orxNULL);
  /* EDGE CASE: Markers of type orxTEXT_MARKER_TYPE_REVERT can be stored as a fallback for any revertable type */
  eType = (_pstNewData->eType == orxTEXT_MARKER_TYPE_REVERT) ? _pstNewData->eRevertType : _pstNewData->eType;
  /* Marker type range checks */
  orxASSERT(eType != orxTEXT_MARKER_TYPE_NONE);
  orxASSERT(eType < orxTEXT_MARKER_TYPE_NUMBER_REVERT);
  /* SANITY CHECK: If someone tries to update a fallback to its current value, it means they did something wrong (like pass in a pointer to marker data that lost scope before the next call to this function) */
  orxASSERT(_pstFallbacks[eType] != _pstNewData);
  /* Get a pointer to the appropriate fallback data */
  pstResult = _pstFallbacks[eType];
  /* Replace the result with the new data */
  _pstFallbacks[eType] = _pstNewData;
  /* Done! */
  return pstResult;
}

/** Parses marker value string
 * @param[in]      _pstText             Concerned text
 * @param[in]      _eType               Expected marker data type
 * @param[in]      _zString             Whole unparsed string
 * @param[in]      _u32Offset           Offset in _zString to the start of the marker value
 * @param[out]     _pzRemainder         Where to store pointer to remainder of string
 * @return         orxTEXT_MARKER_DATA  / orxNULL
 */
static orxSTATUS orxFASTCALL orxText_ParseMarkerValue(const orxTEXT *_pstText, orxTEXT_MARKER_DATA *_pstData, const orxSTRING _zString, orxU32 _u32Offset, const orxSTRING *_pzRemainder)
{
  /* Checks */
  orxSTRUCTURE_ASSERT(_pstText);
  orxASSERT(_pstData->eType != orxTEXT_MARKER_TYPE_NONE);
  orxASSERT((_zString != orxNULL) && (_zString != orxSTRING_EMPTY));
  orxASSERT(_u32Offset != orxU32_UNDEFINED);
  orxASSERT(_pzRemainder != orxNULL);

  orxSTATUS eResult;
  const orxSTRING zValueStart;
  orxS32 s32EndIndex;

  zValueStart = _zString + _u32Offset;
  orxASSERT((zValueStart != orxNULL) && (zValueStart != orxSTRING_EMPTY) && (*zValueStart == orxTEXT_KC_MARKER_SYNTAX_OPEN));

  eResult = orxSTATUS_SUCCESS;

  /* Figure out where the value ends */
  s32EndIndex = orxString_SearchCharIndex(zValueStart, orxTEXT_KC_MARKER_SYNTAX_CLOSE, 1);

  /* No end? Bad marker! */
  if (s32EndIndex < 0)
  {
    eResult = orxSTATUS_FAILURE;
    *_pzRemainder = zValueStart;
  }
  else
  {
    orxU32 u32ValueStringSize = (s32EndIndex + 2);
    /* Set remainder pointer */
    *_pzRemainder = zValueStart + u32ValueStringSize - 1;

    /* Make a temporary string to hold the value alone */
#ifdef __orxMSVC__
    orxCHAR *zValueString = (orxCHAR *)alloca(u32ValueStringSize * sizeof(orxCHAR));
#else /* __orxMSVC__ */
    orxCHAR zValueString[u32ValueStringSize];
#endif /* __orxMSVC__ */

    orxString_NCopy(zValueString, zValueStart, u32ValueStringSize);
    zValueString[u32ValueStringSize - 1] = orxCHAR_NULL;

    /* Check style values - if something is invalid, fall through to default */
    switch(_pstData->eType)
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
        _pstData->stFontData.pstMap = orxFont_GetMap(pstFont);
        _pstData->stFontData.pstFont = orxTexture_GetBitmap(orxFont_GetTexture(pstFont));
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
        _pstData->stRGBA = orxColor_ToRGBA(&stColor);
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
        orxVector_Copy(&(_pstData->vScale), &vScale);
        break;
      }
      /* Fall through */
    }
    /* Handle invalid values/types */
    default:
    {
      /* Get type name */
      const orxSTRING zTypeName;
      switch(_pstData->eType)
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
      /* Log warning */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, orxTEXT_KZ_MARKER_WARNING, orxTEXT_KC_MARKER_SYNTAX_START, zTypeName, zValueString, _zString);
      eResult = orxSTATUS_FAILURE;
    }
    }
  }
  return eResult;
}

/** Parses marker type
 * @param[in]      _zString             Whole unparsed string
 * @param[in]      _u32Offset           Offset in _zString to the start of the marker type
 * @param[out]     _pzRemainder         Where to store pointer to remainder of string
 * @return         orxTEXT_MARKER_TYPE
 */
static orxTEXT_MARKER_TYPE orxFASTCALL orxText_ParseMarkerType(const orxSTRING _zString, orxU32 _u32Offset, const orxSTRING *_pzRemainder)
{
/* TODO: Use the codepoint traversal functions */
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

    /* Make a temporary string to hold the bad value for logging */
    orxU32 u32TypeStringSize = (orxU32)(zNextWhiteSpace - zTypeStart + 1);
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
/* TODO: Use the codepoint traversal functions */
  const orxSTRING zMarkedString;
  const orxSTRING zResult;
  orxSTRING zCleanedString;
  orxU32 u32CleanedSize, u32CleanedSizeUsed;

  /* Used for a dry run of marker traversal */
  orxBANK      *pstDryRunMarkerBank, *pstDryRunStackBank;
  orxLINKLIST   stDryRunStack;
  /* Used for keeping track of marker type fallbacks when manipulating the stack */
  const orxTEXT_MARKER_DATA *ppstFallbacks[orxTEXT_MARKER_TYPE_NUMBER_REVERT] = { orxNULL };

  /* If string is invalid, return it. */
  if (_zString == orxNULL || _zString == orxSTRING_EMPTY)
  {
    return _zString;
  }

  /* Clear marker array memory */
  if (_pstText->pstMarkers != orxNULL)
  {
    orxMemory_Set((void *)_pstText->pstMarkers, 0, sizeof(orxTEXT_MARKER) * orxText_GetMarkerCounter(_pstText));
  }

  /* Initialize string traversal/storage variables */
  zMarkedString      = _zString;
  u32CleanedSize     = orxString_GetLength(zMarkedString) + 1;
  u32CleanedSizeUsed = 0;
  zCleanedString     = (orxSTRING) orxMemory_Allocate(u32CleanedSize * sizeof(orxCHAR), orxMEMORY_TYPE_MAIN);
  orxASSERT(zCleanedString != orxNULL);
  orxMemory_Zero(zCleanedString, u32CleanedSize * sizeof(orxCHAR));

  pstDryRunMarkerBank = orxBank_Create(orxTEXT_KU32_MARKER_DATA_BANK_SIZE, sizeof(orxTEXT_MARKER),
                                       orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);
  orxASSERT(pstDryRunMarkerBank != orxNULL);

  pstDryRunStackBank  = orxBank_Create(orxTEXT_KU32_MARKER_DATA_BANK_SIZE, sizeof(orxTEXT_MARKER_NODE),
                                       orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);
  orxASSERT(pstDryRunStackBank != orxNULL);

  orxMemory_Zero(&stDryRunStack, sizeof(orxLINKLIST));

  /* Add first line height marker */
  {
    orxTEXT_MARKER_DATA stData;
    stData.eType = orxTEXT_MARKER_TYPE_LINE_HEIGHT;
    stData.fLineHeight = orxFLOAT_0;
    orxText_CreateMarker(pstDryRunMarkerBank, u32CleanedSizeUsed, &stData);
  }

  /* Parse the string using zMarkedString as a pointer to our current position in _zString */
  while ((zMarkedString != orxNULL) && (*zMarkedString != orxCHAR_NULL))
  {
    /* Start of marker? */
    if (*zMarkedString != orxTEXT_KC_MARKER_SYNTAX_START)
    {
      if (*zMarkedString == orxCHAR_CR)
      {
        zMarkedString++;
        continue;
      }
      /* Newline or start of string? Add a line height marker. */
      if (*zMarkedString == orxCHAR_LF)
      {
        /* This marker will be updated in orxText_UpdateSize */
        orxTEXT_MARKER_DATA stData;
        stData.eType = orxTEXT_MARKER_TYPE_LINE_HEIGHT;
        stData.fLineHeight = orxFLOAT_0;
        orxText_CreateMarker(pstDryRunMarkerBank, u32CleanedSizeUsed, &stData);
      }

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
            /* Pop the stack */
            orxTEXT_MARKER_NODE *pstPoppedEntry = (orxTEXT_MARKER_NODE *) orxLinkList_GetLast(&stDryRunStack);
            orxASSERT(pstPoppedEntry != orxNULL);
            orxLinkList_Remove((orxLINKLIST_NODE *) pstPoppedEntry);

            /* Sanity checks - Integrity of stDryRunStack must not be violated */
            orxASSERT(pstPoppedEntry->pstData->eType != orxTEXT_MARKER_TYPE_NONE);
            orxASSERT(pstPoppedEntry->pstData->eType < orxTEXT_MARKER_TYPE_NUMBER_REVERT);

            /* The fallback of the popped entry will serve as the data for a new marker */
            orxTEXT_MARKER_DATA stFallbackData;
            /* If the fallback data of the popped entry is null, it means we're reverting to a default value. */
            if (pstPoppedEntry->pstFallbackData == orxNULL)
            {
              /* Default values are unknown to orxTEXT, so we put a placeholder marker that identifies its data type */
              stFallbackData.eType       = orxTEXT_MARKER_TYPE_REVERT;
              stFallbackData.eRevertType = pstPoppedEntry->pstData->eType;
            }
            else
            {
              /* Copy the data of the popped entry's fallback marker data */
              stFallbackData = *(pstPoppedEntry->pstFallbackData);
            }

            /* Delete the popped entry */
            orxBank_Free(pstDryRunStackBank, pstPoppedEntry);

            /* Copy the fallback data into a new marker */
            const orxTEXT_MARKER *pstMarker = orxText_CreateMarker(pstDryRunMarkerBank, u32CleanedSizeUsed, &stFallbackData);
            /* Update the processor fallback data to be the newly added marker's data */
            orxText_UpdateMarkerFallback(&pstMarker->stData, ppstFallbacks);
          }

          /* Continue parsing */
        }
        else if (eType == orxTEXT_MARKER_TYPE_CLEAR)
        {
          /* Clear out the stack */
          /* TODO: Rewrite this to be simpler for clarity */
          /* clear fallback array to defaults
             while stack not empty
              pop
              if entry type not represented in fallback array, add revert marker for that type and store in fallback array.
             by end of loop, all data in fallback array should point to revert marker or null
             clear stack
          */
          for (orxU32 u32FallbackType = 0; u32FallbackType < orxTEXT_MARKER_TYPE_NUMBER_REVERT; u32FallbackType++)
          {
            ppstFallbacks[u32FallbackType] = orxNULL;
          }
          /* We can't pop the stack if it's already empty */
          while (orxLinkList_GetCounter(&stDryRunStack) > 0)
          {
            /* Pop the stack */
            orxTEXT_MARKER_NODE *pstPoppedEntry = (orxTEXT_MARKER_NODE *) orxLinkList_GetLast(&stDryRunStack);
            orxASSERT(pstPoppedEntry != orxNULL);
            orxLinkList_Remove((orxLINKLIST_NODE *) pstPoppedEntry);

            /* Sanity checks - Integrity of stDryRunStack must not be violated */
            orxASSERT(pstPoppedEntry->pstData->eType != orxTEXT_MARKER_TYPE_NONE);
            orxASSERT(pstPoppedEntry->pstData->eType < orxTEXT_MARKER_TYPE_NUMBER_REVERT);

            /* Only add revert markers once per type */
            if (ppstFallbacks[pstPoppedEntry->pstData->eType] != orxNULL)
            {
              continue;
            }

            /* Create revert data for a new marker */
            orxTEXT_MARKER_DATA stFallbackData;
            stFallbackData.eType = orxTEXT_MARKER_TYPE_REVERT;
            stFallbackData.eRevertType = pstPoppedEntry->pstData->eType;

            /* Delete the popped entry */
            orxBank_Free(pstDryRunStackBank, pstPoppedEntry);

            /* Copy the fallback data into a new marker */
            const orxTEXT_MARKER *pstMarker = orxText_CreateMarker(pstDryRunMarkerBank, u32CleanedSizeUsed, &stFallbackData);
            /* Update the processor fallback data to be the newly added marker's data */
            orxText_UpdateMarkerFallback(&pstMarker->stData, ppstFallbacks);
          }

          /* Clear stack storage */
          orxLinkList_Clean(&stDryRunStack);
          orxBank_Clear(pstDryRunStackBank);

          /* Fallback checks */
          for (orxU32 u32FallbackType = 0; u32FallbackType < orxTEXT_MARKER_TYPE_NUMBER_REVERT; u32FallbackType++)
          {
            const orxTEXT_MARKER_DATA *pstFallbackData = ppstFallbacks[u32FallbackType];
            /* orxNULL is an acceptable value for fallback data - it simply means a marker of that type has not previously existed */
            if (pstFallbackData == orxNULL)
            {
              continue;
            }
            orxASSERT(pstFallbackData->eType == orxTEXT_MARKER_TYPE_REVERT);
            orxASSERT(pstFallbackData->eRevertType != orxTEXT_MARKER_TYPE_NONE);
            orxASSERT(pstFallbackData->eRevertType < orxTEXT_MARKER_TYPE_NUMBER_REVERT);
          }

          /* Continue parsing */
        }
        else
        {
          /* This marker has data associated with it */
          orxASSERT(*zMarkedString == orxTEXT_KC_MARKER_SYNTAX_OPEN);
          orxTEXT_MARKER_DATA stData;
          stData.eType = eType;

          /* Parse the marker value into marker data */
          orxSTATUS eResult = orxText_ParseMarkerValue(_pstText, &stData, _zString, (orxU32)(zMarkedString - _zString), &zMarkedString);
          /* Add marker */
          if (eResult == orxSTATUS_SUCCESS)
          {
            /* Create marker */
            orxTEXT_MARKER *pstMarker = orxText_CreateMarker(pstDryRunMarkerBank, u32CleanedSizeUsed, &stData);
            orxASSERT(pstMarker != orxNULL);
            /* Get the data for this marker to fall back to */
            const orxTEXT_MARKER_DATA *pstFallbackData = orxText_UpdateMarkerFallback(&pstMarker->stData, ppstFallbacks);
            /* Push data to stack with fallback data */
            const orxTEXT_MARKER_NODE *pstStackEntry = orxText_AddMarkerStackEntry(&stDryRunStack, pstDryRunStackBank, &pstMarker->stData, pstFallbackData);
          }
          /* Continue parsing */
        }
      }
    }
  }

  /* Move markers to array. */
  orxU32 u32MarkerCounter = orxBank_GetCounter(pstDryRunMarkerBank);
  if (u32MarkerCounter > 0)
  {
    orxASSERT(orxText_GetFont(_pstText) != orxNULL);
    orxFLOAT fCharacterHeight = orxFont_GetCharacterHeight(orxText_GetFont(_pstText));
    orxASSERT(fCharacterHeight > orxFLOAT_0);
    orxFLOAT fScaleY = orxFLOAT_1;
    orxTEXT_MARKER *pstLineMarker = orxNULL;
    _pstText->pstMarkers = (orxTEXT_MARKER *) orxMemory_Allocate(sizeof(orxTEXT_MARKER) * u32MarkerCounter, orxMEMORY_TYPE_MAIN);
    orxASSERT(_pstText->pstMarkers != orxNULL);
    for (orxU32 u32Index = 0; u32Index < u32MarkerCounter; u32Index++)
    {
      const orxTEXT_MARKER *pstStoreMarkerAt = _pstText->pstMarkers + u32Index;
      orxASSERT(pstStoreMarkerAt != orxNULL);
      orxTEXT_MARKER *pstMarker = (orxTEXT_MARKER *) orxBank_GetAtIndex(pstDryRunMarkerBank, u32Index);
      orxASSERT(pstMarker != orxNULL);
      orxASSERT(pstMarker->stData.eType != orxTEXT_MARKER_TYPE_NONE);
      orxASSERT(pstMarker->stData.eType < orxTEXT_MARKER_TYPE_NUMBER);
      /* Eliminate revert markers */
      if (pstMarker->stData.eType == orxTEXT_MARKER_TYPE_REVERT)
      {
        orxTEXT_MARKER_TYPE eRevertType = pstMarker->stData.eRevertType;
        orxASSERT(eRevertType == orxTEXT_MARKER_TYPE_FONT  ||
                  eRevertType == orxTEXT_MARKER_TYPE_COLOR ||
                  eRevertType == orxTEXT_MARKER_TYPE_SCALE);
        switch (eRevertType)
        {
          case orxTEXT_MARKER_TYPE_FONT:
          {
            pstMarker->stData.stFontData.pstMap = orxFont_GetMap(orxText_GetFont(_pstText));
            pstMarker->stData.stFontData.pstFont = orxTexture_GetBitmap(orxFont_GetTexture(orxText_GetFont(_pstText)));
            break;
          }
          case orxTEXT_MARKER_TYPE_COLOR:
          {
            pstMarker->stData.stRGBA.u8R = 255;
            pstMarker->stData.stRGBA.u8G = 255;
            pstMarker->stData.stRGBA.u8B = 255;
            pstMarker->stData.stRGBA.u8A = 255;
            break;
          }
          case orxTEXT_MARKER_TYPE_SCALE:
          {
            pstMarker->stData.vScale.fX = orxFLOAT_1;
            pstMarker->stData.vScale.fY = orxFLOAT_1;
            pstMarker->stData.vScale.fZ = orxFLOAT_1;
            break;
          }
          default:
            orxASSERT(orxFALSE, "Invalid marker type");
        }
        pstMarker->stData.eType = eRevertType;
      }
      /* TODO: Overwrite redundant markers (i.e. multiple markers of the same type at the same index) as we go. */
      pstMarker = (orxTEXT_MARKER *) orxMemory_Copy((void *)pstStoreMarkerAt, (void *)pstMarker, sizeof(orxTEXT_MARKER));
      /* Checks */
      orxASSERT(pstMarker != orxNULL);
      switch (pstMarker->stData.eType)
      {
      case orxTEXT_MARKER_TYPE_FONT:
        orxASSERT(pstMarker->stData.stFontData.pstMap != orxNULL);
        orxASSERT(pstMarker->stData.stFontData.pstMap->fCharacterHeight > orxFLOAT_0);
        orxASSERT(pstMarker->stData.stFontData.pstFont != orxNULL);
        fCharacterHeight = pstMarker->stData.stFontData.pstMap->fCharacterHeight;
        pstLineMarker->stData.fLineHeight = orxMAX(pstLineMarker->stData.fLineHeight, fScaleY * fCharacterHeight);
        break;
      case orxTEXT_MARKER_TYPE_COLOR:
        break;
      case orxTEXT_MARKER_TYPE_SCALE:
        fScaleY = pstMarker->stData.vScale.fY;
        pstLineMarker->stData.fLineHeight = orxMAX(pstLineMarker->stData.fLineHeight, fScaleY * fCharacterHeight);
        break;
      case orxTEXT_MARKER_TYPE_LINE_HEIGHT:
        pstLineMarker = pstMarker;
        pstLineMarker->stData.fLineHeight = fScaleY * fCharacterHeight;
        orxASSERT(pstMarker->stData.fLineHeight > orxFLOAT_0);
        break;
      default:
        orxASSERT(orxFALSE, "Invalid marker type");
      }
    }
  }
  _pstText->u32MarkerCounter = u32MarkerCounter;

  /* Free the dry run banks */
  orxBank_Delete(pstDryRunMarkerBank);
  orxBank_Delete(pstDryRunStackBank);

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
    orxFLOAT        fWidth, fMaxWidth, fHeight, fCharacterHeight;
    orxU32          u32CharacterCodePoint, u32CharacterIndex, u32MarkerIndex;
    orxTEXT_MARKER *pstLineMarker;
    const orxCHAR  *pc;

    /* It's expected that there will be at least one line height marker */
    orxASSERT(orxText_GetMarkerCounter(_pstText) > 0);
    u32MarkerIndex = 0;
    pstLineMarker = orxNULL;

    /* So I hit another one of those points where I found a design flaw in my code, but it should be a fairly simple one. Basically one decision I made earlier on was that "default" markup values (i.e. what the text looks like with an empty marker stack) is up to the user (in this case the `TransformText()`). I do this by having a special marker type (revert) that signifies the need for the user to provide the styling. The problem with this is that it's conceptually incompatible with precalculating line height. I realized the other day that leaving that kind of thing up to the user isn't necessary since we already know what the orxTEXT default font is, and character scaling is a marker-only concept. Colors in orx are multiplicative so at the scope of text rendering, the default color will always be white. */

    /* TODO: make sure we use vScale for char width! */
    /* For all characters */
    for(u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(_pstText->zString, &pc), u32CharacterIndex = 0, fHeight = 0, fWidth = fMaxWidth = orxFLOAT_0;
        u32CharacterCodePoint != orxCHAR_NULL;
        u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(pc, &pc), u32CharacterIndex++)
    {
      /* This is breaking because i dont account for multiple markers at the same index. */
      /* Check for marker at index */
      if (u32MarkerIndex < orxText_GetMarkerCounter(_pstText))
      {
        while (_pstText->pstMarkers[u32MarkerIndex].u32Index == u32CharacterIndex)
        {
          orxTEXT_MARKER *pstMarker = (_pstText->pstMarkers + u32MarkerIndex);
          u32MarkerIndex++;
          /* New line height marker? */
          if (pstMarker->stData.eType == orxTEXT_MARKER_TYPE_LINE_HEIGHT)
          {
            pstLineMarker = pstMarker;
          }
        }
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
          }

          /* Fall through */
        }

        case orxCHAR_LF:
        {
          /* Updates height */
          fHeight += pstLineMarker->stData.fLineHeight;

          /* Updates max width */
          fMaxWidth = orxMAX(fMaxWidth, fWidth);

          /* Resets width */
          fWidth = orxFLOAT_0;

          break;
        }

        default:
        {
          /* Updates width */
          fWidth += orxFont_GetCharacterWidth(_pstText->pstFont, u32CharacterCodePoint);

          break;
        }
      }
    }

    /* Stores values */
    _pstText->fWidth  = orxMAX(fWidth, fMaxWidth);
    _pstText->fHeight = fHeight + pstLineMarker->stData.fLineHeight;
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
    pstResult->pstMarkers = orxNULL;

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
    if (_pstText->pstMarkers != orxNULL)
    {
      orxMemory_Free((void *) _pstText->pstMarkers);
    }

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

/** Gets number of markers
 * @param[in]   _pstText      Concerned text
 * @return      Text marker counter
 */
orxU32 orxFASTCALL orxText_GetMarkerCounter(const orxTEXT *_pstText)
{
  orxU32 u32Result;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Updates result */
  u32Result = _pstText->u32MarkerCounter;

  /* Done! */
  return u32Result;
}

/** Gets marker array
 * @param[in] _pstText  Concerned text
 * @return Pointer to orxTEXT_MARKER / orxNULL if no markers
 */
const orxTEXT_MARKER *orxFASTCALL orxText_GetMarkerArray(const orxTEXT *_pstText)
{
  const orxTEXT_MARKER *pstResult = orxNULL;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Update result */
  pstResult = _pstText->pstMarkers;

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

  /* Has markers? */
  if (_pstText->pstMarkers != orxNULL)
  {
    /* Cleans it */
    orxMemory_Free((void *)_pstText->pstMarkers);
    _pstText->pstMarkers = orxNULL;
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
